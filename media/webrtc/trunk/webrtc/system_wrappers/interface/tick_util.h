/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// System independant wrapper for polling elapsed time in ms and us.
// The implementation works in the tick domain which can be mapped over to the
// time domain.
#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_TICK_UTIL_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_TICK_UTIL_H_

#if _WIN32
// Note: These includes must be in this order since mmsystem depends on windows.
#include <windows.h>
#include <mmsystem.h>
#elif WEBRTC_LINUX
#include <ctime>
#elif WEBRTC_MAC
#include <mach/mach_time.h>
#include <string.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#include "webrtc/typedefs.h"

namespace webrtc {

class TickInterval;

// Class representing the current time.
class TickTime {
 public:
  TickTime();
  explicit TickTime(WebRtc_Word64 ticks);

  // Current time in the tick domain.
  static TickTime Now();

  // Now in the time domain in ms.
  static WebRtc_Word64 MillisecondTimestamp();

  // Now in the time domain in us.
  static WebRtc_Word64 MicrosecondTimestamp();

  // Returns the number of ticks in the tick domain.
  WebRtc_Word64 Ticks() const;

  static WebRtc_Word64 MillisecondsToTicks(const WebRtc_Word64 ms);

  static WebRtc_Word64 TicksToMilliseconds(const WebRtc_Word64 ticks);

  // Returns a TickTime that is ticks later than the passed TickTime.
  friend TickTime operator+(const TickTime lhs, const WebRtc_Word64 ticks);
  TickTime& operator+=(const WebRtc_Word64& ticks);

  // Returns a TickInterval that is the difference in ticks beween rhs and lhs.
  friend TickInterval operator-(const TickTime& lhs, const TickTime& rhs);

  // Call to engage the fake clock. This is useful for tests since relying on
  // a real clock often makes the test flaky.
  static void UseFakeClock(WebRtc_Word64 start_millisecond);

  // Advance the fake clock. Must be called after UseFakeClock.
  static void AdvanceFakeClock(WebRtc_Word64 milliseconds);

 private:
  static WebRtc_Word64 QueryOsForTicks();

  static bool use_fake_clock_;
  static WebRtc_Word64 fake_ticks_;

  WebRtc_Word64 ticks_;
};

// Represents a time delta in ticks.
class TickInterval {
 public:
  TickInterval();

  WebRtc_Word64 Milliseconds() const;
  WebRtc_Word64 Microseconds() const;

  // Returns the sum of two TickIntervals as a TickInterval.
  friend TickInterval operator+(const TickInterval& lhs,
                                const TickInterval& rhs);
  TickInterval& operator+=(const TickInterval& rhs);

  // Returns a TickInterval corresponding to rhs - lhs.
  friend TickInterval operator-(const TickInterval& lhs,
                                const TickInterval& rhs);
  TickInterval& operator-=(const TickInterval& rhs);

  friend bool operator>(const TickInterval& lhs, const TickInterval& rhs);
  friend bool operator<=(const TickInterval& lhs, const TickInterval& rhs);
  friend bool operator<(const TickInterval& lhs, const TickInterval& rhs);
  friend bool operator>=(const TickInterval& lhs, const TickInterval& rhs);

 private:
  explicit TickInterval(WebRtc_Word64 interval);

  friend class TickTime;
  friend TickInterval operator-(const TickTime& lhs, const TickTime& rhs);

