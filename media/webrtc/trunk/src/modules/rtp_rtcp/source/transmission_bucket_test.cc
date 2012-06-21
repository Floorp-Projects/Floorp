/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for the TransmissionBucket.
 */

#include <gtest/gtest.h>

#include "transmission_bucket.h"

namespace webrtc {

class TransmissionBucketTest : public ::testing::Test {
 protected:  
  TransmissionBucket send_bucket_;
};

TEST_F(TransmissionBucketTest, Fill) {
  EXPECT_TRUE(send_bucket_.Empty());
  send_bucket_.Fill(1, 100);
  EXPECT_FALSE(send_bucket_.Empty());
}

TEST_F(TransmissionBucketTest, Reset) {
  send_bucket_.Fill(1, 100);
  EXPECT_FALSE(send_bucket_.Empty());
  send_bucket_.Reset();
  EXPECT_TRUE(send_bucket_.Empty());
}

TEST_F(TransmissionBucketTest, GetNextPacket) {
  EXPECT_EQ(-1, send_bucket_.GetNextPacket());    // empty
  send_bucket_.Fill(1234, 100);
  EXPECT_EQ(1234, send_bucket_.GetNextPacket());  // first packet ok
  send_bucket_.Fill(1235, 100);
  EXPECT_EQ(-1, send_bucket_.GetNextPacket());    // packet does not fit
}

TEST_F(TransmissionBucketTest, UpdateBytesPerInterval) {
  const int delta_time_ms = 1;
  const int target_bitrate_kbps = 800;
  send_bucket_.UpdateBytesPerInterval(delta_time_ms, target_bitrate_kbps);

  send_bucket_.Fill(1234, 50);
  send_bucket_.Fill(1235, 50);
  send_bucket_.Fill(1236, 50);

  EXPECT_EQ(1234, send_bucket_.GetNextPacket());  // first packet ok
  EXPECT_EQ(1235, send_bucket_.GetNextPacket());  // ok
  EXPECT_EQ(1236, send_bucket_.GetNextPacket());  // ok
  EXPECT_TRUE(send_bucket_.Empty());

  send_bucket_.Fill(1237, 50);
  EXPECT_EQ(-1, send_bucket_.GetNextPacket());    // packet does not fit
}
}  // namespace webrtc
