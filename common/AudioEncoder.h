#pragma once
#undef min
#undef max
#include <algorithm>
#include <mutex>
#include <opus/opus.h>
#include <stdexcept>
#include <vector>

class AudioEncoder {
private:
  OpusEncoder *m_OpusEncoder = nullptr;
  int m_SampleRate;
  int m_Channels;
  int m_MaxBytes;
  std::vector<int16_t> m_Buffer;
  std::mutex m_BufferMutex;

public:
  AudioEncoder(int sampleRate, int bitrate = 64, int channels = 2,
               int maxBytes = 4096)
      : m_SampleRate(sampleRate), m_Channels(channels), m_MaxBytes(maxBytes) {

    int error;
    m_OpusEncoder = opus_encoder_create(m_SampleRate, m_Channels,
                                        OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK)
      throw std::runtime_error("Failed to create Opus encoder");

    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_BITRATE(bitrate * 1000));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_VBR(1));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_VBR_CONSTRAINT(1));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(m_OpusEncoder, OPUS_SET_PACKET_LOSS_PERC(5));

    m_Buffer.resize(m_MaxBytes, 0);
  }

  ~AudioEncoder() { opus_encoder_destroy(m_OpusEncoder); }

  std::vector<unsigned char> Encode(const float *ieeFloat, uint32_t samples,
                                    int frameSize) {
    std::lock_guard<std::mutex> lock(m_BufferMutex);

    std::vector<int16_t> pcm16 = IEEEFloatToPCM16(ieeFloat, samples);

    m_Buffer.insert(m_Buffer.end(), pcm16.begin(), pcm16.end());

    if (m_Buffer.size() >= frameSize * m_Channels) {
      std::vector<unsigned char> output(m_MaxBytes);

      int bytesWritten = opus_encode(m_OpusEncoder, m_Buffer.data(), frameSize,
                                     output.data(), m_MaxBytes);
      if (bytesWritten < 0) {
        fprintf(stderr, "Opus encoding failed: %s\n",
                opus_strerror(bytesWritten));
        return {};
      }

      m_Buffer.erase(m_Buffer.begin(),
                     m_Buffer.begin() + frameSize * m_Channels);

      output.resize(bytesWritten);

      return output;
    }

    return {};
  }

  std::vector<float> LinearResample(const std::vector<float> &input,
                                    double srcRate, double dstRate,
                                    int channels = 2) {
    if (srcRate == dstRate)
      return input;

    if (input.empty())
      return {};

    if (channels <= 0)
      throw std::invalid_argument("channels must be positive");

    double ratio = dstRate / srcRate;
    if (ratio <= 0)
      throw std::invalid_argument("Sample rates must be positive");

    size_t inputFrames = input.size() / channels;
    size_t outputFrames = static_cast<size_t>(inputFrames * ratio);
    std::vector<float> output(outputFrames * channels);

    for (size_t i = 0; i < outputFrames; ++i) {
      double srcPos = i / ratio;
      size_t idx0 = static_cast<size_t>(srcPos);
      size_t idx1 = (idx0 + 1 < inputFrames) ? idx0 + 1 : idx0;
      double frac = srcPos - idx0;

      for (int ch = 0; ch < channels; ++ch) {
        float sample0 = input[idx0 * channels + ch];
        float sample1 = input[idx1 * channels + ch];
        output[i * channels + ch] =
            static_cast<float>((1.0 - frac) * sample0 + frac * sample1);
      }
    }

    return output;
  }

  std::vector<int16_t> IEEEFloatToPCM16(const float *ieeFloat,
                                        int samples) const {
    std::vector<int16_t> pcm16(samples);
    for (size_t i = 0; i < samples; ++i) {
      float s = ieeFloat[i];
      if (s > 1.0f)
        s = 1.0f;
      if (s < -1.0f)
        s = -1.0f;
      pcm16[i] = static_cast<int16_t>(s * 32767.0f);
    }
    return pcm16;
  }
};
