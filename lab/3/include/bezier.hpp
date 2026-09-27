#pragma once

#include <cstddef>
#include <opencv2/core.hpp>
#include <vector>

namespace curve {

using ControlPoints = std::vector<cv::Point2d>;
constexpr std::size_t max_control_points = 64;
constexpr int sample_segments = 1000;

enum class Algorithm { Casteljau, Bernstein, Both };

cv::Point2d recursive_bezier(const ControlPoints &points, double t);
cv::Point2d bernstein_bezier(const ControlPoints &points, double t);

void bezier(const ControlPoints &points, cv::Mat &window);
void naive_bezier(const ControlPoints &points, cv::Mat &window);
void draw_curve(const ControlPoints &points, cv::Mat &window, Algorithm algorithm);

} // namespace curve
