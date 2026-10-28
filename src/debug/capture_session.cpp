#include "debug/capture_session.h"
#ifndef NDEBUG
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include "game/player_flags.h"

namespace darker::debug {
namespace {

using boost::property_tree::ptree;

std::array<double, 3> triple(ptree const &tree) {
  if(tree.size() != 3) throw std::invalid_argument{"Capture vectors need three components"};
  std::array<double, 3> result{};
  unsigned int i{0};
  for(auto const &[key, value] : tree) {
    if(!key.empty()) throw std::invalid_argument{"Capture vector must be a JSON array"};
    result[i] = value.get_value<double>();
    if(!std::isfinite(result[i])) throw std::invalid_argument{"Capture coordinates must be finite"};
    ++i;
  }
  return result;
}

uint16_t angle(double degrees) {
  return static_cast<uint16_t>(std::lround(std::remainder(degrees, 360.0) * 65536.0 / 360.0));
}

maths::world_position position(std::array<double, 3> const &value) {
  // Planar coordinates are map cells; altitude is the native signed height unit.
  if(value[0] < 0 || value[0] >= 256 || value[1] < 0 || value[1] >= 256 || value[2] < -32768 || value[2] > 32767) {
    throw std::invalid_argument{"Capture position exceeds native coordinate range"};
  }
  return {
    .column{static_cast<uint16_t>(std::lround(value[0] * 256))},
    .row{static_cast<uint16_t>(std::lround(value[1] * 256))},
    .height{static_cast<uint16_t>(std::lround(value[2]))},
  };
}

maths::attitude_angles look_at(maths::world_position const &eye, maths::world_position const &target, double roll) {
  double const x{static_cast<double>(std::bit_cast<int16_t>(static_cast<uint16_t>(target.column - eye.column)))};
  double const z{static_cast<double>(std::bit_cast<int16_t>(static_cast<uint16_t>(target.row - eye.row)))};
  double const height{std::bit_cast<int16_t>(static_cast<uint16_t>(target.height - eye.height)) / 8.0};
  return {
    .heading{angle(std::atan2(-x, -z) * 180.0 / std::numbers::pi)},
    .pitch{angle(std::atan2(height, std::hypot(x, z)) * 180.0 / std::numbers::pi)},
    .roll{angle(roll)},
  };
}

maths::world_position focus(ptree const &spec, game::player_flight const &player, game::mission_combat const &combat) {
  if(spec.size() != 1) throw std::invalid_argument{"Capture focus needs exactly one subject"};
  if(auto const value{spec.get_child_optional("point")}) return position(triple(*value));
  if(auto const index{spec.get_optional<unsigned int>("actor")}) {
    auto const found{std::ranges::find(combat.actors, *index, &game::scenario_actor::index)};
    if(found == combat.actors.end() || (found->flags & 8)) throw std::invalid_argument{"Capture actor is not active"};
    return found->pose.position;
  }
  if(auto const pool{spec.get_optional<std::string>("projectile")}) {
    if(*pool != "player" && *pool != "hostile") throw std::invalid_argument{"Unknown capture projectile pool"};
    auto const *shot{(*pool == "player" ? combat.projectiles : combat.hostile_projectiles).objects().head()};
    while(shot && (shot->flags & 8)) shot = shot->next;
    if(!shot) throw std::invalid_argument{"No live projectile for capture"};
    return shot->placement.position;
  }
  if(!spec.get<bool>("player", false)) throw std::invalid_argument{"Capture focus needs point, actor, projectile or player"};
  return player.pose().position;
}

ptree pose_json(game::object_pose const &pose) {
  ptree result;
  result.put("column", pose.position.column / 256.0);
  result.put("row", pose.position.row / 256.0);
  result.put("height", std::bit_cast<int16_t>(pose.position.height));
  result.put("heading", pose.angles.heading * 360.0 / 65536);
  result.put("pitch", std::bit_cast<int16_t>(pose.angles.pitch) * 360.0 / 65536);
  result.put("roll", std::bit_cast<int16_t>(pose.angles.roll) * 360.0 / 65536);
  return result;
}

void validate_times(ptree const &items) {
  unsigned int constexpr maximum_ticks{500 * 3600}; // nominal PIT clock: one hour
  unsigned int previous{0};
  for(auto const &[key, item] : items) {
    unsigned int const tick{item.get<unsigned int>("tick")};
    if(!key.empty() || tick < previous || tick % capture_session::step_ticks || tick > maximum_ticks) {
      throw std::invalid_argument{"Capture times must be sorted multiples of eight ticks, within one hour"};
    }
    previous = tick;
  }
}

} // anonymous namespace

capture_session::capture_session(std::filesystem::path const &path) : directory{std::filesystem::absolute(path).parent_path()} {
  boost::property_tree::read_json(path.string(), plan);
  if(plan.get<unsigned int>("version") != 1) throw std::invalid_argument{"Unknown capture plan version"};
  if(!plan.get_child_optional("events")) plan.add_child("events", ptree{});
  validate_times(plan.get_child("events"));
  validate_times(plan.get_child("shots"));
  if(plan.get_child("shots").empty()) throw std::invalid_argument{"Capture plan needs at least one shot"};
  field_of_view = plan.get<double>("fov", 50.2);
  if(!std::isfinite(field_of_view) || field_of_view < 15 || field_of_view > 140) throw std::invalid_argument{"Capture FOV must be 15..140 degrees"};
}

void capture_session::before_frame(unsigned int tick, game::player_flight &player, game::mission_combat &combat,
  game::hangar_state &hangar, uint16_t available_weapons) {
  auto const &events{plan.get_child("events")};
  for(auto it{std::next(events.begin(), next_event)}; it != events.end() && it->second.get<unsigned int>("tick") <= tick; ++it, ++next_event) {
    auto const &event{it->second};
    if(auto const value{event.get_child_optional("position")}) {
      player.pose().position = position(triple(*value));
      player.pose().fractions = {};
      game::set_player_flag(player.lifecycle.flags, game::player_flag::ground_protection, false);
      hangar.returning = game::hangar_return_phase::none;
      player.supply.phase = game::supply_phase::flight;
    }
    if(auto const value{event.get_child_optional("angles")}) {
      auto const degrees{triple(*value)};
      player.pose().angles = {
        .heading{angle(degrees[0])},
        .pitch{angle(degrees[1])},
        .roll{angle(degrees[2])},
      };
    }
    if(auto const value{event.get_child_optional("aim")}) {
      player.pose().angles = look_at(player.pose().position, focus(*value, player, combat), event.get<double>("roll", 0));
    }
    if(event.get<bool>("refill", false)) {
      if(auto *caero{std::get_if<game::caero_flight_state>(&player.craft)}) {
        caero->energy.buffer = game::caero_energy_state::buffer_capacity;
        caero->energy.reserve = game::caero_energy_state::reserve_capacity;
        caero->energy.boost = game::caero_energy_state::boost_capacity;
      } else {
        for(uint8_t slot{0}; slot < combat.skimma.slots.size(); ++slot) game::refill_skimma_weapon(combat.skimma.slots[slot].ammunition, slot);
      }
    }
    if(auto const value{event.get_optional<bool>("freeze")}) player.frozen = *value;
    if(auto const value{event.get_optional<bool>("primary")}) primary = *value;
    if(auto const value{event.get_optional<bool>("secondary")}) secondary = *value;
    if(auto const value{event.get_optional<unsigned int>("weapon")}) {
      if(*value < 1 || *value > 10 || !(available_weapons & (1u << (*value - 1)))) throw std::invalid_argument{"Capture weapon is not unlocked in this mission"};
      if(std::holds_alternative<game::skimma_flight_state>(player.craft)) {
        if(*value > 3 || !combat.skimma.select(player.upgraded, static_cast<uint8_t>(*value), available_weapons, static_cast<uint16_t>(tick))) {
          throw std::invalid_argument{"Capture Skimma weapon unavailable"};
        }
      } else if(*value < 4) combat.primary_weapon = static_cast<uint8_t>(*value);
      else combat.secondary_weapon = static_cast<uint8_t>(*value);
      combat.target = {};
    }
  }
}

bool capture_session::capture(unsigned int tick, game::player_flight const &player, game::mission_combat const &combat,
  game::city_map const &cells, resources::geometry_bank const &bank, uint16_t available_weapons,
  int width, int height, std::function<std::span<framework::render::rgba_pixel const>()> const &render) {
  auto const &shots{plan.get_child("shots")};
  bool written{false};
  for(auto it{std::next(shots.begin(), next_shot)}; it != shots.end() && it->second.get<unsigned int>("tick") <= tick; ++it, ++next_shot) {
    auto const &shot{it->second};
    std::filesystem::path const file{directory / shot.get<std::string>("file")};
    std::filesystem::create_directories(file.parent_path());
    ptree state;
    state.put("tick", tick);
    state.put("available_weapons", available_weapons);
    state.put("primary_weapon", unsigned{combat.primary_weapon});
    state.put("secondary_weapon", unsigned{combat.secondary_weapon});
    state.put("objectives_completed", combat.completed_objectives);
    state.put("random_state", combat.random_state);
    if(auto const *caero{std::get_if<game::caero_flight_state>(&player.craft)}) state.put("weapon_energy", caero->energy.reserve);
    state.add_child("player", pose_json(player.pose()));
    ptree actors;
    for(auto const &actor : combat.actors) {
      auto value{pose_json(actor.pose)};
      value.put("index", unsigned{actor.index});
      value.put("definition", unsigned{actor.definition_slot});
      value.put("flags", unsigned{actor.flags});
      value.put("fade", unsigned{actor.fade});
      value.put("category", static_cast<int>(actor.category));
      value.put("extent", bank.header_at(actor.parameters.model_token).extent);
      actors.push_back({"", value});
    }
    state.add_child("actors", actors);
    ptree targets;
    for(unsigned int index{0}; index < cells.size(); ++index) {
      if(!(cells[index].state & game::city_cell::permitted_target)) continue;
      // Delphi beacons use the whole byte as brightness, not destruction flags.
      if(cells[index].type == 1 && player.world_damage_mask() == 0x20) continue;
      ptree value;
      value.put("column", index % game::city_map_size.column);
      value.put("row", index / game::city_map_size.column);
      value.put("type", unsigned{cells[index].type});
      value.put("state", unsigned{cells[index].state});
      targets.push_back({"", value});
    }
    state.add_child("targets", targets);
    for(auto const &[name, pool] : {std::pair{"player_projectiles", &combat.projectiles}, {"hostile_projectiles", &combat.hostile_projectiles}}) {
      ptree values;
      for(auto const *projectile{pool->objects().head()}; projectile; projectile = projectile->next) {
        auto value{pose_json(projectile->placement)};
        value.put("id", projectile->native_id);
        value.put("model", projectile->parameters.model_token);
        value.put("flags", unsigned{projectile->flags});
        values.push_back({"", value});
      }
      state.add_child(name, values);
    }
    state.add_child("shot", shot);
    if(!shot.get<bool>("state_only", false)) {
      cockpit = shot.get<bool>("cockpit", false);
      camera.reset();
      if(!cockpit) {
        auto const target{focus(shot.get_child("focus"), player, combat)};
        auto const offset{triple(shot.get_child("offset"))};
        auto const coordinates{shot.get_child_optional("position") ? triple(shot.get_child("position"))
          : std::array<double, 3>{target.column / 256.0 + offset[0], target.row / 256.0 + offset[1], std::bit_cast<int16_t>(target.height) + offset[2]}};
        camera = game::object_pose{
          .position{position(coordinates)},
        };
        camera->angles = look_at(camera->position, target, shot.get<double>("roll", 0));
        state.add_child("camera", pose_json(*camera));
      }
      auto const pixels{render()};
      if(pixels.size() != static_cast<size_t>(width) * height) throw std::logic_error{"Capture did not produce a gameplay framebuffer"};
      std::ofstream output{file, std::ios::binary};
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output << "P6\n" << width << ' ' << height << "\n255\n";
      std::vector<char> bytes;
      bytes.reserve(pixels.size() * 3);
      for(auto const &pixel : pixels) {
        bytes.push_back(static_cast<char>(pixel.r));
        bytes.push_back(static_cast<char>(pixel.g));
        bytes.push_back(static_cast<char>(pixel.b));
      }
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      output.close();
      written = true;
      camera.reset();
    }
    boost::property_tree::write_json(file.string() + ".json", state);
  }
  return written;
}

bool capture_session::finished() const noexcept {
  return next_shot == plan.get_child("shots").size();
}

} // namespace darker::debug
#endif // NDEBUG
