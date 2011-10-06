/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_TimeStamp_h
#define mozilla_TimeStamp_h

#include "prinrval.h"
#include "nsDebug.h"
#include "prlong.h"
#include "mozilla/Util.h"

namespace mozilla {

class TimeStamp;

/**
 * Instances of this class represent the length of an interval of time.
 * Negative durations are allowed, meaning the end is before the start.
 * 
 * Internally the duration is stored as a PRInt64 in units of
 * PR_TicksPerSecond() when building with NSPR interval timers, or a
 * system-dependent unit when building with system clocks.  The
 * system-dependent unit must be constant, otherwise the semantics of
 * this class would be broken.
 */
class TimeDuration
{
public:
  // The default duration is 0.
  TimeDuration() : mValue(0) {}
  // Allow construction using '0' as the initial value, for readability,
  // but no other numbers (so we don't have any implicit unit conversions).
  struct _SomethingVeryRandomHere;
  TimeDuration(_SomethingVeryRandomHere* aZero) : mValue(0) {
    MOZ_ASSERT(!aZero && "Who's playing funny games here?");
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
  double operator/(const TimeDuration& aOther) {
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

  static TimeDuration FromTicks(PRInt64 aTicks) {
    TimeDuration t;
    t.mValue = aTicks;
    return t;
  }

  static TimeDuration FromTicks(double aTicks) {
    // NOTE: this MUST be a >= test, because PRInt64(double(LL_MAXINT))
    // overflows and gives LL_MININT.
    if (aTicks >= double(LL_MAXINT))
      return TimeDuration::FromTicks(LL_MAXINT);

    // This MUST be a <= test.
    if (aTicks <= double(LL_MININT))
      return TimeDuration::FromTicks(LL_MININT);

    return TimeDuration::FromTicks(PRInt64(aTicks));
  }

  // Duration in PRIntervalTime units
  PRInt64 mValue;
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
 */
class TimeStamp
{
public:
  /**
   * Initialize to the "null" moment
   */
  TimeStamp() : mValue(0) {}
  // Default copy-constructor and assignment are OK

  /**
   * Return true if this is the "null" moment
   */
  bool IsNull() const { return mValue == 0; }
  /**
   * Return a timestamp reflecting the current elapsed system time. This
   * is monotonically increasing (i.e., does not decrease) over the
   * lifetime of this process' XPCOM session.
   */
  static TimeStamp Now();
  /**
   * Compute the difference between two timestamps. Both must be non-null.
   */
  TimeDuration operator-(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    PR_STATIC_ASSERT(-LL_MAXINT > LL_MININT);
    PRInt64 ticks = PRInt64(mValue - aOther.mValue);
    // Check for overflow.
    if (mValue > aOther.mValue) {
      if (ticks < 0) {
        ticks = LL_MAXINT;
      }
    } else {
      if (ticks > 0) {
        ticks = LL_MININT;
      }
    }
    return TimeDuration::FromTicks(ticks);
  }

  TimeStamp operator+(const TimeDuration& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    return TimeStamp(mValue + aOther.mValue);
  }
  TimeStamp operator-(const TimeDuration& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    return TimeStamp(mValue - aOther.mValue);
  }
  TimeStamp& operator+=(const TimeDuration& aOther) {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    mValue += aOther.mValue;
    return *this;
  }
  TimeStamp& operator-=(const TimeDuration& aOther) {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    mValue -= aOther.mValue;
    return *this;
  }

  bool operator<(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue < aOther.mValue;
  }
  bool operator<=(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue <= aOther.mValue;
  }
  bool operator>=(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue >= aOther.mValue;
  }
  bool operator>(const TimeStamp& aOther) const {
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue > aOther.mValue;
  }
  bool operator==(const TimeStamp& aOther) const {
    // Maybe it's ok to check == with null timestamps?
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue == aOther.mValue;
  }
  bool operator!=(const TimeStamp& aOther) const {
    // Maybe it's ok to check != with null timestamps?
    MOZ_ASSERT(!IsNull() && "Cannot compute with a null value");
    MOZ_ASSERT(!aOther.IsNull() && "Cannot compute with aOther null value");
    return mValue != aOther.mValue;
  }

  // Comparing TimeStamps for equality should be discouraged. Adding
  // two TimeStamps, or scaling TimeStamps, is nonsense and must never
  // be allowed.

  static NS_HIDDEN_(nsresult) Startup();
  static NS_HIDDEN_(void) Shutdown();

private:
  TimeStamp(PRUint64 aValue) : mValue(aValue) {}

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
  PRUint64 mValue;
};

}

#endif /* mozilla_TimeStamp_h */
