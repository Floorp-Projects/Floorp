/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_EVENT_H__
#define WEBRTC_BASE_EVENT_H__

#if defined(WEBRTC_WIN)
#include "webrtc/base/win32.h"  // NOLINT: consider this a system header.
#elif defined(WEBRTC_POSIX)
#include <pthread.h>
#else
#error "Must define either WEBRTC_WIN or WEBRTC_POSIX."
#endif

#include "webrtc/base/basictypes.h"
#include "webrtc/base/common.h"

namespace rtc {

class Event {
 public:
  Event(bool manual_reset, bool initially_signaled);
  ~Event();

  void Set();
  void Reset();
  bool Wait(int cms);

 private:
  bool is_manual_reset_;

#if defined(WEBRTC_WIN)
  bool is_initially_signaled_;
  HANDLE event_handle_;
#elif defined(WEBRTC_POSIX)
  bool event_status_;
  pthread_mutex_t event_mutex_;
  pthread_cond_t event_cond_;
#endif
};

}  // namespace rtc

#endif  // WEBRTC_BASE_EVENT_H__
