#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include "audio/flight_sounds.h"
#include "game/caero_weapons.h"
#include "game/mission_combat.h"
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "game/weapon_selection.h"
#include "maths/direction.h"
#include "resources/archive_set.h"
#include "resources/world_profile.h"
#include "reference/caero_impact_samples.h"
#include "reference/caero_weapon_samples.h"
#include "reference/chargeable_samples.h"
#include "reference/diffuser_samples.h"

TEST_CASE("Caero Pinner and Brent firing matches original guards, targets and reserve accounting", "[game][weapons]") {
  /// Exercise native cost boundaries, blocked player states, trigger edges and an exhausted projectile pool
  for(auto const &sample : darker::test_reference::caero_weapon_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{
      .position{
        .column{1000},
        .row{2000},
        .height{3000}
      },
      .definition_strength{40}
    };
    if(!sample[3]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({
        .definition{darker::game::original_object_definitions[0]},
        .emitter{emitter}
      }));
    }
    darker::game::caero_energy_state energy{
      .reserve{static_cast<uint16_t>(sample[0])}
    };
    uint16_t charge{0};
    auto const result{darker::game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{static_cast<uint8_t>(sample[8])},
      .player_flags{static_cast<uint8_t>(sample[1])},
      .pressed{sample[2] != 0},
      .model{0x400},
      .clock{65000},
      .target{static_cast<uint16_t>(sample[9])},
      .underground{sample[10] == 2}
    })};
    CHECK(energy.reserve == sample[4]);
    CHECK(result.ready == (sample[5] != 0));
    CHECK((result.shot != nullptr) == (sample[6] != 0));
    if(result.shot) {
      CHECK(result.shot->deadline == static_cast<uint16_t>(65000 + sample[7]));
      CHECK(result.shot->parameters.model_token == 0x400);
      CHECK(result.shot->target_token == sample[9]);
      CHECK(result.shot->parameters.definition == &darker::game::original_object_definitions[static_cast<size_t>(sample[8] - 1)]);
    }
  }
}

TEST_CASE("Chargeable charging and release match the native firing handler", "[game][weapons]") {
  /// Compare edge, held and released inputs with native reserve, charge, readiness and lifetime results
  for(auto const &sample : darker::test_reference::chargeable_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{
      .position{
        .column{1000},
        .row{2000},
        .height{3000}
      },
      .definition_strength{40}
    };
    if(!sample[3]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({
        .definition{darker::game::original_object_definitions[0]},
        .emitter{emitter}
      }));
    }
    darker::game::caero_energy_state energy{
      .reserve{static_cast<uint16_t>(sample[0])}
    };
    auto charge{static_cast<uint16_t>(sample[1])};
    auto const result{darker::game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{9},
      .player_flags{static_cast<uint8_t>(sample[2])},
      .pressed{sample[4] != 0},
      .held{sample[5] != 0},
      .model{0x400},
      .clock{65000},
      .frame_step{static_cast<uint16_t>(sample[8])},
      .target{static_cast<uint16_t>(sample[6])},
      .underground{sample[7] == 2}
    })};
    CHECK(energy.reserve == sample[9]);
    CHECK(charge == sample[10]);
    CHECK(result.ready == (sample[11] != 0));
    CHECK((result.shot != nullptr) == (sample[12] != 0));
    if(result.shot) {
      CHECK(result.shot->deadline == static_cast<uint16_t>(65000 + sample[13]));
      CHECK(result.shot->target_token == sample[6]);
      CHECK(result.shot->parameters.definition == &darker::game::original_object_definitions[8]);
    }
  }
}

TEST_CASE("Chargeable impact strength matches native remaining-lifetime dispatch", "[game][weapons]") {
  /// Preserve the signed high-byte cutoff and wrapped strength at every timer page boundary
  for(auto const &sample : darker::test_reference::chargeable_impact_samples) {
    CAPTURE(sample);
    auto const strength{darker::game::chargeable_impact_strength(static_cast<uint16_t>(sample[0]), static_cast<uint16_t>(sample[1]))};
    CHECK(strength.has_value() == (sample[2] != 0));
    if(strength) CHECK(*strength == sample[3]);
  }
}

