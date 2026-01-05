#include "StreamPlayer.h"
#include <iostream>

StreamPlayer::StreamPlayer(int sampleRate, int channels, uint64_t bufferNs)
    : m_SampleRate(sampleRate), m_Channels(channels), m_BufferNs(bufferNs) {
  Pa_Initialize();

  PaStreamParameters out{};
  out.device = Pa_GetDefaultOutputDevice();
  out.channelCount = m_Channels;
  out.sampleFormat = paFloat32;
  out.suggestedLatency = Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;

  m_RingBuffer.resize((m_SampleRate * m_Channels) +
                      (m_SampleRate * m_Channels *
                       (static_cast<float>(m_BufferNs) / 1'000'000'000ULL)));

  Pa_OpenStream(&m_Stream, nullptr, &out, m_SampleRate, 256, paNoFlag, Callback,
                this);
}

StreamPlayer::~StreamPlayer() {
  Pa_StopStream(m_Stream);
  Pa_CloseStream(m_Stream);
  Pa_Terminate();
}

int StreamPlayer::Callback(const void *, void *output, unsigned long frameCount,
                           const PaStreamCallbackTimeInfo *,
                           PaStreamCallbackFlags, void *userData) {
  auto *self = static_cast<StreamPlayer *>(userData);
  float *out = static_cast<float *>(output);

  std::lock_guard<std::mutex> lock(self->m_Mutex);

  for (unsigned long i = 0; i < frameCount * self->m_Channels; ++i) {
    if (!self->m_PlaybackStarted || self->m_ReadPos == self->m_WritePos) {
      out[i] = 0.0f; // silence on underrun
    } else {
      out[i] = self->m_RingBuffer[self->m_ReadPos];
      self->m_ReadPos = (self->m_ReadPos + 1) % self->m_RingBuffer.size();
    }
  }

  if (self->m_PlaybackStarted)
    self->m_PlayedFrames += frameCount;

  return paContinue;
}

void StreamPlayer::AudioBuffer(const std::vector<float> &buffer, uint64_t ns) {

  if (!m_Started) {
    m_StartPtsNs = ns;
    m_PlaybackStartPtsNs = m_StartPtsNs + m_BufferNs;
    m_Started = true;
  }

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    for (float s : buffer) {
      m_RingBuffer[m_WritePos] = s;
      m_WritePos = (m_WritePos + 1) % m_RingBuffer.size();
    }
  }

  if (!m_PlaybackStarted && ns >= m_PlaybackStartPtsNs)
    Start();

  if (m_PlaybackStarted)
    m_AudioClockNs =
        m_StartPtsNs + (m_PlayedFrames * 1'000'000'000ULL) / m_SampleRate;
}

void StreamPlayer::VideoBuffer(const std::vector<uint8_t> &buffer,
                               uint64_t ns) {
  m_VideoQueue.push_back({buffer, ns});
}

void StreamPlayer::Start() {
  m_ReadPos = 0;
  m_PlaybackStarted = true;
  Pa_StartStream(m_Stream);
}

std::vector<uint8_t> StreamPlayer::Update() {

  if (!m_Started || m_VideoQueue.empty())
    return {};

  const uint64_t toleranceNs = 15'000'000;

  auto &frame = m_VideoQueue.front();

  if (frame.ns <= m_AudioClockNs + toleranceNs) {
    auto data = frame.data;
    m_VideoQueue.pop_front();
    return data;
  }

  return {};
}