#include "input/control_preferences.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <GLFW/glfw3.h>

namespace darker::input {
namespace {
constexpr std::array descriptions{
  control_description{
    .name{"left"},
    .label{"Bank left"},
    .default_key{GLFW_KEY_LEFT}
  },
  control_description{
    .name{"right"},
    .label{"Bank right"},
    .default_key{GLFW_KEY_RIGHT}
  },
  control_description{
    .name{"up"},
    .label{"Pitch down"},
    .default_key{GLFW_KEY_UP}
  },
  control_description{
    .name{"down"},
    .label{"Pitch up"},
    .default_key{GLFW_KEY_DOWN}
  },
  control_description{
    .name{"control"},
    .label{"Steering modifier"},
    .default_key{GLFW_KEY_LEFT_CONTROL}
  },
  control_description{
    .name{"brake"},
    .label{"Brake"},
    .default_key{GLFW_KEY_BACKSPACE}
  },
  control_description{
    .name{"boost"},
    .label{"Boost / launch"},
    .default_key{GLFW_KEY_ENTER}
  },
  control_description{
    .name{"engine"},
    .label{"Engine / shield"},
    .default_key{GLFW_KEY_E}
  },
  control_description{
    .name{"altitude"},
    .label{"Altitude hold"},
    .default_key{GLFW_KEY_A}
  },
  control_description{
    .name{"slow"},
    .label{"Skimma low speed"},
    .default_key{GLFW_KEY_MINUS}
  },
  control_description{
    .name{"fast"},
    .label{"Skimma high speed"},
    .default_key{GLFW_KEY_EQUAL}
  },
  control_description{
    .name{"primary"},
    .label{"Primary fire"},
    .default_key{GLFW_KEY_SPACE}
  },
  control_description{
    .name{"secondary"},
    .label{"Secondary fire"},
    .default_key{GLFW_KEY_LEFT_ALT}
  },
  control_description{
    .name{"look"},
    .label{"Look around (hold)"},
    .default_key{GLFW_KEY_TAB}
  },
  control_description{
    .name{"radar"},
    .label{"Radar overlay (hold)"},
    .default_key{GLFW_KEY_INSERT}
  },
  control_description{
    .name{"cockpit"},
    .label{"Cockpit view"},
    .default_key{GLFW_KEY_F1}
  },
  control_description{
    .name{"following"},
    .label{"Following view"},
    .default_key{GLFW_KEY_F2}
  },
  control_description{
    .name{"following-alternate"},
    .label{"Alternate following view"},
    .default_key{GLFW_KEY_F3}
  },
  control_description{
    .name{"fullscreen"},
    .label{"Full-screen view"},
    .default_key{GLFW_KEY_F4}
  },
  control_description{
    .name{"drop-camera"},
    .label{"Drop camera"},
    .default_key{GLFW_KEY_F5}
  },
  control_description{
    .name{"free-camera"},
    .label{"Free camera"},
    .default_key{GLFW_KEY_F6}
  },
  control_description{
    .name{"pick-camera"},
    .label{"Camera target marker"},
    .default_key{GLFW_KEY_F7}
  },
  control_description{
    .name{"camera-nearer"},
    .label{"Camera nearer"},
    .default_key{GLFW_KEY_COMMA}
  },
  control_description{
    .name{"camera-farther"},
    .label{"Camera farther"},
    .default_key{GLFW_KEY_PERIOD}
  },
  control_description{
    .name{"missile-camera"},
    .label{"Follow missile"},
    .default_key{GLFW_KEY_M}
  },
  control_description{
    .name{"clear-target"},
    .label{"Clear weapon target"},
    .default_key{GLFW_KEY_CAPS_LOCK}
  },
  control_description{
    .name{"gouraud"},
    .label{"Gouraud shading"},
    .default_key{GLFW_KEY_F9}
  },
  control_description{
    .name{"fov-narrower"},
    .label{"Narrower field of view"},
    .default_key{GLFW_KEY_LEFT_BRACKET}
  },
  control_description{
    .name{"fov-wider"},
    .label{"Wider field of view"},
    .default_key{GLFW_KEY_RIGHT_BRACKET}
  },
  control_description{
    .name{"weapon-1"},
    .label{"Weapon 1"},
    .default_key{GLFW_KEY_1}
  },
  control_description{
    .name{"weapon-2"},
    .label{"Weapon 2"},
    .default_key{GLFW_KEY_2}
  },
  control_description{
    .name{"weapon-3"},
    .label{"Weapon 3"},
    .default_key{GLFW_KEY_3}
  },
  control_description{
    .name{"weapon-5"},
    .label{"Weapon 5"},
    .default_key{GLFW_KEY_5}
  },
  control_description{
    .name{"weapon-6"},
    .label{"Weapon 6"},
    .default_key{GLFW_KEY_6}
  },
  control_description{
    .name{"weapon-8"},
    .label{"Weapon 8"},
    .default_key{GLFW_KEY_8}
  },
  control_description{
    .name{"weapon-9"},
    .label{"Weapon 9"},
    .default_key{GLFW_KEY_9}
  },
  control_description{
    .name{"weapon-0"},
    .label{"Weapon 0"},
    .default_key{GLFW_KEY_0}
  },
};
static_assert(descriptions.size() == static_cast<unsigned int>(control_action::count));
} // anonymous namespace

std::span<control_description const> control_descriptions() noexcept {
  return descriptions;
}

int normalise_key(int const key) noexcept {
  switch(key) {
  case GLFW_KEY_RIGHT_CONTROL: return GLFW_KEY_LEFT_CONTROL;
  case GLFW_KEY_RIGHT_ALT: return GLFW_KEY_LEFT_ALT;
  case GLFW_KEY_KP_ENTER: return GLFW_KEY_ENTER;
  case GLFW_KEY_KP_0: return GLFW_KEY_INSERT;
  case GLFW_KEY_GRAVE_ACCENT: return GLFW_KEY_F7;
  default: return key;
  }
}

bool bindable_key(int const key) noexcept {
  if(key == GLFW_KEY_UNKNOWN) return true; // Explicitly unbound.
  if(key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F10 || key == GLFW_KEY_PAUSE
    || key == GLFW_KEY_NUM_LOCK || key == GLFW_KEY_DELETE
    || key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER
    || key == GLFW_KEY_X || key == GLFW_KEY_Z || key == GLFW_KEY_K) return false;
  return key == GLFW_KEY_SPACE || key == GLFW_KEY_APOSTROPHE
    || (key >= GLFW_KEY_COMMA && key <= GLFW_KEY_9)
    || key == GLFW_KEY_SEMICOLON || key == GLFW_KEY_EQUAL
    || (key >= GLFW_KEY_A && key <= GLFW_KEY_RIGHT_BRACKET)
    || key == GLFW_KEY_GRAVE_ACCENT
    || (key >= GLFW_KEY_ENTER && key <= GLFW_KEY_END)
    || (key >= GLFW_KEY_CAPS_LOCK && key <= GLFW_KEY_PRINT_SCREEN)
    || (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
    || (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
    || (key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_MENU);
}

std::string key_label(int const key) {
  if(key == GLFW_KEY_UNKNOWN) return "Unbound";
  if((key >= GLFW_KEY_A && key <= GLFW_KEY_Z) || (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)) return std::string(1, static_cast<char>(key));
  if(key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
  if(key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return "Keypad " + std::to_string(key - GLFW_KEY_KP_0);
  switch(key) {
  case GLFW_KEY_SPACE: return "Space";
  case GLFW_KEY_ENTER: return "Enter";
  case GLFW_KEY_TAB: return "Tab";
  case GLFW_KEY_BACKSPACE: return "Backspace";
  case GLFW_KEY_INSERT: return "Insert";
  case GLFW_KEY_LEFT: return "Left";
  case GLFW_KEY_RIGHT: return "Right";
  case GLFW_KEY_UP: return "Up";
  case GLFW_KEY_DOWN: return "Down";
  case GLFW_KEY_PAGE_UP: return "Page Up";
  case GLFW_KEY_PAGE_DOWN: return "Page Down";
  case GLFW_KEY_HOME: return "Home";
  case GLFW_KEY_END: return "End";
  case GLFW_KEY_CAPS_LOCK: return "Caps Lock";
  case GLFW_KEY_SCROLL_LOCK: return "Scroll Lock";
  case GLFW_KEY_PRINT_SCREEN: return "Print Screen";
  case GLFW_KEY_LEFT_CONTROL: return "Ctrl";
  case GLFW_KEY_LEFT_ALT: return "Alt";
  case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
  case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
  case GLFW_KEY_MENU: return "Menu";
  case GLFW_KEY_KP_DECIMAL: return "Keypad .";
  case GLFW_KEY_KP_DIVIDE: return "Keypad /";
  case GLFW_KEY_KP_MULTIPLY: return "Keypad *";
  case GLFW_KEY_KP_SUBTRACT: return "Keypad -";
  case GLFW_KEY_KP_ADD: return "Keypad +";
  case GLFW_KEY_KP_EQUAL: return "Keypad =";
  default: return std::string(1, static_cast<char>(key));
  }
}

control_preferences::control_preferences() {
  for(unsigned int i{0}; i < keys.size(); ++i) keys[i] = descriptions[i].default_key;
}

bool control_preferences::valid() const noexcept {
  if(!std::isfinite(mouse_sensitivity) || mouse_sensitivity < 0.1 || mouse_sensitivity > 5.0) return false;
  for(unsigned int i{0}; i < keys.size(); ++i) {
    if(!bindable_key(keys[i]) || normalise_key(keys[i]) != keys[i]) return false;
    if(keys[i] == GLFW_KEY_UNKNOWN) continue;
    for(unsigned int j{0}; j < i; ++j) if(keys[j] == keys[i]) return false;
  }
  return true;
}

std::optional<control_action> control_preferences::action_for_key(int const key) const noexcept {
  if(key == GLFW_KEY_UNKNOWN) return std::nullopt;
  auto const found{std::ranges::find(keys, normalise_key(key))};
  if(found == keys.end()) return std::nullopt;
  return static_cast<control_action>(found - keys.begin());
}

int control_preferences::command_key(int const physical_key) const noexcept {
  auto const action{action_for_key(physical_key)};
  return action ? descriptions[std::to_underlying(*action)].default_key : GLFW_KEY_UNKNOWN;
}

int control_preferences::binding_for_command(int const command) const noexcept {
  auto const found{std::ranges::find(descriptions, normalise_key(command), &control_description::default_key)};
  return found == descriptions.end() ? GLFW_KEY_UNKNOWN : keys[static_cast<unsigned int>(found - descriptions.begin())];
}

void mouse_motion::reset() noexcept {
  started = false;
}

vec2<uint16_t> mouse_motion::sample(double const x, double const y, control_preferences const &preferences) noexcept {
  vec2<double> const position{x, y};
  if(!started) {
    previous = position;
    accumulated = {};
    started = true;
  }
  auto const delta{position - previous};
  previous = position;
  accumulated.x = std::fmod(accumulated.x + delta.x * preferences.mouse_sensitivity, 65536.0);
  accumulated.y = std::fmod(accumulated.y - delta.y * preferences.mouse_sensitivity * (preferences.invert_mouse ? -1.0 : 1.0), 65536.0);
  return {static_cast<uint16_t>(static_cast<int32_t>(accumulated.x)), static_cast<uint16_t>(static_cast<int32_t>(accumulated.y))};
}

} // namespace darker::input
