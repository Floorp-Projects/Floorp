/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_SINGLE_RW_FIFO_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_SINGLE_RW_FIFO_H_

#include "webrtc/system_wrappers/interface/atomic32.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Implements a lock-free FIFO losely based on
// http://src.chromium.org/viewvc/chrome/trunk/src/media/base/audio_fifo.cc
// Note that this class assumes there is one producer (writer) and one
// consumer (reader) thread.
class SingleRwFifo {
 public:
  explicit SingleRwFifo(int capacity);
  ~SingleRwFifo();

  void Push(int8_t* mem);
  int8_t* Pop();

  void Clear();

  int size() { return size_.Value(); }
  int capacity() const { return capacity_; }

 private:
  scoped_ptr<int8_t*[]> queue_;
  int capacity_;

  Atomic32 size_;

  int read_pos_;
  int write_pos_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_SINGLE_RW_FIFO_H_
