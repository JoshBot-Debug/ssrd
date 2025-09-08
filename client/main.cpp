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
  socket.connect(client);

  std::string m = "Client here!";
  socket.send(m.c_str(), sizeof(m));

  while (true) {
    std::vector<uint8_t> buffer = socket.read();
    if(buffer.size())
    {
      std::string message(buffer.begin(), buffer.end());
      std::cout << "Server says: " << message.c_str() << std::endl;
    }
  }
}
