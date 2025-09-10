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
  m_Socket.connect(m_IP.c_str(), m_Port);

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

    if (m_Socket.read(buffer) == -1)
      break;

    if (!buffer.size())
      continue;

    LOG("Received random bytes");

    m_Openssl.loadPrivateKey(m_Identity.c_str());

    std::vector<uint8_t> signature =
        m_Openssl.sign(buffer.data(), buffer.size());

    LOG("Signed random bytes");

    m_Socket.send(signature.data(), signature.size());

    LOG("Sent signature");

    while (true) {
      std::vector<uint8_t> buffer = {};

      if (m_Socket.read(buffer) == -1)
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

      if (m_Socket.read(buffer) <= 0)
        break;

      std::vector<uint8_t> bytes = Payload::get(0, buffer);
      auto type = std::string(reinterpret_cast<const char *>(bytes.data()),
                              bytes.size());

      if (type == "resize") {
        m_Width.store(Payload::toUInt(Payload::get(1, buffer)),
                      std::memory_order_relaxed);
        m_Height.store(Payload::toUInt(Payload::get(2, buffer)),
                       std::memory_order_relaxed);
      }

      if (type == "stream") {
        std::lock_guard<std::mutex> lock(m_VBufferMut);
        m_VBuffer = Payload::get(1, buffer);
      }
    }
  });
}

void Client::window() {
  m_WindowThread = std::thread([this]() {
    Window w;

    w.initialize({
        .data = &m_Socket,
        .onKeyPress =
            [](GLFWwindow *window, int key, int scancode, int action,
               int mods) {
              if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT))
                  return glfwSetWindowShouldClose(window, GLFW_TRUE);

              Socket *socket =
                  static_cast<Socket *>(glfwGetWindowUserPointer(window));

              Payload payload;
              payload.set("key");
              payload.set(key);
              payload.set(action);
              payload.set(mods);

              socket->send(payload.buffer.data(), payload.buffer.size());
            },
        .onMouseMove =
            [](GLFWwindow *window, double xpos, double ypos) {
              Socket *socket =
                  static_cast<Socket *>(glfwGetWindowUserPointer(window));

              Payload payload;
              payload.set("mouse");
              payload.set(xpos);
              payload.set(ypos);

              socket->send(payload.buffer.data(), payload.buffer.size());
            },
    });

    while (m_Running.load() && !w.shouldClose()) {
      {
        std::lock_guard<std::mutex> lock(m_VBufferMut);
        w.setBuffer(m_VBuffer);
      }

      uint32_t width = m_Width.load(std::memory_order_relaxed);
      uint32_t height = m_Height.load(std::memory_order_relaxed);
      w.present(width, height);
    }

    m_Running.store(false);
  });
}
