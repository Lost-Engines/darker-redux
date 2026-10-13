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
  minimum_light_size, background_fade, gouraud, controls, audio, defaults, apply, cancel, back, count
};
constexpr unsigned int setting_count{std::to_underlying(option_row::controls)};
constexpr unsigned int row_count{std::to_underlying(option_row::count)};
constexpr int first_row{35};
constexpr int row_height{11};
constexpr unsigned int visible_rows{15};
constexpr int scrollbar_left{305};
constexpr int scrollbar_right{310};
constexpr unsigned int control_visible_rows{12};
constexpr int list_bottom{first_row + row_height * static_cast<int>(control_visible_rows)};
constexpr int footer_top{176};
constexpr unsigned int mouse_setting_count{5};
constexpr unsigned int controls_defaults{mouse_setting_count + std::to_underlying(input::control_action::count)};
constexpr unsigned int joystick_setting_count{7};
constexpr unsigned int joystick_defaults{joystick_setting_count + std::to_underlying(input::control_action::count)};
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
    && std::isfinite(minimum_light_size) && minimum_light_size >= 0 && music_volume <= 100 && effects_volume <= 100 && controls.valid() && joystick.valid();
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
    else if(name == "music-volume") value >> result.music_volume;
    else if(name == "effects-volume") value >> result.effects_volume;
    else if(name == "mouse-primary") value >> result.controls.primary_mouse_button;
    else if(name == "mouse-secondary") value >> result.controls.secondary_mouse_button;
    else if(name == "mouse-sensitivity") value >> result.controls.mouse_sensitivity;
    else if(name == "invert-mouse") value >> result.controls.invert_mouse;
    else if(name == "joystick-device") value >> result.joystick.device;
    else if(name == "joystick-bank-axis") value >> result.joystick.bank_axis;
    else if(name == "joystick-pitch-axis") value >> result.joystick.pitch_axis;
    else if(name == "joystick-invert-bank") value >> result.joystick.invert_bank;
    else if(name == "joystick-invert-pitch") value >> result.joystick.invert_pitch;
    else if(name == "joystick-deadzone") value >> result.joystick.deadzone_percent;
    else if(name == "joystick-sensitivity") value >> result.joystick.sensitivity_percent;
    else if(name.starts_with("joystick-button.")) {
      auto const descriptions{input::control_descriptions()};
      auto const found{std::ranges::find(descriptions, std::string_view{name}.substr(16), &input::control_description::name)};
      if(found == descriptions.end()) continue;
      value >> result.joystick.buttons[static_cast<unsigned int>(found - descriptions.begin())];
    }
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
  output << "music-volume=" << value.music_volume << "\neffects-volume=" << value.effects_volume
    << "\nmouse-primary=" << value.controls.primary_mouse_button << "\nmouse-secondary=" << value.controls.secondary_mouse_button << '\n';
  output << "mouse-sensitivity=" << value.controls.mouse_sensitivity
    << "\ninvert-mouse=" << value.controls.invert_mouse << '\n';
  output << "joystick-device=" << value.joystick.device
    << "\njoystick-bank-axis=" << value.joystick.bank_axis << "\njoystick-pitch-axis=" << value.joystick.pitch_axis
    << "\njoystick-invert-bank=" << value.joystick.invert_bank << "\njoystick-invert-pitch=" << value.joystick.invert_pitch
    << "\njoystick-deadzone=" << value.joystick.deadzone_percent << "\njoystick-sensitivity=" << value.joystick.sensitivity_percent << '\n';
  auto const descriptions{input::control_descriptions()};
  for(unsigned int i{0}; i < descriptions.size(); ++i) {
    output << "bind." << descriptions[i].name << '=' << value.controls.keys[i] << '\n';
    output << "joystick-button." << descriptions[i].name << '=' << value.joystick.buttons[i] << '\n';
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
  audio_page = false;
  joystick_page = false;
  binding_pending = false;
  notice.clear();
}

bool options_menu::binding_list() const noexcept { return controls_page || joystick_page; }
unsigned int options_menu::binding_defaults() const noexcept { return joystick_page ? joystick_defaults : controls_defaults; }
bool options_menu::capturing_button() const noexcept { return joystick_page && binding_pending; }

