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
