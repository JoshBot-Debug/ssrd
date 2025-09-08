#include "CLI11.h"
#include "Client.h"
#include "Constant.h"
#include "OpenSSL.h"
#include "Socket.h"
#include "Utility.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-client");

  CLI::App app{"Secure Shell Remote Desktop"};

  Client client;
  std::string identity;

  app.add_option("destination", client.destination,
                 "The destination server eg. <username>@<ip-address>[:<port>]")
      ->required();

  app.add_option("-i", identity, "Identity file");

  CLI11_PARSE(app, argc, argv);

  client.fromString(client.destination);

  Socket socket;
  OpenSSL openssl;

  socket.connect(client);

  while (true) {
    bool authenticated = false;

    std::vector<uint8_t> buffer = socket.read();
    if (!buffer.size())
      continue;

    LOG("Received random bytes");

    openssl.loadPrivateKey("/home/joshua/.ssrd/private.pem");
    std::vector<uint8_t> signature = openssl.sign(buffer.data(), buffer.size());

    LOG("Signed random bytes");

    socket.send(signature.data(), signature.size());

    LOG("Sent signature");

    while (true) {
      std::vector<uint8_t> buffer = socket.read();
      if (!buffer.size())
        continue;

      authenticated = buffer[0];
      break;
    }

    if (authenticated)
      break;
  }

  LOG("Secure connection established");
}
