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
#include "webrtc/base/helpers.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

TEST(TimeTest, TimeInMs) {
  uint32_t ts_earlier = Time();
  Thread::SleepMs(100);
  uint32_t ts_now = Time();
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
  uint32_t ts = 0xfffffff2;
  int64_t unwrapped_ts = ts;
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

class TmToSeconds : public testing::Test {
 public:
  TmToSeconds() {
    // Set use of the test RNG to get deterministic expiration timestamp.
    rtc::SetRandomTestMode(true);
  }
  ~TmToSeconds() {
    // Put it back for the next test.
    rtc::SetRandomTestMode(false);
  }

  void TestTmToSeconds(int times) {
    static char mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < times; i++) {

      // First generate something correct and check that TmToSeconds is happy.
      int year = rtc::CreateRandomId() % 400 + 1970;

      bool leap_year = false;
      if (year % 4 == 0)
        leap_year = true;
      if (year % 100 == 0)
        leap_year = false;
      if (year % 400 == 0)
        leap_year = true;

      std::tm tm;
      tm.tm_year = year - 1900;  // std::tm is year 1900 based.
      tm.tm_mon = rtc::CreateRandomId() % 12;
      tm.tm_mday = rtc::CreateRandomId() % mdays[tm.tm_mon] + 1;
      tm.tm_hour = rtc::CreateRandomId() % 24;
      tm.tm_min = rtc::CreateRandomId() % 60;
      tm.tm_sec = rtc::CreateRandomId() % 60;
      int64_t t = rtc::TmToSeconds(tm);
      EXPECT_TRUE(t >= 0);

      // Now damage a random field and check that TmToSeconds is unhappy.
      switch (rtc::CreateRandomId() % 11) {
        case 0:
          tm.tm_year = 1969 - 1900;
          break;
        case 1:
          tm.tm_mon = -1;
          break;
        case 2:
          tm.tm_mon = 12;
          break;
        case 3:
          tm.tm_mday = 0;
          break;
        case 4:
          tm.tm_mday = mdays[tm.tm_mon] + (leap_year && tm.tm_mon == 1) + 1;
          break;
        case 5:
          tm.tm_hour = -1;
          break;
        case 6:
          tm.tm_hour = 24;
          break;
        case 7:
          tm.tm_min = -1;
          break;
        case 8:
          tm.tm_min = 60;
          break;
        case 9:
          tm.tm_sec = -1;
          break;
        case 10:
          tm.tm_sec = 60;
          break;
      }
      EXPECT_EQ(rtc::TmToSeconds(tm), -1);
    }
    // Check consistency with the system gmtime_r.  With time_t, we can only
    // portably test dates until 2038, which is achieved by the % 0x80000000.
    for (int i = 0; i < times; i++) {
      time_t t = rtc::CreateRandomId() % 0x80000000;
#if defined(WEBRTC_WIN)
      std::tm* tm = std::gmtime(&t);
      EXPECT_TRUE(tm);
      EXPECT_TRUE(rtc::TmToSeconds(*tm) == t);
#else
      std::tm tm;
      EXPECT_TRUE(gmtime_r(&t, &tm));
      EXPECT_TRUE(rtc::TmToSeconds(tm) == t);
#endif
    }
  }
};

TEST_F(TmToSeconds, TestTmToSeconds) {
  TestTmToSeconds(100000);
}

}  // namespace rtc
