#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "graphics/model_renderer.h"
#include "graphics/near_clip.h"
#include "reference/near_clip_samples.h"
#include "reference/near_model_samples.h"

TEST_CASE("Near-plane intersections match original integer halving and saturation", "[graphics][near_clip]") {
  /// Check edge crossings independently of rasterisation, including subpixel depth and coordinate saturation
  size_t index{0};
  for(auto const &sample : darker::test_reference::near_clip_samples) {
    // Native 24-bit subtraction can reverse a crossing on extreme synthetic edges.
    // Redux keeps the full displacement; retain reference checks for representable native edges.
    bool native_edge{true};
    for(unsigned int axis{0}; axis < 3; ++axis) {
      auto const difference{static_cast<int64_t>(sample.inside[axis]) - sample.outside[axis] + (axis == 2 ? 1 : 0)};
      if(difference < -8388608 || difference > 8388607) native_edge = false;
    }
    if(!native_edge) continue;
    CAPTURE(index);
    auto const point{darker::graphics::near_intersection(
      {
        .horizontal{sample.inside[0]},
        .vertical{sample.inside[1]},
        .depth{sample.inside[2]}
      },
      {
        .horizontal{sample.outside[0]},
        .vertical{sample.outside[1]},
        .depth{sample.outside[2]}
      },
      {sample.origin[0], sample.origin[1]})};
    REQUIRE(point.x == sample.result[0]);
    REQUIRE(point.y == sample.result[1]);
    ++index;
  }
}

TEST_CASE("Near model bytecode reproduces native clipped frames", "[graphics][near_clip][models]") {
  /// Exercise clipping, winding, visibility skips and the flat fallback together with the rasteriser
  size_t index{0};
  for(auto const &sample : darker::test_reference::near_model_samples) {
    CAPTURE(index, sample.depth, sample.fraction);
    darker::graphics::projection_parameters const projection{
      .axes{{{
        .horizontal{16384},
        .vertical{4096}
      }, {
        .depth{16384}
      }, {
        .vertical{16384}
      }}},
      .horizontal{
        .fraction{11}
      },
      .vertical{
        .fraction{19}
      },
      .depth{
        .whole{static_cast<darker::graphics::render_geometry::coordinate_bits>(std::bit_cast<int16_t>(static_cast<uint16_t>(sample.depth)))},
        .fraction{static_cast<uint8_t>(sample.fraction)}
      },
      .origin{160, 84},
    };
    darker::graphics::model_colours colours{};
    for(size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<uint8_t>(i);
    std::array<std::byte, 64> bytes{};
    for(size_t i{0}; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(sample.code[i]);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_model(frame, std::span{bytes}.first(sample.size), 0, projection, colours, {
      .bottom{168},
    }, darker::graphics::model_path::near_clipped, {},
      sample.gouraud ? darker::graphics::model_shading::gouraud : darker::graphics::model_shading::flat);
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Wide projection and near crossings retain distant coordinate signs", "[graphics][near_clip]") {
  darker::graphics::camera_vertex const far{
    .horizontal{100000LL * 256},
    .vertical{-50000LL * 256},
    .depth{200000LL * 256},
  };
  auto const projected{darker::graphics::project_vertex(far, {160, 120})};
  CHECK(projected.x == 288);
  CHECK(projected.y == 56);
  // This edge spans more than the original signed 24-bit range. Its constant
  // lateral coordinates still intersect the near plane at exactly (8, -4) pixels.
  auto const crossing{darker::graphics::near_intersection({
    .horizontal{256},
    .vertical{-128},
    .depth{100000LL * 256},
  }, {
    .horizontal{256},
    .vertical{-128},
    .depth{-100000LL * 256},
  }, {160, 120})};
  CHECK(crossing.x == 168);
  CHECK(crossing.y == 116);
}
