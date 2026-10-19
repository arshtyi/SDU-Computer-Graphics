#pragma once
#include "Texture.hpp"
#include <functional>

enum class Shader { Normal, Phong, Texture, Bump, BumpPhong, Displacement, DisplacementFragment };
struct Light {
    Vec3 position;
    Vec3 intensity;
};
struct ShaderSettings {
    const Texture *texture = nullptr;
    const Texture *height_map = nullptr;
    Filter filter = Filter::Bilinear;
    std::vector<Light> lights;
    Vec3 ambient = Vec3::Constant(10);
    float height_scale = 0.1f;
};
struct fragment_shader_payload {
    Vec3 view_pos, color, normal;
    Vec2 tex_coords;
    Vec3 dpdu, dpdv;
};
struct vertex_shader_payload {
    Vec3 position, normal;
    Vec2 tex_coords;
};
using FragmentShader = std::function<Vec3(const fragment_shader_payload &)>;
using VertexShader = std::function<Vec3(const vertex_shader_payload &)>;

Vec3 vertex_shader(const vertex_shader_payload &payload);
Vec3 displacement_vertex_shader(const vertex_shader_payload &payload, const ShaderSettings &settings);
Vec3 blinn_phong(const fragment_shader_payload &payload, const Vec3 &kd, const ShaderSettings &settings);
Vec3 perturbed_normal(const fragment_shader_payload &payload, const ShaderSettings &settings);
Vec3 normal_fragment_shader(const fragment_shader_payload &payload);
Vec3 phong_fragment_shader(const fragment_shader_payload &payload, const ShaderSettings &settings);
Vec3 texture_fragment_shader(const fragment_shader_payload &payload, const ShaderSettings &settings);
Vec3 bump_fragment_shader(const fragment_shader_payload &payload, const ShaderSettings &settings);
Vec3 displacement_fragment_shader(const fragment_shader_payload &payload, const ShaderSettings &settings);
FragmentShader make_shader(Shader shader, const ShaderSettings &settings);
Shader parse_shader(const std::string &name);
