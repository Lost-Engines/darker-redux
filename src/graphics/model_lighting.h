#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "graphics/model_renderer.h"

namespace darker::graphics {

struct fog_range {
  double start_cells{0};
  double end_cells{scene_limits::surface_radius_cells};
};

class distance_shading {
private:
  std::vector<std::array<uint8_t, model_colours::shade_count>> tables;
  uint32_t start_depth{0};
  uint32_t end_depth{0};

public:
  explicit distance_shading(unsigned int count = scene_limits::distance_shade_tables);
  explicit distance_shading(fog_range range);
  model_colours colours(render_geometry::coordinate_bits depth, model_path path, uint8_t light) const;
};

} // namespace darker::graphics
