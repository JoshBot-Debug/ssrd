#include "Socket.h"

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

#include "OpenSSL.h"

bool Socket::isSocketBound(int socket) {
  struct sockaddr_in address;
  socklen_t len = sizeof(address);
  if (getsockname(socket, (struct sockaddr *)&address, &len) == -1)
    return false;
  return address.sin_port != 0;
}

Socket::Socket() {
  // Create socket
  m_Server = socket(AF_INET, SOCK_STREAM, 0);
  if (m_Server < 0)
    throw std::runtime_error("Socket failed");
}

Socket::~Socket() {
  if (m_Server > -1)
    ::close(m_Server);

  if (m_Client > -1)
    ::close(m_Client);
}

void Socket::listen(uint16_t port) {
  if (!isSocketBound(m_Server)) {
    // Bind socket to IP/Port
    m_ServerAddress.sin_family = AF_INET;
    m_ServerAddress.sin_addr.s_addr = INADDR_ANY;
    m_ServerAddress.sin_port = htons(port);
    if (bind(m_Server, (struct sockaddr *)&m_ServerAddress,
             sizeof(m_ServerAddress)) < 0) {
      ::close(m_Server);
      throw std::runtime_error("Bind failed");
    }
  }

  // Listen for connections
  if (::listen(m_Server, SOMAXCONN) < 0) {
    ::close(m_Server);
    throw std::runtime_error("Listen failed");
  }

  LOG("Listening on port", port);

  socklen_t len = sizeof(m_ClientAddress);

  // Accept connection
  m_Client = accept(m_Server, (struct sockaddr *)&m_ClientAddress, &len);

  if (m_Client > 0)
    LOG("Establish a connection with client", m_Client);
}

void Socket::connect(const Client &client) {
  m_ServerAddress.sin_family = AF_INET;
  m_ServerAddress.sin_port = htons(client.port);
  inet_pton(AF_INET, client.ip.c_str(), &m_ServerAddress.sin_addr);

  LOG("Connecting to", client.username, client.ip, client.port);

  // Connect to server
  if (::connect(m_Server, (struct sockaddr *)&m_ServerAddress,
                sizeof(m_ServerAddress)) < 0) {
    ::close(m_Server);
    throw std::runtime_error("Connection failed");
  }

  LOG("Connection established with server", m_Server);
}

ssize_t Socket::send(int fd, const void *bytes, size_t size, int flags) {
  ssize_t total = 0;

  while (total < size) {
    ssize_t sent = ::send(fd, static_cast<const uint8_t *>(bytes) + total,
                          size - total, 0);
    if (sent <= 0)
      return total;
    total += sent;
  }

  return total;
}

ssize_t Socket::read(int fd, void *bytes, size_t size) {
  ssize_t total = 0;

  while (total < size) {
    ssize_t received =
        ::read(fd, static_cast<uint8_t *>(bytes) + total, size - total);

    if (received <= 0)
      return total;

    total += received;
  }

  return total;
}

void Socket::send(const void *bytes, size_t size) {
  int fd = getSocketID();

  uint32_t pSize = htonl(static_cast<uint32_t>(size));

  // Send the size
  if (send(fd, &pSize, sizeof(pSize), 0) < sizeof(pSize))
    throw std::runtime_error("Failed to send size bytes");

  // Send the buffer
  if (send(fd, bytes, size, 0) < size)
    throw std::runtime_error("Failed to send bytes");
}

std::vector<uint8_t> Socket::read() {
  int fd = getSocketID();

  std::vector<uint8_t> buffer(0);

  // Read the first 4-byte
  uint32_t sBuffer = 0;

  ssize_t received = read(fd, &sBuffer, sizeof(sBuffer));

  if (received == 0)
    return buffer;

  if (received < sizeof(sBuffer))
    throw std::runtime_error("Failed to read size bytes");

  uint32_t size = ntohl(sBuffer);

  buffer.resize(size);

  // Read the payload
  if (read(fd, buffer.data(), size) < size)
    throw std::runtime_error("Failed to read bytes");

  return buffer;
}

void Socket::close(Close type) {
  switch (type) {
  case Close::CLIENT:
    ::close(m_Client);
    m_Client = -1;
    break;

  case Close::SERVER:
    ::close(m_Server);
    m_Server = -1;
    break;

  default:
    break;
  }
}
