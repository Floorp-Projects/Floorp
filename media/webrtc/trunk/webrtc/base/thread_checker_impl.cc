/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_impl.cc.

#include "webrtc/base/thread_checker_impl.h"

#include "webrtc/base/thread.h"

namespace rtc {

ThreadCheckerImpl::ThreadCheckerImpl()
    : valid_thread_() {
  EnsureThreadIdAssigned();
}

ThreadCheckerImpl::~ThreadCheckerImpl() {
}

bool ThreadCheckerImpl::CalledOnValidThread() const {
  CritScope scoped_lock(&lock_);
  EnsureThreadIdAssigned();
  return valid_thread_->IsCurrent();
}

void ThreadCheckerImpl::DetachFromThread() {
  CritScope scoped_lock(&lock_);
  valid_thread_ = NULL;
}

void ThreadCheckerImpl::EnsureThreadIdAssigned() const {
  if (!valid_thread_) {
    valid_thread_ = Thread::Current();
  }
}

}  // namespace rtc
