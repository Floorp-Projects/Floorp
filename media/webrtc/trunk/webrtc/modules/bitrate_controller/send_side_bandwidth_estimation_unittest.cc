/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"

namespace webrtc {

TEST(SendSideBweTest, InitialRembWithProbing) {
  SendSideBandwidthEstimation bwe;
  bwe.SetMinMaxBitrate(100000, 1500000);
  bwe.SetSendBitrate(200000);

  const int kRembBps = 1000000;
  const int kSecondRembBps = kRembBps + 500000;
  int64_t now_ms = 0;

  bwe.UpdateReceiverBlock(0, 50, 1, now_ms);

  // Initial REMB applies immediately.
  bwe.UpdateReceiverEstimate(kRembBps);
  bwe.UpdateEstimate(now_ms);
  int bitrate;
  uint8_t fraction_loss;
  int64_t rtt;
  bwe.CurrentEstimate(&bitrate, &fraction_loss, &rtt);
  EXPECT_EQ(kRembBps, bitrate);

  // Second REMB doesn't apply immediately.
  now_ms += 2001;
  bwe.UpdateReceiverEstimate(kSecondRembBps);
  bwe.UpdateEstimate(now_ms);
  bitrate = 0;
  bwe.CurrentEstimate(&bitrate, &fraction_loss, &rtt);
  EXPECT_EQ(kRembBps, bitrate);
}
}  // namespace webrtc
