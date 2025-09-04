#pragma once

#include <vector>

#include <libportal/portal.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include "Encode.h"

class Remote {
private:
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

    spa_video_info format{};
    spa_rectangle dimensions = SPA_RECTANGLE(1920, 1080);

    std::vector<uint8_t> rawFrameBuffer;
    std::vector<uint8_t> encodedFrameBuffer;
    Encode encoder;
  };

  struct PipewireSource {
    GSource base;
    Data *data;
  };

private:
  static void onSessionClosed(GObject *sourceObject, gpointer userData);
  static void onRemoteDesktopReady(GObject *source_object, GAsyncResult *res,
                                   gpointer userData);
  static void onSessionStart(GObject *source_object, GAsyncResult *res,
                             gpointer userData);

  static void onStreamParamsChange(void *userData, uint32_t id,
                                   const struct spa_pod *param);

  static void onStreamProcess(void *userData);

private:
  Data m_Data;

public:
  Remote();
  ~Remote();

  void begin();
};
