#include "Remote.h"

#include <iostream>

#include <linux/input-event-codes.h>
#include <spa/debug/types.h>

#include "Keys.h"
#include "Utility.h"

static double s_PreviousMouseX = 0.0;
static double s_PreviousMouseY = 0.0;

static XdpKeyState GLFWToXDPKeyState(int action) {
  switch (action) {
  case GLFW_PRESS:
    return XDP_KEY_PRESSED;
  case GLFW_RELEASE:
    return XDP_KEY_RELEASED;
  default:
    return XDP_KEY_RELEASED;
  }
}

static XdpButtonState glfwToXdpMouseButtonState(int action) {
  switch (action) {
  case GLFW_PRESS:
    return XDP_BUTTON_PRESSED;
  case GLFW_RELEASE:
    return XDP_BUTTON_RELEASED;
  default:
    return XDP_BUTTON_RELEASED;
  }
}

#include <GLFW/glfw3.h>
#include <linux/input-event-codes.h>

static int glfwToXdpKey(int key) {
  switch (key) {
  // Letters
  case GLFW_KEY_A:
    return KEY_A;
  case GLFW_KEY_B:
    return KEY_B;
  case GLFW_KEY_C:
    return KEY_C;
  case GLFW_KEY_D:
    return KEY_D;
  case GLFW_KEY_E:
    return KEY_E;
  case GLFW_KEY_F:
    return KEY_F;
  case GLFW_KEY_G:
    return KEY_G;
  case GLFW_KEY_H:
    return KEY_H;
  case GLFW_KEY_I:
    return KEY_I;
  case GLFW_KEY_J:
    return KEY_J;
  case GLFW_KEY_K:
    return KEY_K;
  case GLFW_KEY_L:
    return KEY_L;
  case GLFW_KEY_M:
    return KEY_M;
  case GLFW_KEY_N:
    return KEY_N;
  case GLFW_KEY_O:
    return KEY_O;
  case GLFW_KEY_P:
    return KEY_P;
  case GLFW_KEY_Q:
    return KEY_Q;
  case GLFW_KEY_R:
    return KEY_R;
  case GLFW_KEY_S:
    return KEY_S;
  case GLFW_KEY_T:
    return KEY_T;
  case GLFW_KEY_U:
    return KEY_U;
  case GLFW_KEY_V:
    return KEY_V;
  case GLFW_KEY_W:
    return KEY_W;
  case GLFW_KEY_X:
    return KEY_X;
  case GLFW_KEY_Y:
    return KEY_Y;
  case GLFW_KEY_Z:
    return KEY_Z;

  // Numbers
  case GLFW_KEY_0:
    return KEY_0;
  case GLFW_KEY_1:
    return KEY_1;
  case GLFW_KEY_2:
    return KEY_2;
  case GLFW_KEY_3:
    return KEY_3;
  case GLFW_KEY_4:
    return KEY_4;
  case GLFW_KEY_5:
    return KEY_5;
  case GLFW_KEY_6:
    return KEY_6;
  case GLFW_KEY_7:
    return KEY_7;
  case GLFW_KEY_8:
    return KEY_8;
  case GLFW_KEY_9:
    return KEY_9;

  // Punctuation
  case GLFW_KEY_SPACE:
    return KEY_SPACE;
  case GLFW_KEY_APOSTROPHE:
    return KEY_APOSTROPHE;
  case GLFW_KEY_COMMA:
    return KEY_COMMA;
  case GLFW_KEY_MINUS:
    return KEY_MINUS;
  case GLFW_KEY_PERIOD:
    return KEY_DOT;
  case GLFW_KEY_SLASH:
    return KEY_SLASH;
  case GLFW_KEY_SEMICOLON:
    return KEY_SEMICOLON;
  case GLFW_KEY_EQUAL:
    return KEY_EQUAL;
  case GLFW_KEY_LEFT_BRACKET:
    return KEY_LEFTBRACE;
  case GLFW_KEY_BACKSLASH:
    return KEY_BACKSLASH;
  case GLFW_KEY_RIGHT_BRACKET:
    return KEY_RIGHTBRACE;
  case GLFW_KEY_GRAVE_ACCENT:
    return KEY_GRAVE;

  // Modifiers
  case GLFW_KEY_LEFT_SHIFT:
    return KEY_LEFTSHIFT;
  case GLFW_KEY_RIGHT_SHIFT:
    return KEY_RIGHTSHIFT;
  case GLFW_KEY_LEFT_CONTROL:
    return KEY_LEFTCTRL;
  case GLFW_KEY_RIGHT_CONTROL:
    return KEY_RIGHTCTRL;
  case GLFW_KEY_LEFT_ALT:
    return KEY_LEFTALT;
  case GLFW_KEY_RIGHT_ALT:
    return KEY_RIGHTALT;
  case GLFW_KEY_LEFT_SUPER:
    return KEY_LEFTMETA;
  case GLFW_KEY_RIGHT_SUPER:
    return KEY_RIGHTMETA;

  // Arrows
  case GLFW_KEY_UP:
    return KEY_UP;
  case GLFW_KEY_DOWN:
    return KEY_DOWN;
  case GLFW_KEY_LEFT:
    return KEY_LEFT;
  case GLFW_KEY_RIGHT:
    return KEY_RIGHT;

  // Function keys
  case GLFW_KEY_F1:
    return KEY_F1;
  case GLFW_KEY_F2:
    return KEY_F2;
  case GLFW_KEY_F3:
    return KEY_F3;
  case GLFW_KEY_F4:
    return KEY_F4;
  case GLFW_KEY_F5:
    return KEY_F5;
  case GLFW_KEY_F6:
    return KEY_F6;
  case GLFW_KEY_F7:
    return KEY_F7;
  case GLFW_KEY_F8:
    return KEY_F8;
  case GLFW_KEY_F9:
    return KEY_F9;
  case GLFW_KEY_F10:
    return KEY_F10;
  case GLFW_KEY_F11:
    return KEY_F11;
  case GLFW_KEY_F12:
    return KEY_F12;

  // Enter, Backspace, Tab, Escape
  case GLFW_KEY_ENTER:
    return KEY_ENTER;
  case GLFW_KEY_BACKSPACE:
    return KEY_BACKSPACE;
  case GLFW_KEY_TAB:
    return KEY_TAB;
  case GLFW_KEY_ESCAPE:
    return KEY_ESC;

  default:
    return -1; // unknown key
  }
}

