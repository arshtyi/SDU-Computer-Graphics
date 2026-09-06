#include <Eigen/Core>
#include <cmath>
#include <iostream>

void example_cpp() {
    const double a = 1.0, b = 2.0;
    const double pi = std::acos(-1.0);

    std::cout << "=== C++ basics ===\n";
    std::cout << "a = " << a << ", b = " << b << '\n';
    std::cout << "a / b = " << a / b << '\n';
    std::cout << "sqrt(b) = " << std::sqrt(b) << '\n';
    std::cout << "pi = " << pi << '\n';
    std::cout << "sin(30 degrees) = " << std::sin(30.0 / 180.0 * pi) << "\n\n";
}

void example_vector() {
    const Eigen::Vector3f v(1.0f, 2.0f, 3.0f);
    const Eigen::Vector3f w(1.0f, 0.0f, 0.0f);

    std::cout << "=== Vectors ===\n";
    std::cout << "v =\n" << v << "\nw =\n" << w << '\n';
    std::cout << "v + w =\n" << v + w << '\n';
    std::cout << "v - w =\n" << v - w << '\n';
    std::cout << "v * 3 =\n" << v * 3.0f << '\n';
    std::cout << "2 * v =\n" << 2.0f * v << '\n';

    const float dot_by_scalars = v.x() * w.x() + v.y() * w.y() + v.z() * w.z();
    const float dot_by_eigen = v.dot(w);
    const float dot_by_matrix = (v.transpose() * w)(0, 0);
    std::cout << "Dot product by scalar products = " << dot_by_scalars << '\n';
    std::cout << "v.dot(w) = " << dot_by_eigen << '\n';
    std::cout << "(v.transpose() * w)(0, 0) = " << dot_by_matrix << '\n';
    std::cout << "v.dot(v) = " << v.dot(v) << " (squared length)\n";
    std::cout << "v.norm() = " << v.norm() << '\n';
    std::cout << "Projection of v onto unit vector w =\n" << v.dot(w) * w << '\n';
    std::cout << "v.cwiseProduct(w) =\n" << v.cwiseProduct(w) << '\n';
    std::cout << "v.cwiseProduct(w).sum() = " << v.cwiseProduct(w).sum() << "\n\n";
}

void example_matrix() {
    Eigen::Matrix3f i, j;
    i << 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f;
    j << 2.0f, 3.0f, 1.0f, 4.0f, 6.0f, 5.0f, 9.0f, 7.0f, 8.0f;
    const Eigen::Vector3f v(1.0f, 2.0f, 3.0f);

    std::cout << "=== Matrices ===\n";
    std::cout << "i =\n" << i << "\nj =\n" << j << '\n';
    std::cout << "i + j =\n" << i + j << '\n';
    std::cout << "i - j =\n" << i - j << '\n';
    std::cout << "i * 2 =\n" << i * 2.0f << '\n';
    std::cout << "2 * i =\n" << 2.0f * i << '\n';
    std::cout << "i * j =\n" << i * j << '\n';
    std::cout << "j * i =\n" << j * i << "\n(matrix multiplication is not commutative)\n";
    std::cout << "(i * j)(0, 0) = " << (i * j)(0, 0) << '\n';
    std::cout << "i.row(0).dot(j.col(0)) = " << i.row(0).dot(j.col(0)) << '\n';
    std::cout << "i * v =\n" << i * v << '\n';
    std::cout << "i * v by row dot products =\n"
              << Eigen::Vector3f(i.row(0).dot(v), i.row(1).dot(v), i.row(2).dot(v)) << '\n';
}

int main() {
    example_cpp();
    example_vector();
    example_matrix();
    return 0;
}