TEST_CASE("Chargeable sound follows the native charge and timer modulation", "[audio][weapons]") {
  /// Check the rising continuous tone and its silent zero-charge state against callback 370F
  for(auto const &sample : darker::test_reference::chargeable_tone_samples) {
    auto const pitch{darker::audio::chargeable_sound_pitch(static_cast<uint16_t>(sample[0]), static_cast<uint16_t>(sample[1]))};
    CHECK(pitch.has_value() == (sample[2] != 0));
    if(pitch) CHECK(*pitch == sample[3]);
  }
}

TEST_CASE("Pinner Direct strength follows native world setup", "[game][weapons]") {
  /// Compare Delphi and underground definition patches executed by BCA4
  for(auto const &sample : darker::test_reference::pinner_world_strength_samples) {
    CHECK(darker::game::pinner_direct_strength(sample[0] == 2) == sample[1]);
  }
}

TEST_CASE("Diffuser firing preserves its stage while retaining native launch guards and energy costs", "[game][weapons][diffuser]") {
  /// Neither successful launches nor rejected attempts change the selected stage in Redux
  for(auto const &sample : darker::test_reference::diffuser_firing_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{
      .position{
        .column{1000},
        .row{2000},
        .height{3000}
      },
      .definition_strength{40}
    };
    if(!sample[4]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({
        .definition{darker::game::original_object_definitions[0]},
        .emitter{emitter}
      }));
    }
    darker::game::caero_energy_state energy{
      .reserve{static_cast<uint16_t>(sample[2])}
    };
    uint16_t charge{0};
    auto const result{darker::game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{static_cast<uint8_t>(sample[0])},
      .player_flags{static_cast<uint8_t>(sample[5])},
      .pressed{sample[3] != 0},
      .model{0x400},
      .clock{65000},
      .target{static_cast<uint16_t>(sample[6])},
      .underground{sample[1] == 2}
    })};
    CHECK(energy.reserve == sample[7]);
    CHECK(result.ready == (sample[8] != 0));
    CHECK(result.next_selection == 0);
    CHECK((result.shot != nullptr) == (sample[10] != 0));
  }
}

TEST_CASE("Diffuser warehouse impacts retain native detonation timing and protected surfaces", "[game][weapons][diffuser]") {
  /// Ordinary and fragile surfaces now accept gas; compare native fixtures outside that extension and repeat-gas changes
  for(auto const &sample : darker::test_reference::diffuser_impact_samples) {
    CAPTURE(sample);
    if(sample[0] && sample[3] == sample[4]) continue;
    if(sample[1] == std::to_underlying(darker::game::collision_category::ground_target)
      || sample[1] == std::to_underlying(darker::game::collision_category::fragile)) continue;
    darker::game::diffuser_state state{
      .cell{static_cast<uint16_t>(sample[4])},
      .deadline{static_cast<uint16_t>(sample[6])},
      .sound_deadline{static_cast<uint16_t>(sample[7])},
      .active{true},
    };
    auto const result{state.hit(sample[0] != 0, static_cast<darker::game::collision_category>(sample[1]), static_cast<uint8_t>(sample[2]), true, static_cast<uint16_t>(sample[3]), static_cast<uint16_t>(sample[5]))};
    auto const expected{sample[8] == 0x67ea   ? darker::game::diffuser_impact::gas
      : sample[8] == 0x67bf ? darker::game::diffuser_impact::destroyed : darker::game::diffuser_impact::rejected};
    CHECK(result == expected);
    CHECK(state.cell == sample[9]);
    CHECK(state.deadline == sample[10]);
    CHECK(state.sound_deadline == sample[11]);
  }
}

TEST_CASE("Caero Weapon impact strength matches native beacon sampling", "[game][weapons]") {
  /// Include off towers, intermediate strengths, altitude cutoffs and the victim-token fraction quirk
  for(auto const &sample : darker::test_reference::caero_impact_samples) {
    CAPTURE(sample);
    darker::game::city_map cells{};
    cells[36 * 128 + 36] = {static_cast<uint8_t>(sample[3]), static_cast<uint8_t>(sample[4])};
    CHECK(darker::game::caero_weapon_strength(cells, {static_cast<uint16_t>(sample[0]), static_cast<uint16_t>(sample[1]), static_cast<uint16_t>(sample[2])}, static_cast<uint16_t>(sample[5])) == sample[6]);
  }
}

