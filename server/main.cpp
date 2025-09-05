#include <iostream>

#include "Remote/Remote.h"
#include "Socket.h"
#include "Utility.h"

int main(int argc, char *argv[]) {
  LOG("ssrd-server");

  // Remote remote;

  // remote.onStream([](std::vector<uint8_t> buffer) {
  //   writeEncodedRGBBufferToDisk("feed.h264", buffer);
  // });

  // remote.begin();

  Socket socket;
  socket.listen(1998);
  
  std::string m = "Server here!";
  socket.message(m.c_str(), sizeof(m));
  socket.receive(1024, [](void *buffer, ssize_t size) {
    std::cout << "Client says: "
    << std::string(static_cast<char *>(buffer), size) << std::endl;
  });
}