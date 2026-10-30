#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
#include "game/city_map.h"
#include "game/time.h"

namespace darker::game {

class beacon_changes;

struct delphi_power_rules {
  static uint8_t constexpr first_structure_type{83};
  static uint8_t constexpr transformer_type{85};
  static unsigned int constexpr full_output{255};
  static float constexpr outage_radius_cells{18};
  static float constexpr recovery_radius_cells{36};
  static game_duration constexpr fade_ticks{256};                              // about half a second, matching the native beacon fade duration
};

struct delphi_power_station {
  std::string_view name;
  std::array<maths::map_coordinates<unsigned int>, 2> transformers;
};

inline std::array<delphi_power_station, 3> constexpr delphi_power_stations{{
  {
    .name{"Spervent"},
    .transformers{{
      {
        .column{24},
        .row{10},
      },
      {
        .column{21},
        .row{17},
      },
    }},
  },
  {
    .name{"Bridfirth"},
    .transformers{{
      {
        .column{116},
        .row{58},
      },
      {
        .column{108},
        .row{59},
      },
    }},
  },
  {
    .name{"Gesic"},
    .transformers{{
      {
        .column{115},
        .row{111},
      },
      {
        .column{115},
        .row{113},
      },
    }},
  },
}};

class delphi_power_grid {
private:
  struct beacon_supply {
    unsigned int cell;
    uint8_t scripted_level;
    std::array<uint8_t, delphi_power_stations.size()> station_losses;
  };
  std::vector<beacon_supply> beacons;
  std::array<game_duration, delphi_power_stations.size()> station_fades{};
  game_duration blackout_fade{0};
  bool initialised{false};
  bool applied{false};

  void initialise(city_map const &cells);

public:
  void advance(city_map &cells, beacon_changes &scripted_changes, clock_tick clock, game_duration frame_step);
  void restore_beacons(city_map &cells);
};

} // namespace darker::game
