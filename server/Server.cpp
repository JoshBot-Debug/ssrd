#include "Server.h"
#include "Payload.h"
#include "Utility.h"
#include <filesystem>

namespace fs = std::filesystem;

static const std::string HOME_DIR = getHomeDirectory();

Server::~Server() {
  m_Running.store(false);

  if (m_InputThread.joinable())
    m_InputThread.join();
}

void Server::Initialize() {
  while (m_Running.load()) {
    // Start listening
    m_Socket.listen(1998);

    // Try authenticating
    bool authenticated = Authenticate();
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
    Remote();
  }
}

bool Server::Authenticate() {
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

      fs::path authorizedKeysDir = HOME_DIR + "/.ssrd/authorized_keys";

      if (fs::is_directory(authorizedKeysDir)) {
        for (const auto &entry : fs::directory_iterator(authorizedKeysDir)) {
          if (entry.is_regular_file()) {
            EVP_PKEY *publicKey = m_Openssl.loadPublicKey(entry.path().c_str());

            if (m_Openssl.verify(publicKey, bytes.data(), bytes.size(),
                                 static_cast<const uint8_t *>(signature.data()),
                                 signature.size()))
              return true;
          }
        }

        return false;
      }
    }
  }

  return false;
}

void Server::Remote() {
  LOG("Secure connection established");

  m_R2.OnResize([this](int width, int height) {
    m_Encoder.initialize(width, height);

    Payload payload;
    payload.set("resize");
    payload.set(width);
    payload.set(height);

    m_Socket.send(payload.buffer.data(), payload.buffer.size());
  });

  m_R2.OnStreamVideo([this](std::vector<uint8_t> raw, uint64_t time) {
    std::vector<uint8_t> buffer = m_Encoder.encode(raw);

    if (buffer.size() == 0)
      return;

    Payload payload;
    payload.set("stream-video");
    payload.set(time);
    payload.set(buffer.data(), buffer.size());

    if (m_Socket.send(payload.buffer.data(), payload.buffer.size()) == -1)
      m_R2.EndSession();
  });

  m_R2.OnStreamAudio([this](const Chunk &chunk, uint64_t time) {
    auto resampled =
        m_AudioEncoder.LinearResample(chunk.buffer, chunk.sampleRate, 24000);

    std::vector<uint8_t> buffer =
        m_AudioEncoder.Encode(resampled.data(), resampled.size(), 960);

    if (buffer.size() == 0)
      return;

    Payload payload;
    payload.set("stream-audio");
    payload.set(time);
    payload.set(buffer.data(), buffer.size());

    if (m_Socket.send(payload.buffer.data(), payload.buffer.size()) == -1)
      m_R2.EndSession();
  });

  LOG("Remote desktop begin");

  m_R2.OnSessionDisconnected([this]() {
    Payload payload;
    payload.set("end-session");
    m_Socket.send(payload.buffer.data(), payload.buffer.size());
    m_Socket.close(Socket::Close::CLIENT);
    LOG("Remote desktop end");
  });

  m_R2.BeginSession();

  while (m_R2.IsRemoteDesktopActive()) {

    if (!m_R2.IsSessionActive()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    std::vector<uint8_t> buffer = {};

    if (m_Socket.read(buffer) <= 0)
      break;

    std::vector<uint8_t> bytes = Payload::get(0, buffer);
    auto type =
        std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());

    if (type == "key") {
      auto key = Payload::toInt(Payload::get(1, buffer));
      auto action = Payload::toInt(Payload::get(2, buffer));
      auto mods = Payload::toInt(Payload::get(3, buffer));
      m_R2.Keyboard(key, action, mods);
    }

    if (type == "mouse-move") {
      auto x = Payload::toDouble(Payload::get(1, buffer));
      auto y = Payload::toDouble(Payload::get(2, buffer));
      m_R2.Mouse(x, y);
    }

    if (type == "mouse-button") {
      auto button = Payload::toInt(Payload::get(1, buffer));
      auto action = Payload::toInt(Payload::get(2, buffer));
      auto mods = Payload::toInt(Payload::get(3, buffer));
      m_R2.MouseButton(button, action, mods);
    }

    if (type == "mouse-scroll") {
      auto x = Payload::toInt(Payload::get(1, buffer));
      auto y = Payload::toInt(Payload::get(2, buffer));
      m_R2.MouseScroll(x, y);
    }
  }
}