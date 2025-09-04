#pragma once

#include <functional>
#include <libportal/portal.h>
#include <pipewire/pipewire.h>

struct Data {
  pw_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_stream *stream = nullptr;
  spa_hook stream_listener;
  int pwfd = 0;
  guint32 targetId = 0;
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

  GMainLoop *m_MainLoop;
  gulong m_SessionClosedHandler = 0;
  
  static void onProcessStream(void *data);

  static void onStateChanged(void *data, pw_stream_state old,
                             pw_stream_state state, const char *error);

  static void onInitializePipewire(Stream *self);

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