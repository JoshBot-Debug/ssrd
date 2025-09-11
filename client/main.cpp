#include "Client.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-client");

  Client client;
  client.initialize(argc, argv);
}
