#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>

struct Payload {
  std::vector<uint8_t> buffer = {};

  void set(std::string value) {
    uint32_t size = static_cast<uint32_t>(value.size());
    uint32_t csize = static_cast<uint32_t>(buffer.size());
    buffer.resize(csize + sizeof(uint32_t) + size);
    std::memcpy(buffer.data() + csize, &size, sizeof(uint32_t));
    std::memcpy(buffer.data() + csize + sizeof(uint32_t), value.data(), size);
  };

  void set(uint32_t value) {
    value = htonl(value);
    uint32_t size = static_cast<uint32_t>(sizeof(uint32_t));
    uint32_t csize = static_cast<uint32_t>(buffer.size());
    buffer.resize(csize + sizeof(uint32_t) + size);
    std::memcpy(buffer.data() + csize, &size, sizeof(uint32_t));
    std::memcpy(buffer.data() + csize + sizeof(uint32_t), &value, size);
  };

  void set(const void *value, uint32_t size) {
    uint32_t csize = static_cast<uint32_t>(buffer.size());
    buffer.resize(csize + sizeof(uint32_t) + size);
    std::memcpy(buffer.data() + csize, &size, sizeof(uint32_t));
    std::memcpy(buffer.data() + csize + sizeof(uint32_t), value, size);
  };

  static std::vector<uint8_t> get(uint32_t index,
                                  std::vector<uint8_t> &buffer) {
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

    return result;
  };
};
