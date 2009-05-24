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

namespace mozilla {

class TimeStamp;

/**
 * Instances of this class represent the length of an interval of time.
 * Negative durations are allowed, meaning the end is before the start.
 * 
 * Internally the duration is stored as a PRInt64 in units of
 * PR_TicksPerSecond().
 * 
 * This whole class is inline so we don't need any special linkage.
 */
class TimeDuration {
public:
  // The default duration is 0.
  TimeDuration() : mValue(0) {}
  // Allow construction using '0' as the initial value, for readability,
  // but no other numbers (so we don't have any implicit unit conversions).
  struct _SomethingVeryRandomHere;
  TimeDuration(_SomethingVeryRandomHere* aZero) : mValue(0) {
    NS_ASSERTION(!aZero, "Who's playing funny games here?");
  }
  // Default copy-constructor and assignment are OK

  double ToSeconds() const { return double(mValue)/PR_TicksPerSecond(); }

  static TimeDuration FromSeconds(PRInt32 aSeconds) {
    // No overflow is possible here
    return TimeDuration::FromTicks(PRInt64(aSeconds)*PR_TicksPerSecond());
  }
  static TimeDuration FromMilliseconds(PRInt32 aMilliseconds) {
    // No overflow is possible here
    return TimeDuration::FromTicks(PRInt64(aMilliseconds)*PR_TicksPerSecond()/1000);
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

  PRBool operator<(const TimeDuration& aOther) const {
    return mValue < aOther.mValue;
  }
  PRBool operator<=(const TimeDuration& aOther) const {
    return mValue <= aOther.mValue;
  }
  PRBool operator>=(const TimeDuration& aOther) const {
    return mValue >= aOther.mValue;
  }
  PRBool operator>(const TimeDuration& aOther) const {
    return mValue > aOther.mValue;
  }

  // We could define additional operators here:
  // -- convert to/from other time units
  // -- scale duration by a float
  // but let's do that on demand.
  // Comparing durations for equality should be discouraged.

private:
  friend class TimeStamp;

  static TimeDuration FromTicks(PRInt64 aTicks) {
    TimeDuration t;
    t.mValue = aTicks;
    return t;
  }

  // Duration in PRIntervalTime units
  PRInt64 mValue;
};

/**
 * Instances of this class represent moments in time, or a special "null"
 * moment. We do not use the system clock or local time, since they can be
 * reset, causing apparent backward travel in time, which can confuse
 * algorithms. Instead we measure elapsed time according to the system.
 * This time can never go backwards (i.e. it never wraps around, at least
 * not in less than five million years of system elapsed time). It might
 * not advance while the system is sleeping. If TimeStamp::SetNow() is not
 * called at all for hours or days, we might not notice the passage
 * of some of that time.
 * 
 * We deliberately do not expose a way to convert TimeStamps to some
 * particular unit. All you can do is compute a difference between two
 * TimeStamps to get a TimeDuration. You can also add a TimeDuration
 * to a TimeStamp to get a new TimeStamp. You can't do something
 * meaningless like add two TimeStamps.
 *
 * Internally this is implemented as a wrapper around PRIntervalTime.
 * We detect wraparounds of PRIntervalTime and work around them.
 */
class NS_COM TimeStamp {
public:
  /**
   * Initialize to the "null" moment
   */
  TimeStamp() : mValue(0) {}
  // Default copy-constructor and assignment are OK

  /**
   * Return true if this is the "null" moment
   */
  PRBool IsNull() const { return mValue == 0; }
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
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    NS_ASSERTION(!aOther.IsNull(), "Cannot compute with aOther null value");
    return TimeDuration::FromTicks(mValue - aOther.mValue);
  }

  TimeStamp operator+(const TimeDuration& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    return TimeStamp(mValue + aOther.mValue);
  }
  TimeStamp operator-(const TimeDuration& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    return TimeStamp(mValue - aOther.mValue);
  }
  TimeStamp& operator+=(const TimeDuration& aOther) {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    mValue += aOther.mValue;
    return *this;
  }
  TimeStamp& operator-=(const TimeDuration& aOther) {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    mValue -= aOther.mValue;
    return *this;
  }

  PRBool operator<(const TimeStamp& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    NS_ASSERTION(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue < aOther.mValue;
  }
  PRBool operator<=(const TimeStamp& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    NS_ASSERTION(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue <= aOther.mValue;
  }
  PRBool operator>=(const TimeStamp& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    NS_ASSERTION(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue >= aOther.mValue;
  }
  PRBool operator>(const TimeStamp& aOther) const {
    NS_ASSERTION(!IsNull(), "Cannot compute with a null value");
    NS_ASSERTION(!aOther.IsNull(), "Cannot compute with aOther null value");
    return mValue > aOther.mValue;
  }

  // Comparing TimeStamps for equality should be discouraged. Adding
  // two TimeStamps, or scaling TimeStamps, is nonsense and must never
  // be allowed.

  static NS_HIDDEN_(nsresult) Startup();
  static NS_HIDDEN_(void) Shutdown();

private:
  TimeStamp(PRUint64 aValue) : mValue(aValue) {}

  /**
   * A value of 0 means this instance is "null". Otherwise,
   * the low 32 bits represent a PRIntervalTime, and the high 32 bits
   * represent a counter of the number of rollovers of PRIntervalTime
   * that we've seen. This counter starts at 1 to avoid a real time
   * colliding with the "null" value.
   * 
   * PR_INTERVAL_MAX is set at 100,000 ticks per second. So the minimum
   * time to wrap around is about 2^64/100000 seconds, i.e. about
   * 5,849,424 years.
   */
  PRUint64 mValue;
};

}

#endif /* mozilla_TimeStamp_h */
