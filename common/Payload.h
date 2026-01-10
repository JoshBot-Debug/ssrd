#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <stdint.h>
#include <string>
#include <vector>

inline static uint64_t htonll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return ((uint64_t)htonl(value & 0xFFFFFFFFULL) << 32) | htonl(value >> 32);
#else
  return value;
#endif
}

inline static uint64_t ntohll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return ((uint64_t)ntohl(value & 0xFFFFFFFFULL) << 32) | ntohl(value >> 32);
#else
  return value;
#endif
}

struct Payload {
  std::vector<uint8_t> buffer = {};

  void set(const std::string &value) {
    set(value.c_str(), static_cast<uint32_t>(value.size()));
  };

  void set(uint32_t value) {
    value = htonl(value);
    set(&value, sizeof(uint32_t));
  };

  void set(uint64_t value) {
    value = htonll(value);
    set(&value, sizeof(uint64_t));
  };

  void set(int value) { set(static_cast<uint32_t>(value)); };

  void set(double value) {
    uint64_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    uint64_t bytes = htonll(raw);
    set(&bytes, sizeof(uint64_t));
  };

  void set(const void *value, uint32_t size) {
    uint32_t csize = static_cast<uint32_t>(buffer.size());
    buffer.resize(csize + sizeof(uint32_t) + size);
    std::memcpy(buffer.data() + csize, &size, sizeof(uint32_t));
    std::memcpy(buffer.data() + csize + sizeof(uint32_t), value, size);
  };

  static std::vector<uint8_t> get(uint32_t index, const std::vector<uint8_t> &buffer) {
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

  static double toDouble(const std::vector<uint8_t> &buffer) {
    uint64_t net;
    std::memcpy(&net, buffer.data(), sizeof(net));

    uint64_t host = ntohll(net);
    double value;
    std::memcpy(&value, &host, sizeof(value));
    return value;
  };

  static int toInt(const std::vector<uint8_t> &buffer) {
    uint32_t net;
    std::memcpy(&net, buffer.data(), sizeof(uint32_t));
    return static_cast<int>(ntohl(net));
  };

  static uint32_t toUInt32(const std::vector<uint8_t> &buffer) {
    uint32_t net;
    std::memcpy(&net, buffer.data(), sizeof(uint32_t));
    return ntohl(net);
  };

  static uint64_t toUInt64(const std::vector<uint8_t> &buffer) {
    uint64_t net;
    std::memcpy(&net, buffer.data(), sizeof(uint64_t));
    return ntohll(net);
  };
};
