#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <filesystem>
#include <format>
#include <string_view>
#include "game/building_impact.h"
#include "game/caero_weapons.h"
#include "game/city_collision.h"
#include "game/city_persistence.h"
#include "game/city_repairs.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/native_object_layout.h"
#include "game/object_definitions.h"
#include "game/scenario_world.h"
#include "game/weapon_selection.h"
#include "presentation/front_end.h"
#include "resources/archive_set.h"
#include "resources/campaign.h"
#include "resources/geometry_bank.h"

namespace {

namespace game = darker::game;
namespace resources = darker::resources;

std::vector<std::byte> bytes(std::initializer_list<uint8_t> const values) {
  /// Keep synthetic mission opcodes readable without depending on a retail resource
  std::vector<std::byte> result;
  for(auto const value : values) result.push_back(static_cast<std::byte>(value));
  return result;
}

} // anonymous namespace

TEST_CASE("Delphi collateral preserves weapon and protected-surface rules", "[game][weapons][collateral]") {
  auto const category{GENERATE(game::collision_category::ground_target, game::collision_category::protected_surface,
    game::collision_category::fragile, game::collision_category::diffuser)};
  auto const linked{GENERATE(false, true)};
  auto const marked{GENERATE(false, true)};
  auto const slot{GENERATE(0u, 5u, 9u, 10u, 14u)};
  auto const flags{static_cast<uint8_t>(marked ? game::city_cell::permitted_target : 0)};
  bool const native{game::projectile_damages_building(slot, category, flags, linked)};
  bool const redux{game::projectile_damages_building(slot, category, flags, linked, true)};
  if(!linked || category == game::collision_category::protected_surface || (slot < 10 && category == game::collision_category::diffuser)) CHECK_FALSE(redux);
  else if(category == game::collision_category::fragile) CHECK(redux);
  else if(slot == 5) CHECK(redux);                                            // Brent Ground no longer needs authorisation
  else CHECK(redux == native);
}

TEST_CASE("Delphi ground weapons can fire unguided without inventing a target", "[game][weapons][collateral]") {
  auto const weapon{GENERATE(game::caero_weapon::brent_ground, game::caero_weapon::diffuser_gas, game::caero_weapon::diffuser_trigger)};
  auto const target{GENERATE(uint16_t{0xffff}, uint16_t{0x1717}, game::native_object_layout::actor(1))};
  auto const unrestricted{GENERATE(false, true)};
  game::projectile_pool pool;
  game::caero_energy_state energy{
    .reserve{0xffff},
  };
  uint16_t charge{0};
  game::launch_emitter const emitter{};
  auto const result{game::fire_caero_weapon(pool, energy, charge, {
    .emitter{emitter},
    .selection{std::to_underlying(weapon)},
    .pressed{true},
    .target{target},
    .unrestricted_ground_targets{unrestricted},
  })};
  bool const admitted{target == 0x1717 || (target == 0xffff && unrestricted)};
  REQUIRE((result.shot != nullptr) == admitted);
  if(admitted) {
    CHECK(result.shot->target_token == target);
    CHECK(result.shot->parameters.update_entry == (target == 0xffff ? game::object_update::direct_projectile : game::object_update::homing_projectile));
    CHECK(energy.reserve < 0xffff);
  } else CHECK(energy.reserve == 0xffff);
}

