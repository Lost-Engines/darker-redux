#include <catch2/catch_test_macros.hpp>
#include <bit>
#include <limits>
#include <stdexcept>
#include "graphics/model_lighting.h"
#include "reference/model_lighting_samples.h"

TEST_CASE("Distance shading matches native ramps and beacon strength selection", "[graphics][lighting]") {
  /// Include city/underground ramps, byte boundaries, clamping and signed near-path depths
  for(auto const &sample : darker::test_reference::model_lighting_samples) {
    if(!sample.near && sample.depth + 16 * (255 - sample.light) > 65535) continue; // removed native darkness-depth wrap
    CAPTURE(sample.count, sample.depth, sample.light, sample.near);
    darker::graphics::distance_shading const lighting{sample.count};
    auto const colours{lighting.colours(sample.near ? static_cast<uint32_t>(std::bit_cast<int16_t>(static_cast<uint16_t>(sample.depth))) : sample.depth,
      sample.near ? darker::graphics::model_path::near_clipped : darker::graphics::model_path::direct,
      static_cast<uint8_t>(sample.light))};
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const shade : colours.shades) fingerprint = (fingerprint ^ shade) * 0x100000001b3;
    fingerprint = (fingerprint ^ colours.dynamic) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
  }
}

TEST_CASE("Every generated distance shade matches the original table", "[graphics][lighting]") {
  /// Check all 28 palette shades in every row of both original table sizes
  for(auto const &sample : darker::test_reference::shade_table_samples) {
    darker::graphics::distance_shading const lighting{sample.count};
    uint64_t fingerprint{0xcbf29ce484222325};
    for(unsigned int row{0}; row < sample.count; ++row) {
      auto const colours{lighting.colours(static_cast<uint16_t>(row * 256), darker::graphics::model_path::direct, 255)};
      for(auto const shade : colours.shades) fingerprint = (fingerprint ^ shade) * 0x100000001b3;
    }
    REQUIRE(fingerprint == sample.fingerprint);
  }
}

TEST_CASE("Configurable fog has independent clear distance and retains beacon darkness", "[graphics][lighting]") {
  using darker::graphics::model_path;
  darker::graphics::distance_shading const fog{darker::graphics::fog_range{
    .start_cells{32},
    .end_cells{128},
  }};
  auto const clear{fog.colours(0, model_path::direct, 255)};
  CHECK(fog.colours(32 * 1024, model_path::direct, 255).shades == clear.shades);
  auto const middle{fog.colours(80 * 1024, model_path::direct, 255)};
  auto const distant{fog.colours(128 * 1024, model_path::direct, 255)};
  CHECK(middle.shades.back() < clear.shades.back());
  CHECK(middle.shades.back() > distant.shades.back());
  CHECK(fog.colours(180 * 1024, model_path::direct, 255).shades == distant.shades);
  CHECK(fog.colours(0, model_path::direct, 0).shades.back() < clear.shades.back());
  CHECK(fog.colours(32 * 1024, model_path::direct, 0).shades == fog.colours(0, model_path::direct, 0).shades);
  darker::graphics::distance_shading const clear_world{darker::graphics::fog_range{
    .start_cells{182},
    .end_cells{182},
  }};
  CHECK(clear_world.colours(181 * 1024, model_path::direct, 255).shades == clear.shades);
}

TEST_CASE("Fog rejects reversed and non-finite ranges", "[graphics][lighting]") {
  for(auto const range : {
    darker::graphics::fog_range{
      .start_cells{-1},
      .end_cells{64},
    },
    darker::graphics::fog_range{
      .start_cells{65},
      .end_cells{64},
    },
    darker::graphics::fog_range{
      .end_cells{183},
    },
    darker::graphics::fog_range{
      .start_cells{std::numeric_limits<double>::quiet_NaN()},
    },
    darker::graphics::fog_range{
      .end_cells{std::numeric_limits<double>::infinity()},
    },
  }) {
    REQUIRE_THROWS_AS(darker::graphics::distance_shading{range}, std::invalid_argument);
  }
}

TEST_CASE("Distant point colours retain the native cross-row shade lookup", "[graphics][lighting]") {
  using namespace darker::graphics;
  for(unsigned int const count : {28u, 60u}) {
    distance_shading const lighting{count};
    for(unsigned int row{0}; row < count; ++row) {
      auto const colours{lighting.colours(row * 256, model_path::direct, 255)};
      auto const next{lighting.colours((row + 1) * 256, model_path::direct, 255)};
      for(unsigned int source{0}; source < 256; ++source) {
        auto const shade{source & model_colours::shade_mask};
        auto const expected{shade < model_colours::shade_count ? colours.shades[shade]
          : row + 1 < count ? next.shades[shade - model_colours::shade_count] : uint8_t{0}};
        CHECK(colours.point_colour(static_cast<uint8_t>(source)) == static_cast<uint8_t>((source & model_colours::ramp_mask) + expected));
      }
    }
  }
  // A point can precede the first mesh and therefore use the zeroed shade latch.
  CHECK(model_colours{}.point_colour(255) == 224);
}

