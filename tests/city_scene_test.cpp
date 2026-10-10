#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "graphics/city_scene.h"
#include "graphics/world_images.h"
#include "maths/world_coordinates.h"
#include "reference/city_scan_samples.h"
#include "reference/distant_points_samples.h"
#include "reference/object_window_samples.h"

TEST_CASE("City scans preserve native candidate order and map boundaries", "[graphics][city]") {
  /// Match circular spans, heading halves, full-pitch scans and empty-cell skips
  darker::game::city_map cells{};
  for(size_t i{0}; i < cells.size(); ++i) cells[i].type = i % 7 == 0 ? 0 : 1;
  std::vector<uint16_t> candidates;
  size_t index{0};
  for(auto const &sample : darker::test_reference::city_scan_samples) {
    if(sample.radius > darker::graphics::scene_limits::native_surface_radius_cells) continue; // extended radii use the new non-wrapping circular scan
    CAPTURE(index, sample.column, sample.row, sample.heading, sample.pitch, sample.radius);
    darker::graphics::collect_city_cells(cells, static_cast<uint8_t>(sample.column), static_cast<uint8_t>(sample.row),
      {
        .heading{static_cast<uint16_t>(sample.heading)},
        .pitch{static_cast<uint16_t>(sample.pitch)}
      }, sample.radius, candidates);
    REQUIRE(candidates.size() == sample.count);
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const cell : candidates) {
      for(auto const byte : {cell & 255, cell >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
    }
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Moving-object visibility matches the native wrapping byte window", "[graphics][city]") {
  /// Exercise both axes and both native radii before narrower projection arithmetic can alias distant coordinates
  for(auto const &sample : darker::test_reference::object_window_samples) {
    darker::graphics::city_view const view{
      .column{static_cast<uint16_t>(sample[0])},
      .row{static_cast<uint16_t>(sample[1])},
      .radius{sample[2]}
    };
    darker::maths::world_position const position{
      .column{static_cast<uint16_t>(sample[3])},
      .row{static_cast<uint16_t>(sample[4])},
      .height{0}
    };
    CAPTURE(sample);
    REQUIRE(darker::graphics::within_object_window(view, position) == static_cast<bool>(sample[5]));
  }
}

TEST_CASE("Distant moving objects project to the original single pixel", "[graphics][city]") {
  /// Native 2D32 drops fractional coordinates and rejects pixels outside the cockpit/full-screen viewport
  for(auto const &sample : darker::test_reference::distant_points_samples) {
    CAPTURE(sample);
    darker::graphics::model_placement const placement{
      .horizontal{
        .whole{static_cast<darker::graphics::render_geometry::coordinate_bits>(std::bit_cast<int16_t>(static_cast<uint16_t>(sample[0])))},
        .fraction{173}
      },
      .vertical{
        .whole{static_cast<darker::graphics::render_geometry::coordinate_bits>(std::bit_cast<int16_t>(static_cast<uint16_t>(sample[1])))},
        .fraction{89}
      },
      .depth{
        .whole{static_cast<darker::graphics::render_geometry::coordinate_bits>(std::bit_cast<int16_t>(static_cast<uint16_t>(sample[2])))},
        .fraction{255}
      },
    };
    auto const point{darker::graphics::project_distant_object(placement, {160, static_cast<int16_t>(sample[3] / 2)}, {
      .bottom{sample[3]},
    }, static_cast<uint8_t>(sample[4]))};
    REQUIRE(point.has_value() == (sample[5] >= 0));
    if(point) {
      CHECK(point->x == sample[5]);
      CHECK(point->y == sample[6]);
    }
  }
}

TEST_CASE("Draw traversal supplies the native record byte after a farther subtree", "[graphics][city]") {
  /// Captured through native 2B09 insertion and 2C38 traversal, intercepting callbacks at 2D32
  std::array<uint16_t, 6> constexpr distances{10, 20, 5, 15, 30, 7};
  std::array<uint16_t, 6> constexpr records{0x8060, 0x8018, 0x8048, 0x8000, 0x8078, 0x8030};
  std::array<uint8_t, 6> constexpr residues{0, 0x18, 0, 0, 0, 0x30};
  std::vector<darker::graphics::city_draw_item> items;
  for(size_t i{0}; i < distances.size(); ++i) items.push_back({
    .placement{
      .sorting_distance{distances[i]}
    },
    .draw_record{static_cast<uint16_t>(0x8000 + i * 24)},
  });
  darker::graphics::order_city_models(items);
  for(size_t i{0}; i < items.size(); ++i) {
    CHECK(items[i].draw_record == records[i]);
    CHECK(items[i].projection_residue == residues[i]);
  }
}

TEST_CASE("Maximum visibility includes each city cell once across the world seam", "[graphics][city]") {
  darker::game::city_map cells{};
  for(auto &cell : cells) cell.type = 1;
  std::vector<uint16_t> candidates;
  for(uint8_t const origin : {0, 64, 127, 128, 255}) {
    darker::graphics::collect_city_cells(cells, origin, origin, {}, 182, candidates);
    REQUIRE(candidates.size() == cells.size());
    for(unsigned int index{0}; index < candidates.size(); ++index) CHECK(candidates[index] == index);
  }
  darker::graphics::collect_city_cells(cells, 64, 64, {}, 64, candidates);
  for(auto const index : candidates) {
    int const dx{index % 128 - 64};
    int const dy{index / 128 - 64};
    CHECK(dx * dx + dy * dy <= 64 * 64);
  }
}

TEST_CASE("Distant objects with shade code 31 render without a preceding mesh", "[graphics][city]") {
  // A complete special-model header with zero LOD thresholds selects the point
  // path. No bytecode is needed, and an empty city leaves the shade latch zero.
  std::vector<std::byte> bytes(17);
  bytes[1] = std::byte{1}; // one special model at pool offset zero
  bytes[4] = std::byte{11}; // pool size
  bytes[11] = std::byte{255}; // header +5: ramp 7, shade code 31
  darker::resources::geometry_bank const bank{std::move(bytes)};
  darker::game::city_map cells{};
  darker::graphics::city_renderer renderer;
  darker::graphics::city_view const view{
    .row{4096},
    .origin{160, 120},
  };
  std::array<darker::graphics::scene_object, 1> const objects{{{
    .model_offset{0},
    .pose{
      .position{
        .row{3072},
      },
    },
  }}};
  framework::render::indexed_cockpit_framebuffer frame{};
  CHECK(renderer.draw(frame, bank, cells, view, 0x60, darker::graphics::distance_shading{}, {}, objects) == 1);
  CHECK(frame.pixels[120 * 320 + 160] == 224);
}

TEST_CASE("Wide views include farther world copies within the visible circle", "[graphics][city]") {
  using namespace darker::graphics;
  std::vector<world_image> images;
  auto const collect{[&](world_image const image) { images.push_back(image); }};
  visit_world_images(100, 0, 0, 0, 182, collect);
  REQUIRE(images.size() == 2);
  CHECK(images[0] == world_image{});
  CHECK(images[1] == world_image{
    .column{1},
  });
  images.clear();
  visit_world_images(100, 0, 0, 0, 128, collect);
  REQUIRE(images.size() == 1);
  CHECK(images.front() == world_image{});
  images.clear();
  visit_world_images(128, 128, 0, 0, 182, collect);
  CHECK(images.size() == 4);
  images.clear();
  visit_world_images(255, 255, 0, 0, 15, collect);
  REQUIRE(images.size() == 1);
  CHECK(images.front() == world_image{
    .column{1},
    .row{1},
  });
}

TEST_CASE("Buildings and moving objects remain visible in the farther forward world copy", "[graphics][city]") {
  using namespace darker::graphics;
  // One city model, also used by the moving object. The mesh is empty but has
  // a complete header and return opcode; at distance the object becomes a dot.
  std::vector<std::byte> bytes(24);
  bytes[0] = std::byte{1};
  bytes[10] = std::byte{12};
  bytes[17] = std::byte{255};
  bytes[23] = std::byte{13};
  darker::resources::geometry_bank const bank{std::move(bytes)};
  darker::game::city_map cells{};
  cells[0].type = 1;
  city_renderer renderer;
  city_view view{
    .row{100 * 256},
    .angles{
      .heading{32768},
    },
    .origin{160, 120},
    .radius{182},
  };
  std::array<scene_object, 1> const objects{{{
    .model_offset{0},
    .pose{},
  }}};
  framework::render::indexed_cockpit_framebuffer frame{};
  distance_shading const lighting{};
  // Nearest copy is 100 cells behind; the visible copy is 156 cells ahead.
  CHECK(renderer.draw(frame, bank, cells, view, 0x60, lighting, {}, objects) == 2);
  darker::game::effect_system effects;
  effects.emitters.push_back({});
  particle_scene const particles{
    .effects{effects},
    .sheet{{}, 0, 0, 0},
    .clock{0},
  };
  // Effects are admitted for both images; projection rejects the one behind.
  CHECK(renderer.draw(frame, bank, cells, view, 0x60, lighting, {}, objects, &particles) == 4);
  view.radius = 128;
  CHECK(renderer.draw(frame, bank, cells, view, 0x60, lighting, {}, objects) == 0);
}
