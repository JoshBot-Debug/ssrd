#pragma once

#include "Record.h"

#include <atomic>
#include <thread>

#include <audioclient.h>
#include <mmdeviceapi.h>

class RecordWindows : public Record
{
public:
  struct Stream
  {
    int id = 0;
    StreamCallback onStream = nullptr;

    WAVEFORMATEX *format = nullptr;
    IAudioCaptureClient *captureClient = nullptr;
    IAudioClient *audioClient = nullptr;
    IMMDevice *device = nullptr;
    IMMDeviceEnumerator *enumerator = nullptr;
    UINT32 bufferFrameCount = 0;
  };

  struct Data
  {
    Stream internalStream{};
    Stream microphoneStream{};

    std::atomic<bool> isRunning{false};

    std::function<void()> onBegin = nullptr;
    std::function<void()> onEnd = nullptr;
  };

private:
  Data m_Data;
  std::thread m_Thread;
  std::thread m_SilentRenderThread;

  void StartSilentRender();
  void Capture(Stream *stream, EDataFlow flow, DWORD streamFlags);
  void Step(Stream *stream);

public:
  ~RecordWindows();

  void Begin() override;
  void End() override;

  bool IsRecording() const override { return m_Data.isRunning.load(); }

  void OnBegin(const std::function<void()>& callback) override { m_Data.onBegin = callback; };

  void OnEnd(const std::function<void()>& callback) override { m_Data.onEnd = callback; };

  void OnInternalStream(const StreamCallback &callback) override;
  void OnMicrophoneStream(const StreamCallback &callback) override;
};
