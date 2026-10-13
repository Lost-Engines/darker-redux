#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

// Compare displayed RGB brightness, not indices from unrelated palette ramps.
// Built once per palette change; the rasterisers only perform table lookups.
class lighten_blend {
  std::array<uint16_t, 256> brightness{};
public:
  explicit lighten_blend(framework::render::colour_palette const &palette) noexcept {
    for(unsigned int i{0}; i < palette.size(); ++i) {
      auto const &colour{palette[i]};
      brightness[i] = static_cast<uint16_t>(77 * colour.r + 150 * colour.g + 29 * colour.b);
    }
  }

  uint8_t operator()(uint8_t source, uint8_t destination) const noexcept {
    return brightness[source] > brightness[destination] ? source : destination;
  }
};

inline void fill_polygon_span(std::span<uint8_t> pixels, uint8_t colour, lighten_blend const *blend) {
  if(!blend) std::ranges::fill(pixels, colour);
  else for(auto &pixel : pixels) pixel = (*blend)(colour, pixel);
}

} // namespace darker::graphics
