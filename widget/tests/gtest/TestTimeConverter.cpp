/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/TimeStamp.h"
#include "SystemTimeConverter.h"

using mozilla::SystemTimeConverter;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

namespace {

// This class provides a mock implementation of the CurrentTimeGetter template
// type used in SystemTimeConverter. It can be constructed with a particular
// Time and always returns that Time.
template <typename Time>
class MockCurrentTimeGetter {
 public:
  MockCurrentTimeGetter() : mTime(0) {}
  explicit MockCurrentTimeGetter(Time aTime) : mTime(aTime) {}

  // Methods needed for CurrentTimeGetter compatibility
  Time GetCurrentTime() const { return mTime; }
  void GetTimeAsyncForPossibleBackwardsSkew(const TimeStamp& aNow) {}

 private:
  Time mTime;
};

// This is another mock implementation of the CurrentTimeGetter template
// type used in SystemTimeConverter, except this asserts that it will not be
// used. i.e. it should only be used in calls to SystemTimeConverter that we
// know will not invoke it.
template <typename Time>
class UnusedCurrentTimeGetter {
 public:
  Time GetCurrentTime() const {
    EXPECT_TRUE(false);
    return 0;
  }

  void GetTimeAsyncForPossibleBackwardsSkew(const TimeStamp& aNow) {
    EXPECT_TRUE(false);
  }
};

// This class provides a mock implementation of the TimeStampNowProvider
// template type used in SystemTimeConverter. It also has other things in it
// that allow the test to better control time for testing purposes.
class MockTimeStamp {
 public:
  // This should generally be called at the start of every test function, as
  // it will initialize this class's static fields to sane values. In particular
  // it will initialize the baseline TimeStamp against which all other
  // TimeStamps are compared.
  static void Init() {
    sBaseline = TimeStamp::Now();
    sTimeStamp = sBaseline;
  }

  // Advance the timestamp returned by `MockTimeStamp::Now()`
  static void Advance(double ms) {
    sTimeStamp += TimeDuration::FromMilliseconds(ms);
  }

  // Returns the baseline TimeStamp, that is used as a fixed reference point
  // in time against which other TimeStamps can be compared. This is needed
  // because mozilla::TimeStamp itself doesn't provide any conversion to
  // human-readable strings, and we need to convert it to a TimeDuration in
  // order to get that. This baseline TimeStamp can be used to turn an
  // arbitrary TimeStamp into a TimeDuration.
  static TimeStamp Baseline() { return sBaseline; }

  // This is the method needed for TimeStampNowProvider compatibility, and
  // simulates `TimeStamp::Now()`
  static TimeStamp Now() { return sTimeStamp; }

 private:
  static TimeStamp sTimeStamp;
  static TimeStamp sBaseline;
};

TimeStamp MockTimeStamp::sTimeStamp;
TimeStamp MockTimeStamp::sBaseline;

// Could have platform-specific implementations of this using DWORD, guint32,
// etc behind ifdefs. But this is sufficient for now.
using GTestTime = uint32_t;
using TimeConverter = SystemTimeConverter<GTestTime, MockTimeStamp>;

}  // namespace

// Checks the expectation that the TimeStamp `ts` is exactly `ms` milliseconds
// after the baseline timestamp. This is a macro so gtest still gives us useful
// line numbers for failures.
#define EXPECT_TS(ts, ms) \
  EXPECT_EQ((ts)-MockTimeStamp::Baseline(), TimeDuration::FromMilliseconds(ms))

#define EXPECT_TS_FUZZY(ts, ms) \
  EXPECT_DOUBLE_EQ(((ts)-MockTimeStamp::Baseline()).ToMilliseconds(), ms)

TEST(TimeConverter, SanityCheck)
{
  MockTimeStamp::Init();

  MockCurrentTimeGetter timeGetter(10);
  UnusedCurrentTimeGetter<GTestTime> unused;
  TimeConverter converter;

  // This call sets the reference time and timestamp
  TimeStamp ts = converter.GetTimeStampFromSystemTime(10, timeGetter);
  EXPECT_TS(ts, 0);

  // Advance "TimeStamp::Now" by 10ms, use the same event time and OS time.
  // Since the event time is the same as before, we expect to get back the
  // same TimeStamp as before too, despite Now() changing.
  MockTimeStamp::Advance(10);
  ts = converter.GetTimeStampFromSystemTime(10, unused);
  EXPECT_TS(ts, 0);

  // Now let's use an event time 20ms after the old event. This will trigger
  // forward skew detection and resync the TimeStamp for the new event to Now().
  ts = converter.GetTimeStampFromSystemTime(30, unused);
  EXPECT_TS(ts, 10);
}

