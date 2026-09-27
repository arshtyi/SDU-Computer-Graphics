#pragma once

#include "bezier.hpp"
#include <istream>

namespace curve {

ControlPoints read_control_points(std::istream &input, cv::Size canvas);

} // namespace curve
