#include "R2.h"

#include <cstring>
#include <iostream>

#include "Helpers.h"
#include "Keys.h"
#include "Utility.h"

R2::R2() : m_Running(true), m_Thread(&R2::Start, this) {}

R2::~R2() { Stop(); }

void R2::Stop() {
  if (!m_Running.exchange(false))
    return;

  EndSession();
  m_CV.notify_one();
  m_Thread.join();
}

void R2::EndSession() {
  if (m_UserData.xdp.session &&
      xdp_session_get_session_state(m_UserData.xdp.session) ==
          XdpSessionState::XDP_SESSION_ACTIVE) {
    for (uint32_t key : m_PressedKeyboardKeys)
      xdp_session_keyboard_key(m_UserData.xdp.session, FALSE, glfwToXdpKey(key),
                               XDP_KEY_RELEASED);

    for (uint32_t mod : m_PressedKeyboardMods)
      xdp_session_keyboard_key(m_UserData.xdp.session, FALSE, glfwToXdpKey(mod),
                               XDP_KEY_RELEASED);

    for (uint32_t button : m_PressedMouseButtons)
      xdp_session_pointer_button(m_UserData.xdp.session,
                                 glfwToXdpMouseButton(button),
                                 XDP_BUTTON_RELEASED);

    m_PressedKeyboardKeys.clear();
    m_PressedKeyboardMods.clear();
    m_PressedMouseButtons.clear();
    xdp_session_close(m_UserData.xdp.session);
  }

  m_UserData.isRunning.store(false);
}

void R2::BeginSession() {
  auto OnRemoteDesktopReady = [](GObject *source_object, GAsyncResult *res,
                                 gpointer userData) {
    UserData *data = static_cast<UserData *>(userData);

    // Create the xdp session for remote desktop
    data->xdp.session = xdp_portal_create_remote_desktop_session_finish(
        data->xdp.portal, res, nullptr);

    // Handle session closed
    data->g.sessionClosedHandle = g_signal_connect(
        data->xdp.session, "closed", G_CALLBACK(OnSessionClosed), userData);

    xdp_session_start(data->xdp.session, nullptr, nullptr, OnSessionStart,
                      userData);
  };

  xdp_portal_create_remote_desktop_session(
      m_UserData.xdp.portal,
      (XdpDeviceType)(XDP_DEVICE_KEYBOARD | XDP_DEVICE_POINTER),
      XDP_OUTPUT_MONITOR, XDP_REMOTE_DESKTOP_FLAG_NONE,
      XDP_CURSOR_MODE_EMBEDDED, NULL, OnRemoteDesktopReady, &m_UserData);

  m_UserData.isRunning.store(true);
}

void R2::OnStreamVideo(const VideoStreamCallback &callback) {
  m_UserData.onStreamVideo = callback;
}

void R2::OnStreamAudio(const AudioStreamCallback &callback) {
  m_UserData.onStreamAudio = callback;
}

void R2::OnResize(const ResizeCallback &callback) {
  m_UserData.onResize = callback;
}

void R2::OnSessionConnected(const std::function<void()> &callback) {
  m_UserData.onSessionConnected = callback;
}

void R2::OnSessionDisconnected(const std::function<void()> &callback) {
  m_UserData.onSessionDisconnected = callback;
}

bool R2::IsRemoteDesktopActive() {
  return m_UserData.isRunning.load(std::memory_order::acquire);
}

bool R2::IsSessionActive() {
  return !!m_UserData.xdp.session &&
         xdp_session_get_session_state(m_UserData.xdp.session) ==
             XdpSessionState::XDP_SESSION_ACTIVE;
}

void R2::Keyboard(int key, int action, int mods) {
  m_PressedKeyboardKeys.insert(key);
  m_PressedKeyboardMods.insert(mods);
  xdp_session_keyboard_key(m_UserData.xdp.session, FALSE, glfwToXdpKey(mods),
                           GLFWToXDPKeyState(action));
  xdp_session_keyboard_key(m_UserData.xdp.session, FALSE, glfwToXdpKey(key),
                           GLFWToXDPKeyState(action));
}

void R2::Mouse(double x, double y) {
  if (!m_UserData.pw.videoStream.stream)
    return;

  int width = m_UserData.videoFormat.info.raw.size.width;
  int height = m_UserData.videoFormat.info.raw.size.height;

  double positionX = width * x;
  double positionY = height * y;

  xdp_session_pointer_position(m_UserData.xdp.session, m_UserData.targetId,
                               positionX, positionY);
}

