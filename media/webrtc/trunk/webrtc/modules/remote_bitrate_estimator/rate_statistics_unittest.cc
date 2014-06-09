/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"

namespace {

using webrtc::RateStatistics;

class RateStatisticsTest : public ::testing::Test {
 protected:
  RateStatisticsTest() : stats_(500, 8000) {}
  RateStatistics stats_;
};

TEST_F(RateStatisticsTest, TestStrictMode) {
  int64_t now_ms = 0;
  // Should be initialized to 0.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
  stats_.Update(1500, now_ms);
  // Expecting 24 kbps given a 500 ms window with one 1500 bytes packet.
  EXPECT_EQ(24000u, stats_.Rate(now_ms));
  stats_.Reset();
  // Expecting 0 after init.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
  for (int i = 0; i < 100000; ++i) {
    if (now_ms % 10 == 0) {
      stats_.Update(1500, now_ms);
    }
    // Approximately 1200 kbps expected. Not exact since when packets
    // are removed we will jump 10 ms to the next packet.
    if (now_ms > 0 && now_ms % 500 == 0) {
      EXPECT_NEAR(1200000u, stats_.Rate(now_ms), 24000u);
    }
    now_ms += 1;
  }
  now_ms += 500;
  // The window is 2 seconds. If nothing has been received for that time
  // the estimate should be 0.
  EXPECT_EQ(0u, stats_.Rate(now_ms));
}

TEST_F(RateStatisticsTest, IncreasingThenDecreasingBitrate) {
  int64_t now_ms = 0;
  stats_.Reset();
  // Expecting 0 after init.
  uint32_t bitrate = stats_.Rate(now_ms);
  EXPECT_EQ(0u, bitrate);
  // 1000 bytes per millisecond until plateau is reached.
  while (++now_ms < 10000) {
    stats_.Update(1000, now_ms);
    uint32_t new_bitrate = stats_.Rate(now_ms);
    if (new_bitrate != bitrate) {
      // New bitrate must be higher than previous one.
      EXPECT_GT(new_bitrate, bitrate);
    } else {
      // Plateau reached, 8000 kbps expected.
      EXPECT_NEAR(8000000u, bitrate, 80000u);
      break;
    }
    bitrate = new_bitrate;
  }
  // 1000 bytes per millisecond until 10-second mark, 8000 kbps expected.
  while (++now_ms < 10000) {
    stats_.Update(1000, now_ms);
    bitrate = stats_.Rate(now_ms);
    EXPECT_NEAR(8000000u, bitrate, 80000u);
  }
  // Zero bytes per millisecond until 0 is reached.
  while (++now_ms < 20000) {
    stats_.Update(0, now_ms);
    uint32_t new_bitrate = stats_.Rate(now_ms);
    if (new_bitrate != bitrate) {
      // New bitrate must be lower than previous one.
      EXPECT_LT(new_bitrate, bitrate);
    } else {
      // 0 kbps expected.
      EXPECT_EQ(0u, bitrate);
      break;
    }
    bitrate = new_bitrate;
  }
  // Zero bytes per millisecond until 20-second mark, 0 kbps expected.
  while (++now_ms < 20000) {
    stats_.Update(0, now_ms);
    EXPECT_EQ(0u, stats_.Rate(now_ms));
  }
}
}  // namespace
