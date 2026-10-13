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
#include <GLFW/glfw3.h>
#include "graphics/font.h"

namespace darker::presentation {
namespace {
enum class option_row : unsigned int {
  resolution, field_of_view, draw_distance, fog_start, fade_distance, fog_curve,
  minimum_light_size, background_fade, gouraud, controls, defaults, apply, cancel, back, count
};
constexpr unsigned int setting_count{std::to_underlying(option_row::controls)};
constexpr unsigned int row_count{std::to_underlying(option_row::count)};
constexpr int first_row{35};
constexpr int row_height{11};
constexpr unsigned int visible_rows{14};
constexpr int scrollbar_left{305};
constexpr int scrollbar_right{310};
constexpr unsigned int control_visible_rows{12};
constexpr int list_bottom{first_row + row_height * static_cast<int>(control_visible_rows)};
constexpr int footer_top{176};
constexpr unsigned int mouse_setting_count{2};
constexpr unsigned int controls_defaults{mouse_setting_count + std::to_underlying(input::control_action::count)};
constexpr unsigned int controls_back{controls_defaults + 1};
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

bool preferences::valid() const noexcept {
  return width >= 64 && width <= 8192 && height >= 64 && height <= 8192
    && std::isfinite(field_of_view) && field_of_view >= 15 && field_of_view <= 140
    && draw_distance >= 2 && draw_distance <= 182
    && std::isfinite(fog_start) && std::isfinite(fade_distance)
    && fog_start >= 0 && fade_distance >= fog_start && fade_distance <= draw_distance
    && std::isfinite(fog_curve) && fog_curve >= 0
    && std::isfinite(minimum_light_size) && minimum_light_size >= 0 && controls.valid();
}

std::filesystem::path preferences_path() {
#ifdef _WIN32
  if(auto const *root{std::getenv("APPDATA")}; root && *root) return std::filesystem::path{root} / "darker-redux/options.cfg";
#endif
  if(auto const *root{std::getenv("XDG_CONFIG_HOME")}; root && *root && std::filesystem::path{root}.is_absolute()) return std::filesystem::path{root} / "darker-redux/options.cfg";
  if(auto const *home{std::getenv("HOME")}; home && *home) return std::filesystem::path{home} / ".config/darker-redux/options.cfg";
  return "darker-redux-options.cfg";
}

preferences read_preferences(std::filesystem::path const &path) {
  preferences result;
  if(!std::filesystem::exists(path)) return result;
  std::ifstream input{path};
  if(!input) throw std::runtime_error{"Cannot read preferences"};
  std::string line;
  while(std::getline(input, line)) {
    if(line.empty() || line.front() == '#') continue;
    auto const equals{line.find('=')};
    if(equals == std::string::npos) throw std::runtime_error{"Invalid preferences"};
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
    else if(name == "mouse-sensitivity") value >> result.controls.mouse_sensitivity;
    else if(name == "invert-mouse") value >> result.controls.invert_mouse;
    else if(name.starts_with("bind.")) {
      auto const descriptions{input::control_descriptions()};
      auto const found{std::ranges::find(descriptions, std::string_view{name}.substr(5), &input::control_description::name)};
      if(found == descriptions.end()) continue;
      value >> result.controls.keys[static_cast<unsigned int>(found - descriptions.begin())];
    }
    else continue; // Allow newer versions to add preferences.
    if(value.fail() || (value >> std::ws, !value.eof())) throw std::runtime_error{"Invalid preference: " + name};
  }
  if(!result.valid()) throw std::runtime_error{"Preferences are outside supported ranges"};
  return result;
}

void write_preferences(std::filesystem::path const &path, preferences const &value) {
  if(!value.valid()) throw std::invalid_argument{"Invalid preferences"};
  if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
  std::ofstream output{path};
  output << std::setprecision(17)
    << "# Darker Redux preferences; command-line options override these.\n"
    << "width=" << value.width << "\nheight=" << value.height
    << "\nfov=" << value.field_of_view << "\ndraw-distance=" << value.draw_distance
    << "\nfog-start=" << value.fog_start << "\nfade-distance=" << value.fade_distance
    << "\nfog-curve=" << value.fog_curve << "\nminimum-light-size=" << value.minimum_light_size
    << "\nbackground-fade=" << value.background_fade << "\ngouraud=" << value.gouraud << '\n';
  output << "mouse-sensitivity=" << value.controls.mouse_sensitivity
    << "\ninvert-mouse=" << value.controls.invert_mouse << '\n';
  auto const descriptions{input::control_descriptions()};
  for(unsigned int i{0}; i < descriptions.size(); ++i) {
    output << "bind." << descriptions[i].name << '=' << value.controls.keys[i] << '\n';
  }
  output.close();
  if(!output) throw std::runtime_error{"Cannot save preferences to " + path.string()};
}

void options_menu::open() {
  original = values;
  active = true;
  save_requested = false;
  selected = 0;
  scroll = 0;
  controls_page = false;
  binding_pending = false;
  notice.clear();
}

unsigned int options_menu::rows() const noexcept {
  return controls_page ? controls_back + 1 : row_count;
}

unsigned int options_menu::list_rows() const noexcept {
  return controls_page ? controls_defaults : row_count;
}

unsigned int options_menu::page_size() const noexcept {
  return controls_page ? control_visible_rows : visible_rows;
}

int options_menu::selection_y() const noexcept {
  if(controls_page && selected >= controls_defaults) return footer_top + static_cast<int>(selected - controls_defaults) * row_height;
  return first_row + static_cast<int>(selected - scroll) * row_height;
}

void options_menu::keep_visible() noexcept {
  if(selected >= list_rows()) return; // Footer actions never move the list.
  if(selected < scroll) scroll = selected;
  if(selected >= scroll + page_size()) scroll = selected - page_size() + 1;
}

void options_menu::wheel(int const direction) {
  if(!active || binding_pending || direction == 0) return;
  if(controls_page) {
    scroll = static_cast<unsigned int>(std::clamp(static_cast<int>(scroll) + direction, 0, static_cast<int>(list_rows() - page_size())));
    if(selected < list_rows()) selected = std::clamp(selected, scroll, scroll + page_size() - 1);
  } else {
    selected = static_cast<unsigned int>(std::clamp(static_cast<int>(selected) + direction, 0, static_cast<int>(rows() - 1)));
  }
}

bool options_menu::capturing_key() const noexcept {
  return binding_pending;
}

void options_menu::capture_key(int const physical_key) {
  if(!active || !binding_pending) return;
  if(physical_key == GLFW_KEY_ESCAPE) {
    binding_pending = false;
    notice.clear();
    return;
  }
  int const key{physical_key == GLFW_KEY_DELETE ? GLFW_KEY_UNKNOWN : input::normalise_key(physical_key)};
  if(!input::bindable_key(key)) {
    notice = "Reserved key. Choose another, or Esc to cancel.";
    return;
  }
  auto const action{selected - mouse_setting_count};
  auto const conflict{values.controls.action_for_key(key)};
  if(conflict && std::to_underlying(*conflict) != action) {
    notice = "In use: " + std::string{input::control_descriptions()[std::to_underlying(*conflict)].label};
    return;
  }
  values.controls.keys[action] = key;
  binding_pending = false;
  notice.clear();
}

void options_menu::adjust(int const direction) {
  if(controls_page) {
    if(selected == 0) values.controls.mouse_sensitivity = std::clamp(std::round(values.controls.mouse_sensitivity * 10 + direction) / 10, 0.1, 5.0);
    if(selected == 1) values.controls.invert_mouse = !values.controls.invert_mouse;
    return;
  }
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
  if(!active || binding_pending) return;
  notice.clear();
  switch(key) {
  case options_key::up: selected = (selected + rows() - 1) % rows(); break;
  case options_key::down: selected = (selected + 1) % rows(); break;
  case options_key::left: if(selected < (controls_page ? mouse_setting_count : setting_count)) adjust(-1); break;
  case options_key::right: if(selected < (controls_page ? mouse_setting_count : setting_count)) adjust(1); break;
  case options_key::page_up: selected = selected > page_size() ? selected - page_size() : 0; break;
  case options_key::page_down: selected = std::min(selected + page_size(), list_rows() - 1); break;
  case options_key::cancel:
    if(controls_page) {
      controls_page = false;
      selected = std::to_underlying(option_row::controls);
      scroll = 0;
      break;
    }
    values = original;
    save_requested = false;
    active = false;
    break;
  case options_key::accept:
    if(controls_page) {
      if(selected < mouse_setting_count) adjust(1);
      else if(selected < controls_defaults) binding_pending = true;
      else if(selected == controls_defaults) values.controls = input::control_preferences{};
      else {
        controls_page = false;
        selected = std::to_underlying(option_row::controls);
        scroll = 0;
      }
      break;
    }
    if(selected < (controls_page ? mouse_setting_count : setting_count)) adjust(1);
    else if(selected == std::to_underlying(option_row::controls)) {
      controls_page = true;
      selected = 0;
      scroll = 0;
    } else if(selected == std::to_underlying(option_row::defaults)) values = preferences{};
    else if(selected == std::to_underlying(option_row::apply)) save_requested = true;
    else if(selected == std::to_underlying(option_row::cancel)) { values = original; active = false; }
    else active = false; // Keep the preview for this session, without writing preferences.
    break;
  }
  keep_visible();
}

void options_menu::point(int const x, int const y) {
  if(binding_pending || x < 12 || x >= (controls_page ? scrollbar_left : 308)) return;
  if(controls_page && y >= footer_top && y < footer_top + 2 * row_height) {
    selected = controls_defaults + static_cast<unsigned int>((y - footer_top) / row_height);
  } else if(y >= first_row && y < first_row + static_cast<int>(page_size()) * row_height) {
    selected = scroll + static_cast<unsigned int>((y - first_row) / row_height);
  }
}

void options_menu::click(int const x, int const y) {
  if(binding_pending) return;
  if(controls_page && x >= scrollbar_left && x <= scrollbar_right && y >= first_row && y < list_bottom) {
    // Clicking the track positions the visible window directly, including both endpoints.
    scroll = static_cast<unsigned int>(y - first_row) * (list_rows() - page_size()) / (list_bottom - first_row - 1);
    if(selected < list_rows()) selected = std::clamp(selected, scroll, scroll + page_size() - 1);
    return;
  }
  if(x < 12 || x >= (controls_page ? scrollbar_left : 308)) return;
  bool const in_list{y >= first_row && y < first_row + static_cast<int>(page_size()) * row_height};
  bool const in_footer{controls_page && y >= footer_top && y < footer_top + 2 * row_height};
  if(!in_list && !in_footer) return;
  point(x, y);
  key(selected < (controls_page ? mouse_setting_count : setting_count) && x < 235 && x >= 190 ? options_key::left : options_key::accept);
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
  text(controls_page ? "CONTROLS" : "OPTIONS", 125, 15, 2);
  if(controls_page) {
    text(std::to_string(scroll + 1) + "-" + std::to_string(std::min(scroll + page_size(), list_rows())) + "/" + std::to_string(list_rows()), 252, 15);
  }
  std::array<std::string, row_count> const labels{
    "Resolution", "Vertical field of view", "Draw distance", "Fog starts", "Fog reaches full shade",
    "Fog curve", "Minimum light size", "Distant fade", "Gouraud shading", "Controls...", "Restore defaults",
    "Apply and save", "Cancel changes", "Return without saving"
  };
  std::array<std::string, 9> const settings{
    std::to_string(values.width) + "x" + std::to_string(values.height), number(std::round(values.field_of_view * 10) / 10),
    number(values.draw_distance), number(values.fog_start), number(values.fade_distance), number(values.fog_curve),
    number(values.minimum_light_size), values.background_fade ? "Sky colour" : "Original", values.gouraud ? "On" : "Off"
  };
  for(unsigned int row{scroll}; row < std::min(scroll + page_size(), list_rows()); ++row) {
    int const y{first_row + static_cast<int>(row - scroll) * row_height};
    if(controls_page) {
      std::string label;
      std::string value;
      if(row == 0) { label = "Mouse sensitivity"; value = number(values.controls.mouse_sensitivity) + "x"; }
      else if(row == 1) { label = "Invert vertical mouse"; value = values.controls.invert_mouse ? "On" : "Off"; }
      else if(row < controls_defaults) {
        auto const action{row - mouse_setting_count};
        label = input::control_descriptions()[action].label;
        value = binding_pending && row == selected ? "Press a key" : input::key_label(values.controls.keys[action]);
      } else label = row == controls_defaults ? "Restore control defaults" : "Back to options";
      text(label, 20, y, row == selected ? 2 : 1);
      text(value, 220, y, row == selected ? 2 : 1);
      continue;
    }
    text(labels[row], 20, y, row == selected ? 2 : 1);
    if(row < settings.size()) {
      text("<", 193, y);
      text(settings[row], 208, y, row == selected ? 2 : 1);
      text(">", 294, y);
    }
  }
  if(controls_page) {
    text("Restore control defaults", 20, footer_top, selected == controls_defaults ? 2 : 1);
    text("Back to options", 20, footer_top + row_height, selected == controls_back ? 2 : 1);
  }
  std::array<std::string_view, row_count> constexpr hints{
    "Render resolution; window size stays unchanged.", "Vertical angle in degrees. Original: 50.2.",
    "Map cells. Original: 15. Whole city: 182.", "Map cells before distance shading begins.",
    "Map cells to full shade, then distant fade.", "Higher values darken nearby objects faster.",
    "Pixels at 320x200. Zero disables the minimum.", "Blend the farthest Delphi silhouettes into sky.",
    "Gouraud shading can also be switched in flight.", "Key bindings and mouse steering settings.", "Preview the recommended Redux defaults.",
    "Save these settings for future sessions.", "Restore the settings from when this menu opened.",
    "Keep changes until you quit, without saving."
  };
  std::string_view const hint{controls_page
    ? (binding_pending ? "Press a key; Delete clears; Esc cancels."
      : "Mouse buttons: left primary, right secondary.")
    : hints[selected]};
  text(notice.empty() ? hint : std::string_view{notice}, 13, 201);
  text(controls_page ? "Arrows: navigate   Enter: change   Esc: back" : "Arrows: adjust   Enter: select   Esc: cancel", 22, 217);
  text(controls_page ? "PgUp/PgDn: scroll   Changes save via Options" : "Hold Tab to preview the unobscured scene", 20, 225);
  // Retain a faint scene silhouette without competing with the menu text.
  unsigned int constexpr background_divisor{16};
  int const track_height{list_bottom - first_row};
  int const thumb_height{track_height * static_cast<int>(page_size()) / static_cast<int>(list_rows())};
  int const thumb_top{first_row + (controls_page ? (track_height - thumb_height) * static_cast<int>(scroll) / static_cast<int>(list_rows() - page_size()) : 0)};
  auto const viewport{framework::render::fit_viewport(width, height, 320, 240)};
  for(int y{0}; y < viewport.height; ++y) {
    int const source_y{y * 240 / viewport.height};
    if(source_y < 7 || source_y >= 234) continue;
    for(int x{0}; x < viewport.width; ++x) {
      int const source_x{x * 320 / viewport.width};
      if(source_x < 7 || source_x >= 313) continue;
      auto &pixel{pixels[static_cast<size_t>(y + viewport.y) * width + x + viewport.x]};
      auto const code{ink.pixels[static_cast<unsigned int>(source_y * 320 + source_x)]};
      bool const highlight{(!controls_page || source_x < scrollbar_left)
        && source_y >= selection_y() - 1 && source_y < selection_y() + row_height - 1};
      if(code) pixel = code == 2 ? framework::render::rgba_pixel{238, 220, 184} : framework::render::rgba_pixel{174, 170, 186};
      else if(controls_page && source_x >= scrollbar_left && source_x <= scrollbar_right && source_y >= first_row && source_y < list_bottom) {
        pixel = source_y >= thumb_top && source_y < thumb_top + thumb_height
          ? framework::render::rgba_pixel{174, 170, 186} : framework::render::rgba_pixel{55, 48, 64};
      }
      else if(controls_page && source_y == footer_top - 5 && source_x >= 12 && source_x < 308) pixel = {108, 100, 120};
      else if(source_x == 7 || source_x == 312 || source_y == 7 || source_y == 233) pixel = {108, 100, 120};
      else pixel = {static_cast<uint8_t>(pixel.r / background_divisor + (highlight ? 44 : 15)), static_cast<uint8_t>(pixel.g / background_divisor + (highlight ? 38 : 14)), static_cast<uint8_t>(pixel.b / background_divisor + (highlight ? 52 : 22))};
    }
  }
}

} // namespace darker::presentation
