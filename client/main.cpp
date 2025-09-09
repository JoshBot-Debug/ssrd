#include "CLI11.h"
#include "Constant.h"
#include "OpenSSL.h"
#include "Socket.h"
#include "Utility.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-client");

  CLI::App app{"Secure Shell Remote Desktop"};
  app.set_help_flag("--help", "Display help information.");

  std::string identity = "/home/joshua/.ssrd/private.pem";

  std::string ip;
  uint16_t port = 1998;

  app.add_option("-h,--host", ip,
                 "The IP address of the destination server eg. 127.0.0.1")
      ->required();
      
  app.add_option("-p,--port", port, "The destination port. Defaults to 1998");

  app.add_option("-i", identity, "Identity file");

  CLI11_PARSE(app, argc, argv);

  Socket socket;
  OpenSSL openssl;

  socket.connect(ip.c_str(), port);

  while (true) {
    bool authenticated = false;

    std::vector<uint8_t> buffer = {};

    if (socket.read(buffer) == -1)
      break;

    if (!buffer.size())
      continue;

    LOG("Received random bytes");

    openssl.loadPrivateKey(identity.c_str());
    std::vector<uint8_t> signature = openssl.sign(buffer.data(), buffer.size());

    LOG("Signed random bytes");

    socket.send(signature.data(), signature.size());

    LOG("Sent signature");

    while (true) {
      std::vector<uint8_t> buffer = {};

      if (socket.read(buffer) == -1)
        break;

      if (!buffer.size())
        continue;

      authenticated = buffer[0];
      break;
    }

    if (authenticated)
      break;
  }

  LOG("Secure connection established");

  while (true) {
    std::vector<uint8_t> buffer = {};

    if (socket.read(buffer) <= 0)
      break;

    writeRGBBufferToPPM("feed.ppm", buffer, 1920, 1080);
  }
}
