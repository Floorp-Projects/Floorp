/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/timing.h"
#include "webrtc/base/timeutils.h"

#if defined(WEBRTC_POSIX)
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <mach/mach.h>
#include <mach/clock.h>
#endif
#elif defined(WEBRTC_WIN)
#include <sys/timeb.h>
#include "webrtc/base/win32.h"
#endif

namespace rtc {

Timing::Timing() {
#if defined(WEBRTC_WIN)
  // This may fail, but we handle failure gracefully in the methods
  // that use it (use alternative sleep method).
  //
  // TODO: Make it possible for user to tell if IdleWait will
  // be done at lesser resolution because of this.
  timer_handle_ = CreateWaitableTimer(NULL,     // Security attributes.
                                      FALSE,    // Manual reset?
                                      NULL);    // Timer name.
#endif
}

Timing::~Timing() {
#if defined(WEBRTC_WIN)
  if (timer_handle_ != NULL)
    CloseHandle(timer_handle_);
#endif
}

// static
double Timing::WallTimeNow() {
#if defined(WEBRTC_POSIX)
  struct timeval time;
  gettimeofday(&time, NULL);
  // Convert from second (1.0) and microsecond (1e-6).
  return (static_cast<double>(time.tv_sec) +
          static_cast<double>(time.tv_usec) * 1.0e-6);

#elif defined(WEBRTC_WIN)
  struct _timeb time;
  _ftime(&time);
  // Convert from second (1.0) and milliseconds (1e-3).
  return (static_cast<double>(time.time) +
          static_cast<double>(time.millitm) * 1.0e-3);
#endif
}

double Timing::TimerNow() {
  return (static_cast<double>(TimeNanos()) / kNumNanosecsPerSec);
}

double Timing::BusyWait(double period) {
  double start_time = TimerNow();
  while (TimerNow() - start_time < period) {
  }
  return TimerNow() - start_time;
}

double Timing::IdleWait(double period) {
  double start_time = TimerNow();

#if defined(WEBRTC_POSIX)
  double sec_int, sec_frac = modf(period, &sec_int);
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(sec_int);
  ts.tv_nsec = static_cast<long>(sec_frac * 1.0e9);  // NOLINT

  // NOTE(liulk): for the NOLINT above, long is the appropriate POSIX
  // type.

  // POSIX nanosleep may be interrupted by signals.
  while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
  }

#elif defined(WEBRTC_WIN)
  if (timer_handle_ != NULL) {
    LARGE_INTEGER due_time;

    // Negative indicates relative time.  The unit is 100 nanoseconds.
    due_time.QuadPart = -LONGLONG(period * 1.0e7);

    SetWaitableTimer(timer_handle_, &due_time, 0, NULL, NULL, TRUE);
    WaitForSingleObject(timer_handle_, INFINITE);
  } else {
    // Still attempts to sleep with lesser resolution.
    // The unit is in milliseconds.
    Sleep(DWORD(period * 1.0e3));
  }
#endif

  return TimerNow() - start_time;
}

}  // namespace rtc