TEST_CASE("Diffuser accepts permitted destructible surfaces and keeps each building's protection", "[game][weapons][diffuser]") {
  /// Gas and detonators share the same eligibility checks, even if a target is destroyed between shots
  using darker::game::collision_category;
  using darker::game::diffuser_impact;
  using darker::game::diffuser_state;
  auto const category{GENERATE(collision_category::ground_target, collision_category::protected_surface,
    collision_category::fragile, collision_category::diffuser, static_cast<collision_category>(4))};
  bool const permitted{GENERATE(false, true)};
  bool const linked{GENERATE(false, true)};
  CAPTURE(category, permitted, linked);
  uint8_t const flags{permitted ? darker::game::city_cell::permitted_target : uint8_t{0}};
  uint16_t constexpr target{0x1717};
  darker::game::clock_tick constexpr gas_time{1000};
  auto constexpr trigger_time{static_cast<darker::game::clock_tick>(gas_time + diffuser_state::gas_lifetime_ticks - diffuser_state::trigger_window_ticks)};
  bool const accepted{permitted && linked && (category == collision_category::ground_target
    || category == collision_category::fragile || category == collision_category::diffuser)};
  diffuser_state state;
  auto const result{state.hit(true, category, flags, linked, target, gas_time)};
  if(!accepted) {
    CHECK(result == diffuser_impact::rejected);
    CHECK_FALSE(state.active);
    CHECK(state.cell == 0);
    CHECK(state.deadline == 0);
    return;
  }
  REQUIRE(result == diffuser_impact::gas);
  REQUIRE(state.active);
  auto const deadline{state.deadline};
  CHECK(state.hit(false, category, flags, linked, target, trigger_time - 1) == diffuser_impact::rejected);
  CHECK(state.hit(true, category, flags, linked, target, trigger_time) == diffuser_impact::already_gassed);
  CHECK(state.deadline == deadline);
  CHECK(state.hit(false, collision_category::protected_surface, flags, linked, target, trigger_time) == diffuser_impact::rejected);
  CHECK(state.hit(false, category, flags, false, target, trigger_time) == diffuser_impact::rejected);
  CHECK(state.hit(false, category, 0, linked, target, trigger_time) == diffuser_impact::rejected);
  CHECK(state.hit(false, category, flags, linked, target + 1, trigger_time) == diffuser_impact::rejected);
  CHECK(state.active);
  CHECK(state.deadline == deadline);
  CHECK(state.hit(false, category, flags, linked, target, trigger_time) == diffuser_impact::destroyed);
  CHECK_FALSE(state.active);
}

TEST_CASE("Diffuser gas survives retry hits and expires permanently across clock wrapping", "[game][weapons][diffuser]") {
  /// Check the original arming window, duplicate hits, manual retargeting and absence of phantom gas
  using darker::game::clock_tick;
  using darker::game::collision_category;
  using darker::game::diffuser_impact;
  using darker::game::diffuser_state;
  auto constexpr permitted{darker::game::city_cell::permitted_target};
  uint16_t constexpr roof{0x1717};
  for(unsigned int const start : {0, 1024, 60000, 65000}) {
    CAPTURE(start);
    diffuser_state state;
    CHECK(state.hit(false, collision_category::diffuser, permitted, true, 0, static_cast<clock_tick>(start)) == diffuser_impact::rejected);
    REQUIRE(state.hit(true, collision_category::diffuser, permitted, true, roof, static_cast<clock_tick>(start)) == diffuser_impact::gas);
    auto const deadline{state.deadline};
    auto const arm_time{static_cast<clock_tick>(deadline - diffuser_state::trigger_window_ticks)};
    REQUIRE(state.active);
    CHECK(state.hit(false, collision_category::diffuser, permitted, true, roof, static_cast<clock_tick>(arm_time - 1)) == diffuser_impact::rejected);
    CHECK(state.active);
    CHECK(state.hit(true, collision_category::diffuser, permitted, true, roof, arm_time) == diffuser_impact::already_gassed);
    CHECK(state.deadline == deadline);
    CHECK(state.hit(false, collision_category::diffuser, permitted, true, roof + 1, arm_time) == diffuser_impact::rejected);
    CHECK(state.active);

    SECTION("Successful detonation closes the sequence at either end of the valid window") {
      for(auto const clock : {arm_time, static_cast<clock_tick>(deadline - 1)}) {
        auto attempt{state};
        CHECK(attempt.hit(false, collision_category::diffuser, permitted, true, roof, clock) == diffuser_impact::destroyed);
        CHECK_FALSE(attempt.active);
        CHECK(attempt.sound_deadline == clock);
        CHECK(attempt.hit(false, collision_category::diffuser, permitted, true, roof, clock) == diffuser_impact::rejected);
      }
    }
    SECTION("Timeout permits new gas but cannot revive the old target on a later clock cycle") {
      state.advance(static_cast<clock_tick>(deadline - 1));
      CHECK(state.active);
      state.advance(deadline);
      CHECK_FALSE(state.active);
      CHECK(state.hit(false, collision_category::diffuser, permitted, true, roof, deadline) == diffuser_impact::rejected);
      state.advance(arm_time);
      CHECK_FALSE(state.active);
      CHECK(state.hit(false, collision_category::diffuser, permitted, true, roof, arm_time) == diffuser_impact::rejected);
      CHECK(state.hit(true, collision_category::diffuser, permitted, true, roof, arm_time) == diffuser_impact::gas);
      CHECK(state.deadline == static_cast<clock_tick>(arm_time + diffuser_state::gas_lifetime_ticks));
    }
    SECTION("Gas on another valid roof replaces the single shared target") {
      REQUIRE(state.hit(true, collision_category::diffuser, permitted, true, roof + 1, arm_time) == diffuser_impact::gas);
      CHECK(state.cell == roof + 1);
      CHECK(state.deadline == static_cast<clock_tick>(arm_time + diffuser_state::gas_lifetime_ticks));
    }
  }
}

