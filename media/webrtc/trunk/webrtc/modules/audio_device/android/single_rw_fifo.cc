/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/single_rw_fifo.h"

static int UpdatePos(int pos, int capacity) {
  return (pos + 1) % capacity;
}

namespace webrtc {

namespace subtle {

inline void MemoryBarrier() {
  __sync_synchronize();
}

}  // namespace subtle

SingleRwFifo::SingleRwFifo(int capacity)
    : capacity_(capacity),
      size_(0),
      read_pos_(0),
      write_pos_(0) {
  queue_.reset(new int8_t*[capacity_]);
}

SingleRwFifo::~SingleRwFifo() {
}

void SingleRwFifo::Push(int8_t* mem) {
  assert(mem);

  // Ensure that there is space for the new data in the FIFO.
  // Note there is only one writer meaning that the other thread is guaranteed
  // only to decrease the size.
  const int free_slots = capacity() - size();
  if (free_slots <= 0) {
    // Size can be queried outside of the Push function. The caller is assumed
    // to ensure that Push will be successful before calling it.
    assert(false);
    return;
  }
  queue_[write_pos_] = mem;
  // Memory barrier ensures that |size_| is updated after the size has changed.
  subtle::MemoryBarrier();
  ++size_;
  write_pos_ = UpdatePos(write_pos_, capacity());
}

int8_t* SingleRwFifo::Pop() {
  int8_t* ret_val = NULL;
  if (size() <= 0) {
    // Size can be queried outside of the Pop function. The caller is assumed
    // to ensure that Pop will be successfull before calling it.
    assert(false);
    return ret_val;
  }
  ret_val = queue_[read_pos_];
  // Memory barrier ensures that |size_| is updated after the size has changed.
  subtle::MemoryBarrier();
  --size_;
  read_pos_ = UpdatePos(read_pos_, capacity());
  return ret_val;
}

}  // namespace webrtc
