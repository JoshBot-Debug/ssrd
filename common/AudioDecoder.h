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
    m_OpusDecoder = opus_decoder_create(sampleRate, channels, &error);
    if (error != OPUS_OK)
      throw std::runtime_error("Failed to create Opus decoder");
  }

  ~AudioDecoder() {
    if (m_OpusDecoder)
      opus_decoder_destroy(m_OpusDecoder);
  }

  std::vector<float> Decode(const std::vector<unsigned char> &buffer,
                            int frameSize) {
    std::vector<opus_int16> pcm16(frameSize * 6 * m_Channels);

    int decodedFrameSize =
        opus_decode(m_OpusDecoder, buffer.size() == 0 ? nullptr : buffer.data(),
                    buffer.size(), pcm16.data(), frameSize * 6, 0);

    if (decodedFrameSize < 0) {
      fprintf(stderr, "Opus decode error: %s\n",
              opus_strerror(decodedFrameSize));
      return std::vector<float>(frameSize, 0.0f);
    }

    std::vector<float> output(decodedFrameSize * m_Channels);

    for (int i = 0; i < decodedFrameSize * m_Channels; ++i)
      output[i] = pcm16[i] / 32768.0f;

    return output;
  }
};
