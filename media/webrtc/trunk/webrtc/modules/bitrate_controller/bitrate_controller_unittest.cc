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

#include <algorithm>
#include <vector>

#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

using webrtc::RtcpBandwidthObserver;
using webrtc::BitrateObserver;
using webrtc::BitrateController;

uint8_t WeightedLoss(int num_packets1, uint8_t fraction_loss1,
                     int num_packets2, uint8_t fraction_loss2) {
  int weighted_sum = num_packets1 * fraction_loss1 +
      num_packets2 * fraction_loss2;
  int total_num_packets = num_packets1 + num_packets2;
  return (weighted_sum + total_num_packets / 2) / total_num_packets;
}

webrtc::RTCPReportBlock CreateReportBlock(
    uint32_t remote_ssrc, uint32_t source_ssrc,
    uint8_t fraction_lost, uint32_t extended_high_sequence_number) {
  return webrtc::RTCPReportBlock(remote_ssrc, source_ssrc, fraction_lost, 0,
                                 extended_high_sequence_number, 0, 0, 0);
}

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
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer.last_rtt_);

  // Test bitrate increase 8% per second.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1001);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 3001);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 801));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 4001);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 5001);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 6001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 7001);
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
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, 1);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer.last_rtt_);

  // Test bitrate increase 8% per second.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 501);
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 100,
                                                          1001);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(100u, bitrate_observer.last_rtt_);

  // Extra report should not change estimate.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 31));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 100,
                                                          1501);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  // Second report should not change estimate.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 100,
                                                          2001);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);

  // Reports from only one bandwidth observer is ok.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                          3001);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 81));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                          4001);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                          5001);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                          6001);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);  // Max cap.

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                          7001);
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

TEST_F(BitrateControllerTest, OneBitrateObserverMultipleReportBlocks) {
  TestBitrateObserver bitrate_observer;
  uint32_t sequence_number[2] = {0, 0xFF00};
  const uint32_t kStartBitrate = 200000;
  const uint32_t kMinBitrate = 100000;
  const uint32_t kMaxBitrate = 300000;
  controller_->SetBitrateObserver(&bitrate_observer, kStartBitrate, kMinBitrate,
                                  kMaxBitrate);

  // Receive a high REMB, test bitrate increase.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  webrtc::ReportBlockList report_blocks;
  int64_t time_ms = 1001;
  uint32_t last_bitrate = 0;
  // Ramp up to max bitrate.
  for (int i = 0; i < 6; ++i) {
    report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
    report_blocks.push_back(CreateReportBlock(1, 3, 0, sequence_number[1]));
    bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50,
                                                      time_ms);
    EXPECT_GT(bitrate_observer.last_bitrate_, last_bitrate);
    EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
    EXPECT_EQ(50u, bitrate_observer.last_rtt_);
    last_bitrate = bitrate_observer.last_bitrate_;
    time_ms += 1000;
    sequence_number[0] += 20;
    sequence_number[1] += 1;
    report_blocks.clear();
  }

  EXPECT_EQ(kMaxBitrate, bitrate_observer.last_bitrate_);

  // Packet loss on the first stream. Verify that bitrate decreases.
  report_blocks.push_back(CreateReportBlock(1, 2, 50, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 0, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_LT(bitrate_observer.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 50, 1, 0), bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);
  last_bitrate = bitrate_observer.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 20;
  time_ms += 1000;
  report_blocks.clear();

  // Packet loss on the second stream. Verify that bitrate decreases.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 75, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_LT(bitrate_observer.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 0, 20, 75), bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);
  last_bitrate = bitrate_observer.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 1;
  time_ms += 1000;
  report_blocks.clear();

  // All packets lost on stream with few packets, no back-off.
  report_blocks.push_back(CreateReportBlock(1, 2, 1, sequence_number[0]));
  report_blocks.push_back(CreateReportBlock(1, 3, 255, sequence_number[1]));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(bitrate_observer.last_bitrate_, last_bitrate);
  EXPECT_EQ(WeightedLoss(20, 1, 1, 255), bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);
  last_bitrate = bitrate_observer.last_bitrate_;
  sequence_number[0] += 20;
  sequence_number[1] += 1;
  report_blocks.clear();
}

TEST_F(BitrateControllerTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  controller_->SetBitrateObserver(&bitrate_observer_2, 200000, 200000, 300000);
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 300000);

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test too low start bitrate, hence lower than sum of min.
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1);

  // Test bitrate increase 8% per second, distributed equally.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1001);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_1.last_rtt_);

  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_2.last_rtt_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 2001);
  EXPECT_EQ(112500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(212500u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 3001);
  EXPECT_EQ(126000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(226000u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 81));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 4001);
  EXPECT_EQ(140580u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(240580u, bitrate_observer_2.last_bitrate_);

  // Check that the bitrate sum honor our REMB.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 5001);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(250000u, bitrate_observer_2.last_bitrate_);

  // Remove REMB cap, higher than sum of max.
  bandwidth_observer_->OnReceivedEstimatedBitrate(700000);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 6001);
  EXPECT_EQ(166500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(266500u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 7001);
  EXPECT_EQ(184320u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(284320u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 161));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 8001);
  EXPECT_EQ(207130u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);  // Max cap.

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 181));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 9001);
  EXPECT_EQ(248700u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 201));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 10001);
  EXPECT_EQ(293596u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 221));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 11001);
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
