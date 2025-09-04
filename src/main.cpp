#include <iostream>

#include <glib-unix.h>
#include <glib.h>

#include <gio/gio.h>

#include <spa/debug/types.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/result.h>

#include <libportal/portal.h>
#include <pipewire/pipewire.h>

struct PwData {
  pw_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_stream *stream = nullptr;
  spa_hook stream_listener;
  pw_stream_events streamEvents{};
  GSourceFuncs pipewireSourceFuncs{};
};

struct GlibData {
  XdpSession *session = nullptr;
  XdpPortal *portal = nullptr;
  GMainLoop *loop = nullptr;
  GMainContext *context = nullptr;
  gulong sessionClosedHandle = 0;
};

struct Data {
  GlibData g{};
  PwData pw{};

  int pw_fd = 0;
  guint32 target_id = 0;

  spa_rectangle dimensions = SPA_RECTANGLE(1920, 1080);
};

struct PipeWireSource {
  GSource base;
  Data *data;
};

static void onSessionClosed(GObject *sourceObject, gpointer userData) {
  Data *data = static_cast<Data *>(userData);
  if (data->g.loop)
    g_main_loop_quit(data->g.loop);

  std::cout << "Session closed" << std::endl;
}

int main(int argc, char *argv[]) {

  // Initialize pw
  pw_init(&argc, &argv);

  Data data{};

  // Create the pw loop and a context.
  data.pw.loop = pw_loop_new(nullptr);
  data.pw.context = pw_context_new(data.pw.loop, nullptr, 0);

  // Create the xdp portal
  data.g.portal = xdp_portal_new();

  // Create the g main context and loop
  data.g.loop = g_main_loop_new(NULL, FALSE);
  data.g.context = g_main_loop_get_context(data.g.loop);

  // On remote desktop ready handle
  auto onRemoteDesktopReady = [](GObject *source_object, GAsyncResult *res,
                                 gpointer userData) {
    std::cout << "onRemoteDesktopReady()" << std::endl;

    Data *data = static_cast<Data *>(userData);

    // Create the xdp session for remote desktop
    data->g.session = xdp_portal_create_remote_desktop_session_finish(
        data->g.portal, res, nullptr);

    // Handle session closed
    {
      data->g.sessionClosedHandle = g_signal_connect(
          data->g.session, "closed", G_CALLBACK(onSessionClosed), nullptr);
    }

    // On session start handle
    auto onStartSession = [](GObject *source_object, GAsyncResult *res,
                             gpointer userData) {
      std::cout << "onStartSession()" << std::endl;

      Data *data = static_cast<Data *>(userData);

      if (!xdp_session_start_finish(data->g.session, res, nullptr))
        throw std::runtime_error("Failed to start the session");

      // Get the target stream id
      {
        GVariant *streams = xdp_session_get_streams(data->g.session);
        GVariant *child = g_variant_get_child_value(streams, 0);

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

        // Get the stream
        data->pw.stream = pw_stream_new(
            data->pw.core, "ssrd",
            pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                              "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));

        data->pw.streamEvents.version = PW_VERSION_STREAM_EVENTS;

        data->pw.streamEvents.process = [](void *userData) {
          std::cout << "onProcessStream()" << std::endl;

          auto *data = static_cast<Data *>(userData);

          pw_buffer *b = pw_stream_dequeue_buffer(data->pw.stream);
          if (!b)
            return;

          spa_data *d = b->buffer->datas;

          if (d[0].chunk->size > 0) {
            uint8_t *frame =
                (uint8_t *)SPA_MEMBER(d[0].data, d[0].chunk->offset, void);
            size_t size = d[0].chunk->size;

            std::cout << "Got frame, size = " << size << " bytes" << std::endl;
            // TODO: use frame data (e.g., copy, encode, write to file)
          }

          pw_stream_queue_buffer(data->pw.stream, b);
        };

        pw_stream_add_listener(data->pw.stream, &data->pw.stream_listener,
                               &data->pw.streamEvents, userData);

        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const struct spa_pod *params[1];

        params[0] = static_cast<spa_pod *>(spa_pod_builder_add_object(
            &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(6, SPA_VIDEO_FORMAT_RGB,
                                   SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_RGBx,
                                   SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_YUY2,
                                   SPA_VIDEO_FORMAT_I420),
            SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&data->dimensions)));

        pw_stream_connect(data->pw.stream, PW_DIRECTION_INPUT, data->target_id,
                          (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                            PW_STREAM_FLAG_MAP_BUFFERS),
                          params, 1);
      }

      int fd = pw_loop_get_fd(data->pw.loop);

      data->pw.pipewireSourceFuncs = {
          .dispatch = [](GSource *source, GSourceFunc callback,
                         gpointer user_data) -> gboolean {
            auto *pwloop = static_cast<pw_loop *>(
                ((PipeWireSource *)source)->data->pw.loop);

            if (pwloop)
              pw_loop_iterate(pwloop, 0);

            return G_SOURCE_CONTINUE;
          },
      };

      PipeWireSource *source = (PipeWireSource *)g_source_new(
          &data->pw.pipewireSourceFuncs, sizeof(PipeWireSource));

      source->data = data;

      g_source_add_unix_fd(&source->base, fd,
                           (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP));
      g_source_attach(&source->base, NULL);
      g_source_unref(&source->base);
    };

    xdp_session_start(data->g.session, nullptr, nullptr, onStartSession,
                      userData);
  };

  xdp_portal_create_remote_desktop_session(
      data.g.portal, (XdpDeviceType)(XDP_DEVICE_KEYBOARD | XDP_DEVICE_POINTER),
      XDP_OUTPUT_MONITOR, XDP_REMOTE_DESKTOP_FLAG_NONE,
      XDP_CURSOR_MODE_EMBEDDED, NULL, onRemoteDesktopReady, &data);

  g_main_loop_run(data.g.loop);

  pw_core_disconnect(data.pw.core);
  pw_context_destroy(data.pw.context);
  pw_loop_destroy(data.pw.loop);

  g_main_loop_unref(data.g.loop);
}