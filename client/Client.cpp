#include "Client.h"

#include "CLI11.h"
#include "Constant.h"
#include "Payload.h"

static const std::string HOME_DIR = getHomeDirectory();

Client::~Client() { m_Running.store(false); }

int Client::initialize(int argc, char *argv[]) {

  // CLI
  {
    CLI::App app{"Secure Shell Remote Desktop"};

    app.set_help_flag("--help", "Display help information.");

    m_Identity = HOME_DIR + "/.ssrd/private.pem";

    app.add_option("-h,--host", m_IP,
                   "The IP address of the destination server eg. 127.0.0.1")
        ->required();

    app.add_option("-p,--port", m_Port,
                   "The destination port. Defaults to 1998");

    app.add_option("-i", m_Identity, "Identity file");

    CLI11_PARSE(app, argc, argv);
  }

  // Connect to the server
  socket.connect(m_IP.c_str(), m_Port);

  // Authentication
  {
    if (!authentication()) {
      LOG("Failed to establish a secure connection.");
      return EXIT_FAILURE;
    }

    LOG("Secure connection established");
  }

  // Main loop
  {
    window();
    stream();

    if (m_WindowThread.joinable())
      m_WindowThread.join();

    if (m_StreamThread.joinable())
      m_StreamThread.join();
  }

  return EXIT_SUCCESS;
}

bool Client::authentication() {

  bool authenticated = false;

  while (true) {

    std::vector<uint8_t> buffer = {};

    if (socket.read(buffer) == -1)
      break;

    if (!buffer.size())
      continue;

    LOG("Received random bytes");

    m_Openssl.loadPrivateKey(m_Identity.c_str());

    std::vector<uint8_t> signature =
        m_Openssl.sign(buffer.data(), buffer.size());

    LOG("Signed random bytes");

    socket.send(signature.data(), signature.size());

    LOG("Sent signature");

    while (true) {
      std::vector<uint8_t> buffer = {};

      if (socket.read(buffer) == -1)
        break;

      if (!buffer.size())
        continue;

      authenticated = buffer[0];
      break;
    }

    return authenticated;
  }

  return authenticated;
}

void Client::stream() {
  m_StreamThread = std::thread([this]() {
    while (m_Running.load()) {
      std::vector<uint8_t> buffer = {};

      if (socket.read(buffer) <= 0)
        break;

      std::vector<uint8_t> bytes = Payload::get(0, buffer);
      auto type = std::string(reinterpret_cast<const char *>(bytes.data()),
                              bytes.size());

      if (type == "resize") {
        int width = Payload::toUInt(Payload::get(1, buffer));
        int height = Payload::toUInt(Payload::get(2, buffer));

        imageWidth.store(width, std::memory_order_relaxed);
        imageHeight.store(height, std::memory_order_relaxed);

        m_Decoder.initialize(width, height);
      }

      if (type == "stream-video") {
        std::lock_guard<std::mutex> lock(m_VBufferMut);
        m_VBuffer = m_Decoder.decode(Payload::get(1, buffer));
      }

      if (type == "stream-audio")
        m_AudioPlayer.WriteStream(m_AudioDecoder.Decode(Payload::get(1, buffer), 960 * 6));
    }
  });
}

static void onResize(GLFWwindow *window, int width, int height) {
  Client *client = static_cast<Client *>(glfwGetWindowUserPointer(window));

  float texWidth =
      static_cast<float>(client->imageWidth.load(std::memory_order_relaxed));
  float texHeight =
      static_cast<float>(client->imageHeight.load(std::memory_order_relaxed));

  float texAspect = texWidth / texHeight;
  float winAspect = (float)width / (float)height;

  int vpX, vpY, vpW, vpH;

  if (winAspect > texAspect) {
    vpH = height;
    vpW = (int)(height * texAspect);
    vpX = (width - vpW) / 2;
    vpY = 0;
  } else {
    vpW = width;
    vpH = (int)(width / texAspect);
    vpX = 0;
    vpY = (height - vpH) / 2;
  }

  {
    std::unique_lock lock(client->viewportMut);
    client->viewport.x = vpX;
    client->viewport.y = vpY;
    client->viewport.w = vpW;
    client->viewport.h = vpH;
  }

  glViewport(vpX, vpY, vpW, vpH);
}

static void onKeyPress(GLFWwindow *window, int key, int scancode, int action,
                       int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT))
      return glfwSetWindowShouldClose(window, GLFW_TRUE);

  Client *client = static_cast<Client *>(glfwGetWindowUserPointer(window));

  Payload payload;
  payload.set("key");
  payload.set(key);
  payload.set(action);
  payload.set(mods);

  client->socket.send(payload.buffer.data(), payload.buffer.size());
}

static void onMouseMove(GLFWwindow *window, double xpos, double ypos) {
  Client *client = static_cast<Client *>(glfwGetWindowUserPointer(window));

  double x = 0.0;
  double y = 0.0;

  // Get NDC mouse coords
  {
    std::unique_lock lock(client->viewportMut);
    auto viewport = client->viewport;
    x = (static_cast<double>(xpos) - viewport.x) / viewport.w;
    y = (static_cast<double>(ypos) - viewport.y) / viewport.h;
    x = std::clamp(x, 0.0, 1.0);
    y = std::clamp(y, 0.0, 1.0);
  }

  Payload payload;
  payload.set("mouse-move");
  payload.set(x);
  payload.set(y);

  client->socket.send(payload.buffer.data(), payload.buffer.size());
}

static void onMouseButton(GLFWwindow *window, int button, int action,
                          int mods) {
  Client *client = static_cast<Client *>(glfwGetWindowUserPointer(window));

  Payload payload;
  payload.set("mouse-button");
  payload.set(button);
  payload.set(action);
  payload.set(mods);

  client->socket.send(payload.buffer.data(), payload.buffer.size());
}

static void onScroll(GLFWwindow *window, double xoffset, double yoffset) {
  Client *client = static_cast<Client *>(glfwGetWindowUserPointer(window));

  Payload payload;
  payload.set("mouse-scroll");

  bool scrollHorizontal =
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

  if (scrollHorizontal) {
    payload.set(static_cast<int>(yoffset));
    payload.set(0);
  } else {
    payload.set(static_cast<int>(xoffset));
    payload.set(static_cast<int>(yoffset));
  }

  client->socket.send(payload.buffer.data(), payload.buffer.size());
}

void Client::window() {
  m_WindowThread = std::thread([this]() {
    Window w;

    w.initialize({
        .data = this,
        .onResize = onResize,
        .onKeyPress = onKeyPress,
        .onMouseMove = onMouseMove,
        .onMouseButton = onMouseButton,
        .onScroll = onScroll,
    });

    while (m_Running.load() && !w.shouldClose()) {
      {
        std::lock_guard<std::mutex> lock(m_VBufferMut);
        w.setBuffer(m_VBuffer);
      }

      uint32_t width = imageWidth.load(std::memory_order_relaxed);
      uint32_t height = imageHeight.load(std::memory_order_relaxed);
      w.present(width, height);
    }

    m_Running.store(false);
  });
}
