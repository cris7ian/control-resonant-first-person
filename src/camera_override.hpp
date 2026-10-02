#pragma once
#include "game_adapter.hpp"
namespace efp {
bool start_camera_prototype(std::uintptr_t module_base, const Log& log);
bool camera_record_recent();
void publish_camera_control(bool active, Millis state_observed, const Settings& settings);
} // namespace efp
