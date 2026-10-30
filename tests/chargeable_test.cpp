#include <catch2/catch_test_macros.hpp>
#include "game/object_definitions.h"
#include "game/caero_weapons.h"
#include "game/projectile_update.h"
#include "game/projectile_steering.h"
#include "reference/chargeable_motion_samples.h"

TEST_CASE("Chargeable steering, spin and displacement match native updates", "[weapons]") {
  /// Exercise charge-dependent speed and spin alongside object tracking, self-targeting and word wrapping
  for(auto const &sample : darker::test_reference::chargeable_motion_samples) {
    CAPTURE(sample);
    darker::game::projectile shot;
    shot.parameters.definition = &darker::game::original_object_definitions[8];
    shot.parameters.angular_response = 480;
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
    darker::game::advance_chargeable_projectile(shot, sample[18] ? shot.placement : target,
      static_cast<uint16_t>(sample[16]), static_cast<uint16_t>(sample[17]));
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
    CHECK(shot.inherited_roll == sample[31]);
  }
}

TEST_CASE("An unlocked Chargeable release retains charge-dependent flight and damage", "[game][weapons][chargeable]") {
  namespace game = darker::game;
  game::projectile_pool pool;
  game::caero_energy_state energy{
    .reserve{65535},
  };
  uint16_t charge{0};
  game::launch_emitter const emitter{
    .position{10000, 10000, 4000},
    .speed{496},
  };
  auto const fire{[&](bool const pressed, bool const held, game::clock_tick const clock) {
    return game::fire_caero_weapon(pool, energy, charge, {
      .emitter{emitter},
      .selection{9},
      .pressed{pressed},
      .held{held},
      .clock{clock},
      .frame_step{8},
    });
  }};
  CHECK_FALSE(fire(true, true, 64000).shot);
  REQUIRE(charge > 0);
  for(unsigned int elapsed{8}; charge < 65535; elapsed += 8) {
    energy.reserve = 65535;
    CHECK_FALSE(fire(false, true, static_cast<game::clock_tick>(64000 + elapsed)).shot);
  }
  auto const before{energy.reserve};
  auto const result{fire(false, false, 65520)};
  REQUIRE(result.shot);
  auto &shot{*result.shot};
  CHECK(energy.reserve == before);
  CHECK(charge == 0);
  CHECK(shot.target_token == shot.native_id);
  CHECK(shot.parameters.update_entry == game::object_update::chargeable);
  CHECK(shot.deadline == static_cast<game::clock_tick>(65520 + 4095));
  CHECK(game::chargeable_impact_strength(shot.deadline, 65520) == 127);
  auto const start{shot.placement.position};
  for(unsigned int elapsed{8}; elapsed <= 1024; elapsed += 8) {
    CHECK(game::update_projectile(shot, static_cast<game::clock_tick>(65520 + elapsed), 8, &shot.placement)
      == game::projectile_update_result::advanced);
  }
  CHECK(shot.placement.position != start);
  CHECK(shot.placement.angles.roll != 0);
  CHECK(game::chargeable_impact_strength(shot.deadline, static_cast<game::clock_tick>(65520 + 1024)) < 127);
  CHECK_FALSE(fire(false, false, 1024).shot);
}
