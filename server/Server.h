#pragma once

#include "OpenSSL.h"
#include "Remote.h"
#include "Socket.h"

#include <atomic>
#include <string>
#include <thread>

class Server {
private:
  Remote *m_Remote = nullptr;
  Socket m_Socket;
  OpenSSL m_Openssl;

  std::atomic<bool> m_Running = true;

  std::thread m_RemoteThread;
  std::thread m_InputThread;

public:
  Server() = default;
  ~Server();

  void initialize();

  bool authenticate();

  void remote();
};