#include "bezier.hpp"
#include "control_points.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>

namespace {

constexpr int canvas_size = 700;
constexpr char window_name[] = "Bezier Curve";

struct Options {
    std::string input;
    std::string output = "build/my_bezier_curve.png";
    curve::Algorithm algorithm = curve::Algorithm::Casteljau;
    int count = 4;
    bool help = false;
};

void usage() {
    std::cout << "Usage: BezierCurve [--count N] [--algorithm casteljau|bernstein|both] [--output image.png]\n"
              << "       BezierCurve --input points.in [--algorithm casteljau|bernstein|both] [--output image.png]\n"
              << "No --input: click N control points (default 4). File input renders without a window.\n"
              << "Keys: 1 Casteljau (green), 2 Bernstein (red), 3 both (yellow), R reset, S save, Esc quit.\n";
}

Options parse_options(int argc, char **argv) {
    Options options;
    bool count_supplied = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            options.help = true;
            continue;
        }
        if (argument != "--input" && argument != "--output" && argument != "--algorithm" && argument != "--count")
            throw std::invalid_argument("Unknown option: " + argument);
        if (++i == argc)
            throw std::invalid_argument("Missing value for " + argument);
        const std::string value = argv[i];
        if (value.empty())
            throw std::invalid_argument("Empty value for " + argument);
        if (argument == "--input")
            options.input = value;
        else if (argument == "--output")
            options.output = value;
        else if (argument == "--algorithm") {
            if (value == "casteljau")
                options.algorithm = curve::Algorithm::Casteljau;
            else if (value == "bernstein")
                options.algorithm = curve::Algorithm::Bernstein;
            else if (value == "both")
                options.algorithm = curve::Algorithm::Both;
            else
                throw std::invalid_argument("Algorithm must be casteljau, bernstein, or both.");
        } else {
            std::size_t consumed = 0;
            options.count = std::stoi(value, &consumed);
            if (consumed != value.size() || options.count < 1 || options.count > static_cast<int>(curve::max_control_points))
                throw std::invalid_argument("Control point count must be an integer from 1 to 64.");
            count_supplied = true;
        }
    }
    if (count_supplied && !options.input.empty())
        throw std::invalid_argument("--count is only used for mouse input; files provide their own count.");
    return options;
}

cv::Mat render(const curve::ControlPoints &points, curve::Algorithm algorithm, bool complete) {
    cv::Mat image(canvas_size, canvas_size, CV_8UC3, cv::Scalar(0, 0, 0));
    for (const auto &point : points)
        cv::circle(image, cv::Point(cvRound(point.x), cvRound(point.y)), 3, {255, 255, 255}, 2);
    if (complete)
        curve::draw_curve(points, image, algorithm);
    return image;
}

void save_image(const cv::Mat &image, const std::string &filename) {
    const auto parent = std::filesystem::path(filename).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    if (!cv::imwrite(filename, image))
        throw std::runtime_error("Failed to save image: " + filename);
    std::cout << "Saved " << filename << '\n';
}

struct MouseState {
    curve::ControlPoints points;
    int count;
    bool dirty = true;
};

void mouse_handler(int event, int x, int y, int, void *userdata) {
    auto &state = *static_cast<MouseState *>(userdata);
    if (event == cv::EVENT_LBUTTONDOWN && state.points.size() < static_cast<std::size_t>(state.count) && x >= 0 && x < canvas_size && y >= 0 &&
        y < canvas_size) {
        state.points.emplace_back(x, y);
        state.dirty = true;
        std::cout << "Point " << state.points.size() << ": (" << x << ", " << y << ")\n";
    }
}

void interactive(Options options) {
    MouseState state{{}, options.count};
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(window_name, mouse_handler, &state);
    usage();
    cv::Mat image;
    while (true) {
        const bool complete = state.points.size() == static_cast<std::size_t>(state.count);
        if (state.dirty) {
            image = render(state.points, options.algorithm, complete);
            cv::imshow(window_name, image);
            if (complete)
                save_image(image, options.output);
            state.dirty = false;
        }
        const int key = cv::waitKey(20);
        if (key == 27 || cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1)
            break;
        if (key == 'r' || key == 'R') {
            state.points.clear();
            state.dirty = true;
        } else if (key >= '1' && key <= '3') {
            options.algorithm = key == '1' ? curve::Algorithm::Casteljau : key == '2' ? curve::Algorithm::Bernstein : curve::Algorithm::Both;
            state.dirty = true;
        } else if ((key == 's' || key == 'S') && complete) {
            save_image(image, options.output);
        }
    }
    cv::destroyAllWindows();
}

} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) {
            usage();
        } else if (!options.input.empty()) {
            std::ifstream input(options.input);
            if (!input)
                throw std::runtime_error("Cannot open input file: " + options.input);
            const auto points = curve::read_control_points(input, {canvas_size, canvas_size});
            save_image(render(points, options.algorithm, true), options.output);
        } else {
            interactive(options);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
