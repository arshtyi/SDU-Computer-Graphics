#pragma once

#include "Triangle.hpp"
#include <Eigen/Core>
#include <map>
#include <vector>

namespace rst {

enum class Buffers { Color = 1, Depth = 2 };

inline Buffers operator|(Buffers a, Buffers b) {
    return static_cast<Buffers>(static_cast<int>(a) | static_cast<int>(b));
}

inline Buffers operator&(Buffers a, Buffers b) {
    return static_cast<Buffers>(static_cast<int>(a) & static_cast<int>(b));
}

enum class Primitive { Line, Triangle };

struct pos_buf_id {
    int pos_id;
};

struct ind_buf_id {
    int ind_id;
};

class rasterizer {
  public:
    rasterizer(int width, int height);
    pos_buf_id load_positions(const std::vector<Eigen::Vector3f> &positions);
    ind_buf_id load_indices(const std::vector<Eigen::Vector3i> &indices);

    void set_model(const Eigen::Matrix4f &matrix);
    void set_view(const Eigen::Matrix4f &matrix);
    void set_projection(const Eigen::Matrix4f &matrix);
    void clear(Buffers buffers);
    void draw(pos_buf_id positions, ind_buf_id indices, Primitive type);

    std::vector<Eigen::Vector3f> &frame_buffer() { return frame_buf; }

  private:
    void set_pixel(int x, int y);
    void draw_line(const Eigen::Vector3f &begin, const Eigen::Vector3f &end);
    void rasterize_wireframe(const Triangle &triangle);

    int width;
    int height;
    int next_id = 0;
    Eigen::Matrix4f model = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();
    std::map<int, std::vector<Eigen::Vector3f>> pos_buf;
    std::map<int, std::vector<Eigen::Vector3i>> ind_buf;
    std::vector<Eigen::Vector3f> frame_buf;
    std::vector<float> depth_buf;
};

} // namespace rst
