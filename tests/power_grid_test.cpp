#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <algorithm>
#include <string_view>
#include "game/beacon_changes.h"
#include "game/beacon_light.h"
#include "game/city_persistence.h"
#include "game/city_repairs.h"
#include "game/mission_combat.h"
#include "game/power_grid.h"
#include "game/projectile_steering.h"
#include "game/scenario_world.h"
#include "game/weapon_selection.h"
#include "maths/direction.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"
#include "resources/save_file.h"

namespace {

namespace game = darker::game;
namespace resources = darker::resources;

game::city_map power_test_city() {
  /// Use the real beacon spacing and all six transformer positions without unrelated scenery
  game::city_map cells{};
  for(unsigned int row{0}; row < game::city_map_size.row; row += game::beacon_spacing_cells) {
    for(unsigned int column{0}; column < game::city_map_size.column; column += game::beacon_spacing_cells) {
      cells[game::city_cell_index(column, row)] = {
        .type{game::delphi_beacon_type},
        .state{255},
      };
    }
  }
  for(auto const &station : game::delphi_power_stations) {
    for(auto const &position : station.transformers) cells[game::city_cell_index(position.column, position.row)].type = game::delphi_power_rules::transformer_type;
  }
  return cells;
}

void destroy_transformer(game::city_map &cells, unsigned int const station, unsigned int const transformer) {
  /// Change only destruction, as a successful impact does; leave permission absent
  auto const &position{game::delphi_power_stations[station].transformers[transformer]};
  cells[game::city_cell_index(position.column, position.row)].state |= game::delphi_damage_bit;
}

void destroy_station(game::city_map &cells, unsigned int const station) {
  /// Each station loses supply only when both transformers have been destroyed
  destroy_transformer(cells, station, 0);
  destroy_transformer(cells, station, 1);
}

} // anonymous namespace

TEST_CASE("Transformer pairs cause local outages with a dim boundary", "[game][beacons][power]") {
  unsigned int const station{GENERATE(0u, 1u, 2u)};
  unsigned int const first_transformer{GENERATE(0u, 1u)};
  struct probes {
    unsigned int nearby;
    unsigned int intermediate;
    unsigned int distant;
  };
  static std::array<probes, 3> constexpr probe_cells{{
    {
      .nearby{game::city_cell_index(18, 18)},
      .intermediate{game::city_cell_index(45, 18)},
      .distant{game::city_cell_index(63, 18)},
    },
    {
      .nearby{game::city_cell_index(117, 54)},
      .intermediate{game::city_cell_index(90, 54)},
      .distant{game::city_cell_index(72, 54)},
    },
    {
      .nearby{game::city_cell_index(117, 108)},
      .intermediate{game::city_cell_index(90, 108)},
      .distant{game::city_cell_index(72, 108)},
    },
  }};
  auto cells{power_test_city()};
  auto const &probes{probe_cells[station]};
  game::beacon_changes script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  destroy_transformer(cells, station, first_transformer);
  grid.advance(cells, script, 256, 256);
  CHECK(cells[probes.nearby].state == 255);
  destroy_transformer(cells, station, 1 - first_transformer);
  grid.advance(cells, script, 384, 128);
  CHECK(cells[probes.nearby].state == 128);
  grid.advance(cells, script, 512, 128);
  CHECK(cells[probes.nearby].state == 0);
  CHECK(cells[probes.intermediate].state > 0);
  CHECK(cells[probes.intermediate].state < 255);
  CHECK(cells[probes.distant].state == 255);
  auto const stable{cells[probes.intermediate].state};
  for(unsigned int i{0}; i < 1000; ++i) grid.advance(cells, script, static_cast<game::clock_tick>(512 + (i + 1) * 8), 8);
  CHECK(cells[probes.intermediate].state == stable);                            // repeated frames must not accumulate dimming
  CHECK(cells[probes.nearby].state == 0);
  CHECK(cells[probes.distant].state == 255);
  grid.restore_beacons(cells);
  CHECK(cells[probes.nearby].state == 255);
  CHECK(cells[probes.intermediate].state == 255);
}