 private:
  WebRtc_Word64 interval_;
};

inline TickInterval operator+(const TickInterval& lhs,
                              const TickInterval& rhs) {
  return TickInterval(lhs.interval_ + rhs.interval_);
}

inline TickInterval operator-(const TickInterval& lhs,
                              const TickInterval& rhs) {
  return TickInterval(lhs.interval_ - rhs.interval_);
}

inline TickInterval operator-(const TickTime& lhs, const TickTime& rhs) {
  return TickInterval(lhs.ticks_ - rhs.ticks_);
}

inline TickTime operator+(const TickTime lhs, const WebRtc_Word64 ticks) {
  TickTime time = lhs;
  time.ticks_ += ticks;
  return time;
}

inline bool operator>(const TickInterval& lhs, const TickInterval& rhs) {
  return lhs.interval_ > rhs.interval_;
}

inline bool operator<=(const TickInterval& lhs, const TickInterval& rhs) {
  return lhs.interval_ <= rhs.interval_;
}

inline bool operator<(const TickInterval& lhs, const TickInterval& rhs) {
  return lhs.interval_ <= rhs.interval_;
}

inline bool operator>=(const TickInterval& lhs, const TickInterval& rhs) {
  return lhs.interval_ >= rhs.interval_;
}

inline TickTime::TickTime()
    : ticks_(0) {
}

inline TickTime::TickTime(WebRtc_Word64 ticks)
    : ticks_(ticks) {
}

inline TickTime TickTime::Now() {
  if (use_fake_clock_)
    return TickTime(fake_ticks_);
  else
    return TickTime(QueryOsForTicks());
}

inline WebRtc_Word64 TickTime::QueryOsForTicks() {
  TickTime result;
#if _WIN32
  // TODO(wu): Remove QueryPerformanceCounter implementation.
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  // QueryPerformanceCounter returns the value from the TSC which is
  // incremented at the CPU frequency. The algorithm used requires
  // the CPU frequency to be constant. Technology like speed stepping
  // which has variable CPU frequency will therefore yield unpredictable,
  // incorrect time estimations.
  LARGE_INTEGER qpcnt;
  QueryPerformanceCounter(&qpcnt);
  result.ticks_ = qpcnt.QuadPart;
#else
  static volatile LONG last_time_get_time = 0;
  static volatile WebRtc_Word64 num_wrap_time_get_time = 0;
  volatile LONG* last_time_get_time_ptr = &last_time_get_time;
  DWORD now = timeGetTime();
  // Atomically update the last gotten time
  DWORD old = InterlockedExchange(last_time_get_time_ptr, now);
  if (now < old) {
    // If now is earlier than old, there may have been a race between
    // threads.
    // 0x0fffffff ~3.1 days, the code will not take that long to execute
    // so it must have been a wrap around.
    if (old > 0xf0000000 && now < 0x0fffffff) {
      num_wrap_time_get_time++;
    }
  }
  result.ticks_ = now + (num_wrap_time_get_time << 32);
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
  struct timespec ts;
  // TODO(wu): Remove CLOCK_REALTIME implementation.
#ifdef WEBRTC_CLOCK_TYPE_REALTIME
  clock_gettime(CLOCK_REALTIME, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  result.ticks_ = 1000000000LL * static_cast<WebRtc_Word64>(ts.tv_sec) +
      static_cast<WebRtc_Word64>(ts.tv_nsec);
#elif defined(WEBRTC_MAC)
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    kern_return_t retval = mach_timebase_info(&timebase);
    if (retval != KERN_SUCCESS) {
      // TODO(wu): Implement CHECK similar to chrome for all the platforms.
      // Then replace this with a CHECK(retval == KERN_SUCCESS);
#ifndef WEBRTC_IOS
      asm("int3");
#else
      __builtin_trap();
#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_TICK_UTIL_H_
    }
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  result.ticks_ = mach_absolute_time() * timebase.numer / timebase.denom;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  result.ticks_ = 1000000LL * static_cast<WebRtc_Word64>(tv.tv_sec) +
      static_cast<WebRtc_Word64>(tv.tv_usec);
#endif
  return result.ticks_;
}

inline WebRtc_Word64 TickTime::MillisecondTimestamp() {
  WebRtc_Word64 ticks = TickTime::Now().Ticks();
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (ticks * 1000) / qpfreq.QuadPart;
#else
  return ticks;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  return ticks / 1000000LL;
#else
  return ticks / 1000LL;
#endif
}

inline WebRtc_Word64 TickTime::MicrosecondTimestamp() {
  WebRtc_Word64 ticks = TickTime::Now().Ticks();
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (ticks * 1000) / (qpfreq.QuadPart / 1000);
#else
  return ticks * 1000LL;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  return ticks / 1000LL;
#else
  return ticks;
#endif
}

inline WebRtc_Word64 TickTime::Ticks() const {
  return ticks_;
}

inline WebRtc_Word64 TickTime::MillisecondsToTicks(const WebRtc_Word64 ms) {
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (qpfreq.QuadPart * ms) / 1000;
#else
  return ms;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  return ms * 1000000LL;
#else
  return ms * 1000LL;
#endif
}

inline WebRtc_Word64 TickTime::TicksToMilliseconds(const WebRtc_Word64 ticks) {
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (ticks * 1000) / qpfreq.QuadPart;
#else
  return ticks;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  return ticks / 1000000LL;
#else
  return ticks / 1000LL;
#endif
}

inline TickTime& TickTime::operator+=(const WebRtc_Word64& ticks) {
  ticks_ += ticks;
  return *this;
}

inline TickInterval::TickInterval() : interval_(0) {
}

inline TickInterval::TickInterval(const WebRtc_Word64 interval)
  : interval_(interval) {
}

inline WebRtc_Word64 TickInterval::Milliseconds() const {
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (interval_ * 1000) / qpfreq.QuadPart;
#else
  // interval_ is in ms
  return interval_;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  // interval_ is in ns
  return interval_ / 1000000;
#else
  // interval_ is usecs
  return interval_ / 1000;
#endif
}

inline WebRtc_Word64 TickInterval::Microseconds() const {
#if _WIN32
#ifdef USE_QUERY_PERFORMANCE_COUNTER
  LARGE_INTEGER qpfreq;
  QueryPerformanceFrequency(&qpfreq);
  return (interval_ * 1000000) / qpfreq.QuadPart;
#else
  // interval_ is in ms
  return interval_ * 1000LL;
#endif
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  // interval_ is in ns
  return interval_ / 1000;
#else
  // interval_ is usecs
  return interval_;
#endif
}

inline TickInterval& TickInterval::operator+=(const TickInterval& rhs) {
  interval_ += rhs.interval_;
  return *this;
}

inline TickInterval& TickInterval::operator-=(const TickInterval& rhs) {
  interval_ -= rhs.interval_;
  return *this;
}

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_TICK_UTIL_H_
