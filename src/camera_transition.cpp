#include "camera_transition.hpp"
#include <algorithm>
#include <cmath>

namespace efp {
float CameraTransition::value(Millis now) const {
    if (duration_ <= 0) return to_;
    const float t = std::clamp(static_cast<float>(now-started_)/duration_,0.0f,1.0f);
    const float eased = t*t*t*(t*(t*6.0f-15.0f)+10.0f); // zero velocity and acceleration at endpoints
    return std::clamp(from_+(to_-from_)*eased,0.0f,1.0f);
}
void CameraTransition::reset() { from_=to_=duration_=0; started_=last_=0; seen_=false; }
float CameraTransition::update(Millis now,bool first_person,float duration_ms) {
    if (!std::isfinite(duration_ms) || duration_ms<0 || duration_ms>500) { reset();return 0; }
    if (seen_ && now<last_) reset();
    const float current=seen_ ? value(now) : 0;
    if (!seen_ || to_!=(first_person ? 1.0f : 0.0f)) {
        from_=current;to_=first_person ? 1.0f : 0.0f;started_=now;
        duration_=duration_ms*std::abs(to_-from_); // short reversals do not take another full duration
    }
    seen_=true;last_=now;
    if (!duration_ms) { from_=to_;duration_=0; }
    return value(now);
}
bool camera_transition_allowed(const Settings& settings,const Context& c) {
    return settings.enabled && c.state==GameState::exploration && c.state_fresh && c.camera_valid &&
        c.foreground && c.controller_connected;
}
} // namespace efp
