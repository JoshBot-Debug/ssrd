#include "StreamPlayer.h"
#include <iostream>

const uint64_t ONE_SECOND = 1'000'000'000ULL;

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
                       (static_cast<float>(m_BufferNs) / ONE_SECOND)));

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

  size_t read = self->m_ReadPos.load(std::memory_order::relaxed);
  size_t write = self->m_WritePos.load(std::memory_order::acquire);

  for (unsigned long i = 0; i < frameCount * self->m_Channels; ++i) {
    if (!self->m_PlaybackStarted || read == write)
      out[i] = 0.0f;
    else {
      out[i] = self->m_RingBuffer[read];
      read = (read + 1) % self->m_RingBuffer.size();
    }
  }

  self->m_ReadPos.store(read, std::memory_order::release);

  return paContinue;
}

void StreamPlayer::AudioBuffer(const std::vector<float> &buffer, uint64_t ns) {

  if (!m_Started.load(std::memory_order::relaxed)) {
    m_PlaybackStartNs = ns + m_BufferNs;
    m_AudioDeviceStartTime = Pa_GetStreamTime(m_Stream);
    m_Started.store(true, std::memory_order::release);
  }

  size_t write = m_WritePos.load(std::memory_order::relaxed);
  size_t read = m_ReadPos.load(std::memory_order::acquire);
  size_t size = m_RingBuffer.size();

  for (float s : buffer) {
    size_t next = (write + 1) % size;
    if (next == read)
      break;
    m_RingBuffer[write] = s;
    write = next;
  }

  m_WritePos.store(write, std::memory_order::release);

  if (!m_PlaybackStarted.load(std::memory_order::relaxed) &&
      ns >= m_PlaybackStartNs)
    Start();
}

uint64_t StreamPlayer::AudioClockNs() const {
  if (!m_PlaybackStarted.load(std::memory_order::relaxed))
    return m_AudioStartNs;

  double deviceTime = Pa_GetStreamTime(m_Stream);
  double delta = deviceTime - m_AudioDeviceStartTime;
  return m_AudioStartNs + uint64_t(delta * ONE_SECOND);
}

void StreamPlayer::VideoBuffer(const std::vector<uint8_t> &buffer,
                               uint64_t ns) {
  std::lock_guard<std::mutex> lock(m_VideoMutex);
  m_VideoQueue.push_back({buffer, ns});
}

void StreamPlayer::Start() {
  m_PlaybackStarted.store(true, std::memory_order::release);
  Pa_StartStream(m_Stream);
  m_AudioStartNs = m_PlaybackStartNs;
  m_AudioDeviceStartTime = Pa_GetStreamTime(m_Stream);
}

std::vector<uint8_t> StreamPlayer::Update() {
  std::lock_guard<std::mutex> lock(m_VideoMutex);

  if (!m_PlaybackStarted.load(std::memory_order::relaxed) ||
      m_VideoQueue.empty())
    return {};

  uint64_t audioNs = AudioClockNs();
  auto &frame = m_VideoQueue.front();

  // Drop late frames
  if (frame.ns < audioNs - 15'000'000) {
    m_VideoQueue.pop_front();
    return {};
  }

  // Too early → wait
  if (frame.ns > audioNs) {
    return {};
  }

  // Exact time → present ONE frame
  auto data = frame.data;
  m_VideoQueue.pop_front();
  return data;
}