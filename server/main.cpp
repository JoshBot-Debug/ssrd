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
  socket.listen(1998);

  OpenSSL openssl;

  while (true) {
    std::vector<uint8_t> buffer = socket.read();
    if (buffer.size()) {
      std::string message(buffer.begin(), buffer.end());
      std::cout << "Client says: " << message.c_str() << std::endl;

      std::string m = "Server here!";
      socket.send(m.c_str(), sizeof(m));
    }
  }

  // OpenSSL openssl;

  // std::vector<uint8_t> bytes = randomBytes(32);

  // openssl.loadPrivateKey("/home/joshua/.ssrd/private.pem");
  // std::vector<uint8_t> signature = openssl.sign(bytes.data(), bytes.size());

  // EVP_PKEY *publicKey =
  // openssl.loadPublicKey("/home/joshua/.ssrd/public.pem");

  // openssl.verify(publicKey, bytes.data(), bytes.size(), signature.data(),
  // signature.size());
}