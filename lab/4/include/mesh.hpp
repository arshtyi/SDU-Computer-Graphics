#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace subdivision {

struct Point {
    double x = 0, y = 0;
    Point operator+(Point q) const { return {x + q.x, y + q.y}; }
    Point operator-(Point q) const { return {x - q.x, y - q.y}; }
    Point operator*(double s) const { return {x * s, y * s}; }
    Point operator/(double s) const { return {x / s, y / s}; }
};

using Face = std::vector<std::size_t>;
struct Mesh {
    std::vector<Point> vertices;
    std::vector<Face> faces;
};
struct Edge {
    std::array<std::size_t, 2> vertices;
    std::vector<std::size_t> faces;
};
struct Topology {
    std::vector<Edge> edges;
    std::vector<std::vector<std::size_t>> face_edges, vertex_edges, vertex_faces;
    std::size_t boundary_edges() const;
};

Topology build_topology(const Mesh &mesh);
Mesh read_obj(std::istream &input);
void write_obj(const Mesh &mesh, std::ostream &output);
Mesh catmull_clark(const Mesh &mesh);

} // namespace subdivision
