#include "rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
rst::ClipVertex mix(const rst::ClipVertex &a, const rst::ClipVertex &b, float t) {
    return {(1 - t) * a.clip + t * b.clip,
            {(1 - t) * a.attributes.position + t * b.attributes.position, (1 - t) * a.attributes.normal + t * b.attributes.normal,
             (1 - t) * a.attributes.color + t * b.attributes.color, (1 - t) * a.attributes.uv + t * b.attributes.uv}};
}
std::vector<rst::ClipVertex> clip_polygon(std::vector<rst::ClipVertex> polygon) {
    for (int plane = 0; plane < 6 && !polygon.empty(); ++plane) {
        const auto distance = [plane](const Vec4 &p) { return p.w() + (plane % 2 == 0 ? p[plane / 2] : -p[plane / 2]); };
        std::vector<rst::ClipVertex> next;
        auto previous = polygon.back();
        float previous_distance = distance(previous.clip);
        for (const auto &current : polygon) {
            const float current_distance = distance(current.clip);
            if ((previous_distance >= 0) != (current_distance >= 0))
                next.push_back(mix(previous, current, previous_distance / (previous_distance - current_distance)));
            if (current_distance >= 0)
                next.push_back(current);
            previous = current;
            previous_distance = current_distance;
        }
        polygon = std::move(next);
    }
    return polygon;
}
struct ScreenPoint {
    int64_t x, y;
};
int64_t edge(ScreenPoint a, ScreenPoint b, ScreenPoint p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }
bool top_left(ScreenPoint a, ScreenPoint b) { return b.y > a.y || (b.y == a.y && b.x < a.x); }
} // namespace

