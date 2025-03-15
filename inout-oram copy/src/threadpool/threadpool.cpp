#include "threadpool.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "synch.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"

#if defined(__cplusplus)
namespace threadpool {
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-empty-struct"
#pragma GCC diagnostic ignored "-Wnested-anon-types"
struct task {
  struct thread_work* work;
  union {
    struct {
    } single;
    struct {
      size_t i;
    } iter;
    struct {
    } symmetric;
  };
};
#pragma GCC diagnostic pop

int threadpool_context_init(threadpool_context_t* ctx, size_t num_threads) {
  ctx->num_threads = num_threads;
  __atomic_store_n(&ctx->num_threads_working, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&ctx->num_threads_online, 0, __ATOMIC_RELAXED);
  synch_cond_init(&ctx->cond_all_threads_online);
  synch_spinlock_init(&ctx->ctx_lock_online);
  synch_spinlock_init(&ctx->lock_thread_work);
  ctx->work_head = NULL;
  ctx->work_tail = NULL;
  ctx->work_done = false;
  synch_cond_init(&ctx->cond_all_threads_finished);
  synch_spinlock_init(&ctx->ctx_lock);
  __atomic_store_n(&ctx->num_threads_waiting, 0, __ATOMIC_RELAXED);

  // symmetric
  ctx->symmetric_tasks = NULL;
  ctx->symmetric_count = 0;
  synch_spinlock_init(&ctx->symmetric_lock);
  ctx->symmetric_active = false;
  __atomic_store_n(&ctx->symmetric_completed, 0, __ATOMIC_RELAXED);
  synch_cond_init(&ctx->symmetric_cond);

  return 0;
}

void thread_work_push(threadpool_context_t* ctx, struct thread_work* work) {
  synch_sema_init(&work->done, 0);

  switch (work->type) {
  case THREAD_WORK_SINGLE:
    // Do nothing extra
    break;
  case THREAD_WORK_ITER:
    if (!work->iter.count) {
      synch_sema_up(&work->done);
      return;
    }
    work->iter.curr = 0;
    work->iter.num_remaining = work->iter.count;
    break;
  case THREAD_WORK_SYMMETRIC:
    // No additional setup required
    break;
  }

  synch_spinlock_lock(&ctx->lock_thread_work);

  work->next = NULL;
  if (!ctx->work_tail) {
    /* Empty list. Set head and tail. */
    ctx->work_head = work;
    ctx->work_tail = work;
  } else {
    /* List has values. */
    ctx->work_tail->next = work;
    ctx->work_tail = work;
  }

  synch_spinlock_unlock(&ctx->lock_thread_work);
}

void thread_wait(threadpool_context_t* ctx, struct thread_work* work) { synch_sema_down(&work->done); }

int thread_work_push_symmetric(threadpool_context_t* ctx, thread_work_t* work_buffer, size_t count) {
  if (count != ctx->num_threads || !work_buffer) {
    return -1;
  }

  synch_spinlock_lock(&ctx->symmetric_lock);
  if (ctx->symmetric_active) {
    synch_spinlock_unlock(&ctx->symmetric_lock);
    return -1;
  }

  // Set up the symmetric work state
  ctx->symmetric_tasks = work_buffer;
  __atomic_store_n(&ctx->symmetric_completed, 0, __ATOMIC_RELAXED);

  // Make tasks visible to workers before setting active flag
  __atomic_thread_fence(__ATOMIC_RELEASE);

  // Activate symmetric work
  __atomic_store_n(&ctx->symmetric_active, true, __ATOMIC_RELEASE);

  synch_spinlock_unlock(&ctx->symmetric_lock);
  return 0;
}

void thread_wait_symmetric(threadpool_context_t* ctx) {
  synch_spinlock_lock(&ctx->symmetric_lock);
  while (ctx->symmetric_active) {
    synch_cond_wait(&ctx->symmetric_cond, &ctx->symmetric_lock);
  }
  synch_spinlock_unlock(&ctx->symmetric_lock);
}

static bool get_task(threadpool_context_t* ctx, struct task* task) {
  task->work = NULL;
  synch_spinlock_lock(&ctx->lock_thread_work);
  if (ctx->work_head) {
    thread_work_t* work = ctx->work_head;
    if (work->type == THREAD_WORK_SYMMETRIC) {
      // Skip symmetric tasks in the normal queue
      // They should not be queued here anyway
      ctx->work_head = work->next;
      if (!ctx->work_head) {
        ctx->work_tail = NULL;
      }
      synch_spinlock_unlock(&ctx->lock_thread_work);
      return false;
    }
    // Assign the work
    task->work = work;
    ctx->work_head = work->next;
    if (!ctx->work_head) {
      ctx->work_tail = NULL;
    }
  }
  synch_spinlock_unlock(&ctx->lock_thread_work);
  return task->work != NULL;
}

static void do_task(struct task* task) {
  switch (task->work->type) {
  case THREAD_WORK_SINGLE:
    task->work->single.func(task->work->single.arg);
    synch_sema_up(&task->work->done);
    break;
  case THREAD_WORK_ITER:
    task->work->iter.func(task->work->iter.arg, task->iter.i);
    if (!__atomic_sub_fetch(&task->work->iter.num_remaining, 1, __ATOMIC_RELEASE)) {
      synch_sema_up(&task->work->done);
    }
    break;
  case THREAD_WORK_SYMMETRIC:
    // Handled directly in thread_start_work
    // Should never reach here
    break;
  }
}

void thread_start_work(threadpool_context_t* ctx, size_t thread_i) {
  while (!ctx->work_done) {
    // Normal work
    struct task task;
    if (get_task(ctx, &task)) {
      do_task(&task);
    }

    // Check for symmetric work
    if (__atomic_load_n(&ctx->symmetric_active, __ATOMIC_ACQUIRE)) {
      synch_spinlock_lock(&ctx->symmetric_lock);
      thread_work_t* symmetric_task = ctx->symmetric_tasks ? &ctx->symmetric_tasks[thread_i] : NULL;
      synch_spinlock_unlock(&ctx->symmetric_lock);

      if (symmetric_task) {
        symmetric_task->symmetric.func(symmetric_task->symmetric.arg, thread_i);

        if (__atomic_fetch_add(&ctx->symmetric_completed, 1, __ATOMIC_SEQ_CST) == ctx->num_threads - 1) {
          // if this is the last completed symmetric task, reset symmetric state
          synch_spinlock_lock(&ctx->symmetric_lock);
          ctx->symmetric_active = false;
          ctx->symmetric_tasks = NULL;
          synch_cond_broadcast(&ctx->symmetric_cond, &ctx->symmetric_lock);
          synch_spinlock_unlock(&ctx->symmetric_lock);
        } else {
          // wait for other threads to finish symmetric task
          synch_spinlock_lock(&ctx->symmetric_lock);
          while (ctx->symmetric_active) {
            synch_cond_wait(&ctx->symmetric_cond, &ctx->symmetric_lock);
          }
          synch_spinlock_unlock(&ctx->symmetric_lock);
        }

        continue;
      }
    }
  }

  __atomic_fetch_sub(&ctx->num_threads_working, 1, __ATOMIC_SEQ_CST);
}

void thread_work_until_empty(threadpool_context_t* ctx) {
  __atomic_fetch_add(&ctx->num_threads_working, 1, __ATOMIC_ACQUIRE);

  struct task task;
  while (get_task(ctx, &task)) {
    do_task(&task);
  }

  __atomic_fetch_sub(&ctx->num_threads_working, 1, __ATOMIC_RELEASE);
}

void thread_wait_for_others(threadpool_context_t* ctx) {
  synch_spinlock_lock(&ctx->ctx_lock);
  __atomic_fetch_add(&ctx->num_threads_waiting, 1, __ATOMIC_RELAXED);

  if (__atomic_load_n(&ctx->num_threads_waiting, __ATOMIC_RELAXED) >= ctx->num_threads) {
    synch_cond_broadcast(&ctx->cond_all_threads_finished, &ctx->ctx_lock);
    __atomic_store_n(&ctx->num_threads_waiting, 0, __ATOMIC_RELAXED);
  } else {
    synch_cond_wait(&ctx->cond_all_threads_finished, &ctx->ctx_lock);
  }

  synch_spinlock_unlock(&ctx->ctx_lock);
}

void thread_wait_all_online(threadpool_context_t* ctx) {
  synch_spinlock_lock(&ctx->lock_thread_work);
  while (__atomic_load_n(&ctx->num_threads_online, __ATOMIC_ACQUIRE) < ctx->num_threads) {
    synch_cond_wait(&ctx->cond_all_threads_online, &ctx->lock_thread_work);
  }
  synch_spinlock_unlock(&ctx->lock_thread_work);
}

void thread_release_all(threadpool_context_t* ctx) { ctx->work_done = true; }

void thread_unrelease_all(threadpool_context_t* ctx) { ctx->work_done = false; }

static size_t thread_come_online(threadpool_context_t* ctx) {
  size_t thread_ix = __atomic_fetch_add(&ctx->num_threads_online, 1, __ATOMIC_ACQUIRE);
  // fprintf(stderr, "thread_come_online: thread_ix=%d\n", (int) thread_ix);

  synch_spinlock_lock(&ctx->ctx_lock_online);
  if (__atomic_load_n(&ctx->num_threads_online, __ATOMIC_ACQUIRE) >= ctx->num_threads) {
    synch_cond_broadcast(&ctx->cond_all_threads_online, &ctx->ctx_lock_online);
  }
  synch_spinlock_unlock(&ctx->ctx_lock_online);

  return thread_ix;
}

static void thread_go_offline(threadpool_context_t* ctx) {
  __atomic_fetch_sub(&ctx->num_threads_online, 1, __ATOMIC_RELEASE);
}

void* thread_enter_pool(void* arg) {
  threadpool_enter_arg_t* targ = (threadpool_enter_arg_t*) arg;
  threadpool_context_t* ctx = targ->ctx;

  size_t thread_ix = thread_come_online(ctx);

  // Ensure all threads are online before proceeding
  thread_wait_all_online(ctx);

  // Start processing work, passing the unique thread index
  thread_start_work(ctx, thread_ix);

  thread_go_offline(ctx);
  return NULL;
}

#if defined(__cplusplus)
} // namespace threadpool
#endif

#pragma GCC diagnostic pop
