#include "maths/render_sine.h"
#include <array>
#include <cmath>
#include <numbers>

namespace darker::maths {
namespace {

auto const sine_table{[] {
  std::array<int16_t, angle_format::full_turn> result{};
  double constexpr amplitude{32767};
  // Mirror one quadrant exactly, guaranteeing symmetry and exact cardinal angles.
  for(unsigned int angle{0}; angle <= angle_format::quarter_turn; ++angle) {
    auto const value{static_cast<int16_t>(std::lround(amplitude * std::sin(angle * 2 * std::numbers::pi / angle_format::full_turn)))};
    result[angle] = value;
    result[angle_format::half_turn - angle] = value;
    result[static_cast<uint16_t>(angle_format::half_turn + angle)] = static_cast<int16_t>(-value);
    result[static_cast<uint16_t>(angle_format::full_turn - angle)] = static_cast<int16_t>(-value);
  }
  return result;
}()};

} // anonymous namespace

int16_t render_sine(uint16_t const angle) noexcept {
  return sine_table[angle];
}

int16_t render_cosine(uint16_t const angle) noexcept {
  return sine_table[static_cast<uint16_t>(angle + angle_format::quarter_turn)];
}

} // namespace darker::maths
