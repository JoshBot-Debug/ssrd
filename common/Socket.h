#pragma once

#include <arpa/inet.h>
#include <functional>
#include <thread>

#include "Client.h"
#include "Utility.h"

static const std::string HOME_DIR = getHomeDirectory();

class Socket {
private:
  bool m_IsListening = false;
  uint16_t m_ListeningPort = 0;

  int m_Server = -1, m_Client = -1;
  sockaddr_in m_ServerAddress, m_ClientAddress;
  socklen_t m_ClientAddressLength = sizeof(m_ClientAddress);
  std::thread m_ReceiverThread;

  int getSocketID() { return m_Client > -1 ? m_Client : m_Server; };

  bool isSocketBound(int socket);

public:
  Socket();
  ~Socket();

  void listen(uint16_t port);

  void connect(const Client &client, std::string &identity);

  void message(const void *buffer, size_t size);

  void receive(uint32_t size,
               const std::function<void(void *buffer, ssize_t size)> &callback);

  void close();
};