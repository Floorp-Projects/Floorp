/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SystemTimeConverter_h
#define SystemTimeConverter_h

#include <limits>
#include <type_traits>
#include "mozilla/TimeStamp.h"

namespace mozilla {

// Utility class that converts time values represented as an unsigned integral
// number of milliseconds from one time source (e.g. a native event time) to
// corresponding mozilla::TimeStamp objects.
//
// This class handles wrapping of integer values and skew between the time
// source and mozilla::TimeStamp values.
//
// It does this by using an historical reference time recorded in both time
// scales (i.e. both as a numerical time value and as a TimeStamp).
//
// For performance reasons, this class is careful to minimize calls to the
// native "current time" function (e.g. gdk_x11_server_get_time) since this can
// be slow.
template <typename Time>
class SystemTimeConverter {
 public:
  SystemTimeConverter()
      : mReferenceTime(Time(0)),
        mReferenceTimeStamp()  // Initializes to the null timestamp
        ,
        mLastBackwardsSkewCheck(Time(0)),
        kTimeRange(std::numeric_limits<Time>::max()),
        kTimeHalfRange(kTimeRange / 2),
        kBackwardsSkewCheckInterval(Time(2000)) {
    static_assert(!std::is_signed_v<Time>, "Expected Time to be unsigned");
  }

  template <typename CurrentTimeGetter>
  mozilla::TimeStamp GetTimeStampFromSystemTime(
      Time aTime, CurrentTimeGetter& aCurrentTimeGetter) {
    TimeStamp roughlyNow = TimeStamp::Now();

    // If the reference time is not set, use the current time value to fill
    // it in.
    if (mReferenceTimeStamp.IsNull()) {
      // This sometimes happens when ::GetMessageTime returns 0 for the first
      // message on Windows.
      if (!aTime) return roughlyNow;
      UpdateReferenceTime(aTime, aCurrentTimeGetter);
    }

    // Check for skew between the source of Time values and TimeStamp values.
    // We do this by comparing two durations (both in ms):
    //
    // i.  The duration from the reference time to the passed-in time.
    //     (timeDelta in the diagram below)
    // ii. The duration from the reference timestamp to the current time
    //     based on TimeStamp::Now.
    //     (timeStampDelta in the diagram below)
    //
    // Normally, we'd expect (ii) to be slightly larger than (i) to account
    // for the time taken between generating the event and processing it.
    //
    // If (ii) - (i) is negative then the source of Time values is getting
    // "ahead" of TimeStamp. We call this "forwards" skew below.
    //
    // For the reverse case, if (ii) - (i) is positive (and greater than some
    // tolerance factor), then we may have "backwards" skew. This is often
    // the case when we have a backlog of events and by the time we process
    // them, the time given by the system is comparatively "old".
    //
    // We call the absolute difference between (i) and (ii), "deltaFromNow".
    //
    // Graphically:
    //
    //                    mReferenceTime              aTime
    // Time scale:      ........+.......................*........
    //                          |--------timeDelta------|
    //
    //                  mReferenceTimeStamp             roughlyNow
    // TimeStamp scale: ........+...........................*....
    //                          |------timeStampDelta-------|
    //
    //                                                  |---|
    //                                               deltaFromNow
    //
    Time deltaFromNow;
    bool newer = IsTimeNewerThanTimestamp(aTime, roughlyNow, &deltaFromNow);

    // Tolerance when detecting clock skew.
    static const Time kTolerance = 30;

    // Check for forwards skew
    if (newer) {
      // Make aTime correspond to roughlyNow
      UpdateReferenceTime(aTime, roughlyNow);

      // We didn't have backwards skew so don't bother checking for
      // backwards skew again for a little while.
      mLastBackwardsSkewCheck = aTime;

      return roughlyNow;
    }

    if (deltaFromNow <= kTolerance) {
      // If the time between event times and TimeStamp values is within
      // the tolerance then assume we don't have clock skew so we can
      // avoid checking for backwards skew for a while.
      mLastBackwardsSkewCheck = aTime;
    } else if (aTime - mLastBackwardsSkewCheck > kBackwardsSkewCheckInterval) {
      aCurrentTimeGetter.GetTimeAsyncForPossibleBackwardsSkew(roughlyNow);
      mLastBackwardsSkewCheck = aTime;
    }

    // Finally, calculate the timestamp
    return roughlyNow - TimeDuration::FromMilliseconds(deltaFromNow);
  }

