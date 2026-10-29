#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include "graphics/near_clip.h"
#include "graphics/procedural_hud.h"
#include "graphics/screen_layout.h"
#include "reference/procedural_samples.h"

namespace {

uint64_t checksum(framework::render::indexed_cockpit_framebuffer const &frame) {
  /// Compare every indexed pixel, including unchanged background
  uint64_t result{14695981039346656037ULL};
  for(auto const pixel : frame.pixels) result = (result ^ pixel) * 1099511628211ULL;
  return result;
}

} // anonymous namespace

TEST_CASE("Missile camera marker matches native timer boundaries and remains lit when following") {
  /// Captured at 54A3/DC65: X=300, Y=0, width=4, height=3, palette index BE
  std::array<uint16_t, 10> const ticks{0, 127, 128, 255, 256, 383, 384, 511, 512, 65535};
  std::array<bool, 10> const native_blink{false, false, true, true, true, true, true, true, false, true};
  framework::render::indexed_cockpit_framebuffer expected{};
  expected.pixels.fill(42);
  for(int y{0}; y < 3; ++y) {
    for(int x{300}; x < 304; ++x) expected.pixels[y * 320 + x] = 190;
  }
  for(bool const enabled : {false, true}) {
    for(bool const following : {false, true}) {
      for(size_t i{0}; i < ticks.size(); ++i) {
        INFO("clock=" << ticks[i] << " enabled=" << enabled << " following=" << following);
        framework::render::indexed_cockpit_framebuffer frame{};
        frame.pixels.fill(42);
        darker::graphics::draw_missile_camera_indicator(frame, ticks[i], enabled, following);
        if(following || (enabled && native_blink[i])) REQUIRE(frame.pixels == expected.pixels);
        else REQUIRE(std::ranges::all_of(frame.pixels, [](auto const pixel){
          return pixel == 42;
        }));
      }
    }
  }
}

TEST_CASE("Attitude endpoint calculation and line rasterisation match 289 original-code frames") {
  for(auto const &sample : darker::test_reference::attitude) {
    INFO("pitch=" << sample.pitch << " roll=" << sample.roll);
    framework::render::indexed_cockpit_framebuffer frame{};
    auto const line{darker::graphics::calculate_attitude(sample.pitch, sample.roll, 0, false)};
    darker::graphics::draw_screen_line(frame, line.first, line.last, line.colour);
    REQUIRE(checksum(frame) == sample.checksum);
  }
}

TEST_CASE("Target bytecode reproduces native geometry and per-step colour changes") {
  using darker::graphics::target_marker;
  std::array<target_marker, 3> const markers{target_marker::small, target_marker::large, target_marker::skimma_aim};
  for(size_t i{0}; i < markers.size(); ++i) {
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_target_marker(frame, markers[i], {160, 84}, i == 2 ? 14 : 243, i == 2 ? 14 : 233);
    REQUIRE(checksum(frame) == darker::test_reference::markers[i]);
  }
}

TEST_CASE("Original line endpoints and tie rules hold for both slopes, reversals, axes and wide spans") {
  for(auto const &sample : darker::test_reference::lines) {
    INFO("from " << sample.first.x << ',' << sample.first.y << " to " << sample.last.x << ',' << sample.last.y);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_screen_line(frame, sample.first, sample.last, 14);
    REQUIRE(checksum(frame) == sample.checksum);
  }
}

TEST_CASE("Attitude endpoints and shading preserve native signs and full table quadrants") {
  for(auto const &sample : darker::test_reference::endpoints) {
    auto const line{darker::graphics::calculate_attitude(sample.pitch, sample.roll, sample.pitch_high, sample.alternate)};
    REQUIRE(line.first.x == sample.line.first.x);
    REQUIRE(line.first.y == sample.line.first.y);
    REQUIRE(line.last.x == sample.line.last.x);
    REQUIRE(line.last.y == sample.line.last.y);
    REQUIRE(line.colour == sample.line.colour);
  }
}

TEST_CASE("Caero fixed outlines reproduce the original paired-path bytecode") {
  framework::render::indexed_cockpit_framebuffer frame{};
  darker::graphics::draw_attitude_surround(frame, 0);
  REQUIRE(checksum(frame) == darker::test_reference::attitude_surround_checksum);
}

