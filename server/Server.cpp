#include "Server.h"
#include "Payload.h"
#include "Utility.h"

static const std::string HOME_DIR = getHomeDirectory();

Server::~Server() {
  m_Running.store(false);
  delete m_Remote;
  m_Remote = nullptr;
}

void Server::initialize() {
  while (m_Running.load()) {
    // Start listening
    m_Socket.listen(1998);

    // Try authenticating
    bool authenticated = authenticate();
    if (!authenticated) {
      m_Socket.close(Socket::Close::CLIENT);
      continue;
    }

    // Inform the client about the connection
    if (m_Socket.send(&authenticated, sizeof(authenticated)) <= 0) {
      m_Socket.close(Socket::Close::CLIENT);
      continue;
    }

    // Begin the remote connection
    remote();
  }
}

bool Server::authenticate() {
  std::vector<uint8_t> bytes = randomBytes(256);

  while (true) {
    if (m_Socket.send(bytes.data(), bytes.size()) <= 0)
      break;

    LOG("Sending random bytes");

    std::vector<uint8_t> signature = {};

    if (m_Socket.read(signature) == -1)
      break;

    if (signature.size()) {
      LOG("Verifing signature");

      EVP_PKEY *publicKey =
          m_Openssl.loadPublicKey((HOME_DIR + "/.ssrd/public.pem").c_str());

      return m_Openssl.verify(publicKey, bytes.data(), bytes.size(),
                              static_cast<const uint8_t *>(signature.data()),
                              signature.size());
    }
  }

  return false;
}

void Server::remote() {
  LOG("Secure connection established");

  if (!m_Remote)
    m_Remote = new Remote();

  m_Remote->onResize([socket = &m_Socket](int width, int height) {
    Payload payload;
    payload.set("resize");
    payload.set(width);
    payload.set(height);

    socket->send(payload.buffer.data(), payload.buffer.size());
  });

  m_Remote->onStream(
      [remote = m_Remote, socket = &m_Socket](std::vector<uint8_t> buffer) {
        Payload payload;
        payload.set("stream");
        payload.set(buffer.data(), buffer.size());

        if (socket->send(payload.buffer.data(), payload.buffer.size()) == -1)
          remote->end();
      });

  LOG("Remote desktop begin");

  m_RemoteThread = std::thread([this]() { m_Remote->begin(); });

  m_InputThread = std::thread([this]() {
    while (m_Running.load()) {

      std::vector<uint8_t> buffer = {};

      if (m_Socket.read(buffer) <= 0)
        break;

      std::vector<uint8_t> bytes = Payload::get(0, buffer);
      auto type = std::string(reinterpret_cast<const char *>(bytes.data()),
                              bytes.size());

      if (type == "key") {
        auto key = Payload::toInt(Payload::get(1, buffer));
        auto action = Payload::toInt(Payload::get(2, buffer));
        auto mods = Payload::toInt(Payload::get(3, buffer));

        m_Remote->keyboard(key, action, mods);
      }

      if (type == "mouse") {
        auto x = Payload::toDouble(Payload::get(1, buffer));
        auto y = Payload::toDouble(Payload::get(2, buffer));

        m_Remote->mouse(x, y);
      }
    }
  });

  if (m_RemoteThread.joinable())
    m_RemoteThread.join();

  if (m_InputThread.joinable())
    m_InputThread.join();

  delete m_Remote;
  m_Remote = nullptr;
  m_Socket.close(Socket::Close::CLIENT);

  LOG("Remote desktop end");
}