#include <catch2/catch_test_macros.hpp>
#include "graphics/gouraud_polygon.h"
#include "reference/gouraud_samples.h"

TEST_CASE("Gouraud polygons match original palette bands and clipping", "[graphics][gouraud]") {
  /// Compare all pixels for winding, narrow spans, clipped corners and positive/negative colour gradients
  size_t index{0};
  for(auto const &sample : darker::test_reference::gouraud_samples) {
    CAPTURE(index);
    std::array<darker::graphics::shaded_vertex, 4> vertices{};
    for(size_t i{0}; i < sample.count; ++i) {
      auto const &source{sample.vertices[i]};
      vertices[i] = {
        .position{static_cast<int16_t>(source[0]), static_cast<int16_t>(source[1])},
        .shade{static_cast<uint16_t>((sample.base + source[2]) * 256 + source[2] + 128)}
      };
    }
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_gouraud_polygon(frame, std::span{vertices}.first(sample.count), {
      .right{319},
      .bottom{168},
    });
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Fog translates finished Gouraud pixels rather than interpolating translated palette indices", "[graphics][gouraud]") {
  framework::render::colour_translation translation;
  for(unsigned int i{0}; i < translation.size(); ++i) translation[i] = static_cast<uint8_t>((i * 73) & 255);
  for(auto const &sample : darker::test_reference::gouraud_samples) {
    std::array<darker::graphics::shaded_vertex, 4> vertices{};
    for(unsigned int i{0}; i < sample.count; ++i) {
      auto const &source{sample.vertices[i]};
      vertices[i] = {
        .position{static_cast<int16_t>(source[0]), static_cast<int16_t>(source[1])},
        .shade{static_cast<uint16_t>((sample.base + source[2]) * 256 + source[2] + 128)}
      };
    }
    framework::render::indexed_cockpit_framebuffer original{}, fogged{};
    darker::graphics::draw_gouraud_polygon(original, std::span{vertices}.first(sample.count));
    darker::graphics::draw_gouraud_polygon(fogged, std::span{vertices}.first(sample.count), {}, &translation);
    for(unsigned int i{0}; i < original.pixels.size(); ++i) REQUIRE(fogged.pixels[i] == translation[original.pixels[i]]);
  }
}

TEST_CASE("Ground light spill compares displayed brightness after fog", "[graphics][gouraud][light-spill]") {
  framework::render::colour_palette palette{};
  for(unsigned int i{0}; i < palette.size(); ++i) {
    auto const grey{static_cast<uint8_t>((i * 73) & 255)};
    palette[i] = framework::render::rgba_pixel{grey, grey, grey, 255};
  }
  darker::graphics::lighten_blend const blend{palette};
  framework::render::colour_translation translation{};
  for(unsigned int i{0}; i < translation.size(); ++i) translation[i] = static_cast<uint8_t>((i * 31) & 255);
  for(auto const &sample : darker::test_reference::gouraud_samples) {
    std::array<darker::graphics::shaded_vertex, 4> vertices{};
    for(unsigned int i{0}; i < sample.count; ++i) {
      auto const &source{sample.vertices[i]};
      vertices[i] = {
        .position{static_cast<int16_t>(source[0]), static_cast<int16_t>(source[1])},
        .shade{static_cast<uint16_t>((sample.base + source[2]) * 256 + source[2] + 128)},
      };
    }
    framework::render::indexed_cockpit_framebuffer opaque{}, lit{};
    constexpr uint8_t background{2};
    opaque.pixels.fill(background);
    lit.pixels.fill(background);
    darker::graphics::draw_gouraud_polygon(opaque, std::span{vertices}.first(sample.count), {}, &translation);
    darker::graphics::draw_gouraud_polygon(lit, std::span{vertices}.first(sample.count), {}, &translation, &blend);
    for(unsigned int i{0}; i < opaque.pixels.size(); ++i) {
      auto const source{opaque.pixels[i]};
      REQUIRE(lit.pixels[i] == (palette[source].r > palette[background].r ? source : background));
    }
  }
}
