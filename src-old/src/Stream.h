#pragma once

#include <string>
#include <functional>
#include <libportal/portal.h>
#include <pipewire/pipewire.h>

struct Data {
  pw_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_stream *stream = nullptr;
  spa_hook stream_listener;
};

struct PipeWireSource {
  GSource base;
  struct pw_loop *loop;
};

class Stream {
private:
  std::function<void(spa_buffer *spa_buf)> m_OnStream = nullptr;
  XdpPortal *m_Portal = nullptr;
  XdpSession *m_Session = nullptr;
  Data m_Data{};

  PipeWireSource *m_Source;
  GMainLoop *m_MainLoop;
  gulong m_SessionClosedHandler = 0;

  GSourceFuncs m_PipewireSourceFuncs;

  int m_PWFD = 0;
  guint32 m_TargetId = 0;

  bool m_IsSessionStarted = false;
  bool m_IsPipewireInitialized = false;
  bool m_IsRunning = true;

  static void onProcessStream(void *data);

  void InitializePipewire();

  static void onStateChanged(void *data, pw_stream_state old,
                             pw_stream_state state, const char *error);

  static void onStartSession(GObject *source_object, GAsyncResult *res,
                             gpointer data);

  static void onRemoteDesktopReady(GObject *source_object, GAsyncResult *res,
                                   gpointer data);

  static void onSessionClosed(GObject *source_object, gpointer user_data);

public:
  Stream() = default;
  ~Stream();

  void begin(std::function<void(spa_buffer *spa_buf)> onStream);
};