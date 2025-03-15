#pragma once

#include <random>
#include <stdint.h>

// based on: https://en.wikipedia.org/wiki/Xorshift#Example_implementation

struct xorshift64_state {
  uint64_t a;

  xorshift64_state(uint64_t seed) : a(seed) {}
};

__attribute__((always_inline)) static inline uint64_t xorshift64(struct xorshift64_state* state) {
  uint64_t x = state->a;
  x ^= x << 7;
  x ^= x >> 9;
  return state->a = x;
}

struct XorshiftRNG {
  xorshift64_state state;

  XorshiftRNG(uint64_t seed) : state(seed) {}

  XorshiftRNG() : state(0) {
    // seed using random device
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    reseed(dis(gen));
  }

  void reseed(uint64_t seed) { state.a = seed; }

  /** generate random bytes */
  void random_bytes(uint8_t* buf_ptr, size_t len) {
    size_t pos = 0;
    while (pos < len) {
      uint64_t r = xorshift64(&state);
      for (size_t i = 0; i < sizeof(r) && pos < len; i++) {
        uint8_t v = (r >> (i * 8)) & 0xff;
        buf_ptr[pos++] = v;
      }
    }
  }

  uint64_t random_u64() {
    uint64_t ret;
    random_bytes((uint8_t*) &ret, sizeof(ret));
    return ret;
  }

  __attribute__((always_inline)) uint64_t random_u64_fast() {
    return xorshift64(&state);
  }
};