TEST_CASE("Service repairs retain other destruction and never repair authored damage", "[game][collateral]") {
  game::city_map cells{};
  game::delphi_collateral_damage damage;
  auto const destroy{[&](unsigned int const index, uint8_t const type, uint8_t const state, bool const by_player) {
    cells[index] = {
      .type{type},
      .state{state},
    };
    if(by_player) damage.record_player_destruction(index, cells[index]);
    cells[index].state |= game::delphi_damage_bit;
  }};
  destroy(0, 12, 0xc7, true);                                                 // player-damaged radar: preserve variant/open/authorisation bits
  destroy(1, 12, 0x45, false);                                                // enemy-damaged radar: keep the wreck
  destroy(2, 12, 0x65, true);                                                 // already destroyed before the player's hit
  destroy(3, 48, 0, true);                                                    // optional tank: permanent damage, no target marker
  destroy(4, 17, 0, true);
  destroy(5, 21, 0x40, true);
  damage.repair_services(cells);
  CHECK(cells[0].state == 0xc7);
  CHECK(cells[1].state == 0x65);
  CHECK(cells[2].state == 0x65);
  CHECK(cells[3].state == 0x20);
  CHECK(cells[4].state == 0);
  CHECK(cells[5].state == 0x40);
  CHECK_FALSE(damage.headquarters_lost());
  cells[0].state |= game::delphi_damage_bit;
  damage.repair_services(cells);
  CHECK(cells[0].state == 0xe7);                                               // old repair tracking cannot undo a later authored event
}

TEST_CASE("Hemmersan destruction fails only when the player newly destroys its entrance", "[game][collateral]") {
  game::delphi_collateral_damage damage;
  unsigned int constexpr home{game::delphi_collateral_damage::headquarters_cell};
  damage.record_player_destruction(home, {
    .type{17},
    .state{0x20},
  });
  CHECK_FALSE(damage.headquarters_lost());                                    // mission 115's authored ruins are not player sabotage
  damage.record_player_destruction(home, {
    .type{86},
  });
  CHECK_FALSE(damage.headquarters_lost());
  damage.record_player_destruction(home, {
    .type{17},
    .state{0x80},
  });
  CHECK(damage.headquarters_lost());
  game::city_map cells{};
  cells[home].state = 0xa0;
  damage.repair_services(cells);
  CHECK(cells[home].state == 0xa0);
}

TEST_CASE("A player-damaged destination hangar is repaired on capture", "[game][hangar][collateral]") {
  game::city_map cells{};
  game::delphi_collateral_damage damage;
  auto constexpr index{game::city_cell_index(50, 48)};
  cells[index].type = 17;
  damage.record_player_destruction(index, cells[index]);
  cells[index].state = 0x20;
  game::hangar_state hangar{
    .return_site{0x3064},
  };
  game::player_flight player;
  player.pose().position = {
    .column{50 * 256 + 128},
    .row{48 * 256 + 152 - 700},
    .height{256},
  };
  player.pose().angles.heading = 0x8000;
  REQUIRE(game::begin_hangar_return(player, cells, hangar, true));
  damage.repair_return_hangar(cells, hangar.return_site);
  CHECK(cells[index].state == 0x80);                                          // landing opened the gate; repair removes only damage
  CHECK(hangar.returning == game::hangar_return_phase::approaching);
}

TEST_CASE("Script dependencies distinguish required destruction from mere permission", "[game][scenario][collateral]") {
  auto const program{bytes({
    0x38, 0, 2, 0x38,                                                       // two altitude toggles with asymmetric operands
    0x1d, 3, 2, 0x20,                                                       // wait for (1,2) state damage
    0x1d, 5, 2, 0xdf,                                                       // wait for (2,2) state undamaged
    0x1d, 7, 2, 0x40,                                                       // permission-only wait
    0x1d, 8, 2, 0x20,                                                       // type test, not damage
    0x34, 5, 2, 255, 6, 2, 255,                                            // marked optional (5,2), required unmarked (6,2)
    0x34, 7, 2, 254, 8, 2, 255,                                            // both lists required
    0x35, 9, 2, 255,                                                        // spawning-site list
    0x25, 0xfe,                                                             // branch to itself, never read the dead bytes below
    0x1d, 21, 2, 0x20, 0x23,
  })};
  resources::scenario_record record{
    .shared{
      .offset{100},
      .size{program.size()},
    },
    .configuration{0},
    .cell_lists{},
    .groups{},
    .player_program{100},
  };
  record.cell_lists[1].cells.push_back({
    .column{11},
    .row{2},
  });
  record.cell_lists[2].cells.push_back({
    .column{12},
    .row{2},
  });
  auto targets{game::scripted_destruction_targets(record, program)};
  REQUIRE(targets.count() == 6);
  for(unsigned int const column : {1, 2, 6, 7, 8, 12}) CHECK(targets[game::city_cell_index(column, 2)]);
  for(unsigned int const column : {3, 4, 5, 9, 10, 11}) CHECK_FALSE(targets[game::city_cell_index(column, 2)]);
  record.cell_lists[1].terminator = 254;
  record.objective_cell_list = 1;
  targets = game::scripted_destruction_targets(record, program);
  CHECK(targets.count() == 7);
  CHECK(targets[game::city_cell_index(11, 2)]);
}

