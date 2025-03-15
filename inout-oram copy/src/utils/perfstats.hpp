#pragma once

#include <atomic>

#include "ext/tinyformat.hpp"

#include "stopwatch.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#endif

struct TimingsField {
  std::atomic<uint64_t> time_ns = 0;
  std::atomic<uint64_t> count = 0;

  void reset() {
    time_ns = 0;
    count = 0;
  }

  inline double time_s() const { return time_ns / 1e9; }
  inline double fraction(double total_s) const { return time_s() / total_s; }

  inline std::string summary(double total_time) const {
    double pct = fraction(total_time) * 100;
    double mean_time = time_s() / count;
    if (count == 0) {
      mean_time = 0; // avoid nan
    }
    // return tfm::format("%.6f s (%05.2f %%) (n=%d)", time_s(), pct, count);
    return tfm::format("total=%.6fs u=%.6fms (%05.2f %%) (n=%d)", time_s(), mean_time * 1e3, pct, count);
  }
};

#define TIMINGS_SAMPLE(PERF, SW, FIELD, START_TIME)                                                                    \
  PERF.FIELD.time_ns += SW.elapsed_ns() - START_TIME;                                                                  \
  PERF.FIELD.count++;

#define TIMINGS_SAMPLE_AT(PERF, FIELD, START_TIME, END_TIME)                                                           \
  PERF.FIELD.time_ns += END_TIME - START_TIME;                                                                         \
  PERF.FIELD.count++;

struct NumberField {
  std::atomic<uint64_t> total_value = 0;
  std::atomic<uint64_t> count = 0;

  inline double mean() const { return total_value / static_cast<double>(count); }

  inline std::string summary() const { return tfm::format("mean=%.6f (n=%d)", mean(), count); }
};

#define NUMBER_SAMPLE(PERF, FIELD, VALUE)                                                                              \
  PERF.FIELD.total_value += VALUE;                                                                                     \
  PERF.FIELD.count++;
