#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "game/caero_weapons.h"
#include "game/dual_launch.h"
#include "game/object_definitions.h"
#include "game/projectile_motion.h"
#include "game/projectile_steering.h"
#include "reference/dual_launch_samples.h"
#include "reference/dual_motion_samples.h"

TEST_CASE("Dual Launch separation matches the native paired projectile metric", "[game][weapons]") {
  /// Preserve wrapping and the original asymmetric absolute-value arithmetic
  for(auto const &s : darker::test_reference::dual_separation_samples) {
    CAPTURE(s);
    darker::game::object_pose const source{
      .position{
        .column{static_cast<uint16_t>(s[0])},
        .row{static_cast<uint16_t>(s[1])},
        .height{static_cast<uint16_t>(s[2])}
      }
    };
    darker::game::object_pose const target{
      .position{
        .column{static_cast<uint16_t>(s[3])},
        .row{static_cast<uint16_t>(s[4])},
        .height{static_cast<uint16_t>(s[5])}
      }
    };
    CHECK(darker::game::dual_launch_separation(source, target) == s[6]);
  }
}

TEST_CASE("Dual Launch blast matches native category bounds and impact strength", "[game][weapons]") {
  /// Compare the complete 6DB5 admission and CD13 strength path for aircraft and smaller ground/static passes
  for(auto const &s : darker::test_reference::dual_impact_samples) {
    CAPTURE(s);
    darker::game::object_pose const source{
      .position{
        .column{static_cast<uint16_t>(s[0])},
        .row{static_cast<uint16_t>(s[1])},
        .height{static_cast<uint16_t>(s[2])}
      }
    };
    darker::game::object_pose const target{
      .position{
        .column{static_cast<uint16_t>(s[3])},
        .row{static_cast<uint16_t>(s[4])},
        .height{static_cast<uint16_t>(s[5])}
      }
    };
    auto const strength{darker::game::dual_launch_impact(source, target, s[6] != 0)};
    CHECK(strength.has_value() == (s[7] != 0));
    if(strength) CHECK(*strength == s[8]);
  }
}

TEST_CASE("Dual Launch firing matches native stage changes and capsule targeting", "[game][weapons]") {
  /// Retain native firing guards while supplying the explicitly remembered capsule as the target
  for(auto const &s : darker::test_reference::dual_firing_samples) {
    CAPTURE(s);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{
      .position{
        .column{1000},
        .row{2000},
        .height{3000}
      },
      .definition_strength{40}
    };
    darker::game::projectile *tail{nullptr};
    if(s[5] >= 0) tail = pool.launch({
      .definition{darker::game::original_object_definitions[s[5]]},
      .emitter{emitter}
    });
    if(!s[3]) {
      while(pool.objects().free_head()) pool.launch({
        .definition{darker::game::original_object_definitions[0]},
        .emitter{emitter}
      });
    }
    darker::game::caero_energy_state energy{
      .reserve{static_cast<uint16_t>(s[1])}
    };
    uint16_t charge{0};
    auto const result{darker::game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{static_cast<uint8_t>(s[0])},
      .player_flags{static_cast<uint8_t>(s[2])},
      .pressed{s[4] != 0},
      .target{s[0] == 7 && tail ? tail->native_id : uint16_t{0xec00}},
      .released{s[12] != 0}
    })};
    CHECK(energy.reserve == s[6]);
    CHECK(result.ready == (s[7] != 0));
    CHECK((result.next_selection ? result.next_selection : s[0]) == s[8]);
    CHECK((result.shot != nullptr) == (s[9] != 0));
    if(result.shot) CHECK(result.shot->target_token == (s[10] == 0xd000 ? tail->native_id : s[10]));
  }
}

