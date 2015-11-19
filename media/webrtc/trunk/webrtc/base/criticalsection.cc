/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/criticalsection.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/thread.h"

namespace rtc {

void GlobalLockPod::Lock() {
  while (AtomicOps::CompareAndSwap(&lock_acquired, 0, 1)) {
    Thread::SleepMs(0);
  }
}

void GlobalLockPod::Unlock() {
  int old_value = AtomicOps::CompareAndSwap(&lock_acquired, 1, 0);
  DCHECK_EQ(1, old_value) << "Unlock called without calling Lock first";
}

GlobalLock::GlobalLock() {
  lock_acquired = 0;
}

}  // namespace rtc
