#include "Remote.h"

#include <iostream>

#include <spa/debug/types.h>

#include "Utility.h"

Remote::Remote() {
  pw_init(nullptr, nullptr);

  // Create the pw loop and a context.
  m_Data.pw.loop = pw_loop_new(nullptr);
  if (!m_Data.pw.loop)
    throw std::runtime_error("Failed to create pipewire loop");

  m_Data.pw.context = pw_context_new(m_Data.pw.loop, nullptr, 0);
  if (!m_Data.pw.context)
    throw std::runtime_error("Failed to create pipewire context");

  // Create the xdp portal
  m_Data.g.portal = xdp_portal_new();
  if (!m_Data.g.portal)
    throw std::runtime_error("Failed to create xdp portal");

  // Create the g main context and loop
  m_Data.g.loop = g_main_loop_new(NULL, FALSE);
  if (!m_Data.g.loop)
    throw std::runtime_error("Failed to create glib loop");

  m_Data.g.context = g_main_loop_get_context(m_Data.g.loop);
  if (!m_Data.g.loop)
    throw std::runtime_error("Failed to get glib loop context");
}

Remote::~Remote() {
  if (m_Data.pw.core)
    pw_core_disconnect(m_Data.pw.core);

  pw_context_destroy(m_Data.pw.context);
  pw_loop_destroy(m_Data.pw.loop);
  g_main_loop_unref(m_Data.g.loop);
  pw_deinit();
}

void Remote::onStream(
    const std::function<void(std::vector<uint8_t> buffer)> &callback) {
  m_Data.onStream = callback;
}

void Remote::onSessionClosed(GObject *sourceObject, gpointer userData) {
  Data *data = static_cast<Data *>(userData);

  if (data && data->g.loop)
    g_main_loop_quit(data->g.loop);

  LOG("Session closed");
}

void Remote::onRemoteDesktopReady(GObject *source_object, GAsyncResult *res,
                                  gpointer userData) {
  Data *data = static_cast<Data *>(userData);

  // Create the xdp session for remote desktop
  data->g.session = xdp_portal_create_remote_desktop_session_finish(
      data->g.portal, res, nullptr);

  // Handle session closed
  data->g.sessionClosedHandle = g_signal_connect(
      data->g.session, "closed", G_CALLBACK(onSessionClosed), nullptr);

  xdp_session_start(data->g.session, nullptr, nullptr, onSessionStart,
                    userData);
}

void Remote::onSessionStart(GObject *source_object, GAsyncResult *res,
                            gpointer userData) {
  Data *data = static_cast<Data *>(userData);

  GError *error = nullptr;

  if (!xdp_session_start_finish(data->g.session, res, &error)) {
    std::cerr << (error ? error->message
                        : "Failed to start session: unknown error")
              << std::endl;
    if (error)
      g_error_free(error);

    return exit(EXIT_FAILURE);
  }

  // Get the target stream id
  {
    GVariant *streams = xdp_session_get_streams(data->g.session);
    if (!streams)
      throw std::runtime_error("Failed to get session stream");

    GVariant *child = g_variant_get_child_value(streams, 0);
    if (!streams)
      throw std::runtime_error("Failed to get session child value");

    g_variant_get(child, "(u@a{sv})", &data->target_id, nullptr);
    g_variant_unref(child);
    g_variant_unref(streams);
  }

  // Get pipewire discriptor
  data->pw_fd = xdp_session_open_pipewire_remote(data->g.session);

  LOG("pipewire discriptor", data->pw_fd);
  LOG("pipewire target id", data->target_id);

  // Setup pipewire
  {
    // Get the core
    data->pw.core =
        pw_context_connect_fd(data->pw.context, data->pw_fd, nullptr, 0);

    if (!data->pw.core)
      throw std::runtime_error("Failed to connect context to pipewire fd");

    // Get the stream
    data->pw.stream = pw_stream_new(
        data->pw.core, "ssrd",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));

    if (!data->pw.stream)
      throw std::runtime_error("Failed to create pipewire stream");

    data->pw.streamEvents.version = PW_VERSION_STREAM_EVENTS;
    data->pw.streamEvents.param_changed = onStreamParamsChange;
    data->pw.streamEvents.process = onStreamProcess;

    pw_stream_add_listener(data->pw.stream, &data->pw.stream_listener,
                           &data->pw.streamEvents, userData);

    uint8_t buffer[1024];
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

    if (pw_stream_connect(data->pw.stream, PW_DIRECTION_INPUT, data->target_id,
                          (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                            PW_STREAM_FLAG_MAP_BUFFERS),
                          params, 1) < 0)
      throw std::runtime_error("Failed to connect to pipewire stream");
  }

  int fd = pw_loop_get_fd(data->pw.loop);

  data->pw.pipewireSourceFuncs = {
      .dispatch = [](GSource *source, GSourceFunc callback,
                     gpointer user_data) -> gboolean {
        auto *pwloop =
            static_cast<pw_loop *>(((PipewireSource *)source)->data->pw.loop);

        if (pwloop)
          pw_loop_iterate(pwloop, 0);

        return G_SOURCE_CONTINUE;
      },
  };

  PipewireSource *source = (PipewireSource *)g_source_new(
      &data->pw.pipewireSourceFuncs, sizeof(PipewireSource));

  if (!source)
    throw std::runtime_error("Failed to create glib pipewire source");

  source->data = data;

  g_source_add_unix_fd(&source->base, fd,
                       (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP));
  g_source_attach(&source->base, data->g.context);
  g_source_unref(&source->base);
}

