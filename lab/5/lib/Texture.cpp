#include "Texture.hpp"
#include <algorithm>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

Texture::Texture(const std::string &path) : Texture(cv::imread(path, cv::IMREAD_COLOR)) {}

Texture::Texture(const cv::Mat &bgr) {
    if (bgr.empty() || bgr.type() != CV_8UC3)
        throw std::runtime_error("Texture must be a nonempty 8-bit BGR image");
    cv::cvtColor(bgr, rgb_, cv::COLOR_BGR2RGB);
}

Vec2 Texture::texel(float u, float v) const {
    if (!std::isfinite(u) || !std::isfinite(v))
        throw std::invalid_argument("Nonfinite texture coordinate");
    return {std::clamp(u, 0.f, 1.f) * (width() - 1), (1 - std::clamp(v, 0.f, 1.f)) * (height() - 1)};
}

Vec3 Texture::pixel(int x, int y) const {
    const auto c = rgb_.at<cv::Vec3b>(y, x);
    return {float(c[0]), float(c[1]), float(c[2])};
}

Vec3 Texture::getColor(float u, float v) const {
    const Vec2 p = texel(u, v);
    return pixel(int(std::lround(p.x())), int(std::lround(p.y())));
}

Vec3 Texture::getColorBilinear(float u, float v) const {
    const Vec2 p = texel(u, v);
    const int x0 = int(std::floor(p.x())), y0 = int(std::floor(p.y()));
    const int x1 = std::min(x0 + 1, width() - 1), y1 = std::min(y0 + 1, height() - 1);
    const float s = p.x() - x0, t = p.y() - y0;
    return (1 - t) * ((1 - s) * pixel(x0, y0) + s * pixel(x1, y0)) + t * ((1 - s) * pixel(x0, y1) + s * pixel(x1, y1));
}

Vec3 Texture::sample(const Vec2 &uv, Filter filter) const {
    return filter == Filter::Bilinear ? getColorBilinear(uv.x(), uv.y()) : getColor(uv.x(), uv.y());
}

float Texture::heightAt(const Vec2 &uv, Filter filter) const { return sample(uv, filter).dot(Vec3(0.2126f, 0.7152f, 0.0722f)) / 255.f; }
