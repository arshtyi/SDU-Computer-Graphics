#pragma once
#include "mesh.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace subdivision {
cv::Mat render_mesh(const Mesh &mesh, const Mesh &original, std::size_t level, const Mesh *previous = nullptr);
cv::Mat render_overview(const std::vector<cv::Mat> &frames);
void save_image(const cv::Mat &image, const std::string &filename);
} // namespace subdivision