TEST_CASE("Diffuser controls follow actual building impacts and preserve manual selection", "[game][weapons][diffuser][integration]") {
  /// Use retail tanks and warehouse geometry with real flight/collision paths and no enemy interference
  if(std::string_view{DARKER_TEST_RESOURCE_DIR}.empty()) SKIP("Requires extracted retail geometry");
  using darker::game::caero_weapon;
  using darker::game::clock_tick;
  darker::resources::geometry_bank const bank{darker::resources::read_binary_file(
    std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / "00_030.bin", 1'000'000)};
  auto const type{GENERATE(uint8_t{46}, uint8_t{48}, uint8_t{75}, uint8_t{76})};
  CAPTURE(static_cast<unsigned int>(type));
  darker::game::city_map cells{};
  auto &building{cells[darker::game::city_cell_index(23, 23)]};
  building = {
    .type{type},
    .state{darker::game::city_cell::permitted_target},
  };
  unsigned int constexpr rejected_impact_emitters{2};
  unsigned int constexpr gas_emitters{24};
  uint16_t constexpr target{0x1717};
  uint16_t constexpr missed_target{0x0a0a};
  cells[darker::game::packed_cell_reference{missed_target}.index()] = building;
  auto constexpr damage_mask{darker::resources::profile(darker::resources::world_kind::delphi).damage_mask};
  auto const aim{darker::game::resolve_map_guidance(target, cells, bank, damage_mask)};
  darker::maths::world_position const centre{
    .column{aim.position.column},
    .row{aim.position.row},
    .height{static_cast<uint16_t>(aim.height - aim.height_extent / 2)},
  };
  darker::game::player_flight player;
  auto &craft{std::get<darker::game::caero_flight_state>(player.craft)};
  craft.flying = true;
  player.pose().position = {
    .column{static_cast<uint16_t>(centre.column + 32)},
    .row{centre.row},
    .height{static_cast<uint16_t>(aim.height + 2048)},
  };
  player.pose().speed = 496;
  darker::game::mission_combat combat{{}};
  combat.secondary_weapon = std::to_underlying(caero_weapon::diffuser_gas);
  CHECK(combat.secondary_weapon == 5);
  CHECK(std::to_underlying(caero_weapon::diffuser_trigger) == 4);
  darker::game::campaign_clock clock{65000};
  auto const step{[&](bool const fire = false){
    clock += 8;
    combat.advance(player, cells, bank, {
      .elapsed_ticks{clock},
      .frame_step{8},
      .changes{static_cast<uint16_t>(clock ^ (clock - 8))},
    }, {
      .secondary_pressed{fire},
    });
  }};
  bool unlocked{false};
  auto const fire{[&](bool const hit_building)->darker::game::projectile* {
    craft.energy.reserve = 0xffff;
    combat.target.token = unlocked ? darker::game::weapon_target::no_target : hit_building ? target : missed_target;
    auto const direction{darker::maths::object_target_direction(player.pose().position, centre)};
    player.pose().angles = {
      .heading{direction.heading},
      .pitch{hit_building ? direction.pitch : uint16_t{0}},
      .roll{0},
    };
    step(true);
    REQUIRE(combat.player_fired);
    return combat.projectiles.objects().head();
  }};
  auto const impact{[&](darker::game::projectile const *const shot){
    for(unsigned int i{0}; i < 256 && !(shot->flags & 8); ++i) step();
    REQUIRE((shot->flags & 8));
  }};
  auto const wait_until{[&](clock_tick const destination){
    auto const ticks{static_cast<clock_tick>(destination - clock)};
    REQUIRE(ticks < 0x8000);
    auto const end{clock + ticks};
    while(clock < end) step();
  }};

  if(type == 75) {
    // this warehouse block has collision geometry and a target flag, but no destroyed model
    auto const initial_state{building.state};
    impact(fire(true));
    CHECK_FALSE(combat.diffuser.active);
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
    CHECK(building.state == initial_state);
    CHECK(combat.effects.emitters.size() == rejected_impact_emitters);
    return;
  }

  SECTION("An unmarked Delphi building accepts gas and detonation without becoming a marked target") {
    building.state = 0;
    unlocked = true;
    impact(fire(true));
    REQUIRE(combat.diffuser.active);
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
    CHECK(building.state == 0);
    CHECK(combat.effects.emitters.size() == gas_emitters);
    wait_until(static_cast<clock_tick>(combat.diffuser.deadline - darker::game::diffuser_state::trigger_window_ticks));
    impact(fire(true));
    CHECK(building.state == damage_mask);
    CHECK_FALSE(combat.diffuser.active);
  }
  SECTION("A missed gas shot never arms the detonator") {
    auto const *shot{fire(false)};
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
    auto const deadline{shot->deadline};
    wait_until(static_cast<clock_tick>(deadline + 256));
    CHECK_FALSE(combat.diffuser.active);
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
  }
  SECTION("Impact and timeout respect another weapon selected during flight") {
    auto const *shot{fire(true)};
    combat.secondary_weapon = std::to_underlying(caero_weapon::brent_ground);
    impact(shot);
    REQUIRE(combat.diffuser.active);
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::brent_ground));
    wait_until(combat.diffuser.deadline);
    CHECK_FALSE(combat.diffuser.active);
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::brent_ground));
  }
  SECTION("Successful gas arms retries until detonation or timeout") {
    auto const *shot{fire(true)};
    CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
    impact(shot);
    REQUIRE(combat.diffuser.active);
    REQUIRE(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
    auto const deadline{combat.diffuser.deadline};

    SECTION("Manual gas reselection never restarts the existing smoke") {
      REQUIRE(combat.effects.emitters.size() == gas_emitters);
      auto const first_smoke_start{combat.effects.emitters.front().start};
      combat.secondary_weapon = 5;                                              // same assignment as pressing the weapon selection key
      step();
      REQUIRE(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
      impact(fire(true));
      CHECK(combat.diffuser.deadline == deadline);
      CHECK(combat.effects.emitters.size() == gas_emitters);
      CHECK(combat.effects.emitters.front().start == first_smoke_start);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
    }
    SECTION("Early and missed detonators allow another attempt in the same smoke window") {
      impact(fire(true));
      CHECK_FALSE(building.state & damage_mask);
      CHECK(combat.diffuser.deadline == deadline);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
      wait_until(static_cast<clock_tick>(deadline - darker::game::diffuser_state::trigger_window_ticks));
      auto const *miss{fire(false)};
      wait_until(static_cast<clock_tick>(miss->deadline + 256));
      REQUIRE(combat.diffuser.active);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
      impact(fire(true));
      CHECK(building.state & damage_mask);
      CHECK_FALSE(combat.diffuser.active);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
    }
    SECTION("Expiry resets to gas without a firing attempt") {
      wait_until(deadline);
      CHECK_FALSE(combat.diffuser.active);
      CHECK_FALSE(building.state & damage_mask);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_gas));
      impact(fire(true));
      CHECK(combat.diffuser.active);
      CHECK(combat.diffuser.deadline != deadline);
      CHECK(combat.secondary_weapon == std::to_underlying(caero_weapon::diffuser_trigger));
    }
  }
}
