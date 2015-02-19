/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SystemTimeConverter_h
#define SystemTimeConverter_h

#include <limits>
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {

// Utility class that takes a time value representing an integral number of
// milliseconds (e.g. a native event time) that wraps within a fixed range (e.g.
// unsigned 32-bit range) and converts it to a TimeStamp.
//
// It does this by using a historical reference time recorded in both time
// scales (i.e. both as a numerical time value and as a TimeStamp).
//
// For performance reasons, this class is careful to minimize calls to the
// native "current time" function (e.g. gdk_x11_server_get_time) since this can
// be slow. Furthermore, it uses TimeStamp::NowLowRes instead of TimeStamp::Now
// except when establishing the reference time.
template <typename Time>
class SystemTimeConverter {
public:
  SystemTimeConverter()
    : mReferenceTime(Time(0))
    , mReferenceTimeStamp() // Initializes to the null timestamp
    , kTimeRange(std::numeric_limits<Time>::max())
    , kTimeHalfRange(kTimeRange / 2)
  {
    static_assert(!IsSigned<Time>::value, "Expected Time to be unsigned");
  }

  template <typename GetCurrentTimeFunc>
  mozilla::TimeStamp
  GetTimeStampFromSystemTime(Time aTime,
                             GetCurrentTimeFunc aGetCurrentTimeFunc) {
    // If the reference time is not set, use the current time value to fill
    // it in.
    if (mReferenceTimeStamp.IsNull()) {
      UpdateReferenceTime(aTime, aGetCurrentTimeFunc);
    }
    TimeStamp roughlyNow = TimeStamp::NowLoRes();

    // If the time is before the reference time there are two possibilities:
    //
    //   a) Time has wrapped
    //   b) The initial times were not delivered strictly in time order
    //
    // I'm not sure if (b) ever happens but even if it doesn't currently occur,
    // it could come about if we change the way we fetch events, for example.
    //
    // We can tell we have (b) if the time is a little bit before the reference
    // time (i.e. less than half the range) and the timestamp is only a little
    // bit past the reference timestamp (again less than half the range).
    // In that case we should adjust the reference time so it is the
    // earliest time.
    Time timeSinceReference = aTime - mReferenceTime;
    if (timeSinceReference > kTimeHalfRange &&
        roughlyNow - mReferenceTimeStamp <
          TimeDuration::FromMilliseconds(kTimeHalfRange)) {
      UpdateReferenceTime(aTime, aGetCurrentTimeFunc);
      timeSinceReference = aTime - mReferenceTime;
    }

    TimeStamp timestamp =
      mReferenceTimeStamp + TimeDuration::FromMilliseconds(timeSinceReference);

    // Time may have wrapped several times since we recorded the reference
    // time so we extend timestamp as needed.
    double timesWrapped =
      (roughlyNow - mReferenceTimeStamp).ToMilliseconds() / kTimeRange;
    int32_t cyclesToAdd =
      static_cast<int32_t>(timesWrapped); // Round towards zero

    // There is some imprecision in the above calculation since we are using
    // TimeStamp::NowLoRes and mReferenceTime and mReferenceTimeStamp may not
    // be *exactly* the same moment. It is possible we think that time
    // has *just* wrapped based on comparing timestamps, but actually the
    // time is *just about* to wrap; or vice versa.
    // In the following, we detect this situation and adjust cyclesToAdd as
    // necessary.
    double intervalFraction = fmod(timesWrapped, 1.0);

    // If our rough calculation of how many times we've wrapped based on
    // comparing timestamps says we've just wrapped (specifically, less 10% past
    // the wrap point), but the time is just before the wrap point (again,
    // within 10%), then we need to reduce the number of wraps by 1.
    if (intervalFraction < 0.1 && timeSinceReference > kTimeRange * 0.9) {
      cyclesToAdd--;
    // Likewise, if our rough calculation says we've just wrapped but actually
    // the time is just after the wrap point, we need to add an extra wrap.
    } else if (intervalFraction > 0.9 &&
               timeSinceReference < kTimeRange * 0.1) {
      cyclesToAdd++;
    }

    if (cyclesToAdd > 0) {
      timestamp += TimeDuration::FromMilliseconds(kTimeRange * cyclesToAdd);
    }

    return timestamp;
  }

private:
  template <typename GetCurrentTimeFunc>
  void
  UpdateReferenceTime(Time aTime, GetCurrentTimeFunc aGetCurrentTimeFunc) {
    mReferenceTime = aTime;
    Time currentTime = aGetCurrentTimeFunc();
    TimeStamp currentTimeStamp = TimeStamp::Now();
    Time timeSinceReference = currentTime - aTime;
    mReferenceTimeStamp =
      currentTimeStamp - TimeDuration::FromMilliseconds(timeSinceReference);
  }

  Time mReferenceTime;
  TimeStamp mReferenceTimeStamp;

  const Time kTimeRange;
  const Time kTimeHalfRange;
};

} // namespace mozilla

#endif /* SystemTimeConverter_h */
