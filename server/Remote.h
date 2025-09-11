#pragma once

#include <functional>
#include <vector>

#include <libportal/portal.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

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

    std::vector<uint8_t> framebuffer = {};
    std::function<void(int width, int height)> onResize = nullptr;
    std::function<void(std::vector<uint8_t> buffer)> onStream = nullptr;
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

  void
  onStream(const std::function<void(std::vector<uint8_t> buffer)> &callback);

  void onResize(const std::function<void(int width, int height)> &callback);

  void begin();

  void end();

  void keyboard(int key, int action, int mods);

  void mouse(double x, double y);

  void mouseButton(int button, int action, int mods);
  
  void mouseScroll(double x, double y);
};
