#pragma once

#include <vector>

#include "ints.hpp"
#include "minlog.hpp"

template <typename T> struct StructPool {
  enum class StructPoolItemState : u8 { uninitialized, free, in_use };

  u64 capacity;
  u64 available;
  u64 owned;
  std::vector<T> pool;
  std::vector<StructPoolItemState> free_list;
  u64 next_free; // index of the next free item for quick acquisition

  // constructor: preallocate memory for the pool
  StructPool(u64 capacity) : capacity(capacity), available(0), owned(0), next_free(0) {
    // preallocate memory for the pool and free list
    pool.reserve(capacity);
    free_list.resize(capacity, StructPoolItemState::uninitialized);
  }

  /** insert a new item into the pool */
  void insert_new(T value) {
    if (owned >= capacity) {
      fail(tfm::format("StructPool::insert_new: pool is full: %s", typeid(T).name()));
    }
    pool.push_back(value);
    free_list[owned] = StructPoolItemState::free;
    // update the next_free index if this is the first free item
    if (available == 0) {
      next_free = owned;
    }
    owned++;
    available++;
  }

  /** acquire a free item from the pool */
  T* acquire_one() {
    if (available == 0) {
      fail(tfm::format("StructPool::acquire_one: pool is empty: %s", typeid(T).name()));
    }
    // use the next_free index for quick acquisition
    u64 index = next_free;
    free_list[index] = StructPoolItemState::in_use;
    available--;

    // find the next free item for future acquisitions
    if (available > 0) {
      do {
        next_free = (next_free + 1) % capacity;
      } while (free_list[next_free] != StructPoolItemState::free);
    }

    return &pool[index];
  }

  /** release an item back into the pool */
  void release_one(T* value) {
    if (value == nullptr) {
      fail(tfm::format("StructPool::release_one: invalid resource: %s", typeid(T).name()));
    }
    if (available >= capacity) {
      fail(tfm::format("StructPool::release_one: pool is full: %s", typeid(T).name()));
    }
    ENSURE(
        value >= pool.data() && value < pool.data() + capacity,
        "StructPool::release_one: resource did not come from this pool"
    );
    u64 pool_index = (reinterpret_cast<u8*>(value) - reinterpret_cast<u8*>(pool.data())) / sizeof(T);
    ENSURE(free_list[pool_index] == StructPoolItemState::in_use, "StructPool::release_one: resource is not in use");
    free_list[pool_index] = StructPoolItemState::free;
    // update next_free if this is the only free item
    if (available == 0) {
      next_free = pool_index;
    }
    available++;
  }

  /** clear all items in the pool. caller is responsible for freeing any memory */
  void clear() {
    available = 0;
    owned = 0;
    next_free = 0;
    for (u64 i = 0; i < capacity; i++) {
      free_list[i] = StructPoolItemState::uninitialized;
    }
    pool.clear();
  }

  T* unsafe_get(u64 index) { return &pool[index]; }
};

template <typename T> struct SinglePool {
  T raw;
  bool available;

  // constructor: initialize with a single instance
  SinglePool(T instance) : raw(instance), available(true) {}

  /** acquire the single resource */
  T* acquire() {
    if (!available) {
      fail(tfm::format("SinglePool::acquire: resource is already in use: %s", typeid(T).name()));
    }
    available = false;
    return &raw;
  }

  /** release the single resource */
  void release(T* ptr) {
    if (available) {
      fail(tfm::format("SinglePool::release: resource already released: %s", typeid(T).name()));
    }
    ENSURE(ptr == &raw, "SinglePool::release: invalid resource");
    available = true;
  }
};
