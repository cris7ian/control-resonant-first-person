#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace efp {
using Millis = std::uint64_t;
enum class GameState { unknown, exploration, combat, protected_camera };
struct Settings {
    bool enabled = true;
    int pad_button = 267;
    int keyboard_key = 75; // K; double-tap fallback
    Millis double_tap_ms = 350;
    Millis max_tap_ms = 250;
    Millis min_gap_ms = 40;
    float prototype_distance = -6.35f;
    float eye_height = -0.15f;
    float eye_forward = -0.05f;
    float eye_side = 0.0f;
    float transition_ms = 180.0f;
    bool debug = false;
    bool operator==(const Settings&) const = default;
};
bool valid(const Settings& settings);
std::optional<Settings> parse_settings(std::string_view ini, const Settings& base = {});

class DoubleTap {
public:
    explicit DoubleTap(Settings settings = {}) : settings_(settings) {}
    bool update(Millis now, bool down);
    void reset();
    void configure(Settings settings);
private:
    Settings settings_;
    bool require_release_ = true;
    bool previous_down_ = false;
    std::optional<Millis> press_;
    std::optional<Millis> first_release_;
    std::optional<Millis> last_time_;
};

struct Context {
    GameState state = GameState::unknown;
    bool state_fresh = false;
    bool camera_valid = false;
    bool foreground = false;
    bool controller_connected = false;
    Millis state_observed = 0;
};
class CameraPolicy {
public:
    explicit CameraPolicy(Settings settings = {}) : settings_(settings), taps_(settings) {}
    bool update(Millis now, bool button_down, const Context& context);
    void configure(Settings settings);
    void reset();
    bool requested() const { return requested_; }
    bool active() const { return active_; }
private:
    Settings settings_;
    DoubleTap taps_;
    bool requested_ = false;
    bool active_ = false;
};
const char* state_name(GameState state);
} // namespace efp
