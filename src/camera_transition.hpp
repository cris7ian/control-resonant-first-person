#pragma once
#include "core.hpp"

namespace efp {
// Frame-rate independent, bounded scalar blend; callers reset it on safety/settings changes.
class CameraTransition {
public:
    float update(Millis now, bool first_person, float duration_ms);
    void reset();
private:
    float value(Millis now) const;
    float from_{}, to_{}, duration_{};
    Millis started_{}, last_{};
    bool seen_ = false;
};
bool camera_transition_allowed(const Settings& settings, const Context& context);
} // namespace efp