TEST_CASE("Three failed stations extinguish every beacon regardless of distance", "[game][beacons][power]") {
  auto cells{power_test_city()};
  game::beacon_changes script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  destroy_station(cells, 0);
  destroy_station(cells, 1);
  destroy_transformer(cells, 2, 0);
  grid.advance(cells, script, 256, 256);
  CHECK(cells[game::city_cell_index(0, 126)].state == 255);
  destroy_transformer(cells, 2, 1);
  grid.advance(cells, script, 384, 128);
  CHECK(cells[game::city_cell_index(0, 126)].state == 128);
  grid.advance(cells, script, 512, 128);
  unsigned int beacons{0};
  for(auto const &cell : cells) {
    if(cell.type == game::delphi_beacon_type) {
      CHECK(cell.state == 0);
      ++beacons;
    }
  }
  CHECK(beacons == 225);
  auto constexpr near{game::city_cell_index(18, 18)};
  darker::maths::world_position const position{
    .column{18 * 256 + 128},
    .row{18 * 256 + 128},
    .height{2400},
  };
  CHECK(game::beacon_light(cells, position, {}) == 0);
  grid.restore_beacons(cells);
  CHECK(cells[near].state == 255);
  CHECK(game::beacon_light(cells, position, {}) > 0);
}

TEST_CASE("Power loss leaves radio coverage and unrelated world cells alone", "[game][beacons][power]") {
  auto cells{power_test_city()};
  cells[game::city_cell_index(13, 13)] = {
    .type{12},
    .state{0},
  };
  cells[game::city_cell_index(18, 17)] = {
    .type{11},
    .state{0x43},
  };
  cells[game::city_cell_index(23, 14)] = {
    .type{83},
    .state{0x20},
  };
  cells[game::city_cell_index(21, 11)] = {
    .type{84},
    .state{0x20},
  };
  game::beacon_changes script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  CHECK(cells[game::city_cell_index(18, 18)].state == 255);                      // generators and components do not substitute for transformers
  destroy_station(cells, 0);
  grid.advance(cells, script, 256, 256);
  CHECK(cells[game::city_cell_index(18, 18)].state == 0);
  CHECK(cells[game::city_cell_index(13, 13)].state == 0);
  CHECK(cells[game::city_cell_index(18, 17)].state == 0x43);                     // streetlight appearance follows beacon lighting, not destruction
  CHECK(cells[game::city_cell_index(23, 14)].state == 0x20);
  CHECK(cells[game::city_cell_index(21, 11)].state == 0x20);
}

TEST_CASE("Script fades and restoration remain independent of temporary supply loss", "[game][beacons][power]") {
  auto cells{power_test_city()};
  destroy_station(cells, 0);
  auto const origin{GENERATE(uint8_t{0x22}, uint8_t{0x52})};                    // fully unpowered and partly powered beacons
  auto const restoring{GENERATE(false, true)};
  if(restoring) {
    for(auto &cell : cells) {
      if(cell.type == game::delphi_beacon_type) cell.state = 0;
    }
  }
  auto expected{cells};
  game::beacon_changes script, reference_script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  if(restoring) {
    script.command(0x14, 0, 0, 0, {});
    reference_script.command(0x14, 0, 0, 0, {});
  }
  game::clock_tick constexpr start{65500};
  script.command(0x12, origin, 1, start, {});
  reference_script.command(0x12, origin, 1, start, {});
  for(unsigned int elapsed{0}; elapsed <= 512; elapsed += 8) {
    auto const clock{static_cast<game::clock_tick>(start + elapsed)};
    grid.advance(cells, script, clock, 8);
    reference_script.advance(expected, clock);
    for(unsigned int i{0}; i < cells.size(); ++i) {
      if(cells[i].type == game::delphi_beacon_type) CHECK(cells[i].state <= expected[i].state);
    }
  }
  grid.restore_beacons(cells);
  for(unsigned int i{0}; i < cells.size(); ++i) CHECK(cells[i].state == expected[i].state);
  grid.restore_beacons(cells);                                                // save cleanup is safe when repeated
  for(unsigned int i{0}; i < cells.size(); ++i) CHECK(cells[i].state == expected[i].state);
}

