#pragma once
#include "core.hpp"
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace efp {
using MemoryReader = std::function<bool(std::uintptr_t, void*, std::size_t)>;
struct StackActivity {
    std::string name;
    std::string current;
    bool explicit_active = false;
    bool operator==(const StackActivity&) const = default;
};
struct StateSnapshot {
    bool readable = false;
    bool game_found = false;
    bool program_found = false;
    bool explicit_game_active = false;
    bool effective_game_active = false; // explicit activity OR activation inherited from parent stacks
    std::vector<StackActivity> activity_stacks;
    bool contains_combat = false;
    int game_current = -1;
    std::string game_top;
    std::string game_base;
    std::string program_top;
    std::string names;
    // Not read directly: operator== compares them so the coherent double read rejects changed non-top states.
    std::vector<std::string> game_states;
    std::vector<std::string> program_states;
    std::uintptr_t environment = 0;
    bool operator==(const StateSnapshot&) const = default;
};
StateSnapshot read_state_snapshot(std::uintptr_t environment, const MemoryReader& read);
GameState diagnostic_state(const StateSnapshot& snapshot, int camera_mode);
} // namespace efp
