#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <algorithm>
#include <vector>
#include "graphics/sky_ground.h"
#include "reference/sky_ground_samples.h"

TEST_CASE("Sky and ground bands match native horizon projection", "[graphics][background]") {
  /// Compare both viewport heights, pitch directions and bank quadrants against captured native spans
  for(auto const &sample : darker::test_reference::sky_ground_samples) {
    CAPTURE(sample.height, sample.pitch, sample.roll);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_sky_ground(frame, {
      .pitch{sample.pitch},
      .roll{sample.roll}
    }, {160, static_cast<int16_t>(sample.height / 2)}, sample.height);
    uint64_t hash{14695981039346656037ull};
    for(int i{0}; i < sample.height * 320; ++i) {
      hash ^= frame.pixels[i];
      hash *= 1099511628211ull;
    }
    CHECK(hash == sample.hash);
  }
}

TEST_CASE("Sky rendering respects runtime dimensions and padded row stride", "[graphics][background]") {
  int constexpr width{853}, height{479}, stride{864};
  uint8_t constexpr untouched{255};
  std::vector<uint8_t> pixels(stride * (height + 1), untouched);
  framework::render::indexed_surface const surface{pixels, width, height, stride};
  for(auto const roll : {0u, 7000u, 19000u, 47000u}) {
    std::ranges::fill(pixels, untouched);
    darker::graphics::draw_sky_ground(surface, {
      .pitch{1000},
      .roll{static_cast<uint16_t>(roll)},
    }, {width / 2, height / 2}, height, 380);
    for(int y{0}; y < height; ++y) {
      for(int x{0}; x < width; ++x) REQUIRE(pixels[y * stride + x] != untouched);
      for(int x{width}; x < stride; ++x) REQUIRE(pixels[y * stride + x] == untouched);
    }
    for(int x{0}; x < stride; ++x) REQUIRE(pixels[height * stride + x] == untouched);
  }
}