TEST_CASE("Logarithmic fog thickens nearby shading without moving either endpoint", "[graphics][lighting]") {
  using namespace darker::graphics;
  distance_shading const linear{fog_range{
    .start_cells{20},
    .end_cells{120},
    .curve{0},
  }};
  distance_shading const curved{fog_range{
    .start_cells{20},
    .end_cells{120},
    .curve{100},
  }};
  auto const sample{[](distance_shading const &fog, unsigned int cells, uint8_t light = 255) {
    return fog.colours(cells * render_geometry::camera_units_per_cell, model_path::direct, static_cast<uint8_t>(light)).shades;
  }};
  CHECK(sample(curved, 0) == sample(linear, 0));
  CHECK(sample(curved, 20) == sample(linear, 20));
  CHECK(sample(curved, 30).back() < sample(linear, 30).back());
  // log(11)/log(101) = 0.51957: about 52% fog at 10% of the distance.
  CHECK(sample(curved, 30) == sample(linear, 72));
  CHECK(sample(curved, 120) == sample(linear, 120));
  CHECK(sample(curved, 182) == sample(curved, 120));
  CHECK(sample(curved, 20, 0) == sample(linear, 20, 0));
  auto previous{sample(curved, 20)};
  for(unsigned int cells{21}; cells <= 120; ++cells) {
    auto const current{sample(curved, cells)};
    for(unsigned int shade{0}; shade < current.size(); ++shade) {
      // Palette shade zero rises towards one; the remaining shades fall towards it.
      if(shade == 0) CHECK(current[shade] >= previous[shade]);
      else CHECK(current[shade] <= previous[shade]);
    }
    previous = current;
  }
  for(double const value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    fog_range const invalid{
      .curve{value},
    };
    REQUIRE_THROWS_AS(distance_shading{invalid}, std::invalid_argument);
  }
}

TEST_CASE("Background fade starts only after the original palette fog is complete", "[graphics][lighting]") {
  using namespace darker::graphics;
  framework::render::colour_palette palette;
  for(unsigned int i{0}; i < palette.size(); ++i) {
    auto const value{static_cast<uint8_t>(i)};
    palette[i] = {value, value, value};
  }
  fog_range const range{
    .start_cells{1},
    .end_cells{90},
    .curve{100},
    .background_end_cells{100}
  };
  distance_shading const original{range};
  distance_shading fog{range};
  fog.set_fog_palette(&palette, 127);
  for(auto const light : {0, 128, 255}) {
    for(unsigned int distance{0}; distance <= 100; ++distance) {
      auto const depth{distance * render_geometry::camera_units_per_cell};
      auto const sample{fog.colours(depth, model_path::direct, static_cast<uint8_t>(light))};
      auto const reference{original.colours(depth, model_path::direct, static_cast<uint8_t>(light))};
      CHECK(sample.shades == reference.shades);
      if(distance <= 90) {
        CHECK_FALSE(sample.fog);
        for(unsigned int i{0}; i < 256; ++i) CHECK(sample.point_colour(static_cast<uint8_t>(i)) == reference.point_colour(static_cast<uint8_t>(i)));
      }
    }
  }
  int previous_error{127};
  for(unsigned int distance{90}; distance <= 100; ++distance) {
    auto const sample{fog.colours(distance * render_geometry::camera_units_per_cell, model_path::direct, 255)};
    int const error{127 - sample.apply_fog(0)};
    CHECK(error <= previous_error);
    previous_error = error;
  }
  auto const halfway{fog.colours(95 * render_geometry::camera_units_per_cell, model_path::direct, 255)};
  CHECK(halfway.apply_fog(0) >= 61);
  CHECK(halfway.apply_fog(0) <= 64); // linear halfway blend, quantised to a shade-table row
  for(auto const distance : {100, 110}) {
    auto const far{fog.colours(distance * render_geometry::camera_units_per_cell, model_path::direct, 255)};
    for(unsigned int i{0}; i < 256; ++i) CHECK(far.point_colour(static_cast<uint8_t>(i)) == 127);
  }
  CHECK_FALSE(fog.colours(static_cast<render_geometry::coordinate_bits>(-1), model_path::near_clipped, 255).fog);
  fog.set_fog_palette(nullptr, 0);
  CHECK_FALSE(fog.colours(100 * render_geometry::camera_units_per_cell, model_path::direct, 255).fog);
}

TEST_CASE("Background fade validates its range and permits a disabled interval", "[graphics][lighting]") {
  using namespace darker::graphics;
  for(double const end : {-1.0, 89.0, 183.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    fog_range const range{
      .start_cells{1},
      .end_cells{90},
      .background_end_cells{end}
    };
    CHECK_THROWS_AS(distance_shading{range}, std::invalid_argument);
  }
  framework::render::colour_palette const palette{};
  for(auto const end : {std::optional<double>{}, std::optional<double>{90}}) {
    distance_shading fog{fog_range{
      .start_cells{1},
      .end_cells{90},
      .background_end_cells{end}
    }};
    fog.set_fog_palette(&palette, 127);
    CHECK_FALSE(fog.colours(100 * render_geometry::camera_units_per_cell, model_path::direct, 255).fog);
  }
}
