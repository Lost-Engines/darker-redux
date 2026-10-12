#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include "graphics/model_renderer.h"
#include "reference/model_renderer_samples.h"

TEST_CASE("Flat model bytecode reproduces complete native indexed frames", "[graphics][models]") {
  /// Compare calls, branches, coordinate reuse, visibility and colour selection together with rasterisation
  darker::graphics::model_workspace workspace;
  size_t case_index{0};
  for(auto const &sample : darker::test_reference::model_renderer_samples) {
    // Synthetic skip-near threshold 255 at depth byte 1 exercises signed byte overflow,
    // rather than a meaningful near-distance comparison. Redux uses an ordered wide comparison.
    bool const wide_near_comparison{sample.size == 49 && sample.code[39] == 26 && sample.code[40] == 255};
    CAPTURE(case_index, sample.mirrored, sample.dynamic);
    darker::graphics::projection_parameters parameters{
      .axes{{
        {
        .horizontal{static_cast<int16_t>(sample.mirrored ? -16384 : 16384)},
        .vertical{4096}
      },
        {
        .horizontal{4096},
        .vertical{-4096}
      },
        {
        .vertical{16384}
      },
      }},
      .horizontal{
        .fraction{11}
      },
      .vertical{
        .fraction{19}
      },
      .depth{
        .whole{256}
      },
      .origin{160, 120},
    };
    darker::graphics::model_colours colours{
      .dynamic{static_cast<uint8_t>(sample.dynamic)}
    };
    for(size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<uint8_t>(sample.dynamic == 17 ? 27 - i : i);
    std::array<std::byte, 64> bytes{};
    for(size_t i{0}; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(sample.code[i]);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_model(workspace, frame, std::span{bytes}.first(sample.size), 0, parameters, colours);
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    if(wide_near_comparison) REQUIRE(std::ranges::all_of(frame.pixels, [](auto pixel){ return pixel == 0; }));
    else REQUIRE(fingerprint == sample.fingerprint);
    ++case_index;
  }
}

TEST_CASE("Reused model storage does not make previous vertices valid", "[graphics][models]") {
  darker::graphics::model_workspace workspace;
  framework::render::indexed_cockpit_framebuffer frame{};
  darker::graphics::projection_parameters const projection{
    .depth{
      .whole{256},
    },
  };
  auto const &sample{darker::test_reference::model_renderer_samples.front()};
  std::array<std::byte, 64> model{};
  for(unsigned int i{0}; i < model.size(); ++i) model[i] = static_cast<std::byte>(sample.code[i]);
  darker::graphics::draw_model(workspace, frame, std::span{model}.first(sample.size), 0, projection, {});

  // A second model tries to reuse three vertices without emitting any of its own.
  std::array<std::byte, 17> invalid{};
  invalid[11] = std::byte{0x02}; // flat triangle
  invalid[12] = std::byte{1};    // colour
  invalid[13] = std::byte{0};
  invalid[14] = std::byte{1};
  invalid[15] = std::byte{2};
  invalid[16] = std::byte{0x0d}; // return
  REQUIRE_THROWS_AS(darker::graphics::draw_model(workspace, frame, invalid, 0, projection, {}), std::invalid_argument);
}

TEST_CASE("City light minimum enlarges distant discs without shrinking nearby discs", "[graphics][models]") {
  using namespace darker::graphics;
  std::array<std::byte, 17> model{};
  model[11] = std::byte{0x97}; // emit the origin
  model[12] = std::byte{0x22}; // disc consuming that vertex
  model[13] = std::byte{1};
  model[14] = std::byte{1};    // radius numerator 1: projects to a single pixel
  model[16] = std::byte{0x0d};
  model_colours colours{};
  colours.shades[1] = 7;
  projection_parameters const projection{
    .depth{
      .whole{256},
    },
    .origin{160, 100},
  };
  auto const render{[&](unsigned int const minimum, model_path const path) {
    framework::render::indexed_cockpit_framebuffer frame{};
    draw_model(frame, model, 0, projection, colours, {}, path, {}, model_shading::flat, minimum);
    return frame;
  }};
  for(auto const path : {model_path::direct, model_path::near_clipped}) {
    auto const native{render(0, path)};
    CHECK(std::ranges::count(native.pixels, 7) == 1);
    auto const enlarged{render(3, path)};
    CHECK(std::ranges::count(enlarged.pixels, 7) > 1);
    model[15] = std::byte{8}; // radius numerator 2049: larger than the minimum
    auto const large{render(0, path)};
    auto const large_with_minimum{render(3, path)};
    CHECK(large.pixels == large_with_minimum.pixels);
    model[15] = std::byte{0};
  }
}