static int glfwToXdpMouseButton(int glfwButton) {
  switch (glfwButton) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return BTN_LEFT;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    return BTN_MIDDLE;
  case GLFW_MOUSE_BUTTON_RIGHT:
    return BTN_RIGHT;
  default:
    return 0; // Unknown
  }
}

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
  if (m_Data.g.loop && g_main_loop_is_running(m_Data.g.loop)) {
    g_main_loop_quit(m_Data.g.loop);
    m_Data.g.loop = nullptr;
  }

  if (m_Data.pw.stream) {
    pw_stream_destroy(m_Data.pw.stream);
    m_Data.pw.stream = nullptr;
  }

  if (m_Data.pw.core) {
    pw_core_disconnect(m_Data.pw.core);
    m_Data.pw.core = nullptr;
  }

  if (m_Data.pw.context) {
    pw_context_destroy(m_Data.pw.context);
    m_Data.pw.context = nullptr;
  }

  if (m_Data.pw.loop) {
    pw_loop_destroy(m_Data.pw.loop);
    m_Data.pw.loop = nullptr;
  }

  if (m_Data.g.session) {
    if (m_Data.g.sessionClosedHandle)
      g_signal_handler_disconnect(m_Data.g.session,
                                  m_Data.g.sessionClosedHandle);
    g_object_unref(m_Data.g.session);
    m_Data.g.session = nullptr;
  }

  if (m_Data.g.portal) {
    g_object_unref(m_Data.g.portal);
    m_Data.g.portal = nullptr;
  }

  if (m_Data.g.loop) {
    g_main_loop_unref(m_Data.g.loop);
    m_Data.g.loop = nullptr;
  }

  pw_deinit();
}

