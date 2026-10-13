#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "vectorstorm/vector/vector2.h"

namespace darker::input {

enum class control_action : unsigned int {
  left,
  right,
  up,
  down,
  control,
  brake,
  boost,
  engine,
  altitude,
  slow,
  fast,
  primary,
  secondary,
  look,
  radar,
  cockpit,
  following,
  following_alternate,
  fullscreen,
  drop_camera,
  free_camera,
  pick_camera,
  camera_nearer,
  camera_farther,
  missile_camera,
  clear_target,
  gouraud,
  fov_narrower,
  fov_wider,
  weapon_1,
  weapon_2,
  weapon_3,
  weapon_5,
  weapon_6,
  weapon_8,
  weapon_9,
  weapon_0,
  count
};

struct control_description {
  std::string_view name;
  std::string_view label;
  int default_key;
};

std::span<control_description const> control_descriptions() noexcept;
int normalise_key(int key) noexcept;
bool bindable_key(int key) noexcept;
std::string key_label(int key);

struct control_preferences {
  double mouse_sensitivity{1.0};
  bool invert_mouse{false};
  int primary_mouse_button{0};
  int secondary_mouse_button{1};
  std::array<int, static_cast<unsigned int>(control_action::count)> keys;

  control_preferences();
  bool valid() const noexcept;
  std::optional<control_action> action_for_key(int key) const noexcept;
  int command_key(int physical_key) const noexcept;
  int binding_for_command(int command_key) const noexcept;
  bool operator==(control_preferences const&) const = default;
};

class mouse_motion {
  bool started{false};
  vec2<double> previous;
  vec2<double> accumulated;

public:
  void reset() noexcept;
  vec2<uint16_t> sample(double x, double y, control_preferences const &preferences) noexcept;
};

} // namespace darker::input
