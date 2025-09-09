#pragma once

#include "Utility.h"
#include <arpa/inet.h>

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
  
  bool isValidPort(const std::string &portStr, uint16_t &portOut) {
    if (portStr.empty())
      return false;
    for (char c : portStr)
      if (!isdigit(c))
        return false;
    long val = std::stol(portStr);
    if (val < 1 || val > 65535)
      return false;
    portOut = static_cast<uint16_t>(val);
    return true;
  }


public:
  Socket();
  ~Socket();

  void listen(uint16_t port);

  void connect(const char *ip, uint16_t port);

  ssize_t send(int fd, const void *bytes, size_t size, int flags);

  ssize_t read(int fd, void *bytes, size_t size);

  ssize_t send(const void *bytes, size_t size);

  ssize_t read(std::vector<uint8_t> &buffer);

  void close(Close type);
};