TEST(TimeConverter, Overflow)
{
  // This tests wrapping time around the max value supported in the GTestTime
  // type and ensuring it is handled properly.

  MockTimeStamp::Init();

  const GTestTime max = std::numeric_limits<GTestTime>::max();
  const GTestTime min = std::numeric_limits<GTestTime>::min();
  double fullRange = (double)max - (double)min;
  double wrapPeriod = fullRange + 1.0;

  GTestTime almostOverflowed = max - 100;
  GTestTime overflowed = max + 100;
  MockCurrentTimeGetter timeGetter(almostOverflowed);
  UnusedCurrentTimeGetter<GTestTime> unused;
  TimeConverter converter;

  // Set reference time to 100ms before the overflow point
  TimeStamp ts =
      converter.GetTimeStampFromSystemTime(almostOverflowed, timeGetter);
  EXPECT_TS(ts, 0);

  // Advance everything by 200ms and verify we get back a TimeStamp 200ms from
  // the baseline despite wrapping an overflow.
  MockTimeStamp::Advance(200);
  ts = converter.GetTimeStampFromSystemTime(overflowed, unused);
  EXPECT_TS(ts, 200);

  // Advance by another full wraparound of the time. This loses some precision
  // so we have to do the FUZZY match
  MockTimeStamp::Advance(wrapPeriod);
  ts = converter.GetTimeStampFromSystemTime(overflowed, unused);
  EXPECT_TS_FUZZY(ts, 200.0 + wrapPeriod);
}

TEST(TimeConverter, InvertedOverflow)
{
  // This tests time going from near the min value of GTestTime to the max
  // value of GTestTime

  MockTimeStamp::Init();

  const GTestTime max = std::numeric_limits<GTestTime>::max();
  const GTestTime min = std::numeric_limits<GTestTime>::min();
  double fullRange = (double)max - (double)min;
  double wrapPeriod = fullRange + 1.0;

  GTestTime nearRangeMin = min + 100;
  GTestTime nearRangeMax = max - 100;
  double gap = (double)nearRangeMax - (double)nearRangeMin;

  MockCurrentTimeGetter timeGetter(nearRangeMin);
  UnusedCurrentTimeGetter<GTestTime> unused;
  TimeConverter converter;

  // Set reference time to value near min numeric limit
  TimeStamp ts = converter.GetTimeStampFromSystemTime(nearRangeMin, timeGetter);
  EXPECT_TS(ts, 0);

  // Advance to value near max numeric limit
  MockTimeStamp::Advance(gap);
  ts = converter.GetTimeStampFromSystemTime(nearRangeMax, unused);
  EXPECT_TS(ts, gap);

  // Advance by another full wraparound of the time. This loses some precision
  // so we have to do the FUZZY match
  MockTimeStamp::Advance(wrapPeriod);
  ts = converter.GetTimeStampFromSystemTime(nearRangeMax, unused);
  EXPECT_TS_FUZZY(ts, gap + wrapPeriod);
}

TEST(TimeConverter, HalfRangeBoundary)
{
  MockTimeStamp::Init();

  GTestTime max = std::numeric_limits<GTestTime>::max();
  GTestTime min = std::numeric_limits<GTestTime>::min();
  double fullRange = (double)max - (double)min;
  double wrapPeriod = fullRange + 1.0;
  GTestTime halfRange = (GTestTime)(fullRange / 2.0);
  GTestTime halfWrapPeriod = (GTestTime)(wrapPeriod / 2.0);

  TimeConverter converter;

  GTestTime firstEvent = 10;
  MockCurrentTimeGetter timeGetter(firstEvent);
  UnusedCurrentTimeGetter<GTestTime> unused;

  // Set reference time
  TimeStamp ts = converter.GetTimeStampFromSystemTime(firstEvent, timeGetter);
  EXPECT_TS(ts, 0);

  // Advance event time by just under the half-period, to trigger about as big
  // a forwards skew as we possibly can.
  GTestTime secondEvent = firstEvent + (halfWrapPeriod - 1);
  ts = converter.GetTimeStampFromSystemTime(secondEvent, unused);
  EXPECT_TS(ts, 0);

  // The above forwards skew will have reset the reference timestamp. Now
  // advance Now time by just under the half-range, to trigger about as big
  // a backwards skew as we possibly can.
  MockTimeStamp::Advance(halfRange - 1);
  ts = converter.GetTimeStampFromSystemTime(secondEvent, unused);
  EXPECT_TS(ts, 0);
}

TEST(TimeConverter, FractionalMillisBug1626734)
{
  MockTimeStamp::Init();

  TimeConverter converter;

  GTestTime eventTime = 10;
  MockCurrentTimeGetter timeGetter(eventTime);
  UnusedCurrentTimeGetter<GTestTime> unused;

  TimeStamp ts = converter.GetTimeStampFromSystemTime(eventTime, timeGetter);
  EXPECT_TS(ts, 0);

  MockTimeStamp::Advance(0.2);
  ts = converter.GetTimeStampFromSystemTime(eventTime, unused);
  EXPECT_TS(ts, 0);

  MockTimeStamp::Advance(0.9);
  TimeStamp ts2 = converter.GetTimeStampFromSystemTime(eventTime, unused);
  EXPECT_TS(ts2, 0);

  // Since ts2 came from a "future" call relative to ts, we expect ts2 to not
  // be "before" ts. (i.e. time shouldn't go backwards, even by fractional
  // milliseconds). This assertion is technically already implied by the
  // EXPECT_TS checks above, but fixing this assertion is the end result that
  // we wanted in bug 1626734 so it feels appropriate to recheck it explicitly.
  EXPECT_TRUE(ts <= ts2);
}
