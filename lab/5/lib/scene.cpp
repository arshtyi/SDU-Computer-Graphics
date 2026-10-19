#include "scene.hpp"
#include <OBJ_Loader.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>

Mesh load_obj(const std::string &path) {
    if (!std::filesystem::is_regular_file(path) || std::filesystem::path(path).extension() != ".obj")
        throw std::runtime_error("Cannot open OBJ: " + path);
    objl::Loader loader;
    if (!loader.LoadFile(path))
        throw std::runtime_error("Failed to load OBJ: " + path);
    Mesh result;
    for (const auto &mesh : loader.LoadedMeshes) {
        if (mesh.Indices.size() % 3 != 0)
            throw std::runtime_error("Non-triangular loader output");
        for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
            Triangle triangle;
            for (size_t j = 0; j < 3; ++j) {
                const auto &v = mesh.Vertices.at(mesh.Indices[i + j]);
                triangle[j].position = {v.Position.X, v.Position.Y, v.Position.Z};
                triangle[j].normal = {v.Normal.X, v.Normal.Y, v.Normal.Z};
                triangle[j].uv = {v.TextureCoordinate.X, v.TextureCoordinate.Y};
                if (!triangle[j].position.allFinite() || !triangle[j].normal.allFinite() || !triangle[j].uv.allFinite())
                    throw std::runtime_error("Nonfinite OBJ attribute");
                triangle[j].normal = unit(triangle[j].normal);
            }
            result.push_back(triangle);
        }
    }
    if (result.empty())
        throw std::runtime_error("OBJ contains no triangles");
    return result;
}

static Vertex midpoint(const Vertex &a, const Vertex &b) {
    return {(a.position + b.position) / 2, unit(a.normal + b.normal), (a.color + b.color) / 2, (a.uv + b.uv) / 2};
}
Mesh subdivide(const Mesh &mesh, int levels) {
    if (levels < 0 || levels > 3)
        throw std::invalid_argument("Subdivision must be between 0 and 3");
    Mesh result = mesh;
    for (int level = 0; level < levels; ++level) {
        if (result.size() > 500000)
            throw std::invalid_argument("Subdivision would exceed two million triangles");
        Mesh next;
        next.reserve(result.size() * 4);
        for (const auto &t : result) {
            const auto a = midpoint(t[0], t[1]), b = midpoint(t[1], t[2]), c = midpoint(t[2], t[0]);
            next.push_back({t[0], a, c});
            next.push_back({a, t[1], b});
            next.push_back({c, b, t[2]});
            next.push_back({a, b, c});
        }
        result = std::move(next);
    }
    return result;
}
void fit_mesh(Mesh &mesh) {
    Vec3 low = Vec3::Constant(std::numeric_limits<float>::infinity()), high = -low;
    for (const auto &t : mesh)
        for (const auto &v : t) {
            low = low.cwiseMin(v.position);
            high = high.cwiseMax(v.position);
        }
    const float extent = (high - low).maxCoeff();
    if (!std::isfinite(extent) || extent < 1e-8f)
        throw std::invalid_argument("Empty or degenerate model bounds");
    const Vec3 center = (low + high) / 2;
    for (auto &t : mesh)
        for (auto &v : t)
            v.position = (v.position - center) * (2.f / extent);
}
Mat4 get_model_matrix(float angle, float scale) {
    Mat4 result = Mat4::Identity();
    result.topLeftCorner<3, 3>() = scale * Eigen::AngleAxisf(angle * float(std::acos(-1)) / 180.f, Vec3::UnitY()).toRotationMatrix();
    return result;
}
Mat4 get_view_matrix(const Vec3 &eye) {
    Mat4 result = Mat4::Identity();
    result.topRightCorner<3, 1>() = -eye;
    return result;
}
Mat4 get_projection_matrix(float fov, float aspect, float near, float far) {
    if (!std::isfinite(fov) || !std::isfinite(aspect) || !std::isfinite(near) || !std::isfinite(far) || fov <= 0 || fov >= 180 || aspect <= 0 ||
        near <= 0 || far <= near)
        throw std::invalid_argument("Invalid perspective parameters");
    const float cotangent = 1.f / std::tan(fov * float(std::acos(-1)) / 360.f);
    Mat4 result = Mat4::Zero();
    result(0, 0) = cotangent / aspect;
    result(1, 1) = cotangent;
    result(2, 2) = -(far + near) / (far - near);
    result(2, 3) = -2 * far * near / (far - near);
    result(3, 2) = -1;
    return result;
}
