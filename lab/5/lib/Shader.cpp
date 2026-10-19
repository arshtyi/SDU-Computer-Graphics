#include "Shader.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

Vec3 vertex_shader(const vertex_shader_payload &p) { return p.position; }

Vec3 displacement_vertex_shader(const vertex_shader_payload &p, const ShaderSettings &s) {
    if (!s.height_map)
        throw std::invalid_argument("Displacement requires a height map");
    return p.position + s.height_scale * s.height_map->heightAt(p.tex_coords, s.filter) * unit(p.normal);
}

Vec3 blinn_phong(const fragment_shader_payload &p, const Vec3 &kd, const ShaderSettings &s) {
    const Vec3 n = unit(p.normal), v = unit(-p.view_pos);
    const Vec3 ka = Vec3::Constant(0.005f), ks = Vec3::Constant(0.7937f);
    Vec3 result = ka.cwiseProduct(s.ambient);
    for (const auto &light : s.lights) {
        const Vec3 delta = light.position - p.view_pos;
        const float r2 = delta.squaredNorm();
        if (r2 < 1e-12f)
            continue;
        const Vec3 l = delta / std::sqrt(r2);
        const float ndotl = std::max(0.f, n.dot(l));
        if (ndotl == 0)
            continue;
        const Vec3 h = l + v;
        const float specular = h.squaredNorm() > 1e-12f ? std::pow(std::max(0.f, n.dot(h.normalized())), 150.f) : 0.f;
        result += (kd * ndotl + ks * specular).cwiseProduct(light.intensity / r2);
    }
    return 255.f * result;
}

Vec3 perturbed_normal(const fragment_shader_payload &p, const ShaderSettings &s) {
    if (!s.height_map)
        throw std::invalid_argument("Bump mapping requires a height map");
    const Texture &map = *s.height_map;
    const Vec2 uv = p.tex_coords.cwiseMax(0.f).cwiseMin(1.f);
    const float du = 1.f / std::max(1, map.width() - 1), dv = 1.f / std::max(1, map.height() - 1);
    const float u0 = std::max(0.f, uv.x() - du), u1 = std::min(1.f, uv.x() + du);
    const float v0 = std::max(0.f, uv.y() - dv), v1 = std::min(1.f, uv.y() + dv);
    const float hu = (map.heightAt({u1, uv.y()}, s.filter) - map.heightAt({u0, uv.y()}, s.filter)) / (u1 - u0);
    const float hv = (map.heightAt({uv.x(), v1}, s.filter) - map.heightAt({uv.x(), v0}, s.filter)) / (v1 - v0);
    const Vec3 n = unit(p.normal);
    Vec3 tu = p.dpdu - n.dot(p.dpdu) * n;
    Vec3 tv = p.dpdv - n.dot(p.dpdv) * n;
    if (tu.cross(tv).squaredNorm() < 1e-12f) {
        // A stable orthonormal fallback also handles normals at the Y-axis poles.
        tu = n.unitOrthogonal();
        tv = n.cross(tu);
    }
    const float orientation = tu.cross(tv).dot(n) < 0 ? -1.f : 1.f;
    return unit(orientation * (tu + s.height_scale * hu * n).cross(tv + s.height_scale * hv * n), n);
}

Vec3 normal_fragment_shader(const fragment_shader_payload &p) { return 127.5f * (unit(p.normal) + Vec3::Ones()); }
Vec3 phong_fragment_shader(const fragment_shader_payload &p, const ShaderSettings &s) { return blinn_phong(p, p.color, s); }
Vec3 texture_fragment_shader(const fragment_shader_payload &p, const ShaderSettings &s) {
    if (!s.texture)
        throw std::invalid_argument("Texture shading requires a texture");
    return blinn_phong(p, s.texture->sample(p.tex_coords, s.filter) / 255.f, s);
}
Vec3 bump_fragment_shader(const fragment_shader_payload &p, const ShaderSettings &s) { return 127.5f * (perturbed_normal(p, s) + Vec3::Ones()); }
Vec3 displacement_fragment_shader(const fragment_shader_payload &p, const ShaderSettings &s) {
    auto displaced = p;
    displaced.view_pos = displacement_vertex_shader({p.view_pos, p.normal, p.tex_coords}, s);
    displaced.normal = perturbed_normal(p, s);
    return blinn_phong(displaced, p.color, s);
}
FragmentShader make_shader(Shader shader, const ShaderSettings &s) {
    return [shader, s](const fragment_shader_payload &p) -> Vec3 {
        switch (shader) {
        case Shader::Normal:
            return normal_fragment_shader(p);
        case Shader::Phong:
            return phong_fragment_shader(p, s);
        case Shader::Texture:
            return texture_fragment_shader(p, s);
        case Shader::Bump:
            return bump_fragment_shader(p, s);
        case Shader::DisplacementFragment:
            return displacement_fragment_shader(p, s);
        case Shader::BumpPhong:
        case Shader::Displacement: {
            auto perturbed = p;
            perturbed.normal = perturbed_normal(p, s);
            return blinn_phong(perturbed, p.color, s);
        }
        }
        throw std::logic_error("Unknown shader");
    };
}
Shader parse_shader(const std::string &name) {
    if (name == "normal")
        return Shader::Normal;
    if (name == "phong")
        return Shader::Phong;
    if (name == "texture")
        return Shader::Texture;
    if (name == "bump")
        return Shader::Bump;
    if (name == "bump-phong")
        return Shader::BumpPhong;
    if (name == "displacement")
        return Shader::Displacement;
    if (name == "displacement-fragment")
        return Shader::DisplacementFragment;
    throw std::invalid_argument("Unknown shader: " + name);
}
