#include <catch2/catch_test_macros.hpp>
#include <fstream>
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
  for(int i{0}; i < 10; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.values == preferences{});
  menu.key(options_key::cancel);
  CHECK(menu.values.width == 960);
  menu.open();
  for(int i{0}; i < 11; ++i) menu.key(options_key::down);
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
  saved.controls.mouse_sensitivity = 1.7;
  saved.controls.invert_mouse = true;
  saved.controls.keys[0] = 'J';
  darker::presentation::write_preferences(path, saved);
  CHECK(darker::presentation::read_preferences(path) == saved);
  for(auto const bad : {"fov=nan", "width=12", "draw-distance=999", "fog-start=100", "gouraud=2", "fog-curve=12garbage", "mouse-sensitivity=0", "mouse-sensitivity=nan", "invert-mouse=2", "bind.left=69", "bind.left=256"}) {
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
  menu.key(options_key::down);
  menu.key(options_key::down);
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
