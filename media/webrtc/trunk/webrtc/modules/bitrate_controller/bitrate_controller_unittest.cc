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

#include <algorithm>
#include <vector>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

using webrtc::RtcpBandwidthObserver;
using webrtc::BitrateObserver;
using webrtc::BitrateController;

class TestBitrateObserver: public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_(0),
        last_fraction_loss_(0),
        last_rtt_(0) {
  }

  virtual void OnNetworkChanged(const uint32_t bitrate,
                                const uint8_t fraction_loss,
                                const uint32_t rtt) {
    last_bitrate_ = bitrate;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ = rtt;
  }
  uint32_t last_bitrate_;
  uint8_t last_fraction_loss_;
  uint32_t last_rtt_;
};

class BitrateControllerTest : public ::testing::Test {
 protected:
  BitrateControllerTest() {
  }
  ~BitrateControllerTest() {}

  virtual void SetUp() {
    controller_ = BitrateController::CreateBitrateController();
    bandwidth_observer_ = controller_->CreateRtcpBandwidthObserver();
  }

  virtual void TearDown() {
    delete bandwidth_observer_;
    delete controller_;
  }
  BitrateController* controller_;
  RtcpBandwidthObserver* bandwidth_observer_;
};

TEST_F(BitrateControllerTest, Basic) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);
  controller_->RemoveBitrateObserver(&bitrate_observer);
}

TEST_F(BitrateControllerTest, OneBitrateObserverOneRtcpObserver) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test start bitrate.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer.last_rtt_);

  // Test bitrate increase 8% per second.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 1001);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 801, 4001);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  // Test that a low REMB trigger immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(250000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000u, bitrate_observer.last_bitrate_);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer);
}

TEST_F(BitrateControllerTest, OneBitrateObserverTwoRtcpObservers) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  RtcpBandwidthObserver* second_bandwidth_observer =
      controller_->CreateRtcpBandwidthObserver();

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test start bitrate.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 1, 1);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer.last_rtt_);

  // Test bitrate increase 8% per second.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 501);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 21, 1001);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(100u, bitrate_observer.last_rtt_);

  // Extra report should not change estimate.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 31, 1501);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  // Second report should not change estimate.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 100, 41, 2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  // Reports from only one bandwidth observer is ok.
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 81, 4001);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  second_bandwidth_observer->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  // Test that a low REMB trigger immediately.
  // We don't care which bandwidth observer that delivers the REMB.
  second_bandwidth_observer->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(250000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000u, bitrate_observer.last_bitrate_);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer);
  delete second_bandwidth_observer;
}

TEST_F(BitrateControllerTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  controller_->SetBitrateObserver(&bitrate_observer_2, 200000, 200000, 300000);
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 300000);

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test too low start bitrate, hence lower than sum of min.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 1, 1);

  // Test bitrate increase 8% per second, distributed equally.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 21, 1001);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_1.last_rtt_);

  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_2.last_rtt_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 41, 2001);
  EXPECT_EQ(112500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(212500u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 61, 3001);
  EXPECT_EQ(126000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(226000u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 81, 4001);
  EXPECT_EQ(140580u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(240580u, bitrate_observer_2.last_bitrate_);

  // Check that the bitrate sum honor our REMB.
  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 101, 5001);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(250000u, bitrate_observer_2.last_bitrate_);

  // Remove REMB cap, higher than sum of max.
  bandwidth_observer_->OnReceivedEstimatedBitrate(700000);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 121, 6001);
  EXPECT_EQ(166500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(266500u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 141, 7001);
  EXPECT_EQ(184320u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(284320u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 161, 8001);
  EXPECT_EQ(207130u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);  // Max cap.

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 181, 9001);
  EXPECT_EQ(248700u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 201, 10001);
  EXPECT_EQ(293596u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);

  bandwidth_observer_->OnReceivedRtcpReceiverReport(1, 0, 50, 221, 11001);
  EXPECT_EQ(300000u, bitrate_observer_1.last_bitrate_);  // Max cap.
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);

  // Test that a low REMB trigger immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(350000);
  EXPECT_EQ(125000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_1.last_rtt_);
  EXPECT_EQ(225000u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_2.last_rtt_);

  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);  // Min cap.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);  // Min cap.
  controller_->RemoveBitrateObserver(&bitrate_observer_1);
  controller_->RemoveBitrateObserver(&bitrate_observer_2);
}
