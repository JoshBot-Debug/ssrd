#include "Socket.h"

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

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

void Socket::connect(const char *ip, uint16_t port) {
  m_ServerAddress.sin_family = AF_INET;
  m_ServerAddress.sin_port = htons(port);

  if (!inet_pton(AF_INET, ip, &m_ServerAddress.sin_addr))
    throw std::runtime_error("Invalid ip address");

  LOG("Connecting to", ip, port);

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
    if (sent == -1)
      return -1;

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

    if (received == -1)
      return -1;

    if (received <= 0)
      return total;

    total += received;
  }

  return total;
}

ssize_t Socket::send(const void *bytes, size_t size) {
  int fd = getSocketID();

  uint32_t pSize = htonl(static_cast<uint32_t>(size));

  // Send the size
  ssize_t sent = send(fd, &pSize, sizeof(pSize), 0);

  if (sent <= 0)
    return sent;

  if (sent < sizeof(pSize))
    throw std::runtime_error("Failed to send size bytes");

  sent = send(fd, bytes, size, 0);

  if (sent <= 0)
    return sent;

  // Send the buffer
  if (sent < size)
    throw std::runtime_error("Failed to send bytes");

  return sent;
}

ssize_t Socket::read(std::vector<uint8_t> &buffer) {
  buffer.clear();

  int fd = getSocketID();

  uint32_t sBuffer = 0;

  // Read the first 4-byte
  ssize_t received = read(fd, &sBuffer, sizeof(sBuffer));

  if (received <= 0)
    return received;

  if (received < sizeof(sBuffer))
    throw std::runtime_error("Failed to read size bytes");

  uint32_t size = ntohl(sBuffer);

  buffer.resize(size);

  // Read the payload
  received = read(fd, buffer.data(), size);

  if (received <= 0)
    return received;

  if (received < size)
    throw std::runtime_error("Failed to read bytes");

  return received;
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
