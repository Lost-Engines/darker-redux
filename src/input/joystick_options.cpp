#include "input/joystick_options.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace darker::input {
namespace {
int numbered(std::string_view const value, int const limit, std::string_view const option) {
  int result{};
  auto const parsed{std::from_chars(value.data(), value.data() + value.size(), result)};
  if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < 1 || result > limit) {
    throw std::invalid_argument{std::string{option} + " requires a number from 1 to " + std::to_string(limit)};
  }
  return result - 1;
}
} // anonymous namespace

void add_joystick_options(boost::program_options::options_description &options) {
  namespace po = boost::program_options;
  std::string button_help{"bind ACTION to button 1..128 or off; repeat for multiple actions. Actions: "};
  for(auto const &description : control_descriptions()) {
    if(button_help.back() != ' ') button_help += ", ";
    button_help += description.name;
  }
  options.add_options()
    ("joystick", po::value<std::string>(), "auto (first available), off, or device 1..16; default: saved choice or auto")
    ("joystick-bank-axis", po::value<std::string>(), "bank axis 1..32 (default: saved choice or 1)")
    ("joystick-pitch-axis", po::value<std::string>(), "pitch axis 1..32 (default: saved choice or 2)")
    ("joystick-invert-bank", po::value<bool>()->implicit_value(true), "invert bank axis; use =false to override a saved inversion")
    ("joystick-invert-pitch", po::value<bool>()->implicit_value(true), "invert pitch axis; use =false to override a saved inversion")
    ("joystick-deadzone", po::value<double>(), "joystick centre dead zone in percent, 0..50 (original/default: 6.25); 0 disables")
    ("joystick-sensitivity", po::value<unsigned int>(), "axis sensitivity in percent, 25..200 (default: 100)")
    ("joystick-button", po::value<std::vector<std::string>>()->composing(), button_help.c_str());
}

joystick_preferences apply_joystick_options(joystick_preferences const &base,
  boost::program_options::variables_map const &arguments) {
  auto result{base};
  if(arguments.contains("joystick")) {
    auto const &name{arguments["joystick"].as<std::string>()};
    result.device = name == "auto" ? joystick_automatic : name == "off" ? joystick_disabled : numbered(name, 16, "--joystick (auto/off/device)");
  }
  if(arguments.contains("joystick-bank-axis")) result.bank_axis = numbered(arguments["joystick-bank-axis"].as<std::string>(), 32, "--joystick-bank-axis");
  if(arguments.contains("joystick-pitch-axis")) result.pitch_axis = numbered(arguments["joystick-pitch-axis"].as<std::string>(), 32, "--joystick-pitch-axis");
  if(arguments.contains("joystick-invert-bank")) result.invert_bank = arguments["joystick-invert-bank"].as<bool>();
  if(arguments.contains("joystick-invert-pitch")) result.invert_pitch = arguments["joystick-invert-pitch"].as<bool>();
  if(arguments.contains("joystick-deadzone")) result.deadzone_percent = arguments["joystick-deadzone"].as<double>();
  if(arguments.contains("joystick-sensitivity")) result.sensitivity_percent = arguments["joystick-sensitivity"].as<unsigned int>();
  if(result.bank_axis == result.pitch_axis) throw std::invalid_argument{"Joystick bank and pitch axes must differ; specify both options when swapping axes"};
  if(!std::isfinite(result.deadzone_percent) || result.deadzone_percent < 0 || result.deadzone_percent > 50) throw std::invalid_argument{"--joystick-deadzone must be between 0 and 50 percent"};
  if(result.sensitivity_percent < 25 || result.sensitivity_percent > 200) throw std::invalid_argument{"--joystick-sensitivity must be between 25 and 200 percent"};
  auto const descriptions{control_descriptions()};
  if(arguments.contains("joystick-button")) {
    for(auto const &assignment : arguments["joystick-button"].as<std::vector<std::string>>()) {
      auto const equals{assignment.find('=')};
      if(equals == std::string::npos) throw std::invalid_argument{"--joystick-button requires ACTION=NUMBER or ACTION=off"};
      auto const action{std::string_view{assignment}.substr(0, equals)};
      auto const found{std::ranges::find(descriptions, action, &control_description::name)};
      if(found == descriptions.end()) throw std::invalid_argument{"Unknown joystick action: " + std::string{action} + "; see --help"};
      auto const button{std::string_view{assignment}.substr(equals + 1)};
      result.buttons[static_cast<unsigned int>(found - descriptions.begin())] = button == "off" ? -1 : numbered(button, joystick_button_limit, "--joystick-button");
    }
  }
  for(unsigned int i{0}; i < result.buttons.size(); ++i) {
    if(result.buttons[i] < 0) continue;
    for(unsigned int j{0}; j < i; ++j) {
      if(result.buttons[i] == result.buttons[j]) {
        throw std::invalid_argument{"Joystick button " + std::to_string(result.buttons[i] + 1) + " is assigned to both "
          + std::string{descriptions[j].name} + " and " + std::string{descriptions[i].name} + "; reassign one or set it to off"};
      }
    }
  }
  if(!result.valid()) throw std::invalid_argument{"Invalid joystick settings"};
  return result;
}

} // namespace darker::input
