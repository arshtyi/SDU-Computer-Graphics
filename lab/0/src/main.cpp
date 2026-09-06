#include <Eigen/Core>
#include <cmath>
#include <iomanip>
#include <iostream>

int main() {
    const Eigen::Vector3d point(2.0, 1.0, 1.0);
    const double angle = 45.0 / 180.0 * std::acos(-1.0);
    const double c = std::cos(angle);
    const double s = std::sin(angle);

    Eigen::Matrix3d rotation;
    Eigen::Matrix3d translation;
    // clang-format off
    rotation <<   c,  -s, 0.0,
                  s,   c, 0.0,
                0.0, 0.0, 1.0;
    translation << 1.0, 0.0, 1.0,
                   0.0, 1.0, 2.0,
                   0.0, 0.0, 1.0;
    // clang-format on

    const Eigen::Matrix3d transform = translation * rotation;
    const Eigen::Vector3d transformed = transform * point;
    const Eigen::Vector2d cartesian = transformed.head<2>() / transformed.z();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "P (homogeneous) =\n" << point << '\n';
    std::cout << "R (45 degrees counterclockwise) =\n" << rotation << '\n';
    std::cout << "T (translation by (1, 2)) =\n" << translation << '\n';
    std::cout << "T * R =\n" << transform << '\n';
    std::cout << "P' = T * R * P =\n" << transformed << '\n';
    std::cout << "Cartesian coordinates: (" << cartesian.x() << ", " << cartesian.y() << ")\n";
    return 0;
}
