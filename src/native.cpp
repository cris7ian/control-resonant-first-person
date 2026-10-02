#include "core.hpp"
#include "game_adapter.hpp"
#include "state_observer.hpp"
#include "camera_override.hpp"
#include <windows.h>
#include <xinput.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {
HMODULE own_module{};
std::filesystem::path own_directory;
std::ofstream logfile;
void log(const std::string& message) {
    SYSTEMTIME time{}; GetLocalTime(&time);
    char stamp[64]{};
    std::snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u ",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
    logfile << stamp << message << '\n'; logfile.flush();
}
std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > 65536) return {};
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (text.size() != size || stream.bad()) return {};
    return text;
}
bool foreground() {
    DWORD owner{};
    const auto window = GetForegroundWindow();
    if (!window) return false;
    GetWindowThreadProcessId(window, &owner);
    return owner == GetCurrentProcessId();
}
std::string camera_geometry(const efp::CameraTelemetry& sample) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(3);
    text << "CAMERA GEOMETRY: record=" << sample.record_index
         << "; write=" << (sample.write_attempted ? (sample.write_ok ? "ok" : "failed") : "inactive")
         << "; write_bytes=" << sample.write_bytes
         << "; input_before=" << (sample.input_before_readable ? "readable" : "unavailable")
         << "; input_after=" << (sample.input_after_readable ? "readable" : "unavailable");
    const auto vector = [&text](const char* name, const std::array<float,3>& value) {
        text << "; " << name << "=(" << value[0] << ',' << value[1] << ',' << value[2] << ')';
    };
    if (sample.input_before_readable) { vector("input0_before", sample.input0_before); vector("input1_before", sample.input1_before); }
    if (sample.input_after_readable) { vector("input0_after", sample.input0_after); vector("input1_after", sample.input1_after); }
    vector("native", sample.native_position); vector("requested", sample.requested_position); vector("axis", sample.direction);
    return text.str();
}
using GetState = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
GetState load_xinput() {
    // Absolute system path prevents loading an unrelated local XInput DLL.
    std::array<wchar_t, 32768> system{};
    if (!GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()))) return nullptr;
    for (const auto name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
        auto module = GetModuleHandleW(name);
        if (!module) module = LoadLibraryW((std::filesystem::path(system.data()) / name).c_str());
        if (!module) continue;
        const auto function = reinterpret_cast<GetState>(GetProcAddress(module, "XInputGetState"));
        if (function) return function;
    }
    return nullptr;
}
bool pad_down(const XINPUT_GAMEPAD& pad, int code) {
    constexpr std::array<WORD, 16> masks = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0,
        XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT};
    if (code < 256 || code > 271) return false;
    if (code == 262) return pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    if (code == 263) return pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    return (pad.wButtons & masks[static_cast<std::size_t>(code - 256)]) != 0;
}
DWORD WINAPI run(void*) {
    try {
        // Loader and menu require lifetime until process exit. No hot unloading.
        HMODULE pinned{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&run), &pinned)) return 1;
        std::array<wchar_t, 32768> name{};
        const auto length = GetModuleFileNameW(own_module, name.data(), static_cast<DWORD>(name.size()));
        if (!length || length >= name.size()) return 1;
        own_directory = std::filesystem::path(name.data()).parent_path();
        logfile.open(own_directory / "ExplorationFirstPerson.log", std::ios::app);
        if (!logfile) return 1;
        const bool preview = GetPrivateProfileIntW(L"Safety", L"camera_writes", 0, (own_directory / "ExplorationFirstPerson.ini").c_str()) == 1;
        log(std::string("Exploration First Person ") + EFP_VERSION + (preview ? ": EXPERIMENTAL camera-offset preview." : ": diagnostic mode; camera writes disabled."));
        log("Steam Input Xbox layout; manual double-tap reactivation after combat. No physical HID polling.");
        efp::GameAdapter adapter;
        adapter.initialize(log, preview);
        const auto get_state = load_xinput();
        log(get_state ? "XInput backend ready." : "XInput unavailable; controller gestures disabled.");
        efp::Settings settings;
        efp::DoubleTap diagnostic_taps(settings);
        efp::CameraPolicy policy(settings);
        std::optional<std::string> pending_ini;
        std::optional<std::string> accepted_ini;
        efp::Millis last_config{}, last_heartbeat{}, last_geometry{}, last_geometry_observed{};
        int selected = -1;
        int last_mode = -999;
        bool last_focus = false;
        bool last_camera_active = false;
        auto last_state = efp::GameState::unknown;
        std::string last_stack_summary;
        std::uintptr_t last_environment = 0;
        for (;;) {
            const auto now = GetTickCount64();
            if (now - last_config >= 500) {
                last_config = now;
                auto contents = read_file(own_directory / "ModMenuConfig/exploration_first_person.ini");
                if (contents && contents == pending_ini && contents != accepted_ini) {
                    if (const auto parsed = efp::parse_settings(*contents, settings)) {
                        settings = *parsed; policy.configure(settings); diagnostic_taps.configure(settings);
                        accepted_ini = contents; log("Validated settings applied; gesture history cleared.");
                    } else if (settings.debug) log("Incomplete or invalid settings ignored; keeping previous values.");
                }
                pending_ini = contents;
            }
            XINPUT_STATE state{};
            if (selected >= 0 && (!get_state || get_state(static_cast<DWORD>(selected), &state) != ERROR_SUCCESS)) {
                selected = -1; diagnostic_taps.reset(); policy.reset(); log("Controller disconnected; intent cleared.");
            }
            if (selected < 0 && get_state) {
                for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
                    if (get_state(index, &state) == ERROR_SUCCESS) {
                        selected = static_cast<int>(index); diagnostic_taps.reset(); policy.reset();
                        log("Selected XInput slot " + std::to_string(selected) + "; release RS before double-tapping."); break;
                    }
                }
            }
            const bool focused = foreground();
            if (focused != last_focus) {
                diagnostic_taps.reset(); policy.reset(); last_focus = focused;
                log(focused ? "Game foreground; input detector rearmed after release." : "Focus lost; input and intent cleared.");
            }
            bool button = selected >= 0 && pad_down(state.Gamepad, settings.pad_button);
            if (settings.keyboard_key) button = button || ((GetAsyncKeyState(settings.keyboard_key) & 0x8000) != 0);
            auto context = adapter.context();
            context.foreground = focused;
            context.controller_connected = selected >= 0 || settings.keyboard_key != 0;
            const bool camera_active = policy.update(now, button, context);
            efp::publish_camera_control(camera_active, context.state_observed, settings);
            if (camera_active != last_camera_active) {
                last_camera_active = camera_active;
                log(camera_active ? "CAMERA PREVIEW ON (double-tap)." : "CAMERA PREVIEW OFF (manual toggle or safety/combat rollback).");
            }
            if (context.state != last_state) { last_state = context.state; log(std::string("Observed gameplay state (diagnostic): ") + efp::state_name(last_state)); }
            efp::Millis observed{};
            const auto snapshot = efp::latest_state_snapshot(observed);
            if (snapshot.readable && snapshot.environment != last_environment) {
                last_environment = snapshot.environment;
                log("UI stack names: " + snapshot.names);
            }
            const auto summary = std::string(snapshot.readable ? "readable" : "unavailable") + "; program=" + snapshot.program_top +
                "; game_base=" + snapshot.game_base + "; game_top=" + snapshot.game_top +
                "; combat_present=" + (snapshot.contains_combat ? "yes" : "no") +
                "; explicit_active=" + (snapshot.explicit_game_active ? "yes" : "no") +
                "; effective_active=" + (snapshot.effective_game_active ? "yes" : "no");
            if (summary != last_stack_summary) { last_stack_summary = summary; log("UI snapshot: " + summary); }
            if (focused && context.controller_connected && settings.enabled) {
                if (diagnostic_taps.update(now, button)) {
                    log(std::string("Double-tap observed; state=") + efp::state_name(context.state) +
                        "; camera_record=" + (context.camera_valid ? "eligible" : "unavailable") + "; preview=" + (camera_active ? "on" : "off"));
                }
            } else diagnostic_taps.reset();
            if (settings.debug && now - last_geometry >= 500) {
                last_geometry = now;
                const auto sample = efp::latest_camera_telemetry();
                if (sample.observed && sample.observed != last_geometry_observed && now >= sample.observed && now - sample.observed <= 200) {
                    last_geometry_observed = sample.observed;
                    log(camera_geometry(sample));
                }
            }
            const int mode = adapter.camera_mode();
            if (mode != last_mode) { last_mode = mode; log("Native camera mode mirror: " + std::to_string(mode) + " (semantics unvalidated)"); }
            if (now - last_heartbeat >= 5000) {
                last_heartbeat = now;
                log(std::string("Heartbeat: state=") + efp::state_name(context.state) + "; preview=" +
                    (camera_active ? "on" : "off") + "; camera_record=" + (context.camera_valid ? "eligible" : "unavailable") +
                    "; controller=" + (selected >= 0 ? "connected" : "not detected"));
            }
            Sleep(10);
        }
    } catch (const std::exception& error) { efp::publish_camera_control(false, 0, {}); if (logfile) log(std::string("Worker stopped; preview disabled: ") + error.what()); }
    catch (...) { efp::publish_camera_control(false, 0, {}); if (logfile) log("Worker stopped; preview disabled after an unexpected exception."); }
    return 1;
}
}
extern "C" BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        own_module = module;
        DisableThreadLibraryCalls(module);
        const auto thread = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