void Remote::onStreamParamsChange(void *userData, uint32_t id,
                                  const struct spa_pod *param) {
  auto *data = static_cast<Data *>(userData);

  if (param == NULL || id != SPA_PARAM_Format)
    return;

  if (spa_format_parse(param, &data->format.media_type,
                       &data->format.media_subtype) < 0)
    return;

  if (data->format.media_type != SPA_MEDIA_TYPE_video ||
      data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    return;

  if (spa_format_video_raw_parse(param, &data->format.info.raw) < 0)
    return;

  LOG("Video format:");
  LOG("  format:", spa_debug_type_find_name(spa_type_video_format,
                                            data->format.info.raw.format));

  int width = data->format.info.raw.size.width;
  int height = data->format.info.raw.size.height;
  LOG("  size:", width, height);
  LOG("  framerate:", data->format.info.raw.framerate.num,
      data->format.info.raw.framerate.denom);

  // Resize the framebuffer to fit the image size
  data->rawFrameBuffer.resize((width * height) * 3);
  data->encoder.initialize(width, height);
}

void Remote::onStreamProcess(void *userData) {
  auto *data = static_cast<Data *>(userData);

  pw_buffer *b = pw_stream_dequeue_buffer(data->pw.stream);

  if (!b)
    return;

  spa_data *d = b->buffer->datas;

  if (d[0].chunk->size == 0 || d[0].data == NULL) {
    pw_stream_queue_buffer(data->pw.stream, b);
    return;
  }

  uint8_t *frame = (uint8_t *)SPA_MEMBER(d[0].data, d[0].chunk->offset, void);
  size_t size = d[0].chunk->size;

  int width = data->format.info.raw.size.width;
  int height = data->format.info.raw.size.height;

  writeTobuffer(&data->rawFrameBuffer, frame, width, height,
                data->format.info.raw.format);

  data->encoder.encodeFrame(data->rawFrameBuffer, data->encodedFrameBuffer);

  data->onStream(data->encodedFrameBuffer);

  pw_stream_queue_buffer(data->pw.stream, b);
}

void Remote::begin() {
  xdp_portal_create_remote_desktop_session(
      m_Data.g.portal,
      (XdpDeviceType)(XDP_DEVICE_KEYBOARD | XDP_DEVICE_POINTER),
      XDP_OUTPUT_MONITOR, XDP_REMOTE_DESKTOP_FLAG_NONE,
      XDP_CURSOR_MODE_EMBEDDED, NULL, onRemoteDesktopReady, &m_Data);

  g_main_loop_run(m_Data.g.loop);
}
