#pragma once
#include "game_adapter.hpp"
#include "state_snapshot.hpp"

namespace efp {
bool start_state_observer(std::uintptr_t module_base, const Log& log);
StateSnapshot latest_state_snapshot(Millis& observed_at);
} // namespace efp
