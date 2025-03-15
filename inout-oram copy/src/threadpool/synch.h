#ifndef THREADPOOL_SYNCH_H
#define THREADPOOL_SYNCH_H

#define SYNCH_API extern "C"

#include <stdbool.h>

SYNCH_API void synch_thread_pause();

typedef struct spinlock {
  volatile bool locked = false;
} synch_spinlock_t;

SYNCH_API void synch_spinlock_init(synch_spinlock_t* lock);
SYNCH_API void synch_spinlock_lock(synch_spinlock_t* lock);
SYNCH_API bool synch_spinlock_trylock(synch_spinlock_t* lock);
SYNCH_API void synch_spinlock_unlock(synch_spinlock_t* lock);

typedef struct sema {
  volatile unsigned int value = 0;
} synch_sema_t;

SYNCH_API void synch_sema_init(synch_sema_t* sema, unsigned int initial_value);
SYNCH_API void synch_sema_up(synch_sema_t* sema);
SYNCH_API void synch_sema_down(synch_sema_t* sema);

typedef struct condvar {
  struct synch_cond_waiter* head = 0;
  struct synch_cond_waiter* tail = 0;
} synch_cond_t;

SYNCH_API void synch_cond_init(synch_cond_t* condvar);
SYNCH_API void synch_cond_wait(synch_cond_t* condvar, synch_spinlock_t* lock);
SYNCH_API void synch_cond_signal(synch_cond_t* condvar, synch_spinlock_t* lock);
SYNCH_API void synch_cond_broadcast(synch_cond_t* condvar, synch_spinlock_t* lock);

#endif /* distributed-sgx-sort/enclave/synch.h */
