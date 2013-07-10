/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtest/gtest.h>

#include "webrtc/modules/remote_bitrate_estimator/bitrate_estimator.h"

namespace {

using webrtc::BitRateStats;

class BitRateStatsTest : public ::testing::Test {
 protected:
  BitRateStatsTest() {};
  BitRateStats stats_;
};

TEST_F(BitRateStatsTest, TestStrictMode) {
  int64_t now_ms = 0;
  // Should be initialized to 0.
  EXPECT_EQ(0u, stats_.BitRate(now_ms));
  stats_.Update(1500, now_ms);
  // Expecting 24 kbps given a 500 ms window with one 1500 bytes packet.
  EXPECT_EQ(24000u, stats_.BitRate(now_ms));
  stats_.Init();
  // Expecting 0 after init.
  EXPECT_EQ(0u, stats_.BitRate(now_ms));
  for (int i = 0; i < 100000; ++i) {
    if (now_ms % 10 == 0) {
      stats_.Update(1500, now_ms);
    }
    // Approximately 1200 kbps expected. Not exact since when packets
    // are removed we will jump 10 ms to the next packet.
    if (now_ms > 0 && now_ms % 500 == 0) {
      EXPECT_NEAR(1200000u, stats_.BitRate(now_ms), 24000u);
    }
    now_ms += 1;
  }
  now_ms += 500;
  // The window is 2 seconds. If nothing has been received for that time
  // the estimate should be 0.
  EXPECT_EQ(0u, stats_.BitRate(now_ms));
}
}  // namespace
