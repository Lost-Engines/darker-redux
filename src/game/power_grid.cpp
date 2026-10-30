#include "game/power_grid.h"
#include <algorithm>
#include <cmath>
#include "vectorstorm/vector/vector2.h"
#include "game/beacon_changes.h"
#include "game/city_repairs.h"

namespace darker::game {
namespace {

bool disabled(delphi_power_station const &station, city_map const &cells) {
  /// One intact transformer can supply its station; missing models are not destroyed transformers
  return std::ranges::all_of(station.transformers, [&](auto const &position) {
    auto const &cell{cells[city_cell_index(position.column, position.row)]};
    return cell.type == delphi_power_rules::transformer_type && (cell.state & delphi_damage_bit);
  });
}

uint8_t station_loss(delphi_power_station const &station, unsigned int const column, unsigned int const row) {
  /// Local supply loss is complete nearby, then falls linearly with distance from the pair's midpoint
  auto const &first{station.transformers[0]};
  auto const &second{station.transformers[1]};
  vec2<float> const centre{static_cast<float>(first.column + second.column) / 2, static_cast<float>(first.row + second.row) / 2};
  vec2<float> const position{static_cast<float>(column), static_cast<float>(row)};
  auto const distance{(position - centre).length()};
  auto const loss{(delphi_power_rules::recovery_radius_cells - distance)
    / (delphi_power_rules::recovery_radius_cells - delphi_power_rules::outage_radius_cells)};
  return static_cast<uint8_t>(std::lround(std::clamp(loss, 0.0f, 1.0f) * delphi_power_rules::full_output));
}

game_duration advance_fade(game_duration const previous, bool const lost, game_duration const step) noexcept {
  /// Saturating elapsed time is independent of native clock wrapping and the number of rendered frames
  return lost ? static_cast<game_duration>(std::min<unsigned int>(delphi_power_rules::fade_ticks, static_cast<unsigned int>(previous) + step))
    : static_cast<game_duration>(previous > step ? previous - step : 0);
}

} // anonymous namespace

std::optional<power_loss_warning> unauthorised_power_loss(city_map const &cells, unsigned int const destroyed_cell) {
  /// Warn only for a newly destroyed transformer completing an unauthorised station demolition
  for(auto const &station : delphi_power_stations) {
    bool const hit{std::ranges::any_of(station.transformers, [&](auto const &position) {
      return city_cell_index(position.column, position.row) == destroyed_cell;
    })};
    if(!hit || !disabled(station, cells)) continue;
    if(std::ranges::any_of(station.transformers, [&](auto const &position) {
      return cells[city_cell_index(position.column, position.row)].state & city_cell::permitted_target;
    })) return std::nullopt;
    auto const lost{std::ranges::count_if(delphi_power_stations, [&](auto const &candidate) { return disabled(candidate, cells); })};
    return static_cast<power_loss_warning>(lost - 1);
  }
  return std::nullopt;
}

void delphi_power_grid::initialise(city_map const &cells) {
  /// Cache spatial falloff once per mission and immediately reflect stations already destroyed in the saved campaign
  bool all_disabled{true};
  for(unsigned int station{0}; station < delphi_power_stations.size(); ++station) {
    bool const lost{disabled(delphi_power_stations[station], cells)};
    station_fades[station] = lost ? delphi_power_rules::fade_ticks : 0;
    all_disabled &= lost;
  }
  blackout_fade = all_disabled ? delphi_power_rules::fade_ticks : 0;
  for(unsigned int index{0}; index < cells.size(); ++index) {
    if(cells[index].type != delphi_beacon_type) continue;
    beacon_supply supply{
      .cell{index},
      .scripted_level{cells[index].state},
      .station_losses{},
    };
    for(unsigned int station{0}; station < delphi_power_stations.size(); ++station) {
      supply.station_losses[station] = station_loss(delphi_power_stations[station], index % city_map_size.column, index / city_map_size.column);
    }
    beacons.push_back(supply);
  }
  initialised = true;
}

void delphi_power_grid::restore_beacons(city_map &cells) {
  /// Remove only our temporary supply effect before script fades or save packing; never undo an authored outage
  if(!applied) return;
  for(auto const &supply : beacons) {
    auto &cell{cells[supply.cell]};
    if(cell.type == delphi_beacon_type) cell.state = supply.scripted_level;
  }
  applied = false;
}

void delphi_power_grid::advance(city_map &cells, beacon_changes &scripted_changes, clock_tick const clock, game_duration const frame_step) {
  /// Compose original scripted brightness with transformer supply instead of repeatedly dimming our previous output
  if(!initialised) initialise(cells);
  restore_beacons(cells);
  scripted_changes.advance(cells, clock);
  bool all_disabled{true};
  for(unsigned int station{0}; station < delphi_power_stations.size(); ++station) {
    bool const lost{disabled(delphi_power_stations[station], cells)};
    station_fades[station] = advance_fade(station_fades[station], lost, frame_step);
    all_disabled &= lost;
  }
  blackout_fade = advance_fade(blackout_fade, all_disabled, frame_step);
  auto const global_supply{delphi_power_rules::full_output - delphi_power_rules::full_output * blackout_fade / delphi_power_rules::fade_ticks};
  for(auto &supply : beacons) {
    auto &cell{cells[supply.cell]};
    if(cell.type != delphi_beacon_type) continue;
    supply.scripted_level = cell.state;
    auto available{global_supply};
    for(unsigned int station{0}; station < station_fades.size(); ++station) {
      available = std::min(available, delphi_power_rules::full_output
        - static_cast<unsigned int>(supply.station_losses[station]) * station_fades[station] / delphi_power_rules::fade_ticks);
    }
    cell.state = static_cast<uint8_t>(supply.scripted_level * available / delphi_power_rules::full_output);
  }
  applied = true;
}

} // namespace darker::game
