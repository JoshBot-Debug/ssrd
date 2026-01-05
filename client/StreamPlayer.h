#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <portaudio.h>
#include <vector>

class StreamPlayer {
private:
  struct VideoFrame {
    std::vector<uint8_t> data;
    uint64_t ns;
  };

  std::deque<VideoFrame> m_VideoQueue;

  static int Callback(const void *input, void *output, unsigned long frameCount,
                      const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags,
                      void *userData);

  PaStream *m_Stream = nullptr;
  int m_SampleRate;
  int m_Channels;
  uint64_t m_BufferNs;

  std::vector<float> m_RingBuffer;
  size_t m_ReadPos = 0;
  size_t m_WritePos = 0;

  std::atomic<uint64_t> m_PlayedFrames{0};
  std::mutex m_Mutex;

  uint64_t m_AudioClockNs = 0;
  bool m_Started = false;
  uint64_t m_StartPtsNs = 0;

  uint64_t m_PlaybackStartPtsNs = 0;
  bool m_PlaybackStarted = false;

  void Start();

public:
  StreamPlayer(int sampleRate, int channels = 2,
               uint64_t bufferNs = 100'000'000);
  ~StreamPlayer();

  void AudioBuffer(const std::vector<float> &buffer, uint64_t ns);
  void VideoBuffer(const std::vector<uint8_t> &buffer, uint64_t ns);

  std::vector<uint8_t> Update();
};