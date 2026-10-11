#include "mesh.hpp"
#include "render.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <stdexcept>
#include <string>

namespace {
struct Options {
    std::string input = "test/data/required.obj", output = "build/results/required";
    int levels = 3;
    bool show = false, help = false;
};
void usage() {
    std::cout << "Usage: CatmullClark [--input mesh.obj] [--levels 0..6] [--output directory] [--show]\n"
                 "Default: required mesh, 3 subdivisions, headless export to build/results/required.\n"
                 "Exports level_N.obj, level_N.png, overview.png and stats.csv (including level 0).\n"
                 "Window keys: N/Space/+ next; P/- previous; R reset; 0..6 select; Esc/Q quit.\n";
}
Options parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
            options.help = true;
        else if (arg == "--show")
            options.show = true;
        else {
            if (arg != "--input" && arg != "--output" && arg != "--levels")
                throw std::invalid_argument("Unknown option: " + arg);
            if (++i == argc || std::string(argv[i]).empty())
                throw std::invalid_argument("Missing value for " + arg);
            const std::string value = argv[i];
            if (arg == "--input")
                options.input = value;
            else if (arg == "--output")
                options.output = value;
            else {
                std::size_t used = 0;
                options.levels = std::stoi(value, &used);
                if (used != value.size() || options.levels < 0 || options.levels > 6)
                    throw std::invalid_argument("Levels must be an integer from 0 to 6.");
            }
        }
    }
    return options;
}
void show(const std::vector<cv::Mat> &frames) {
    const char *window = "Catmull-Clark | N: next, P: previous, R: reset, Esc: quit";
    cv::namedWindow(window, cv::WINDOW_AUTOSIZE);
    int level = 0;
    bool dirty = true;
    while (true) {
        if (dirty) {
            cv::imshow(window, frames[level]);
            dirty = false;
        }
        const int key = cv::waitKey(20);
        if (key == 27 || key == 'q' || key == 'Q' || cv::getWindowProperty(window, cv::WND_PROP_VISIBLE) < 1)
            break;
        const int old = level;
        if (key == ' ' || key == '+' || key == 'n' || key == 'N')
            level = std::min(level + 1, static_cast<int>(frames.size()) - 1);
        else if (key == '-' || key == 'p' || key == 'P')
            level = std::max(level - 1, 0);
        else if (key == 'r' || key == 'R')
            level = 0;
        else if (key >= '0' && key <= '6' && key - '0' < static_cast<int>(frames.size()))
            level = key - '0';
        dirty = old != level;
    }
    cv::destroyAllWindows();
}
} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse(argc, argv);
        if (options.help) {
            usage();
            return 0;
        }
        std::ifstream input(options.input);
        if (!input)
            throw std::runtime_error("Cannot open input: " + options.input);
        std::vector<subdivision::Mesh> meshes{subdivision::read_obj(input)};
        for (int level = 1; level <= options.levels; ++level)
            meshes.push_back(subdivision::catmull_clark(meshes.back()));
        const std::filesystem::path directory(options.output);
        for (std::size_t level = 0; level < meshes.size(); ++level)
            if (std::filesystem::weakly_canonical(options.input) ==
                std::filesystem::weakly_canonical(directory / ("level_" + std::to_string(level) + ".obj")))
                throw std::invalid_argument("Output would overwrite the input OBJ; choose another output directory.");
        std::filesystem::create_directories(directory);
        std::ofstream stats(directory / "stats.csv");
        if (!stats)
            throw std::runtime_error("Cannot write stats.csv.");
        const std::string header = "level,vertices,edges,faces,boundary_edges,euler\n";
        stats << header;
        std::cout << header;
        std::vector<cv::Mat> frames;
        for (std::size_t level = 0; level < meshes.size(); ++level) {
            const auto &mesh = meshes[level];
            const auto topology = subdivision::build_topology(mesh);
            const auto euler = static_cast<long long>(mesh.vertices.size()) - static_cast<long long>(topology.edges.size()) +
                               static_cast<long long>(mesh.faces.size());
            const std::string row = std::to_string(level) + "," + std::to_string(mesh.vertices.size()) + "," + std::to_string(topology.edges.size()) +
                                    "," + std::to_string(mesh.faces.size()) + "," + std::to_string(topology.boundary_edges()) + "," +
                                    std::to_string(euler) + "\n";
            stats << row;
            std::cout << row;
            const auto stem = directory / ("level_" + std::to_string(level));
            std::ofstream obj(stem.string() + ".obj");
            subdivision::write_obj(mesh, obj);
            obj.close();
            if (!obj)
                throw std::runtime_error("Cannot finish OBJ output.");
            frames.push_back(subdivision::render_mesh(mesh, meshes.front(), level, level == 0 ? nullptr : &meshes[level - 1]));
            subdivision::save_image(frames.back(), stem.string() + ".png");
        }
        stats.close();
        if (!stats)
            throw std::runtime_error("Cannot finish stats.csv.");
        subdivision::save_image(subdivision::render_overview(frames), (directory / "overview.png").string());
        std::cout << "Saved results to " << directory.string() << '\n';
        if (options.show) {
            usage();
            show(frames);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
