#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(OE_ENCLAVE)
#include <openenclave/enclave.h>
#endif
#if defined(OE_HOST)
#include <openenclave/host.h>
#endif

static inline void oeu_assert_ok(oe_result_t result, const char* message) {
  if (result != OE_OK) {
    fprintf(stderr, "assertion failed: %s: result=%u (%s)\n", message, result, oe_result_str(result));
    abort();
  }
}