TEST_CASE("Dual Launch steering and displacement match native updates", "[weapons]") {
  /// Exercise separation-dependent speed alongside object tracking, self-targeting and word wrapping
  for(auto const &sample : darker::test_reference::dual_motion_samples) {
    CAPTURE(sample);
    darker::game::projectile shot;
    shot.parameters.definition = &darker::game::original_object_definitions[6];
    shot.parameters.angular_response = 512;
    darker::game::object_pose target;
    shot.placement.angles.heading = static_cast<uint16_t>(sample[6 + 0]);
    shot.placement.angles.pitch = static_cast<uint16_t>(sample[6 + 1]);
    shot.placement.angles.roll = static_cast<uint16_t>(sample[6 + 2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.placement.position[axis] = static_cast<uint16_t>(sample[axis]);
      shot.placement.fractions[axis] = static_cast<uint8_t>(sample[3 + axis]);
      target.position[axis] = static_cast<uint16_t>(sample[13 + axis]);
    }
    shot.placement.speed = static_cast<uint16_t>(sample[9]);
    shot.inherited_roll = static_cast<uint16_t>(sample[10]);
    shot.angular_motion.pitch = static_cast<uint16_t>(sample[11]);
    shot.angular_motion.turn = static_cast<uint16_t>(sample[12]);
    uint16_t pitch{614};
    if(sample[18]) darker::game::advance_direct_projectile(shot.placement, *shot.parameters.definition, static_cast<uint16_t>(sample[17]));
    else {
      auto const separation{darker::game::dual_launch_separation(shot.placement, target)};
      REQUIRE(separation >= 20);
      darker::game::advance_dual_projectile(shot, target, separation, static_cast<uint16_t>(sample[17]));
      pitch = static_cast<uint16_t>((0x80c - std::min<uint16_t>(separation, 0xcd)) >> 2);
    }
    CHECK(shot.placement.angles.heading == sample[27 + 0]);
    CHECK(shot.placement.angles.pitch == sample[27 + 1]);
    CHECK(shot.placement.angles.roll == sample[27 + 2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      CHECK(shot.placement.position[axis] == sample[19 + axis]);
      CHECK(shot.placement.fractions[axis] == sample[22 + axis]);
    }
    CHECK(shot.angular_motion.pitch == sample[25]);
    CHECK(shot.angular_motion.turn == sample[26]);
    CHECK(shot.placement.speed == sample[30]);
    CHECK(pitch == sample[31]);
  }
}

TEST_CASE("Dual Launch follows its remembered capsule despite older or newer projectiles", "[game][weapons][dual-launch]") {
  namespace game = darker::game;
  game::projectile_pool pool;
  game::launch_emitter const emitter{
    .position{1000, 2000, 3000},
  };
  auto const launch{[&](unsigned int const slot) {
    return pool.launch({
      .definition{game::original_object_definitions[slot]},
      .emitter{emitter},
    });
  }};
  auto const *old_pinner{launch(0)};
  auto *capsule{launch(2)};
  auto const *new_pinner{launch(1)};
  auto const *other_capsule{launch(2)};
  REQUIRE(pool.objects().tail() == old_pinner);
  REQUIRE(pool.objects().head() == other_capsule);
  game::caero_energy_state energy{
    .reserve{65535},
  };
  uint16_t charge{0};
  auto const fire{[&] {
    return game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{7},
      .target{capsule->native_id},
      .released{true},
    });
  }};
  SECTION("Unrelated shots do not change the remembered pairing") {
    auto const result{fire()};
    REQUIRE(result.shot);
    CHECK(result.shot->target_token == capsule->native_id);
    CHECK(result.shot->target_token != new_pinner->native_id);
    CHECK(result.shot->target_token != other_capsule->native_id);
    CHECK(result.next_selection == 3);
  }
  SECTION("An impacted capsule cannot be used again") {
    capsule->flags |= 8;
    auto const result{fire()};
    CHECK_FALSE(result.shot);
    CHECK(result.next_selection == 3);
    CHECK(energy.reserve == 65535);
  }
  SECTION("A retired capsule cannot be confused with an unrelated reused slot") {
    auto const id{capsule->native_id};
    pool.recycle(*capsule);
    REQUIRE(launch(0)->native_id == id);
    auto const result{fire()};
    CHECK_FALSE(result.shot);
    CHECK(result.next_selection == 3);
    CHECK(energy.reserve == 65535);
  }
}
