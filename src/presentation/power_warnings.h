#pragma once

#include <array>
#include <string_view>
#include "game/power_grid.h"
#include "resources/scenario.h"
#include "resources/font.h"

namespace darker::presentation {

inline game::game_duration constexpr power_warning_duration{4000};             // eight seconds at the native game-clock rate

struct power_warning_notice {
  std::string_view text;
  game::clock_tick expiry;
  uint16_t width;
};

constexpr std::string_view power_warning_text(game::power_loss_warning warning, resources::scenario_language language) {
  /// Redux-only radio reactions, ordered from local supply loss to total blackout
  using namespace std::literals;
  std::array constexpr messages{
    std::array{
      "Tolly, we're losing beacon power! What are you doing?"sv,
      "Another power failure! Tolly, stop attacking the grid!"sv,
      "Tolly! You've blacked out the city! We're losing you..."sv,
    },
    std::array{
      "Tolly, les balises s'eteignent ! Que faites-vous ?"sv,
      "Encore une panne ! Tolly, laissez le reseau tranquille !"sv,
      "Tolly ! Toute la ville est dans le noir ! On vous perd..."sv,
    },
    std::array{
      "Tolly, die Baken verlieren Energie! Was tun Sie da?"sv,
      "Noch ein Ausfall! Tolly, lassen Sie das Stromnetz in Ruhe!"sv,
      "Tolly! Die ganze Stadt ist dunkel! Wir verlieren Sie..."sv,
    },
  };
  return messages.at(static_cast<unsigned int>(language)).at(static_cast<unsigned int>(warning));
}

inline power_warning_notice make_power_warning(game::power_loss_warning const warning, resources::scenario_language const language,
  resources::font_resource const &font, game::clock_tick const clock) {
  /// Measure the original HUD font once so the notice uses the normal centred radio-message strip
  auto const line{power_warning_text(warning, language)};
  uint16_t width{0};
  for(auto const code : line) width = static_cast<uint16_t>(width + font.glyph(resources::font_face::compact, static_cast<uint8_t>(code)).advance);
  return {
    .text{line},
    .expiry{static_cast<game::clock_tick>(clock + power_warning_duration)},
    .width{width},
  };
}

} // namespace darker::presentation
