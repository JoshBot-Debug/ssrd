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
  static T get(uint32_t index, const std::vector<uint8_t> &buffer) {
    
  };
};