TEST_CASE("Camera picking cross retains its green shading and transparent surroundings") {
  framework::render::indexed_cockpit_framebuffer frame{};
  frame.pixels.fill(42);
  darker::graphics::draw_camera_selection_marker(frame, 120);
  std::array<uint8_t, 7> constexpr shades{188, 189, 190, 190, 190, 189, 188};
  for(int offset{-3}; offset <= 3; ++offset) {
    CHECK(frame.pixels[(120 + offset) * 320 + 160] == shades[offset + 3]);
  }
  CHECK(frame.pixels[120 * 320 + 157] == 188);
  CHECK(frame.pixels[120 * 320 + 163] == 188);
  CHECK(std::ranges::count(frame.pixels, 42) == 320 * 240 - 13);
}

TEST_CASE("Target markers preserve the native projection at every overlay scale", "[graphics][hud]") {
  /// Changing resolution alone must not move an existing lock within the native HUD
  for(int const height : {240, 777, 1080, 1440, 2160}) {
    double const focal_length{256.0 * height / 240};
    for(int const centre_y : {92, 120}) {
      for(int const x : {-54, -20, 0, 20, 54}) {
        for(int const y : {-54, -20, 0, 20, 54}) {
          CHECK(darker::graphics::project_target_marker({x, y}, focal_length, height, centre_y)
            == darker::graphics::pixel_position{160 + x, centre_y + y});
        }
      }
    }
  }
}

TEST_CASE("Target marker placement agrees with scene projection across FOVs and framebuffers", "[graphics][hud]") {
  /// Compare the magnified HUD centre against the world projector, allowing one native HUD pixel of quantisation
  using darker::graphics::pixel_position;
  std::array<pixel_position, 5> const sizes{{{320, 240}, {1920, 1080}, {3840, 2160}, {3440, 1440}, {1000, 777}}};
  std::array<pixel_position, 5> const offsets{{{0, 0}, {48, 18}, {-45, 12}, {21, -32}, {-18, -45}}};
  double const native_fov{2 * std::atan(120.0 / 256.0) * 180 / std::numbers::pi};
  for(auto const &size : sizes) {
    double const overlay_scale{static_cast<double>(size.y) / 240};
    int const overlay_width{static_cast<int>(320 * overlay_scale)};
    int const overlay_left{(size.x - overlay_width) / 2};
    for(bool const cockpit : {false, true}) {
      int const world_height{(cockpit ? darker::graphics::cockpit_view_layout::caero_height : 240) * size.y / 240};
      int const world_top{cockpit ? static_cast<int>(darker::graphics::cockpit_view_layout::caero_top * overlay_scale) : 0};
      int const centre_y{cockpit ? 92 : 120};
      for(double const fov : {15.0, 35.0, native_fov, 75.0, 90.0, 120.0, 140.0}) {
        double const focal_length{size.y / (2 * std::tan(fov * std::numbers::pi / 360))};
        for(auto const &offset : offsets) {
          CAPTURE(size.x, size.y, cockpit, fov, offset.x, offset.y);
          auto const marker{darker::graphics::project_target_marker(offset, focal_length, size.y, centre_y)};
          int const depth{1024};
          auto const point{darker::graphics::project_vertex({
            .horizontal{offset.x * depth},
            .vertical{offset.y * depth},
            .depth{depth * 256},
          }, {static_cast<int16_t>(size.x / 2), static_cast<int16_t>(world_height / 2)}, focal_length)};
          double const marker_x{overlay_left + static_cast<double>(marker.x) * overlay_width / 320};
          double const marker_y{marker.y * overlay_scale};
          CHECK(std::abs(marker_x - point.x) <= overlay_scale + 1);
          CHECK(std::abs(marker_y - (point.y + world_top)) <= overlay_scale + 1);
        }
      }
    }
  }
}

TEST_CASE("Target marker outlines clip safely when a narrow FOV moves them beyond the HUD", "[graphics][hud]") {
  /// A retained gameplay lock can leave the visible overlay without becoming an invalid drawing request
  framework::render::indexed_cockpit_framebuffer frame;
  frame.pixels.fill(42);
  darker::graphics::draw_target_marker(frame, darker::graphics::target_marker::large, {-1, 92}, 243, 233);
  CHECK(std::ranges::any_of(frame.pixels, [](auto const pixel){ return pixel != 42; }));
  frame.pixels.fill(42);
  auto const offscreen{darker::graphics::project_target_marker({-54, 0}, 912.0, 240)};
  REQUIRE(offscreen.x < -20);
  darker::graphics::draw_target_marker(frame, darker::graphics::target_marker::large, offscreen, 243, 233);
  CHECK(std::ranges::all_of(frame.pixels, [](auto const pixel){ return pixel == 42; }));
}
