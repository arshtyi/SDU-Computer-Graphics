#include "rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rst {

rasterizer::rasterizer(int width, int height)
    : width(width), height(height), frame_buf(width * height), depth_buf(width * height) {}

pos_buf_id rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions) {
    const int id = next_id++;
    pos_buf.emplace(id, positions);
    return {id};
}

ind_buf_id rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices) {
    const int id = next_id++;
    ind_buf.emplace(id, indices);
    return {id};
}

void rasterizer::set_model(const Eigen::Matrix4f &matrix) { model = matrix; }

void rasterizer::set_view(const Eigen::Matrix4f &matrix) { view = matrix; }

void rasterizer::set_projection(const Eigen::Matrix4f &matrix) { projection = matrix; }

void rasterizer::clear(Buffers buffers) {
    if ((buffers & Buffers::Color) == Buffers::Color) {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f::Zero());
    }
    if ((buffers & Buffers::Depth) == Buffers::Depth) {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

void rasterizer::draw(pos_buf_id positions, ind_buf_id indices, Primitive type) {
    if (type != Primitive::Triangle) {
        throw std::invalid_argument("Only triangle wireframes are supported.");
    }
    const auto &vertices = pos_buf.at(positions.pos_id);
    const auto &faces = ind_buf.at(indices.ind_id);
    const Eigen::Matrix4f mvp = projection * view * model;

    for (const auto &face : faces) {
        Triangle triangle;
        for (int i = 0; i < 3; ++i) {
            const auto &vertex = vertices[face[i]];
            Eigen::Vector4f point = mvp * Eigen::Vector4f(vertex.x(), vertex.y(), vertex.z(), 1);
            point /= point.w();
            point.x() = 0.5f * width * (point.x() + 1.0f);
            point.y() = 0.5f * height * (point.y() + 1.0f);
            triangle[i] = point.head<3>();
        }
        rasterize_wireframe(triangle);
    }
}

void rasterizer::rasterize_wireframe(const Triangle &triangle) {
    for (int i = 0; i < 3; ++i) {
        draw_line(triangle[i], triangle[(i + 1) % 3]);
    }
}

void rasterizer::set_pixel(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }
    frame_buf[(height - 1 - y) * width + x] = Eigen::Vector3f(255, 255, 255);
}

void rasterizer::draw_line(const Eigen::Vector3f &begin, const Eigen::Vector3f &end) {
    int x = static_cast<int>(begin.x());
    int y = static_cast<int>(begin.y());
    const int end_x = static_cast<int>(end.x());
    const int end_y = static_cast<int>(end.y());
    const int dx = std::abs(end_x - x);
    const int dy = std::abs(end_y - y);
    const int step_x = x < end_x ? 1 : -1;
    const int step_y = y < end_y ? 1 : -1;
    int error = dx - dy;
    while (true) {
        set_pixel(x, y);
        if (x == end_x && y == end_y) {
            break;
        }
        const int twice_error = 2 * error;
        if (twice_error > -dy) {
            error -= dy;
            x += step_x;
        }
        if (twice_error < dx) {
            error += dx;
            y += step_y;
        }
    }
}

} // namespace rst
