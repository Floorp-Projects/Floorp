/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimeStamp_h
#define mozilla_TimeStamp_h

#include <stdint.h>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "nscore.h"

namespace IPC {
template <typename T> struct ParamTraits;
}

#ifdef XP_WIN
// defines TimeStampValue as a complex value keeping both
// GetTickCount and QueryPerformanceCounter values
#include "TimeStamp_windows.h"
#endif

namespace mozilla {

#ifndef XP_WIN
typedef uint64_t TimeStampValue;
#endif

class TimeStamp;

/**
 * Instances of this class represent the length of an interval of time.
 * Negative durations are allowed, meaning the end is before the start.
 * 
 * Internally the duration is stored as a int64_t in units of
 * PR_TicksPerSecond() when building with NSPR interval timers, or a
 * system-dependent unit when building with system clocks.  The
 * system-dependent unit must be constant, otherwise the semantics of
 * this class would be broken.
 */
class TimeDuration
{
public:
  // The default duration is 0.
  MOZ_CONSTEXPR TimeDuration() : mValue(0) {}
  // Allow construction using '0' as the initial value, for readability,
  // but no other numbers (so we don't have any implicit unit conversions).
  struct _SomethingVeryRandomHere;
  TimeDuration(_SomethingVeryRandomHere* aZero) : mValue(0) {
    MOZ_ASSERT(!aZero, "Who's playing funny games here?");
  }
  // Default copy-constructor and assignment are OK

  double ToSeconds() const;
  // Return a duration value that includes digits of time we think to
  // be significant.  This method should be used when displaying a
  // time to humans.
  double ToSecondsSigDigits() const;
  double ToMilliseconds() const {
    return ToSeconds() * 1000.0;
  }
  double ToMicroseconds() const {
    return ToMilliseconds() * 1000.0;
  }

  // Using a double here is safe enough; with 53 bits we can represent
  // durations up to over 280,000 years exactly.  If the units of
  // mValue do not allow us to represent durations of that length,
  // long durations are clamped to the max/min representable value
  // instead of overflowing.
  static inline TimeDuration FromSeconds(double aSeconds) {
    return FromMilliseconds(aSeconds * 1000.0);
  }
  static TimeDuration FromMilliseconds(double aMilliseconds);
  static inline TimeDuration FromMicroseconds(double aMicroseconds) {
    return FromMilliseconds(aMicroseconds / 1000.0);
  }

  static TimeDuration Forever() {
    return FromTicks(INT64_MAX);
  }

  TimeDuration operator+(const TimeDuration& aOther) const {
    return TimeDuration::FromTicks(mValue + aOther.mValue);
  }
  TimeDuration operator-(const TimeDuration& aOther) const {
    return TimeDuration::FromTicks(mValue - aOther.mValue);
  }
  TimeDuration& operator+=(const TimeDuration& aOther) {
    mValue += aOther.mValue;
    return *this;
  }
  TimeDuration& operator-=(const TimeDuration& aOther) {
    mValue -= aOther.mValue;
    return *this;
  }

private:
  // Block double multiplier (slower, imprecise if long duration) - Bug 853398.
  // If required, use MultDouble explicitly and with care.
  TimeDuration operator*(const double aMultiplier) const MOZ_DELETE;

public:
  TimeDuration MultDouble(double aMultiplier) const {
    return TimeDuration::FromTicks(static_cast<int64_t>(mValue * aMultiplier));
  }
  TimeDuration operator*(const int32_t aMultiplier) const {
    return TimeDuration::FromTicks(mValue * int64_t(aMultiplier));
  }
  TimeDuration operator*(const uint32_t aMultiplier) const {
    return TimeDuration::FromTicks(mValue * int64_t(aMultiplier));
  }
  TimeDuration operator*(const int64_t aMultiplier) const {
    return TimeDuration::FromTicks(mValue * int64_t(aMultiplier));
  }
  TimeDuration operator/(const int64_t aDivisor) const {
    return TimeDuration::FromTicks(mValue / aDivisor);
  }
  double operator/(const TimeDuration& aOther) const {
    return static_cast<double>(mValue) / aOther.mValue;
  }

