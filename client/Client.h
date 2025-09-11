#pragma once

#include <atomic>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>

#include "OpenSSL.h"
#include "Socket.h"
#include "Window.h"
#include "Decoder.h"

class Client {
private:
  struct Viewport {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

private:
  OpenSSL m_Openssl;
  Decoder m_Decoder;

  std::string m_IP;
  uint16_t m_Port = 1998;
  std::string m_Identity;

  std::mutex m_VBufferMut;
  std::vector<uint8_t> m_VBuffer = {};

  std::atomic<bool> m_Running = true;

  std::thread m_WindowThread;
  std::thread m_StreamThread;

public:
  Socket socket;
  std::atomic<uint32_t> imageWidth = 0;
  std::atomic<uint32_t> imageHeight = 0;

  Viewport viewport;
  std::mutex viewportMut;

public:
  Client() = default;
  ~Client();

  int initialize(int argc, char *argv[]);

private:
  bool authentication();
  void stream();
  void window();
};