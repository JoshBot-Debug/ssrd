#pragma once

#include <atomic>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>

#include "AudioDecoder.h"
#include "Decoder.h"
#include "OpenSSL.h"
#include "Socket.h"
#include "StreamPlayer.h"
#include "Window.h"

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
  AudioDecoder m_AudioDecoder{24000};

  std::string m_IP;

  uint16_t m_Port = 1998;
  std::string m_Identity;

  std::atomic<bool> m_Running = true;

  std::thread m_WindowThread;
  std::thread m_StreamThread;

  StreamPlayer m_StreamPlayer{24000, 2, 40'000'000};

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