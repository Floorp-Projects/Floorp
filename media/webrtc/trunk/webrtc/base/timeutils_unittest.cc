/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

TEST(TimeTest, TimeInMs) {
  uint32 ts_earlier = Time();
  Thread::SleepMs(100);
  uint32 ts_now = Time();
  // Allow for the thread to wakeup ~20ms early.
  EXPECT_GE(ts_now, ts_earlier + 80);
  // Make sure the Time is not returning in smaller unit like microseconds.
  EXPECT_LT(ts_now, ts_earlier + 1000);
}

TEST(TimeTest, Comparison) {
  // Obtain two different times, in known order
  TimeStamp ts_earlier = Time();
  Thread::SleepMs(100);
  TimeStamp ts_now = Time();
  EXPECT_NE(ts_earlier, ts_now);

  // Common comparisons
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_now));
  EXPECT_TRUE( TimeIsLater(       ts_earlier, ts_now));
  EXPECT_FALSE(TimeIsLaterOrEqual(ts_now,     ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_now,     ts_earlier));

  // Edge cases
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_earlier, ts_earlier));

  // Obtain a third time
  TimeStamp ts_later = TimeAfter(100);
  EXPECT_NE(ts_now, ts_later);
  EXPECT_TRUE( TimeIsLater(ts_now,     ts_later));
  EXPECT_TRUE( TimeIsLater(ts_earlier, ts_later));

  // Common comparisons
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_now,     ts_later));
  EXPECT_FALSE(TimeIsBetween(ts_earlier, ts_later,   ts_now));
  EXPECT_FALSE(TimeIsBetween(ts_now,     ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsBetween(ts_now,     ts_later,   ts_earlier));
  EXPECT_TRUE( TimeIsBetween(ts_later,   ts_earlier, ts_now));
  EXPECT_FALSE(TimeIsBetween(ts_later,   ts_now,     ts_earlier));

  // Edge cases
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_earlier, ts_earlier));
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsBetween(ts_earlier, ts_later,   ts_later));

  // Earlier of two times
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_now));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_later));
  EXPECT_EQ(ts_earlier, TimeMin(ts_now,     ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_later,   ts_earlier));

  // Later of two times
  EXPECT_EQ(ts_earlier, TimeMax(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_now,     TimeMax(ts_earlier, ts_now));
  EXPECT_EQ(ts_later,   TimeMax(ts_earlier, ts_later));
  EXPECT_EQ(ts_now,     TimeMax(ts_now,     ts_earlier));
  EXPECT_EQ(ts_later,   TimeMax(ts_later,   ts_earlier));
}

TEST(TimeTest, Intervals) {
  TimeStamp ts_earlier = Time();
  TimeStamp ts_later = TimeAfter(500);

  // We can't depend on ts_later and ts_earlier to be exactly 500 apart
  // since time elapses between the calls to Time() and TimeAfter(500)
  EXPECT_LE(500,  TimeDiff(ts_later, ts_earlier));
  EXPECT_GE(-500, TimeDiff(ts_earlier, ts_later));

  // Time has elapsed since ts_earlier
  EXPECT_GE(TimeSince(ts_earlier), 0);

  // ts_earlier is earlier than now, so TimeUntil ts_earlier is -ve
  EXPECT_LE(TimeUntil(ts_earlier), 0);

  // ts_later likely hasn't happened yet, so TimeSince could be -ve
  // but within 500
  EXPECT_GE(TimeSince(ts_later), -500);

  // TimeUntil ts_later is at most 500
  EXPECT_LE(TimeUntil(ts_later), 500);
}

TEST(TimeTest, BoundaryComparison) {
  // Obtain two different times, in known order
  TimeStamp ts_earlier = static_cast<TimeStamp>(-50);
  TimeStamp ts_later = ts_earlier + 100;
  EXPECT_NE(ts_earlier, ts_later);

  // Common comparisons
  EXPECT_TRUE( TimeIsLaterOrEqual(ts_earlier, ts_later));
  EXPECT_TRUE( TimeIsLater(       ts_earlier, ts_later));
  EXPECT_FALSE(TimeIsLaterOrEqual(ts_later,   ts_earlier));
  EXPECT_FALSE(TimeIsLater(       ts_later,   ts_earlier));

  // Earlier of two times
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_earlier, TimeMin(ts_earlier, ts_later));
  EXPECT_EQ(ts_earlier, TimeMin(ts_later,   ts_earlier));

  // Later of two times
  EXPECT_EQ(ts_earlier, TimeMax(ts_earlier, ts_earlier));
  EXPECT_EQ(ts_later,   TimeMax(ts_earlier, ts_later));
  EXPECT_EQ(ts_later,   TimeMax(ts_later,   ts_earlier));

  // Interval
  EXPECT_EQ(100,  TimeDiff(ts_later, ts_earlier));
  EXPECT_EQ(-100, TimeDiff(ts_earlier, ts_later));
}

TEST(TimeTest, DISABLED_CurrentTmTime) {
  struct tm tm;
  int microseconds;

  time_t before = ::time(NULL);
  CurrentTmTime(&tm, &microseconds);
  time_t after = ::time(NULL);

  // Assert that 'tm' represents a time between 'before' and 'after'.
  // mktime() uses local time, so we have to compensate for that.
  time_t local_delta = before - ::mktime(::gmtime(&before));  // NOLINT
  time_t t = ::mktime(&tm) + local_delta;

  EXPECT_TRUE(before <= t && t <= after);
  EXPECT_TRUE(0 <= microseconds && microseconds < 1000000);
}

class TimestampWrapAroundHandlerTest : public testing::Test {
 public:
  TimestampWrapAroundHandlerTest() {}

 protected:
  TimestampWrapAroundHandler wraparound_handler_;
};

TEST_F(TimestampWrapAroundHandlerTest, Unwrap) {
  uint32 ts = 0xfffffff2;
  int64 unwrapped_ts = ts;
  EXPECT_EQ(ts, wraparound_handler_.Unwrap(ts));
  ts = 2;
  unwrapped_ts += 0x10;
  EXPECT_EQ(unwrapped_ts, wraparound_handler_.Unwrap(ts));
  ts = 0xfffffff2;
  unwrapped_ts += 0xfffffff0;
  EXPECT_EQ(unwrapped_ts, wraparound_handler_.Unwrap(ts));
  ts = 0;
  unwrapped_ts += 0xe;
  EXPECT_EQ(unwrapped_ts, wraparound_handler_.Unwrap(ts));
}

}  // namespace rtc
