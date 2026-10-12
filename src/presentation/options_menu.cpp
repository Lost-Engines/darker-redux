#include "presentation/options_menu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include "graphics/font.h"

namespace darker::presentation {
namespace {
enum class option_row : unsigned int {
  resolution, field_of_view, draw_distance, fog_start, fade_distance, fog_curve,
  minimum_light_size, background_fade, gouraud, defaults, apply, cancel, back, count
};
constexpr unsigned int setting_count{std::to_underlying(option_row::defaults)};
constexpr unsigned int row_count{std::to_underlying(option_row::count)};
constexpr int first_row{35};
constexpr int row_height{12};
constexpr std::array<std::array<int, 2>, 9> resolutions{{
  {320, 240}, {640, 480}, {960, 540}, {1280, 720}, {1280, 960},
  {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}
}};

std::string number(double value) {
  std::ostringstream text;
  text << std::fixed << std::setprecision(value == std::floor(value) ? 0 : 2) << value;
  auto result{text.str()};
  if(result.find('.') != std::string::npos && result.back() == '0') result.pop_back();
  return result;
}
} // namespace

bool display_preferences::valid() const noexcept {
  return width >= 64 && width <= 8192 && height >= 64 && height <= 8192
    && std::isfinite(field_of_view) && field_of_view >= 15 && field_of_view <= 140
    && draw_distance >= 2 && draw_distance <= 182
    && std::isfinite(fog_start) && std::isfinite(fade_distance)
    && fog_start >= 0 && fade_distance >= fog_start && fade_distance <= draw_distance
    && std::isfinite(fog_curve) && fog_curve >= 0
    && std::isfinite(minimum_light_size) && minimum_light_size >= 0;
}

std::filesystem::path preferences_path() {
#ifdef _WIN32
  if(auto const *root{std::getenv("APPDATA")}; root && *root) return std::filesystem::path{root} / "darker-redux/options.cfg";
#endif
  if(auto const *root{std::getenv("XDG_CONFIG_HOME")}; root && *root && std::filesystem::path{root}.is_absolute()) return std::filesystem::path{root} / "darker-redux/options.cfg";
  if(auto const *home{std::getenv("HOME")}; home && *home) return std::filesystem::path{home} / ".config/darker-redux/options.cfg";
  return "darker-redux-options.cfg";
}

display_preferences read_preferences(std::filesystem::path const &path) {
  display_preferences result;
  if(!std::filesystem::exists(path)) return result;
  std::ifstream input{path};
  if(!input) throw std::runtime_error{"Cannot read display preferences"};
  std::string line;
  while(std::getline(input, line)) {
    if(line.empty() || line.front() == '#') continue;
    auto const equals{line.find('=')};
    if(equals == std::string::npos) throw std::runtime_error{"Invalid display preferences"};
    auto const name{line.substr(0, equals)};
    std::istringstream value{line.substr(equals + 1)};
    if(name == "width") value >> result.width;
    else if(name == "height") value >> result.height;
    else if(name == "fov") value >> result.field_of_view;
    else if(name == "draw-distance") value >> result.draw_distance;
    else if(name == "fog-start") value >> result.fog_start;
    else if(name == "fade-distance") value >> result.fade_distance;
    else if(name == "fog-curve") value >> result.fog_curve;
    else if(name == "minimum-light-size") value >> result.minimum_light_size;
    else if(name == "background-fade") value >> result.background_fade;
    else if(name == "gouraud") value >> result.gouraud;
    else continue; // Allow newer versions to add preferences.
    if(value.fail() || (value >> std::ws, !value.eof())) throw std::runtime_error{"Invalid display preference: " + name};
  }
  if(!result.valid()) throw std::runtime_error{"Display preferences are outside supported ranges"};
  return result;
}

void write_preferences(std::filesystem::path const &path, display_preferences const &value) {
  if(!value.valid()) throw std::invalid_argument{"Invalid display preferences"};
  if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
  std::ofstream output{path};
  output << std::setprecision(17)
    << "# Darker Redux display preferences; command-line options override these.\n"
    << "width=" << value.width << "\nheight=" << value.height
    << "\nfov=" << value.field_of_view << "\ndraw-distance=" << value.draw_distance
    << "\nfog-start=" << value.fog_start << "\nfade-distance=" << value.fade_distance
    << "\nfog-curve=" << value.fog_curve << "\nminimum-light-size=" << value.minimum_light_size
    << "\nbackground-fade=" << value.background_fade << "\ngouraud=" << value.gouraud << '\n';
  output.close();
  if(!output) throw std::runtime_error{"Cannot save display preferences to " + path.string()};
}

void options_menu::open() {
  original = values;
  active = true;
  save_requested = false;
  selected = 0;
  notice.clear();
}

void options_menu::adjust(int const direction) {
  switch(static_cast<option_row>(selected)) {
  case option_row::resolution: {
    auto position{std::ranges::find_if(resolutions, [&](auto const &size){ return size[0] == values.width && size[1] == values.height; })};
    auto const index{position == resolutions.end() ? 0 : static_cast<int>(position - resolutions.begin())};
    auto const &size{resolutions[static_cast<unsigned int>(std::clamp(index + direction, 0, static_cast<int>(resolutions.size()) - 1))]};
    values.width = size[0];
    values.height = size[1];
    break;
  }
  case option_row::field_of_view:
    values.field_of_view = std::clamp(5 * (direction > 0 ? std::floor(values.field_of_view / 5) + 1 : std::ceil(values.field_of_view / 5) - 1), 15.0, 140.0);
    break;
  case option_row::draw_distance:
    values.draw_distance = static_cast<unsigned int>(std::clamp(static_cast<int>(values.draw_distance) + direction, 2, 182));
    values.fade_distance = std::min(values.fade_distance, static_cast<double>(values.draw_distance));
    values.fog_start = std::min(values.fog_start, values.fade_distance);
    break;
  case option_row::fog_start: values.fog_start = std::clamp(values.fog_start + direction, 0.0, values.fade_distance); break;
  case option_row::fade_distance:
    values.fade_distance = std::clamp(values.fade_distance + direction, values.fog_start, static_cast<double>(values.draw_distance));
    break;
  case option_row::fog_curve: {
    double const step{values.fog_curve < 20 || (direction < 0 && values.fog_curve <= 20) ? 1.0 : 10.0};
    values.fog_curve = std::clamp(step * (direction > 0 ? std::floor(values.fog_curve / step) + 1 : std::ceil(values.fog_curve / step) - 1), 0.0, 1000.0);
    break;
  }
  case option_row::minimum_light_size: values.minimum_light_size = std::clamp(values.minimum_light_size + direction * 0.25, 0.0, 8.0); break;
  case option_row::background_fade: values.background_fade = !values.background_fade; break;
  case option_row::gouraud: values.gouraud = !values.gouraud; break;
  default: break;
  }
}

void options_menu::key(options_key const key) {
  if(!active) return;
  notice.clear();
  switch(key) {
  case options_key::up: selected = (selected + row_count - 1) % row_count; break;
  case options_key::down: selected = (selected + 1) % row_count; break;
  case options_key::left: if(selected < setting_count) adjust(-1); break;
  case options_key::right: if(selected < setting_count) adjust(1); break;
  case options_key::cancel:
    values = original;
    save_requested = false;
    active = false;
    break;
  case options_key::accept:
    if(selected < setting_count) adjust(1);
    else if(selected == std::to_underlying(option_row::defaults)) values = display_preferences{};
    else if(selected == std::to_underlying(option_row::apply)) save_requested = true;
    else if(selected == std::to_underlying(option_row::cancel)) { values = original; active = false; }
    else active = false; // Keep the preview for this session, without writing preferences.
    break;
  }
}

void options_menu::point(int const x, int const y) {
  if(x >= 12 && x < 308 && y >= first_row && y < first_row + static_cast<int>(row_count) * row_height) {
    selected = static_cast<unsigned int>((y - first_row) / row_height);
  }
}

void options_menu::click(int const x, int const y) {
  if(x < 12 || x >= 308 || y < first_row || y >= first_row + static_cast<int>(row_count) * row_height) return;
  point(x, y);
  key(selected < setting_count && x < 235 && x >= 190 ? options_key::left : options_key::accept);
}

void options_menu::draw(std::span<framework::render::rgba_pixel> pixels, int const width, int const height,
  resources::font_resource const &font) const {
  framework::render::indexed_cockpit_framebuffer ink{};
  auto const text{[&](std::string_view value, int x, int y, uint8_t colour = 1){
    graphics::draw_text(ink, font, resources::font_face::interface, std::as_bytes(std::span{value}),
      {static_cast<int16_t>(x), static_cast<int16_t>(y)}, {
        .ink{colour},
        .edge{0}
      });
  }};
  text("DISPLAY OPTIONS", 95, 15, 2);
  std::array<std::string, row_count> const labels{
    "Resolution", "Vertical field of view", "Draw distance", "Fog starts", "Fog reaches full shade",
    "Fog curve", "Minimum light size", "Distant fade", "Smooth shading", "Restore defaults",
    "Apply and save", "Cancel changes", "Return without saving"
  };
  std::array<std::string, 9> const settings{
    std::to_string(values.width) + "x" + std::to_string(values.height), number(std::round(values.field_of_view * 10) / 10),
    number(values.draw_distance), number(values.fog_start), number(values.fade_distance), number(values.fog_curve),
    number(values.minimum_light_size), values.background_fade ? "Sky colour" : "Original", values.gouraud ? "On" : "Off"
  };
  for(unsigned int row{0}; row < row_count; ++row) {
    int const y{first_row + static_cast<int>(row) * row_height};
    text(labels[row], 20, y, row == selected ? 2 : 1);
    if(row < settings.size()) {
      text("<", 193, y);
      text(settings[row], 208, y, row == selected ? 2 : 1);
      text(">", 294, y);
    }
  }
  std::array<std::string_view, row_count> constexpr hints{
    "Render resolution; window size stays unchanged.", "Vertical angle in degrees. Original: 50.2.",
    "Map cells. Original: 15. Whole city: 182.", "Map cells before distance shading begins.",
    "Map cells to full shade, then distant fade.", "Higher values darken nearby objects faster.",
    "Pixels at 320x200. Zero disables the minimum.", "Blend the farthest Delphi silhouettes into sky.",
    "Gouraud shading. F9 also switches this in flight.", "Preview the recommended Redux defaults.",
    "Save these settings for future sessions.", "Restore the settings from when this menu opened.",
    "Keep changes until you quit, without saving."
  };
  text(notice.empty() ? hints[selected] : std::string_view{notice}, 13, 201);
  text("Arrows: adjust   Enter: select   Esc: cancel", 26, 217);
  text("Hold Tab to preview the unobscured scene", 36, 225);
  auto const viewport{framework::render::fit_viewport(width, height, 320, 240)};
  for(int y{0}; y < viewport.height; ++y) {
    int const source_y{y * 240 / viewport.height};
    if(source_y < 7 || source_y >= 234) continue;
    for(int x{0}; x < viewport.width; ++x) {
      int const source_x{x * 320 / viewport.width};
      if(source_x < 7 || source_x >= 313) continue;
      auto &pixel{pixels[static_cast<size_t>(y + viewport.y) * width + x + viewport.x]};
      auto const code{ink.pixels[static_cast<unsigned int>(source_y * 320 + source_x)]};
      bool const highlight{source_y >= first_row + static_cast<int>(selected) * row_height - 1
        && source_y < first_row + static_cast<int>(selected + 1) * row_height - 1};
      if(code) pixel = code == 2 ? framework::render::rgba_pixel{238, 220, 184} : framework::render::rgba_pixel{174, 170, 186};
      else if(source_x == 7 || source_x == 312 || source_y == 7 || source_y == 233) pixel = {108, 100, 120};
      else pixel = {static_cast<uint8_t>(pixel.r / 5 + (highlight ? 44 : 15)), static_cast<uint8_t>(pixel.g / 5 + (highlight ? 38 : 14)), static_cast<uint8_t>(pixel.b / 5 + (highlight ? 52 : 22))};
    }
  }
}

} // namespace darker::presentation
