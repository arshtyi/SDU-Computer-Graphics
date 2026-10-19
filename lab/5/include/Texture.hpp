#pragma once
#include "Triangle.hpp"
#include <opencv2/core.hpp>
#include <string>

enum class Filter { Nearest, Bilinear };

class Texture {
  public:
    explicit Texture(const std::string &path);
    explicit Texture(const cv::Mat &bgr);
    int width() const { return rgb_.cols; }
    int height() const { return rgb_.rows; }
    Vec3 getColor(float u, float v) const;
    Vec3 getColorBilinear(float u, float v) const;
    Vec3 sample(const Vec2 &uv, Filter filter) const;
    float heightAt(const Vec2 &uv, Filter filter) const;

  private:
    cv::Mat rgb_;
    Vec2 texel(float u, float v) const;
    Vec3 pixel(int x, int y) const;
};
