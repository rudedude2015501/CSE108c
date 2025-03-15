#pragma once

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <new>
#include <stdexcept>
#include <vector>

#include "threadpool.h"

namespace threadpool {
namespace wrapper {

// template <typename Func> struct alignas(64) ThreadData {
template <typename Func> struct ThreadData {
  Func func;
  size_t thread_index;

  ThreadData() : thread_index(0) {}
  ThreadData(const Func& f, size_t ti) : func(f), thread_index(ti) {}
};

template <typename Func, typename TThreadData> class BaseThreadpool {
protected:
  size_t thread_count;
  threadpool_context* threadpool_ctx;
  std::vector<thread_work> work_items;
  std::vector<TThreadData> thread_data;

  BaseThreadpool(threadpool_context* ctx, size_t requested_threads) :
      thread_count(requested_threads),
      threadpool_ctx(ctx),
      work_items(requested_threads - 1),
      thread_data(requested_threads - 1) {
    if (requested_threads - 1 != threadpool_ctx->num_threads) {
      fprintf(stderr, "requested_threads=%zu, num_threads=%zu\n", requested_threads, threadpool_ctx->num_threads);
      throw std::runtime_error("requested_threads-1 != threadpool_ctx->num_threads");
    }
  }

  __attribute__((always_inline)) void wait_for_threads() {
    for (auto& work : work_items) {
      thread_wait(threadpool_ctx, &work);
    }
  }

public:
  std::pair<size_t, size_t> get_thread_range(size_t thread_index, size_t n_tasks) const {
    size_t start = (thread_index * n_tasks) / thread_count;
    size_t end = ((thread_index + 1) * n_tasks) / thread_count;
    return std::make_pair(start, end);
  }
};

template <typename Func> class ParallelWorker final : public BaseThreadpool<Func, ThreadData<Func>> {
public:
  ParallelWorker(threadpool_context* ctx, size_t requested_threads) :
      BaseThreadpool<Func, ThreadData<Func>>(ctx, requested_threads) {}

  void parallel_work(const Func& func) {
    // Distribute work to worker threads (indices 1 to thread_count)
    for (size_t i = 1; i < this->thread_count; ++i) {
      ThreadData<Func>& data = this->thread_data[i - 1];
      thread_work& work = this->work_items[i - 1];
      data.func = func;
      data.thread_index = i;
      work.type = THREAD_WORK_SINGLE;
      work.single.func = [](void* arg) {
        ThreadData<Func>* data = static_cast<ThreadData<Func>*>(arg);
        data->func(data->thread_index);
      };
      work.single.arg = &data;

      thread_work_push(this->threadpool_ctx, &work);
    }

    // Main thread work (index 0)
    func(0);

    // Wait for worker threads to complete
    this->wait_for_threads();
  }
};

using DefaultParallelWorker = ParallelWorker<std::function<void(size_t)>>;

} // namespace wrapper
} // namespace threadpool