TEST_CASE("Repairs follow actor programs and preserve beacon power and explicit mission damage", "[game][scenario][collateral]") {
  auto const program{bytes({0x23, 0x1d, 3, 2, 0x20, 0x1d, 5, 2, 0x20, 0x23})};
  resources::scenario_record record{
    .configuration{0},
    .cell_lists{},
    .groups{},
    .player_program{0},
  };
  record.groups[1].objects.push_back({
    .form{resources::placement_form::moving},
    .script_or_target{},
    .program_offset{1},
  });
  game::city_map cells{};
  cells[game::city_cell_index(1, 2)] = {
    .type{1},
    .state{255},
  };
  cells[game::city_cell_index(2, 2)] = {
    .type{48},
    .state{0xe3},
  };
  record.cell_lists[0].cells.push_back({
    .column{2},
    .row{2},
  });
  game::repair_scenario_buildings(cells, record, program);
  CHECK(cells[game::city_cell_index(1, 2)].state == 255);
  CHECK(cells[game::city_cell_index(2, 2)].state == 0xc3);
  game::apply_scenario_cells(cells, record);
  CHECK(cells[game::city_cell_index(2, 2)].state == 0xe3);
  record.configuration = 2;                                                  // Halon remains unchanged
  game::repair_scenario_buildings(cells, record, program);
  CHECK(cells[game::city_cell_index(2, 2)].state == 0xe3);
  record.groups[1].objects[0].form = resources::placement_form::compact_special;
  CHECK(game::scripted_destruction_targets(record, program).none());
}

TEST_CASE("Retail Delphi repair dependencies match the independently decoded scripts", "[game][scenario][collateral][integration]") {
  if(std::string_view{DARKER_TEST_RESOURCE_DIR}.empty()) SKIP("Requires extracted retail scenarios");
  // counts include active objectives, runtime objective replacements and cell-damage waits, deduplicated by location
  std::array<unsigned int, 124> expected{};
  expected[26] = 18;
  expected[31] = 18;
  expected[42] = 14;
  expected[49] = 5;
  expected[50] = 3;
  expected[53] = 7;
  expected[57] = 5;
  expected[59] = 14;
  expected[60] = 1;
  expected[64] = 8;
  expected[68] = 8;
  expected[72] = 9;
  expected[73] = 5;
  expected[74] = 19;
  expected[75] = 16;
  expected[76] = 43;
  expected[79] = 4;
  expected[80] = 6;
  expected[81] = 4;
  expected[82] = 12;
  expected[86] = 9;
  expected[93] = 5;
  expected[95] = 11;
  expected[120] = 2;
  for(unsigned int archive_slot{0}; archive_slot < 16; ++archive_slot) {
    resources::scenario_resource const scenario{resources::read_binary_file(
      std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / std::format("04_{:03}.bin", archive_slot), 1'000'000)};
    unsigned int index{0};
    for(auto const &record : scenario.records()) {
      unsigned int const stage{archive_slot * 8 + index++ + 1};
      if((record.configuration & 15) > 1) continue;
      CAPTURE(stage);
      auto const targets{game::scripted_destruction_targets(record, scenario.bytes(record.shared))};
      CHECK(targets.count() == expected.at(stage - 1));
    }
  }
}

