#include "camera_override.hpp"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

namespace efp {
namespace {
// Ten integer/pointer arguments are forwarded in the same Win64 slots observed
// in the reference detour. This prototype does not call any player/FPS setter.
using CameraFunction = std::uintptr_t (*)(void*, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t);
CameraFunction original{};
std::atomic<bool> installed{false};
std::atomic<Millis> last_valid_record{0};
std::atomic<Millis> active_sample{0};
std::atomic<bool> debug_enabled{false};
SRWLOCK telemetry_lock = SRWLOCK_INIT;
CameraTelemetry telemetry;
void publish_telemetry(const CameraTelemetry& sample) {
    if (!debug_enabled.load(std::memory_order_relaxed) || !TryAcquireSRWLockExclusive(&telemetry_lock)) return;
    if (sample.observed >= telemetry.observed) telemetry = sample;
    ReleaseSRWLockExclusive(&telemetry_lock);
}
std::atomic<float> distance{Settings{}.prototype_distance}, height{Settings{}.eye_height},
    forward{Settings{}.eye_forward}, side{Settings{}.eye_side};
std::uintptr_t mode_address{};
constexpr std::array<unsigned char, 25> prefix = {
    0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20,0x53,0x56,0x57,0x41,0x54,0x41,
    0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x90,0x03,0x00,0x00};
bool read(std::uintptr_t address, void* out, std::size_t size) {
    SIZE_T got{};
    return address >= 0x10000 && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &got) && got == size;
}
bool writable_record(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
        info.State != MEM_COMMIT || info.Type != MEM_PRIVATE || (info.Protect & PAGE_GUARD)) return false;
    const auto access = info.Protect & 0xff;
    if (access != PAGE_READWRITE && access != PAGE_WRITECOPY) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    return address >= begin && address - begin <= info.RegionSize && 0x40 <= info.RegionSize - (address - begin);
}
struct CameraRecord { float fields[16]; };
bool valid_record(const CameraRecord& record) {
    const auto* f = record.fields;
    for (unsigned i = 6; i <= 11; ++i) if (!std::isfinite(f[i]) || std::abs(f[i]) > 1000000.0f) return false;
    const auto length_squared = f[6]*f[6] + f[7]*f[7] + f[8]*f[8];
    return length_squared >= 0.25f && length_squared <= 4.0f;
}
struct InputRecord {
    bool readable = false;
    std::uintptr_t index = 0;
    std::array<float,3> first{}, second{};
};
InputRecord read_input_record(std::uintptr_t owner) {
    InputRecord input;
    constexpr auto max = std::numeric_limits<std::uintptr_t>::max();
    std::uintptr_t base{};
    std::array<float,12> fields{};
    if (owner < 0x10000 || owner > max - 0x70 ||
        !read(owner + 0x30, &base, sizeof(base)) || !read(owner + 0x68, &input.index, sizeof(input.index)) ||
        input.index > 0x100000 || base < 0x10000 || base > max - input.index * 0x30 - sizeof(fields) ||
        !read(base + input.index * 0x30, fields.data(), sizeof(fields))) return input;
    for (float value : fields) if (!std::isfinite(value) || std::abs(value) > 1000000.0f) return input;
    input.first = {fields[0],fields[1],fields[2]};
    input.second = {fields[4],fields[5],fields[6]};
    input.readable = true;
    return input;
}
std::uintptr_t camera_detour(void* object, std::uintptr_t a2, std::uintptr_t a3, std::uintptr_t a4,
    std::uintptr_t a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8, std::uintptr_t a9, std::uintptr_t a10) {
    const auto owner = reinterpret_cast<std::uintptr_t>(object);
    // Optional bounded reads only. Native execution, collision results and history remain unchanged.
    const bool debug = debug_enabled.load(std::memory_order_relaxed);
    const auto before = debug ? read_input_record(owner) : InputRecord{};
    const auto result = original(object,a2,a3,a4,a5,a6,a7,a8,a9,a10);
    const auto now = GetTickCount64();
    std::uintptr_t base{}, index{};
    if (owner < 0x10000 || owner > std::numeric_limits<std::uintptr_t>::max() - 0x70 ||
        !read(owner + 0x38, &base, sizeof(base)) || !read(owner + 0x68, &index, sizeof(index)) ||
        index > 0x100000 || base > std::numeric_limits<std::uintptr_t>::max() - index * 0x40) return result;
    const auto address = base + index * 0x40;
    CameraRecord record{};
    if (!writable_record(address) || !read(address, &record, sizeof(record)) || !valid_record(record)) return result;
    last_valid_record.store(now, std::memory_order_release);
    CameraTelemetry sample;
    sample.observed = now; sample.record_index = index;
    sample.native_position = {record.fields[9],record.fields[10],record.fields[11]};
    sample.requested_position = sample.native_position;
    sample.direction = {record.fields[6],record.fields[7],record.fields[8]};
    if (debug) {
        const auto after = read_input_record(owner);
        sample.input_before_readable = before.readable && before.index == index;
        sample.input_after_readable = after.readable && after.index == index;
        if (sample.input_before_readable) { sample.input0_before = before.first; sample.input1_before = before.second; }
        if (sample.input_after_readable) { sample.input0_after = after.first; sample.input1_after = after.second; }
    }
    const auto observed = active_sample.load(std::memory_order_acquire);
    int mode = -1;
    DWORD process{}; const auto window = GetForegroundWindow();
    if (window) GetWindowThreadProcessId(window, &process);
    if (!observed || now < observed || now - observed > 150 || process != GetCurrentProcessId() ||
        !read(mode_address, &mode, sizeof(mode)) || mode != 0) { publish_telemetry(sample); return result; }

    const auto* f = record.fields;
    const float norm = std::sqrt(f[6]*f[6] + f[7]*f[7] + f[8]*f[8]);
    const float fx = f[6]/norm, fy = f[7]/norm, fz = f[8]/norm;
    // Prototype switches immediately. Rollback leaves the original output untouched.
    const float advance = distance.load(std::memory_order_relaxed) - forward.load(std::memory_order_relaxed);
    std::array<float,3> position = {f[9] - fx*advance, f[10] - fy*advance + height.load(), f[11] - fz*advance};
    const auto horizontal = std::sqrt(fx*fx + fz*fz);
    if (horizontal > 0.01f) {
        const auto sideways = side.load(std::memory_order_relaxed) / horizontal;
        position[0] += fz*sideways; position[2] -= fx*sideways;
    }
    for (const auto value : position) if (!std::isfinite(value) || std::abs(value) > 1000000.0f) { publish_telemetry(sample); return result; }
    SIZE_T written{};
    sample.requested_position = position;
    sample.write_attempted = true;
    // Bounded write to output position only. No game flags, archives, FOV, or visibility writes.
    sample.write_ok = WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address + 0x24), position.data(), sizeof(position), &written) && written == sizeof(position);
    sample.write_bytes = written;
    publish_telemetry(sample);
    return result;
}
}
bool start_camera_prototype(std::uintptr_t module_base, const Log& log) {
    auto* target = reinterpret_cast<void*>(module_base + 0x207BF90);
    std::array<unsigned char,25> actual{};
    if (!read(reinterpret_cast<std::uintptr_t>(target), actual.data(), actual.size()) || actual != prefix) {
        log("Prototype camera hook rejected: signature differs or another camera mod hooked first."); return false;
    }
    const auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;
    const auto create = MH_CreateHook(target, reinterpret_cast<void*>(&camera_detour), reinterpret_cast<void**>(&original));
    if (create != MH_OK) { log(std::string("Camera hook creation failed: ") + MH_StatusToString(create)); return false; }
    mode_address = module_base + 0x5D05058;
    const auto enable = MH_EnableHook(target);
    if (enable != MH_OK) { log(std::string("Camera hook enable failed: ") + MH_StatusToString(enable)); return false; }
    installed.store(true, std::memory_order_release);
    log("EXPERIMENTAL camera-offset prototype installed. Eye anchor/body visibility/collision not yet validated.");
    return true;
}
CameraTelemetry latest_camera_telemetry() {
    CameraTelemetry copy;
    if (!TryAcquireSRWLockShared(&telemetry_lock)) return copy;
    copy = telemetry;
    ReleaseSRWLockShared(&telemetry_lock);
    return copy;
}
bool camera_record_recent() {
    const auto seen = last_valid_record.load(std::memory_order_acquire);
    const auto now = GetTickCount64();
    return installed.load(std::memory_order_acquire) && seen != 0 && now >= seen && now - seen <= 200;
}
void publish_camera_control(bool active, Millis observed, const Settings& settings) {
    distance.store(settings.prototype_distance, std::memory_order_relaxed);
    height.store(settings.eye_height, std::memory_order_relaxed);
    forward.store(settings.eye_forward, std::memory_order_relaxed);
    side.store(settings.eye_side, std::memory_order_relaxed);
    debug_enabled.store(settings.debug, std::memory_order_relaxed);
    active_sample.store(active ? observed : 0, std::memory_order_release);
}
} // namespace efp
