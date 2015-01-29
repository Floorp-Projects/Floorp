/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/base/ratetracker.h"

namespace rtc {

class RateTrackerForTest : public RateTracker {
 public:
  RateTrackerForTest() : time_(0) {}
  virtual uint32 Time() const { return time_; }
  void AdvanceTime(uint32 delta) { time_ += delta; }

 private:
  uint32 time_;
};

TEST(RateTrackerTest, TestBasics) {
  RateTrackerForTest tracker;
  EXPECT_EQ(0U, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Add a sample.
  tracker.Update(1234);
  // Advance the clock by 100 ms.
  tracker.AdvanceTime(100);
  // total_units should advance, but units_second should stay 0.
  EXPECT_EQ(1234U, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Repeat.
  tracker.Update(1234);
  tracker.AdvanceTime(100);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Advance the clock by 800 ms, so we've elapsed a full second.
  // units_second should now be filled in properly.
  tracker.AdvanceTime(800);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(1234U * 2, tracker.units_second());

  // Poll the tracker again immediately. The reported rate should stay the same.
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(1234U * 2, tracker.units_second());

  // Do nothing and advance by a second. We should drop down to zero.
  tracker.AdvanceTime(1000);
  EXPECT_EQ(1234U * 2, tracker.total_units());
  EXPECT_EQ(0U, tracker.units_second());

  // Send a bunch of data at a constant rate for 5.5 "seconds".
  // We should report the rate properly.
  for (int i = 0; i < 5500; i += 100) {
    tracker.Update(9876U);
    tracker.AdvanceTime(100);
  }
  EXPECT_EQ(9876U * 10, tracker.units_second());

  // Advance the clock by 500 ms. Since we sent nothing over this half-second,
  // the reported rate should be reduced by half.
  tracker.AdvanceTime(500);
  EXPECT_EQ(9876U * 5, tracker.units_second());
}

}  // namespace rtc
