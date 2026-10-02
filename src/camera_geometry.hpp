#pragma once
#include "core.hpp"
#include <array>
#include <optional>

namespace efp {
using Vec3 = std::array<float,3>;
// Fixed free-space reference measured in the user's 0.2.4 capture, not the current retracted boom.
inline constexpr float reference_boom = 6.0f;
inline constexpr float reference_height = 0.05f;
inline constexpr float reference_side = 0.10f;
inline constexpr float max_eye_displacement = 1.25f;
bool plausible_anchor(const Vec3& anchor, const Vec3& secondary, const Vec3& native_position);
// Use the matched pair's local-up candidate for height and lateral calibration.
std::optional<Vec3> anchored_position(const Vec3& anchor, const Vec3& secondary, const Vec3& direction, const Settings& settings);
} // namespace efp
