#include "graphics/city_scene.h"
#include <algorithm>
#include <bit>
#include <ranges>
#include <stdexcept>
#include "game/beacon_light.h"
#include "graphics/near_clip.h"
#include "graphics/world_images.h"
#include "graphics/particles.h"
#include "graphics/screen_layout.h"
#include "graphics/tunnel_visibility.h"
#include "maths/angle.h"
#include "maths/world_coordinates.h"

namespace darker::graphics {

namespace {

render_geometry::coordinate word(render_geometry::accumulator const value) noexcept {
  /// Retain the culler's signed word arithmetic
  return render_geometry::wrap_coordinate(value);
}

int magnitude(render_geometry::coordinate_bits const value) noexcept {
  /// CWD/XOR complements negative coordinates rather than taking their mathematical absolute value
  return value & (render_geometry::coordinate_bits{1} << (render_geometry::whole_bits - 1)) ? static_cast<render_geometry::coordinate_bits>(~value) : value;
}

std::optional<city_draw_item> classify_model(model_placement const placement, resources::model_header const header, double const cull_scale) {
  /// Apply the shared extent cull and direct/near model-path selection
  auto const diameter{word(header.extent * 2)};
  auto bound{word(placement.depth.whole - render_geometry::near_depth)};
  bool const near{bound <= diameter};
  if(near && static_cast<render_geometry::coordinate_bits>(bound) >= static_cast<render_geometry::coordinate_bits>(diameter)) {
    if(word(bound + diameter) < 0) return std::nullopt;
    bound = -render_geometry::near_depth;
  }
  bound = word(bound + word(diameter + render_geometry::near_depth));
  if(magnitude(placement.horizontal.whole) >= bound * cull_scale || magnitude(placement.vertical.whole) >= bound * cull_scale) return std::nullopt;
  return city_draw_item{
    .placement{placement},
    .path{near ? model_path::near_clipped : model_path::direct}
  };
}

} // anonymous namespace

std::optional<city_draw_item> place_city_cell(resources::geometry_bank const &bank, game::city_cell const cell, uint16_t const index,
  uint8_t const damage_mask, camera_basis const &basis, camera_position const camera, std::optional<world_image> const image, double const cull_scale) {
  /// 2A1A selects the linked model, places its origin, chooses its drawing path and applies the extent cull
  if(!cell.type) return std::nullopt;
  if(index >= game::city_map_cell_count) throw std::invalid_argument{"City model position exceeds the map"};
  auto const offset{bank.city_model_offset(cell.type, cell.state, damage_mask)};
  auto const type{bank.city_types()[cell.type - 1]};
  auto const header{bank.header_at(offset)};
  bool const background{type.collision_marker == resources::city_type::background_marker};
  auto placement{place_model(basis, camera, {
    .column{static_cast<uint16_t>((index % game::city_map_size.column) * maths::world_format::units_per_cell + type.column_fraction)},
    .row{static_cast<uint16_t>((index / game::city_map_size.column) * maths::world_format::units_per_cell + type.row_fraction)},
    .height{std::bit_cast<int16_t>(static_cast<uint16_t>(header.height - (background ? 0 : type.collision_marker * 256)))},
  }, image)};
  if(!background) placement.sorting_distance = static_cast<render_geometry::sorting_distance>(placement.sorting_distance + header.extent);
  auto item{classify_model(placement, header, cull_scale)};
  if(item) {
    item->cell = index;
    item->model_offset = offset;
    item->background = background;
  }
  return item;
}

bool within_object_window(city_view const &view, maths::world_position const &position) noexcept {
  /// Object admission follows the same 256-cell world period as geometry placement
  auto const column{static_cast<int>(std::bit_cast<int8_t>(static_cast<uint8_t>((position.column >> maths::world_format::cell_fraction_bits) - (view.column >> maths::world_format::cell_fraction_bits))))};
  auto const row{static_cast<int>(std::bit_cast<int8_t>(static_cast<uint8_t>((position.row >> maths::world_format::cell_fraction_bits) - (view.row >> maths::world_format::cell_fraction_bits))))};
  if(view.radius <= scene_limits::native_surface_radius_cells) return column >= 1 - static_cast<int>(view.radius) && column < static_cast<int>(view.radius) && row >= 1 - static_cast<int>(view.radius) && row < static_cast<int>(view.radius);
  return column * column + row * row <= static_cast<int>(view.radius * view.radius);
}

std::optional<city_draw_item> place_scene_object(resources::geometry_bank const &bank, scene_object const &object,
  camera_basis const &basis, camera_position camera, bool const underground, std::optional<world_image> const image, double const cull_scale) {
  /// 2F35 preserves the object's fractional origin before the same model extent cull as city geometry
  // retain two sub-word bits in projection units; subtracting from the camera adds the object's fractional displacement
  camera.column = static_cast<render_geometry::coordinate_bits>(camera.column - (object.pose.fractions.column >> render_geometry::camera_fraction_shift));
  camera.row = static_cast<render_geometry::coordinate_bits>(camera.row - (object.pose.fractions.row >> render_geometry::camera_fraction_shift));
  auto placement{place_model(basis, camera, {
    .column{object.pose.position.column},
    .row{object.pose.position.row},
    .height{std::bit_cast<int16_t>(static_cast<uint16_t>(-object.pose.position.height))}
  }, image)};
  auto const header{bank.header_at(object.model_offset)};
  // BC94 patches 2EDC from ADD to SUB for underground moving objects
  placement.sorting_distance = static_cast<render_geometry::sorting_distance>(placement.sorting_distance + (underground ? -header.extent : header.extent));
  auto item{classify_model(placement, header, cull_scale)};
  if(item) {
    item->model_offset = object.model_offset;
    item->orientation = orient_model(basis, object.pose.angles);
    item->object_light = object.light;
    item->draw_record = object.native_id ? static_cast<uint16_t>(object.native_id + 14) : 0;
    // Preserve the original dot transition: it required passing both LOD thresholds.
    // The old flat-shading threshold no longer changes how meshes are shaded.
    auto const depth_band{static_cast<render_geometry::coordinate_bits>(word(placement.depth.whole - render_geometry::near_depth)) >> 8};
    item->distant_point = item->path == model_path::direct
      && depth_band >= header.flat_distance && depth_band >= header.point_distance;
  }
  return item;
}

std::optional<screen_vertex> project_distant_object(model_placement const placement, screen_vertex const &origin, raster_viewport const viewport, uint8_t const residue, double const focal_length) {
  /// 2D32 consumes traversal AL for the Y divide, then projected Y's low byte for the X divide
  auto point{project_vertex({
    .horizontal{0},
    .vertical{word(placement.vertical.whole) * render_geometry::fraction_scale + residue},
    .depth{word(placement.depth.whole) * render_geometry::fraction_scale}
  }, origin, focal_length)};
  if(point.y < 0 || point.y >= viewport.bottom) return std::nullopt;
  point.x = project_vertex({
    .horizontal{word(placement.horizontal.whole) * render_geometry::fraction_scale + static_cast<uint8_t>(point.y)},
    .vertical{0},
    .depth{word(placement.depth.whole) * render_geometry::fraction_scale}
  }, origin, focal_length).x;
  if(!viewport.contains(point.x, point.y)) return std::nullopt;
  return point;
}

void order_city_models(std::vector<city_draw_item> &items) {
  /// Background records use the original LIFO list; ordinary records use descending distance with insertion-stable ties
  // 2B09 inserts a binary tree. At 2C49, AL is zero after a left descent,
  // or the current record's low byte when returning from its farther subtree
  auto const absent{items.size()};
  struct branches {
    size_t farther, nearer;
  };
  std::vector<branches> tree(items.size(), {absent, absent});
  size_t root{absent};
  for(size_t index{0}; index < items.size(); ++index) {
    items[index].projection_residue = 0;
    if(items[index].background) continue;
    if(root == absent) {
      root = index;
      continue;
    }
    auto parent{root};
    for(;;) {
      bool const farther{items[index].placement.sorting_distance > items[parent].placement.sorting_distance};
      auto &branch{farther ? tree[parent].farther : tree[parent].nearer};
      if(branch == absent) {
        branch = index;
        if(farther) items[parent].projection_residue = static_cast<uint8_t>(items[parent].draw_record);
        break;
      }
      parent = branch;
    }
  }
  auto const first_sorted{std::stable_partition(items.begin(), items.end(), [](auto const &item){
    return item.background;
  })};
  std::reverse(items.begin(), first_sorted);
  std::stable_sort(first_sorted, items.end(), [](auto const &left, auto const &right){
    return left.placement.sorting_distance > right.placement.sorting_distance;
  });
}

size_t city_renderer::draw(framework::render::indexed_surface target, resources::geometry_bank const &bank,
  std::span<game::city_cell const, game::city_map_cell_count> const cells, city_view const view, uint8_t const damage_mask,
  distance_shading const &lighting, model_animation animation, std::span<scene_object const> const objects, particle_scene const *const particles) {
  /// Assemble the selected native visibility path before sorting world geometry, actors and particle effects
  if(!view.viewport.fits(target.width, target.height)) throw std::invalid_argument{"City viewport exceeds the framebuffer"};
  auto const basis{make_camera_basis(view.angles)};
  double const cull_scale{std::max({1.0, (view.viewport.right + 1) / (2 * view.focal_length), view.viewport.bottom / (2 * view.focal_length)})};
  // position words hold 1/256-cell units; the extra fraction byte subdivides those units by another 256
  // projection needs 1/1024-cell units: multiply the word by four and retain the fraction's top two bits
  // Retain the full scaled world position for placement of each visible world image.
  camera_position const camera{
    .column{static_cast<render_geometry::coordinate_bits>(view.column * render_geometry::world_to_camera_scale + (view.column_fraction >> render_geometry::camera_fraction_shift))},
    .row{static_cast<render_geometry::coordinate_bits>(view.row * render_geometry::world_to_camera_scale + (view.row_fraction >> render_geometry::camera_fraction_shift))},
    .altitude{view.altitude},
  };
  items.clear();
  auto const place{[&](uint16_t const index){
    if(auto item{place_city_cell(bank, cells[index], index, damage_mask, basis, camera, std::nullopt, cull_scale)}) {
      items.push_back(*item);
      return true;
    }
    return false;
  }};
  // visibility scans address whole map cells: the high byte discards the word's eight sub-cell bits
  auto const camera_column{static_cast<uint8_t>(view.column >> maths::world_format::cell_fraction_bits)};
  auto const camera_row{static_cast<uint8_t>(view.row >> maths::world_format::cell_fraction_bits)};
  if(view.underground && !view.unrestricted_visibility) {
    visit_tunnel_cells(cells, camera_column, camera_row, tunnel_visibility, place);
  } else {
    tunnel_visibility.fill(0);
    collect_city_cells(cells, camera_column, camera_row, view.angles, view.radius, candidates, cull_scale > 1);
    for(auto const index : candidates) {
      if(view.radius <= scene_limits::native_surface_radius_cells) {
        place(index);
        continue;
      }
      visit_world_images(camera_column, camera_row,
        index % game::city_map_size.column, index / game::city_map_size.column, view.radius,
        [&](world_image const image) {
          if(auto item{place_city_cell(bank, cells[index], index, damage_mask, basis, camera, image, cull_scale)}) items.push_back(*item);
        });
    }
  }
  if(particles) {
    auto const append{[&](auto const &emitters){
      for(auto const &emitter : emitters | std::views::reverse) {
        auto const phase{game::particle_phase(emitter, particles->clock)};
        if(!phase) continue;
        // 2F35 admits effects through the same wrapping coordinate window as moving objects
        if(!within_object_window(view, emitter.position)) continue;
        auto const place_emitter{[&](std::optional<world_image> const image) {
          auto const placement{place_model(basis, camera, {
            .column{emitter.position.column},
            .row{emitter.position.row},
            .height{std::bit_cast<int16_t>(static_cast<uint16_t>(-emitter.position.height))}
          }, image)};
          items.push_back({
            .placement{placement},
            .emitter{&emitter},
            .phase{*phase}
          });
        }};
        if(view.radius <= scene_limits::native_surface_radius_cells || (view.underground && !view.unrestricted_visibility)) place_emitter(std::nullopt);
        else visit_world_images(camera_column, camera_row,
          emitter.position.column >> maths::world_format::cell_fraction_bits,
          emitter.position.row >> maths::world_format::cell_fraction_bits, view.radius, place_emitter);
      }
    }};
    append(particles->effects.trails);
    append(particles->effects.emitters);
  }
  // 2BC1 scans the city, 6BF3 adds particles, then 2BD0/2BEA adds craft and projectiles
  for(auto const &object : objects) {
    if(!within_object_window(view, object.pose.position)) continue;
    auto const place_object{[&](std::optional<world_image> const image) {
      if(auto item{place_scene_object(bank, object, basis, camera, view.underground, image, cull_scale)}) items.push_back(*item);
    }};
    if(view.radius <= scene_limits::native_surface_radius_cells || (view.underground && !view.unrestricted_visibility)) place_object(std::nullopt);
    else visit_world_images(camera_column, camera_row,
      object.pose.position.column >> maths::world_format::cell_fraction_bits,
      object.pose.position.row >> maths::world_format::cell_fraction_bits, view.radius, place_object);
  }
  order_city_models(items);
  for(auto const &item : items) {
    if(item.emitter) {
      for(auto const point : project_emitter(*item.emitter, item.placement, basis, view.origin, view.focal_length)) {
        draw_particle(target, particles->sheet, point, item.phase, view.viewport, view.focal_length);
      }
      continue;
    }
    if(item.distant_point) {
      if(auto const point{project_distant_object(item.placement, view.origin, view.viewport, item.projection_residue, view.focal_length)}) {
        auto const colour{bank.header_at(item.model_offset).point_colour};
        // 2D71 reuses the last mesh's shade table; it does not calculate distance or fade again
        target.pixels[static_cast<size_t>(point->y) * target.stride + point->x] = retained_colours.point_colour(colour);
      }
      continue;
    }
    projection_parameters const projection{
      .axes{item.orientation.value_or(basis)},
      .horizontal{item.placement.horizontal},
      .vertical{item.placement.vertical},
      .depth{item.placement.depth},
      .origin{screen_vertex{view.origin}},
      .focal_length{view.focal_length},
    };
    uint8_t light{item.orientation ? item.object_light : uint8_t{255}};
    if(view.beacon_lighting && !item.orientation) {
      // 2D85 samples the nearest lattice cell's state without the charging routine's type check
      auto const column{((item.cell % game::city_map_size.column + game::beacon_spacing_cells / 2) / game::beacon_spacing_cells) * game::beacon_spacing_cells};
      auto const row{((item.cell / game::city_map_size.column + game::beacon_spacing_cells / 2) / game::beacon_spacing_cells) * game::beacon_spacing_cells};
      light = cells[game::city_cell_index(column, row)].state;
    }
    auto const colours{lighting.colours(item.placement.depth.whole, item.path, light)};
    retained_colours = colours;
    animation.cell_state = item.orientation ? 0 : cells[item.cell].state;
    draw_model(workspace, target, bank.model_pool(), item.model_offset, projection, colours, view.viewport, item.path, animation,
      view.gouraud ? model_shading::gouraud : model_shading::flat,
      item.orientation ? 0 : view.minimum_light_radius);
  }
  return items.size();
}

} // namespace darker::graphics
