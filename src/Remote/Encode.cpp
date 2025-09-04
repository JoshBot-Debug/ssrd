#include "Encode.h"

#include <iostream>

Encode::~Encode() {
  sws_freeContext(m_Sws_ctx);
  av_frame_free(&m_FrameRGB);
  av_frame_free(&m_FrameYUV);
  avcodec_free_context(&m_Ctx);
}

void Encode::initialize(int width, int height) {
  m_Width = width;
  m_Height = height;

  m_FrameRGB = av_frame_alloc();
  if (!m_FrameRGB)
    throw std::runtime_error("Failed to allocate RGB frame");

  m_FrameRGB->format = AV_PIX_FMT_RGB24;
  m_FrameRGB->width = width;
  m_FrameRGB->height = height;
  if (av_image_alloc(m_FrameRGB->data, m_FrameRGB->linesize, m_Width, m_Height,
                     AV_PIX_FMT_RGB24, 32) < 0)
    throw std::runtime_error("Failed to allocate RGB frame data");

  m_FrameYUV = av_frame_alloc();
  if (!m_FrameYUV)
    throw std::runtime_error("Failed to allocate YUV frame");

  m_FrameYUV->format = AV_PIX_FMT_YUV420P;
  m_FrameYUV->width = width;
  m_FrameYUV->height = height;
  if (av_image_alloc(m_FrameYUV->data, m_FrameYUV->linesize, m_Width, m_Height,
                     AV_PIX_FMT_YUV420P, 32) < 0)
    throw std::runtime_error("Failed to allocate YUV frame data");

  m_Sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24, width, height,
                             AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr,
                             nullptr, nullptr);
  if (!m_Sws_ctx)
    throw std::runtime_error("Failed to get swscale context");

  //  Hardware encode, need to add cpu fallback
  AVCodec *codec = avcodec_find_encoder_by_name("libx264");
  if (!codec)
    throw std::runtime_error("Failed to find x264 encoder");

  m_Ctx = avcodec_alloc_context3(codec);
  if (!m_Ctx)
    throw std::runtime_error("Failed to allocate codec context");

  m_Ctx->width = width;
  m_Ctx->height = height;
  m_Ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  m_Ctx->time_base = AVRational{1, 60};
  m_Ctx->framerate = AVRational{60, 1};
  m_Ctx->max_b_frames = 0;

  AVDictionary *opts = nullptr;
  av_dict_set(&opts, "preset", "ultrafast", 0);
  av_dict_set(&opts, "tune", "zerolatency", 0);

  if (avcodec_open2(m_Ctx, codec, &opts) < 0)
    throw std::runtime_error("Failed to open codec");

  av_dict_free(&opts);
}

void Encode::encodeFrame(const std::vector<uint8_t> &buffer,
                         std::vector<uint8_t> &output) {
  output.clear();
  
  if (buffer.size() < static_cast<size_t>(m_Width * m_Height * 3))
    throw std::runtime_error("Input buffer too small for frame dimensions");

  for (int y = 0; y < m_Height; y++)
    memcpy(m_FrameRGB->data[0] + y * m_FrameRGB->linesize[0],
           buffer.data() + y * m_Width * 3, m_Width * 3);

  // convert RGB -> YUV
  sws_scale(m_Sws_ctx, m_FrameRGB->data, m_FrameRGB->linesize, 0, m_Height,
            m_FrameYUV->data, m_FrameYUV->linesize);

  m_FrameYUV->pts = m_Pts++;

  // encode frame
  AVPacket *pkt = av_packet_alloc();
  if (!pkt)
    throw std::runtime_error("Failed to allocate packet");

  if (avcodec_send_frame(m_Ctx, m_FrameYUV) < 0) {
    av_packet_free(&pkt);
    throw std::runtime_error("Error sending frame to encoder");
  }

  while (true) {
    int ret = avcodec_receive_packet(m_Ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    } else if (ret < 0) {
      av_packet_free(&pkt);
      throw std::runtime_error("Error receiving encoded packet");
    }
    output.insert(output.end(), pkt->data, pkt->data + pkt->size);
    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
}