void R2::MouseButton(int button, int action, int mods) {
  m_PressedMouseButtons.insert(button);
  xdp_session_pointer_button(m_UserData.xdp.session,
                             glfwToXdpMouseButton(button),
                             glfwToXdpMouseButtonState(action));
}

void R2::MouseScroll(int x, int y) {
  // Swap axes, XDP interprets them inverted
  if (y != 0)
    xdp_session_pointer_axis_discrete(
        m_UserData.xdp.session, XdpDiscreteAxis::XDP_AXIS_HORIZONTAL_SCROLL,
        -y);

  if (x != 0)
    xdp_session_pointer_axis_discrete(
        m_UserData.xdp.session, XdpDiscreteAxis::XDP_AXIS_VERTICAL_SCROLL, -x);
}

void R2::Start() {

  Initialize();

  while (true) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CV.wait(lock, [&] { return !m_Running; });
  }

  Deinitialize();
}

void R2::Initialize() {
  pw_init(nullptr, nullptr);

  // Create the pw loop and a context.
  m_UserData.pw.loop = pw_loop_new(nullptr);
  if (!m_UserData.pw.loop)
    throw std::runtime_error("Failed to create pipewire loop");

  m_UserData.pw.context = pw_context_new(m_UserData.pw.loop, nullptr, 0);
  if (!m_UserData.pw.context)
    throw std::runtime_error("Failed to create pipewire context");

  // Create the xdp portal
  m_UserData.xdp.portal = xdp_portal_new();
  if (!m_UserData.xdp.portal)
    throw std::runtime_error("Failed to create xdp portal");

  // Create the g main context and loop
  m_UserData.g.loop = g_main_loop_new(NULL, FALSE);
  if (!m_UserData.g.loop)
    throw std::runtime_error("Failed to create glib loop");

  m_UserData.g.context = g_main_loop_get_context(m_UserData.g.loop);
  if (!m_UserData.g.context)
    throw std::runtime_error("Failed to get glib loop context");

  g_main_loop_run(m_UserData.g.loop);
}

void R2::Deinitialize() {
  if (m_UserData.g.loop && g_main_loop_is_running(m_UserData.g.loop))
    g_main_loop_quit(m_UserData.g.loop);

  if (m_UserData.pw.videoStream.stream)
    pw_stream_destroy(m_UserData.pw.videoStream.stream);

  if (m_UserData.pw.audioStream.stream)
    pw_stream_destroy(m_UserData.pw.audioStream.stream);

  if (m_UserData.pw.core)
    pw_core_disconnect(m_UserData.pw.core);

  if (m_UserData.pw.context)
    pw_context_destroy(m_UserData.pw.context);

  if (m_UserData.pw.loop)
    pw_loop_destroy(m_UserData.pw.loop);

  if (m_UserData.xdp.session) {
    if (m_UserData.g.sessionClosedHandle)
      g_signal_handler_disconnect(m_UserData.xdp.session,
                                  m_UserData.g.sessionClosedHandle);

    g_object_unref(m_UserData.xdp.session);
  }

  if (m_UserData.xdp.portal)
    g_object_unref(m_UserData.xdp.portal);

  if (m_UserData.g.loop)
    g_main_loop_unref(m_UserData.g.loop);

  m_UserData.pw.videoStream.stream = nullptr;
  m_UserData.pw.audioStream.stream = nullptr;
  m_UserData.pw.core = nullptr;
  m_UserData.pw.context = nullptr;
  m_UserData.pw.loop = nullptr;
  m_UserData.g.loop = nullptr;
  m_UserData.g.context = nullptr;
  m_UserData.g.sessionClosedHandle = 0;
  m_UserData.xdp.session = nullptr;
  m_UserData.xdp.portal = nullptr;

  pw_deinit();
}

void R2::OnSessionClosed(GObject *sourceObject, gpointer userData) {
  LOG("Session closed");

  UserData *data = static_cast<UserData *>(userData);

  if (data->onSessionDisconnected)
    data->onSessionDisconnected();

  data->isRunning.store(false);
}

