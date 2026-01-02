#include "Record.h"

#include <errno.h>
#include <iostream>
#include <math.h>
#include <signal.h>
#include <stdio.h>

#include <linux/input-event-codes.h>
#include <spa/debug/types.h>

#include "RecordLinux.h"

static inline uint32_t spa_audio_format_depth(enum spa_audio_format fmt) {
  switch (fmt) {
  case SPA_AUDIO_FORMAT_S8:
  case SPA_AUDIO_FORMAT_U8:
    return 8;
  case SPA_AUDIO_FORMAT_S16:
  case SPA_AUDIO_FORMAT_U16:
    return 16;
  case SPA_AUDIO_FORMAT_S24:
    return 24;
  case SPA_AUDIO_FORMAT_S32:
  case SPA_AUDIO_FORMAT_U32:
    return 32;
  case SPA_AUDIO_FORMAT_F32:
    return 32;
  case SPA_AUDIO_FORMAT_F64:
    return 64;
  default:
    return 0; // unknown
  }
}

void RecordLinux::OnStreamParamsChange(void *userData, uint32_t id,
                                       const struct spa_pod *param) {
  auto *data = static_cast<Stream *>(userData);

  if (param == NULL || id != SPA_PARAM_Format)
    return;

  if (spa_format_parse(param, &data->format.media_type,
                       &data->format.media_subtype) < 0)
    return;

  if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
      data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    return;

  spa_format_audio_raw_parse(param, &data->format.info.raw);

  printf("Capture format: %d Hz, %d channels, %d bit\n",
         data->format.info.raw.rate, data->format.info.raw.channels,
         spa_audio_format_depth(data->format.info.raw.format));
}

void RecordLinux::OnStreamProcess(void *userData) {
  auto *data = static_cast<Stream *>(userData);

  pw_buffer *b = pw_stream_dequeue_buffer(data->stream);
  spa_buffer *buffer = b == nullptr ? nullptr : b->buffer;

  if (buffer == nullptr) {
    pw_log_warn("out of buffers: %m");
    return;
  }

  float *samples = nullptr;
  if ((samples = (float *)buffer->datas[0].data) == nullptr)
    return;

  uint32_t channels = data->format.info.raw.channels;
  uint32_t frames = buffer->datas[0].chunk->size / sizeof(float) / channels;
  float *in = reinterpret_cast<float *>(buffer->datas[0].data);

  std::vector<float> mono(frames);

  for (uint32_t i = 0; i < frames; ++i) {
    float sum = 0.0f;
    for (uint32_t c = 0; c < channels; ++c)
      sum += in[i * channels + c];
    mono[i] = sum / static_cast<float>(channels);
  }

  if (data->onStream)
    data->onStream(Chunk{
        .buffer = mono,
        .frames = frames,
        .sampleRate = data->format.info.raw.rate,
        .channels = 1,
        .bits = spa_audio_format_depth(data->format.info.raw.format),
    });

  pw_stream_queue_buffer(data->stream, b);
}

void RecordLinux::OnQuit(void *userData, int signalNumber) {
  auto *data = static_cast<Data *>(userData);
  pw_main_loop_quit(data->loop);
}

void RecordLinux::Initialize() {
  pw_init(nullptr, nullptr);

  m_Data.loop = pw_main_loop_new(nullptr);

  if (!m_Data.loop)
    throw std::runtime_error("Failed to create pipewire loop");

  pw_loop_add_signal(pw_main_loop_get_loop(m_Data.loop), SIGINT, OnQuit,
                     &m_Data);
  pw_loop_add_signal(pw_main_loop_get_loop(m_Data.loop), SIGTERM, OnQuit,
                     &m_Data);

  //  Internal stream
  {
    pw_properties *properties =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Communication", NULL);

    pw_properties_set(properties, PW_KEY_STREAM_CAPTURE_SINK, "true");

    m_Data.internalStream.id = 0;

    m_Data.internalStream.events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .param_changed = OnStreamParamsChange,
        .process = OnStreamProcess,
    };

    m_Data.internalStream.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(m_Data.loop), "audio-capture", properties,
        &m_Data.internalStream.events, &m_Data.internalStream);
  }

  //  Microphone stream
  {
    pw_properties *properties =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Communication", NULL);

    m_Data.microphoneStream.id = 1;

    m_Data.microphoneStream.events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .param_changed = OnStreamParamsChange,
        .process = OnStreamProcess,
    };

    m_Data.microphoneStream.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(m_Data.loop), "audio-capture", properties,
        &m_Data.microphoneStream.events, &m_Data.microphoneStream);
  }
}

RecordLinux::~RecordLinux() { End(); }

void RecordLinux::Begin() {

  Initialize();

  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod *params[2];

  struct spa_audio_info_raw info =
      SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32);

  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

  if (m_Data.internalStream.stream)
    pw_stream_connect(m_Data.internalStream.stream, PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                        PW_STREAM_FLAG_MAP_BUFFERS |
                                        PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);

  if (m_Data.microphoneStream.stream)
    pw_stream_connect(m_Data.microphoneStream.stream, PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                        PW_STREAM_FLAG_MAP_BUFFERS |
                                        PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);

  m_Data.loopEvents = {
      .version = PW_VERSION_MAIN_LOOP_EVENTS,
      .destroy =
          [](void *userData) {
            auto *data = static_cast<Data *>(userData);
            data->isRunning = false;
            if (data->onEnd)
              data->onEnd();
          },
  };

  pw_main_loop_add_listener(m_Data.loop, &m_Data.eventListener,
                            &m_Data.loopEvents, &m_Data);

  m_Thread = std::thread([&]() {
    m_Data.isRunning = true;

    pw_main_loop_run(m_Data.loop);

    if (m_Data.internalStream.stream)
      pw_stream_destroy(m_Data.internalStream.stream);

    if (m_Data.microphoneStream.stream)
      pw_stream_destroy(m_Data.microphoneStream.stream);

    pw_main_loop_destroy(m_Data.loop);

    pw_deinit();
  });
}

void RecordLinux::End() {
  if (m_Data.loop && m_Data.isRunning)
    pw_main_loop_quit(m_Data.loop);
  if (m_Thread.joinable())
    m_Thread.join();
}

void RecordLinux::OnInternalStream(const StreamCallback &callback) {
  m_Data.internalStream.onStream = callback;
}

void RecordLinux::OnMicrophoneStream(const StreamCallback &callback) {
  m_Data.microphoneStream.onStream = callback;
}