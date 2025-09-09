#include <iostream>

#include "OpenSSL.h"
#include "Remote/Remote.h"
#include "Socket.h"
#include "Utility.h"

#include <openssl/rand.h>
#include <signal.h>

#include "Payload.h"

std::vector<uint8_t> randomBytes(size_t length) {
  std::vector<uint8_t> buffer(length);
  if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1)
    throw std::runtime_error("Failed to generate random bytes");
  return buffer;
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);

  Payload payload;

  payload.set("resize");
  // payload.set("w");
  // payload.set(1920);
  // payload.set("h");
  // payload.set(1080);

  auto result = Payload::get<const char *>(0, payload.buffer);
  std::cout << "Result: " << result << std::endl;

  return 1;

  Socket socket;
  OpenSSL openssl;

  bool authenticated = false;

  while (true) {
    socket.listen(1998);

    std::vector<uint8_t> bytes = randomBytes(256);

    while (true) {
      if (socket.send(bytes.data(), bytes.size()) <= 0)
        break;

      LOG("Sending random bytes");

      std::vector<uint8_t> signature = {};

      if (socket.read(signature) == -1)
        break;

      if (signature.size()) {
        LOG("Verifing signature");

        EVP_PKEY *publicKey =
            openssl.loadPublicKey("/home/joshua/.ssrd/public.pem");

        authenticated = openssl.verify(
            publicKey, bytes.data(), bytes.size(),
            static_cast<const uint8_t *>(signature.data()), signature.size());

        break;
      }
    }

    if (!authenticated) {
      socket.close(Socket::Close::CLIENT);
      continue;
    }

    if (socket.send(&authenticated, sizeof(authenticated)) > 0) {
      LOG("Secure connection established");

      Remote remote;

      remote.onResize([&socket](int width, int height) {
        // TODO send buffer
      });

      remote.onStream([&socket, &remote](std::vector<uint8_t> buffer) {
        if (socket.send(buffer.data(), buffer.size()) == -1)
          remote.end();
      });

      LOG("Remote desktop begin");

      remote.begin();

      socket.close(Socket::Close::CLIENT);
      LOG("Remote desktop end");
    }
  }
}