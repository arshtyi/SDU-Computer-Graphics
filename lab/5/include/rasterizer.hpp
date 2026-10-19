#pragma once
#include "Shader.hpp"
#include <cstdint>

namespace rst {
struct ClipVertex {
    Vec4 clip;
    Vertex attributes;
};
struct Statistics {
    uint64_t input_triangles = 0, raster_triangles = 0, shaded_fragments = 0, covered_pixels = 0;
};
class rasterizer {
  public:
    rasterizer(int width, int height);
    void set_model(const Mat4 &m) { model_ = m; }
    void set_view(const Mat4 &v) { view_ = v; }
    void set_projection(const Mat4 &p) { projection_ = p; }
    void set_vertex_shader(VertexShader shader) { vertex_shader_ = std::move(shader); }
    void set_fragment_shader(FragmentShader shader) { fragment_shader_ = std::move(shader); }
    void clear();
    void draw(const Mesh &mesh);
    cv::Mat image() const;
    cv::Mat mask() const;
    const std::vector<Vec3> &frame_buffer() const { return colors_; }
    const std::vector<float> &depth_buffer() const { return depths_; }
    Statistics statistics() const;

  private:
    void rasterize_triangle(std::array<ClipVertex, 3> t, const Vec3 &dpdu, const Vec3 &dpdv);
    int width_, height_;
    Mat4 model_ = Mat4::Identity(), view_ = Mat4::Identity(), projection_ = Mat4::Identity();
    VertexShader vertex_shader_ = vertex_shader;
    FragmentShader fragment_shader_ = normal_fragment_shader;
    std::vector<Vec3> colors_;
    std::vector<float> depths_;
    Statistics stats_;
};
} // namespace rst
