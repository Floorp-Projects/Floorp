/*
 *  Copyright 2005 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_TIMEUTILS_H_
#define WEBRTC_BASE_TIMEUTILS_H_

#include <time.h>

#include "webrtc/base/basictypes.h"

namespace rtc {

static const int64 kNumMillisecsPerSec = INT64_C(1000);
static const int64 kNumMicrosecsPerSec = INT64_C(1000000);
static const int64 kNumNanosecsPerSec = INT64_C(1000000000);

static const int64 kNumMicrosecsPerMillisec = kNumMicrosecsPerSec /
    kNumMillisecsPerSec;
static const int64 kNumNanosecsPerMillisec =  kNumNanosecsPerSec /
    kNumMillisecsPerSec;
static const int64 kNumNanosecsPerMicrosec =  kNumNanosecsPerSec /
    kNumMicrosecsPerSec;

// January 1970, in NTP milliseconds.
static const int64 kJan1970AsNtpMillisecs = INT64_C(2208988800000);

typedef uint32 TimeStamp;

// Returns the current time in milliseconds.
uint32 Time();
// Returns the current time in microseconds.
uint64 TimeMicros();
// Returns the current time in nanoseconds.
uint64 TimeNanos();

// Stores current time in *tm and microseconds in *microseconds.
void CurrentTmTime(struct tm *tm, int *microseconds);

// Returns a future timestamp, 'elapsed' milliseconds from now.
uint32 TimeAfter(int32 elapsed);

// Comparisons between time values, which can wrap around.
bool TimeIsBetween(uint32 earlier, uint32 middle, uint32 later);  // Inclusive
bool TimeIsLaterOrEqual(uint32 earlier, uint32 later);  // Inclusive
bool TimeIsLater(uint32 earlier, uint32 later);  // Exclusive

// Returns the later of two timestamps.
inline uint32 TimeMax(uint32 ts1, uint32 ts2) {
  return TimeIsLaterOrEqual(ts1, ts2) ? ts2 : ts1;
}

// Returns the earlier of two timestamps.
inline uint32 TimeMin(uint32 ts1, uint32 ts2) {
  return TimeIsLaterOrEqual(ts1, ts2) ? ts1 : ts2;
}

// Number of milliseconds that would elapse between 'earlier' and 'later'
// timestamps.  The value is negative if 'later' occurs before 'earlier'.
int32 TimeDiff(uint32 later, uint32 earlier);

// The number of milliseconds that have elapsed since 'earlier'.
inline int32 TimeSince(uint32 earlier) {
  return TimeDiff(Time(), earlier);
}

// The number of milliseconds that will elapse between now and 'later'.
inline int32 TimeUntil(uint32 later) {
  return TimeDiff(later, Time());
}

// Converts a unix timestamp in nanoseconds to an NTP timestamp in ms.
inline int64 UnixTimestampNanosecsToNtpMillisecs(int64 unix_ts_ns) {
  return unix_ts_ns / kNumNanosecsPerMillisec + kJan1970AsNtpMillisecs;
}

class TimestampWrapAroundHandler {
 public:
  TimestampWrapAroundHandler();

  int64 Unwrap(uint32 ts);

 private:
  uint32 last_ts_;
  int64 num_wrap_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_TIMEUTILS_H_