  void CompensateForBackwardsSkew(Time aReferenceTime,
                                  const TimeStamp& aLowerBound) {
    // Check if we actually have backwards skew. Backwards skew looks like
    // the following:
    //
    //        mReferenceTime
    // Time:      ..+...a...b...c..........................
    //
    //     mReferenceTimeStamp
    // TimeStamp: ..+.....a.....b.....c....................
    //
    // Converted
    // time:      ......a'..b'..c'.........................
    //
    // What we need to do is bring mReferenceTime "forwards".
    //
    // Suppose when we get (c), we detect possible backwards skew and trigger
    // an async request for the current time (which is passed in here as
    // aReferenceTime).
    //
    // We end up with something like the following:
    //
    //        mReferenceTime     aReferenceTime
    // Time:      ..+...a...b...c...v......................
    //
    //     mReferenceTimeStamp
    // TimeStamp: ..+.....a.....b.....c..........x.........
    //                                ^          ^
    //                          aLowerBound  TimeStamp::Now()
    //
    // If the duration (aLowerBound - mReferenceTimeStamp) is greater than
    // (aReferenceTime - mReferenceTime) then we know we have backwards skew.
    //
    // If that's not the case, then we probably just got caught behind
    // temporarily.
    Time delta;
    if (IsTimeNewerThanTimestamp(aReferenceTime, aLowerBound, &delta)) {
      return;
    }

    // We have backwards skew; the equivalent TimeStamp for aReferenceTime lies
    // somewhere between aLowerBound (which was the TimeStamp when we triggered
    // the async request for the current time) and TimeStamp::Now().
    //
    // If aReferenceTime was waiting in the event queue for a long time, the
    // equivalent TimeStamp might be much closer to aLowerBound than
    // TimeStamp::Now() so for now we just set it to aLowerBound. That's
    // guaranteed to be at least somewhat of an improvement.
    UpdateReferenceTime(aReferenceTime, aLowerBound);
  }

 private:
  template <typename CurrentTimeGetter>
  void UpdateReferenceTime(Time aReferenceTime,
                           const CurrentTimeGetter& aCurrentTimeGetter) {
    Time currentTime = aCurrentTimeGetter.GetCurrentTime();
    TimeStamp currentTimeStamp = TimeStamp::Now();
    Time timeSinceReference = currentTime - aReferenceTime;
    TimeStamp referenceTimeStamp =
        currentTimeStamp - TimeDuration::FromMilliseconds(timeSinceReference);
    UpdateReferenceTime(aReferenceTime, referenceTimeStamp);
  }

  void UpdateReferenceTime(Time aReferenceTime,
                           const TimeStamp& aReferenceTimeStamp) {
    mReferenceTime = aReferenceTime;
    mReferenceTimeStamp = aReferenceTimeStamp;
  }

  bool IsTimeNewerThanTimestamp(Time aTime, TimeStamp aTimeStamp,
                                Time* aDelta) {
    Time timeDelta = aTime - mReferenceTime;

    // Cast the result to signed 64-bit integer first since that should be
    // enough to hold the range of values returned by ToMilliseconds() and
    // the result of converting from double to an integer-type when the value
    // is outside the integer range is undefined.
    // Then we do an implicit cast to Time (typically an unsigned 32-bit
    // integer) which wraps times outside that range.
    Time timeStampDelta = static_cast<int64_t>(
        (aTimeStamp - mReferenceTimeStamp).ToMilliseconds());

    Time timeToTimeStamp = timeStampDelta - timeDelta;
    bool isNewer = false;
    if (timeToTimeStamp == 0) {
      *aDelta = 0;
    } else if (timeToTimeStamp < kTimeHalfRange) {
      *aDelta = timeToTimeStamp;
    } else {
      isNewer = true;
      *aDelta = timeDelta - timeStampDelta;
    }

    return isNewer;
  }

  Time mReferenceTime;
  TimeStamp mReferenceTimeStamp;
  Time mLastBackwardsSkewCheck;

  const Time kTimeRange;
  const Time kTimeHalfRange;
  const Time kBackwardsSkewCheckInterval;
};

}  // namespace mozilla

#endif /* SystemTimeConverter_h */
