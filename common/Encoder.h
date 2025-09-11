#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class Encoder {
private:
  int m_Width = 0;
  int m_Height = 0;

  uint64_t m_Pts = 0;

  AVCodecContext *m_Ctx = nullptr;
  AVFrame *m_FrameRGB = nullptr;
  AVFrame *m_FrameYUV = nullptr;
  SwsContext *m_Sws_ctx = nullptr;

public:
  Encoder() = default;
  ~Encoder();

  void initialize(int width, int height);

  std::vector<uint8_t> encode(const std::vector<uint8_t> &buffer);
};