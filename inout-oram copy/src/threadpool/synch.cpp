#include "synch.h"

#include <stddef.h>

#include "defs.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"

#define ADAPTIVE_TIMEOUT 10000

// https://stackoverflow.com/questions/70069855/is-there-a-yield-intrinsic-on-arm

#if defined(__i386__) || defined(__x86_64__)
// #define PAUSE() _mm_pause()
#define PAUSE() __asm__ volatile("pause")
#elif defined(__aarch64__)
// #define PAUSE() __yield()
#define PAUSE() __asm__ volatile("yield")
#else
#error "PAUSE() not defined for this architecture"
#endif

__attribute__((always_inline)) void synch_thread_pause() { PAUSE(); }

void synch_spinlock_init(synch_spinlock_t* lock) { lock->locked = false; }

void synch_spinlock_lock(synch_spinlock_t* lock) {
  while (lock->locked || __atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
    PAUSE();
  }
}

bool synch_spinlock_trylock(synch_spinlock_t* lock) {
  return !lock->locked && !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

void synch_spinlock_unlock(synch_spinlock_t* lock) { __atomic_clear(&lock->locked, __ATOMIC_RELEASE); }

void synch_sema_init(synch_sema_t* sema, unsigned int initial_value) { sema->value = initial_value; }

void synch_sema_up(synch_sema_t* sema) { __atomic_add_fetch(&sema->value, 1, __ATOMIC_ACQUIRE); }

void synch_sema_down(synch_sema_t* sema) {
  unsigned int val;
  size_t spin_count = 0;
  do {
    spin_count++;
    PAUSE();
    val = sema->value;
  } while (!val || !__atomic_compare_exchange_n(&sema->value, &val, val - 1, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)
  );
}

struct synch_cond_waiter {
  synch_sema_t sema;
  struct synch_cond_waiter* next;
};

void synch_cond_init(synch_cond_t* condvar) {
  condvar->head = NULL;
  condvar->tail = NULL;
}

void synch_cond_wait(synch_cond_t* condvar, synch_spinlock_t* lock) {
  struct synch_cond_waiter waiter;
  synch_sema_init(&waiter.sema, 0);
  waiter.next = NULL;
  if (condvar->tail) {
    condvar->tail->next = &waiter;
  } else {
    condvar->head = &waiter;
  }
  condvar->tail = &waiter;
  synch_spinlock_unlock(lock);
  synch_sema_down(&waiter.sema);
  synch_spinlock_lock(lock);
}

void synch_cond_signal(synch_cond_t* condvar, synch_spinlock_t* lock UNUSED) {
  if (condvar->head) {
    synch_sema_up(&condvar->head->sema);
    if (condvar->head == condvar->tail) {
      condvar->tail = NULL;
    }
    condvar->head = condvar->head->next;
  }
}

void synch_cond_broadcast(synch_cond_t* condvar, synch_spinlock_t* lock UNUSED) {
  while (condvar->head) {
    synch_sema_up(&condvar->head->sema);
    if (condvar->head == condvar->tail) {
      condvar->tail = NULL;
    }
    condvar->head = condvar->head->next;
  }
}

#pragma GCC diagnostic pop
