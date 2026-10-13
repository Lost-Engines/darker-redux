#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <boost/program_options.hpp>
#include "input/joystick_options.h"
#include "presentation/options_menu.h"

using darker::presentation::preferences;
using darker::presentation::options_key;
using darker::presentation::options_menu;

TEST_CASE("Options preview preserves valid fog ordering and cancellation restores the snapshot", "[options]") {
  options_menu menu;
  auto const original{menu.values};
  menu.open();
  menu.key(options_key::down);
  menu.key(options_key::down);
  for(int i{0}; i < 100; ++i) menu.key(options_key::left);
  CHECK(menu.values.draw_distance == 2);
  CHECK(menu.values.valid());
  CHECK(menu.values.fade_distance == 2);
  menu.key(options_key::cancel);
  CHECK_FALSE(menu.active);
  CHECK(menu.values == original);
}

TEST_CASE("Options apply is a save request and defaults remain a cancellable preview", "[options]") {
  options_menu menu;
  menu.values.width = 960;
  menu.open();
  for(int i{0}; i < 11; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.values == preferences{});
  menu.key(options_key::cancel);
  CHECK(menu.values.width == 960);
  menu.open();
  for(int i{0}; i < 12; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.save_requested);
  CHECK(menu.active); // The host only closes after a successful write.
}

TEST_CASE("Display preferences round-trip and reject malformed values", "[options]") {
  auto const path{std::filesystem::temp_directory_path() / "darker-redux-options-test.cfg"};
  struct cleanup {
    std::filesystem::path path;
    ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); }
  } remove_test_file{path};
  preferences saved;
  saved.width = 853;
  saved.height = 479;
  saved.minimum_light_size = 0.25;
  saved.field_of_view = 73.125;
  saved.background_fade = false;
  saved.music_volume = 35;
  saved.effects_volume = 80;
  saved.controls.primary_mouse_button = 2;
  saved.controls.secondary_mouse_button = -1;
  saved.controls.mouse_sensitivity = 1.7;
  saved.controls.invert_mouse = true;
  saved.controls.keys[0] = 'J';
  saved.joystick.device = 2;
  saved.joystick.bank_axis = 3;
  saved.joystick.invert_pitch = true;
  saved.joystick.deadzone_percent = 12.5;
  saved.joystick.buttons[0] = 12;
  darker::presentation::write_preferences(path, saved);
  CHECK(darker::presentation::read_preferences(path) == saved);
  { std::ofstream output{path}; output << "joystick-deadzone=0\n"; }
  CHECK(darker::presentation::read_preferences(path).joystick.deadzone_percent == 6.25);
  for(auto const bad : {"fov=nan", "width=12", "draw-distance=999", "fog-start=100", "gouraud=2", "fog-curve=12garbage", "mouse-sensitivity=0", "mouse-sensitivity=nan", "invert-mouse=2", "bind.left=69", "bind.left=256", "music-volume=101", "effects-volume=-1", "mouse-primary=1", "mouse-secondary=8", "joystick-device=16", "joystick-bank-axis=1", "joystick-deadzone-percent=51", "joystick-deadzone-percent=-1", "joystick-sensitivity=0", "joystick-button.left=0"}) {
    { std::ofstream output{path}; output << bad << '\n'; }
    CHECK_THROWS(darker::presentation::read_preferences(path));
  }
}


TEST_CASE("Control bindings preserve defaults and report conflicting assignments", "[options][controls]") {
  options_menu menu;
  auto const original{menu.values};
  menu.open();
  menu.click(30, 35 + 9 * 11); // Controls.
  menu.key(options_key::right);
  CHECK(menu.values.controls.mouse_sensitivity == 1.1);
  menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.values.controls.invert_mouse);
  menu.key(options_key::down);
  menu.key(options_key::down);
  menu.key(options_key::down);
  menu.key(options_key::down); // Bank left.
  menu.key(options_key::accept);
  REQUIRE(menu.capturing_key());
  menu.capture_key('E'); // Already assigned to engine/shield.
  CHECK(menu.capturing_key());
  CHECK_FALSE(menu.notice.empty());
  menu.capture_key(299); // F10 stays available regardless of bindings.
  CHECK(menu.capturing_key());
  menu.capture_key('J');
  CHECK_FALSE(menu.capturing_key());
  CHECK(menu.values.controls.command_key('J') == original.controls.keys[0]);
  CHECK(menu.values.controls.command_key(original.controls.keys[0]) == -1);
  menu.key(options_key::cancel); // Back to options, retaining the preview.
  CHECK(menu.active);
  CHECK(menu.values.controls.keys[0] == 'J');
  menu.key(options_key::cancel); // Cancel all changes.
  CHECK(menu.values == original);
  CHECK_FALSE(menu.active);
}

