#pragma once

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "input/joystick.h"

namespace darker::input {

void add_joystick_options(boost::program_options::options_description &options);
joystick_preferences apply_joystick_options(joystick_preferences const &base,
  boost::program_options::variables_map const &arguments);

} // namespace darker::input
