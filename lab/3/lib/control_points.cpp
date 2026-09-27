#include "control_points.hpp"
#include <cmath>
#include <stdexcept>
#include <string>

namespace curve {

ControlPoints read_control_points(std::istream &input, cv::Size canvas) {
    if (canvas.width <= 0 || canvas.height <= 0)
        throw std::invalid_argument("Canvas dimensions must be positive.");
    int count = 0;
    if (!(input >> count) || count < 1 || count > static_cast<int>(max_control_points))
        throw std::invalid_argument("Input must start with a control point count from 1 to 64.");
    ControlPoints points;
    for (int i = 0; i < count; ++i) {
        cv::Point2d point;
        if (!(input >> point.x >> point.y) || !std::isfinite(point.x) || !std::isfinite(point.y))
            throw std::invalid_argument("Expected finite x y coordinates for every control point.");
        if (point.x < 0 || point.x > canvas.width - 1 || point.y < 0 || point.y > canvas.height - 1)
            throw std::invalid_argument("Control point is outside the canvas.");
        points.push_back(point);
    }
    std::string extra;
    if (input >> extra)
        throw std::invalid_argument("Unexpected data after the control points.");
    return points;
}

} // namespace curve
