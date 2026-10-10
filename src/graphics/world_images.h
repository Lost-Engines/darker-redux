#pragma once

#include "graphics/camera.h"

namespace darker::graphics {

template<typename Visitor>
void visit_world_images(int const camera_column, int const camera_row,
  int const column, int const row, unsigned int const radius, Visitor const &visit) {
  // Positions occupy one 256-cell world. With a radius smaller than one world
  // period, only that world and its eight neighbours can intersect the circle.
  static_assert(scene_limits::maximum_scan_radius_cells < scene_limits::world_period_cells);
  int const radius_squared{static_cast<int>(radius * radius)};
  for(int image_row{-1}; image_row <= 1; ++image_row) {
    int const dy{row - camera_row + image_row * scene_limits::world_period_cells};
    if(dy * dy > radius_squared) continue;
    for(int image_column{-1}; image_column <= 1; ++image_column) {
      int const dx{column - camera_column + image_column * scene_limits::world_period_cells};
      if(dx * dx + dy * dy > radius_squared) continue;
      visit(world_image{
        .column{image_column},
        .row{image_row},
      });
    }
  }
}

} // namespace darker::graphics
