#pragma once

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <new>
#include <stdexcept>
#include <vector>
#include <memory>

#include "threadpool.h"

namespace threadpool::worker {

// represents a background task that can be awaited
struct Task {
  thread_work work;                   // embedded work item
  size_t task_id;                     // unique identifier
  std::atomic<bool> completed{false}; // completion status, relaxed ordering ok
  std::atomic<bool> busy{false};      // thread busy status
  size_t thread_index;                // assigned thread index

  Task(size_t id) : task_id(id) {}
};

class BaseWorker {
protected:
  size_t _thread_count;
  size_t _thread_count_background;
  threadpool_context* _threadpool_ctx;

  BaseWorker(threadpool_context* ctx, size_t requested_threads) :
      _thread_count(requested_threads), _thread_count_background(requested_threads - 1), _threadpool_ctx(ctx) {
    if (_thread_count_background > _threadpool_ctx->num_threads) {
      fprintf(stderr, "requested_threads=%zu, num_threads=%zu\n", requested_threads, _threadpool_ctx->num_threads);
      throw std::runtime_error("requested_threads-1 > _threadpool_ctx->num_threads");
    }
  }

public:
  size_t thread_count() const { return _thread_count; }
};

template <typename Func> class ParallelWorker final : public BaseWorker {
protected:
  // thread data specific to parallel execution model
  struct alignas(64) ThreadState {
    Func func;
    size_t thread_index;

    ThreadState() : thread_index(0) {}
    ThreadState(const Func& f, size_t ti) : func(f), thread_index(ti) {}
  };

private:
  std::vector<thread_work> _work_items;
  std::vector<ThreadState> _thread_data;

  void wait_for_threads() {
    for (auto& work : _work_items) {
      thread_wait(this->_threadpool_ctx, &work);
    }
  }

public:
  ParallelWorker(threadpool_context* ctx) :
      BaseWorker(ctx, ctx->num_threads + 1),
      _work_items(this->_thread_count_background),
      _thread_data(this->_thread_count_background) {}

  ParallelWorker(threadpool_context* ctx, size_t requested_threads) :
      BaseWorker(ctx, requested_threads),
      _work_items(this->_thread_count_background),
      _thread_data(this->_thread_count_background) {}

  __attribute__((always_inline)) std::pair<size_t, size_t> get_thread_range(size_t thread_index, size_t n_tasks) const {
    size_t start = (thread_index * n_tasks) / this->_thread_count;
    size_t end = ((thread_index + 1) * n_tasks) / this->_thread_count;
    return std::make_pair(start, end);
  }

  __attribute__((always_inline)) void parallel_work(const Func& func) {
    // set up work items for background threads
    for (size_t i = 1; i < this->_thread_count; ++i) {
      ThreadState& state = this->_thread_data[i - 1];
      thread_work& work = this->_work_items[i - 1];
      state.func = func; // move when possible
      state.thread_index = i;
      work.type = THREAD_WORK_SINGLE;
      work.single.func = [](void* arg) {
        ThreadState* st = static_cast<ThreadState*>(arg);
        st->func(st->thread_index);
      };
      work.single.arg = &state;

      thread_work_push(this->_threadpool_ctx, &work);
    }

    // main thread work (index 0)
    func(0);

    // wait for background threads to finish
    this->wait_for_threads();
  }

  template <typename T>
  __attribute__((always_inline)) void parallel_process_data(
      std::vector<T>& items, const std::function<void(size_t, T&)>& process_func
  ) {
    const size_t total_items = items.size();
    const size_t num_threads = this->_thread_count;

    // capture values by value and func by ref
    auto work_func = [total_items, num_threads, &items, &process_func](size_t thread_index) {
      // compute range directly
      const size_t start = (thread_index * total_items) / num_threads;
      const size_t end = ((thread_index + 1) * total_items) / num_threads;

      // process items in this thread's range
      for (size_t i = start; i < end; ++i) {
        process_func(thread_index, items[i]);
      }
    };

    this->parallel_work(work_func);
  }
};

template <typename Func> class SymmetricParallelWorker final : public BaseWorker {
protected:
  struct alignas(64) ThreadState {
    Func func;
    size_t thread_index;
    ThreadState() : thread_index(0) {}
    ThreadState(const Func& f, size_t ti) : func(f), thread_index(ti) {}
  };

private:
  // preallocated work items for symmetric tasks
  std::vector<thread_work_t> _work_items;

  // thread-specific data
  std::vector<ThreadState> _thread_data;

  // wait for all symmetric tasks to complete
  void wait_for_threads() {
    for (auto& work : _work_items) {
      thread_wait(this->_threadpool_ctx, &work);
    }
  }

public:
  SymmetricParallelWorker(threadpool_context* ctx) :
      BaseWorker(ctx, ctx->num_threads + 1),
      _work_items(this->_thread_count_background),
      _thread_data(this->_thread_count_background) {}

  SymmetricParallelWorker(threadpool_context* ctx, size_t requested_threads) :
      BaseWorker(ctx, requested_threads),
      _work_items(this->_thread_count_background),
      _thread_data(this->_thread_count_background) {}

  // calculate thread range (unchanged)
  __attribute__((always_inline)) std::pair<size_t, size_t> get_thread_range(size_t thread_index, size_t n_tasks) const {
    size_t start = (thread_index * n_tasks) / this->_thread_count;
    size_t end = ((thread_index + 1) * n_tasks) / this->_thread_count;
    return std::make_pair(start, end);
  }

  // enqueue symmetric work to guarantee 1:1 mapping
  __attribute__((always_inline)) void symmetric_parallel_work(const Func& func) {
    // setup symmetric work items for background threads
    for (size_t i = 0; i < this->_thread_count_background; ++i) {
      ThreadState& state = this->_thread_data[i];
      thread_work_t& work = this->_work_items[i];
      // initialize thread-specific state
      state.func = func;
      state.thread_index = i;
      // configure symmetric work item
      work.ctx = this->_threadpool_ctx; // ensure context is set
      work.type = THREAD_WORK_SYMMETRIC;
      work.symmetric.index = i;
      work.symmetric.arg = &state;
      work.symmetric.func = [](void* arg, size_t work_i) {
        ThreadState* st = static_cast<ThreadState*>(arg);
        st->func(work_i);
      };
    }

    if (_thread_count_background > 0) {
      // enqueue symmetric work items using the preallocated buffer
      if (thread_work_push_symmetric(this->_threadpool_ctx, _work_items.data(), this->_thread_count_background) != 0) {
        fprintf(stderr, "error: thread_work_push_symmetric\n");
        abort();
      }
    }

    // execute work in the main thread
    // main thread gets the last index, to not overlap with background threads
    func(this->_thread_count_background);

    if (_thread_count_background > 0) {
      // wait for symmetric work completion
      thread_wait_symmetric(this->_threadpool_ctx);
    }
  }
};

template <typename Func> class BackgroundWorker final : public BaseWorker {
private:
  // holds the function and thread index for each worker thread
  struct ThreadData {
    Func func;
    size_t thread_index;
    ThreadData() : thread_index(0) {}
  };

  // connects tasks with their worker context
  struct TaskData {
    Task* task;
    BackgroundWorker* worker;
    TaskData() : task(nullptr), worker(nullptr) {}
  };

  // pool management
  alignas(64) std::atomic<size_t> _next_task_id{0};
  size_t _pool_size;                             // total number of available slots
  std::vector<std::unique_ptr<Task>> _task_pool; // owned tasks
  std::vector<TaskData> _task_data_pool;         // corresponding task data
  std::vector<bool> _task_slot_used;             // tracks which slots are in use

  // thread management
  std::vector<std::atomic<bool>> _thread_busy; // tracks which threads are working
  std::vector<ThreadData> _thread_data;        // per-thread function storage

  synch_spinlock_t _task_lock;

  // finds an unused slot in the task pool
  size_t find_available_task_slot() {
    synch_spinlock_lock(&_task_lock);
    for (size_t i = 0; i < _pool_size; ++i) {
      if (!_task_slot_used[i]) {
        _task_slot_used[i] = true;
        synch_spinlock_unlock(&_task_lock);
        return i;
      }
    }
    synch_spinlock_unlock(&_task_lock);
    return size_t(-1);
  }

public:
  // handle for referring to tasks, includes validation info
  struct TaskHandle {
    size_t index;
    size_t id;
    bool valid;

    TaskHandle() : index(-1), id(0), valid(false) {}
    TaskHandle(size_t idx, size_t task_id) : index(idx), id(task_id), valid(true) {}
  };

  // creates a worker with the specified number of threads and optional pool size
  BackgroundWorker(threadpool_context* ctx, size_t requested_threads, size_t pool_size = 0) :
      BaseWorker(ctx, requested_threads + 1),
      _pool_size(pool_size == 0 ? this->_thread_count_background * 2 : pool_size),
      _thread_busy(this->_thread_count_background),
      _thread_data(this->_thread_count_background) {

    // pre-allocate all our storage
    _task_pool.reserve(_pool_size);
    for (size_t i = 0; i < _pool_size; ++i) {
      _task_pool.push_back(std::make_unique<Task>(_next_task_id++));
    }

    _task_data_pool.resize(_pool_size);
    _task_slot_used.resize(_pool_size, false);

    synch_spinlock_init(&_task_lock);
  }

  __attribute__((always_inline)) TaskHandle queue_task(const Func& func) {
    // find an available thread
    size_t thread_idx = size_t(-1);
    for (size_t i = 0; i < this->_thread_count_background; ++i) {
      bool expected = false;
      if (_thread_busy[i].compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        thread_idx = i;
        break;
      }
    }
    if (thread_idx == size_t(-1)) {
      fprintf(stderr, "no available threads for task\n");
      return TaskHandle();
    }

    // get a slot from the task pool
    size_t task_idx = find_available_task_slot();
    if (task_idx == size_t(-1)) {
      _thread_busy[thread_idx].store(false, std::memory_order_relaxed);
      fprintf(stderr, "task pool exhausted\n");
      return TaskHandle();
    }

    // setup task and task data
    Task* task = _task_pool[task_idx].get();
    TaskData& task_data = _task_data_pool[task_idx];

    task->thread_index = thread_idx;
    task->completed.store(false, std::memory_order_relaxed);

    task_data.task = task;
    task_data.worker = this;

    // store function for the thread
    _thread_data[thread_idx].func = func; // move when possible
    _thread_data[thread_idx].thread_index = thread_idx;

    // setup the work item
    task->work.type = THREAD_WORK_SINGLE;
    task->work.single.func = [](void* arg) {
      TaskData* td = static_cast<TaskData*>(arg);
      auto* worker = td->worker;
      auto* task = td->task;
      worker->_thread_data[task->thread_index].func();
      worker->_thread_busy[task->thread_index].store(false, std::memory_order_relaxed);
    };
    task->work.single.arg = &task_data;

    // submit the work
    thread_work_push(this->_threadpool_ctx, &task->work);
    return TaskHandle(task_idx, task->task_id);
  }

  // checks if a task has completed
  __attribute__((always_inline)) bool is_completed(const TaskHandle& handle) const {
    if (!handle.valid || handle.index >= _pool_size || !_task_slot_used[handle.index]) {
      return true; // invalid tasks are considered completed
    }
    const Task* task = _task_pool[handle.index].get();
    if (task->task_id != handle.id) {
      return true; // task slot has been reused
    }
    return task->completed.load(std::memory_order_relaxed);
  }

  // waits for a task to complete and releases its resources
  __attribute__((always_inline)) void wait_task(const TaskHandle& handle) {
    if (!handle.valid || handle.index >= _pool_size || !_task_slot_used[handle.index]) {
      fprintf(stderr, "invalid task handle\n");
      return;
    }

    Task* task = _task_pool[handle.index].get();
    if (task->task_id != handle.id) {
      fprintf(stderr, "task slot has been reused\n");
      return;
    }

    thread_wait(this->_threadpool_ctx, &task->work);
    task->completed.store(true, std::memory_order_relaxed);

    // return the slot to the pool
    synch_spinlock_lock(&_task_lock);
    _task_slot_used[handle.index] = false;
    synch_spinlock_unlock(&_task_lock);
  }

  ~BackgroundWorker() {
    // ensure all tasks complete before destruction
    for (size_t i = 0; i < _pool_size; ++i) {
      if (_task_slot_used[i]) {
        wait_task(TaskHandle(i, _task_pool[i].get()->task_id));
      }
    }
  }
};

template <typename T> class DynamicParallelWorker final : public BaseWorker {
  using TProcessFunc = std::function<void(size_t, T&)>;

private:
  // core state - cache line aligned
  alignas(64) std::atomic<size_t> _next_item{0};  // tracks next item to process
  alignas(64) std::atomic<size_t> _items_done{0}; // tracks completion progress

  // pre-allocated storage
  const size_t _max_batch_size;
  std::vector<T> _batch_items;          // storage for current batch
  std::vector<thread_work> _work_items; // thread work structures
  size_t _current_batch_size{0};        // size of current batch

  // wait for all background threads to complete
  void wait_for_threads() {
    for (auto& work : _work_items) {
      thread_wait(this->_threadpool_ctx, &work);
    }
  }

  // worker function that processes items until none remain
  static void process_items(void* arg) {
    auto* ctx = static_cast<WorkContext*>(arg);
    auto* scheduler = ctx->scheduler;

    while (true) {
      // atomically grab next item
      size_t item_index = scheduler->_next_item.fetch_add(1, std::memory_order_relaxed);

      // check if we're done
      if (item_index >= scheduler->_current_batch_size) {
        break;
      }

      // process the item
      ctx->process_func(ctx->thread_index, scheduler->_batch_items[item_index]);

      // increment completion counter
      scheduler->_items_done.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // context passed to worker threads
  struct WorkContext {
    DynamicParallelWorker* scheduler;
    TProcessFunc process_func;
    size_t thread_index;
  };

  std::vector<WorkContext> _work_contexts; // pre-allocated contexts

public:
  DynamicParallelWorker(threadpool_context* ctx, size_t max_batch_size) :
      BaseWorker(ctx, ctx->num_threads + 1),
      _max_batch_size(max_batch_size),
      _batch_items(max_batch_size),
      _work_items(this->_thread_count_background),
      _work_contexts(this->_thread_count_background) {

    // initialize work contexts
    for (size_t i = 0; i < this->_thread_count_background; ++i) {
      _work_contexts[i].scheduler = this;
    }
  }

private:
  // internal implementation that handles the common processing logic
  void process_batch_impl(size_t batch_size, TProcessFunc process_func) {
    // reset atomic counters
    _next_item.store(0, std::memory_order_relaxed);
    _items_done.store(0, std::memory_order_relaxed);

    // setup work contexts for background threads
    for (size_t i = 0; i < this->_thread_count_background; ++i) {
      _work_contexts[i].process_func = process_func;
      _work_contexts[i].thread_index = i + 1;
    }

    // launch background threads
    for (size_t i = 0; i < this->_thread_count_background; ++i) {
      thread_work& work = _work_items[i];
      work.type = THREAD_WORK_SINGLE;
      work.single.func = process_items;
      work.single.arg = &_work_contexts[i];
      thread_work_push(this->_threadpool_ctx, &work);
    }

    // create context for main thread
    WorkContext main_ctx{this, process_func, 0};

    // main thread participates in processing
    process_items(&main_ctx);

    // wait for all threads to complete
    wait_for_threads();
  }

public:
  // vector interface
  void process_batch(const std::vector<T>& items, TProcessFunc process_func) {
    if (items.size() > _max_batch_size) {
      fprintf(stderr, "batch size exceeds maximum\n");
      abort();
      return;
    }
    if (items.empty()) {
      return;
    }

    // copy items to internal buffer
    _batch_items = items;
    _current_batch_size = items.size();

    process_batch_impl(items.size(), process_func);
  }

  // pointer + length interface
  void process_batch(T* items, size_t length, TProcessFunc process_func) {
    if (length > _max_batch_size) {
      fprintf(stderr, "batch size exceeds maximum\n");
      abort();
      return;
    }
    if (length == 0 || items == nullptr) {
      return;
    }

    // copy items to internal buffer
    _batch_items.resize(length);
    std::copy(items, items + length, _batch_items.begin());
    _current_batch_size = length;

    process_batch_impl(length, process_func);
  }
};

using DefaultParallelWorker = ParallelWorker<std::function<void(size_t)>>;
using DefaultSymmetricParallelWorker = SymmetricParallelWorker<std::function<void(size_t)>>;
using DefaultBackgroundWorker = BackgroundWorker<std::function<void()>>;

} // namespace threadpool::worker
