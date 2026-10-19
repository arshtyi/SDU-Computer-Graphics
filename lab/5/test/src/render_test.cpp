#include "rasterizer.hpp"
#include "scene.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
void near(float actual, float expected, const std::string &message, float epsilon = 1e-5f) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= epsilon,
            message + ": actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}
void near(const Vec3 &actual, const Vec3 &expected, const std::string &message, float epsilon = 1e-5f) {
    require(actual.allFinite() && (actual - expected).norm() <= epsilon, message);
}
template <class Function> void throws(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid input must throw");
}
Triangle projected(const Vec3 &w, const Vec3 &color = Vec3::Ones()) {
    Triangle t;
    t[0].position = {-w[0], -w[0], -w[0]};
    t[1].position = {w[1], -w[1], -w[1]};
    t[2].position = {-w[2], w[2], -w[2]};
    for (auto &v : t)
        v.color = color;
    t[0].uv = {0, 0};
    t[1].uv = {1, 0};
    t[2].uv = {0, 1};
    return t;
}
rst::rasterizer camera(int size = 8) {
    rst::rasterizer r(size, size);
    r.set_projection(get_projection_matrix(90, 1, 0.1f, 10));
    r.set_fragment_shader([](const fragment_shader_payload &p) -> Vec3 { return p.color * 100; });
    return r;
}
void projection() {
    const Mat4 p = get_projection_matrix(60, 2, 0.1f, 50);
    const Vec4 n = p * Vec4(0, 0, -0.1f, 1), f = p * Vec4(0, 0, -50, 1);
    near(n.z() / n.w(), -1, "Near plane");
    near(f.z() / f.w(), 1, "Far plane");
    near(p(1, 1) / p(0, 0), 2, "Aspect ratio");
    near(n.w(), 0.1f, "Clip w is -view z");
    throws([] { get_projection_matrix(0, 1, .1f, 50); });
    throws([] { get_projection_matrix(45, 1, 0, 50); });
    throws([] { get_projection_matrix(45, 1, 2, 1); });
    rst::rasterizer r(8, 8);
    Triangle t = projected(Vec3::Ones());
    for (auto &v : t) {
        v.position *= .4f;
        v.normal = unit({1, 1, 1});
    }
    Mat4 model = Mat4::Identity();
    model(0, 0) = 2;
    model(1, 1) = 3;
    model(2, 2) = .5f;
    r.set_model(model);
    bool checked = false;
    r.set_fragment_shader([&](const fragment_shader_payload &payload) -> Vec3 {
        near(payload.normal, unit({.5f, 1.f / 3, 2}), "Inverse-transpose normal");
        checked = true;
        return Vec3::Ones();
    });
    r.draw({t});
    require(checked, "Normal transform must shade pixels");
}
void texture() {
    cv::Mat bgr(2, 2, CV_8UC3);
    bgr.at<cv::Vec3b>(0, 0) = {0, 0, 255};
    bgr.at<cv::Vec3b>(0, 1) = {0, 255, 0};
    bgr.at<cv::Vec3b>(1, 0) = {255, 0, 0};
    bgr.at<cv::Vec3b>(1, 1) = {255, 255, 255};
    Texture t(bgr);
    near(t.getColor(0, 1), {255, 0, 0}, "RGB and vertical origin");
    near(t.getColor(1, 0), {255, 255, 255}, "Last texel");
    near(t.getColorBilinear(.5f, .5f), Vec3::Constant(127.5f), "Four-texel average");
    near(t.getColorBilinear(.25f, .75f), {159.375f, 63.75f, 63.75f}, "Bilinear unequal weights");
    near(t.getColorBilinear(-2, 4), {255, 0, 0}, "Clamp-to-edge");
    near(t.getColor(.49f, .51f), {255, 0, 0}, "Nearest is nearest, not floor");
    Texture single(cv::Mat(1, 1, CV_8UC3, cv::Scalar(12, 34, 56)));
    near(single.getColorBilinear(1, 0), {56, 34, 12}, "1x1 texture");
    Texture strip(cv::Mat(1, 4, CV_8UC3, cv::Scalar(5, 6, 7)));
    near(strip.getColorBilinear(.7f, 1), {7, 6, 5}, "1xN texture");
    throws([] { Texture empty{cv::Mat()}; });
}
fragment_shader_payload payload() { return {{0, 0, -1}, {.2f, .4f, .6f}, Vec3::UnitZ(), {.5f, .5f}, {2, 0, 0}, {0, 3, 0}}; }
void lighting() {
    auto p = payload();
    ShaderSettings s;
    s.lights = {{{0, 0, 0}, Vec3::Constant(2)}};
    near(blinn_phong(p, p.color, s), 255 * (Vec3::Constant(.05f) + 2 * (p.color + Vec3::Constant(.7937f))), "Analytic front lighting", .0002f);
    s.lights.push_back({{0, 0, 0}, Vec3::Zero()});
    near(blinn_phong(p, p.color, s), 255 * (Vec3::Constant(.05f) + 2 * (p.color + Vec3::Constant(.7937f))), "Ambient counted once", .0002f);
    s.lights = {{{0, 0, 1}, Vec3::Constant(2)}};
    near(blinn_phong(p, p.color, s), 255 * (Vec3::Constant(.05f) + .5f * (p.color + Vec3::Constant(.7937f))), "Inverse-square falloff", .0002f);
    s.lights = {{{0, 0, -2}, Vec3::Constant(2)}};
    near(blinn_phong(p, p.color, s), Vec3::Constant(12.75f), "Back light has no diffuse or specular");
    Texture red(cv::Mat(1, 1, CV_8UC3, cv::Scalar(0, 0, 255)));
    s.texture = &red;
    s.lights = {{{0, 0, 0}, Vec3::Ones()}};
    near(texture_fragment_shader(p, s), blinn_phong(p, {1, 0, 0}, s), "Texture supplies kd");
}
void bump() {
    Texture flat(cv::Mat(3, 3, CV_8UC3, cv::Scalar(128, 128, 128)));
    auto p = payload();
    ShaderSettings s;
    s.height_map = &flat;
    s.height_scale = .5f;
    near(perturbed_normal(p, s), p.normal, "Constant height preserves normal");
    cv::Mat ramp(3, 3, CV_8UC3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            ramp.at<cv::Vec3b>(y, x) = cv::Vec3b::all(x * 127);
    Texture slope(ramp);
    s.height_map = &slope;
    near(perturbed_normal(p, s), unit({-.25f * 254 / 255, 0, 1}), "Analytic UV height derivative");
    p.dpdv = -p.dpdv;
    near(perturbed_normal(p, s), unit({-.25f * 254 / 255, 0, 1}), "Mirrored UV orientation");
    for (const Vec3 &normal : {Vec3(0, 1, 0), Vec3(0, -1, 0)}) {
        p.normal = normal;
        p.dpdu.setZero();
        p.dpdv.setZero();
        p.tex_coords = {1, 0};
        near(perturbed_normal(p, s).norm(), 1, "Pole fallback stays finite and normalized");
    }
}
void interpolation() {
    auto r = camera();
    auto t = projected({1, 2, 4});
    t[0].color = Vec3::UnitX();
    t[1].color = Vec3::UnitY();
    t[2].color = Vec3::UnitZ();
    t[0].normal = unit({1, 0, 1});
    t[1].normal = unit({0, 1, 1});
    t[2].normal = Vec3::UnitZ();
    const Vec3 weights = Vec3(40, 6, 3) / 49.f;
    const Vec3 expected_position = weights[0] * t[0].position + weights[1] * t[1].position + weights[2] * t[2].position;
    bool checked = false;
    r.set_fragment_shader([&](const fragment_shader_payload &p) -> Vec3 {
        if ((p.view_pos - expected_position).norm() < 1e-5f) {
            checked = true;
            near(p.tex_coords.x(), 6.f / 49, "Perspective-correct U");
            near(p.tex_coords.y(), 3.f / 49, "Perspective-correct V");
            near(p.normal, unit(weights[0] * t[0].normal + weights[1] * t[1].normal + weights[2] * t[2].normal), "Interpolated normal");
        }
        return p.color * 100;
    });
    r.draw({t});
    require(checked, "Expected shading position observed");
    near(r.frame_buffer()[6 * 8 + 1], weights * 100, "Perspective-correct color");
    const auto projection = get_projection_matrix(90, 1, .1f, 10);
    float expected_z = 0;
    const Vec3 bary(.625f, .1875f, .1875f);
    for (int i = 0; i < 3; ++i) {
        const Vec4 c = projection * Vec4(t[i].position.x(), t[i].position.y(), t[i].position.z(), 1);
        expected_z += bary[i] * c.z() / c.w();
    }
    near(r.depth_buffer()[6 * 8 + 1], (expected_z + 1) / 2, "Depth uses screen-linear NDC z");
}
void depth() {
    auto r = camera();
    auto a = projected({1, 4, 4}, Vec3::UnitX()), b = projected(Vec3::Constant(2), Vec3::UnitY());
    r.draw({a, b});
    const auto first = r.frame_buffer();
    near(first[6 * 8 + 1], {100, 0, 0}, "Near part of slanted triangle");
    near(first[6 * 8 + 5], {0, 100, 0}, "Far part behind constant-depth triangle");
    r.clear();
    r.draw({b, a});
    for (size_t i = 0; i < first.size(); ++i)
        near(r.frame_buffer()[i], first[i], "Depth is draw-order independent");
}
void clipping() {
    auto r = camera(32);
    auto t = projected(Vec3::Ones());
    t[0].position = {-.2f, -.2f, .2f};
    r.draw({t});
    require(r.statistics().covered_pixels > 0, "Near-plane crossing remains visible");
    for (float depth : r.depth_buffer())
        if (std::isfinite(depth))
            require(depth >= 0 && depth <= 1, "Clipped depth range");
    r.clear();
    for (auto &v : t)
        v.position.z() = 1;
    r.draw({t});
    require(r.statistics().covered_pixels == 0, "Fully behind camera rejected");
    r.clear();
    for (auto &v : t)
        v.position.z() = -11;
    r.draw({t});
    require(r.statistics().covered_pixels == 0, "Beyond far plane rejected");
    r.clear();
    t = projected(Vec3::Ones());
    for (auto &v : t)
        v.position.x() += 10;
    r.draw({t});
    require(r.statistics().covered_pixels == 0, "Outside side plane rejected");
    r.clear();
    t[1] = t[0];
    t[2] = t[0];
    r.draw({t});
    require(r.statistics().covered_pixels == 0, "Degenerate triangle rejected");
}
void coverage() {
    rst::rasterizer r(8, 8);
    Triangle a, b;
    a[0].position = {-1, -1, 0};
    a[1].position = {1, -1, 0};
    a[2].position = {1, 1, 0};
    b[0].position = {-1, -1, 0};
    b[1].position = {1, 1, 0};
    b[2].position = {-1, 1, 0};
    r.draw({a});
    const auto da = r.depth_buffer();
    r.clear();
    r.draw({b});
    const auto db = r.depth_buffer();
    for (size_t i = 0; i < da.size(); ++i)
        require(std::isfinite(da[i]) != std::isfinite(db[i]), "Shared edge belongs to exactly one triangle; all border pixels covered");
    r.clear();
    std::swap(a[1], a[2]);
    r.draw({a});
    for (size_t i = 0; i < da.size(); ++i)
        require(std::isfinite(da[i]) == std::isfinite(r.depth_buffer()[i]), "Both windings supported");
}
void displacement() {
    Texture flat(cv::Mat(1, 1, CV_8UC3, cv::Scalar(255, 255, 255)));
    ShaderSettings s;
    s.height_map = &flat;
    s.height_scale = .5f;
    const auto p = payload();
    near(displacement_vertex_shader({p.view_pos, p.normal, p.tex_coords}, s), {0, 0, -.5f}, "Vertex position actually displaced");
    s.lights = {{{0, 0, 0}, Vec3::Ones()}};
    auto moved = p;
    moved.view_pos.z() = -.5f;
    near(displacement_fragment_shader(p, s), blinn_phong(moved, moved.color, s), "Framework fragment displacement lighting");
    auto t = projected(Vec3::Constant(2));
    for (auto &v : t) {
        v.position.x() *= .5f;
        v.position.y() *= .5f;
    }
    auto r = camera(32);
    r.draw({t});
    const auto base = r.statistics().covered_pixels;
    const auto depth_before = r.depth_buffer();
    r.clear();
    r.set_fragment_shader(make_shader(Shader::DisplacementFragment, s));
    r.draw({t});
    require(r.statistics().covered_pixels == base && depth_before == r.depth_buffer(), "Fragment displacement keeps coverage and depth");
    r.clear();
    r.set_fragment_shader(make_shader(Shader::Displacement, s));
    r.set_vertex_shader([&](const vertex_shader_payload &v) { return displacement_vertex_shader(v, s); });
    r.draw({t});
    require(r.statistics().covered_pixels > base, "Geometric displacement changes silhouette");
    require(r.depth_buffer()[20 * 32 + 12] < depth_before[20 * 32 + 12], "Geometric displacement updates depth");
    require(subdivide({t}, 2).size() == 16, "Two subdivision levels multiply faces by 16");
}
void models() {
    for (const auto &[path, count] : std::vector<std::pair<std::string, size_t>>{{"models/spot/spot_triangulated_good.obj", 5856},
                                                                                 {"models/rock/rock.obj", 192},
                                                                                 {"models/cube/cube.obj", 12},
                                                                                 {"models/Crate/Crate1.obj", 12}}) {
        auto mesh = load_obj(path);
        require(mesh.size() == count, "OBJ triangulation and index count: " + path);
        for (const auto &t : mesh)
            for (const auto &v : t)
                near(v.normal.norm(), 1, "Model vertex normals normalized");
        fit_mesh(mesh);
        for (const auto &t : mesh)
            for (const auto &v : t)
                require(v.position.cwiseAbs().maxCoeff() <= 1.00001f, "Fit bounds");
    }
    throws([] { load_obj("missing.obj"); });
}
} // namespace
int main(int argc, char **argv) {
    const std::map<std::string, std::function<void()>> tests{
        {"projection", projection},       {"texture", texture}, {"lighting", lighting}, {"bump", bump},
        {"interpolation", interpolation}, {"depth", depth},     {"clipping", clipping}, {"coverage", coverage},
        {"displacement", displacement},   {"models", models}};
    try {
        if (argc != 2 || !tests.count(argv[1]))
            throw std::invalid_argument("Choose one registered test group");
        tests.at(argv[1])();
        std::cout << "PASS " << argv[1] << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
