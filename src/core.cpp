#include "core.hpp"
#include "camera_geometry.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>

namespace efp {
namespace {
std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}
std::optional<int> integer(std::string_view s) {
    int result{};
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    if (ec != std::errc{} || end != s.data() + s.size()) return {};
    return result;
}
std::optional<float> number(std::string_view s) {
    float result{};
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    if (ec != std::errc{} || end != s.data() + s.size() || !std::isfinite(result)) return {};
    return result;
}
}
bool valid(const Settings& s) {
    return (s.pad_button == 0 || (s.pad_button >= 256 && s.pad_button <= 271)) &&
        (s.keyboard_key == 0 || (s.keyboard_key >= 3 && s.keyboard_key <= 254)) &&
        s.double_tap_ms >= 150 && s.double_tap_ms <= 800 &&
        s.max_tap_ms >= 50 && s.max_tap_ms <= 500 &&
        s.min_gap_ms >= 10 && s.min_gap_ms <= 100 && s.min_gap_ms < s.double_tap_ms &&
        std::isfinite(s.prototype_distance) && s.prototype_distance >= -7.0f && s.prototype_distance <= -5.0f &&
        std::isfinite(s.eye_side) && s.eye_side >= -1.0f && s.eye_side <= 1.0f &&
        std::isfinite(s.eye_height) && s.eye_height >= -0.5f && s.eye_height <= 0.5f &&
        std::isfinite(s.eye_forward) && s.eye_forward >= -0.3f && s.eye_forward <= 0.3f &&
        std::isfinite(s.transition_ms) && s.transition_ms >= 0 && s.transition_ms <= 500 &&
        std::isfinite(s.first_person_fov) && s.first_person_fov >= 60 && s.first_person_fov <= 120 &&
        std::abs(reference_boom + s.prototype_distance - s.eye_forward) +
        std::abs(reference_height + s.eye_height) + std::abs(reference_side + s.eye_side) <= max_eye_displacement + 0.00001f;
}
std::optional<Settings> parse_settings(std::string_view ini, const Settings& base) {
    if (!valid(base)) return {};
    Settings result = base;
    bool settings = false;
    std::set<std::string> seen;
    // CRModMenu writes UTF-8 files; tolerate a UTF-8 BOM.
    if (ini.starts_with("\xEF\xBB\xBF")) ini.remove_prefix(3);
    while (!ini.empty()) {
        const auto eol = ini.find('\n');
        auto line = trim(ini.substr(0, eol));
        if (eol == std::string_view::npos) ini = {}; else ini.remove_prefix(eol + 1);
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[') {
            if (line.back() != ']') return {};
            settings = line == "[Settings]";
            continue;
        }
        if (!settings) continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) return {};
        const auto key = trim(line.substr(0, equals));
        const auto text = trim(line.substr(equals + 1));
        if (key.empty() || text.empty() || !seen.insert(std::string(key)).second) return {};
        const auto i = integer(text);
        if (key == "enabled" || key == "debug" || key == "first_person_fov_enabled") {
            if (!i || (*i != 0 && *i != 1)) return {};
            if (key=="enabled") result.enabled=*i!=0;
            else if (key=="debug") result.debug=*i!=0;
            else result.first_person_fov_enabled=*i!=0;
        } else if (key == "pad_button" || key == "keyboard_key") {
            if (!i) return {};
            (key == "pad_button" ? result.pad_button : result.keyboard_key) = *i;
        } else if (key == "double_tap_ms" || key == "max_tap_ms" || key == "min_gap_ms") {
            if (!i || *i < 0) return {};
            if (key == "double_tap_ms") result.double_tap_ms = static_cast<Millis>(*i);
            else if (key == "max_tap_ms") result.max_tap_ms = static_cast<Millis>(*i);
            else result.min_gap_ms = static_cast<Millis>(*i);
        } else if (key == "prototype_distance" || key == "eye_height" || key == "eye_forward" || key == "eye_side" || key == "transition_ms" || key == "first_person_fov") {
            const auto f = number(text);
            if (!f) return {};
            if (key == "prototype_distance") result.prototype_distance = *f;
            else if (key == "eye_height") result.eye_height = *f;
            else if (key == "eye_forward") result.eye_forward = *f;
            else if (key == "eye_side") result.eye_side = *f;
            else if (key=="transition_ms") result.transition_ms = *f;
            else result.first_person_fov = *f;
        }
    }
    // A partial/missing section must not reset current settings.
    if (!settings || seen.empty() || !valid(result)) return {};
    return result;
}
void DoubleTap::reset() {
    require_release_ = true;
    previous_down_ = false;
    press_.reset(); first_release_.reset(); last_time_.reset();
}
void DoubleTap::configure(Settings settings) { settings_ = settings; reset(); }
bool DoubleTap::update(Millis now, bool down) {
    if (last_time_ && now < *last_time_) { reset(); last_time_ = now; return false; }
    last_time_ = now;
    if (require_release_) {
        if (!down) require_release_ = false;
        previous_down_ = down;
        return false;
    }
    if (first_release_ && now - *first_release_ > settings_.double_tap_ms) first_release_.reset();
    bool fire = false;
    if (down && !previous_down_) {
        if (first_release_ && now - *first_release_ < settings_.min_gap_ms) {
            first_release_.reset(); press_.reset(); require_release_ = true;
        } else press_ = now;
    } else if (!down && previous_down_ && press_) {
        if (now - *press_ <= settings_.max_tap_ms) {
            if (first_release_ && now - *first_release_ <= settings_.double_tap_ms) {
                fire = true; first_release_.reset();
            } else first_release_ = now;
        } else first_release_.reset();
        press_.reset();
    }
    previous_down_ = down;
    return fire;
}
void CameraPolicy::reset() { requested_ = false; active_ = false; taps_.reset(); }
void CameraPolicy::configure(Settings settings) {
    if (settings == settings_) return;
    settings_ = settings; taps_.configure(settings); requested_ = false; active_ = false;
}
bool CameraPolicy::update(Millis now, bool down, const Context& c) {
    if (!settings_.enabled || !c.state_fresh || c.state == GameState::unknown ||
        c.state == GameState::combat || !c.foreground || !c.controller_connected || !c.camera_valid) {
        reset(); return false;
    }
    if (c.state != GameState::exploration) {
        taps_.reset(); active_ = false; return false;
    }
    if (taps_.update(now, down)) requested_ = !requested_;
    active_ = requested_;
    return active_;
}
const char* state_name(GameState s) {
    switch (s) {
    case GameState::exploration: return "exploration";
    case GameState::combat: return "combat";
    case GameState::protected_camera: return "protected";
    default: return "unknown";
    }
}
} // namespace efp
