#if !defined(THREADPOOL_THREADING_H)
#define THREADPOOL_THREADING_H

#define THREADPOOL_API extern "C"

#include "synch.h"

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
namespace threadpool {
#endif

struct thread_work;

typedef struct threadpool_context {
  size_t num_threads;
  size_t num_threads_working;
  size_t num_threads_online;
  synch_cond_t cond_all_threads_online;
  synch_spinlock_t ctx_lock_online;
  synch_spinlock_t lock_thread_work;
  struct thread_work* volatile work_head;
  struct thread_work* volatile work_tail;
  volatile bool work_done;
  synch_cond_t cond_all_threads_finished;
  synch_spinlock_t ctx_lock;
  size_t num_threads_waiting;

  // symmetric work
  thread_work* symmetric_tasks;      // pointer to symmetric tasks
  size_t symmetric_count;            // number of tasks in the symmetric batch
  synch_spinlock_t symmetric_lock;   // lock to protect symmetric task access
  bool symmetric_active;             // flag indicating if a symmetric batch is active
  size_t symmetric_completed; // counter for completed symmetric tasks
  synch_cond_t symmetric_cond;       // condition variable to signal symmetric batch completion
} threadpool_context_t;

enum thread_work_type {
  THREAD_WORK_SINGLE,
  THREAD_WORK_ITER,
  THREAD_WORK_SYMMETRIC,
};

typedef struct thread_work {
  threadpool_context_t* ctx;
  enum thread_work_type type;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnested-anon-types"
  union {
    struct {
      void (*func)(void* arg);
      void* arg;
    } single;

    struct {
      void (*func)(void* arg, size_t i);
      void* arg;
      size_t count;
      size_t num_remaining;
      size_t curr;
    } iter;

    struct {
      void (*func)(void* arg, size_t i);
      void* arg;
      size_t index;
    } symmetric;
  };
#pragma GCC diagnostic pop

  synch_sema_t done;
  struct thread_work* next;
} thread_work_t;

struct threadpool_enter_arg_t {
  threadpool_context_t* ctx;
  void* user_data;
};

extern size_t num_threads;
extern size_t num_threads_working;

THREADPOOL_API int threadpool_context_init(threadpool_context_t* ctx, size_t num_threads);
THREADPOOL_API void thread_work_push(threadpool_context_t* ctx, thread_work_t* work);
THREADPOOL_API int thread_work_push_symmetric(threadpool_context_t* ctx, thread_work_t* work_buffer, size_t count);
THREADPOOL_API void thread_wait(threadpool_context_t* ctx, thread_work_t* work);
THREADPOOL_API int thread_work_push_symmetric(threadpool_context_t* ctx, thread_work_t* work_buffer, size_t count);
THREADPOOL_API void thread_wait_symmetric(threadpool_context_t* ctx);
THREADPOOL_API void thread_start_work(threadpool_context_t* ctx);
THREADPOOL_API void thread_work_until_empty(threadpool_context_t* ctx);
THREADPOOL_API void thread_wait_for_others(threadpool_context_t* ctx);
THREADPOOL_API void thread_wait_all_online(threadpool_context_t* ctx);
THREADPOOL_API void thread_release_all(threadpool_context_t* ctx);
THREADPOOL_API void thread_unrelease_all(threadpool_context_t* ctx);
THREADPOOL_API void* thread_enter_pool(void* arg);

#if defined(__cplusplus)
} // namespace threadpool
#endif

#endif
