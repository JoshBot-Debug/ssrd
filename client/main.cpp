#include "CLI11.h"
#include "Constant.h"
#include "OpenSSL.h"
#include "Payload.h"
#include "Socket.h"
#include "Utility.h"

#include "Window.h"

#include "Client.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-client");

  Client client;
  client.initialize(argc, argv);
}
