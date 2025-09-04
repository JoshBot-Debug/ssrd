#pragma once

#include <array>
#include <fstream>
#include <vector>

#include <spa/param/video/type-info.h>

#ifdef DEBUG

#include <iostream>

template <typename... T>
void Log(const char *file, int line, const char *functionName,
         const T &...args) {
  std::cout << "LOG " << file << ":" << line << " (" << functionName << "):";
  ((std::cout << " " << args), ...);
  std::cout << std::endl;
}

#define LOG(...) Log(__FILE__, __LINE__, __func__, __VA_ARGS__)

#else
#define LOG(...)
#endif

struct PixelFormatInfo {
  int bytesPerPixel;
  std::array<int, 3> order;
};

static inline PixelFormatInfo pixelFormatInfo(enum spa_video_format format) {
  switch (format) {
  case SPA_VIDEO_FORMAT_RGB: // R G B
    return {3, {0, 1, 2}};
  case SPA_VIDEO_FORMAT_BGR: // B G R
    return {3, {2, 1, 0}};

  case SPA_VIDEO_FORMAT_RGBx: // R G B X
  case SPA_VIDEO_FORMAT_RGBA: // R G B A
    return {4, {0, 1, 2}};
  case SPA_VIDEO_FORMAT_BGRx: // B G R X
  case SPA_VIDEO_FORMAT_BGRA: // B G R A
    return {4, {2, 1, 0}};

  case SPA_VIDEO_FORMAT_xRGB: // X R G B
  case SPA_VIDEO_FORMAT_ARGB: // A R G B
    return {4, {1, 2, 3}};
  case SPA_VIDEO_FORMAT_xBGR: // X B G R
  case SPA_VIDEO_FORMAT_ABGR: // A B G R
    return {4, {3, 2, 1}};

  case SPA_VIDEO_FORMAT_GRAY8: // Grayscale → R=G=B=Y
    return {1, {0, 0, 0}};

  // Planar YUV formats need special handling (conversion)
  case SPA_VIDEO_FORMAT_I420:
  case SPA_VIDEO_FORMAT_YV12:
  case SPA_VIDEO_FORMAT_NV12:
  case SPA_VIDEO_FORMAT_YUY2:
    throw std::runtime_error("Unhandled planar video format");

  default:
    throw std::runtime_error("Unhandled video format");
  }
}

static inline void writePPM(const std::string &filename, const uint8_t *frame,
                            int width, int height, spa_video_format format) {

  PixelFormatInfo formatInfo = pixelFormatInfo(format);
  int stride = width * formatInfo.bytesPerPixel;

  std::ofstream out(filename, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Failed to open " << filename << std::endl;
    return;
  }

  // PPM header
  out << "P6\n" << width << " " << height << "\n255\n";

  std::vector<uint8_t> row_rgb(width * 3);

  for (int y = 0; y < height; y++) {
    const uint8_t *src = frame + y * stride;
    uint8_t *dst = row_rgb.data();

    for (int x = 0; x < width; x++) {
      dst[0] = src[formatInfo.order[0]]; // R
      dst[1] = src[formatInfo.order[1]]; // G
      dst[2] = src[formatInfo.order[2]]; // B
      src += formatInfo.bytesPerPixel;
      dst += 3;
    }

    out.write(reinterpret_cast<const char *>(row_rgb.data()), row_rgb.size());
  }

  out.close();

  LOG("Wrote", filename, "(", width, "x", height, ")");
}