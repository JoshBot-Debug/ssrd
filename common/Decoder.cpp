#include "Decoder.h"

Decoder::~Decoder() {
  if (m_Sws_ctx) {
    sws_freeContext(m_Sws_ctx);
    m_Sws_ctx = nullptr;
  }
  if (m_FrameRGB) {
    av_frame_free(&m_FrameRGB);
    m_FrameRGB = nullptr;
  }
  if (m_FrameYUV) {
    av_frame_free(&m_FrameYUV);
    m_FrameYUV = nullptr;
  }
  if (m_Ctx) {
    avcodec_free_context(&m_Ctx);
    m_Ctx = nullptr;
  }
}

void Decoder::initialize(int width, int height) {
  m_Width = width;
  m_Height = height;

  const AVCodec *codec = avcodec_find_decoder_by_name("h264");
  if (!codec)
    throw std::runtime_error("Failed to find H.264 decoder");

  m_Ctx = avcodec_alloc_context3(codec);
  if (!m_Ctx)
    throw std::runtime_error("Failed to allocate decoder context");

  if (avcodec_open2(m_Ctx, codec, nullptr) < 0)
    throw std::runtime_error("Failed to open decoder");

  m_FrameYUV = av_frame_alloc();
  m_FrameRGB = av_frame_alloc();
  if (!m_FrameYUV || !m_FrameRGB)
    throw std::runtime_error("Failed to allocate frames");

  m_FrameRGB->format = AV_PIX_FMT_RGB24;
  m_FrameRGB->width = width;
  m_FrameRGB->height = height;

  if (av_image_alloc(m_FrameRGB->data, m_FrameRGB->linesize, width, height,
                     AV_PIX_FMT_RGB24, 32) < 0)
    throw std::runtime_error("Failed to allocate RGB frame buffer");

  m_Sws_ctx =
      sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height,
                     AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!m_Sws_ctx)
    throw std::runtime_error("Failed to create sws context");
}

std::vector<uint8_t> Decoder::decode(const std::vector<uint8_t> &encoded) {
  std::vector<uint8_t> output;

  AVPacket *pkt = av_packet_alloc();
  if (!pkt)
    throw std::runtime_error("Failed to allocate packet");

  av_new_packet(pkt, encoded.size());
  memcpy(pkt->data, encoded.data(), encoded.size());

  if (avcodec_send_packet(m_Ctx, pkt) < 0) {
    av_packet_free(&pkt);
    throw std::runtime_error("Error sending packet to decoder");
  }

  while (true) {
    int ret = avcodec_receive_frame(m_Ctx, m_FrameYUV);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0) {
      av_packet_free(&pkt);
      throw std::runtime_error("Error receiving frame from decoder");
    }

    // Convert YUV → RGB
    sws_scale(m_Sws_ctx, m_FrameYUV->data, m_FrameYUV->linesize, 0, m_Height,
              m_FrameRGB->data, m_FrameRGB->linesize);

    // Copy RGB data into vector
    output.resize(m_Width * m_Height * 3);
    for (int y = 0; y < m_Height; y++) {
      memcpy(output.data() + y * m_Width * 3,
             m_FrameRGB->data[0] + y * m_FrameRGB->linesize[0], m_Width * 3);
    }
  }

  av_packet_free(&pkt);
  return output;
}
