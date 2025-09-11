#include <iostream>

#include "OpenSSL.h"
#include "Remote.h"
#include "Socket.h"
#include "Utility.h"

#include <openssl/rand.h>
#include <signal.h>

#include "Payload.h"

#include "Server.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-server");
  signal(SIGPIPE, SIG_IGN);

  Server server;
  server.initialize();
}