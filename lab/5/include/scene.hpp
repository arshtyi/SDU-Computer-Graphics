#pragma once
#include "Triangle.hpp"
#include <string>

Mesh load_obj(const std::string &path);
Mesh subdivide(const Mesh &mesh, int levels);
void fit_mesh(Mesh &mesh);
Mat4 get_model_matrix(float angle, float scale = 2.5f);
Mat4 get_view_matrix(const Vec3 &eye);
Mat4 get_projection_matrix(float fov, float aspect, float near, float far);
