#pragma once

#include <functional>
#include <stdint.h>
#include <vector>

struct Chunk {
  std::vector<float> &buffer;
  uint32_t frames;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t bits;
};

using StreamCallback = std::function<void(const Chunk &chunk)>;

class Record {
public:
  virtual void Begin() = 0;

  virtual void End() = 0;

  virtual bool IsRecording() const = 0;

  virtual void OnBegin(const std::function<void()> &callback) = 0;

  virtual void OnEnd(const std::function<void()> &callback) = 0;

  virtual void OnInternalStream(const StreamCallback &callback) = 0;

  virtual void OnMicrophoneStream(const StreamCallback &callback) = 0;
};