void options_menu::capture_button(int const button) {
  if(!capturing_button() || button < -1 || button >= static_cast<int>(input::joystick_button_limit)) return;
  auto const action{selected - joystick_setting_count};
  if(button >= 0) {
    auto const found{std::ranges::find(values.joystick.buttons, button)};
    if(found != values.joystick.buttons.end() && static_cast<unsigned int>(found - values.joystick.buttons.begin()) != action) {
      notice = "In use: " + std::string{input::control_descriptions()[found - values.joystick.buttons.begin()].label};
      return;
    }
  }
  values.joystick.buttons[action] = button;
  binding_pending = false;
  notice.clear();
}

unsigned int options_menu::rows() const noexcept {
  return binding_list() ? binding_defaults() + 2 : audio_page ? 4 : row_count;
}

unsigned int options_menu::list_rows() const noexcept {
  return binding_list() ? binding_defaults() : rows();
}

unsigned int options_menu::page_size() const noexcept {
  return binding_list() ? control_visible_rows : std::min(visible_rows, rows());
}

int options_menu::selection_y() const noexcept {
  if(binding_list() && selected >= binding_defaults()) return footer_top + static_cast<int>(selected - binding_defaults()) * row_height;
  return first_row + static_cast<int>(selected - scroll) * row_height;
}

void options_menu::keep_visible() noexcept {
  if(selected >= list_rows()) return; // Footer actions never move the list.
  if(selected < scroll) scroll = selected;
  if(selected >= scroll + page_size()) scroll = selected - page_size() + 1;
}

