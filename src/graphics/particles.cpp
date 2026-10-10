#include "graphics/particles.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "graphics/particle_tables.h"
#include "graphics/near_clip.h"
#include "graphics/screen_layout.h"
#include "maths/angle.h"

namespace darker::graphics {

std::vector<particle_point> project_emitter(game::particle_emitter const &emitter, model_placement const centre,
  camera_basis const &basis, screen_vertex const &origin, double const focal_length) {
  /// 689F projects each ring sample and walks both arcs from the farthest accepted point towards the nearest
  vec3<render_geometry::accumulator> const offsets{
    render_geometry::wrap_coordinate(centre.horizontal.whole) * 256 + centre.horizontal.fraction,
    render_geometry::wrap_coordinate(centre.vertical.whole) * 256 + centre.vertical.fraction,
    render_geometry::wrap_coordinate(centre.depth.whole) * 256 + centre.depth.fraction,
  };
  std::array<int16_t projection_axis::*, 3> const members{&projection_axis::horizontal, &projection_axis::vertical, &projection_axis::depth};
  std::array<vec2<int>, 3> coefficients{};
  for(unsigned int i{0}; i < 3; ++i) {
    coefficients[i] = {((basis[1].*members[i] * std::bit_cast<int16_t>(emitter.radius)) >> 16) >> 2,
      ((basis[0].*members[i] * std::bit_cast<int16_t>(emitter.radius)) >> 16) >> 2};
  }
  auto const retain{[](render_geometry::accumulator const value){
    return render_geometry::wrap_projection(static_cast<render_geometry::accumulator_bits>(value));
  }};
  unsigned int index{static_cast<unsigned int>(emitter.angle >> 5)};
  std::vector<particle_point> points;
  size_t far{0}, near{0};
  for(unsigned int sample{0}; sample < (emitter.sampling & 255); ++sample) {
    index &= 2046;
    auto const sine{maths::original_sine[index >> 1]};
    auto const cosine{maths::phase_cosine(index >> 1)};
    vec3<render_geometry::accumulator> v{};
    for(unsigned int i{0}; i < 3; ++i) v[i] = retain(offsets[i] + ((coefficients[i][0] * sine) >> 8) + ((coefficients[i][1] * cosine) >> 8));
    auto const depth{v[2] >> 8};
    if(depth >= 32) {
      auto const point{focal_length == render_geometry::focal_length
        ? screen_vertex{std::bit_cast<int16_t>(static_cast<uint16_t>(v[0] / depth + origin.x)), std::bit_cast<int16_t>(static_cast<uint16_t>(v[1] / depth + origin.y))}
        : project_vertex({v[0], v[1], v[2]}, origin, focal_length)};
      points.push_back({
        .x{point.x},
        .y{point.y},
        .depth{static_cast<render_geometry::coordinate>(depth)}
      });
      if(depth > points[far].depth) far = points.size() - 1;
      if(depth < points[near].depth) near = points.size() - 1;
    }
    index += ((emitter.sampling >> 6) | 3) + 1;
  }
  if(points.size() < 2) return points;
  std::ranges::reverse(points);
  int const count{static_cast<int>(points.size())};
  int const f{count - 1 - static_cast<int>(far)}, n{count - 1 - static_cast<int>(near)};
  std::vector<particle_point> ordered;
  int sp{0}, si{f};
  while(true) {
    ordered.push_back(points[si++]);
    if(si >= count) {
      if(sp == n) break;
      while(sp != n) ordered.push_back(points[sp++]);
      break;
    }
    if(si == n) break;
  }
  si = f - 1;
  if(si < sp) si = count - 1;
  while(true) {
    ordered.push_back(points[si]);
    if(si == n) break;
    if(--si < sp) si = count - 1;
  }
  return ordered;
}

void draw_particle(framework::render::indexed_surface target,
  framework::render::const_indexed_surface sheet, particle_point const point, uint8_t const phase, raster_viewport const viewport, double const focal_length) {
  if(!viewport.fits(target.width, target.height)) throw std::invalid_argument{"Particle viewport exceeds the framebuffer"};
  /// 6AA6 selects discrete source artwork and masked rows, preserving even-X destination alignment
  unsigned int constexpr animation_phase_count{22};
  int constexpr near_plane_depth{32};
  int constexpr point_sprite_depth{8190};
  int constexpr selector_table_base{0x6b34};
  int constexpr animation_column_table{0x6bc7};
  if(phase >= animation_phase_count || point.depth < near_plane_depth) return;
  if(point.depth >= point_sprite_depth) {
    if(viewport.contains(point.x, point.y)) target.pixels[point.y * target.stride + point.x] = particle_point_colours[phase];
    return;
  }
  // five baked sprite sizes, nearest first; these addresses identify records in the extracted native selector table
  int const table{point.depth < 1752 ? 0x6b9c : point.depth < 2454 ? 0x6b79 : point.depth < 3510   ? 0x6b5e : point.depth < 4914   ? 0x6b47 : 0x6b34};
  auto const at{[=](int const address){
    return particle_selectors[address - selector_table_base];
  }};
  int const radius{at(table + 10)};
  int const x{point.x - radius}, y{point.y - radius};
  int const alignment{x & 2}, destination_x{x & ~1};
  // each parity record has five bytes: animation-column base and two source-coordinate pairs selected by X alignment
  int const record{table + 5 * (phase & 1)};
  int const source_x{2 * (at(animation_column_table + at(record) + phase / 2) + at(record + 1 + alignment))};
  int const source_y{at(record + 2 + alignment)};
  // radius follows both parity records; subsequent byte pairs give each masked row's skip and width
  for(int row{0}; row < radius * 2; ++row) {
    double const scale{focal_length / render_geometry::focal_length};
    int const top{std::max(0, point.y + static_cast<int>((y + row - point.y) * scale))};
    int const bottom{std::min(viewport.bottom, point.y + static_cast<int>((y + row + 1 - point.y) * scale))};
    int const skip{at(table + 11 + row * 2)}, width{at(table + 12 + row * 2)};
    for(int column{skip}; column < skip + width; ++column) {
      int const left{std::max(0, point.x + static_cast<int>((destination_x + column - point.x) * scale))};
      int const right{std::min(viewport.right + 1, point.x + static_cast<int>((destination_x + column + 1 - point.x) * scale))};
      if(left >= right) continue;
      auto const colour{sheet.pixels[(source_y + row) * sheet.stride + source_x + column]};
      for(int output_y{top}; output_y < bottom; ++output_y) std::fill(target.row(output_y).begin() + left, target.row(output_y).begin() + right, colour);
    }
  }
}

} // namespace darker::graphics
