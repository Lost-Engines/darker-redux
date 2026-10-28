#pragma once
#ifndef NDEBUG

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <boost/property_tree/ptree.hpp>
#include "game/flight_camera.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "render/framebuffer.h"

namespace darker::debug {

// Offline capture uses the ordinary mission loop. Only explicit events change the
// player; the camera never moves actors, creates effects or completes objectives.
class capture_session {
public:
  static unsigned int constexpr step_ticks{8};
  std::optional<game::object_pose> camera;
  bool cockpit{false};
  bool primary{false};
  bool secondary{false};
  double field_of_view{50.2};

  explicit capture_session(std::filesystem::path const &path);
  void before_frame(unsigned int tick, game::player_flight &player, game::mission_combat &combat,
    game::hangar_state &hangar, uint16_t available_weapons);
  bool capture(unsigned int tick, game::player_flight const &player, game::mission_combat const &combat,
    game::city_map const &cells, resources::geometry_bank const &bank, uint16_t available_weapons,
    int width, int height, std::function<std::span<framework::render::rgba_pixel const>()> const &render);
  bool finished() const noexcept;

private:
  std::filesystem::path directory;
  boost::property_tree::ptree plan;
  unsigned int next_event{0};
  unsigned int next_shot{0};
};

} // namespace darker::debug
#endif // NDEBUG