namespace rst {
rasterizer::rasterizer(int width, int height) : width_(width), height_(height) {
    if (width < 1 || height < 1 || width > 4096 || height > 4096)
        throw std::invalid_argument("Image dimensions must be between 1 and 4096");
    colors_.resize(size_t(width) * height);
    depths_.resize(colors_.size());
    clear();
}
void rasterizer::clear() {
    std::fill(colors_.begin(), colors_.end(), Vec3::Zero());
    std::fill(depths_.begin(), depths_.end(), std::numeric_limits<float>::infinity());
    stats_ = {};
}
void rasterizer::draw(const Mesh &mesh) {
    const Mat4 mv = view_ * model_;
    const Eigen::Matrix3f linear = mv.topLeftCorner<3, 3>();
    if (!mv.allFinite() || !projection_.allFinite() || std::abs(linear.determinant()) < 1e-12f)
        throw std::invalid_argument("Invalid or singular transform");
    const Eigen::Matrix3f normal_matrix = linear.inverse().transpose();
    for (const auto &triangle : mesh) {
        ++stats_.input_triangles;
        std::array<ClipVertex, 3> transformed;
        std::array<Vec3, 3> base;
        for (size_t i = 0; i < 3; ++i) {
            auto v = triangle[i];
            base[i] = (mv * Vec4(v.position.x(), v.position.y(), v.position.z(), 1)).head<3>();
            v.normal = unit(normal_matrix * v.normal);
            v.position = vertex_shader_({base[i], v.normal, v.uv});
            transformed[i] = {projection_ * Vec4(v.position.x(), v.position.y(), v.position.z(), 1), v};
            if (!transformed[i].clip.allFinite() || !v.normal.allFinite() || !v.color.allFinite() || !v.uv.allFinite())
                throw std::runtime_error("Nonfinite vertex attribute");
        }
        const Vec3 e1 = base[1] - base[0], e2 = base[2] - base[0];
        const Vec2 a = triangle[1].uv - triangle[0].uv, b = triangle[2].uv - triangle[0].uv;
        const float determinant = a.x() * b.y() - a.y() * b.x();
        Vec3 dpdu = Vec3::Zero(), dpdv = Vec3::Zero();
        if (std::abs(determinant) > 1e-10f) {
            dpdu = (b.y() * e1 - a.y() * e2) / determinant;
            dpdv = (-b.x() * e1 + a.x() * e2) / determinant;
        }
        const auto polygon = clip_polygon({transformed.begin(), transformed.end()});
        for (size_t i = 1; i + 1 < polygon.size(); ++i)
            rasterize_triangle({polygon[0], polygon[i], polygon[i + 1]}, dpdu, dpdv);
    }
}
void rasterizer::rasterize_triangle(std::array<ClipVertex, 3> t, const Vec3 &dpdu, const Vec3 &dpdv) {
    std::array<ScreenPoint, 3> p;
    std::array<float, 3> invw, z;
    for (int i = 0; i < 3; ++i) {
        if (t[i].clip.w() <= 1e-8f)
            return;
        invw[i] = 1.f / t[i].clip.w();
        const Vec3 ndc = t[i].clip.head<3>() * invw[i];
        p[i] = {std::llround((ndc.x() + 1) * 0.5 * width_ * 256), std::llround((ndc.y() + 1) * 0.5 * height_ * 256)};
        z[i] = ndc.z();
    }
    int64_t area = edge(p[0], p[1], p[2]);
    if (area == 0)
        return;
    if (area < 0) {
        std::swap(p[1], p[2]);
        std::swap(t[1], t[2]);
        std::swap(invw[1], invw[2]);
        std::swap(z[1], z[2]);
        area = -area;
    }
    ++stats_.raster_triangles;
    const int minx = std::max(0, int(std::min({p[0].x, p[1].x, p[2].x}) / 256));
    const int maxx = std::min(width_ - 1, int(std::max({p[0].x, p[1].x, p[2].x}) / 256));
    const int miny = std::max(0, int(std::min({p[0].y, p[1].y, p[2].y}) / 256));
    const int maxy = std::min(height_ - 1, int(std::max({p[0].y, p[1].y, p[2].y}) / 256));
    const bool inclusive[3] = {top_left(p[1], p[2]), top_left(p[2], p[0]), top_left(p[0], p[1])};
    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            const ScreenPoint sample{int64_t(x) * 256 + 128, int64_t(y) * 256 + 128};
            const int64_t edges[3] = {edge(p[1], p[2], sample), edge(p[2], p[0], sample), edge(p[0], p[1], sample)};
            if (edges[0] < 0 || (edges[0] == 0 && !inclusive[0]) || edges[1] < 0 || (edges[1] == 0 && !inclusive[1]) || edges[2] < 0 ||
                (edges[2] == 0 && !inclusive[2]))
                continue;
            const Vec3 bary(float(double(edges[0]) / area), float(double(edges[1]) / area), float(double(edges[2]) / area));
            const float depth = 0.5f * (bary.dot(Vec3(z[0], z[1], z[2])) + 1);
            const size_t index = size_t(height_ - 1 - y) * width_ + x;
            if (depth >= depths_[index])
                continue;
            Vec3 weights = bary.cwiseProduct(Vec3(invw[0], invw[1], invw[2]));
            weights /= weights.sum();
            fragment_shader_payload payload{Vec3::Zero(), Vec3::Zero(), Vec3::Zero(), Vec2::Zero(), dpdu, dpdv};
            for (int i = 0; i < 3; ++i) {
                payload.view_pos += weights[i] * t[i].attributes.position;
                payload.color += weights[i] * t[i].attributes.color;
                payload.normal += weights[i] * t[i].attributes.normal;
                payload.tex_coords += weights[i] * t[i].attributes.uv;
            }
            payload.normal = unit(payload.normal);
            const Vec3 color = fragment_shader_(payload);
            if (!color.allFinite())
                throw std::runtime_error("Fragment shader returned nonfinite color");
            colors_[index] = color.cwiseMax(0.f).cwiseMin(255.f);
            depths_[index] = depth;
            ++stats_.shaded_fragments;
        }
    }
}
cv::Mat rasterizer::image() const {
    cv::Mat result(height_, width_, CV_8UC3);
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            const auto &c = colors_[size_t(y) * width_ + x];
            result.at<cv::Vec3b>(y, x) = {cv::saturate_cast<unsigned char>(c.z()), cv::saturate_cast<unsigned char>(c.y()),
                                          cv::saturate_cast<unsigned char>(c.x())};
        }
    return result;
}
cv::Mat rasterizer::mask() const {
    cv::Mat result(height_, width_, CV_8UC1);
    for (size_t i = 0; i < depths_.size(); ++i)
        result.data[i] = std::isfinite(depths_[i]) ? 255 : 0;
    return result;
}
Statistics rasterizer::statistics() const {
    auto result = stats_;
    result.covered_pixels = std::count_if(depths_.begin(), depths_.end(), [](float d) { return std::isfinite(d); });
    return result;
}
} // namespace rst
