#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "ext/tinyformat.hpp"

static constexpr char humanize_bytes_units[][3] = {"", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};

inline std::string humanize_bytes(size_t n_bytes, size_t unit = 1024) {
  if (n_bytes == 0) {
    return "0 B";
  }

#if defined(OE_ENCLAVE)
  // this function is problematic on SGX because it doesn't like casting size_t to double for some reason?
  return tfm::format("%d B", n_bytes);
#endif

  auto exp = static_cast<uint32_t>(std::log(static_cast<double>(n_bytes)) / std::log(static_cast<double>(unit)));
  auto size = n_bytes / std::pow(unit, exp);
  return tfm::format("%.2f %sB", size, std::string(humanize_bytes_units[exp]));
}