TEST_CASE("Actual Delphi projectile hits preserve targeting and track repairable services", "[game][weapons][collateral][integration]") {
  if(std::string_view{DARKER_TEST_RESOURCE_DIR}.empty()) SKIP("Requires extracted retail geometry");
  resources::geometry_bank const bank{resources::read_binary_file(
    std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / "00_030.bin", 1'000'000)};
  auto const type{GENERATE(uint8_t{1}, uint8_t{12}, uint8_t{17}, uint8_t{21}, uint8_t{48}, uint8_t{75})};
  auto const enemy{GENERATE(false, true)};
  CAPTURE(static_cast<unsigned int>(type), enemy);
  unsigned int const column{type == 17 ? 49u : 23u};
  unsigned int const row{type == 17 ? 113u : 23u};
  auto const index{game::city_cell_index(column, row)};
  game::city_map cells{};
  cells[index] = {
    .type{type},
    .state{static_cast<uint8_t>(type == 1 ? 255 : 0)},
  };
  auto const original_state{cells[index].state};
  std::optional<darker::maths::world_position> impact;
  for(auto const &box : game::city_collision_boxes(bank, type, original_state, game::delphi_damage_bit,
    static_cast<uint8_t>(column), static_cast<uint8_t>(row), 0)) {
    // sample inside the exposed part of each box; hangar entrances overlap the protected floor and roof boxes
    for(unsigned int const x : {2, 4, 6, 7}) {
      for(unsigned int const y : {2, 4, 6, 7}) {
        for(unsigned int const z : {2, 4, 6, 7}) {
          auto const point{[&](unsigned int const axis, unsigned int const fraction) {
            return static_cast<uint16_t>(box.bounds.min[axis] + (box.bounds.max[axis] - box.bounds.min[axis]) * fraction / 8);
          }};
          darker::maths::world_position const start{
            .column{point(0, x)},
            .row{point(1, y)},
            .height{static_cast<uint16_t>(point(2, z) * 8)},
          };
          auto end{start};
          auto const contact{game::sweep_city(bank, cells, game::delphi_damage_bit, start, end, 2, 10)};
          if(contact.contact == game::city_contact::building && contact.category == game::collision_category::ground_target) impact = start;
        }
      }
    }
    if(impact) break;
  }
  REQUIRE(impact.has_value());
  game::mission_combat combat{{}};
  game::player_flight player;
  player.pose().position = {
    .column{10000},
    .row{10000},
    .height{20000},
  };
  game::launch_emitter const emitter{
    .position{*impact},
  };
  auto *shot{(enemy ? combat.hostile_projectiles : combat.projectiles).launch({
    .definition{game::original_object_definitions[enemy ? 14 : 5]},
    .emitter{emitter},
    .lifetime{1000},
  })};
  REQUIRE(shot);
  shot->placement.position = *impact;                                       // test a known surface contact independently of muzzle offsets
  shot->parameters.update_entry = game::object_update::direct_projectile;
  combat.advance(player, cells, bank, {
    .elapsed_ticks{8},
    .frame_step{0},
  });
  CHECK(shot->flags & 8);
  bool const destructible{type != 1 && type != 75};
  CHECK(cells[index].state == (destructible ? game::delphi_damage_bit : original_state));
  CHECK(combat.collateral.headquarters_lost() == (type == 17 && !enemy));
  CHECK(combat.target.token == game::weapon_target::no_target);
  combat.collateral.repair_services(cells);
  bool const repaired{!enemy && (type == 12 || type == 21)};
  CHECK(cells[index].state == (destructible && !repaired ? game::delphi_damage_bit : original_state));
}

