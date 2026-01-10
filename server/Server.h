#pragma once

#include "OpenSSL.h"
// #include "Remote.h"
#include "R2.h"
#include "Encoder.h"
#include "Socket.h"
#include "AudioEncoder.h"

#include <atomic>
#include <string>
#include <thread>

class Server {
private:
  // Remote *m_Remote = nullptr;
  R2 m_R2;
  Socket m_Socket;
  OpenSSL m_Openssl;
  Encoder m_Encoder;
  AudioEncoder m_AudioEncoder{24000};

  std::atomic<bool> m_Running = true;

  std::thread m_InputThread;

public:
  Server() = default;
  ~Server();

  void Initialize();

  bool Authenticate();

  void Remote();
};