#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include "graphics/model_renderer.h"

namespace darker::graphics {

struct fog_range {
  double start_cells{1};
  double end_cells{scene_limits::surface_radius_cells};
  double curve{100}; // zero is linear; positive values use log(1 + curve * distance) / log(1 + curve)
  std::optional<double> background_end_cells{}; // optional second stage, after the original fog reaches end_cells
};

class distance_shading {
private:
  std::vector<std::array<uint8_t, model_colours::shade_count>> tables;
  uint32_t start_depth{0};
  uint32_t end_depth{0};
  uint32_t background_end_depth{0};
  std::vector<uint16_t> curved_depths;
  std::vector<framework::render::colour_translation> fog_tables;

public:
  explicit distance_shading(unsigned int count = scene_limits::distance_shade_tables);
  explicit distance_shading(fog_range range);
  void set_fog_palette(framework::render::colour_palette const *palette, uint8_t background);
  model_colours colours(render_geometry::coordinate_bits depth, model_path path, uint8_t light) const;
};

} // namespace darker::graphics
