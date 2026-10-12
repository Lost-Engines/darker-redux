#pragma once

#include <cstdint>
#include "maths/angle.h"

namespace darker::maths {

// Full 16-bit angular sampling, rounded mathematical sine with symmetric peaks.
// Generated once, shared by all render orientations; simulation retains angle_sine.
int16_t render_sine(uint16_t angle) noexcept;
int16_t render_cosine(uint16_t angle) noexcept;

} // namespace darker::maths
