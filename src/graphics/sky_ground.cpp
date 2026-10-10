#include "graphics/sky_ground.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include "graphics/screen_layout.h"
#include "maths/angle.h"

namespace darker::graphics {

void draw_sky_ground(framework::render::indexed_surface target, camera_angles const angles, screen_vertex const &origin, int const bottom, double const focal_length) {
  /// Reconstruct B409's indexed sky/ground bands using its rounded angles, integer projection and B71A colour sequence
  if(bottom < 0 || bottom > target.height) throw std::invalid_argument{"Sky/ground viewport exceeds the framebuffer"};
  std::array<uint8_t, 17> constexpr colours{28, 29, 30, 31, 60, 61, 62, 63, 252, 92, 93, 94, 95, 124, 125, 126, 127};
  std::array<int, 16> constexpr offsets{74, 56, 42, 30, 20, 12, 6, 2, -2, -6, -12, -20, -30, -42, -56, -74};
  auto pitch{maths::view_angle_phase(angles.pitch)};
  auto roll{maths::view_angle_phase(angles.roll)};
  // The native shortcuts assume a narrow viewport. For other projections,
  // classify view rays against the horizon plane instead: no pitch cutoff or
  // tangent singularity, even when looking past straight up/down or banking.
  bool const native_projection{target.width <= 320 && bottom <= 240 && focal_length == render_geometry::focal_length};
  if(!native_projection) {
    double constexpr sine_scale{32768.0};
    double const pitch_sine{maths::original_sine[pitch] / sine_scale};
    double const pitch_cosine{maths::phase_cosine(pitch) / sine_scale};
    double const horizontal{pitch_cosine * maths::original_sine[roll] / sine_scale};
    double const vertical{pitch_cosine * maths::phase_cosine(roll) / sine_scale};
    std::array<double, offsets.size()> boundaries{};
    for(unsigned int i{0}; i < offsets.size(); ++i) boundaries[i] = -offsets[i] * focal_length / (2 * render_geometry::focal_length);
    for(int y{0}; y < bottom; ++y) {
      double const start{(y - origin.y) * vertical - origin.x * horizontal - focal_length * pitch_sine};
      std::array<int, offsets.size() + 2> cuts{};
      unsigned int count{0};
      cuts[count++] = 0;
      if(horizontal != 0) {
        for(double const boundary : boundaries) {
          double const crossing{(boundary - start) / horizontal};
          double const cut{horizontal > 0 ? std::ceil(crossing) : std::floor(crossing) + 1};
          if(cut > 0 && cut < target.width) cuts[count++] = static_cast<int>(cut);
        }
      }
      cuts[count++] = target.width;
      std::sort(cuts.begin(), cuts.begin() + count);
      auto pixels{target.row(y)};
      for(unsigned int i{1}; i < count; ++i) {
        int const left{cuts[i - 1]}, right{cuts[i]};
        double const coordinate{start + (left + (right - left - 1) / 2.0) * horizontal};
        auto const band{std::upper_bound(boundaries.begin(), boundaries.end(), coordinate) - boundaries.begin()};
        std::fill(pixels.begin() + left, pixels.begin() + right, colours[band]);
      }
    }
    return;
  }
  if(((pitch >> 7) & 3) == 1 || ((pitch >> 7) & 3) == 2) {
    for(int y{0}; y < bottom; ++y) std::ranges::fill(target.row(y), pitch < 512 ? colours.front() : colours.back());
    return;
  }
  auto const outer_colour{pitch < 512 ? colours.front() : colours.back()};
  bool const reverse{roll >= 512};
  if(reverse) {
    roll &= 511;
    pitch ^= 511;
  }
  bool const mirror{roll >= 256};
  if(mirror) roll = 512 - roll;
  auto const sine{maths::original_sine[roll]};
  auto const cosine{maths::phase_cosine(roll)};
  auto const tangent{maths::original_sine[pitch] * 512 / maths::phase_cosine(pitch)};
  double const scale{focal_length / render_geometry::focal_length};
  int const centre_x{origin.x + static_cast<int>(((tangent * sine) >> 16) * scale)};
  int const centre_y{origin.y + static_cast<int>(((tangent * cosine) >> 16) * scale)};
  std::array<int, 16> boundaries{};
  // preserve the original independent signed high-word products at each band boundary
  int const slope{sine == 0 ? 0 : cosine * 256 / sine};
  if((sine == 0 || slope >= 64 * 256) && (centre_y + static_cast<int>(41 * scale) <= 0 || (sine != 0 && centre_y - static_cast<int>(41 * scale) >= bottom))) {
    for(int y{0}; y < bottom; ++y) std::ranges::fill(target.row(y), outer_colour);
    return;
  }
  int const first_row{sine != 0 && slope >= 64 * 256 && centre_y + static_cast<int>(41 * scale) >= bottom ? std::max(0, centre_y - static_cast<int>(41 * scale)) : 0};
  for(size_t i{0}; i < offsets.size(); ++i) {
    int const x{centre_x - static_cast<int>(((offsets[i] * sine) >> 16) * scale)};
    int const y{centre_y - first_row - static_cast<int>(((offsets[i] * cosine) >> 16) * scale)};
    boundaries[i] = sine == 0 ? y : x + static_cast<int>((static_cast<int64_t>(y) * slope) >> 8);
  }
  for(int row{0}; row < bottom; ++row) {
    int const y{mirror ? bottom - 1 - row : row};
    if(row < first_row) {
      std::fill_n(target.pixels.begin() + y * target.stride, target.width, outer_colour);
      continue;
    }
    // Along a scanline the band coordinate increases by exactly one per pixel.
    // Find the first band once, then fill up to each boundary instead of doing
    // a binary search for every pixel. A level horizon has one colour per row.
    int const coordinate{sine == 0 ? row : ((row - first_row) * slope + 255) >> 8};
    auto band{static_cast<unsigned int>(std::upper_bound(boundaries.begin(), boundaries.end(), coordinate) - boundaries.begin())};
    auto pixels{target.row(y)};
    if(sine == 0) {
      std::ranges::fill(pixels, colours[reverse ? 16 - band : band]);
      continue;
    }
    int x{0};
    while(x < target.width) {
      int const end{band == boundaries.size() ? target.width : std::min(target.width, boundaries[band] - coordinate)};
      std::fill(pixels.begin() + x, pixels.begin() + end, colours[reverse ? 16 - band : band]);
      x = end;
      ++band;
    }
  }
}

} // namespace darker::graphics
