#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class Decoder {
private:
  int m_Width = 0;
  int m_Height = 0;

  AVCodecContext *m_Ctx = nullptr;
  AVFrame *m_FrameYUV = nullptr;
  AVFrame *m_FrameRGB = nullptr;
  SwsContext *m_Sws_ctx = nullptr;

public:
  Decoder() = default;
  ~Decoder();

  void initialize(int width, int height);
  std::vector<uint8_t> decode(const std::vector<uint8_t> &encoded);
};
