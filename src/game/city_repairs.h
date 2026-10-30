#pragma once

#include <bitset>
#include <span>
#include <optional>
#include "game/city_map.h"
#include "game/power_grid.h"
#include "resources/scenario.h"
#include "resources/world_profile.h"

namespace darker::game {

inline uint8_t constexpr delphi_damage_bit{resources::profile(resources::world_kind::delphi).damage_mask};
inline uint8_t constexpr delphi_beacon_type{1};
using city_cell_set = std::bitset<city_map_cell_count>;

class delphi_collateral_damage {
private:
  city_cell_set service_repairs;
  bool headquarters_destroyed{false};
  std::optional<power_loss_warning> power_warning;

public:
  static unsigned int constexpr headquarters_cell{city_cell_index(49, 113)};

  void record_player_destruction(unsigned int index, city_cell const &before);
  void record_power_loss(city_map const &cells, unsigned int destroyed_cell);
  std::optional<power_loss_warning> take_power_warning() noexcept;
  void repair_services(city_map &cells);
  void repair_return_hangar(city_map &cells, uint16_t site);
  bool headquarters_lost() const noexcept;
};

city_cell_set scripted_destruction_targets(resources::scenario_record const &record, std::span<std::byte const> program);
void repair_power_station_buildings(city_map &cells);
void repair_scenario_buildings(city_map &cells, resources::scenario_record const &record, std::span<std::byte const> program);

} // namespace darker::game
