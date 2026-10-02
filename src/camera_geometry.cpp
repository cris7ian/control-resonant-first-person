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
}
bool plausible_anchor(const Vec3& anchor, const Vec3& secondary, const Vec3& native_position) {
    if (!finite_position(anchor) || !finite_position(secondary) || !finite_position(native_position)) return false;
    // Observed pair: same horizontal location, first 0.25 units above second.
    const float separation = anchor[1] - secondary[1];
    return std::abs(anchor[0]-secondary[0]) <= 0.1f && std::abs(anchor[2]-secondary[2]) <= 0.1f &&
        separation >= 0.15f && separation <= 0.35f && squared_distance(anchor, native_position) <= 144.0f;
}
std::optional<Vec3> anchored_position(const Vec3& anchor, const Vec3& direction, const Settings& settings) {
    if (!finite_position(anchor) || !finite_position(direction) || !valid(settings)) return {};
    const float length_squared = direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2];
    if (length_squared < 0.25f || length_squared > 4.0f) return {};
    const float norm = std::sqrt(length_squared);
    const Vec3 f = {direction[0]/norm,direction[1]/norm,direction[2]/norm};
    const float axial = -(reference_boom + settings.prototype_distance - settings.eye_forward);
    Vec3 position = {anchor[0]+f[0]*axial,anchor[1]+f[1]*axial+reference_height+settings.eye_height,anchor[2]+f[2]*axial};
    const float horizontal = std::hypot(f[0],f[2]);
    if (horizontal > 0.01f) {
        const float lateral = (reference_side + settings.eye_side) / horizontal;
        position[0] += f[2]*lateral; position[2] -= f[0]*lateral;
    }
    if (!finite_position(position) || squared_distance(anchor,position) > max_eye_displacement*max_eye_displacement+0.00001f) return {};
    return position;
}
} // namespace efp
