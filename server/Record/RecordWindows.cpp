#include "RecordWindows.h"
#include <initguid.h>

#include <audiopolicy.h>
#include <comdef.h>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>

#include "Log.h"

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

#define EXIT_ON_ERROR(hres)                                                    \
  if (FAILED(hres)) {                                                          \
    LOG_TO_FILE("error.log", "Exit on error: ", std::to_string(hres));         \
    throw std::runtime_error("Exit on error: " + std::to_string(hres));        \
  }

#define SAFE_RELEASE(punk)                                                     \
  if ((punk) != NULL) {                                                        \
    (punk)->Release();                                                         \
    (punk) = NULL;                                                             \
  }

const IID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

void RecordWindows::StartSilentRender() {
  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *audioClient = nullptr;
  IAudioRenderClient *renderClient = nullptr;
  WAVEFORMATEX *format = nullptr;

  EXIT_ON_ERROR(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                 IID_IMMDeviceEnumerator,
                                 (void **)&enumerator));

  EXIT_ON_ERROR(
      enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
  EXIT_ON_ERROR(device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                                 (void **)&audioClient));
  EXIT_ON_ERROR(audioClient->GetMixFormat(&format));
  EXIT_ON_ERROR(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                        REFTIMES_PER_SEC, 0, format, nullptr));

  UINT32 bufferFrameCount = 0;
  EXIT_ON_ERROR(audioClient->GetBufferSize(&bufferFrameCount));
  EXIT_ON_ERROR(
      audioClient->GetService(IID_IAudioRenderClient, (void **)&renderClient));
  EXIT_ON_ERROR(audioClient->Start());

  const UINT32 frameSize = format->nBlockAlign;

  m_SilentRenderThread = std::thread(
      [this, audioClient, renderClient, bufferFrameCount, frameSize]() mutable {
        BYTE *data = nullptr;

        while (m_Data.isRunning.load()) {
          Sleep(10);

          UINT32 padding = 0;
          if (FAILED(audioClient->GetCurrentPadding(&padding)))
            break;

          UINT32 framesAvailable = bufferFrameCount - padding;
          if (framesAvailable == 0)
            continue;

          if (FAILED(renderClient->GetBuffer(framesAvailable, &data)))
            break;

          memset(data, 0, static_cast<size_t>(framesAvailable) * frameSize);
          if (FAILED(renderClient->ReleaseBuffer(framesAvailable, 0)))
            break;
        }

        audioClient->Stop();
        SAFE_RELEASE(renderClient);
        SAFE_RELEASE(audioClient);
      });

  if (format)
    CoTaskMemFree(format);
  SAFE_RELEASE(device);
  SAFE_RELEASE(enumerator);
}

void RecordWindows::Capture(Stream *stream, EDataFlow flow, DWORD streamFlags) {
  UINT32 bufferFrameCount = 0;

  EXIT_ON_ERROR(CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                 IID_IMMDeviceEnumerator,
                                 (void **)&stream->enumerator));

  HRESULT hr = stream->enumerator->GetDefaultAudioEndpoint(flow, eConsole,
                                                           &stream->device);

  if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
    LOG_TO_FILE("error.log", "No default audio endpoint found for flow", flow);
    std::cerr << "No default audio endpoint found for flow " << flow
              << std::endl;
    return;
  }

  EXIT_ON_ERROR(hr);

  EXIT_ON_ERROR(stream->device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                                         (void **)&stream->audioClient));

  EXIT_ON_ERROR(stream->audioClient->GetMixFormat(&stream->format));

  EXIT_ON_ERROR(stream->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                streamFlags, REFTIMES_PER_SEC,
                                                0, stream->format, nullptr));

  EXIT_ON_ERROR(stream->audioClient->GetBufferSize(&stream->bufferFrameCount));

  EXIT_ON_ERROR(stream->audioClient->GetService(
      IID_IAudioCaptureClient, (void **)&stream->captureClient));

  EXIT_ON_ERROR(stream->audioClient->Start());
}

void RecordWindows::Step(Stream *stream) {
  if (!stream || !stream->captureClient)
    return;

  UINT32 packetLength = 0;
  if (FAILED(stream->captureClient->GetNextPacketSize(&packetLength)))
    return;

  while (packetLength != 0) {
    BYTE *data;
    UINT32 frames;
    DWORD flags;
    EXIT_ON_ERROR(stream->captureClient->GetBuffer(&data, &frames, &flags,
                                                   nullptr, nullptr));

    std::vector<float> mono(frames);
    float *in = reinterpret_cast<float *>(data);

    UINT32 channels = stream->format->nChannels;
    for (uint32_t i = 0; i < frames; ++i) {
      float sum = 0.0f;
      for (uint32_t c = 0; c < channels; ++c)
        sum += in[i * channels + c];
      mono[i] = sum / static_cast<float>(channels);
    }

    if (stream->onStream)
      stream->onStream(Chunk{.buffer = mono,
                             .frames = frames,
                             .sampleRate = stream->format->nSamplesPerSec,
                             .channels = 1,
                             .bits = stream->format->wBitsPerSample});

    EXIT_ON_ERROR(stream->captureClient->ReleaseBuffer(frames));
    EXIT_ON_ERROR(stream->captureClient->GetNextPacketSize(&packetLength));
  }
}

RecordWindows::~RecordWindows() { End(); }

void RecordWindows::Begin() {

  EXIT_ON_ERROR(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  m_Data.isRunning.store(true);
  StartSilentRender();

  m_Thread = std::thread([&]() {
    if (m_Data.onBegin)
      m_Data.onBegin();

    Capture(&m_Data.internalStream, eRender, AUDCLNT_STREAMFLAGS_LOOPBACK);
    Capture(&m_Data.microphoneStream, eCapture, 0);

    while (m_Data.isRunning.load()) {
      Step(&m_Data.internalStream);
      Step(&m_Data.microphoneStream);
      Sleep(10);
    }

    if (m_Data.internalStream.audioClient != NULL)
      m_Data.internalStream.audioClient->Stop();
    SAFE_RELEASE(m_Data.internalStream.audioClient);
    SAFE_RELEASE(m_Data.internalStream.captureClient);

    if (m_Data.microphoneStream.audioClient != NULL)
      m_Data.microphoneStream.audioClient->Stop();
    SAFE_RELEASE(m_Data.microphoneStream.audioClient);
    SAFE_RELEASE(m_Data.microphoneStream.captureClient);

    if (m_Data.onEnd)
      m_Data.onEnd();
  });

  SAFE_RELEASE(m_Data.internalStream.device);
  SAFE_RELEASE(m_Data.internalStream.enumerator);

  SAFE_RELEASE(m_Data.microphoneStream.device);
  SAFE_RELEASE(m_Data.microphoneStream.enumerator);
}

void RecordWindows::End() {
  m_Data.isRunning.store(false);

  if (m_Thread.joinable())
    m_Thread.join();

  if (m_SilentRenderThread.joinable())
    m_SilentRenderThread.join();

  CoUninitialize();
}

void RecordWindows::OnInternalStream(const StreamCallback &callback) {
  m_Data.internalStream.onStream = callback;
}

void RecordWindows::OnMicrophoneStream(const StreamCallback &callback) {
  m_Data.microphoneStream.onStream = callback;
}
