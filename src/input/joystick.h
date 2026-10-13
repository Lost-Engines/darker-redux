#pragma once

#include <array>
#include <string>
#include <vector>
#include "input/control_preferences.h"
#include "game/flight_controls.h"

namespace darker::input {

inline constexpr int joystick_automatic{-1};
inline constexpr int joystick_disabled{-2};
inline constexpr unsigned int joystick_button_limit{128};

struct joystick_preferences {
  int device{joystick_automatic};
  int bank_axis{0};
  int pitch_axis{1};
  bool invert_bank{false};
  bool invert_pitch{false};
  double deadzone_percent{100.0 * game::original_joystick_deadzone / game::joystick_response_scale};
  unsigned int sensitivity_percent{100};
  std::array<int, static_cast<unsigned int>(control_action::count)> buttons;

  joystick_preferences();
  bool valid() const noexcept;
  bool operator==(joystick_preferences const&) const = default;
};

struct joystick_sample {
  int device{joystick_disabled};
  std::string name{};
  bool gamepad{false};
  std::vector<float> axes{};
  std::vector<unsigned char> buttons{};
};

joystick_sample poll_joystick(int device);
int16_t joystick_axis(joystick_sample const &sample, int axis, bool invert,
  joystick_preferences const &preferences) noexcept;

// Tracks physical buttons separately from action bindings, including release-to-arm
// after connection, focus changes, and menu/game transitions.
class joystick_buttons {
  int device{joystick_disabled};
  bool enabled{false};
  std::array<bool, joystick_button_limit> previous{}, blocked{}, held{}, pressed{};
public:
  void update(joystick_sample const &sample, bool accept_input) noexcept;
  void suppress() noexcept;
  bool down(int button) const noexcept;
  bool pushed(int button) const noexcept;
};

} // namespace darker::input
