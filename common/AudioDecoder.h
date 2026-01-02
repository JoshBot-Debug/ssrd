#pragma once
#include <cstdio>
#include <opus/opus.h>
#include <stdexcept>
#include <vector>

class AudioDecoder {
private:
  OpusDecoder *m_OpusDecoder = nullptr;
  int m_SampleRate;
  int m_Channels;

public:
  AudioDecoder(int sampleRate, int channels = 2)
      : m_SampleRate(sampleRate), m_Channels(channels) {
    int error;
    m_OpusDecoder = opus_decoder_create(m_SampleRate, m_Channels, &error);
    if (error != OPUS_OK)
      throw std::runtime_error("Failed to create Opus decoder");
  }

  ~AudioDecoder() {
    if (m_OpusDecoder)
      opus_decoder_destroy(m_OpusDecoder);
  }

  std::vector<float> Decode(const std::vector<unsigned char> &buffer, int maxFrameSize) {
    if (buffer.size() <= 0)
      return {};

    std::vector<opus_int16> pcm16(maxFrameSize * m_Channels);

    int frameSize = opus_decode(m_OpusDecoder, buffer.data(), buffer.size(),
                                pcm16.data(), maxFrameSize, 0);

    if (frameSize < 0) {
      fprintf(stderr, "Opus decode error: %s\n", opus_strerror(frameSize));
      return {};
    }

    std::vector<float> output(frameSize * m_Channels);

    for (int i = 0; i < frameSize * m_Channels; ++i)
      output[i] = pcm16[i] / 32768.0f;

    return output;
  }
};
