#include "render.hpp"
#include <algorithm>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace subdivision {
namespace {
constexpr int size = 800;
const cv::Scalar ink(65, 55, 42), boundary(160, 79, 35), interior(160, 138, 103);
const cv::Scalar edge_color(102, 137, 35), face_color(45, 132, 224), ghost(221, 224, 228);
void label(cv::Mat &image, const std::string &text, cv::Point position, double scale = 0.65, cv::Scalar color = ink) {
    cv::putText(image, text, position, cv::FONT_HERSHEY_SIMPLEX, scale, color, 1, cv::LINE_AA);
}
} // namespace

cv::Mat render_mesh(const Mesh &mesh, const Mesh &original, std::size_t level, const Mesh *previous) {
    const auto topology = build_topology(mesh), base = build_topology(original);
    double min_x = original.vertices[0].x, max_x = min_x, min_y = original.vertices[0].y, max_y = min_y;
    for (auto p : original.vertices) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    const double scale = 588.0 / std::max(max_x - min_x, max_y - min_y);
    const Point center{(min_x + max_x) / 2.0, (min_y + max_y) / 2.0};
    const auto pixel = [&](Point p) { return cv::Point(cvRound(400 + (p.x - center.x) * scale), cvRound(413 - (p.y - center.y) * scale)); };
    cv::Mat image(size, size, CV_8UC3, cv::Scalar(255, 255, 255));
    label(image, "CATMULL-CLARK  /  LEVEL " + std::to_string(level), {32, 40}, 0.82);
    label(image,
          "V " + std::to_string(mesh.vertices.size()) + "   E " + std::to_string(topology.edges.size()) + "   F " +
              std::to_string(mesh.faces.size()) + "   Boundary " + std::to_string(topology.boundary_edges()),
          {32, 73}, 0.58);
    cv::line(image, {32, 91}, {768, 91}, ghost, 1, cv::LINE_AA);
    if (level > 0)
        for (const auto &edge : base.edges)
            cv::line(image, pixel(original.vertices[edge.vertices[0]]), pixel(original.vertices[edge.vertices[1]]), ghost, 1, cv::LINE_AA);
    for (const auto &edge : topology.edges)
        cv::line(image, pixel(mesh.vertices[edge.vertices[0]]), pixel(mesh.vertices[edge.vertices[1]]), edge.faces.size() == 1 ? boundary : interior,
                 edge.faces.size() == 1 ? 2 : 1, cv::LINE_AA);
    const auto old_vertices = previous ? previous->vertices.size() : mesh.vertices.size();
    const auto old_edges = previous ? build_topology(*previous).edges.size() : 0;
    if (mesh.vertices.size() <= 300) {
        for (std::size_t v = 0; v < mesh.vertices.size(); ++v) {
            const auto color = v < old_vertices ? ink : v < old_vertices + old_edges ? edge_color : face_color;
            cv::circle(image, pixel(mesh.vertices[v]), level < 2 ? 4 : 2, color, -1, cv::LINE_AA);
            if (level == 0 && mesh.vertices.size() <= 32)
                label(image, std::to_string(v + 1), pixel(mesh.vertices[v]) + cv::Point(7, -8), 0.48);
        }
    }
    cv::line(image, {32, 741}, {768, 741}, ghost, 1, cv::LINE_AA);
    if (level == 1) {
        cv::circle(image, {39, 771}, 4, ink, -1, cv::LINE_AA);
        label(image, "Updated vertex", {52, 777}, 0.52);
        cv::circle(image, {276, 771}, 4, edge_color, -1, cv::LINE_AA);
        label(image, "Edge point", {289, 777}, 0.52);
        cv::circle(image, {477, 771}, 4, face_color, -1, cv::LINE_AA);
        label(image, "Face point", {490, 777}, 0.52);
    } else {
        label(image, "Blue: boundary    Gray: original cage    X right / Y up", {32, 777}, 0.52);
    }
    return image;
}

cv::Mat render_overview(const std::vector<cv::Mat> &frames) {
    if (frames.empty())
        throw std::invalid_argument("No frames to combine.");
    const int columns = frames.size() <= 2 ? static_cast<int>(frames.size()) : frames.size() <= 4 ? 2 : 3;
    const int rows = (static_cast<int>(frames.size()) + columns - 1) / columns;
    cv::Mat overview(rows * size, columns * size, CV_8UC3, cv::Scalar(245, 245, 245));
    for (std::size_t i = 0; i < frames.size(); ++i)
        frames[i].copyTo(overview(cv::Rect(static_cast<int>(i) % columns * size, static_cast<int>(i) / columns * size, size, size)));
    return overview;
}

void save_image(const cv::Mat &image, const std::string &filename) {
    const auto parent = std::filesystem::path(filename).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    if (!cv::imwrite(filename, image))
        throw std::runtime_error("Cannot save image: " + filename);
}
} // namespace subdivision
