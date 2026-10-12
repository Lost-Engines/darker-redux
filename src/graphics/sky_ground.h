#pragma once

#include "graphics/camera.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

inline uint8_t constexpr ground_background_colour{127};

void draw_sky_ground(framework::render::indexed_surface target, camera_angles angles, screen_vertex const &origin, int bottom, double focal_length = render_geometry::focal_length, maths::angle_precision precision = maths::angle_precision::full);

} // namespace darker::graphics