TEST_CASE("Delphi mission repairs survive native save packing and leave optional targets destroyed", "[game][scenario][collateral][integration]") {
  if(std::string_view{DARKER_TEST_INSTALL_DIR}.empty()) SKIP("Requires retail packs");
  resources::archive_set const archives{DARKER_TEST_INSTALL_DIR};
  resources::geometry_bank const bank{archives.load({0, 30})};
  resources::campaign_resources campaign{archives};
  auto cells{game::make_city_map(archives.load({0, 68}), true)};
  auto const &scenario{campaign.scenario(80)};
  auto const &mission{scenario.records()[resources::select_campaign_stage(80).record]};
  auto const dependencies{game::scripted_destruction_targets(mission, scenario.bytes(mission.shared))};
  for(unsigned int i{0}; i < cells.size(); ++i) {
    if(dependencies[i]) cells[i].state |= game::delphi_damage_bit;
  }
  auto constexpr optional_tank{game::city_cell_index(24, 80)};
  auto constexpr radar{game::city_cell_index(13, 49)};
  REQUIRE(cells[optional_tank].type == 48);
  REQUIRE(cells[radar].type == 12);
  REQUIRE_FALSE(dependencies[optional_tank]);
  cells[optional_tank].state |= game::delphi_damage_bit;
  game::delphi_collateral_damage damage;
  damage.record_player_destruction(radar, cells[radar]);
  cells[radar].state |= game::delphi_damage_bit;
  damage.repair_services(cells);
  resources::pilot_record pilot;
  game::pack_city_state(cells, bank.city_types(), pilot.delphi);
  cells = game::make_city_map(archives.load({0, 68}), true);
  game::restore_city_state(cells, bank.city_types(), pilot.delphi, 80);
  game::repair_scenario_buildings(cells, mission, scenario.bytes(mission.shared));
  game::apply_scenario_cells(cells, mission);
  for(unsigned int i{0}; i < cells.size(); ++i) {
    if(dependencies[i]) CHECK_FALSE(cells[i].state & game::delphi_damage_bit);
  }
  CHECK(cells[optional_tank].state & game::delphi_damage_bit);
  CHECK_FALSE(cells[radar].state & game::delphi_damage_bit);
  auto const &final_scenario{campaign.scenario(115)};
  auto const &final_mission{final_scenario.records()[resources::select_campaign_stage(115).record]};
  game::repair_scenario_buildings(cells, final_mission, final_scenario.bytes(final_mission.shared));
  game::apply_scenario_cells(cells, final_mission);
  CHECK(cells[game::delphi_collateral_damage::headquarters_cell].state & game::delphi_damage_bit);
}

TEST_CASE("Headquarters failure can be dismissed or retried without altering the saved campaign", "[game][presentation][collateral][integration]") {
  if(std::string_view{DARKER_TEST_INSTALL_DIR}.empty()) SKIP("Requires retail packs");
  resources::archive_set const archives{DARKER_TEST_INSTALL_DIR};
  resources::font_resource const font{archives.load({0, 29})};
  resources::campaign_resources campaign{archives};
  auto const language{GENERATE(resources::scenario_language::english, resources::scenario_language::french, resources::scenario_language::german)};
  resources::save_file save;
  darker::presentation::front_end front{archives, font, campaign, save, true, language};
  front.start_level(80);
  auto const saved{resources::encode_save(save)};
  framework::render::cockpit_framebuffer frame;
  front.show_headquarters_loss();
  CHECK(front.active());
  CHECK_FALSE(front.save_requested);
  REQUIRE_NOTHROW(front.draw(frame));
  front.key(darker::presentation::front_key::back);
  front.draw(frame);
  CHECK(resources::encode_save(save) == saved);
  front.show_headquarters_loss();
  front.click(160, 164);
  for(unsigned int i{0}; front.active() && i < 256; ++i) {
    front.advance(4000);
    front.key(darker::presentation::front_key::accept);
  }
  CHECK_FALSE(front.active());
  CHECK_FALSE(front.save_requested);
  CHECK(resources::encode_save(save) == saved);
}
