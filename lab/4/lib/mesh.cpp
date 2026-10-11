#include "mesh.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace subdivision {
namespace {
using Key = std::pair<std::size_t, std::size_t>;
Key key(std::size_t a, std::size_t b) { return std::minmax(a, b); }
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::invalid_argument(message);
}
double cross(Point a, Point b, Point c) {
    const auto u = b - a, v = c - a;
    return u.x * v.y - u.y * v.x;
}
bool on_segment(Point a, Point b, Point p) {
    return cross(a, b, p) == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) && p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}
bool intersects(Point a, Point b, Point c, Point d) {
    const auto x = cross(a, b, c), y = cross(a, b, d), z = cross(c, d, a), w = cross(c, d, b);
    return ((x > 0 && y < 0) || (x < 0 && y > 0)) && ((z > 0 && w < 0) || (z < 0 && w > 0))
               ? true
               : on_segment(a, b, c) || on_segment(a, b, d) || on_segment(c, d, a) || on_segment(c, d, b);
}
void validate_face(const Mesh &mesh, const Face &face) {
    require(face.size() >= 3 && face.size() <= 1024, "A face must contain 3 to 1024 vertices.");
    std::set<std::size_t> unique;
    for (auto v : face) {
        require(v < mesh.vertices.size(), "Face index out of range.");
        require(unique.insert(v).second, "Repeated vertex in a face.");
    }
    const auto origin = mesh.vertices[face[0]];
    double area = 0, scale = 0;
    for (std::size_t i = 0; i < face.size(); ++i) {
        const auto a = mesh.vertices[face[i]], b = mesh.vertices[face[(i + 1) % face.size()]];
        require(a.x != b.x || a.y != b.y, "Zero-length edge.");
        area += cross(origin, a, b);
        scale = std::max({scale, std::abs(a.x - origin.x), std::abs(a.y - origin.y)});
        const auto c = mesh.vertices[face[(i + 2) % face.size()]];
        const auto u = a - b, v = c - b;
        require(cross(a, b, c) != 0 || u.x * v.x + u.y * v.y <= 0, "Overlapping adjacent edges.");
        for (std::size_t j = i + 1; j < face.size(); ++j) {
            if (j == i + 1 || (i == 0 && j + 1 == face.size()))
                continue;
            require(!intersects(a, b, mesh.vertices[face[j]], mesh.vertices[face[(j + 1) % face.size()]]), "Self-intersecting face.");
        }
    }
    require(std::abs(area) > 1e-12 * scale * scale, "Degenerate face.");
}
std::size_t read_index(const std::string &token, std::size_t count) {
    std::size_t used = 0;
    const auto index = std::stoll(token, &used);
    require(used == token.size() && index != 0, "Expected a nonzero integer vertex index (no texture/normal indices).");
    const auto resolved = index > 0 ? index - 1 : static_cast<long long>(count) + index;
    require(resolved >= 0 && static_cast<std::size_t>(resolved) < count, "Vertex index out of range.");
    return static_cast<std::size_t>(resolved);
}
double read_coordinate(const std::string &token) {
    std::size_t used = 0;
    const auto value = std::stod(token, &used);
    require(used == token.size() && std::isfinite(value) && std::abs(value) <= 1e9, "Expected a finite coordinate in [-1e9, 1e9].");
    return value;
}
} // namespace

std::size_t Topology::boundary_edges() const {
    return static_cast<std::size_t>(std::count_if(edges.begin(), edges.end(), [](const Edge &e) { return e.faces.size() == 1; }));
}

