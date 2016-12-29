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

const uint32_t HALF = 0x80000000;

uint64_t TimeNanos() {
  int64_t ticks = 0;
#if defined(WEBRTC_MAC)
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) {
      RTC_DCHECK(false);
    }
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  ticks = mach_absolute_time() * timebase.numer / timebase.denom;
#elif defined(WEBRTC_POSIX)
  struct timespec ts;
  // TODO: Do we need to handle the case when CLOCK_MONOTONIC
  // is not supported?
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ticks = kNumNanosecsPerSec * static_cast<int64_t>(ts.tv_sec) +
          static_cast<int64_t>(ts.tv_nsec);
#elif defined(WEBRTC_WIN)
  static volatile LONG last_timegettime = 0;
  static volatile int64_t num_wrap_timegettime = 0;
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
#else
#error Unsupported platform.
#endif
  return ticks;
}

uint32_t Time() {
  return static_cast<uint32_t>(TimeNanos() / kNumNanosecsPerMillisec);
}

uint64_t TimeMicros() {
  return static_cast<uint64_t>(TimeNanos() / kNumNanosecsPerMicrosec);
}

#if defined(WEBRTC_WIN)
static const uint64_t kFileTimeToUnixTimeEpochOffset = 116444736000000000ULL;

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
  int64_t micros = (li.QuadPart - kFileTimeToUnixTimeEpochOffset) / 10;
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

uint32_t TimeAfter(int32_t elapsed) {
  RTC_DCHECK_GE(elapsed, 0);
  RTC_DCHECK_LT(static_cast<uint32_t>(elapsed), HALF);
  return Time() + elapsed;
}

bool TimeIsBetween(uint32_t earlier, uint32_t middle, uint32_t later) {
  if (earlier <= later) {
    return ((earlier <= middle) && (middle <= later));
  } else {
    return !((later < middle) && (middle < earlier));
  }
}

bool TimeIsLaterOrEqual(uint32_t earlier, uint32_t later) {
#if EFFICIENT_IMPLEMENTATION
  int32_t diff = later - earlier;
  return (diff >= 0 && static_cast<uint32_t>(diff) < HALF);
#else
  const bool later_or_equal = TimeIsBetween(earlier, later, earlier + HALF);
  return later_or_equal;
#endif
}

bool TimeIsLater(uint32_t earlier, uint32_t later) {
#if EFFICIENT_IMPLEMENTATION
  int32_t diff = later - earlier;
  return (diff > 0 && static_cast<uint32_t>(diff) < HALF);
#else
  const bool earlier_or_equal = TimeIsBetween(later, earlier, later + HALF);
  return !earlier_or_equal;
#endif
}

int32_t TimeDiff(uint32_t later, uint32_t earlier) {
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

int64_t TimestampWrapAroundHandler::Unwrap(uint32_t ts) {
  if (ts < last_ts_) {
    if (last_ts_ > 0xf0000000 && ts < 0x0fffffff) {
      ++num_wrap_;
    }
  }
  last_ts_ = ts;
  int64_t unwrapped_ts = ts + (num_wrap_ << 32);
  return unwrapped_ts;
}

int64_t TmToSeconds(const std::tm& tm) {
  static short int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static short int cumul_mdays[12] = {0,   31,  59,  90,  120, 151,
                                      181, 212, 243, 273, 304, 334};
  int year = tm.tm_year + 1900;
  int month = tm.tm_mon;
  int day = tm.tm_mday - 1;  // Make 0-based like the rest.
  int hour = tm.tm_hour;
  int min = tm.tm_min;
  int sec = tm.tm_sec;

  bool expiry_in_leap_year = (year % 4 == 0 &&
                              (year % 100 != 0 || year % 400 == 0));

  if (year < 1970)
    return -1;
  if (month < 0 || month > 11)
    return -1;
  if (day < 0 || day >= mdays[month] + (expiry_in_leap_year && month == 2 - 1))
    return -1;
  if (hour < 0 || hour > 23)
    return -1;
  if (min < 0 || min > 59)
    return -1;
  if (sec < 0 || sec > 59)
    return -1;

  day += cumul_mdays[month];

  // Add number of leap days between 1970 and the expiration year, inclusive.
  day += ((year / 4 - 1970 / 4) - (year / 100 - 1970 / 100) +
          (year / 400 - 1970 / 400));

  // We will have added one day too much above if expiration is during a leap
  // year, and expiration is in January or February.
  if (expiry_in_leap_year && month <= 2 - 1) // |month| is zero based.
    day -= 1;

  // Combine all variables into seconds from 1970-01-01 00:00 (except |month|
  // which was accumulated into |day| above).
  return (((static_cast<int64_t>
            (year - 1970) * 365 + day) * 24 + hour) * 60 + min) * 60 + sec;
}

} // namespace rtc
