/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/voice_engine/network_predictor.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace voe {

class TestNetworkPredictor : public ::testing::Test {
 protected:
  TestNetworkPredictor()
      : clock_(0),
        network_predictor_(new NetworkPredictor(&clock_)) {}
  SimulatedClock clock_;
  rtc::scoped_ptr<NetworkPredictor> network_predictor_;
};

TEST_F(TestNetworkPredictor, TestPacketLossRateFilter) {
  // Test initial packet loss rate estimate is 0.
  EXPECT_EQ(0, network_predictor_->GetLossRate());
  network_predictor_->UpdatePacketLossRate(32);
  // First time, no filtering.
  EXPECT_EQ(32, network_predictor_->GetLossRate());
  clock_.AdvanceTimeMilliseconds(1000);
  network_predictor_->UpdatePacketLossRate(40);
  float exp = pow(0.9999f, 1000);
  float value = 32.0f * exp + (1 - exp) * 40.0f;
  EXPECT_EQ(static_cast<uint8_t>(value + 0.5f),
            network_predictor_->GetLossRate());
}
}  // namespace voe
}  // namespace webrtc
