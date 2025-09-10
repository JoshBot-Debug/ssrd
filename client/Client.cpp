#include "Client.h"

#include "CLI11.h"
#include "Constant.h"
#include "Payload.h"

#include <string>

static const std::string HOME_DIR = getHomeDirectory();

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

    while (m_ApplicationRunning) {
    }
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
    while (true) {
      std::vector<uint8_t> buffer = {};

      if (m_Socket.read(buffer) <= 0)
        break;

      auto type = std::string(
          reinterpret_cast<const char *>(Payload::get(0, buffer).data()));

      if (type == "resize") {
        m_Width.store(ntohl(*reinterpret_cast<uint32_t *>(
                          Payload::get(1, buffer).data())),
                      std::memory_order_relaxed);
        m_Height.store(ntohl(*reinterpret_cast<uint32_t *>(
                           Payload::get(2, buffer).data())),
                       std::memory_order_relaxed);
      }

      if (type == "stream") {
        std::lock_guard<std::mutex> lock(m_VideoBufferMutex);
        m_VideoBuffer = Payload::get(1, buffer);
      }
    }
  });

  m_StreamThread.detach();
}

void Client::window() {
  m_WindowThread = std::thread([this]() {
    m_Window.create();

    while (!m_Window.shouldClose()) {
      {
        std::lock_guard<std::mutex> lock(m_VideoBufferMutex);
        m_Window.setBuffer(m_VideoBuffer);
      }

      m_Window.present(m_Width.load(std::memory_order_relaxed),
                     m_Height.load(std::memory_order_relaxed));
    }
  });

  m_WindowThread.detach();
}
