#include "game/city_repairs.h"
#include <array>
#include <bit>
#include <stdexcept>
#include <vector>

namespace darker::game {
namespace {

bool repairable_service(uint8_t const type) noexcept {
  /// Radar masts and hangar entrances recover from player damage; ordinary buildings retain their ruins
  uint8_t constexpr radar_mast{12};
  std::array<uint8_t, 4> constexpr hangar_entrances{17, 19, 21, 23};
  if(type == radar_mast) return true;
  for(auto const entrance : hangar_entrances) {
    if(type == entrance) return true;
  }
  return false;
}

void add_objectives(city_cell_set &targets, std::span<resources::scenario_cell_list const> lists, unsigned int first) {
  /// FF ends the active objective chain; a mere authorisation list is not a destruction requirement
  for(auto index{first}; index < lists.size(); ++index) {
    for(auto const &cell : lists[index].cells) targets.set(city_cell_index(cell.column, cell.row));
    if(lists[index].terminator != 254) break;
  }
}

} // anonymous namespace

void delphi_collateral_damage::record_player_destruction(unsigned int const index, city_cell const &before) {
  /// Record only a new player-caused destruction, preserving damage already supplied by enemies or the scenario
  if(before.state & delphi_damage_bit) return;
  if(index == headquarters_cell && before.type == 17) headquarters_destroyed = true;
  else if(repairable_service(before.type)) service_repairs.set(index);
}

void delphi_collateral_damage::repair_services(city_map &cells) {
  /// Repair player-damaged services before committing a completed mission; retain authorisation and model variants
  for(unsigned int index{0}; index < cells.size(); ++index) {
    if(service_repairs[index]) cells[index].state &= static_cast<uint8_t>(~delphi_damage_bit);
  }
  service_repairs.reset();
}

void delphi_collateral_damage::repair_return_hangar(city_map &cells, uint16_t const site) {
  /// A repaired destination admits landing in this mission too; the site token stores twice the column
  auto const index{city_cell_index((site & 255) >> 1, site >> 8)};
  if(service_repairs.test(index)) {
    cells.at(index).state &= static_cast<uint8_t>(~delphi_damage_bit);
    service_repairs.reset(index);
  }
}

bool delphi_collateral_damage::headquarters_lost() const noexcept {
  /// Hemmersan is the one player-caused hangar loss that ends the current attempt
  return headquarters_destroyed;
}

city_cell_set scripted_destruction_targets(resources::scenario_record const &record, std::span<std::byte const> const program) {
  /// Collect cell dependencies without running scripts, consuming messages, activating enemies or changing the world
  city_cell_set targets;
  add_objectives(targets, record.cell_lists, record.objective_cell_list);
  std::vector<size_t> roots;
  if(record.player_program) roots.push_back(*record.player_program);
  for(auto const &group : record.groups) {
    for(auto const &actor : group.objects) {
      if(actor.form == resources::placement_form::moving && actor.program_offset) roots.push_back(*actor.program_offset);
    }
  }
  // fixed operand widths of the original gameplay opcodes 00..38; variable lists and the altitude toggle are handled below
  static std::array<unsigned int, 57> constexpr operand_sizes{
    2, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 1,
    3, 0, 2, 2, 0, 2, 2, 2, 0, 0, 0, 1, 2, 3, 0, 1,
    1, 0, 1, 0, 0, 1, 0, 1, 3, 2, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 0, 0, 1, 1, 0,
  };
  std::vector<std::array<bool, 2>> visited(program.size());
  for(auto const root : roots) {
    if(root < record.shared.offset || root - record.shared.offset >= program.size()) throw std::invalid_argument{"Building repair script starts outside its shared section"};
    auto cursor{root - record.shared.offset};
    bool altitude_hold{false};
    auto const byte{[&] {
      if(cursor >= program.size()) throw std::invalid_argument{"Building repair script is truncated"};
      return std::to_integer<uint8_t>(program[cursor++]);
    }};
    auto const cell_list{[&] {
      resources::scenario_cell_list list;
      for(;;) {
        auto const column{byte()};
        if(column >= city_map_size.column) {
          list.terminator = column;
          return list;
        }
        auto const row{byte()};
        if(row >= city_map_size.row) throw std::invalid_argument{"Building repair target lies outside the city"};
        list.cells.push_back({
          .column{column},
          .row{row},
        });
      }
    }};
    while(cursor < program.size() && !visited[cursor][altitude_hold]) {
      visited[cursor][altitude_hold] = true;
      auto const opcode{byte()};
      if(opcode >= 128) continue;                                              // one-byte animation parameter
      if(opcode == 0x08 || opcode == 0x23 || opcode == 0x24) break;              // retirement, stop, or retained actor owner
      if(opcode == 0x25) {
        auto const displacement{std::bit_cast<int8_t>(byte())};
        auto const destination{static_cast<ptrdiff_t>(cursor) + displacement};
        if(destination < 0 || static_cast<size_t>(destination) >= program.size()) throw std::invalid_argument{"Building repair branch leaves its shared section"};
        cursor = static_cast<size_t>(destination);
      } else if(opcode == 0x1d) {                                             // wait for a cell's type/state flags
        auto const column{byte()};
        auto const row{byte()};
        auto const mask{byte()};
        auto const tested_bits{mask & 128 ? static_cast<uint8_t>(~mask) : mask};
        if(row >= city_map_size.row) throw std::invalid_argument{"Building repair wait lies outside the city"};
        if((column & 1) && (tested_bits & delphi_damage_bit)) targets.set(city_cell_index(column >> 1, row));
      } else if(opcode == 0x34) {                                             // replace marked and unmarked building objective lists
        std::array const lists{cell_list(), cell_list()};
        add_objectives(targets, lists, lists[0].terminator == 254 ? 0 : 1);
      } else if(opcode == 0x35) {
        cell_list();                                                         // aircraft spawning sites are not destruction objectives
      } else if(opcode == 0x38) {
        altitude_hold = !altitude_hold;
        if(altitude_hold) {
          byte();
          byte();
        }
      } else {
        if(opcode >= operand_sizes.size()) throw std::invalid_argument{"Unexpected presentation opcode in building repair script"};
        for(unsigned int i{0}; i < operand_sizes[opcode]; ++i) byte();
        // exchange-context (26) retains this continuation for the return from the supplementary program
      }
    }
  }
  return targets;
}

void repair_scenario_buildings(city_map &cells, resources::scenario_record const &record, std::span<std::byte const> const program) {
  /// Restore required Delphi buildings before applying this mission's explicit damage and target flags
  if((record.configuration & 15) > 1) return;
  auto const targets{scripted_destruction_targets(record, program)};
  for(unsigned int index{0}; index < cells.size(); ++index) {
    if(targets[index] && cells[index].type != delphi_beacon_type) cells[index].state &= static_cast<uint8_t>(~delphi_damage_bit);
  }
}

} // namespace darker::game
