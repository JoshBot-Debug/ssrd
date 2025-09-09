#pragma once

#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>

struct Payload {
  std::vector<uint8_t> buffer = {};

  void set(std::string value) {
    uint32_t size = static_cast<uint32_t>(value.size());
    buffer.resize(size);
    std::memcpy(buffer.data(), value.data(), size);
  };

  void set(uint32_t value) {
    buffer.resize(sizeof(uint32_t));
    value = htonl(value);
    std::memcpy(buffer.data(), &value, sizeof(uint32_t));
  };

  void set(const void *value, uint32_t size) {
    buffer.resize(size);
    std::memcpy(buffer.data(), value, size);
  };

  template <typename T>
  static T get(uint32_t index, std::vector<uint8_t> &buffer) {
    // uint8_t *bytes = buffer.data();

    std::vector<uint8_t> result = {};

    size_t offset = 0;

    for (size_t i = 0; i < index + 1; i++) {
      uint32_t size = 0;
      std::memcpy(buffer.data() + offset, &size, sizeof(uint32_t));

      std::cout << size << std::endl;
      if (i == index) {
        result.resize(size);
        std::memcpy(buffer.data() + offset, result.data(), size);
        break;
      }

      offset += sizeof(uint32_t) + size;
    }

    return reinterpret_cast<T>(result.data());
  };
};
