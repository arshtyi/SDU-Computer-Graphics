#include <Eigen/Core>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>

using Point = Eigen::Vector2d;
using Triangle = std::array<Point, 3>;
constexpr double eps = 1e-9;

double cross(const Point &a, const Point &b) { return a.x() * b.y() - a.y() * b.x(); }

bool onBoundary(const Triangle &triangle, const Point &p) {
    for (int i = 0; i < 3; ++i) {
        const Point &a = triangle[i];
        const Point &b = triangle[(i + 1) % 3];
        if (std::abs(cross(b - a, p - a)) <= eps && (p - a).dot(p - b) <= eps) {
            return true;
        }
    }
    return false;
}

bool rayCasting(const Triangle &triangle, const Point &p) {
    int intersections = 0;
    for (int i = 0; i < 3; ++i) {
        const Point &a = triangle[i];
        const Point &b = triangle[(i + 1) % 3];
        if ((a.y() > p.y()) != (b.y() > p.y())) {
            const double x = a.x() + (p.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
            if (x > p.x()) {
                ++intersections;
            }
        }
    }
    return intersections % 2 == 1;
}

bool sameSide(const Triangle &triangle, const Point &p) {
    const double c1 = cross(triangle[1] - triangle[0], p - triangle[0]);
    const double c2 = cross(triangle[2] - triangle[1], p - triangle[1]);
    const double c3 = cross(triangle[0] - triangle[2], p - triangle[2]);
    return (c1 > 0 && c2 > 0 && c3 > 0) || (c1 < 0 && c2 < 0 && c3 < 0);
}

bool barycentric(const Triangle &triangle, const Point &p) {
    const Point ab = triangle[1] - triangle[0];
    const Point ac = triangle[2] - triangle[0];
    const Point ap = p - triangle[0];
    const double determinant = cross(ab, ac);
    // P = (1 - u - v) A + u B + v C。
    const double u = cross(ap, ac) / determinant;
    const double v = cross(ab, ap) / determinant;
    return u > 0 && v > 0 && u + v < 1;
}

const char *location(bool inside) { return inside ? "inside" : "outside"; }

int main(int argc, char *argv[]) {
    std::ifstream file;
    if (argc > 1) {
        file.open(argv[1]);
        if (!file) {
            std::cerr << "Cannot open input file: " << argv[1] << '\n';
            return 1;
        }
    }
    std::istream &input = argc > 1 ? file : std::cin;

    Triangle triangle;
    for (Point &vertex : triangle) {
        if (!(input >> vertex.x() >> vertex.y())) {
            std::cerr << "Invalid vertex input.\n";
            return 1;
        }
    }
    if (std::abs(cross(triangle[1] - triangle[0], triangle[2] - triangle[0])) <= eps) {
        std::cerr << "Degenerate triangle: vertices are collinear or repeated.\n";
        return 1;
    }

    int count;
    if (!(input >> count) || count <= 0) {
        std::cerr << "Invalid point count.\n";
        return 1;
    }
    for (int i = 0; i < count; ++i) {
        Point p;
        if (!(input >> p.x() >> p.y())) {
            std::cerr << "Invalid point input.\n";
            return 1;
        }
        const bool boundary = onBoundary(triangle, p);
        std::cout << "P = (" << p.x() << ", " << p.y() << ")\n";
        std::cout << "  Ray casting: "
                  << (boundary ? "boundary" : location(rayCasting(triangle, p))) << '\n';
        std::cout << "  Same side:   " << (boundary ? "boundary" : location(sameSide(triangle, p)))
                  << '\n';
        std::cout << "  Barycentric: "
                  << (boundary ? "boundary" : location(barycentric(triangle, p))) << '\n';
    }
    return 0;
}
