#include "bezier.hpp"
#include "control_points.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <opencv2/imgcodecs.hpp>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

void near(cv::Point2d actual, cv::Point2d expected, const std::string &message, double tolerance = 1e-9) {
    require(cv::norm(actual - expected) <= tolerance, message);
}

void rejects(const std::function<void()> &operation, const std::string &message) {
    try {
        operation();
    } catch (const std::invalid_argument &) {
        return;
    }
    throw std::runtime_error(message);
}

curve::ControlPoints load(const std::filesystem::path &directory, const std::string &name) {
    std::ifstream input(directory / (name + ".in"));
    require(input.good(), "Missing fixture: " + name);
    return curve::read_control_points(input, {700, 700});
}

void evaluation(const std::filesystem::path &directory) {
    const auto cubic = load(directory, "cubic");
    for (const auto evaluate : {curve::recursive_bezier, curve::bernstein_bezier}) {
        near(evaluate(cubic, 0), cubic.front(), "Start endpoint", 0);
        near(evaluate(cubic, 1), cubic.back(), "End endpoint", 0);
        near(evaluate(cubic, 0.5), {350, 231.25}, "Cubic analytical midpoint");
        near(evaluate({{10, 20}, {110, 220}}, 0.25), {35, 70}, "Linear interpolation");
        near(evaluate({{0, 0}, {2, 4}, {4, 0}}, 0.5), {2, 2}, "Quadratic midpoint");
        near(evaluate({{7, 9}}, 0.37), {7, 9}, "Constant curve");
        near(evaluate(curve::ControlPoints(64, {32, 48}), 0.37), {32, 48}, "64 repeated points");
        near(evaluate(load(directory, "six_points"), 0.5), {334.375, 350}, "Degree-five analytical midpoint");
        auto reversed = cubic;
        std::reverse(reversed.begin(), reversed.end());
        near(evaluate(reversed, 0.2), evaluate(cubic, 0.8), "Reversal identity");
        auto transformed = cubic;
        for (auto &point : transformed)
            point = 2.5 * point + cv::Point2d(-17, 23);
        near(evaluate(transformed, 0.37), 2.5 * evaluate(cubic, 0.37) + cv::Point2d(-17, 23), "Affine invariance");
        rejects([&] { evaluate({}, 0.5); }, "Empty points accepted");
        rejects([&] { evaluate(curve::ControlPoints(65), 0.5); }, "Too many points accepted");
        rejects([&] { evaluate(cubic, -0.01); }, "Negative parameter accepted");
        rejects([&] { evaluate(cubic, 1.01); }, "Parameter above one accepted");
        rejects([&] { evaluate(cubic, std::numeric_limits<double>::quiet_NaN()); }, "NaN parameter accepted");
        rejects([&] { evaluate(cubic, std::numeric_limits<double>::infinity()); }, "Infinite parameter accepted");
        rejects([&] { evaluate({{std::numeric_limits<double>::infinity(), 0}}, 0.5); }, "Nonfinite coordinate accepted");
    }

    std::mt19937 random(42);
    double maximum_error = 0;
    std::size_t comparisons = 0;
    for (const int count : {1, 2, 3, 4, 6, 16, 32, 64}) {
        curve::ControlPoints points;
        for (int i = 0; i < count; ++i)
            points.emplace_back(random() % 700, random() % 700);
        for (int sample = 0; sample <= 1000; ++sample) {
            const double t = sample / 1000.0;
            const auto a = curve::recursive_bezier(points, t);
            const auto b = curve::bernstein_bezier(points, t);
            const double error = cv::norm(a - b);
            maximum_error = std::max(maximum_error, error);
            require(error < 1e-9, "Algorithms disagree");
            require(a.x >= -1e-9 && a.x <= 699 + 1e-9 && a.y >= -1e-9 && a.y <= 699 + 1e-9, "Convex hull bounding box");
            ++comparisons;
        }
    }
    std::cout << "comparisons=" << comparisons << " maximum_error=" << std::scientific << maximum_error << '\n';
}

