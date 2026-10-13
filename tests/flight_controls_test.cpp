#include <catch2/catch_test_macros.hpp>
#include "game/flight_controls.h"
#include "reference/flight_controls_samples.h"
#include "reference/joystick_samples.h"
#include "input/joystick.h"

TEST_CASE("Keyboard and mouse steering preserve native smoothing and source priority", "[game][controls]") {
  /// Compare persistent targets, mouse history, published references and both timestep-scaled steering drives
  size_t index{0};
  for(auto const &sample : darker::test_reference::flight_controls_samples) {
    CAPTURE(index);
    auto const axis{[&](size_t const i){
      return darker::game::steering_axis_state{
        .keyboard_target{static_cast<uint16_t>(sample.before[i])},
        .mouse_target{static_cast<uint16_t>(sample.before[i + 1])},
        .previous_mouse{static_cast<uint16_t>(sample.before[i + 2])},
        .reference{static_cast<uint16_t>(sample.before[i + 3])},
      };
    }};
    darker::game::flight_controls_state state{
      .bank{axis(0)},
      .pitch{axis(4)}
    };
    auto const held{sample.input[0]};
    auto const drive{state.update(
      {
        .left{(held & 1) != 0},
        .right{(held & 2) != 0},
        .up{(held & 4) != 0},
        .down{(held & 8) != 0},
        .control{(held & 16) != 0},
        .mouse_x{static_cast<uint16_t>(sample.input[1])},
        .mouse_y{static_cast<uint16_t>(sample.input[2])},
        .mouse_sensitivity{static_cast<uint16_t>(sample.input[4])}
      }, static_cast<uint16_t>(sample.input[3]))};
    std::array<int, 8> const actual{state.bank.keyboard_target, state.bank.mouse_target, state.bank.previous_mouse, state.bank.reference,
      state.pitch.keyboard_target, state.pitch.mouse_target, state.pitch.previous_mouse, state.pitch.reference};
    REQUIRE(actual == sample.after);
    REQUIRE(drive.bank == sample.drive[0]);
    REQUIRE(drive.pitch == sample.drive[1]);
    ++index;
  }
}

TEST_CASE("Joystick fallback matches retail calibrated response and smoothing", "[game][controls][joystick]") {
  darker::game::flight_controls_state state;
  int sequence{-1};
  for(auto const &sample : darker::test_reference::joystick_samples) {
    if(sequence != sample[0]) { state = {}; sequence = sample[0]; }
    auto const drive{state.update({
      .joystick_enabled{true},
      .joystick_bank{static_cast<int16_t>(sample[1])},
      .joystick_pitch{static_cast<int16_t>(sample[2])}
    }, static_cast<uint16_t>(sample[3]))};
    CAPTURE(sample);
    CHECK(drive.bank == sample[4]);
    CHECK(drive.pitch == sample[5]);
    CHECK(state.bank.reference == sample[6]);
    CHECK(state.pitch.reference == sample[7]);
  }
}

TEST_CASE("Host joystick axes handle missing axes, inversion, deadzone and saturation", "[controls][joystick]") {
  darker::input::joystick_sample sample{
    .device{0},
    .axes{0.02f, -1.0f, 0.5f}
  };
  darker::input::joystick_preferences settings;
  settings.deadzone_percent = 5;
  CHECK(darker::input::joystick_axis(sample, 0, false, settings) == 0);
  CHECK(darker::input::joystick_axis(sample, 1, false, settings) == -32767);
  CHECK(darker::input::joystick_axis(sample, 1, true, settings) == 32767);
  CHECK(darker::input::joystick_axis(sample, 10, false, settings) == 0);
  settings.sensitivity_percent = 200;
  CHECK(darker::input::joystick_axis(sample, 1, false, settings) == -32767);
}

TEST_CASE("Joystick buttons require release after focus, connection and menu changes", "[controls][joystick]") {
  darker::input::joystick_buttons buttons;
  darker::input::joystick_sample sample{
    .device{0},
    .buttons{1, 0}
  };
  buttons.update(sample, true);
  CHECK_FALSE(buttons.down(0));
  sample.buttons[0] = 0;
  buttons.update(sample, true);
  sample.buttons[0] = 1;
  buttons.update(sample, true);
  CHECK(buttons.down(0));
  CHECK(buttons.pushed(0));
  buttons.update(sample, true);
  CHECK_FALSE(buttons.pushed(0));
  buttons.suppress();
  buttons.update(sample, true);
  CHECK_FALSE(buttons.down(0));
  buttons.update(sample, false);
  buttons.update(sample, true);
  CHECK_FALSE(buttons.down(0));
  sample.buttons[0] = 0;
  buttons.update(sample, true);
  sample.buttons[0] = 1;
  buttons.update(sample, true);
  CHECK(buttons.pushed(0));
  buttons.update({}, true);
  CHECK_FALSE(buttons.down(0));
}
