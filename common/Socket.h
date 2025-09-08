#pragma once

#include <arpa/inet.h>
#include <functional>

#include "Client.h"
#include "Utility.h"

class Socket {

private:
  struct Payload {
    uint32_t size;
    std::vector<uint8_t> bytes;
  };

public:
  enum class Close { CLIENT = 0, SERVER = 1 };

private:
  int m_Server = -1, m_Client = -1;
  sockaddr_in m_ServerAddress, m_ClientAddress;

  int getSocketID() { return m_Client > -1 ? m_Client : m_Server; };

  bool isSocketBound(int socket);

public:
  Socket();
  ~Socket();

  void listen(uint16_t port);

  void connect(const Client &client);

  ssize_t send(int fd, const void *bytes, size_t size, int flags);

  ssize_t read(int fd, void *bytes, size_t size);

  void send(const void *bytes, size_t size);

  std::vector<uint8_t> read();

  void close(Close type);
};