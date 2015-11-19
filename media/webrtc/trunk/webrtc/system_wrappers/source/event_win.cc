/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/event_win.h"

#include "Mmsystem.h"

namespace webrtc {

EventWindows::EventWindows()
    : event_(::CreateEvent(NULL,    // security attributes
                           FALSE,   // manual reset
                           FALSE,   // initial state
                           NULL)),  // name of event
#ifdef WIN32_USE_TIMER_QUEUES
    timerHandle_(NULL),
    pulse_(false)
#else
    timerID_(NULL)
#endif
{
}

EventWindows::~EventWindows() {
  StopTimer();
  CloseHandle(event_);
}

bool EventWindows::Set() {
  // Note: setting an event that is already set has no effect.
  return SetEvent(event_) == 1;
}

EventTypeWrapper EventWindows::Wait(unsigned long max_time) {
  unsigned long res = WaitForSingleObject(event_, max_time);
  switch (res) {
    case WAIT_OBJECT_0:
      return kEventSignaled;
    case WAIT_TIMEOUT:
      return kEventTimeout;
    default:
      return kEventError;
  }
}

#ifdef WIN32_USE_TIMER_QUEUES
// static
void CALLBACK EventWindows::TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
  EventWindows *eventwin = (EventWindows*) lpParam;
  if (eventwin->pulse_) {
    PulseEvent(eventwin->event_);
  } else {
    SetEvent(eventwin->event_);
  }
}
#endif

bool EventWindows::StartTimer(bool periodic, unsigned long time) {
#ifdef WIN32_USE_TIMER_QUEUES
  if (timerHandle_) {
    // Wait for running timer callbacks to finish
    DeleteTimerQueueTimer(NULL, timerHandle_, INVALID_HANDLE_VALUE);
    timerHandle_ = NULL;
  }
  pulse_ = periodic;
  if (!CreateTimerQueueTimer(&timerHandle_, NULL,
                             (WAITORTIMERCALLBACK) TimerRoutine,
                             (PVOID) this, time, periodic ? time : 0,
                             WT_EXECUTEINTIMERTHREAD)) {
    return false;
  }
  return true;
#else
  if (timerID_ != NULL) {
    timeKillEvent(timerID_);
    timerID_ = NULL;
  }

  if (periodic) {
    timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
                            TIME_PERIODIC | TIME_CALLBACK_EVENT_PULSE);
  } else {
    timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
                            TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
  }

  return timerID_ != NULL;
#endif
}

bool EventWindows::StopTimer() {
#ifdef WIN32_USE_TIMER_QUEUES
  if (timerHandle_) {
    // Wait for running timer callbacks to finish
    DeleteTimerQueueTimer(NULL, timerHandle_, INVALID_HANDLE_VALUE);
    timerHandle_ = NULL;
  }
#else
  if (timerID_ != NULL) {
    timeKillEvent(timerID_);
    timerID_ = NULL;
  }
#endif

  return true;
}

}  // namespace webrtc