TEST_CASE("Control capture can cancel, clear and reassign a previously occupied key", "[options][controls]") {
  options_menu menu;
  menu.open();
  menu.click(30, 35 + 9 * 11);
  for(int i{0}; i < 5; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  menu.capture_key(256); // Escape cancels only capture.
  CHECK_FALSE(menu.capturing_key());
  CHECK(menu.active);
  menu.key(options_key::accept);
  menu.capture_key(261); // Delete unbinds.
  CHECK(menu.values.controls.keys[0] == -1);
  menu.key(options_key::down);
  menu.key(options_key::accept);
  menu.capture_key(darker::input::control_descriptions()[0].default_key);
  CHECK(menu.values.controls.keys[1] == darker::input::control_descriptions()[0].default_key);
  CHECK(menu.values.controls.valid());
}

TEST_CASE("Mouse settings preserve fractional motion, wrapping and capture resets", "[controls]") {
  darker::input::control_preferences settings;
  darker::input::mouse_motion motion;
  CHECK(motion.sample(100, 200, settings) == vec2<uint16_t>{0, 0});
  CHECK(motion.sample(103, 198, settings) == vec2<uint16_t>{3, 2});
  settings.mouse_sensitivity = 0.5;
  CHECK(motion.sample(104, 198, settings).x == 3);
  CHECK(motion.sample(105, 198, settings).x == 4);
  settings.invert_mouse = true;
  CHECK(motion.sample(105, 196, settings).y == 1);
  motion.reset();
  CHECK(motion.sample(-5000, 9999, settings) == vec2<uint16_t>{0, 0});
  settings.mouse_sensitivity = 1;
  CHECK(motion.sample(-5001, 9999, settings).x == 65535);
}

TEST_CASE("Legacy key aliases follow the rebound action rather than remaining hardcoded", "[controls]") {
  darker::input::control_preferences settings;
  auto const index{std::to_underlying(darker::input::control_action::boost)};
  CHECK(settings.command_key(335) == 257); // Keypad Enter, Enter.
  settings.keys[index] = 'B';
  CHECK(settings.command_key('B') == 257);
  CHECK(settings.command_key(335) == -1);
  CHECK(settings.command_key(257) == -1);
  CHECK(settings.binding_for_command(257) == 'B');
  CHECK(settings.valid());
}

TEST_CASE("Scrolling controls reaches the last weapon and control defaults", "[options][controls]") {
  options_menu menu;
  menu.open();
  menu.click(30, 35 + 9 * 11);
  menu.key(options_key::up); // Back to options.
  menu.key(options_key::up); // Restore control defaults.
  menu.key(options_key::up); // Weapon 0.
  menu.key(options_key::accept);
  menu.capture_key('J');
  CHECK(menu.values.controls.keys.back() == 'J');
  menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.values.controls == darker::input::control_preferences{});
}

TEST_CASE("Mouse wheel stops at list boundaries and control footer remains fixed", "[options][controls]") {
  options_menu menu;
  menu.open();
  menu.wheel(-1);
  menu.key(options_key::right);
  CHECK(menu.values.width == 640); // Top-level wheel did not wrap to Return.
  menu.click(30, 35 + 9 * 11);
  for(int i{0}; i < 100; ++i) menu.wheel(1);
  menu.click(30, 35 + 11 * 11); // Last visible binding remains Weapon 0.
  REQUIRE(menu.capturing_key());
  menu.capture_key('J');
  CHECK(menu.values.controls.keys.back() == 'J');
  menu.click(30, 176); // Defaults stays outside the scrolling list.
  CHECK(menu.values.controls == darker::input::control_preferences{});
  for(int i{0}; i < 100; ++i) menu.wheel(-1);
  menu.click(250, 35); // First setting remains mouse sensitivity.
  CHECK(menu.values.controls.mouse_sensitivity == 1.1);
  menu.click(30, 187); // Back stays in the same place at either end.
  menu.key(options_key::cancel);
  CHECK_FALSE(menu.active);
}

TEST_CASE("Sound settings preview, clamp, restore and cancel", "[options]") {
  options_menu menu;
  menu.open();
  menu.click(30, 35 + 10 * 11);
  menu.key(options_key::left);
  CHECK(menu.values.music_volume == 95);
  menu.key(options_key::down);
  for(int i{0}; i < 30; ++i) menu.key(options_key::left);
  CHECK(menu.values.effects_volume == 0);
  menu.key(options_key::cancel);
  CHECK(menu.values.music_volume == 95);
  menu.key(options_key::cancel);
  CHECK(menu.values.music_volume == 100);
  CHECK(menu.values.effects_volume == 100);
}