void options_menu::wheel(int const direction) {
  if(!active || binding_pending || direction == 0) return;
  if(binding_list()) {
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
  if(joystick_page) {
    if(physical_key == GLFW_KEY_DELETE) capture_button(-1);
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
  if(joystick_page) {
    auto &settings{values.joystick};
    if(selected == 0) settings.device = std::clamp(settings.device + direction, input::joystick_disabled, 15);
    if(selected == 1 || selected == 2) {
      auto &axis{selected == 1 ? settings.bank_axis : settings.pitch_axis};
      auto &other{selected == 1 ? settings.pitch_axis : settings.bank_axis};
      int const previous{axis};
      axis = std::clamp(axis + direction, 0, 31);
      if(axis == other) other = previous;
    }
    if(selected == 3) settings.invert_bank = !settings.invert_bank;
    if(selected == 4) settings.invert_pitch = !settings.invert_pitch;
    if(selected == 5) settings.deadzone_percent = static_cast<unsigned int>(std::clamp(static_cast<int>(settings.deadzone_percent) + direction, 0, 50));
    if(selected == 6) settings.sensitivity_percent = static_cast<unsigned int>(std::clamp(static_cast<int>(settings.sensitivity_percent) + direction * 5, 25, 200));
    return;
  }
  if(audio_page) {
    if(selected < 2) {
      auto &volume{selected == 0 ? values.music_volume : values.effects_volume};
      volume = static_cast<unsigned int>(std::clamp(static_cast<int>(volume) + direction * 5, 0, 100));
    }
    return;
  }
  if(controls_page) {
    if(selected == 0) values.controls.mouse_sensitivity = std::clamp(std::round(values.controls.mouse_sensitivity * 10 + direction) / 10, 0.1, 5.0);
    if(selected == 1) values.controls.invert_mouse = !values.controls.invert_mouse;
    if(selected == 2 || selected == 3) {
      auto &button{selected == 2 ? values.controls.primary_mouse_button : values.controls.secondary_mouse_button};
      auto &other{selected == 2 ? values.controls.secondary_mouse_button : values.controls.primary_mouse_button};
      int const previous{button};
      button = std::clamp(button + direction, -1, 7);
      if(button >= 0 && button == other) other = previous; // Swap occupied buttons, retaining both actions.
    }
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
  case options_key::left: if(selected < (joystick_page ? joystick_setting_count : controls_page ? mouse_setting_count : audio_page ? 2 : setting_count)) adjust(-1); break;
  case options_key::right: if(selected < (joystick_page ? joystick_setting_count : controls_page ? mouse_setting_count : audio_page ? 2 : setting_count)) adjust(1); break;
  case options_key::page_up: selected = selected > page_size() ? selected - page_size() : 0; break;
  case options_key::page_down: selected = std::min(selected + page_size(), list_rows() - 1); break;
  case options_key::cancel:
    if(joystick_page) {
      joystick_page = false;
      controls_page = true;
      selected = 4;
      scroll = 0;
      break;
    }
    if(audio_page) {
      audio_page = false;
      selected = std::to_underlying(option_row::audio);
      scroll = 0;
      break;
    }
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
    if(joystick_page) {
      if(selected < joystick_setting_count) adjust(1);
      else if(selected < joystick_defaults) binding_pending = true;
      else if(selected == joystick_defaults) values.joystick = input::joystick_preferences{};
      else { joystick_page = false; controls_page = true; selected = 4; scroll = 0; }
      break;
    }
    if(audio_page) {
      if(selected < 2) adjust(1);
      else if(selected == 2) { values.music_volume = 100; values.effects_volume = 100; }
      else { audio_page = false; selected = std::to_underlying(option_row::audio); scroll = 0; }
      break;
    }
    if(controls_page) {
      if(selected == 4) { controls_page = false; joystick_page = true; selected = 0; scroll = 0; }
      else if(selected < mouse_setting_count) adjust(1);
      else if(selected < controls_defaults) binding_pending = true;
      else if(selected == controls_defaults) values.controls = input::control_preferences{};
      else {
        controls_page = false;
        selected = std::to_underlying(option_row::controls);
        scroll = 0;
      }
      break;
    }
    if(selected < (joystick_page ? joystick_setting_count : controls_page ? mouse_setting_count : audio_page ? 2 : setting_count)) adjust(1);
    else if(selected == std::to_underlying(option_row::controls)) {
      controls_page = true;
      selected = 0;
      scroll = 0;
    } else if(selected == std::to_underlying(option_row::audio)) {
      audio_page = true;
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
  if(binding_pending || x < 12 || x >= (binding_list() ? scrollbar_left : 308)) return;
  if(binding_list() && y >= footer_top && y < footer_top + 2 * row_height) {
    selected = binding_defaults() + static_cast<unsigned int>((y - footer_top) / row_height);
  } else if(y >= first_row && y < first_row + static_cast<int>(page_size()) * row_height) {
    selected = scroll + static_cast<unsigned int>((y - first_row) / row_height);
  }
}

void options_menu::click(int const x, int const y) {
  if(binding_pending) return;
  if(binding_list() && x >= scrollbar_left && x <= scrollbar_right && y >= first_row && y < list_bottom) {
    // Clicking the track positions the visible window directly, including both endpoints.
    scroll = static_cast<unsigned int>(y - first_row) * (list_rows() - page_size()) / (list_bottom - first_row - 1);
    if(selected < list_rows()) selected = std::clamp(selected, scroll, scroll + page_size() - 1);
    return;
  }
  if(x < 12 || x >= (binding_list() ? scrollbar_left : 308)) return;
  bool const in_list{y >= first_row && y < first_row + static_cast<int>(page_size()) * row_height};
  bool const in_footer{binding_list() && y >= footer_top && y < footer_top + 2 * row_height};
  if(!in_list && !in_footer) return;
  point(x, y);
  key(selected < (joystick_page ? joystick_setting_count : controls_page ? mouse_setting_count : audio_page ? 2 : setting_count) && x < 235 && x >= 190 ? options_key::left : options_key::accept);
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
  text(joystick_page ? "JOYSTICK" : controls_page ? "CONTROLS" : audio_page ? "SOUND" : "OPTIONS", 125, 15, 2);
  if(binding_list()) {
    text(std::to_string(scroll + 1) + "-" + std::to_string(std::min(scroll + page_size(), list_rows())) + "/" + std::to_string(list_rows()), 252, 15);
  }
  std::array<std::string, row_count> const labels{
    "Resolution", "Vertical field of view", "Draw distance", "Fog starts", "Fog reaches full shade",
    "Fog curve", "Minimum light size", "Distant fade", "Gouraud shading", "Controls...", "Sound...", "Restore defaults",
    "Apply and save", "Cancel changes", "Return without saving"
  };
  std::array<std::string, 9> const settings{
    std::to_string(values.width) + "x" + std::to_string(values.height), number(std::round(values.field_of_view * 10) / 10),
    number(values.draw_distance), number(values.fog_start), number(values.fade_distance), number(values.fog_curve),
    number(values.minimum_light_size), values.background_fade ? "Sky colour" : "Original", values.gouraud ? "On" : "Off"
  };
  for(unsigned int row{scroll}; row < std::min(scroll + page_size(), list_rows()); ++row) {
    int const y{first_row + static_cast<int>(row - scroll) * row_height};
    if(joystick_page) {
      auto const &settings{values.joystick};
      std::string label, value;
      if(row < joystick_setting_count) {
        std::array<std::string_view, joystick_setting_count> constexpr labels{"Device", "Bank axis", "Pitch axis", "Invert bank", "Invert pitch", "Extra dead zone", "Sensitivity"};
        label = labels[row];
        switch(row) {
        case 0: value = settings.device == input::joystick_disabled ? "Off" : settings.device == input::joystick_automatic ? "Auto" : "Device " + std::to_string(settings.device + 1); break;
        case 1: value = std::to_string(settings.bank_axis + 1); break;
        case 2: value = std::to_string(settings.pitch_axis + 1); break;
        case 3: value = settings.invert_bank ? "On" : "Off"; break;
        case 4: value = settings.invert_pitch ? "On" : "Off"; break;
        case 5: value = std::to_string(settings.deadzone_percent) + "%"; break;
        case 6: value = std::to_string(settings.sensitivity_percent) + "%"; break;
        }
      } else {
        auto const action{row - joystick_setting_count};
        label = input::control_descriptions()[action].label;
        int const button{settings.buttons[action]};
        value = binding_pending && row == selected ? "Press button" : button < 0 ? "Unbound" : "Button " + std::to_string(button + 1);
      }
      text(label, 20, y, row == selected ? 2 : 1);
      text(value, 220, y, row == selected ? 2 : 1);
      continue;
    }
    if(audio_page) {
      std::array<std::string_view, 4> constexpr audio_labels{"Music volume", "Effects volume", "Restore sound defaults", "Back to options"};
      text(audio_labels[row], 20, y, row == selected ? 2 : 1);
      if(row < 2) {
        text("<", 193, y);
        text(std::to_string(row == 0 ? values.music_volume : values.effects_volume) + "%", 220, y, row == selected ? 2 : 1);
        text(">", 294, y);
      }
      continue;
    }
    if(controls_page) {
      std::string label;
      std::string value;
      if(row == 0) { label = "Mouse sensitivity"; value = number(values.controls.mouse_sensitivity) + "x"; }
      else if(row == 1) { label = "Invert vertical mouse"; value = values.controls.invert_mouse ? "On" : "Off"; }
      else if(row == 2 || row == 3) {
        label = row == 2 ? "Mouse primary fire" : "Mouse secondary fire";
        int const button{row == 2 ? values.controls.primary_mouse_button : values.controls.secondary_mouse_button};
        value = button < 0 ? "Off" : button == 0 ? "Left" : button == 1 ? "Right" : button == 2 ? "Middle" : "Button " + std::to_string(button + 1);
      }
      else if(row == 4) label = "Joystick / gamepad...";
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
  if(binding_list()) {
    text(joystick_page ? "Restore joystick defaults" : "Restore control defaults", 20, footer_top, selected == binding_defaults() ? 2 : 1);
    text(joystick_page ? "Back to controls" : "Back to options", 20, footer_top + row_height, selected == binding_defaults() + 1 ? 2 : 1);
  }
  std::array<std::string_view, row_count> constexpr hints{
    "Render resolution; window size stays unchanged.", "Vertical angle in degrees. Original: 50.2.",
    "Map cells. Original: 15. Whole city: 182.", "Map cells before distance shading begins.",
    "Map cells to full shade, then distant fade.", "Higher values darken nearby objects faster.",
    "Pixels at 320x200. Zero disables the minimum.", "Blend the farthest Delphi silhouettes into sky.",
    "Gouraud shading can also be switched in flight.", "Key bindings and mouse steering settings.", "Music and effects playback volumes.", "Preview the recommended Redux defaults.",
    "Save these settings for future sessions.", "Restore the settings from when this menu opened.",
    "Keep changes until you quit, without saving."
  };
  std::string joystick_hint;
  if(joystick_page) {
    if(binding_pending) joystick_hint = "Press button; Delete clears; Esc cancels.";
    else if(joystick_status.device < 0) joystick_hint = "No device connected (or input is off).";
    else if(selected == 1 || selected == 2) {
      auto const axis{selected == 1 ? values.joystick.bank_axis : values.joystick.pitch_axis};
      joystick_hint = static_cast<unsigned int>(axis) < joystick_status.axes.size()
        ? "Axis " + std::to_string(axis + 1) + ": " + std::to_string(static_cast<int>(joystick_status.axes[axis] * 100)) + "%"
        : "This device does not have that axis.";
    } else joystick_hint = std::to_string(joystick_status.device + 1) + ": " + joystick_status.name.substr(0, 37);
  }
  std::string_view const hint{joystick_page ? std::string_view{joystick_hint} : controls_page
    ? (binding_pending ? "Press a key; Delete clears; Esc cancels."
      : "Mouse buttons affect flight; menus use left.")
    : audio_page ? "Changes apply immediately. Zero mutes." : hints[selected]};
  text(notice.empty() ? hint : std::string_view{notice}, 13, 201);
  text(binding_list() || audio_page ? "Arrows: navigate   Enter: change   Esc: back" : "Arrows: adjust   Enter: select   Esc: cancel", 22, 217);
  text(binding_list() ? "PgUp/PgDn: scroll   Changes save via Options" : audio_page ? "Changes save via Options" : "Hold Tab to preview the unobscured scene", 20, 225);
  // Retain a faint scene silhouette without competing with the menu text.
  unsigned int constexpr background_divisor{16};
  int const track_height{list_bottom - first_row};
  int const thumb_height{track_height * static_cast<int>(page_size()) / static_cast<int>(list_rows())};
  int const thumb_top{first_row + (binding_list() ? (track_height - thumb_height) * static_cast<int>(scroll) / static_cast<int>(list_rows() - page_size()) : 0)};
  auto const viewport{framework::render::fit_viewport(width, height, 320, 240)};
  for(int y{0}; y < viewport.height; ++y) {
    int const source_y{y * 240 / viewport.height};
    if(source_y < 7 || source_y >= 234) continue;
    for(int x{0}; x < viewport.width; ++x) {
      int const source_x{x * 320 / viewport.width};
      if(source_x < 7 || source_x >= 313) continue;
      auto &pixel{pixels[static_cast<size_t>(y + viewport.y) * width + x + viewport.x]};
      auto const code{ink.pixels[static_cast<unsigned int>(source_y * 320 + source_x)]};
      bool const highlight{(!binding_list() || source_x < scrollbar_left)
        && source_y >= selection_y() - 1 && source_y < selection_y() + row_height - 1};
      if(code) pixel = code == 2 ? framework::render::rgba_pixel{238, 220, 184} : framework::render::rgba_pixel{174, 170, 186};
      else if(binding_list() && source_x >= scrollbar_left && source_x <= scrollbar_right && source_y >= first_row && source_y < list_bottom) {
        pixel = source_y >= thumb_top && source_y < thumb_top + thumb_height
          ? framework::render::rgba_pixel{174, 170, 186} : framework::render::rgba_pixel{55, 48, 64};
      }
      else if(binding_list() && source_y == footer_top - 5 && source_x >= 12 && source_x < 308) pixel = {108, 100, 120};
      else if(source_x == 7 || source_x == 312 || source_y == 7 || source_y == 233) pixel = {108, 100, 120};
      else pixel = {static_cast<uint8_t>(pixel.r / background_divisor + (highlight ? 44 : 15)), static_cast<uint8_t>(pixel.g / background_divisor + (highlight ? 38 : 14)), static_cast<uint8_t>(pixel.b / background_divisor + (highlight ? 52 : 22))};
    }
  }
}

} // namespace darker::presentation
