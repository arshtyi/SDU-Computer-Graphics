#pragma once
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <array>
#include <vector>

using Vec2 = Eigen::Vector2f;
using Vec3 = Eigen::Vector3f;
using Vec4 = Eigen::Vector4f;
using Mat4 = Eigen::Matrix4f;

struct Vertex {
    Vec3 position = Vec3::Zero();
    Vec3 normal = Vec3::UnitZ();
    Vec3 color = Vec3(148, 121, 92) / 255.f;
    Vec2 uv = Vec2::Zero();
};
using Triangle = std::array<Vertex, 3>;
using Mesh = std::vector<Triangle>;

inline Vec3 unit(const Vec3 &v, const Vec3 &fallback = Vec3::UnitZ()) { return v.squaredNorm() > 1e-16f ? Vec3(v.normalized()) : fallback; }
