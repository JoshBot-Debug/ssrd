#pragma once

#include <atomic>
#include <mutex>
#include <stdlib.h>
#include <thread>

#include "OpenSSL.h"
#include "Socket.h"
#include "Window.h"

class Client {
private:
  Socket m_Socket;
  OpenSSL m_Openssl;
  Window m_Window;

  std::string m_IP;
  uint16_t m_Port = 1998;
  std::string m_Identity;

  std::atomic<uint32_t> m_Width = 0;
  std::atomic<uint32_t> m_Height = 0;

  std::mutex m_VideoBufferMutex;
  std::vector<uint8_t> m_VideoBuffer = {};

  std::atomic<bool> m_Running = true;

  std::thread m_WindowThread;
  std::thread m_StreamThread;

public:
  Client() = default;
  ~Client();

  int initialize(int argc, char *argv[]);

private:
  bool authentication();
  void stream();
  void window();
};