#include "CLI11.h"
#include "Constant.h"
#include "Client.h"
#include "Socket.h"
#include "Utility.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-client");

  CLI::App app{"Secure Shell Remote Desktop"};

  Client client;

  app.add_option("destination", client.destination,
                 "The destination server eg. <username>@<ip-address>[:<port>]")
      ->required();

  CLI11_PARSE(app, argc, argv);

  client.fromString(client.destination);

  Socket socket;
  socket.connect(client);

  std::string m = "Client here!";
  socket.message(m.c_str(), sizeof(m));
  socket.receive(1024, [](void *buffer, ssize_t size) {
    std::cout << "Server says: "
              << std::string(static_cast<char *>(buffer), size) << std::endl;
  });
}