Topology build_topology(const Mesh &mesh) {
    require(!mesh.vertices.empty() && !mesh.faces.empty(), "Mesh requires vertices and faces; edge-only input has no face topology.");
    require(mesh.vertices.size() <= 1000000 && mesh.faces.size() <= 250000, "Mesh size limit exceeded.");
    for (auto p : mesh.vertices)
        require(std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 1e9 && std::abs(p.y) <= 1e9, "Invalid vertex coordinates.");
    Topology topology;
    topology.vertex_edges.resize(mesh.vertices.size());
    topology.vertex_faces.resize(mesh.vertices.size());
    topology.face_edges.resize(mesh.faces.size());
    std::map<Key, std::size_t> indices;
    std::set<Face> face_keys;
    std::vector<bool> first_direction;
    for (std::size_t f = 0; f < mesh.faces.size(); ++f) {
        const auto &face = mesh.faces[f];
        validate_face(mesh, face);
        auto sorted = face;
        std::sort(sorted.begin(), sorted.end());
        require(face_keys.insert(sorted).second, "Duplicate face.");
        for (std::size_t i = 0; i < face.size(); ++i) {
            const auto a = face[i], b = face[(i + 1) % face.size()];
            topology.vertex_faces[a].push_back(f);
            auto [it, inserted] = indices.emplace(key(a, b), topology.edges.size());
            const auto e = it->second;
            if (inserted) {
                topology.edges.push_back({{std::min(a, b), std::max(a, b)}, {}});
                first_direction.push_back(a < b);
                topology.vertex_edges[a].push_back(e);
                topology.vertex_edges[b].push_back(e);
            } else {
                require(topology.edges[e].faces.size() < 2, "Non-manifold edge: more than two faces.");
                require(first_direction[e] != (a < b), "Inconsistent face winding across a shared edge.");
            }
            topology.edges[e].faces.push_back(f);
            topology.face_edges[f].push_back(e);
        }
    }
    for (std::size_t v = 0; v < mesh.vertices.size(); ++v) {
        const auto &faces = topology.vertex_faces[v];
        require(!faces.empty(), "Isolated vertex.");
        std::map<std::size_t, std::vector<std::size_t>> adjacent;
        std::size_t boundary = 0;
        for (auto e : topology.vertex_edges[v]) {
            const auto &ef = topology.edges[e].faces;
            if (ef.size() == 1)
                ++boundary;
            else {
                adjacent[ef[0]].push_back(ef[1]);
                adjacent[ef[1]].push_back(ef[0]);
            }
        }
        require(boundary == 0 || boundary == 2, "Non-manifold boundary vertex.");
        std::set<std::size_t> visited;
        std::vector<std::size_t> pending{faces.front()};
        while (!pending.empty()) {
            const auto f = pending.back();
            pending.pop_back();
            if (visited.insert(f).second)
                for (auto next : adjacent[f])
                    pending.push_back(next);
        }
        require(visited.size() == faces.size(), "Non-manifold vertex: disconnected face fans.");
    }
    return topology;
}

Mesh read_obj(std::istream &input) {
    Mesh mesh;
    std::set<Key> declared_edges;
    std::string line;
    std::size_t line_number = 0, corners = 0;
    while (std::getline(input, line)) {
        ++line_number;
        try {
            auto slash_comment = line.find("//");
            if (slash_comment != std::string::npos && slash_comment > 0 && !std::isspace(static_cast<unsigned char>(line[slash_comment - 1])))
                slash_comment = std::string::npos;
            const auto comment = std::min(line.find('#'), slash_comment);
            if (comment != std::string::npos)
                line.erase(comment);
            std::istringstream row(line);
            std::string kind, token;
            if (!(row >> kind))
                continue;
            std::vector<std::string> fields;
            while (row >> token)
                fields.push_back(token);
            if (kind == "v") {
                require(fields.size() == 2 || fields.size() == 3, "Expected v x y [0].");
                const Point p{read_coordinate(fields[0]), read_coordinate(fields[1])};
                if (fields.size() == 3)
                    require(read_coordinate(fields[2]) == 0, "Only the XY plane (z = 0) is supported.");
                require(mesh.vertices.size() < 1000000, "Too many vertices.");
                mesh.vertices.push_back(p);
            } else if (kind == "f" || kind == "e" || kind == "l") {
                require(kind == "f" ? fields.size() >= 3 && fields.size() <= 1024 : fields.size() == 2, "Expected f i j k ... or e/l i j.");
                Face indices;
                for (const auto &field : fields)
                    indices.push_back(read_index(field, mesh.vertices.size()));
                if (kind == "f") {
                    corners += indices.size();
                    require(mesh.faces.size() < 250000 && corners <= 1000000, "Too many faces or face corners.");
                    mesh.faces.push_back(std::move(indices));
                } else {
                    require(indices[0] != indices[1], "Self-loop edge.");
                    require(declared_edges.insert(key(indices[0], indices[1])).second, "Duplicate declared edge.");
                }
            } else if (kind != "o" && kind != "g" && kind != "s" && kind != "usemtl" && kind != "mtllib") {
                throw std::invalid_argument("Unsupported OBJ record: " + kind);
            }
        } catch (const std::exception &error) {
            throw std::invalid_argument("Line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    require(!input.bad(), "Failed while reading OBJ input.");
    const auto topology = build_topology(mesh);
    if (!declared_edges.empty()) {
        std::set<Key> actual;
        for (const auto &edge : topology.edges)
            actual.insert(key(edge.vertices[0], edge.vertices[1]));
        require(actual == declared_edges, "Declared edges must exactly match the edges of all faces.");
    }
    return mesh;
}

void write_obj(const Mesh &mesh, std::ostream &output) {
    build_topology(mesh);
    output << "# Catmull-Clark planar mesh; standard OBJ vertices and faces\n" << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const auto p : mesh.vertices)
        output << "v " << p.x << ' ' << p.y << " 0\n";
    for (const auto &face : mesh.faces) {
        output << 'f';
        for (auto v : face)
            output << ' ' << v + 1;
        output << '\n';
    }
    if (!output)
        throw std::runtime_error("Failed to write OBJ output.");
}
} // namespace subdivision