void rasterization(const std::filesystem::path &directory) {
    for (const std::string name : {"cubic", "s_curve", "six_points", "boundary", "repeated", "line", "single"}) {
        const auto points = load(directory, name);
        cv::Mat green(700, 700, CV_8UC3, cv::Scalar(0));
        cv::Mat red = green.clone();
        cv::Mat both = green.clone();
        curve::bezier(points, green);
        curve::naive_bezier(points, red);
        curve::draw_curve(points, both, curve::Algorithm::Both);
        int occupied = 0, yellow = 0;
        for (int y = 0; y < both.rows; ++y) {
            for (int x = 0; x < both.cols; ++x) {
                const auto g = green.at<cv::Vec3b>(y, x);
                const auto r = red.at<cv::Vec3b>(y, x);
                const auto b = both.at<cv::Vec3b>(y, x);
                require(g[0] == 0 && g[2] == 0 && r[0] == 0 && r[1] == 0, "Incorrect BGR channel");
                require(b == cv::Vec3b(0, g[1], r[2]), "Overlay overwrote another channel");
                occupied += b[1] != 0 || b[2] != 0;
                yellow += b == cv::Vec3b(0, 255, 255);
            }
        }
        require(occupied > 0 && static_cast<double>(yellow) / occupied > 0.98, "Insufficient curve overlap: " + name);
        for (const auto point : {points.front(), points.back()})
            require(both.at<cv::Vec3b>(cvRound(point.y), cvRound(point.x)) == cv::Vec3b(0, 255, 255), "Endpoint pixel missing");
        std::cout << name << ": occupied=" << occupied << " yellow=" << yellow << '\n';
    }
    cv::Mat tiny(2, 2, CV_8UC3, cv::Scalar(0));
    curve::draw_curve({{-10, -10}, {10, 10}}, tiny, curve::Algorithm::Both);
    require(tiny.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 255, 255), "Clipped curve should cross origin");
    require(tiny.at<cv::Vec3b>(1, 1) == cv::Vec3b(0, 255, 255), "Clipped curve should cross last pixel");
    curve::bezier({{1e100, -1e100}}, tiny);
    cv::Mat empty;
    cv::Mat wrong_type(2, 2, CV_32FC3);
    rejects([&] { curve::bezier({{0, 0}}, empty); }, "Empty canvas accepted");
    rejects([&] { curve::bezier({{0, 0}}, wrong_type); }, "Wrong canvas type accepted");
    std::filesystem::create_directories("build/test");
    const std::string path = "build/test/pixel_roundtrip.png";
    require(cv::imwrite(path, tiny), "PNG write failed");
    const auto restored = cv::imread(path);
    require(restored.size() == tiny.size() && restored.type() == tiny.type() && cv::norm(restored, tiny, cv::NORM_INF) == 0,
            "PNG pixels changed after saving");
}

void input(const std::filesystem::path &directory) {
    require(load(directory, "cubic").size() == 4, "Cubic fixture count");
    require(load(directory, "six_points").size() == 6, "Six-point fixture count");
    for (const std::string name : {"empty", "invalid_count", "too_many", "incomplete", "outside", "nonfinite", "trailing"})
        rejects([&] { load(directory, name); }, "Invalid fixture accepted: " + name);
    std::istringstream fractional("2\n0.25 1.5\n698.75 699\n");
    const auto points = curve::read_control_points(fractional, {700, 700});
    near(points.front(), {0.25, 1.5}, "Fractional coordinate lost");
    std::istringstream negative("1\n-1 0\n");
    rejects([&] { curve::read_control_points(negative, {700, 700}); }, "Negative coordinate accepted");
    std::istringstream good("1 0 0");
    rejects([&] { curve::read_control_points(good, {0, 700}); }, "Invalid canvas dimensions accepted");
}

} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 3, "Usage: BezierTests evaluation|rasterization|input data-directory");
        const std::string group = argv[1];
        if (group == "evaluation")
            evaluation(argv[2]);
        else if (group == "rasterization")
            rasterization(argv[2]);
        else if (group == "input")
            input(argv[2]);
        else
            throw std::invalid_argument("Unknown test group");
        std::cout << "PASS " << group << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