  bool operator<(const TimeDuration& aOther) const {
    return mValue < aOther.mValue;
  }
  bool operator<=(const TimeDuration& aOther) const {
    return mValue <= aOther.mValue;
  }
  bool operator>=(const TimeDuration& aOther) const {
    return mValue >= aOther.mValue;
  }
  bool operator>(const TimeDuration& aOther) const {
    return mValue > aOther.mValue;
  }
  bool operator==(const TimeDuration& aOther) const {
    return mValue == aOther.mValue;
  }

  // Return a best guess at the system's current timing resolution,
  // which might be variable.  TimeDurations below this order of
  // magnitude are meaningless, and those at the same order of
  // magnitude or just above are suspect.
  static TimeDuration Resolution();

  // We could define additional operators here:
  // -- convert to/from other time units
  // -- scale duration by a float
  // but let's do that on demand.
  // Comparing durations for equality will only lead to bugs on
  // platforms with high-resolution timers.

private:
  friend class TimeStamp;
  friend struct IPC::ParamTraits<mozilla::TimeDuration>;

  static TimeDuration FromTicks(int64_t aTicks) {
    TimeDuration t;
    t.mValue = aTicks;
    return t;
  }

  static TimeDuration FromTicks(double aTicks) {
    // NOTE: this MUST be a >= test, because int64_t(double(INT64_MAX))
    // overflows and gives INT64_MIN.
    if (aTicks >= double(INT64_MAX))
      return TimeDuration::FromTicks(INT64_MAX);

    // This MUST be a <= test.
    if (aTicks <= double(INT64_MIN))
      return TimeDuration::FromTicks(INT64_MIN);

    return TimeDuration::FromTicks(int64_t(aTicks));
  }

  // Duration, result is implementation-specific difference of two TimeStamps
  int64_t mValue;
};

/**
 * Instances of this class represent moments in time, or a special
 * "null" moment. We do not use the non-monotonic system clock or
 * local time, since they can be reset, causing apparent backward
 * travel in time, which can confuse algorithms. Instead we measure
 * elapsed time according to the system.  This time can never go
 * backwards (i.e. it never wraps around, at least not in less than
 * five million years of system elapsed time). It might not advance
 * while the system is sleeping. If TimeStamp::SetNow() is not called
 * at all for hours or days, we might not notice the passage of some
 * of that time.
 * 
 * We deliberately do not expose a way to convert TimeStamps to some
 * particular unit. All you can do is compute a difference between two
 * TimeStamps to get a TimeDuration. You can also add a TimeDuration
 * to a TimeStamp to get a new TimeStamp. You can't do something
 * meaningless like add two TimeStamps.
 *
 * Internally this is implemented as either a wrapper around
 *   - high-resolution, monotonic, system clocks if they exist on this
 *     platform
 *   - PRIntervalTime otherwise.  We detect wraparounds of
 *     PRIntervalTime and work around them.
 *
 * This class is similar to C++11's time_point, however it is
 * explicitly nullable and provides an IsNull() method. time_point
 * is initialized to the clock's epoch and provides a
 * time_since_epoch() method that functions similiarly. i.e.
 * t.IsNull() is equivalent to t.time_since_epoch() == decltype(t)::duration::zero();
 */
class TimeStamp
{
public:
  /**
   * Initialize to the "null" moment
   */
  MOZ_CONSTEXPR TimeStamp() : mValue(0) {}
  // Default copy-constructor and assignment are OK

  /**
   * Return true if this is the "null" moment
   */
  bool IsNull() const { return mValue == 0; }
  /**
   * Return a timestamp reflecting the current elapsed system time. This
   * is monotonically increasing (i.e., does not decrease) over the
   * lifetime of this process' XPCOM session.
   *
   * Now() is trying to ensure the best possible precision on each platform,
   * at least one millisecond.
   *
   * NowLoRes() has been introduced to workaround performance problems of
   * QueryPerformanceCounter on the Windows platform.  NowLoRes() is giving
   * lower precision, usually 15.6 ms, but with very good performance benefit.
   * Use it for measurements of longer times, like >200ms timeouts.
   */
  static TimeStamp Now() { return Now(true); }
  static TimeStamp NowLoRes() { return Now(false); }

