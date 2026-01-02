#pragma once

#include "Record.h"

#include <thread>
#include <functional>
#include <libportal/portal.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

class RecordLinux : public Record {
public:
  struct Stream {
    int id = 0;
    pw_stream *stream = nullptr;
    pw_stream_events events = {};
    spa_audio_info format = {};

    StreamCallback onStream = nullptr;
  };

  struct Data {
    spa_hook eventListener;
    pw_main_loop *loop = nullptr;
    pw_main_loop_events loopEvents;
    bool isRunning = false;

    Stream internalStream{};
    Stream microphoneStream{};

    std::function<void()> onBegin = nullptr;
    std::function<void()> onEnd = nullptr;
  };

  std::thread m_Thread;

private:
  static void OnStreamParamsChange(void *userData, uint32_t id,
                                   const spa_pod *param);

  static void OnStreamProcess(void *userData);

  static void OnQuit(void *userData, int signalNumber);

  void Initialize();

private:
  Data m_Data;

public:
  ~RecordLinux();

  void Begin() override;

  void End() override;

  bool IsRecording() const override { return m_Data.isRunning; }

  void OnBegin(const std::function<void()> &callback) override { m_Data.onBegin = callback; };

  void OnEnd(const std::function<void()> &callback) override { m_Data.onEnd = callback; };

  void OnInternalStream(const StreamCallback &callback) override;

  void OnMicrophoneStream(const StreamCallback &callback) override;
};
