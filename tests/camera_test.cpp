#include <catch2/catch_test_macros.hpp>
#include <bit>
#include <cmath>
#include <numbers>
#include "maths/render_sine.h"
#include "graphics/camera.h"
#include "reference/camera_samples.h"
#include "reference/object_camera_samples.h"

TEST_CASE("Camera coefficients and model origins match native fixed-point transforms", "[graphics][camera]") {
  /// Compare native coefficients everywhere and placement where no native word overflow occurred
  size_t index{0};
  for(auto const &sample : darker::test_reference::camera_samples) {
    CAPTURE(index);
    auto const basis{darker::maths::make_view_basis({
      .heading{sample.angles[0]},
      .pitch{sample.angles[1]},
      .roll{sample.angles[2]}
    })};
    for(size_t axis{0}; axis < basis.size(); ++axis) {
      REQUIRE(basis[axis].horizontal == sample.coefficients[axis * 3]);
      REQUIRE(basis[axis].vertical == sample.coefficients[axis * 3 + 1]);
      REQUIRE(basis[axis].depth == sample.coefficients[axis * 3 + 2]);
    }
    auto const placement{darker::graphics::place_model(basis,
      {
        .column{sample.camera[0]},
        .row{sample.camera[1]},
        .altitude{std::bit_cast<int16_t>(sample.camera[2])}
      },
      {
        .column{sample.origin[0]},
        .row{sample.origin[1]},
        .height{std::bit_cast<int16_t>(sample.origin[2])}
      })};
    auto const column{static_cast<int>(sample.origin[0]) * 4 - sample.camera[0]};
    auto const row{static_cast<int>(sample.origin[1]) * 4 - sample.camera[1]};
    auto const height{static_cast<int>(std::bit_cast<int16_t>(sample.origin[2])) + std::bit_cast<int16_t>(sample.camera[2])};
    // The captured camera positions have already lost their upper world bits.
    // Only compare their unaliased neighbourhood; long-range placement has separate analytic tests.
    if(column < -16384 || column > 16383 || row < -16384 || row > 16383 || height < -32768 || height > 32767) continue;
    auto const signed_whole{[](auto value) { return darker::graphics::render_geometry::wrap_coordinate(value); }};
    if(signed_whole(placement.horizontal.whole) < -32768 || signed_whole(placement.horizontal.whole) > 32767
      || signed_whole(placement.vertical.whole) < -32768 || signed_whole(placement.vertical.whole) > 32767
      || signed_whole(placement.depth.whole) < -32768 || signed_whole(placement.depth.whole) > 32767) continue;
    REQUIRE(static_cast<uint16_t>(placement.horizontal.whole) == sample.result[0]);
    REQUIRE(placement.horizontal.fraction == sample.result[1]);
    REQUIRE(static_cast<uint16_t>(placement.vertical.whole) == sample.result[2]);
    REQUIRE(placement.vertical.fraction == sample.result[3]);
    REQUIRE(static_cast<uint16_t>(placement.depth.whole) == sample.result[4]);
    REQUIRE(placement.depth.fraction == sample.result[5]);
    REQUIRE(placement.sorting_distance == sample.result[6]);
    ++index;
  }
  REQUIRE(index > 0);
}

TEST_CASE("Object attitude axes match native camera composition", "[graphics][camera]") {
  /// Compare independently rounded products rather than a floating-point matrix approximation
  auto const angles{[](std::array<int, 3> const &v){
    return darker::graphics::camera_angles{
      .heading{static_cast<uint16_t>(v[0])},
      .pitch{static_cast<uint16_t>(v[1])},
      .roll{static_cast<uint16_t>(v[2])}
    };
  }};
  for(auto const &sample : darker::test_reference::object_camera_samples) {
    auto const basis{darker::graphics::orient_model(darker::maths::make_view_basis(angles(sample.camera)), angles(sample.object), darker::maths::angle_precision::original)};
    for(size_t i{0}; i < basis.size(); ++i) {
      CAPTURE(sample.camera, sample.object, i);
      CHECK(basis[i].horizontal == sample.axes[i * 3]);
      CHECK(basis[i].vertical == sample.axes[i * 3 + 1]);
      CHECK(basis[i].depth == sample.axes[i * 3 + 2]);
    }
  }
}

TEST_CASE("Wide camera placement retains distant geometry and the actual world period", "[graphics][camera]") {
  darker::graphics::camera_basis const basis{{{
    .depth{-32768},
  }, {
    .horizontal{32767},
  }, {
    .vertical{32767},
  }}};
  // Exactly one fixed-point transform, independently calculated from map-cell displacement.
  for(unsigned int const cells : {1u, 15u, 16u, 32u, 64u, 127u}) {
    auto const placement{darker::graphics::place_model(basis, {}, {
      .column{static_cast<uint16_t>(cells * 256)},
      .row{static_cast<uint16_t>(cells * 256)},
    })};
    auto const expected{static_cast<int64_t>(cells) * 1024 * 2 * 32767};
    CHECK(placement.horizontal.whole == static_cast<uint32_t>(expected >> 16));
    CHECK(placement.horizontal.fraction == static_cast<uint8_t>(expected >> 8));
    CHECK(placement.depth.whole == cells * 1024);
    CHECK(placement.sorting_distance == cells * 2048);
  }
  auto const across_seam{darker::graphics::place_model(basis, {
    .row{255 * 1024},
  }, {})};
  CHECK(across_seam.depth.whole == 1024);
  auto const behind{darker::graphics::place_model(basis, {
    .row{1024},
  }, {})};
  CHECK(darker::graphics::render_geometry::wrap_coordinate(behind.depth.whole) == -1024);
}

TEST_CASE("Full angle sine table is rounded mathematical sine with symmetric peaks", "[graphics][camera]") {
  using namespace darker::maths;
  CHECK(render_sine(0) == 0);
  CHECK(render_sine(16384) == 32767);
  CHECK(render_sine(32768) == 0);
  CHECK(render_sine(49152) == -32767);
  for(unsigned int angle{0}; angle < angle_format::full_turn; ++angle) {
    auto const value{render_sine(static_cast<uint16_t>(angle))};
    auto const expected{32767 * std::sin(angle * 2 * std::numbers::pi / angle_format::full_turn)};
    CHECK(std::abs(value - expected) <= 0.500001);
    CHECK(render_sine(static_cast<uint16_t>(angle + angle_format::half_turn)) == -value);
    CHECK(render_cosine(static_cast<uint16_t>(angle)) == render_sine(static_cast<uint16_t>(angle + angle_format::quarter_turn)));
  }
  CHECK(original_sine[256] == 32766); // retain the source quirk for simulation
}

TEST_CASE("Camera and model rotations resolve steps inside one original sine phase", "[graphics][camera]") {
  using namespace darker::graphics;
  auto const base{make_camera_basis({})};
  auto const turned{make_camera_basis({.heading{16}})};
  auto const pitched{make_camera_basis({.pitch{16}})};
  auto const rolled{make_camera_basis({.roll{16}})};
  CHECK(base[0].horizontal != turned[0].horizontal);
  CHECK(base[2].depth != pitched[2].depth);
  CHECK(base[2].horizontal != rolled[2].horizontal);
  auto const object_base{orient_model(base, {})};
  auto const object_turned{orient_model(base, {.heading{16}})};
  CHECK(object_base[0].horizontal != object_turned[0].horizontal);
}
