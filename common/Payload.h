#pragma once

#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>

struct Payload {
  std::vector<uint8_t> buffer = {};

  void set(std::string value) {
    uint32_t vSize = static_cast<uint32_t>(value.size());
    buffer.resize(sizeof(uint32_t) + vSize);
    std::memcpy(buffer.data(), &vSize, sizeof(uint32_t));
    std::memcpy(buffer.data() + sizeof(uint32_t), value.data(), vSize);
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
    std::vector<uint8_t> result = {};

    size_t offset = 0;

    for (size_t i = 0; i < index + 1; i++) {
      uint32_t size = 0;
      std::memcpy(&size, buffer.data() + offset, sizeof(uint32_t));

      offset += sizeof(uint32_t);

      if (i == index) {
        result.resize(size);
        std::memcpy(result.data(), buffer.data() + offset, size);
        break;
      }

      offset += size;
    }

    return reinterpret_cast<T>(result.data());
  };
};
