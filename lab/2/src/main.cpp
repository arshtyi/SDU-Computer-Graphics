#include "rasterizer.hpp"
#include <Eigen/Core>
#include <cmath>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>
#include <vector>

constexpr float MY_PI = 3.14159265358979323846f;

Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos) {
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, -eye_pos[0], 0, 1, 0, -eye_pos[1], 0, 0, 1, -eye_pos[2], 0, 0, 0, 1;

    view = translate * view;

    return view;
}
Eigen::Matrix4f get_model_matrix(float rotation_angle) {
    const float radians = rotation_angle * MY_PI / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Eigen::Matrix4f model;
    model << c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1;
    return model;
}
Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar) {
    const float t = zNear * std::tan(eye_fov * MY_PI / 360.0f);
    const float r = t * aspect_ratio;
    Eigen::Matrix4f projection;
    projection << zNear / r, 0, 0, 0, 0, zNear / t, 0, 0, 0, 0, -(zFar + zNear) / (zFar - zNear),
        -2.0f * zFar * zNear / (zFar - zNear), 0, 0, -1, 0;
    return projection;
}
Eigen::Matrix4f get_rotation(Eigen::Vector3f axis, float angle) {
    const float length = axis.norm();
    if (!axis.allFinite() || !std::isfinite(length) || length == 0.0f) {
        throw std::invalid_argument("Rotation axis must be finite and nonzero.");
    }
    axis /= length;
    const float radians = angle * MY_PI / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float d = 1.0f - c;
    const float x = axis.x(), y = axis.y(), z = axis.z();
    Eigen::Matrix4f rotation;
    rotation << c + d * x * x, d * x * y - s * z, d * x * z + s * y, 0, d * y * x + s * z,
        c + d * y * y, d * y * z - s * x, 0, d * z * x - s * y, d * z * y + s * x, c + d * z * z, 0,
        0, 0, 0, 1;
    return rotation;
}
int main(int argc, const char **argv) {
    try {
        float angle = 0.0f;
        bool command_line = argc > 1;
        bool arbitrary_axis = false;
        Eigen::Vector3f axis = {0, 0, 1};
        std::string filename = "output.png";

        if (command_line) {
            const std::string option = argv[1];
            if (option == "-r" && (argc == 3 || argc == 4)) {
                angle = std::stof(argv[2]);
                if (argc == 4)
                    filename = argv[3];
            } else if (option == "-a" && (argc == 6 || argc == 7)) {
                arbitrary_axis = true;
                angle = std::stof(argv[2]);
                axis = {std::stof(argv[3]), std::stof(argv[4]), std::stof(argv[5])};
                if (argc == 7)
                    filename = argv[6];
            } else {
                std::cerr << "Usage: Rasterizer\n"
                          << "       Rasterizer -r angle [filename]\n"
                          << "       Rasterizer -a angle x y z [filename]\n";
                return 1;
            }
            if (!std::isfinite(angle)) {
                throw std::invalid_argument("Angle must be finite.");
            }
        }
        rst::rasterizer r(700, 700);
        Eigen::Vector3f eye_pos = {0, 0, 5};
        std::vector<Eigen::Vector3f> pos{{2, 0, -2}, {0, 2, -2}, {-2, 0, -2}};
        std::vector<Eigen::Vector3i> ind{{0, 1, 2}};
        auto pos_id = r.load_positions(pos);
        auto ind_id = r.load_indices(ind);

        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45, 1, 0.1f, 50));
        if (!command_line) {
            std::cout << "A/D: rotate by +/-10 degrees; Esc: exit.\n";
        }
        while (true) {
            r.clear(rst::Buffers::Color | rst::Buffers::Depth);
            r.set_model(arbitrary_axis ? get_rotation(axis, angle) : get_model_matrix(angle));
            r.draw(pos_id, ind_id, rst::Primitive::Triangle);
            cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
            image.convertTo(image, CV_8UC3, 1.0f);
            if (command_line) {
                if (!cv::imwrite(filename, image)) {
                    throw std::runtime_error("Failed to save image: " + filename);
                }
                std::cout << "Saved " << filename << '\n';
                break;
            }
            cv::imshow("image", image);
            const int key = cv::waitKey(10);
            if (key == 27 || cv::getWindowProperty("image", cv::WND_PROP_VISIBLE) < 1)
                break;
            if (key == 'a' || key == 'A')
                angle += 10.0f;
            if (key == 'd' || key == 'D')
                angle -= 10.0f;
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
