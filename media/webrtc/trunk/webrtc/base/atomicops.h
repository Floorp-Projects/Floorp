/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ATOMICOPS_H_
#define WEBRTC_BASE_ATOMICOPS_H_

#include <string>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {

// A single-producer, single-consumer, fixed-size queue.
// All methods not ending in Unsafe can be safely called without locking,
// provided that calls to consumer methods (Peek/Pop) or producer methods (Push)
// only happen on a single thread per method type. If multiple threads need to
// read simultaneously or write simultaneously, other synchronization is
// necessary. Synchronization is also required if a call into any Unsafe method
// could happen at the same time as a call to any other method.
template <typename T>
class FixedSizeLockFreeQueue {
 private:
// Atomic primitives and memory barrier
#if defined(__arm__)
  typedef uint32 Atomic32;

  // Copied from google3/base/atomicops-internals-arm-v6plus.h
  static inline void MemoryBarrier() {
    asm volatile("dmb":::"memory");
  }

  // Adapted from google3/base/atomicops-internals-arm-v6plus.h
  static inline void AtomicIncrement(volatile Atomic32* ptr) {
    Atomic32 str_success, value;
    asm volatile (
        "1:\n"
        "ldrex  %1, [%2]\n"
        "add    %1, %1, #1\n"
        "strex  %0, %1, [%2]\n"
        "teq    %0, #0\n"
        "bne    1b"
        : "=&r"(str_success), "=&r"(value)
        : "r" (ptr)
        : "cc", "memory");
  }
#elif !defined(SKIP_ATOMIC_CHECK)
#error "No atomic operations defined for the given architecture."
#endif

 public:
  // Constructs an empty queue, with capacity 0.
  FixedSizeLockFreeQueue() : pushed_count_(0),
                             popped_count_(0),
                             capacity_(0),
                             data_() {}
  // Constructs an empty queue with the given capacity.
  FixedSizeLockFreeQueue(size_t capacity) : pushed_count_(0),
                                            popped_count_(0),
                                            capacity_(capacity),
                                            data_(new T[capacity]) {}

  // Pushes a value onto the queue. Returns true if the value was successfully
  // pushed (there was space in the queue). This method can be safely called at
  // the same time as PeekFront/PopFront.
  bool PushBack(T value) {
    if (capacity_ == 0) {
      LOG(LS_WARNING) << "Queue capacity is 0.";
      return false;
    }
    if (IsFull()) {
      return false;
    }

    data_[pushed_count_ % capacity_] = value;
    // Make sure the data is written before the count is incremented, so other
    // threads can't see the value exists before being able to read it.
    MemoryBarrier();
    AtomicIncrement(&pushed_count_);
    return true;
  }

  // Retrieves the oldest value pushed onto the queue. Returns true if there was
  // an item to peek (the queue was non-empty). This method can be safely called
  // at the same time as PushBack.
  bool PeekFront(T* value_out) {
    if (capacity_ == 0) {
      LOG(LS_WARNING) << "Queue capacity is 0.";
      return false;
    }
    if (IsEmpty()) {
      return false;
    }

    *value_out = data_[popped_count_ % capacity_];
    return true;
  }

  // Retrieves the oldest value pushed onto the queue and removes it from the
  // queue. Returns true if there was an item to pop (the queue was non-empty).
  // This method can be safely called at the same time as PushBack.
  bool PopFront(T* value_out) {
    if (PeekFront(value_out)) {
      AtomicIncrement(&popped_count_);
      return true;
    }
    return false;
  }

  // Clears the current items in the queue and sets the new (fixed) size. This
  // method cannot be called at the same time as any other method.
  void ClearAndResizeUnsafe(int new_capacity) {
    capacity_ = new_capacity;
    data_.reset(new T[new_capacity]);
    pushed_count_ = 0;
    popped_count_ = 0;
  }

  // Returns true if there is no space left in the queue for new elements.
  int IsFull() const { return pushed_count_ == popped_count_ + capacity_; }
  // Returns true if there are no elements in the queue.
  int IsEmpty() const { return pushed_count_ == popped_count_; }
  // Returns the current number of elements in the queue. This is always in the
  // range [0, capacity]
  size_t Size() const { return pushed_count_ - popped_count_; }

  // Returns the capacity of the queue (max size).
  size_t capacity() const { return capacity_; }

 private:
  volatile Atomic32 pushed_count_;
  volatile Atomic32 popped_count_;
  size_t capacity_;
  rtc::scoped_ptr<T[]> data_;
  DISALLOW_COPY_AND_ASSIGN(FixedSizeLockFreeQueue);
};

}

#endif  // WEBRTC_BASE_ATOMICOPS_H_
