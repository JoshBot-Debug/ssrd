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
  m_IsListening = true;
  m_ListeningPort = port;

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

  OpenSSL openssl;
  openssl.loadPrivateKey((HOME_DIR + "/.ssrd/id_rsa").c_str());

  while (true) {
    // Accept connection
    m_Client = accept(m_Server, (struct sockaddr *)&m_ClientAddress,
                      &m_ClientAddressLength);

    if (m_Client < 0) {
      LOG("Failed to accept an incomming connection");
      continue;
    }

    char buffer[1024] = {0};
    memset(buffer, 0, sizeof(buffer));
    ssize_t size = read(m_Client, buffer, sizeof(buffer));

    if (size > 0) {
      try {
        openssl.loadPublicKey(buffer, size);
        if (openssl.validate()) {
          LOG("Secure connection established");
          break;
        }
      } catch (const std::runtime_error &e) {
        LOG(e.what());
      }
    }

    ::close(m_Client);
    m_Client = -1;

    LOG("Failed to establish a secure connection");
  }
}

void Socket::connect(const Client &client, const std::string &identity) {
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

  std::vector<uint8_t> bytes;

  try {
    bytes = readFileBytes(identity);
  } catch (const std::runtime_error &e) {
  }

  message(bytes.data(), bytes.size());

  LOG("Connection established with server", m_Server);
}

void Socket::message(const void *buffer, size_t size) {
  send(getSocketID(), buffer, size, 0);
}

void Socket::receive(uint32_t size,
                     const std::function<void(void *, ssize_t)> &callback) {

  int socket = getSocketID();

  if (socket < 0)
    return;

  m_ReceiverThread = std::thread([size, socket, callback]() {
    char buffer[size] = {0};
    while (true) {
      memset(buffer, 0, sizeof(buffer));
      ssize_t n = read(socket, buffer, sizeof(buffer));
      if (n > 0)
        callback(buffer, n);
    }
  });

  m_ReceiverThread.join();
}

void Socket::close() {
  if (!m_IsListening) {
    if (m_Server > -1)
      ::close(m_Server);

    m_Server = -1;
    return;
  }

  if (m_Client > -1)
    ::close(m_Client);

  m_Client = -1;

  listen(m_ListeningPort);
}