void R2::OnSessionStart(GObject *source_object, GAsyncResult *res,
                        gpointer userData) {
  UserData *data = static_cast<UserData *>(userData);

  GError *error = nullptr;

  if (!xdp_session_start_finish(data->xdp.session, res, &error)) {
    std::cerr << (error ? error->message
                        : "Failed to start session: unknown error")
              << std::endl;
    if (error)
      g_error_free(error);

    return;
  }

  // Get the target stream id
  {
    GVariant *streams = xdp_session_get_streams(data->xdp.session);
    if (!streams)
      throw std::runtime_error("Failed to get session stream");

    GVariant *child = g_variant_get_child_value(streams, 0);
    if (!streams)
      throw std::runtime_error("Failed to get session child value");

    g_variant_get(child, "(u@a{sv})", &data->targetId, nullptr);
    g_variant_unref(child);
    g_variant_unref(streams);
  }

  // Get pipewire discriptor
  data->pwFd = xdp_session_open_pipewire_remote(data->xdp.session);

  // Get the core
  data->pw.core =
      pw_context_connect_fd(data->pw.context, data->pwFd, nullptr, 0);

  if (!data->pw.core)
    throw std::runtime_error("Failed to connect context to pipewire fd");

  // Setup video pipewire
  {
    pw_properties *properties =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Screen", NULL);

    data->pw.videoStream.streamEvents.version = PW_VERSION_STREAM_EVENTS;
    data->pw.videoStream.streamEvents.param_changed = OnVideoStreamParamsChange;
    data->pw.videoStream.streamEvents.process = OnVideoStreamProcess;

    data->pw.videoStream.stream =
        pw_stream_new(data->pw.core, "ssrd-video", properties);

    if (!data->pw.videoStream.stream)
      throw std::runtime_error("Failed to create pipewire video stream");

    pw_stream_add_listener(data->pw.videoStream.stream,
                           &data->pw.videoStream.streamListener,
                           &data->pw.videoStream.streamEvents, userData);

    uint8_t buffer[4096];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    params[0] = static_cast<spa_pod *>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
        SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
        SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format,
        SPA_POD_CHOICE_ENUM_Id(8, SPA_VIDEO_FORMAT_RGB, SPA_VIDEO_FORMAT_RGBA,
                               SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_BGR,
                               SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
                               SPA_VIDEO_FORMAT_NV12, SPA_VIDEO_FORMAT_I420)));

    if (pw_stream_connect(data->pw.videoStream.stream, PW_DIRECTION_INPUT,
                          data->targetId,
                          (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                            PW_STREAM_FLAG_MAP_BUFFERS |
                                            PW_STREAM_FLAG_RT_PROCESS),
                          params, 1) < 0)
      throw std::runtime_error("Failed to connect to pipewire video stream");
  }

  // Setup audio pipewire
  {
    pw_properties *properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music", PW_KEY_STREAM_CAPTURE_SINK, "true", NULL);

    data->pw.audioStream.streamEvents.version = PW_VERSION_STREAM_EVENTS;
    data->pw.audioStream.streamEvents.param_changed = OnAudioStreamParamsChange;
    data->pw.audioStream.streamEvents.process = OnAudioStreamProcess;

    data->pw.audioStream.stream =
        pw_stream_new_simple(data->pw.loop, "ssrd-audio", properties,
                             &data->pw.audioStream.streamEvents, userData);

    if (!data->pw.audioStream.stream)
      throw std::runtime_error("Failed to create pipewire audio stream");

    pw_stream_add_listener(data->pw.audioStream.stream,
                           &data->pw.audioStream.streamListener,
                           &data->pw.audioStream.streamEvents, userData);

    uint8_t buffer[4096];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    struct spa_audio_info_raw info = {.format = SPA_AUDIO_FORMAT_F32,
                                      .channels = 2};

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    if (pw_stream_connect(data->pw.audioStream.stream, PW_DIRECTION_INPUT,
                          data->targetId,
                          (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                            PW_STREAM_FLAG_MAP_BUFFERS |
                                            PW_STREAM_FLAG_RT_PROCESS),
                          params, 1) < 0)
      throw std::runtime_error("Failed to connect to pipewire audio stream");
  }

  data->g.sourceFunctions = {
      .dispatch = [](GSource *source, GSourceFunc callback,
                     gpointer user_data) -> gboolean {
        auto *pwloop = static_cast<pw_loop *>(
            ((PipewireGSource *)source)->userData->pw.loop);

        if (pwloop)
          pw_loop_iterate(pwloop, 0);

        return G_SOURCE_CONTINUE;
      },
  };

  PipewireGSource *source = (PipewireGSource *)g_source_new(
      &data->g.sourceFunctions, sizeof(PipewireGSource));

  if (!source)
    throw std::runtime_error("Failed to create glib pipewire source");

  source->userData = data;

  int fd = pw_loop_get_fd(data->pw.loop);

  g_source_add_unix_fd(&source->base, fd,
                       (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP));
  g_source_attach(&source->base, data->g.context);
  g_source_unref(&source->base);

  if (data->onSessionConnected)
    data->onSessionConnected();
}

