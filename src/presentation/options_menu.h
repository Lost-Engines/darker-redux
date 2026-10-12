#pragma once

#include <filesystem>
#include <cmath>
#include <numbers>
#include "graphics/model_lighting.h"
#include <span>
#include <string>
#include "render/framebuffer.h"
#include "resources/font.h"

namespace darker::presentation {

struct display_preferences {
  int width{320};
  int height{240};
  double field_of_view{2 * std::atan(120.0 / 256.0) * 180 / std::numbers::pi};
  unsigned int draw_distance{graphics::scene_limits::surface_radius_cells};
  double fog_start{graphics::fog_range{}.start_cells};
  double fade_distance{graphics::scene_limits::surface_fade_cells};
  double fog_curve{graphics::fog_range{}.curve};
  double minimum_light_size{0.5};
  bool background_fade{true};
  bool gouraud{true};

  bool valid() const noexcept;
  bool operator==(display_preferences const&) const = default;
};

std::filesystem::path preferences_path();
display_preferences read_preferences(std::filesystem::path const &path);
void write_preferences(std::filesystem::path const &path, display_preferences const &value);

enum class options_key { up, down, left, right, accept, cancel };

class options_menu {
  display_preferences original;
  unsigned int selected{0};
  void adjust(int direction);

public:
  display_preferences values;
  bool active{false};
  bool save_requested{false};
  std::string notice;

  void open();
  void key(options_key key);
  void point(int x, int y);
  void click(int x, int y);
  void draw(std::span<framework::render::rgba_pixel> pixels, int width, int height,
    resources::font_resource const &font) const;
};

} // namespace darker::presentation
