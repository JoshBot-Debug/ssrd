#pragma once

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

class Encode {
private:
  int m_Width, m_Height;

  uint64_t m_Pts;

  AVCodecContext *m_Ctx;
  AVFrame *m_FrameRGB;
  AVFrame *m_FrameYUV;
  SwsContext *m_Sws_ctx;

public:
  Encode() = default;
  ~Encode();

  void initialize(int width, int height);

  void encodeFrame(const std::vector<uint8_t> &buffer,
                   std::vector<uint8_t> &output);
};
