#include "camera_geometry.hpp"
#include <cmath>

namespace efp {
namespace {
bool finite_position(const Vec3& position) {
    for (float value : position) if (!std::isfinite(value) || std::abs(value) > 1000000.0f) return false;
    return true;
}
float squared_distance(const Vec3& a, const Vec3& b) {
    float total = 0;
    for (unsigned i = 0; i < 3; ++i) { const auto d = a[i] - b[i]; total += d*d; }
    return total;
}
struct AnchorAxis { Vec3 direction; float separation; };
std::optional<AnchorAxis> anchor_axis(const Vec3& anchor, const Vec3& secondary) {
    if (!finite_position(anchor) || !finite_position(secondary)) return {};
    Vec3 up{};
    for (unsigned i = 0; i < 3; ++i) up[i] = anchor[i] - secondary[i];
    const float separation = std::sqrt(squared_distance(anchor, secondary));
    // Captured floor and wall pairs keep approximately 0.25 units of separation.
    // Their axis is a local-up candidate, not proof of player/head ownership.
    if (separation < 0.15f || separation > 0.35f) return {};
    return AnchorAxis{up, separation};
}
}
bool plausible_anchor(const Vec3& anchor, const Vec3& secondary, const Vec3& native_position) {
    return anchor_axis(anchor, secondary).has_value() && finite_position(native_position) &&
        squared_distance(anchor, native_position) <= 144.0f;
}
std::optional<Vec3> anchored_position(const Vec3& anchor, const Vec3& secondary, const Vec3& direction, const Settings& settings) {
    const auto axis = anchor_axis(anchor, secondary);
    if (!axis || !finite_position(direction) || !valid(settings)) return {};
    Vec3 up = axis->direction;
    for (auto& value : up) value /= axis->separation;
    const float length_squared = direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2];
    if (length_squared < 0.25f || length_squared > 4.0f) return {};
    const float norm = std::sqrt(length_squared);
    const Vec3 f = {direction[0]/norm,direction[1]/norm,direction[2]/norm};
    const float axial = -(reference_boom + settings.prototype_distance - settings.eye_forward);
    Vec3 position{};
    for (unsigned i = 0; i < 3; ++i)
        position[i] = anchor[i] + f[i]*axial + up[i]*(reference_height + settings.eye_height);
    const Vec3 right{up[1]*f[2] - up[2]*f[1],
        up[2]*f[0] - up[0]*f[2], up[0]*f[1] - up[1]*f[0]};
    const float lateral_length = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    // Looking along local up leaves no unambiguous lateral axis. Do not invent one.
    if (lateral_length > 0.01f) {
        const float lateral = (reference_side + settings.eye_side) / lateral_length;
        for (unsigned i = 0; i < 3; ++i) position[i] += right[i]*lateral;
    }
    if (!finite_position(position) || squared_distance(anchor,position) > max_eye_displacement*max_eye_displacement+0.00001f) return {};
    return position;
}
} // namespace efp
