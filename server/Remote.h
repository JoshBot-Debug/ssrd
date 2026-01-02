#pragma once

#include <functional>
#include <vector>

#include <libportal/portal.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/audio/format-utils.h>

struct Chunk {
  std::vector<float> &buffer;
  uint32_t frames;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t bits;
};

class Remote {
private:
  struct Stream {
    pw_stream *stream = nullptr;
    spa_hook streamListener;
    pw_stream_events streamEvents{};
  };

  struct PwData {
    pw_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    GSourceFuncs pipewireSourceFuncs{};
    Stream videoStream;
    Stream audioStream;
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

    int pwFd = 0;
    guint32 targetId = 0;

    spa_video_info videoFormat{};
    spa_audio_info audioFormat{};

    std::vector<uint8_t> framebuffer = {};
    std::function<void(int width, int height)> onResize = nullptr;
    std::function<void(std::vector<uint8_t> buffer)> onStreamVideo = nullptr;
    std::function<void(const Chunk &chunk)> onStreamAudio = nullptr;
  };

  struct PipewireSource {
    GSource base;
    Data *data;
  };

private:
  static void onSessionClosed(GObject *sourceObject, gpointer userData);

  static void onRemoteDesktopReady(GObject *source_object, GAsyncResult *res,
                                   gpointer userData);

  static void onSessionStart(GObject *sourceObject, GAsyncResult *res,
                             gpointer userData);

  static void onVideoStreamParamsChange(void *userData, uint32_t id,
                                        const struct spa_pod *param);

  static void onVideoStreamProcess(void *userData);

  static void onAudioStreamParamsChange(void *userData, uint32_t id,
                                        const struct spa_pod *param);

  static void onAudioStreamProcess(void *userData);

private:
  Data m_Data;

public:
  Remote();
  ~Remote();

  void onStreamVideo(
      const std::function<void(std::vector<uint8_t> buffer)> &callback);
  void onStreamAudio(const std::function<void(const Chunk &chunk)> &callback);

  void onResize(const std::function<void(int width, int height)> &callback);

  void begin();

  void end();

  void keyboard(int key, int action, int mods);

  void mouse(double x, double y);

  void mouseButton(int button, int action, int mods);

  void mouseScroll(int x, int y);
};
