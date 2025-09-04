#include "Stream.h"

#include <iostream>

#include <gio/gio.h>
#include <spa/debug/types.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/result.h>

void Stream::onProcessStream(void *data) {
  std::cout << "onProcessStream()" << std::endl;

  auto *self = static_cast<Stream *>(data);

  struct pw_buffer *buf;

  if (!(buf = pw_stream_dequeue_buffer(self->m_Data.stream))) {
    std::cerr << "Out of buffers" << std::endl;
    return;
  }

  self->m_OnStream(buf->buffer);

  pw_stream_queue_buffer(self->m_Data.stream, buf);
}

void Stream::onStateChanged(void *data, pw_stream_state old,
                            pw_stream_state state, const char *error) {
  std::cout << "Stream state changed from " << pw_stream_state_as_string(old)
            << " to " << pw_stream_state_as_string(state) << std::endl;
  if (state == PW_STREAM_STATE_ERROR) {
    std::cerr << "Stream error: " << (error ? error : "unknown") << std::endl;
  }
}

void Stream::onInitializePipewire(Stream *self) {
  std::cout << "onInitializePipewire(" << self->m_Data.pwfd
            << ", target_id=" << self->m_Data.targetId << ")" << std::endl;

  self->m_Data.core = pw_context_connect_fd(self->m_Data.context,
                                            self->m_Data.pwfd, nullptr, 0);

  if (self->m_Data.core == nullptr)
    throw std::runtime_error("Failed to connect to PipeWire remote");

  self->m_Data.stream = pw_stream_new(
      self->m_Data.core, "ssrd",
      pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                        "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));

  if (!self->m_Data.stream)
    throw std::runtime_error("Failed to create PipeWire stream");

  pw_stream_events stream_events = {};
  stream_events.version = PW_VERSION_STREAM_EVENTS;
  stream_events.process = onProcessStream;
  stream_events.state_changed = onStateChanged;
  pw_stream_add_listener(self->m_Data.stream, &self->m_Data.stream_listener,
                         &stream_events, self);

  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod *params[1];

  params[0] = static_cast<spa_pod *>(spa_pod_builder_add_object(
      &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
      SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
      SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format,
      SPA_POD_CHOICE_ENUM_Id(6, SPA_VIDEO_FORMAT_RGB, SPA_VIDEO_FORMAT_RGBA,
                             SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_BGRx,
                             SPA_VIDEO_FORMAT_YUY2, SPA_VIDEO_FORMAT_I420)));

  pw_stream_connect(self->m_Data.stream, PW_DIRECTION_INPUT,
                    self->m_Data.targetId,
                    (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS),
                    params, 1);
}

static gboolean pipewire_loop_source_dispatch(GSource *source,
                                              GSourceFunc callback,
                                              gpointer user_data) {
  PipeWireSource *s = (PipeWireSource *)source;
  int result = 0;

  if (result = pw_loop_iterate(s->loop, -1) < 0) {
    if (result == -EINTR)
      return TRUE;
    g_warning("pipewire_loop_iterate failed: %s", spa_strerror(result));
  }

  return TRUE;
}

static GSourceFuncs pipewire_source_funcs = {
    .dispatch = pipewire_loop_source_dispatch,
};

void Stream::onStartSession(GObject *source_object, GAsyncResult *res,
                            gpointer data) {
  auto *self = static_cast<Stream *>(data);
  GError *error = nullptr;
  gboolean success = xdp_session_start_finish(self->m_Session, res, &error);

  if (!success) {
    std::cerr << "Failed to start session: "
              << (error ? error->message : "unknown") << std::endl;
    if (error)
      g_error_free(error);
    return;
  }

  self->m_SessionClosedHandler = g_signal_connect(
      self->m_Session, "closed", G_CALLBACK(onSessionClosed), self);

  // Retrieve streams to get the PipeWire target ID
  GVariant *streams = xdp_session_get_streams(self->m_Session);
  if (!streams || g_variant_n_children(streams) == 0) {
    std::cerr << "No streams available in session" << std::endl;
    if (streams)
      g_variant_unref(streams);
    return;
  }

  // Assume first stream (for single monitor); iterate if multiple
  GVariant *child = g_variant_get_child_value(streams, 0);
  g_variant_get(child, "(u@a{sv})", &self->m_Data.targetId, nullptr);
  g_variant_unref(child);
  g_variant_unref(streams);

  std::cout << "Got screencast node ID: " << self->m_Data.targetId << std::endl;

  self->m_Data.pwfd = xdp_session_open_pipewire_remote(self->m_Session);
  if (self->m_Data.pwfd < 0) {
    std::cerr << "Failed to open PipeWire remote" << std::endl;
    return;
  }

  std::cout << "Got PipeWire remote fd: " << self->m_Data.pwfd << std::endl;

  PipeWireSource *source = (PipeWireSource *)g_source_new(
      &pipewire_source_funcs, sizeof(PipeWireSource));

  source->loop = self->m_Data.loop;

  g_source_add_unix_fd(&source->base, pw_loop_get_fd(self->m_Data.loop),
                       (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP));

  g_source_attach(&source->base, g_main_loop_get_context(self->m_MainLoop));
  g_source_unref(&source->base);
  g_main_loop_quit(self->m_MainLoop);
}

void Stream::onRemoteDesktopReady(GObject *source_object, GAsyncResult *res,
                                  gpointer data) {
  auto *self = static_cast<Stream *>(data);
  GError *error = nullptr;
  self->m_Session = xdp_portal_create_remote_desktop_session_finish(
      self->m_Portal, res, &error);

  if (!self->m_Session) {
    std::cout << "Failed to create remote desktop session: "
              << (error ? error->message : "unknown") << std::endl;
    if (error)
      g_error_free(error);
    return;
  }

  xdp_session_start(self->m_Session, nullptr, nullptr, onStartSession, self);
}

void Stream::onSessionClosed(GObject *source_object, gpointer user_data) {
  auto *self = static_cast<Stream *>(user_data);
  std::cout << "Session closed; stopping stream" << std::endl;
  if (self->m_MainLoop)
    g_main_loop_quit(self->m_MainLoop);
}

Stream::~Stream() {
  if (m_SessionClosedHandler)
    g_signal_handler_disconnect(m_Session, m_SessionClosedHandler);

  if (m_Session) {
    xdp_session_close(m_Session);
    g_object_unref(m_Session);
  }

  if (m_Portal)
    g_object_unref(m_Portal);
  if (m_Data.stream)
    pw_stream_destroy(m_Data.stream);
  if (m_Data.core)
    pw_core_disconnect(m_Data.core);
  if (m_Data.context)
    pw_context_destroy(m_Data.context);
  if (m_Data.loop) {
    pw_loop_leave(m_Data.loop);
    pw_loop_destroy(m_Data.loop);
  }
  if (m_MainLoop)
    g_main_loop_unref(m_MainLoop);

  pw_deinit();
}

void Stream::begin(std::function<void(spa_buffer *spa_buf)> onStream) {
  m_OnStream = onStream;

  pw_init(nullptr, nullptr);

  m_Data.loop = pw_loop_new(nullptr);

  if (!m_Data.loop)
    throw std::runtime_error("Failed to create PipeWire main loop");

  m_Data.context = pw_context_new(m_Data.loop, nullptr, 0);

  if (!m_Data.context)
    throw std::runtime_error("Failed to create PipeWire context");

  m_Portal = xdp_portal_new();

  if (!m_Portal)
    throw std::runtime_error("Failed to create XdpPortal");

  m_MainLoop = g_main_loop_new(nullptr, FALSE);

  if (!m_MainLoop)
    throw std::runtime_error("Failed to create GLib main loop");

  xdp_portal_create_remote_desktop_session(
      m_Portal, (XdpDeviceType)(XDP_DEVICE_KEYBOARD | XDP_DEVICE_POINTER),
      XDP_OUTPUT_MONITOR, XDP_REMOTE_DESKTOP_FLAG_NONE,
      XDP_CURSOR_MODE_EMBEDDED, NULL, onRemoteDesktopReady, this);

  // onInitializePipewire(self);

  // pw_loop_enter(m_Data.loop);
  g_main_loop_run(m_MainLoop);

  
  onInitializePipewire(this);

  pw_loop_enter(m_Data.loop);
  g_main_loop_run(m_MainLoop);
  std::cout << m_Data.pwfd << std::endl;
}