void Remote::onStream(
    const std::function<void(std::vector<uint8_t> buffer)> &callback) {
  m_Data.onStream = callback;
}

void Remote::onResize(
    const std::function<void(int width, int height)> &callback) {
  m_Data.onResize = callback;
}

void Remote::onSessionClosed(GObject *sourceObject, gpointer userData) {
  Data *data = static_cast<Data *>(userData);

  if (data && data->g.loop && g_main_loop_is_running(data->g.loop)) {
    g_main_loop_quit(data->g.loop);
    data->g.loop = nullptr;
  }

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

    if (data->g.loop && g_main_loop_is_running(data->g.loop)) {
      g_main_loop_quit(data->g.loop);
      data->g.loop = nullptr;
    }

    return;
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
  data->framebuffer.resize((width * height) * 3);
  if (data->onResize)
    data->onResize(width, height);
}

void Remote::onStreamProcess(void *userData) {
  auto *data = static_cast<Data *>(userData);

  if (!data->onStream)
    return;

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

  writeTobuffer(&data->framebuffer, frame, width, height,
                data->format.info.raw.format);

  data->onStream(data->framebuffer);

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

void Remote::end() {
  if (m_Data.g.loop && g_main_loop_is_running(m_Data.g.loop))
    g_main_loop_quit(m_Data.g.loop);
}

void Remote::keyboard(int key, int action, int mods) {
  // Press modifiers
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    if (mods & GLFW_MOD_CONTROL)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTCTRL, XDP_KEY_PRESSED);
    if (mods & GLFW_MOD_SHIFT)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTSHIFT, XDP_KEY_PRESSED);
    if (mods & GLFW_MOD_ALT)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTALT, XDP_KEY_PRESSED);
    if (mods & GLFW_MOD_SUPER)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTMETA, XDP_KEY_PRESSED);
  }

  xdp_session_keyboard_key(m_Data.g.session, TRUE, glfwToXdpKey(key),
                           GLFWToXDPKeyState(action));

  // Release modifiers
  if (action == GLFW_RELEASE) {
    if (mods & GLFW_MOD_CONTROL)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTCTRL, XDP_KEY_RELEASED);
    if (mods & GLFW_MOD_SHIFT)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTSHIFT, XDP_KEY_RELEASED);
    if (mods & GLFW_MOD_ALT)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTALT, XDP_KEY_RELEASED);
    if (mods & GLFW_MOD_SUPER)
      xdp_session_keyboard_key(m_Data.g.session, TRUE, KEY_LEFTMETA, XDP_KEY_RELEASED);
  }
}

void Remote::mouse(double x, double y) {
  if (!m_Data.pw.stream)
    return;

  int width = m_Data.format.info.raw.size.width;
  int height = m_Data.format.info.raw.size.height;

  double positionX = width * x;
  double positionY = height * y;

  xdp_session_pointer_position(m_Data.g.session, m_Data.target_id, positionX,
                               positionY);
}

void Remote::mouseButton(int button, int action, int mods) {
  xdp_session_pointer_button(m_Data.g.session, glfwToXdpMouseButton(button),
                             glfwToXdpMouseButtonState(action));
}

void Remote::mouseScroll(int x, int y) {
  // Swap axes, XDP interprets them inverted
  if (y != 0)
    xdp_session_pointer_axis_discrete(
        m_Data.g.session, XdpDiscreteAxis::XDP_AXIS_HORIZONTAL_SCROLL, -y);

  if (x != 0)
    xdp_session_pointer_axis_discrete(
        m_Data.g.session, XdpDiscreteAxis::XDP_AXIS_VERTICAL_SCROLL, -x);
}
