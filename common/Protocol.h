#pragma once

#include <stdint.h>
#include <vector>

enum class Prototype : uint8_t { STREAM = 0, GENERAL = 1 };

struct Protocol {
  Prototype type;
  std::vector<uint8_t> buffer;
};
