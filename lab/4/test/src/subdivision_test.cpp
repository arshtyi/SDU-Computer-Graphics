#include "mesh.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace subdivision;
namespace {
void check(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
void near(Point actual, Point expected, const std::string &message) {
    check(std::abs(actual.x - expected.x) < 1e-11 && std::abs(actual.y - expected.y) < 1e-11, message);
}
void rejects(const std::function<void()> &action, const std::string &message) {
    try {
        action();
    } catch (const std::invalid_argument &) {
        return;
    }
    throw std::runtime_error("Accepted invalid input: " + message);
}
Mesh parse(const std::string &text) {
    std::istringstream input(text);
    return read_obj(input);
}
Mesh load(const std::filesystem::path &directory, const std::string &name) {
    std::ifstream input(directory / (name + ".obj"));
    check(bool(input), "Cannot open fixture: " + name);
    return read_obj(input);
}
std::size_t edge_id(const Topology &t, std::size_t a, std::size_t b) {
    for (std::size_t e = 0; e < t.edges.size(); ++e)
        if (t.edges[e].vertices == std::array<std::size_t, 2>{std::min(a, b), std::max(a, b)})
            return e;
    throw std::runtime_error("Missing edge.");
}
void stencils(const std::filesystem::path &directory) {
    const auto mesh = load(directory, "required"), next = catmull_clark(mesh);
    const auto t = build_topology(mesh);
    const auto offset = mesh.vertices.size() + t.edges.size();
    near(next.vertices[offset], {.5, .5}, "Quad face average");
    near(next.vertices[offset + 4], {5.0 / 3, 4.0 / 3}, "Triangle face average (not divided by four)");
    near(next.vertices[0], {.125, .125}, "Boundary corner 6/8, 1/8, 1/8");
    near(next.vertices[1], {1, 0}, "Straight boundary preserved");
    near(next.vertices[5], {1.06, 1.06}, "Valence-five vertex uses denominator 25, not 20");
    near(next.vertices[10], {1.94, 1.94}, "Other valence-five vertex");
    near(next.vertices[6], {193.0 / 96, 95.0 / 96}, "Valence-four vertex next to triangle");
    near(next.vertices[16 + edge_id(t, 0, 1)], {.5, 0}, "Boundary edge midpoint");
    near(next.vertices[16 + edge_id(t, 5, 10)], {1.5, 1.5}, "Interior diagonal edge");
    near(next.vertices[16 + edge_id(t, 5, 6)], {37.0 / 24, 23.0 / 24}, "Interior edge beside triangle");
    const auto fan = catmull_clark(load(directory, "fan3"));
    near(fan.vertices[0], {4.0 / 45, 2.0 / 45}, "Asymmetric valence-three vertex");
    const auto square = catmull_clark(load(directory, "quad"));
    near(square.vertices[0], {.25, .25}, "Single face boundary corner");
    near(square.vertices.back(), {1, 1}, "Square face point");
    std::cout << "13 analytic coordinate checks passed.\n";
}
void topology(const std::filesystem::path &directory) {
    const std::size_t expected[4][4] = {{16, 25, 10, 12}, {51, 88, 38, 24}, {177, 328, 152, 48}, {657, 1264, 608, 96}};
    for (const std::string name : {"required", "warped", "quad", "pentagon", "fan3", "ring"}) {
        auto mesh = load(directory, name);
        for (int level = 0; level <= 3; ++level) {
            const auto t = build_topology(mesh);
            const auto chi =
                static_cast<long long>(mesh.vertices.size()) - static_cast<long long>(t.edges.size()) + static_cast<long long>(mesh.faces.size());
            check(chi == (name == "ring" ? 0 : 1), "Euler characteristic changed");
            if (name == "required")
                check(mesh.vertices.size() == expected[level][0] && t.edges.size() == expected[level][1] && mesh.faces.size() == expected[level][2] &&
                          t.boundary_edges() == expected[level][3],
                      "Required mesh counts");
            if (level > 0)
                for (const auto &face : mesh.faces)
                    check(face.size() == 4, "Non-quad after subdivision");
            if (level == 3)
                break;
            const auto next = catmull_clark(mesh);
            const auto nt = build_topology(next);
            std::size_t corners = 0;
            for (const auto &face : mesh.faces)
                corners += face.size();
            check(next.vertices.size() == mesh.vertices.size() + t.edges.size() + mesh.faces.size(), "V' = V + E + F");
            check(nt.edges.size() == 2 * t.edges.size() + corners && next.faces.size() == corners, "E', F' identities");
            check(nt.boundary_edges() == 2 * t.boundary_edges(), "Boundary edges double");
            mesh = next;
        }
    }
    std::cout << "Six meshes, four levels: manifold topology, quad faces, counts and Euler invariants passed.\n";
}
void invariance(const std::filesystem::path &directory) {
    auto original = load(directory, "warped"), transformed = original, reordered = original;
    const auto transform = [](Point p) { return Point{2 * p.x + .3 * p.y + 10, -.4 * p.x + 1.5 * p.y - 7}; };
    for (auto &p : transformed.vertices)
        p = transform(p);
    std::reverse(reordered.faces.begin(), reordered.faces.end());
    for (auto &f : reordered.faces)
        std::reverse(f.begin(), f.end());
    auto a = catmull_clark(original), b = catmull_clark(transformed), c = catmull_clark(reordered);
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
        near(b.vertices[i], transform(a.vertices[i]), "Affine invariance");
    const auto sort_points = [](Mesh &m) {
        std::sort(m.vertices.begin(), m.vertices.end(), [](Point p, Point q) { return p.x == q.x ? p.y < q.y : p.x < q.x; });
    };
    sort_points(a);
    sort_points(c);
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
        near(a.vertices[i], c.vertices[i], "Face order / winding independence");
    std::cout << "Affine transform and face-order/winding invariance passed.\n";
}
void io(const std::filesystem::path &directory) {
    auto mesh = load(directory, "required");
    for (int i = 0; i < 4; ++i) {
        std::ostringstream out;
        write_obj(mesh, out);
        const auto copy = parse(out.str());
        check(copy.faces == mesh.faces && copy.vertices.size() == mesh.vertices.size(), "Roundtrip topology");
        for (std::size_t v = 0; v < mesh.vertices.size(); ++v)
            check(copy.vertices[v].x == mesh.vertices[v].x && copy.vertices[v].y == mesh.vertices[v].y, "Lossless double roundtrip");
        if (i < 3)
            mesh = catmull_clark(mesh);
    }
    const auto negative = parse("# comment\nv 0 0 0\nv 1 0\nv 0 1\nf -3 -2 -1 # negative indices\n");
    check(negative.faces[0] == Face{0, 1, 2}, "Negative OBJ indices");
    const auto lines = parse("v 0 0\nv 1 0\nv 0 1\nl 1 2\nl 2 3\nl 3 1\nf 1 2 3\n");
    check(build_topology(lines).edges.size() == 3, "Standard l edge records");
    std::cout << "Four levels roundtrip exactly; negative indices, 2D/3D planar vertices and l records passed.\n";
}
void invalid(const std::filesystem::path &directory) {
    const std::string vertices = "v 0 0\nv 1 0\nv 1 1\nv 0 1\n";
    const std::vector<std::string> inputs = {"",
                                             "v nan 0\n",
                                             "v 1e309 0\n",
                                             "v 0 0 1\n",
                                             "v 0 0 extra\n",
                                             "v 0\n",
                                             "v 0 0 0 1\n",
                                             vertices + "e 1 2\n",
                                             vertices + "f 0 2 3 4\n",
                                             vertices + "f 1 2 3 5\n",
                                             vertices + "f 1 2\n",
                                             vertices + "f 1 2 2 4\n",
                                             vertices + "f 1 2 3 4\nf 4 3 2 1\n",
                                             vertices + "f 1 3 2 4\n",
                                             vertices + "f 1 2 3\nf 1 4 3\n",
                                             vertices + "f 1 2 3 4\ne 1 3\n",
                                             vertices + "f 1 2 3 4\ne 1 2\ne 2 1\n",
                                             vertices + "f 1 2x 3 4\n",
                                             vertices + "f 1 2 3 4\nbanana\n",
                                             vertices + "f 1 2 3\n",
                                             "v 0 0\nv 1 0\nv 2 0\nf 1 2 3\n",
                                             "v 0 0\nv 0 0\nv 1 1\nf 1 2 3\n",
                                             "v 0 0\nv 1 0\nv 0 1\nv -1 0\nv 0 -1\nf 1 2 3\nf 1 4 5\n",
                                             "v 0 0\nv 1 0\nv 0 1\nv 0 -1\nv 1 1\nf 1 2 3\nf 2 1 4\nf 1 2 5\n"};
    for (std::size_t i = 0; i < inputs.size(); ++i)
        rejects([&] { parse(inputs[i]); }, "case " + std::to_string(i));
    Mesh bowtie{{{0, 0}, {1, 0}, {0, 1}, {-.3, -.4}, {2, 0}, {0, 2}, {-.6, -.8}},
                {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}, {0, 4, 5}, {0, 6, 4}, {0, 5, 6}, {4, 6, 5}}};
    rejects([&] { build_topology(bowtie); }, "Disconnected closed fans");
    auto bad = load(directory, "quad");
    bad.vertices[0].x = std::numeric_limits<double>::infinity();
    rejects([&] { catmull_clark(bad); }, "Nonfinite in-memory vertex");
    std::cout << inputs.size() + 2 << " malformed input / topology checks passed.\n";
}
void render(const std::filesystem::path &directory) {
    const auto mesh = load(directory, "required"), next = catmull_clark(mesh);
    const auto a = render_mesh(mesh, mesh, 0), b = render_mesh(next, mesh, 1, &mesh);
    check(a.rows == 800 && a.cols == 800 && a.type() == CV_8UC3, "Canvas format");
    check(cv::norm(a, b, cv::NORM_L1) > 10000, "Iterations must draw different images");
    const auto overview = render_overview({a, b});
    check(overview.rows == 800 && overview.cols == 1600, "Overview layout");
    const std::string path = "build/test/render/roundtrip.png";
    save_image(b, path);
    check(cv::norm(cv::imread(path), b, cv::NORM_INF) == 0, "PNG is readable and lossless");
    check(a.at<cv::Vec3b>(707, 106) != cv::Vec3b(255, 255, 255), "Origin vertex drawn at correct position");
    std::cout << "Canvas, level differences, overview, PNG roundtrip and coordinate mapping passed.\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        check(argc == 3, "Usage: SubdivisionTests group data-directory");
        const std::string group = argv[1];
        const std::filesystem::path directory = argv[2];
        if (group == "stencils")
            stencils(directory);
        else if (group == "topology")
            topology(directory);
        else if (group == "invariance")
            invariance(directory);
        else if (group == "io")
            io(directory);
        else if (group == "invalid")
            invalid(directory);
        else if (group == "render")
            render(directory);
        else
            throw std::invalid_argument("Unknown test group.");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
