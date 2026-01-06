#include "StreamPlayer.h"
#include <iostream>

constexpr uint64_t ONE_SECOND_NS = 1'000'000'000ULL; // 1 s
constexpr uint64_t EARLY_TOLERANCE_NS = 5'000'000;   // 5 ms
constexpr uint64_t MAX_LATE_NS = 30'000'000;         // 30 ms

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
                       (static_cast<float>(m_BufferNs) / ONE_SECOND_NS)));

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

  size_t read = self->m_ReadPos.load(std::memory_order::acquire);
  size_t write = self->m_WritePos.load(std::memory_order::acquire);

  bool played = false;

  for (unsigned long i = 0; i < frameCount * self->m_Channels; ++i) {
    if (!self->m_PlaybackStarted.load(std::memory_order::acquire) ||
        read == write)
      out[i] = 0.0f;
    else {
      out[i] = self->m_RingBuffer[read];
      read = (read + 1) % self->m_RingBuffer.size();
      played = true;
    }
  }

  self->m_ReadPos.store(read, std::memory_order::release);

  if (played)
    self->m_FramesPlayed.fetch_add(frameCount, std::memory_order::release);

  return paContinue;
}

void StreamPlayer::AudioBuffer(const std::vector<float> &buffer, uint64_t pts) {

  if (!m_Started.load(std::memory_order::acquire)) {
    m_StartPTS = pts;
    m_PlaybackStartPTS = pts + m_BufferNs;
    m_Started.store(true, std::memory_order::release);
  }

  size_t write = m_WritePos.load(std::memory_order::acquire);
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

  if (!m_PlaybackStarted.load(std::memory_order::acquire) &&
      pts >= m_PlaybackStartPTS)
    Start();
}

uint64_t StreamPlayer::AudioClock() const {
  if (!m_PlaybackStarted.load(std::memory_order::acquire))
    return m_PlaybackStartPTS;

  uint64_t frames = m_FramesPlayed.load(std::memory_order::acquire);
  uint64_t ns = (frames * ONE_SECOND_NS) / m_SampleRate;
  return m_StartPTS + ns;
}

void StreamPlayer::VideoBuffer(const std::vector<uint8_t> &buffer,
                               uint64_t ns) {
  std::lock_guard<std::mutex> lock(m_VideoMutex);
  m_VideoQueue.push_back({buffer, ns});
}

void StreamPlayer::Start() {
  m_PlaybackStarted.store(true, std::memory_order::release);
  Pa_StartStream(m_Stream);
}

std::vector<uint8_t> StreamPlayer::Update() {
  if (!m_PlaybackStarted.load(std::memory_order::acquire))
    return {};

  uint64_t audioNs = AudioClock();
  std::vector<uint8_t> result;

  std::lock_guard<std::mutex> lock(m_VideoMutex);

  while (!m_VideoQueue.empty() && m_VideoQueue.front().ns < audioNs - MAX_LATE_NS)
    m_VideoQueue.pop_front();

  if (m_VideoQueue.empty())
    return {};

  auto &frame = m_VideoQueue.front();

  if (frame.ns <= audioNs + EARLY_TOLERANCE_NS) {
    result = frame.data;
    m_VideoQueue.pop_front();
  }

  return result;
}