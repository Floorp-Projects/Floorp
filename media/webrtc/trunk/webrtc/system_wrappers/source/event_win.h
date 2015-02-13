/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_

#include <windows.h>

#include "webrtc/system_wrappers/interface/event_wrapper.h"

#include "webrtc/typedefs.h"

// Added by Mozilla since there are a maximum of 16 timerSetEvent() timers per process
#define WIN32_USE_TIMER_QUEUES

namespace webrtc {

class EventWindows : public EventWrapper {
 public:
  EventWindows();
  virtual ~EventWindows();

  virtual EventTypeWrapper Wait(unsigned long max_time);
  virtual bool Set();
  virtual bool Reset();

#ifdef WIN32_USE_TIMER_QUEUES
  static void CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);
#endif

  virtual bool StartTimer(bool periodic, unsigned long time);
  virtual bool StopTimer();

 private:
  HANDLE  event_;
#ifdef WIN32_USE_TIMER_QUEUES
  HANDLE  timerHandle_;
  bool pulse_;
#else
  uint32_t timerID_;
#endif
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_
