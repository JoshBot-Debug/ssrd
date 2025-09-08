#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class Encode {
private:
  int m_Width = 0, m_Height = 0;

  uint64_t m_Pts = 0;

  AVCodecContext *m_Ctx = nullptr;
  AVFrame *m_FrameRGB = nullptr;
  AVFrame *m_FrameYUV = nullptr;
  SwsContext *m_Sws_ctx = nullptr;

public:
  Encode() = default;
  ~Encode();

  void initialize(int width, int height);

  void encodeFrame(const std::vector<uint8_t> &buffer,
                   std::vector<uint8_t> &output);
};