void R2::OnVideoStreamParamsChange(void *userData, uint32_t id,
                                   const struct spa_pod *param) {
  auto *data = static_cast<UserData *>(userData);

  if (param == NULL || id != SPA_PARAM_Format)
    return;

  if (spa_format_parse(param, &data->videoFormat.media_type,
                       &data->videoFormat.media_subtype) < 0)
    return;

  if (data->videoFormat.media_type != SPA_MEDIA_TYPE_video ||
      data->videoFormat.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    return;

  if (spa_format_video_raw_parse(param, &data->videoFormat.info.raw) < 0)
    return;

  int width = data->videoFormat.info.raw.size.width;
  int height = data->videoFormat.info.raw.size.height;

  LOG("Video format:");
  LOG("  format:", spa_debug_type_find_name(spa_type_video_format,
                                            data->videoFormat.info.raw.format));
  LOG("  size:", width, height);
  LOG("  framerate:", data->videoFormat.info.raw.framerate.num,
      data->videoFormat.info.raw.framerate.denom);

  // Resize the framebuffer to fit the image size
  data->framebuffer.resize((width * height) * 3);
  if (data->onResize)
    data->onResize(width, height);
}

void R2::OnAudioStreamParamsChange(void *userData, uint32_t id,
                                   const struct spa_pod *param) {
  auto *data = static_cast<UserData *>(userData);

  if (param == NULL || id != SPA_PARAM_Format)
    return;

  if (spa_format_parse(param, &data->audioFormat.media_type,
                       &data->audioFormat.media_subtype) < 0)
    return;

  if (data->audioFormat.media_type != SPA_MEDIA_TYPE_audio ||
      data->audioFormat.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    return;

  if (spa_format_audio_raw_parse(param, &data->audioFormat.info.raw) < 0)
    return;

  LOG("Audio format:");
  LOG("  Capture format:", data->audioFormat.info.raw.rate, "hz");
  LOG("  channels:", data->audioFormat.info.raw.channels);
  LOG("  bit:", spa_audio_format_depth(data->audioFormat.info.raw.format));
}

void R2::OnVideoStreamProcess(void *userData) {
  auto *data = static_cast<UserData *>(userData);

  if (!data->onStreamVideo)
    return;

  pw_buffer *b = pw_stream_dequeue_buffer(data->pw.videoStream.stream);

  if (!b)
    return;

  spa_data *d = b->buffer->datas;

  if (d[0].chunk->size == 0 || d[0].data == NULL) {
    pw_stream_queue_buffer(data->pw.videoStream.stream, b);
    return;
  }

  uint8_t *frame = (uint8_t *)SPA_MEMBER(d[0].data, d[0].chunk->offset, void);
  size_t size = d[0].chunk->size;

  int width = data->videoFormat.info.raw.size.width;
  int height = data->videoFormat.info.raw.size.height;

  writeTobuffer(&data->framebuffer, frame, width, height,
                data->videoFormat.info.raw.format);

  uint64_t time = pw_stream_get_nsec(data->pw.videoStream.stream);

  data->onStreamVideo(data->framebuffer, time);

  pw_stream_queue_buffer(data->pw.videoStream.stream, b);
}

void R2::OnAudioStreamProcess(void *userData) {
  auto *data = static_cast<UserData *>(userData);

  pw_buffer *b = pw_stream_dequeue_buffer(data->pw.audioStream.stream);
  spa_buffer *buffer = b == nullptr ? nullptr : b->buffer;

  if (buffer == nullptr) {
    pw_log_warn("out of buffers: %m");
    return;
  }

  float *samples = nullptr;
  if ((samples = (float *)buffer->datas[0].data) == nullptr)
    return;

  uint32_t channels = data->audioFormat.info.raw.channels;
  uint32_t frames = buffer->datas[0].chunk->size / (sizeof(float) * channels);
  float *in = reinterpret_cast<float *>(buffer->datas[0].data);
  size_t sampleCount = frames * channels;

  std::vector<float> raw(sampleCount);
  std::memcpy(raw.data(), samples, sampleCount * sizeof(float));

  if (data->onStreamAudio)
    data->onStreamAudio(
        Chunk{
            .buffer = raw,
            .frames = frames,
            .sampleRate = data->audioFormat.info.raw.rate,
            .channels = channels,
            .bits = spa_audio_format_depth(data->audioFormat.info.raw.format),
        },
        b->time);

  pw_stream_queue_buffer(data->pw.audioStream.stream, b);
}
