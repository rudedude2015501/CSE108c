#pragma once

#include <algorithm>
#include <vector>

#include "minlog.hpp"

#include "threadpool.h"
#include <pthread.h>

struct PThreadThreadpool {
  threadpool::threadpool_context_t threadpool_ctx;
  std::vector<pthread_t*> threads;
  size_t n_threads;
  size_t n_worker_threads;

  PThreadThreadpool(size_t thread_count) :
      n_threads(thread_count), n_worker_threads(std::max(0, (int) thread_count - 1)) {
    // calling this is ok with 0
    threadpool::threadpool_context_init(&threadpool_ctx, n_worker_threads);
    if (n_worker_threads > 0) {
      g_logger.inf(tfm::format("setting up threadpool: n_threads=%zu, n_worker_threads=%zu", n_threads, n_worker_threads));
      threads.resize(n_worker_threads);
      threadpool::threadpool_enter_arg_t enter_arg;
      enter_arg.ctx = &threadpool_ctx;

      for (size_t i = 0; i < n_worker_threads; i++) {
        pthread_t* thread = new pthread_t();
        threads[i] = thread;
        pthread_create(thread, nullptr, threadpool::thread_enter_pool, &enter_arg);
      }

      threadpool::thread_wait_all_online(&threadpool_ctx);
      g_logger.inf("threadpool ready");
    }
  }

  ~PThreadThreadpool() {
    if (n_worker_threads > 0) {
      g_logger.inf("cleaning up threadpool");
      threadpool::thread_release_all(&threadpool_ctx);
      for (size_t i = 0; i < n_worker_threads; i++) {
        pthread_t* thread = threads[i];
        pthread_join(*thread, nullptr);
        delete thread;
      }
    }
  }

  threadpool::threadpool_context_t* get_context() { return &threadpool_ctx; }
};
