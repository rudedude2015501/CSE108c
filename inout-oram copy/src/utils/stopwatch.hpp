#pragma once

#include <ext/nanotimer.hpp>

struct Stopwatch {
  plf::nanotimer timer;

  void start() { timer.start(); }

  // void stop() {
  //   timer.stop();
  // }

  double elapsed_ns() { return timer.get_elapsed_ns(); }

  double elapsed_sec() {
    return timer.get_elapsed_ms() / 1000.0;
  }
};
