/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#if defined(WEBRTC_POSIX)
#include <sys/time.h>
#if defined(WEBRTC_MAC)
#include <mach/mach_time.h>
#endif
#endif

#if defined(WEBRTC_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"

#define EFFICIENT_IMPLEMENTATION 1

namespace rtc {

const uint32 HALF = 0x80000000;

uint64 TimeNanos() {
  int64 ticks = 0;
#if defined(WEBRTC_MAC)
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
      DCHECK(false);
    }
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  ticks = mach_absolute_time() * timebase.numer / timebase.denom;
#elif defined(WEBRTC_POSIX)
  struct timespec ts;
  // TODO: Do we need to handle the case when CLOCK_MONOTONIC
  // is not supported?
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ticks = kNumNanosecsPerSec * static_cast<int64>(ts.tv_sec) +
      static_cast<int64>(ts.tv_nsec);
#elif defined(WEBRTC_WIN)
  static volatile LONG last_timegettime = 0;
  static volatile int64 num_wrap_timegettime = 0;
  volatile LONG* last_timegettime_ptr = &last_timegettime;
  DWORD now = timeGetTime();
  // Atomically update the last gotten time
  DWORD old = InterlockedExchange(last_timegettime_ptr, now);
  if (now < old) {
    // If now is earlier than old, there may have been a race between
    // threads.
    // 0x0fffffff ~3.1 days, the code will not take that long to execute
    // so it must have been a wrap around.
    if (old > 0xf0000000 && now < 0x0fffffff) {
      num_wrap_timegettime++;
    }
  }
  ticks = now + (num_wrap_timegettime << 32);
  // TODO: Calculate with nanosecond precision.  Otherwise, we're just
  // wasting a multiply and divide when doing Time() on Windows.
  ticks = ticks * kNumNanosecsPerMillisec;
#endif
  return ticks;
}

uint32 Time() {
  return static_cast<uint32>(TimeNanos() / kNumNanosecsPerMillisec);
}

uint64 TimeMicros() {
  return static_cast<uint64>(TimeNanos() / kNumNanosecsPerMicrosec);
}

#if defined(WEBRTC_WIN)
static const uint64 kFileTimeToUnixTimeEpochOffset = 116444736000000000ULL;

struct timeval {
  long tv_sec, tv_usec;  // NOLINT
};

// Emulate POSIX gettimeofday().
// Based on breakpad/src/third_party/glog/src/utilities.cc
static int gettimeofday(struct timeval *tv, void *tz) {
  // FILETIME is measured in tens of microseconds since 1601-01-01 UTC.
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  LARGE_INTEGER li;
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;

  // Convert to seconds and microseconds since Unix time Epoch.
  int64 micros = (li.QuadPart - kFileTimeToUnixTimeEpochOffset) / 10;
  tv->tv_sec = static_cast<long>(micros / kNumMicrosecsPerSec);  // NOLINT
  tv->tv_usec = static_cast<long>(micros % kNumMicrosecsPerSec); // NOLINT

  return 0;
}

// Emulate POSIX gmtime_r().
static struct tm *gmtime_r(const time_t *timep, struct tm *result) {
  // On Windows, gmtime is thread safe.
  struct tm *tm = gmtime(timep);  // NOLINT
  if (tm == NULL) {
    return NULL;
  }
  *result = *tm;
  return result;
}
#endif  // WEBRTC_WIN

void CurrentTmTime(struct tm *tm, int *microseconds) {
  struct timeval timeval;
  if (gettimeofday(&timeval, NULL) < 0) {
    // Incredibly unlikely code path.
    timeval.tv_sec = timeval.tv_usec = 0;
  }
  time_t secs = timeval.tv_sec;
  gmtime_r(&secs, tm);
  *microseconds = timeval.tv_usec;
}

uint32 TimeAfter(int32 elapsed) {
  DCHECK_GE(elapsed, 0);
  DCHECK_LT(static_cast<uint32>(elapsed), HALF);
  return Time() + elapsed;
}

bool TimeIsBetween(uint32 earlier, uint32 middle, uint32 later) {
  if (earlier <= later) {
    return ((earlier <= middle) && (middle <= later));
  } else {
    return !((later < middle) && (middle < earlier));
  }
}

bool TimeIsLaterOrEqual(uint32 earlier, uint32 later) {
#if EFFICIENT_IMPLEMENTATION
  int32 diff = later - earlier;
  return (diff >= 0 && static_cast<uint32>(diff) < HALF);
#else
  const bool later_or_equal = TimeIsBetween(earlier, later, earlier + HALF);
  return later_or_equal;
#endif
}

bool TimeIsLater(uint32 earlier, uint32 later) {
#if EFFICIENT_IMPLEMENTATION
  int32 diff = later - earlier;
  return (diff > 0 && static_cast<uint32>(diff) < HALF);
#else
  const bool earlier_or_equal = TimeIsBetween(later, earlier, later + HALF);
  return !earlier_or_equal;
#endif
}

int32 TimeDiff(uint32 later, uint32 earlier) {
#if EFFICIENT_IMPLEMENTATION
  return later - earlier;
#else
  const bool later_or_equal = TimeIsBetween(earlier, later, earlier + HALF);
  if (later_or_equal) {
    if (earlier <= later) {
      return static_cast<long>(later - earlier);
    } else {
      return static_cast<long>(later + (UINT32_MAX - earlier) + 1);
    }
  } else {
    if (later <= earlier) {
      return -static_cast<long>(earlier - later);
    } else {
      return -static_cast<long>(earlier + (UINT32_MAX - later) + 1);
    }
  }
#endif
}

TimestampWrapAroundHandler::TimestampWrapAroundHandler()
    : last_ts_(0), num_wrap_(0) {}

int64 TimestampWrapAroundHandler::Unwrap(uint32 ts) {
  if (ts < last_ts_) {
    if (last_ts_ > 0xf0000000 && ts < 0x0fffffff) {
      ++num_wrap_;
    }
  }
  last_ts_ = ts;
  int64_t unwrapped_ts = ts + (num_wrap_ << 32);
  return unwrapped_ts;
}

} // namespace rtc
