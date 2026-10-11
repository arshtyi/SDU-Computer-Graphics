#include "mesh.hpp"
#include <stdexcept>

namespace subdivision {
Mesh catmull_clark(const Mesh &mesh) {
    const auto topology = build_topology(mesh);
    const auto nv = mesh.vertices.size(), ne = topology.edges.size(), nf = mesh.faces.size();
    std::size_t next_faces = 0;
    for (const auto &face : mesh.faces)
        next_faces += face.size();
    if (nv + ne + nf > 1000000 || next_faces > 250000)
        throw std::invalid_argument("Subdivision would exceed the mesh size limit.");

    Mesh next;
    next.vertices.resize(nv + ne + nf);
    next.faces.reserve(next_faces);
    for (std::size_t f = 0; f < nf; ++f) {
        Point sum;
        for (auto v : mesh.faces[f])
            sum = sum + mesh.vertices[v];
        next.vertices[nv + ne + f] = sum / static_cast<double>(mesh.faces[f].size());
    }
    for (std::size_t e = 0; e < ne; ++e) {
        const auto &edge = topology.edges[e];
        const auto endpoints = mesh.vertices[edge.vertices[0]] + mesh.vertices[edge.vertices[1]];
        next.vertices[nv + e] = edge.faces.size() == 1
                                    ? endpoints / 2.0
                                    : (endpoints + next.vertices[nv + ne + edge.faces[0]] + next.vertices[nv + ne + edge.faces[1]]) / 4.0;
    }
    for (std::size_t v = 0; v < nv; ++v) {
        const auto p = mesh.vertices[v];
        Point face_sum, midpoint_sum, boundary_sum;
        std::size_t boundary_count = 0;
        for (auto f : topology.vertex_faces[v])
            face_sum = face_sum + next.vertices[nv + ne + f];
        for (auto e : topology.vertex_edges[v]) {
            const auto &edge = topology.edges[e];
            const auto other = edge.vertices[0] == v ? edge.vertices[1] : edge.vertices[0];
            midpoint_sum = midpoint_sum + (p + mesh.vertices[other]) / 2.0;
            if (edge.faces.size() == 1) {
                boundary_sum = boundary_sum + mesh.vertices[other];
                ++boundary_count;
            }
        }
        const double n = static_cast<double>(topology.vertex_edges[v].size());
        next.vertices[v] = boundary_count == 2 ? (p * 6.0 + boundary_sum) / 8.0 : (face_sum / n + midpoint_sum * (2.0 / n) + p * (n - 3.0)) / n;
    }
    for (std::size_t f = 0; f < nf; ++f) {
        const auto &face = mesh.faces[f];
        const auto &edges = topology.face_edges[f];
        for (std::size_t i = 0; i < face.size(); ++i)
            next.faces.push_back({face[i], nv + edges[i], nv + ne + f, nv + edges[(i + face.size() - 1) % face.size()]});
    }
    build_topology(next);
    return next;
}
} // namespace subdivision
