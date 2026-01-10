#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include <functional>
#include <set>
#include <vector>

#include <libportal/portal.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/video/format-utils.h>

struct Chunk {
  std::vector<float> &buffer;
  uint32_t frames;
  uint32_t sampleRate;
  uint32_t channels;
  uint32_t bits;
};

struct Stream {
  pw_stream *stream = nullptr;
  spa_hook streamListener;
  pw_stream_events streamEvents{};
};

struct PW {
  pw_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;

  Stream videoStream;
  Stream audioStream;
};

struct Glib {
  GMainLoop *loop = nullptr;
  GMainContext *context = nullptr;
  GSourceFuncs sourceFunctions = {};
  gulong sessionClosedHandle = 0;
};

struct XDP {
  XdpSession *session = nullptr;
  XdpPortal *portal = nullptr;
};

using VideoStreamCallback =
    std::function<void(std::vector<uint8_t> buffer, uint64_t time)>;
using AudioStreamCallback =
    std::function<void(const Chunk &chunk, uint64_t time)>;
using ResizeCallback = std::function<void(int width, int height)>;

struct UserData {
  PW pw = {};
  Glib g = {};
  XDP xdp = {};

  int pwFd = 0;
  guint32 targetId = 0;

  spa_video_info videoFormat = {};
  spa_audio_info audioFormat = {};

  std::vector<uint8_t> framebuffer = {};

  ResizeCallback onResize = nullptr;

  VideoStreamCallback onStreamVideo = nullptr;
  AudioStreamCallback onStreamAudio = nullptr;

  std::function<void()> onSessionConnected = nullptr;
  std::function<void()> onSessionDisconnected = nullptr;

  std::atomic<bool> isRunning;
};

struct PipewireGSource {
  GSource base;
  UserData *userData;
};

class R2 {
private:
  std::atomic<bool> m_Running;
  std::thread m_Thread;
  std::mutex m_Mutex;
  std::condition_variable m_CV;
  std::queue<std::function<void()>> m_Queue;

  UserData m_UserData;
  std::set<uint32_t> m_PressedKeyboardKeys;
  std::set<uint32_t> m_PressedKeyboardMods;
  std::set<uint32_t> m_PressedMouseButtons;

public:
  R2();
  ~R2();

  void Stop();
  void EndSession();
  void BeginSession();

  void OnStreamVideo(const VideoStreamCallback &callback);

  void OnStreamAudio(const AudioStreamCallback &callback);

  void OnResize(const ResizeCallback &callback);

  void OnSessionConnected(const std::function<void()> &callback);

  void OnSessionDisconnected(const std::function<void()> &callback);

  bool IsRemoteDesktopActive();
  bool IsSessionActive();

  void Keyboard(int key, int action, int mods);

  void Mouse(double x, double y);

  void MouseButton(int button, int action, int mods);

  void MouseScroll(int x, int y);

private:
  void Start();
  void Initialize();
  void Deinitialize();

  static void OnSessionStart(GObject *sourceObject, GAsyncResult *res,
                             gpointer userData);
  static void OnSessionClosed(GObject *sourceObject, gpointer userData);

  static void OnVideoStreamParamsChange(void *userData, uint32_t id,
                                        const struct spa_pod *param);

  static void OnVideoStreamProcess(void *userData);

  static void OnAudioStreamParamsChange(void *userData, uint32_t id,
                                        const struct spa_pod *param);

  static void OnAudioStreamProcess(void *userData);
};