TEST_CASE("Power station repair preserves authorisation and other city damage", "[game][collateral][power]") {
  game::city_map cells{};
  for(unsigned int flags{0}; flags < 256; ++flags) {
    for(unsigned int type{82}; type <= 86; ++type) {
      auto const i{(type - 82) * 256 + flags};
      cells[i] = {
        .type{static_cast<uint8_t>(type)},
        .state{static_cast<uint8_t>(flags)},
      };
    }
  }
  auto const original{cells};
  game::repair_power_station_buildings(cells);
  for(unsigned int i{0}; i < 5 * 256; ++i) {
    auto const &before{original[i]};
    bool const repair{before.type >= 83 && before.type <= 85 && !(before.state & game::city_cell::permitted_target)};
    CHECK(cells[i].state == (repair ? before.state & ~game::delphi_damage_bit : before.state));
    CHECK(cells[i].type == before.type);
  }
}

TEST_CASE("Retail transformer pairs and mission-75 authorisation survive a saved campaign", "[game][beacons][power][integration]") {
  if(std::string_view{DARKER_TEST_INSTALL_DIR}.empty()) SKIP("Requires retail packs");
  resources::archive_set const archives{DARKER_TEST_INSTALL_DIR};
  resources::geometry_bank const bank{archives.load({0, 30})};
  resources::campaign_resources campaign{archives};
  auto cells{game::make_city_map(archives.load({0, 68}), true)};
  unsigned int transformers{0};
  for(auto const &station : game::delphi_power_stations) {
    for(auto const &position : station.transformers) {
      CHECK(cells[game::city_cell_index(position.column, position.row)].type == 85);
      ++transformers;
    }
  }
  CHECK(std::ranges::count(cells, uint8_t{85}, &game::city_cell::type) == transformers);
  auto const &scenario{campaign.scenario(75)};
  auto const &mission{scenario.records()[resources::select_campaign_stage(75).record]};
  game::apply_scenario_cells(cells, mission);
  for(auto &cell : cells) {
    if(cell.type >= 83 && cell.type <= 85) cell.state |= game::delphi_damage_bit;
  }
  game::beacon_changes script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  CHECK(cells[game::city_cell_index(0, 126)].state == 0);                        // all three sites down, including already authorised Spervent
  grid.restore_beacons(cells);
  game::commit_beacon_queue(cells, std::array{std::byte{0x22}});
  game::repair_power_station_buildings(cells);
  resources::pilot_record pilot;
  game::pack_city_state(cells, bank.city_types(), pilot.delphi);
  cells = game::make_city_map(archives.load({0, 68}), true);
  game::restore_city_state(cells, bank.city_types(), pilot.delphi, 76);
  unsigned int marked_ruins{0};
  for(auto const &cell : cells) {
    if(cell.type < 83 || cell.type > 85) continue;
    CHECK(bool(cell.state & game::delphi_damage_bit) == bool(cell.state & game::city_cell::permitted_target));
    marked_ruins += bool(cell.state & game::delphi_damage_bit);
  }
  CHECK(marked_ruins == 14);                                                  // all of Spervent, but neither of the other two sites
  CHECK(cells[game::city_cell_index(18, 18)].state == 0);                        // explicit script outage was saved
  CHECK(cells[game::city_cell_index(27, 18)].state == 255);                      // temporary supply loss was not saved
  game::delphi_power_grid next_run;
  next_run.advance(cells, script, 0, 0);
  CHECK(cells[game::city_cell_index(27, 18)].state == 0);                        // persisted authorised ruins still disable their own station
  CHECK(cells[game::city_cell_index(117, 54)].state == 255);
  CHECK(cells[game::city_cell_index(117, 108)].state == 255);
  CHECK(cells[game::city_cell_index(0, 126)].state == 255);
}

