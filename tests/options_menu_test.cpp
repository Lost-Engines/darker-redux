#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "presentation/options_menu.h"

using darker::presentation::display_preferences;
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
  for(int i{0}; i < 9; ++i) menu.key(options_key::down);
  menu.key(options_key::accept);
  CHECK(menu.values == display_preferences{});
  menu.key(options_key::cancel);
  CHECK(menu.values.width == 960);
  menu.open();
  for(int i{0}; i < 10; ++i) menu.key(options_key::down);
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
  display_preferences saved;
  saved.width = 853;
  saved.height = 479;
  saved.minimum_light_size = 0.25;
  saved.field_of_view = 73.125;
  saved.background_fade = false;
  darker::presentation::write_preferences(path, saved);
  CHECK(darker::presentation::read_preferences(path) == saved);
  for(auto const bad : {"fov=nan", "width=12", "draw-distance=999", "fog-start=100", "gouraud=2", "fog-curve=12garbage"}) {
    { std::ofstream output{path}; output << bad << '\n'; }
    CHECK_THROWS(darker::presentation::read_preferences(path));
  }
}
