#include <signal.h>

#include "Server.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-server");
  signal(SIGPIPE, SIG_IGN);

  Server server;
  server.Initialize();
}