#include "rasterizer.hpp"
#include "scene.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {
struct Options {
    std::string output = "output.png", model = "models/spot/spot_triangulated_good.obj";
    std::string texture = "models/spot/spot_texture.png", height = "models/spot/hmap.jpg";
    std::string shader = "phong", stats, mask;
    Filter filter = Filter::Bilinear;
    int size = 700, subdivisions = 2;
    float angle = 140, scale = 2.5f, height_scale = 0.1f;
    bool fit = false, interactive = false;
};
void help() {
    std::cout << "Usage: Rasterizer [output.png] [shader] [options]\n"
                 "  --shader normal|phong|texture|bump|bump-phong|displacement|displacement-fragment\n"
                 "  --output PATH       Render one frame (default output.png)\n"
                 "  --model PATH        OBJ model, paths relative to lab/5\n"
                 "  --texture PATH      Color texture; --height-map PATH selects the height image\n"
                 "  --filter nearest|bilinear (default bilinear)\n"
                 "  --angle DEGREES     Rotation around Y (default 140)\n"
                 "  --scale VALUE       Uniform model scale (default 2.5)\n"
                 "  --height-scale N    View-space displacement distance (default 0.1)\n"
                 "  --subdivide 0..3    Midpoint refinement for geometric displacement (default 2)\n"
                 "  --size 1..4096      Square image size (default 700)\n"
                 "  --fit               Center model and fit its longest dimension to 2\n"
                 "  --stats PATH        Write rendering statistics as CSV\n"
                 "  --mask PATH         Save actual geometric coverage as a PNG\n"
                 "  --interactive       A/D rotate, S saves, Esc exits\n";
}
int integer(const std::string &s) {
    size_t used = 0;
    const int value = std::stoi(s, &used);
    if (used != s.size())
        throw std::invalid_argument("Invalid integer: " + s);
    return value;
}
float number(const std::string &s) {
    size_t used = 0;
    const float value = std::stof(s, &used);
    if (used != s.size() || !std::isfinite(value))
        throw std::invalid_argument("Invalid finite number: " + s);
    return value;
}
Options parse(int argc, char **argv) {
    Options o;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 == argc)
                throw std::invalid_argument("Missing value for " + arg);
            return argv[++i];
        };
        if (arg == "--output")
            o.output = value();
        else if (arg == "--model")
            o.model = value();
        else if (arg == "--texture")
            o.texture = value();
        else if (arg == "--height-map")
            o.height = value();
        else if (arg == "--shader")
            o.shader = value();
        else if (arg == "--stats")
            o.stats = value();
        else if (arg == "--mask")
            o.mask = value();
        else if (arg == "--angle")
            o.angle = number(value());
        else if (arg == "--scale")
            o.scale = number(value());
        else if (arg == "--height-scale")
            o.height_scale = number(value());
        else if (arg == "--size")
            o.size = integer(value());
        else if (arg == "--subdivide")
            o.subdivisions = integer(value());
        else if (arg == "--fit")
            o.fit = true;
        else if (arg == "--interactive")
            o.interactive = true;
        else if (arg == "--filter") {
            const auto filter = value();
            if (filter != "nearest" && filter != "bilinear")
                throw std::invalid_argument("Unknown filter: " + filter);
            o.filter = filter == "bilinear" ? Filter::Bilinear : Filter::Nearest;
        } else if (arg.rfind("--", 0) == 0) {
            throw std::invalid_argument("Unknown option: " + arg);
        } else if (positional == 0) {
            o.output = arg;
            ++positional;
        } else if (positional == 1) {
            o.shader = arg;
            ++positional;
        } else {
            throw std::invalid_argument("Too many positional arguments");
        }
    }
    if (o.size < 1 || o.size > 4096 || o.scale <= 0 || o.scale > 100 || o.height_scale < 0 || o.height_scale > 5 || o.subdivisions < 0 ||
        o.subdivisions > 3)
        throw std::invalid_argument("Size, scale, height scale, or subdivision is out of range");
    return o;
}
void prepare_parent(const std::string &path) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
}
void save(const std::string &path, const cv::Mat &image) {
    prepare_parent(path);
    if (!cv::imwrite(path, image))
        throw std::runtime_error("Could not save image: " + path);
}
} // namespace

int main(int argc, char **argv) {
    try {
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--help") {
                help();
                return 0;
            }
        auto o = parse(argc, argv);
        const Shader shader = parse_shader(o.shader);
        auto mesh = load_obj(o.model);
        if (o.fit)
            fit_mesh(mesh);
        if (shader == Shader::Displacement)
            mesh = subdivide(mesh, o.subdivisions);

        std::unique_ptr<Texture> color_map, height_map;
        if (shader == Shader::Texture)
            color_map = std::make_unique<Texture>(o.texture);
        if (shader == Shader::Bump || shader == Shader::BumpPhong || shader == Shader::Displacement || shader == Shader::DisplacementFragment)
            height_map = std::make_unique<Texture>(o.height);
        const Mat4 view = get_view_matrix({0, 0, 10});
        ShaderSettings settings;
        settings.texture = color_map.get();
        settings.height_map = height_map.get();
        settings.filter = o.filter;
        settings.height_scale = o.height_scale;
        for (const Vec3 &position : {Vec3(20, 20, 20), Vec3(-20, 20, 0)}) {
            const Vec4 p = view * Vec4(position.x(), position.y(), position.z(), 1);
            settings.lights.push_back({p.head<3>(), Vec3::Constant(500)});
        }
        rst::rasterizer rasterizer(o.size, o.size);
        rasterizer.set_view(view);
        rasterizer.set_projection(get_projection_matrix(45, 1, 0.1f, 50));
        rasterizer.set_fragment_shader(make_shader(shader, settings));
        if (shader == Shader::Displacement)
            rasterizer.set_vertex_shader([settings](const vertex_shader_payload &p) { return displacement_vertex_shader(p, settings); });

        const auto render = [&]() {
            rasterizer.clear();
            rasterizer.set_model(get_model_matrix(o.angle, o.scale));
            const auto start = std::chrono::steady_clock::now();
            rasterizer.draw(mesh);
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            const auto stats = rasterizer.statistics();
            std::cout << "shader=" << o.shader << " triangles=" << stats.input_triangles << " covered=" << stats.covered_pixels
                      << " render_ms=" << milliseconds << '\n';
            if (!o.stats.empty()) {
                prepare_parent(o.stats);
                std::ofstream out(o.stats);
                out << "shader,triangles,raster_triangles,shaded_fragments,covered_pixels,render_ms\n"
                    << o.shader << ',' << stats.input_triangles << ',' << stats.raster_triangles << ',' << stats.shaded_fragments << ','
                    << stats.covered_pixels << ',' << milliseconds << '\n';
                if (!out)
                    throw std::runtime_error("Could not write statistics");
            }
        };
        render();
        save(o.output, rasterizer.image());
        if (!o.mask.empty())
            save(o.mask, rasterizer.mask());
        if (o.interactive) {
            for (;;) {
                cv::imshow("Lab 5 - " + o.shader, rasterizer.image());
                const int key = cv::waitKey(0);
                if (key == 27)
                    break;
                if (key == 's')
                    save(o.output, rasterizer.image());
                if (key == 'a' || key == 'd') {
                    o.angle += key == 'a' ? -5.f : 5.f;
                    render();
                }
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Rasterizer: " << error.what() << '\n';
        return 1;
    }
}
