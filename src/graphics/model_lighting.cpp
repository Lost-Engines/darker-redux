#include "graphics/model_lighting.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "vectorstorm/vector/vector3.h"

namespace darker::graphics {

distance_shading::distance_shading(unsigned int const count) {
  /// B73C builds palette-index ramps towards shade one, with a half-step initial fraction and signed IDIV rounding
  unsigned int constexpr minimum_distance_tables{2};                           // interpolation needs both a near and a far endpoint
  int constexpr interpolated_shades{model_colours::shade_count};               // ramp entries 28–31 have special renderer meanings
  int constexpr distant_shade{1};
  int constexpr shade_fraction_scale{256};
  int constexpr half_shade{shade_fraction_scale / 2};
  if(count < minimum_distance_tables || count > scene_limits::maximum_distance_shade_tables) throw std::invalid_argument{"Distance shade table count exceeds its original byte range"};
  tables.resize(count);
  end_depth = (count - 1) << scene_limits::distance_shade_shift;
  for(int shade{0}; shade < interpolated_shades; ++shade) {
    int const step{((shade - distant_shade) * shade_fraction_scale) / static_cast<int>(count - 1)};
    for(unsigned int distance{0}; distance < count; ++distance) {
      tables[distance][shade] = static_cast<uint8_t>((shade * shade_fraction_scale + half_shade - static_cast<int>(distance) * step) >> 8);
    }
  }
}

distance_shading::distance_shading(fog_range const range) : distance_shading{} {
  /// Distances are map cells; table generation and indexed palette ramps remain unchanged
  if(!std::isfinite(range.start_cells) || !std::isfinite(range.end_cells)
    || range.start_cells < 0 || range.end_cells < range.start_cells || range.end_cells > scene_limits::maximum_scan_radius_cells) {
    throw std::invalid_argument{"Fog distances must be finite, ordered and within the visible world range"};
  }
  if(!std::isfinite(range.curve) || range.curve < 0) throw std::invalid_argument{"Fog curve must be finite and non-negative"};
  if(range.background_end_cells && (!std::isfinite(*range.background_end_cells)
    || *range.background_end_cells < range.end_cells || *range.background_end_cells > scene_limits::maximum_scan_radius_cells)) {
    throw std::invalid_argument{"Background fade must end at or beyond the natural fog endpoint, within the visible world range"};
  }
  background_end_depth = static_cast<uint32_t>(std::llround(range.background_end_cells.value_or(range.end_cells) * render_geometry::camera_units_per_cell));
  start_depth = static_cast<uint32_t>(std::llround(range.start_cells * render_geometry::camera_units_per_cell));
  end_depth = static_cast<uint32_t>(std::llround(range.end_cells * render_geometry::camera_units_per_cell));
  if(range.curve > 0 && end_depth > start_depth) {
    auto const span{end_depth - start_depth};
    curved_depths.resize(span + 1);
    auto const shade_distance{(tables.size() - 1) << scene_limits::distance_shade_shift};
    // Precompute at camera-depth precision, avoiding a logarithm per object per frame.
    // Long double also retains useful ratios for very small positive curve values.
    long double const curve{range.curve};
    auto const normalisation{std::log1p(curve)};
    for(uint32_t depth{0}; depth <= span; ++depth) {
      auto const strength{std::log1p(curve * (static_cast<long double>(depth) / span)) / normalisation};
      curved_depths[depth] = static_cast<uint16_t>(std::clamp(strength, 0.0L, 1.0L) * shade_distance);
    }
    curved_depths.back() = static_cast<uint16_t>(shade_distance);
  }
}

void distance_shading::set_fog_palette(framework::render::colour_palette const *const palette, uint8_t const background) {
  /// Build indexed RGB blends once per palette change, retaining the game's colours and cheap span fills.
  fog_tables.clear();
  if(!palette) return;
  fog_tables.resize(tables.size());
  auto const &destination{(*palette)[background]};
  auto const last{static_cast<unsigned int>(fog_tables.size() - 1)};
  for(unsigned int step{0}; step <= last; ++step) {
    for(unsigned int index{0}; index < palette->size(); ++index) {
      if(step == 0 || step == last) {
        fog_tables[step][index] = step == 0 ? static_cast<uint8_t>(index) : background;
        continue;
      }
      auto const &source{(*palette)[index]};
      vec3<int> const blended{
        static_cast<int>((source.r * (last - step) + destination.r * step + last / 2) / last),
        static_cast<int>((source.g * (last - step) + destination.g * step + last / 2) / last),
        static_cast<int>((source.b * (last - step) + destination.b * step + last / 2) / last)
      };
      int best_error{3 * 256 * 256};
      for(unsigned int candidate{0}; candidate < palette->size(); ++candidate) {
        auto const &colour{(*palette)[candidate]};
        auto const difference{blended - vec3<int>{colour.r, colour.g, colour.b}};
        auto const error{difference.dot(difference)};
        if(error < best_error) {
          best_error = error;
          fog_tables[step][index] = static_cast<uint8_t>(candidate);
        }
      }
    }
  }
}

model_colours distance_shading::colours(render_geometry::coordinate_bits depth, model_path const path, uint8_t const light) const {
  /// Remap fog distance, then retain 2CE4 beacon darkening; near clipping clamps a negative model origin to zero
  render_geometry::coordinate_bits constexpr depth_sign_bit{render_geometry::coordinate_bits{1} << (render_geometry::whole_bits - 1)};
  unsigned int constexpr full_beacon_light{255};
  unsigned int constexpr darkness_depth_scale{16};                             // each missing light unit adds 16 projection-depth units
  unsigned int constexpr dynamic_colour_base{0xf0};                            // palette ramp 7, upper half; light supplies the low nibble
  // Keep native palette darkening and beacon lighting through the whole view.
  // Only the final distance interval blends the resulting silhouettes into the background.
  if(path == model_path::near_clipped && (depth & depth_sign_bit)) depth = 0;
  uint64_t const shade_distance{(tables.size() - 1) << scene_limits::distance_shade_shift};
  uint64_t const fog_depth{depth <= start_depth ? 0 : depth >= end_depth ? shade_distance
    : !curved_depths.empty() ? curved_depths[depth - start_depth]
    : std::min(shade_distance, static_cast<uint64_t>(depth - start_depth) * shade_distance / (end_depth - start_depth))};
  auto const adjusted{fog_depth + darkness_depth_scale * (full_beacon_light - light)};
  auto const index{std::min<size_t>(adjusted >> scene_limits::distance_shade_shift, tables.size() - 1)};
  model_colours result{
    .shades{tables[index]},
    .dynamic{static_cast<uint8_t>(dynamic_colour_base | (light >> 4))}
  };
  if(!fog_tables.empty() && background_end_depth > end_depth && depth > end_depth) {
    uint64_t const fade_depth{std::min(depth, background_end_depth) - end_depth};
    auto const fade_index{static_cast<size_t>(fade_depth * (fog_tables.size() - 1) / (background_end_depth - end_depth))};
    result.fog = &fog_tables[fade_index];
  }
  // Retain the original next-row lookup explicitly. Beyond the final table,
  // use zero padding rather than reading unrelated emulated scratch memory.
  if(index + 1 < tables.size()) std::copy_n(tables[index + 1].begin(), result.point_shade_tail.size(), result.point_shade_tail.begin());
  return result;
}

} // namespace darker::graphics
