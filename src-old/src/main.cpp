#include "Stream.h"

#include <iostream>

int main(int argc, char **argv) {

  Stream stream;

  stream.begin([](spa_buffer *spa_buf) {
    if (spa_buf->datas[0].data) {
      void *frame = spa_buf->datas[0].data;
      size_t size = spa_buf->datas[0].chunk->size;

      std::cout << "Got frame, size = " << size << " bytes" << std::endl;
      // TODO: use frame data (e.g., copy, encode, write to file)
    }
  });

  return 0;
}