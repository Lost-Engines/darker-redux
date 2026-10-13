#include "game/flight_controls.h"
#include <algorithm>
#include <bit>
#include <cstdlib>

namespace darker::game {
namespace {

int16_t word(int const value) noexcept {
  /// Preserve signed interpretation after each wrapping control operation
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

} // anonymous namespace

uint16_t steering_axis_state::filtered_drive(uint16_t const previous, uint16_t const target, game_duration const step) noexcept {
  /// 7C15 publishes the midpoint reference and its signed timestep-scaled drive
  reference = static_cast<uint16_t>(previous + (word(target - previous) >> 1));
  return static_cast<uint16_t>((word(reference) * word(step)) >> 8);
}

uint16_t steering_axis_state::keyboard(bool const negative, bool const positive, bool const control, game_duration const step) noexcept {
  /// 7C04/7C50 retain opposite-key behaviour, initial force and Ctrl's frame-step-dependent force changes
  uint16_t constexpr initial_keyboard_force{0x0db6};
  int constexpr keyboard_force_limit{8191};
  auto const previous{keyboard_target};
  int const signed_previous{word(previous)};
  auto const magnitude{static_cast<uint16_t>(signed_previous < 0 ? -signed_previous : signed_previous)};
  uint16_t target{0};
  if(negative && positive) {
    target = previous;
    if(control) {
      auto const reduction{static_cast<uint16_t>(step << 2)};
      auto const reduced{magnitude < reduction ? 0 : magnitude - reduction};
      target = static_cast<uint16_t>(signed_previous < 0 ? -reduced : reduced);
    }
  } else if(negative || positive) {
    target = magnitude == 0 ? initial_keyboard_force : magnitude;
    if(control) target = static_cast<uint16_t>(std::clamp<int>(word(magnitude + static_cast<uint16_t>(step << 3)), -keyboard_force_limit, keyboard_force_limit));
    if(negative) target = static_cast<uint16_t>(-target);
  }
  keyboard_target = target;
  return filtered_drive(previous, negative && positive && !control ? 0 : target, step);
}

uint16_t steering_axis_state::mouse(uint16_t const position, uint16_t const sensitivity, game_duration const step) noexcept {
  /// 7C73 combines a wrapping counter delta with the previous target, using asymmetric negative-side rounding
  uint16_t constexpr filter_step_origin{128};                                  // previous target coefficient is twice (128 - timestep), in 8-bit fixed point
  uint32_t constexpr negative_rounding_bias{128};                              // negative motion rounds the discarded fractional byte differently
  auto const delta{static_cast<uint16_t>(position - previous_mouse)};
  previous_mouse = position;
  auto target{static_cast<uint16_t>(static_cast<uint32_t>(delta) * sensitivity)};
  auto const previous{mouse_target};
  auto const coefficient{static_cast<uint16_t>((filter_step_origin - step) * 2)};
  int const old{word(previous)};
  auto const product{static_cast<uint32_t>(coefficient) * static_cast<uint16_t>(old < 0 ? -old : old)};
  if(old < 0) target = static_cast<uint16_t>(target - ((product + negative_rounding_bias) >> 8));
  else target = static_cast<uint16_t>(target + (product >> 8));
  mouse_target = target;
  return filtered_drive(previous, target, step);
}

uint16_t steering_axis_state::joystick(int16_t const position, game_duration const step) noexcept {
  // 7BCA averages current/previous samples, then removes the inner 1/16 of
  // calibrated travel. Full deflection is 0x0f00, scaled by timestep at 7BA6.
  int const midpoint{word(previous_joystick + (word(position - previous_joystick) >> 1))};
  previous_joystick = position;
  int constexpr calibrated_extent{32767};
  int constexpr response_scale{4096};
  int constexpr centre_deadzone{256};
  int constexpr maximum_response{response_scale - centre_deadzone};
  int const magnitude{std::abs(midpoint)};
  int const response{magnitude >= calibrated_extent ? maximum_response
    : std::max(0, magnitude * response_scale / calibrated_extent - centre_deadzone)};
  reference = static_cast<uint16_t>(midpoint < 0 ? -response : response);
  return static_cast<uint16_t>((word(reference) * word(static_cast<uint16_t>(step << 8))) >> 16);
}

flight_steering flight_controls_state::update(flight_controls_input const input, game_duration const frame_step) noexcept {
  /// 7AD6 gives nonzero keyboard drive priority over ordinary mouse processing; joystick is the final fallback; VR orientation remains separate
  flight_steering result{
    .bank{bank.keyboard(input.left, input.right, input.control, frame_step)},
    .pitch{pitch.keyboard(input.up, input.down, input.control, frame_step)},
  };
  if(result.bank != 0 || result.pitch != 0) return result;
  result.bank = bank.mouse(input.mouse_x, input.mouse_sensitivity, frame_step);
  result.pitch = pitch.mouse(static_cast<uint16_t>(-input.mouse_y), input.mouse_sensitivity, frame_step);
  if(result.bank == 0 && result.pitch == 0 && input.joystick_enabled) {
    result.bank = bank.joystick(input.joystick_bank, frame_step);
    result.pitch = pitch.joystick(input.joystick_pitch, frame_step);
  } else if(!input.joystick_enabled) {
    bank.previous_joystick = pitch.previous_joystick = 0;
  }
  return result;
}

} // namespace darker::game
