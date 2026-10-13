#include "input/joystick.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <GLFW/glfw3.h>

namespace darker::input {

joystick_preferences::joystick_preferences() {
  buttons.fill(-1);
  buttons[std::to_underlying(control_action::primary)] = 0;
  buttons[std::to_underlying(control_action::secondary)] = 1;
  buttons[std::to_underlying(control_action::boost)] = 2;
  buttons[std::to_underlying(control_action::engine)] = 3;
  buttons[std::to_underlying(control_action::brake)] = 4;
  buttons[std::to_underlying(control_action::look)] = 5;
  buttons[std::to_underlying(control_action::radar)] = 6;
}

bool joystick_preferences::valid() const noexcept {
  if(device < joystick_disabled || device > GLFW_JOYSTICK_LAST
    || bank_axis < 0 || bank_axis > 31 || pitch_axis < 0 || pitch_axis > 31
    || bank_axis == pitch_axis || !std::isfinite(deadzone_percent) || deadzone_percent < 0 || deadzone_percent > 50
    || sensitivity_percent < 25 || sensitivity_percent > 200) return false;
  for(unsigned int i{0}; i < buttons.size(); ++i) {
    if(buttons[i] < -1 || buttons[i] >= static_cast<int>(joystick_button_limit)) return false;
    if(buttons[i] < 0) continue;
    for(unsigned int j{0}; j < i; ++j) if(buttons[i] == buttons[j]) return false;
  }
  return true;
}

joystick_sample poll_joystick(int device) {
  joystick_sample result;
  if(device == joystick_disabled) return result;
  if(device == joystick_automatic) {
    for(int candidate{GLFW_JOYSTICK_1}; candidate <= GLFW_JOYSTICK_LAST; ++candidate) {
      if(glfwJoystickPresent(candidate)) { device = candidate; break; }
    }
  }
  if(device < GLFW_JOYSTICK_1 || device > GLFW_JOYSTICK_LAST || !glfwJoystickPresent(device)) return result;
  result.device = device;
  GLFWgamepadstate gamepad{};
  result.gamepad = glfwGetGamepadState(device, &gamepad) == GLFW_TRUE;
  auto const *name{result.gamepad ? glfwGetGamepadName(device) : glfwGetJoystickName(device)};
  result.name = name ? name : "Joystick";
  // Device names are UTF-8, while the original menu font accepts ASCII here.
  for(char &character : result.name) if(static_cast<unsigned char>(character) < 32 || static_cast<unsigned char>(character) > 126) character = '?';
  if(result.gamepad) {
    result.axes.assign(std::begin(gamepad.axes), std::end(gamepad.axes));
    result.buttons.assign(std::begin(gamepad.buttons), std::end(gamepad.buttons));
    // Expose the standard pad's analogue triggers as assignable buttons 16/17.
    result.buttons.push_back(gamepad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.0f);
    result.buttons.push_back(gamepad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.0f);
  } else {
    int count{0};
    auto const *axes{glfwGetJoystickAxes(device, &count)};
    if(axes && count > 0) result.axes.assign(axes, axes + count);
    auto const *buttons{glfwGetJoystickButtons(device, &count)};
    if(buttons && count > 0) result.buttons.assign(buttons, buttons + std::min(count, static_cast<int>(joystick_button_limit)));
  }
  return result;
}

int16_t joystick_axis(joystick_sample const &sample, int const axis, bool const invert,
  joystick_preferences const &preferences) noexcept {
  if(axis < 0 || static_cast<unsigned int>(axis) >= sample.axes.size()) return 0;
  double const raw{sample.axes[static_cast<unsigned int>(axis)]};
  if(!std::isfinite(raw)) return 0;
  double const magnitude{std::clamp(std::abs(raw) * preferences.sensitivity_percent / 100.0, 0.0, 1.0)};
  return static_cast<int16_t>(std::lround(std::copysign(magnitude * 32767, invert ? -raw : raw)));
}

void joystick_buttons::update(joystick_sample const &sample, bool const accept_input) noexcept {
  bool const reset{device != sample.device || enabled != accept_input};
  device = sample.device;
  enabled = accept_input;
  for(unsigned int i{0}; i < held.size(); ++i) {
    bool const active{i < sample.buttons.size() && sample.buttons[i] != 0};
    if(reset) blocked[i] = active;
    if(!active) blocked[i] = false;
    held[i] = enabled && active && !blocked[i];
    pressed[i] = held[i] && !previous[i];
    previous[i] = active;
  }
}

void joystick_buttons::suppress() noexcept {
  for(unsigned int i{0}; i < held.size(); ++i) {
    blocked[i] = previous[i];
    held[i] = pressed[i] = false;
  }
}

bool joystick_buttons::down(int const button) const noexcept {
  return button >= 0 && static_cast<unsigned int>(button) < held.size() && held[static_cast<unsigned int>(button)];
}

bool joystick_buttons::pushed(int const button) const noexcept {
  return button >= 0 && static_cast<unsigned int>(button) < pressed.size() && pressed[static_cast<unsigned int>(button)];
}

} // namespace darker::input
