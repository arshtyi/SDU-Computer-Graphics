#include "bezier.hpp"
#include <cmath>
#include <stdexcept>

namespace curve {
namespace {

void validate(const ControlPoints &points, double t) {
    if (points.empty() || points.size() > max_control_points)
        throw std::invalid_argument("Expected 1 to 64 control points.");
    if (!std::isfinite(t) || t < 0.0 || t > 1.0)
        throw std::invalid_argument("Parameter t must be finite and in [0, 1].");
    for (const auto &point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            throw std::invalid_argument("Control point coordinates must be finite.");
    }
}

cv::Point2d reduce(const ControlPoints &points, double t) {
    if (points.size() == 1)
        return points.front();
    ControlPoints next;
    next.reserve(points.size() - 1);
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        next.push_back((1.0 - t) * points[i] + t * points[i + 1]);
    return reduce(next, t);
}

using Evaluator = cv::Point2d (*)(const ControlPoints &, double);

void draw_samples(const ControlPoints &points, cv::Mat &window, Evaluator evaluate, int channel) {
    validate(points, 0.0);
    if (window.empty() || window.type() != CV_8UC3)
        throw std::invalid_argument("Canvas must be a nonempty CV_8UC3 image.");
    for (int i = 0; i <= sample_segments; ++i) {
        const auto point = evaluate(points, static_cast<double>(i) / sample_segments);
        if (point.x < 0 || point.x > window.cols - 1 || point.y < 0 || point.y > window.rows - 1)
            continue;
        window.at<cv::Vec3b>(cvRound(point.y), cvRound(point.x))[channel] = 255;
    }
}

} // namespace

cv::Point2d recursive_bezier(const ControlPoints &points, double t) {
    validate(points, t);
    if (t == 0.0)
        return points.front();
    if (t == 1.0)
        return points.back();
    return reduce(points, t);
}

cv::Point2d bernstein_bezier(const ControlPoints &points, double t) {
    validate(points, t);
    if (t == 0.0)
        return points.front();
    if (t == 1.0)
        return points.back();
    const int degree = static_cast<int>(points.size()) - 1;
    double binomial = 1.0;
    cv::Point2d result(0.0, 0.0);
    for (int i = 0; i <= degree; ++i) {
        const double weight = binomial * std::pow(1.0 - t, degree - i) * std::pow(t, i);
        result += weight * points[i];
        if (i < degree)
            binomial *= static_cast<double>(degree - i) / (i + 1);
    }
    return result;
}

void bezier(const ControlPoints &points, cv::Mat &window) { draw_samples(points, window, recursive_bezier, 1); }

void naive_bezier(const ControlPoints &points, cv::Mat &window) { draw_samples(points, window, bernstein_bezier, 2); }

void draw_curve(const ControlPoints &points, cv::Mat &window, Algorithm algorithm) {
    switch (algorithm) {
    case Algorithm::Casteljau:
        bezier(points, window);
        break;
    case Algorithm::Bernstein:
        naive_bezier(points, window);
        break;
    case Algorithm::Both:
        naive_bezier(points, window);
        bezier(points, window);
        break;
    default:
        throw std::invalid_argument("Unknown curve algorithm.");
    }
}

} // namespace curve
