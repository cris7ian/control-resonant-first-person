#pragma once
#include "game_adapter.hpp"
#include <array>
namespace efp {
struct CameraTelemetry {
    Millis observed = 0;
    std::uintptr_t record_index = 0;
    bool input_before_readable = false;
    bool input_after_readable = false;
    bool anchor_valid = false;
    bool anchor_used = false;
    bool write_attempted = false;
    bool write_ok = false;
    std::size_t write_bytes = 0;
    float first_person_blend = 0;
    std::array<float,3> input0_before{}, input1_before{}, input0_after{}, input1_after{};
    std::array<float,3> direction{}, native_position{}, requested_position{};
};
// Diagnostic copies only; input vectors are candidates, not proven eye/pivot coordinates.
CameraTelemetry latest_camera_telemetry();
bool start_camera_prototype(std::uintptr_t module_base, const Log& log);
bool camera_record_recent();
// allowed distinguishes a manual exit (ease out) from a safety interruption (immediate native output).
void publish_camera_control(bool active, Millis state_observed, const Settings& settings, bool allowed = false);
struct FovTelemetry {
    Millis observed{};
    bool matched{}, applied{};
    float native_degrees{}, output_degrees{}, blend{};
    const char* reason = "awaiting render camera";
};
FovTelemetry latest_fov_telemetry();
} // namespace efp
