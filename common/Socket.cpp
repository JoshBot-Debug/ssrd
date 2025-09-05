#include "Socket.h"

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

#include "Utility.h"

Socket::Socket() {
  // Create socket
  m_Server = socket(AF_INET, SOCK_STREAM, 0);
  if (m_Server < 0)
    throw std::runtime_error("Socket failed");
}

Socket::~Socket() {
  if (m_Server > -1)
    close(m_Server);

  if (m_Client > -1)
    close(m_Client);
}

void Socket::listen(uint16_t port) {
  // Bind socket to IP/Port
  m_ServerAddress.sin_family = AF_INET;
  m_ServerAddress.sin_addr.s_addr = INADDR_ANY;
  m_ServerAddress.sin_port = htons(port);
  if (bind(m_Server, (struct sockaddr *)&m_ServerAddress,
           sizeof(m_ServerAddress)) < 0) {
    close(m_Server);
    throw std::runtime_error("Bind failed");
  }

  // Listen for connections
  if (::listen(m_Server, 1) < 0) {
    close(m_Server);
    throw std::runtime_error("Listen failed");
  }

  LOG("Listening on port", port);

  // Accept connection
  m_Client = accept(m_Server, (struct sockaddr *)&m_ClientAddress,
                    &m_ClientAddressLength);
  if (m_Client < 0) {
    close(m_Server);
    throw std::runtime_error("Failed to accept an incomming connection");
  }

  LOG("Connection established with client", m_Client);
}

void Socket::connect(const Client &client) {
  m_ServerAddress.sin_family = AF_INET;
  m_ServerAddress.sin_port = htons(client.port);
  inet_pton(AF_INET, client.ip.c_str(), &m_ServerAddress.sin_addr);

  LOG("Connecting to", client.username, client.ip, client.port);

  // Connect to server
  if (::connect(m_Server, (struct sockaddr *)&m_ServerAddress,
                sizeof(m_ServerAddress)) < 0) {
    close(m_Server);
    throw std::runtime_error("Connection failed");
  }

  LOG("Connection established with server", m_Server);
}

void Socket::message(const void *buffer, size_t size) {
  send(getSocketID(), buffer, size, 0);
}

void Socket::receive(uint32_t size,
                     const std::function<void(void *, ssize_t)> &callback) {

  int socket = getSocketID();

  LOG(m_Client, m_Server);

  if (socket < 0)
    return;

  LOG(m_Client > -1 ? "Receiving from client" : "Receiving from server");

  m_ReceiverThread = std::thread([size, socket, callback]() {
    char buffer[size] = {0};
    while (true) {
      memset(buffer, 0, sizeof(buffer));
      ssize_t n = read(socket, buffer, sizeof(buffer));
      if (n > 0)
        callback(buffer, n);
    }
  });

  // m_ReceiverThread.detach();
  m_ReceiverThread.join();
}
