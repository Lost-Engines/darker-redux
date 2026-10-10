#include <catch2/catch_test_macros.hpp>
#include <array>
#include <filesystem>
#include <string_view>
#include "audio/midi_music.h"
#include "resources/archive_set.h"
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "audio/roland_patches.h"

TEST_CASE("Roland setup MIDI extracts checked DT1 data and rejects damaged banks", "[audio]") {
  std::array<uint8_t, 35> const midi{
    'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xe0,
    'M', 'T', 'r', 'k', 0, 0, 0, 13,
    0, 0xf0, 10, 0x41, 0x10, 0x16, 0x12, 3, 1, 0x10, 127, 109, 0xf7
  };
  auto const bytes{std::as_bytes(std::span{midi})};
  auto const messages{darker::audio::roland_setup_messages(bytes)};
  REQUIRE(messages.size() == 1);
  REQUIRE(messages.front() == std::vector<uint8_t>{0xf0, 0x41, 0x10, 0x16, 0x12, 3, 1, 0x10, 127, 109, 0xf7});
  for(size_t length{0}; length < bytes.size(); ++length) {
    REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(bytes.first(length)), std::invalid_argument);
  }
  auto damaged{midi};
  damaged[33] ^= 1;
  REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(std::as_bytes(std::span{damaged})), std::invalid_argument);
  damaged = midi;
  damaged[23] = 0x90;
  REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(std::as_bytes(std::span{damaged})), std::invalid_argument);
}

TEST_CASE("Repaired Roland opening keeps its melodic part aligned on repeated loops", "[audio]") {
  if(std::string_view{DARKER_TEST_RESOURCE_DIR}.empty()) SKIP("Requires retail music resources");
  auto song{darker::resources::read_binary_file(std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / "00_040.bin", 65536)};
  darker::audio::repair_roland_opening(song);
  auto const repaired{song};
  darker::audio::repair_roland_opening(song);
  REQUIRE(song == repaired);
  darker::audio::midi_music music{darker::audio::music_variant::lapc1};
  std::vector<unsigned int> fantasy_changes;
  unsigned int clock{0};
  auto const sink{[&](darker::audio::midi_message const message) {
    if(message.status == 0xc7 && message.first == 32) fantasy_changes.push_back(clock);
  }};
  music.start(song, sink);
  for(; clock < 20000; ++clock) music.advance(sink);
  REQUIRE(fantasy_changes.size() >= 6);
  // 9168 sequence ticks at the original 96-PPQN / 98-BPM tempo: each
  // recurrence must stay within one scheduler call of the common loop.
  auto const word{[&](unsigned int offset) {
    return std::to_integer<unsigned int>(song[offset]) | (std::to_integer<unsigned int>(song[offset + 1]) << 8);
  }};
  auto const descriptor{word(word(1))};
  unsigned int const increment{static_cast<unsigned int>((static_cast<uint64_t>(std::to_integer<unsigned int>(song[descriptor]))
    * std::to_integer<unsigned int>(song[descriptor + 1]) << 16) / (0x04446390 / 0x5d24))};
  for(unsigned int i{1}; i < fantasy_changes.size(); ++i) {
    auto const actual{fantasy_changes[i] - fantasy_changes[0]};
    auto const expected{static_cast<unsigned int>((uint64_t{9168} * i << 16) / increment)};
    REQUIRE(actual >= expected);
    REQUIRE(actual <= expected + 1);
  }
}
