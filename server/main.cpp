#include <iostream>

#include "OpenSSL.h"
#include "Remote/Remote.h"
#include "Socket.h"
#include "Utility.h"

#include <openssl/rand.h>

std::vector<uint8_t> randomBytes(size_t length) {
  std::vector<uint8_t> buffer(length);
  if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1)
    throw std::runtime_error("Failed to generate random bytes");
  return buffer;
}

int main(int argc, char *argv[]) {
  // LOG("ssrd-server");

  // // Remote remote;

  // // remote.onStream([](std::vector<uint8_t> buffer) {
  // //   writeEncodedRGBBufferToDisk("feed.h264", buffer);
  // // });

  // // remote.begin();

  Socket socket;
  OpenSSL openssl;

  bool authenticated = false;

  while (true) {
    socket.listen(1998);

    std::vector<uint8_t> bytes = randomBytes(32);

    while (true) {
      socket.send(bytes.data(), bytes.size());

      LOG("Sending random bytes");

      std::vector<uint8_t> signature = socket.read();
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

    if (authenticated)
      break;

    socket.close(Socket::Close::CLIENT);
  }

  socket.send(&authenticated, sizeof(authenticated));

  LOG("Secure connection established");

  // OpenSSL openssl;

  // std::vector<uint8_t> bytes = randomBytes(32);

  // openssl.loadPrivateKey("/home/joshua/.ssrd/private.pem");
  // std::vector<uint8_t> signature = openssl.sign(bytes.data(), bytes.size());

  // EVP_PKEY *publicKey =
  // openssl.loadPublicKey("/home/joshua/.ssrd/public.pem");

  // bool isVerified = openssl.verify(publicKey, bytes.data(), bytes.size(),
  // signature.data(), signature.size());

  // LOG(isVerified);
}