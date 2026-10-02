#include "camera_override.hpp"
#include "camera_geometry.hpp"
#include "camera_transition.hpp"
#include <algorithm>
#include <numbers>
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
struct CameraControl {
    bool active{}, allowed{};
    Millis observed{}, epoch{};
    float blend{};
    Settings settings;
};
SRWLOCK control_lock = SRWLOCK_INIT;
CameraControl control;
CameraTransition transition;
Millis (*clock_now)() = [] { return static_cast<Millis>(GetTickCount64()); };
bool game_foreground() {
    DWORD process{};const auto window=GetForegroundWindow();
    if (window) GetWindowThreadProcessId(window,&process);
    return process==GetCurrentProcessId();
}
bool (*foreground_check)() = &game_foreground;
struct PositionedCamera {
    std::uintptr_t address{};
    Millis observed{}, epoch{};
    float blend{};
    Vec3 position{}, direction{};
};
SRWLOCK positioned_lock = SRWLOCK_INIT;
std::array<PositionedCamera,8> positioned;
void publish_position(const PositionedCamera& sample) {
    if (!TryAcquireSRWLockExclusive(&positioned_lock)) return;
    auto slot=std::find_if(positioned.begin(),positioned.end(),[&](const auto& p) { return p.address==sample.address; });
    if (slot==positioned.end()) slot=std::min_element(positioned.begin(),positioned.end(),[](const auto& a,const auto& b) { return a.observed<b.observed; });
    *slot=sample;ReleaseSRWLockExclusive(&positioned_lock);
}
std::atomic<bool> debug_enabled{false};
SRWLOCK telemetry_lock = SRWLOCK_INIT;
CameraTelemetry telemetry;
void publish_telemetry(const CameraTelemetry& sample) {
    if (!debug_enabled.load(std::memory_order_relaxed) || !TryAcquireSRWLockExclusive(&telemetry_lock)) return;
    if (sample.observed >= telemetry.observed) telemetry = sample;
    ReleaseSRWLockExclusive(&telemetry_lock);
}
using ProjectionFunction = void (*)(void*,const void*,float*,float);
using FovSetter = void (*)(void*,float);
ProjectionFunction original_projection{};
FovSetter set_render_fov{};
std::uintptr_t render_camera_address{}, render_vtable{}, override_flag_address{};
SRWLOCK fov_lock = SRWLOCK_INIT;
FovTelemetry fov_telemetry;
Millis fov_epoch{};
std::uintptr_t fov_record_address{};
float fov_baseline{};
bool fov_calibrated = false;
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
    std::uintptr_t base_after{}, index_after{};
    if (!read(owner + 0x30, &base_after, sizeof(base_after)) || !read(owner + 0x68, &index_after, sizeof(index_after)) ||
        base_after != base || index_after != input.index) return input;
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
    const auto now = clock_now();
    std::uintptr_t base{}, index{};
    if (owner < 0x10000 || owner > std::numeric_limits<std::uintptr_t>::max() - 0x70 ||
        !read(owner + 0x38, &base, sizeof(base)) || !read(owner + 0x68, &index, sizeof(index)) ||
        index > 0x100000 || base > std::numeric_limits<std::uintptr_t>::max() - index * 0x40) { last_valid_record.store(0, std::memory_order_release); return result; }
    const auto address = base + index * 0x40;
    CameraRecord record{};
    if (!writable_record(address) || !read(address, &record, sizeof(record)) || !valid_record(record)) { last_valid_record.store(0, std::memory_order_release); return result; }
    CameraTelemetry sample;
    sample.observed = now; sample.record_index = index;
    sample.native_position = {record.fields[9],record.fields[10],record.fields[11]};
    sample.requested_position = sample.native_position;
    sample.direction = {record.fields[6],record.fields[7],record.fields[8]};
    const auto after = read_input_record(owner);
    sample.input_before_readable = before.readable && before.index == index;
    sample.input_after_readable = after.readable && after.index == index;
    if (sample.input_before_readable) { sample.input0_before = before.first; sample.input1_before = before.second; }
    if (sample.input_after_readable) { sample.input0_after = after.first; sample.input1_after = after.second; }
    sample.anchor_valid = sample.input_after_readable && plausible_anchor(after.first, after.second, sample.native_position);
    last_valid_record.store(sample.anchor_valid ? now : 0, std::memory_order_release);
    if (!sample.anchor_valid) { publish_telemetry(sample); return result; }
    // Hold a coherent control snapshot through the bounded write; worker changes cannot race a safety exit.
    if (!TryAcquireSRWLockShared(&control_lock)) { publish_telemetry(sample);return result; }
    struct Unlock { ~Unlock() { ReleaseSRWLockShared(&control_lock); } } unlock;
    int mode=-1;
    if (!control.allowed || !control.observed || now<control.observed || now-control.observed>150 ||
        !foreground_check() || !read(mode_address,&mode,sizeof(mode)) || mode!=0) { publish_telemetry(sample);return result; }
    sample.first_person_blend=control.blend;
    if (control.blend<=0) { publish_telemetry(sample);return result; }
    const auto& calibration=control.settings;
    // Never fall back to the retracted third-person position if the anchor or calibration fails.
    const auto target = anchored_position(after.first, sample.direction, calibration);
    if (!target) { last_valid_record.store(0, std::memory_order_release); publish_telemetry(sample); return result; }
    Vec3 position;
    for (unsigned i=0;i<3;++i) position[i]=sample.native_position[i]+((*target)[i]-sample.native_position[i])*control.blend;
    // Blend current endpoints, not frozen world positions, so walking/pitching/native boom recovery continue.
    std::uintptr_t base_after{}, index_after{};
    CameraRecord current{};
    if (!read(owner+0x38,&base_after,sizeof(base_after)) || !read(owner+0x68,&index_after,sizeof(index_after)) ||
        base_after!=base || index_after!=index || !read(address,&current,sizeof(current)) ||
        std::memcmp(&current,&record,sizeof(record))!=0 || !foreground_check() ||
        !read(mode_address,&mode,sizeof(mode)) || mode!=0) { publish_telemetry(sample);return result; }
    sample.anchor_used = true;
    SIZE_T written{};
    sample.requested_position = position;
    sample.write_attempted = true;
    // Bounded position-only write. FOV is independently gated in the render projection hook.
    sample.write_ok = WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address + 0x24), position.data(), sizeof(position), &written) && written == sizeof(position);
    sample.write_bytes = written;
    if (sample.write_ok) publish_position({address,now,control.epoch,control.blend,position,sample.direction});
    publish_telemetry(sample);
    return result;
}
// Native builder: RCX render camera, RDX transform, R8 FOV pointer, XMM3 aspect ratio.
// Run it unchanged first. Adjust only its scratch render camera via the native matrix-rebuilding setter.
void projection_detour(void* camera,const void* frame,float* fov,float aspect) {
    original_projection(camera,frame,fov,aspect);
    // Unrelated cameras must not clear the gameplay camera's entry FOV or diagnostic result.
    if (reinterpret_cast<std::uintptr_t>(camera)!=render_camera_address || !set_render_fov) return;
    if (!TryAcquireSRWLockExclusive(&fov_lock)) return;
    struct UnlockFov { ~UnlockFov() { ReleaseSRWLockExclusive(&fov_lock); } } unlock_fov;
    const auto now=clock_now();
    fov_telemetry={};fov_telemetry.observed=now;
    const auto reject=[&](const char* why) { fov_calibrated=false;fov_telemetry.reason=why; };
    if (!TryAcquireSRWLockShared(&control_lock)) { reject("control busy");return; }
    struct UnlockControl { ~UnlockControl() { ReleaseSRWLockShared(&control_lock); } } unlock_control;
    int mode=-1;
    if (!control.allowed || !control.observed || now<control.observed || now-control.observed>150 ||
        !foreground_check() || !read(mode_address,&mode,sizeof(mode)) || mode!=0) { reject("native camera: safety interruption");return; }
    if (!control.settings.first_person_fov_enabled || control.blend<=0) { reject("native FOV");return; }
    std::uintptr_t vtable{};int projection_mode{};unsigned char override_flag{};float native_fov{},native_aspect{};
    MEMORY_BASIC_INFORMATION info{};
    const auto address=reinterpret_cast<std::uintptr_t>(camera);
    if (!VirtualQuery(camera,&info,sizeof(info)) || info.State!=MEM_COMMIT || (info.Protect&PAGE_GUARD) ||
        (info.Protect&0xff)!=PAGE_READWRITE || address<reinterpret_cast<std::uintptr_t>(info.BaseAddress) ||
        address-reinterpret_cast<std::uintptr_t>(info.BaseAddress)>info.RegionSize ||
        0x2e0>info.RegionSize-(address-reinterpret_cast<std::uintptr_t>(info.BaseAddress)) ||
        !read(address,&vtable,8) || vtable!=render_vtable || !read(address+0x2cc,&projection_mode,4) || projection_mode!=1 ||
        !read(override_flag_address,&override_flag,1) || override_flag ||
        !read(address+0x2d0,&native_fov,4) || !std::isfinite(native_fov) || native_fov<0.35f || native_fov>2.8f ||
        !read(address+0x2d4,&native_aspect,4) || !std::isfinite(native_aspect) || native_aspect<0.5f || native_aspect>5.0f) {
        reject("render lens validation failed");return;
    }
    std::array<float,12> transform{};
    if (!read(reinterpret_cast<std::uintptr_t>(frame),transform.data(),sizeof(transform)) || !TryAcquireSRWLockShared(&positioned_lock)) {
        reject("render transform unavailable");return;
    }
    PositionedCamera match{};bool found=false,ambiguous=false;
    for (const auto& p:positioned) {
        if (!p.observed || now<p.observed || now-p.observed>100 || p.epoch!=control.epoch || p.blend<=0) continue;
        bool same=true;
        for (unsigned i=0;i<3;++i) same=same && std::isfinite(transform[6+i]) && std::isfinite(transform[9+i]) &&
            std::abs(transform[6+i]-p.direction[i])<=0.0001f && std::abs(transform[9+i]-p.position[i])<=0.0001f;
        if (same) {
            if (found) { ambiguous=true;break; }
            match=p;found=true;
        }
    }
    ReleaseSRWLockShared(&positioned_lock);
    if (ambiguous) { reject("ambiguous positioned camera");return; }
    if (!found) { reject("no matching positioned camera");return; }
    // RVA 0x208F050 passes a stack-built query to the position hook (RCX = RSP+0x50).
    // Its owner pointer expires before rendering. Revalidate the actual output record instead.
    CameraRecord current{};
    if (!writable_record(match.address) || !read(match.address,&current,sizeof(current)) ||
        !valid_record(current)) { reject("positioned record unavailable");return; }
    for (unsigned i=0;i<3;++i) if (current.fields[6+i]!=match.direction[i] || current.fields[9+i]!=match.position[i]) {
        reject("positioned camera changed");return;
    }
    if (!fov_calibrated || fov_epoch!=control.epoch || fov_record_address!=match.address) {
        fov_baseline=native_fov;fov_epoch=control.epoch;fov_record_address=match.address;fov_calibrated=true;
    }
    constexpr float radians=std::numbers::pi_v<float>/180.0f;
    // Retain native FOV changes after entry as additive offsets, including native sprint changes.
    const float output=native_fov+(control.settings.first_person_fov*radians-fov_baseline)*match.blend;
    // The transition can start outside the menu target range; bound it to the validated native lens range.
    if (!std::isfinite(output) || output<0.35f || output>2.8f || !foreground_check() ||
        !read(mode_address,&mode,4) || mode!=0) { reject("FOV bounds or safety changed");return; }
    const bool changed=std::abs(output-native_fov)>0.000001f;
    if (changed) set_render_fov(camera,output);
    fov_telemetry.matched=true;fov_telemetry.applied=changed;fov_telemetry.blend=match.blend;
    fov_telemetry.native_degrees=native_fov/radians;fov_telemetry.output_degrees=output/radians;
    fov_telemetry.reason="matched positioned render camera";
}
void start_scoped_fov(std::uintptr_t module_base,const Log& log) {
    constexpr std::array<unsigned char,22> builder_prefix={0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x48,0x89,0x68,0x18,0x56,0x57,0x41,0x56,0x48,0x81,0xec,0x90,0x03,0x00,0x00};
    constexpr std::array<unsigned char,15> setter_prefix={0x33,0xd2,0xc5,0xfa,0x11,0x89,0xd0,0x02,0x00,0x00,0xe9,0xc1,0x0b,0x00,0x00};
    std::array<unsigned char,22> builder{};std::array<unsigned char,15> setter{};
    if (!read(module_base+0x1bd5660,builder.data(),builder.size()) || builder!=builder_prefix ||
        !read(module_base+0x3228200,setter.data(),setter.size()) || setter!=setter_prefix) {
        log("Scoped FOV unavailable: signature mismatch; position transitions remain available.");return;
    }
    auto* target=reinterpret_cast<void*>(module_base+0x1bd5660);
    if (MH_CreateHook(target,reinterpret_cast<void*>(&projection_detour),reinterpret_cast<void**>(&original_projection))!=MH_OK) {
        log("Scoped FOV unavailable: hook creation failed; position transitions remain available.");return;
    }
    render_camera_address=module_base+0x5d047c0;render_vtable=module_base+0x4835370;
    override_flag_address=module_base+0x5d04f90;set_render_fov=reinterpret_cast<FovSetter>(module_base+0x3228200);
    if (MH_EnableHook(target)!=MH_OK) { log("Scoped FOV unavailable: hook enable failed; position transitions remain available.");return; }
    log("Scoped FOV render hook installed; writes require a matching positioned camera and validated perspective lens. Position-record lifetime fix awaits live FOV retesting.");
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
    start_scoped_fov(module_base,log);
    log("EXPERIMENTAL anchored camera prototype installed. Native boom retraction is excluded from placement; eye collision/body visibility still need testing.");
    return true;
}
FovTelemetry latest_fov_telemetry() {
    FovTelemetry copy;
    if (!TryAcquireSRWLockShared(&fov_lock)) return copy;
    copy=fov_telemetry;ReleaseSRWLockShared(&fov_lock);return copy;
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
void publish_camera_control(bool active, Millis observed, const Settings& settings, bool allowed) {
    const auto now=clock_now();
    allowed=allowed && settings.enabled && valid(settings) && observed && now>=observed && now-observed<=150;
    AcquireSRWLockExclusive(&control_lock);
    if (settings!=control.settings || !allowed || (control.observed && observed<control.observed)) {
        transition.reset();++control.epoch;
    }
    control.active=active;control.allowed=allowed;control.observed=observed;control.settings=settings;
    control.blend=allowed ? transition.update(now,active,settings.transition_ms) : 0;
    debug_enabled.store(settings.debug,std::memory_order_relaxed);
    ReleaseSRWLockExclusive(&control_lock);
}
} // namespace efp
