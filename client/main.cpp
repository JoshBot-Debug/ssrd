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

  LOG(identity);
  
  client.fromString(client.destination);

  Socket socket;
  socket.connect(client, identity);

  std::string m = "Client here!";
  socket.message(m.c_str(), m.size());

  socket.receive(1024, [](void *buffer, ssize_t size) {
    std::cout << "Server says: " << static_cast<char *>(buffer) << std::endl;
  });
}