TEST_CASE("Real Brent Ground impacts disable the three retail transformer pairs", "[game][weapons][beacons][power][integration]") {
  /// Shoot the actual models at their authored positions, without granting target permission or running mission scripts
  if(std::string_view{DARKER_TEST_INSTALL_DIR}.empty()) SKIP("Requires retail packs");
  resources::archive_set const archives{DARKER_TEST_INSTALL_DIR};
  resources::geometry_bank const bank{archives.load({0, 30})};
  auto cells{game::make_city_map(archives.load({0, 68}), true)};
  game::beacon_changes script;
  game::delphi_power_grid grid;
  grid.advance(cells, script, 0, 0);
  game::player_flight player;
  auto &craft{std::get<game::caero_flight_state>(player.craft)};
  craft.flying = true;
  player.pose().speed = 496;
  game::mission_combat combat{{}};
  combat.secondary_weapon = std::to_underlying(game::caero_weapon::brent_ground);
  game::campaign_clock clock{0};
  auto const step{[&](bool const fire = false) {
    clock += 8;
    grid.advance(cells, script, static_cast<game::clock_tick>(clock), 8);
    combat.advance(player, cells, bank, {
      .elapsed_ticks{clock},
      .frame_step{8},
      .changes{static_cast<uint16_t>(clock ^ (clock - 8))},
    }, {
      .secondary_pressed{fire},
    });
  }};
  static std::array<unsigned int, 3> constexpr nearby{
    game::city_cell_index(18, 18), game::city_cell_index(117, 54), game::city_cell_index(117, 108),
  };
  auto constexpr damage_mask{resources::profile(resources::world_kind::delphi).damage_mask};
  for(unsigned int station{0}; station < game::delphi_power_stations.size(); ++station) {
    unsigned int destroyed{0};
    for(auto const &position : game::delphi_power_stations[station].transformers) {
      CAPTURE(game::delphi_power_stations[station].name, position.column, position.row);
      auto &cell{cells[game::city_cell_index(position.column, position.row)]};
      REQUIRE(cell.state == 0);
      auto const target{static_cast<uint16_t>((position.row << 8) | position.column)};
      auto const aim{game::resolve_map_guidance(target, cells, bank, damage_mask)};
      darker::maths::world_position const centre{
        .column{aim.position.column},
        .row{aim.position.row},
        .height{static_cast<uint16_t>(aim.height - aim.height_extent / 2)},
      };
      player.pose().position = {
        .column{static_cast<uint16_t>(centre.column + 32)},
        .row{centre.row},
        .height{static_cast<uint16_t>(aim.height + 2048)},
      };
      auto const direction{darker::maths::object_target_direction(player.pose().position, centre)};
      player.pose().angles = {
        .heading{direction.heading},
        .pitch{direction.pitch},
        .roll{0},
      };
      craft.energy.reserve = 0xffff;
      combat.target.token = game::weapon_target::no_target;
      step(true);
      REQUIRE(combat.player_fired);
      auto const *shot{combat.projectiles.objects().head()};
      REQUIRE(shot != nullptr);
      for(unsigned int i{0}; i < 256 && !(shot->flags & 8); ++i) step();
      REQUIRE(shot->flags & 8);
      REQUIRE(cell.state == game::delphi_damage_bit);
      for(unsigned int i{0}; i < 32; ++i) step();                            // let supply fade and the weapon recover before the next shot
      ++destroyed;
      CHECK(cells[nearby[station]].state == (destroyed == 2 ? 0 : 255));
    }
  }
  CHECK(cells[game::city_cell_index(0, 126)].state == 0);
  CHECK(std::ranges::none_of(cells, [](auto const &cell) {
    return cell.type == game::delphi_beacon_type && cell.state != 0;
  }));
}