TEST_CASE("Mouse firing buttons swap occupied assignments and allow disabling", "[options][controls]") {
  options_menu menu;
  menu.open();
  menu.click(30, 35 + 9 * 11);
  menu.key(options_key::down);
  menu.key(options_key::down);
  menu.key(options_key::right);
  CHECK(menu.values.controls.primary_mouse_button == 1);
  CHECK(menu.values.controls.secondary_mouse_button == 0);
  CHECK(menu.values.controls.valid());
  menu.key(options_key::down);
  menu.key(options_key::left);
  CHECK(menu.values.controls.secondary_mouse_button == -1);
  CHECK(menu.values.controls.valid());
}

TEST_CASE("Joystick menu captures buttons, reports conflicts and saves its settings", "[options][joystick]") {
  options_menu menu;
  menu.open();
  menu.click(30, 35 + 9 * 11);
  menu.click(30, 35 + 4 * 11);
  menu.key(options_key::right);
  CHECK(menu.values.joystick.device == 0);
  for(int i{0}; i < 7; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  REQUIRE(menu.capturing_button());
  menu.capture_button(0);
  CHECK(menu.capturing_button());
  CHECK_FALSE(menu.notice.empty());
  menu.capture_button(10);
  CHECK_FALSE(menu.capturing_button());
  CHECK(menu.values.joystick.buttons[0] == 10);
  menu.key(options_key::accept);
  menu.capture_key(261);
  CHECK(menu.values.joystick.buttons[0] == -1);
  menu.click(30, 187);
  menu.click(30, 187);
  menu.key(options_key::cancel);
  CHECK(menu.values.joystick.device == darker::input::joystick_automatic);
}

namespace {
darker::input::joystick_preferences joystick_arguments(std::vector<std::string> const &args,
  darker::input::joystick_preferences const &base = {}) {
  boost::program_options::options_description options;
  darker::input::add_joystick_options(options);
  boost::program_options::variables_map parsed;
  boost::program_options::store(boost::program_options::command_line_parser(args).options(options).run(), parsed);
  boost::program_options::notify(parsed);
  return darker::input::apply_joystick_options(base, parsed);
}
}

TEST_CASE("Joystick command line overrides only explicitly supplied settings", "[options][joystick]") {
  darker::input::joystick_preferences saved;
  saved.device = 3;
  saved.deadzone_percent = 8;
  saved.invert_pitch = true;
  CHECK(joystick_arguments({}, saved) == saved);
  auto const changed{joystick_arguments({"--joystick=off", "--joystick-invert-pitch=false", "--joystick-invert-bank", "--joystick-sensitivity=125"}, saved)};
  CHECK(changed.device == darker::input::joystick_disabled);
  CHECK_FALSE(changed.invert_pitch);
  CHECK(changed.invert_bank);
  CHECK(changed.sensitivity_percent == 125);
  CHECK(changed.deadzone_percent == 8);
  CHECK(saved.device == 3);
  CHECK(joystick_arguments({"--joystick=auto"}, saved).device == darker::input::joystick_automatic);
  CHECK(joystick_arguments({"--joystick=16"}).device == 15);
  CHECK(joystick_arguments({}).deadzone_percent == 6.25);
  CHECK(joystick_arguments({"--joystick-deadzone=6.25"}).deadzone_percent == 6.25);
  CHECK(joystick_arguments({"--joystick-deadzone=0"}).deadzone_percent == 0);
}

TEST_CASE("Joystick CLI applies axis and button swaps together and validates conflicts", "[options][joystick]") {
  auto const settings{joystick_arguments({"--joystick-bank-axis=2", "--joystick-pitch-axis=1",
    "--joystick-button=primary=2", "--joystick-button=secondary=1", "--joystick-button=engine=off"})};
  CHECK(settings.bank_axis == 1);
  CHECK(settings.pitch_axis == 0);
  CHECK(settings.buttons[std::to_underlying(darker::input::control_action::primary)] == 1);
  CHECK(settings.buttons[std::to_underlying(darker::input::control_action::secondary)] == 0);
  CHECK(settings.buttons[std::to_underlying(darker::input::control_action::engine)] == -1);
  for(auto const argument : {"--joystick=0", "--joystick=17", "--joystick=1junk", "--joystick-bank-axis=2",
    "--joystick-pitch-axis=33", "--joystick-deadzone=51", "--joystick-sensitivity=24",
    "--joystick-button=primary", "--joystick-button=bogus=1", "--joystick-button=primary=2",
    "--joystick-button=primary=0", "--joystick-button=primary=129"}) {
    CAPTURE(argument);
    CHECK_THROWS(joystick_arguments({argument}));
  }
}
