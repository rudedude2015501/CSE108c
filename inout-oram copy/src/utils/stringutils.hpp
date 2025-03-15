#pragma once

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "ext/tinyformat.hpp"

inline std::string join(const std::vector<std::string>& list, const std::string& delimiter) {
  std::string result;
  for (size_t i = 0; i < list.size(); i++) {
    if (i > 0) {
      result += delimiter;
    }
    result += list[i];
  }
  return result;
}

inline std::string format_hex(const uint8_t* data, size_t size, bool spaced = false) {
  std::ostringstream result;
  for (size_t i = 0; i < size; i++) {
    result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    if (spaced && (i < size - 1)) {
      result << " ";
    }
  }
  return result.str();
}

inline std::string format_hex(const std::vector<uint8_t>& data) { return format_hex(data.data(), data.size()); }
