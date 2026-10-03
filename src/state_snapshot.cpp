#include "state_snapshot.hpp"
#include <cstring>
#include <limits>
#include <vector>

namespace efp {
namespace {
template<class T> T field(const unsigned char* bytes, std::size_t offset) {
    T value{}; std::memcpy(&value, bytes + offset, sizeof(value)); return value;
}
bool address_valid(std::uintptr_t p, std::size_t size) {
    return p >= 0x10000 && p <= std::numeric_limits<std::uintptr_t>::max() - size;
}
bool engine_string(const unsigned char* bytes, const MemoryReader& read, std::string& out) {
    const auto meta = field<std::uint32_t>(bytes, 0);
    const auto length = field<std::uint32_t>(bytes, 4);
    if (length > 63) return false;
    const auto scaled = (meta & 0xffffffU) * 8;
    const auto encoded = meta >> 24;
    const auto threshold = encoded < 0x80 ? encoded * 8 : (encoded << 8) - 0x7c00;
    out.resize(length);
    if (scaled <= threshold) {
        if (length > 32) return false;
        std::memcpy(out.data(), bytes + 8, length);
    } else {
        const auto pointer = field<std::uintptr_t>(bytes, 8);
        if (!address_valid(pointer, length) || !read(pointer, out.data(), length)) return false;
    }
    for (const auto c : out) if (c < 0x20 || c > 0x7e) return false;
    return true;
}
bool same_name(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto fold = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
        if (fold(a[i]) != fold(b[i])) return false;
    }
    return true;
}
bool effective_active(std::size_t target, const std::vector<StackActivity>& stacks, std::array<bool,256>& visiting) {
    if (stacks[target].explicit_active) return true;
    if (visiting[target]) return false;
    visiting[target] = true;
    // Native RVA 0x17D8680: a stack inherits activity when an active parent
    // stack's current state names it. Zero explicit flag does not mean inactive.
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if (same_name(stacks[i].name, stacks[target].name)) continue;
        if (same_name(stacks[i].current, stacks[target].name) && effective_active(i, stacks, visiting)) {
            visiting[target] = false; return true;
        }
    }
    visiting[target] = false;
    return false;
}
}
static StateSnapshot read_snapshot_once(std::uintptr_t environment, const MemoryReader& read) {
    StateSnapshot s; s.environment = environment;
    std::uintptr_t header{};
    std::array<unsigned char, 16> before{}, after{};
    if (!address_valid(environment, sizeof(header)) || !read(environment, &header, sizeof(header)) ||
        !address_valid(header, before.size()) || !read(header, before.data(), before.size())) return s;
    const auto base = field<std::uintptr_t>(before.data(), 0);
    const auto count = field<std::uint32_t>(before.data(), 8);
    if (count == 0 || count > 256 || !address_valid(base, count * 0xf0ULL)) return s;
    std::vector<unsigned char> stacks(count * 0xf0ULL);
    if (!read(base, stacks.data(), stacks.size())) return s;
    std::array<unsigned char, 0xf0> game_before{}, program_before{};
    std::uintptr_t game_address{}, program_address{};
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* stack = stacks.data() + i * 0xf0;
        std::string name;
        if (!engine_string(stack, read, name)) return s;
        if (s.names.size() < 4096) s.names += (s.names.empty() ? "" : ",") + name;
        const bool game = name == "game";
        const bool program = name == "program_flow";
        const bool critical = game || program;
        StackActivity activity{name, {}, stack[0xb8] != 0};
        if (critical) {
            if ((game && s.game_found) || (program && s.program_found)) return s;
            if (game) { s.game_found = true; game_address = base + i * 0xf0; std::memcpy(game_before.data(), stack, 0xf0); }
            else { s.program_found = true; program_address = base + i * 0xf0; std::memcpy(program_before.data(), stack, 0xf0); }
        }
        const auto index = field<std::int32_t>(stack, 0xbc);
        const auto state_base = field<std::uintptr_t>(stack, 0xc0);
        const auto state_count = field<std::uint32_t>(stack, 0xc8);
        if (state_count > (critical ? 64U : 512U)) return s;
        if (state_count == 0 || index < 0 || static_cast<std::uint32_t>(index) >= state_count) {
            if (critical) return s;
            s.activity_stacks.push_back(std::move(activity)); continue;
        }
        if (!address_valid(state_base, state_count * 0x28ULL)) return s;
        const auto first = critical ? 0U : static_cast<std::uint32_t>(index);
        const auto last = critical ? state_count : first + 1;
        std::vector<unsigned char> states((last - first) * 0x28ULL);
        if (!read(state_base + first * 0x28ULL, states.data(), states.size())) return s;
        for (std::uint32_t n = first; n < last; ++n) {
            std::string state;
            if (!engine_string(states.data() + (n - first) * 0x28, read, state)) return s;
            if (n == static_cast<std::uint32_t>(index)) activity.current = state;
            if (game) {
                s.game_states.push_back(state);
                if (state == "combat" && n <= static_cast<std::uint32_t>(index)) s.contains_combat = true;
                if (n == 0) s.game_base = state;
                if (n == static_cast<std::uint32_t>(index)) s.game_top = state;
            } else if (program) {
                s.program_states.push_back(state);
                if (n == static_cast<std::uint32_t>(index)) s.program_top = state;
            }
        }
        if (game) {
            s.game_current = index;
            s.explicit_game_active = activity.explicit_active;
        }
        s.activity_stacks.push_back(std::move(activity));
    }
    if (!read(header, after.data(), after.size()) || before != after) return s;
    std::uintptr_t header_after{};
    if (!read(environment, &header_after, sizeof(header_after)) || header != header_after) return s;
    // Reject changed critical stack headers; do not retain potentially replaced pointers.
    for (const auto& pair : {std::pair{game_address, game_before}, std::pair{program_address, program_before}}) {
        if (!pair.first) continue;
        std::array<unsigned char, 0xf0> check{};
        if (!read(pair.first, check.data(), check.size()) ||
            std::memcmp(pair.second.data(), check.data(), 0x28) != 0 ||
            std::memcmp(pair.second.data() + 0xb8, check.data() + 0xb8, 0x14) != 0) return s;
    }
    std::array<bool,256> visiting{};
    for (std::size_t i = 0; i < s.activity_stacks.size(); ++i) {
        if (s.activity_stacks[i].name == "game") s.effective_game_active = effective_active(i, s.activity_stacks, visiting);
    }
    s.readable = true;
    return s;
}
StateSnapshot read_state_snapshot(std::uintptr_t environment, const MemoryReader& read) {
    auto first = read_snapshot_once(environment, read);
    if (!first.readable) return first;
    const auto second = read_snapshot_once(environment, read);
    // Re-read decoded heap/inline contents, not only stable pointers and indices.
    // Matching reads mitigate races; they do not establish atomic engine ownership.
    if (!(first == second)) first.readable = false;
    return first;
}
GameState diagnostic_state(const StateSnapshot& s, int mode) {
    if (!s.readable || !s.game_found || !s.program_found) return GameState::unknown;
    // Combat beneath a menu still clears intent.
    if (s.contains_combat) return GameState::combat;
    if (s.program_top != "game" || !s.effective_game_active || mode != 0) return GameState::protected_camera;
    if (s.game_top == "exploration") return GameState::exploration;
    // Observed dialogue-capable free-roam area. Do not admit nested story overlays.
    if (s.game_top == "story" && s.game_current == 0 && s.game_base == "story") return GameState::exploration;
    return GameState::protected_camera;
}
} // namespace efp
