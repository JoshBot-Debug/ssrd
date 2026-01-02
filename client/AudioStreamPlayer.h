#include <portaudio.h>
#include <vector>
#include <cstdio>

class AudioStreamPlayer {
private:
  PaStream *m_Stream = nullptr;
  int m_Channels;
  int m_SampleRate;

public:
  AudioStreamPlayer(int sampleRate, int channels)
      : m_Channels(channels), m_SampleRate(sampleRate) {
    Pa_Initialize();

    PaStreamParameters outputParams;
    outputParams.device = Pa_GetDefaultOutputDevice();
    outputParams.channelCount = channels;
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency =
        Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    PaError err =
        Pa_OpenStream(&m_Stream, nullptr, &outputParams, sampleRate,
                      paFramesPerBufferUnspecified, paNoFlag, nullptr, nullptr);

    if (err != paNoError) {
      fprintf(stderr, "Pa_OpenStream failed: %s\n", Pa_GetErrorText(err));
      return;
    }

    Pa_StartStream(m_Stream);
  };

  ~AudioStreamPlayer() {
    Pa_StopStream(m_Stream);
    Pa_CloseStream(m_Stream);
    Pa_Terminate();
  };

  void WriteStream(const std::vector<float> buffer) {
    Pa_WriteStream(m_Stream, buffer.data(), buffer.size() / m_Channels);
  }
};