  /**
   * Return a timestamp representing the time when the current process was
   * created which will be comparable with other timestamps taken with this
   * class. If the actual process creation time is detected to be inconsistent
   * the @a aIsInconsistent parameter will be set to true, the returned
   * timestamp however will still be valid though inaccurate.
   *
   * @param aIsInconsistent Set to true if an inconsistency was detected in the
   * process creation time
   * @returns A timestamp representing the time when the process was created,
   * this timestamp is always valid even when errors are reported
   */
  static TimeStamp ProcessCreation(bool& aIsInconsistent);

  /**
   * Records a process restart. After this call ProcessCreation() will return
   * the time when the browser was restarted instead of the actual time when
   * the process was created.
   */
  static void RecordProcessRestart();

  /**
   * Compute the difference between two timestamps. Both must be non-null.
   */
  TimeDuration operator-(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    static_assert(-INT64_MAX > INT64_MIN, "int64_t sanity check");
    int64_t ticks = int64_t(mValue - aOther.mValue);
    // Check for overflow.
    if (mValue > aOther.mValue) {
      if (ticks < 0) {
        ticks = INT64_MAX;
      }
    } else {
      if (ticks > 0) {
        ticks = INT64_MIN;
      }
    }
    return TimeDuration::FromTicks(ticks);
  }

  TimeStamp operator+(const TimeDuration& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    return TimeStamp(mValue + aOther.mValue);
  }
  TimeStamp operator-(const TimeDuration& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    return TimeStamp(mValue - aOther.mValue);
  }
  TimeStamp& operator+=(const TimeDuration& aOther) {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    mValue += aOther.mValue;
    return *this;
  }
  TimeStamp& operator-=(const TimeDuration& aOther) {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    mValue -= aOther.mValue;
    return *this;
  }

  bool operator<(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue < aOther.mValue;
  }
  bool operator<=(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue <= aOther.mValue;
  }
  bool operator>=(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue >= aOther.mValue;
  }
  bool operator>(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue > aOther.mValue;
  }
  bool operator==(const TimeStamp& aOther) const {
    // Maybe it's ok to check == with null timestamps?
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue == aOther.mValue;
  }
  bool operator!=(const TimeStamp& aOther) const {
    // Maybe it's ok to check != with null timestamps?
    MOZ_ASSERT(!IsNull(), "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue != aOther.mValue;
  }

  // Comparing TimeStamps for equality should be discouraged. Adding
  // two TimeStamps, or scaling TimeStamps, is nonsense and must never
  // be allowed.

  static NS_HIDDEN_(nsresult) Startup();
  static NS_HIDDEN_(void) Shutdown();

private:
  friend struct IPC::ParamTraits<mozilla::TimeStamp>;
  friend void StartupTimelineRecordExternal(int, uint64_t);

  TimeStamp(TimeStampValue aValue) : mValue(aValue) {}

  static TimeStamp Now(bool aHighResolution);

  /**
   * Computes the uptime of the current process in microseconds. The result
   * is platform-dependent and needs to be checked against existing timestamps
   * for consistency.
   *
   * @returns The number of microseconds since the calling process was started
   *          or 0 if an error was encountered while computing the uptime
   */
  static uint64_t ComputeProcessUptime();

  /**
   * When built with PRIntervalTime, a value of 0 means this instance
   * is "null". Otherwise, the low 32 bits represent a PRIntervalTime,
   * and the high 32 bits represent a counter of the number of
   * rollovers of PRIntervalTime that we've seen. This counter starts
   * at 1 to avoid a real time colliding with the "null" value.
   * 
   * PR_INTERVAL_MAX is set at 100,000 ticks per second. So the minimum
   * time to wrap around is about 2^64/100000 seconds, i.e. about
   * 5,849,424 years.
   *
   * When using a system clock, a value is system dependent.
   */
  TimeStampValue mValue;
};

}

#endif /* mozilla_TimeStamp_h */
