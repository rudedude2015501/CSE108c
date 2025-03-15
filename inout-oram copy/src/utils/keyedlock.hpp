#pragma once

#include <atomic>
#include <unordered_map>
#include <vector>

#include "synch.h"
#include <pthread.h>

#include "ints.hpp"
#include "minlog.hpp"

template <typename TKey> struct KeyedLock {
  std::unordered_map<TKey, bool> _lock_map;
  pthread_cond_t _unlock_evt;
  pthread_mutex_t _lock_mut;
  bool _noremove;

  KeyedLock(bool noremove = false) : _noremove(noremove) {
    pthread_cond_init(&_unlock_evt, nullptr);
    pthread_mutex_init(&_lock_mut, nullptr);
  }

  ~KeyedLock() {
    pthread_cond_destroy(&_unlock_evt);
    pthread_mutex_destroy(&_lock_mut);
  }

  bool is_locked(TKey key) { return _lock_map[key]; }

  void acquire_wait(TKey key) {
    pthread_mutex_lock(&_lock_mut);
    while (is_locked(key)) {
      pthread_cond_wait(&_unlock_evt, &_lock_mut);
    }
    _lock_map[key] = true;
    pthread_mutex_unlock(&_lock_mut);
  }

  void acquire_expect(TKey key) {
    pthread_mutex_lock(&_lock_mut);
    ENSURE(!is_locked(key), tfm::format("KeyedLock: key %d is already locked", key));
    _lock_map[key] = true;
    pthread_mutex_unlock(&_lock_mut);
  }

  void release(TKey key) {
    pthread_mutex_lock(&_lock_mut);
    ENSURE(_lock_map[key], tfm::format("KeyedLock: key %d is not locked", key));
    if (_noremove) {
      _lock_map[key] = false;
    } else {
      _lock_map.erase(key);
    }
    pthread_cond_signal(&_unlock_evt);
    pthread_mutex_unlock(&_lock_mut);
  }
};

template <typename TKey> struct KeyedLockSpinlock {
  std::unordered_map<TKey, bool> _lock_map;
  synch_spinlock_t _lock_spinlock;
  bool _noremove;

  KeyedLockSpinlock(bool noremove = false) : _noremove(noremove) { synch_spinlock_init(&_lock_spinlock); }

  bool is_locked(TKey key) { return _lock_map[key]; }

  size_t acquire_wait(TKey key) {
    size_t waits = 0;
    while (true) {
      synch_spinlock_lock(&_lock_spinlock);
      if (!is_locked(key)) {
        _lock_map[key] = true;
        synch_spinlock_unlock(&_lock_spinlock);
        return waits;
      }
      synch_spinlock_unlock(&_lock_spinlock);
      waits++;
    }
  }

  void acquire_expect(TKey key) {
    synch_spinlock_lock(&_lock_spinlock);
    ENSURE(!is_locked(key), tfm::format("KeyedLockSpinlock: key %d is already locked", key));
    _lock_map[key] = true;
    synch_spinlock_unlock(&_lock_spinlock);
  }

  void release(TKey key) {
    synch_spinlock_lock(&_lock_spinlock);
    ENSURE(_lock_map[key], tfm::format("KeyedLockSpinlock: key %d is not locked", key));
    if (_noremove) {
      _lock_map[key] = false;
    } else {
      _lock_map.erase(key);
    }
    synch_spinlock_unlock(&_lock_spinlock);
  }
};
