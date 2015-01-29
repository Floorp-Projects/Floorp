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
  BitrateControllerTest() : clock_(0), enforce_min_bitrate_(true) {}
  ~BitrateControllerTest() {}

  virtual void SetUp() {
    controller_ = BitrateController::CreateBitrateController(
        &clock_, enforce_min_bitrate_);
    bandwidth_observer_ = controller_->CreateRtcpBandwidthObserver();
  }

  virtual void TearDown() {
    delete bandwidth_observer_;
    delete controller_;
  }

  webrtc::SimulatedClock clock_;
  bool enforce_min_bitrate_;
  BitrateController* controller_;
  RtcpBandwidthObserver* bandwidth_observer_;
};

TEST_F(BitrateControllerTest, Basic) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);
  controller_->RemoveBitrateObserver(&bitrate_observer);
}

TEST_F(BitrateControllerTest, InitialRemb) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 1500000);
  const uint32_t kRemb = 1000000u;
  const uint32_t kSecondRemb = kRemb + 500000u;

  // Initial REMB applies immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(kRemb);
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 1);
  report_blocks.clear();
  EXPECT_EQ(kRemb, bitrate_observer.last_bitrate_);

  // Second REMB doesn't apply immediately.
  bandwidth_observer_->OnReceivedEstimatedBitrate(kRemb + 500000);
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, 2001);
  EXPECT_LT(bitrate_observer.last_bitrate_, kSecondRemb);
}

TEST_F(BitrateControllerTest, UpdatingBitrateObserver) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 1500000);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);

  controller_->SetBitrateObserver(&bitrate_observer, 1500000, 100000, 1500000);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_);

  controller_->SetBitrateObserver(&bitrate_observer, 500000, 100000, 1500000);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_);
}

TEST_F(BitrateControllerTest, OneBitrateObserverOneRtcpObserver) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  // First REMB applies immediately.
  int64_t time_ms = 1001;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer.last_rtt_);
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 2000;

  // Receive a high remb, test bitrate inc.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test bitrate increase 8% per second.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 81));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 801));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  // Reach max cap.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);

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

  // REMBs during the first 2 seconds apply immediately.
  int64_t time_ms = 1;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 500;

  RtcpBandwidthObserver* second_bandwidth_observer =
      controller_->CreateRtcpBandwidthObserver();

  // Test start bitrate.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, 1);
  EXPECT_EQ(217000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(100u, bitrate_observer.last_rtt_);
  time_ms += 500;

  // Test bitrate increase 8% per second.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  time_ms += 500;
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(100u, bitrate_observer.last_rtt_);
  time_ms += 500;

  // Extra report should not change estimate.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 31));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(235360u, bitrate_observer.last_bitrate_);
  time_ms += 500;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);

  // Second report should not change estimate.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 100, time_ms);
  EXPECT_EQ(255189u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  // Reports from only one bandwidth observer is ok.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(276604u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 81));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(299732u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  // Reach max cap.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  second_bandwidth_observer->OnReceivedRtcpReceiverReport(
      report_blocks, 50, time_ms);
  EXPECT_EQ(300000u, bitrate_observer.last_bitrate_);

  // Test that a low REMB trigger immediately.
  // We don't care which bandwidth observer that delivers the REMB.
  second_bandwidth_observer->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(250000u, bitrate_observer.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer.last_rtt_);

  // Min cap.
  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000u, bitrate_observer.last_bitrate_);
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

  // REMBs during the first 2 seconds apply immediately.
  int64_t time_ms = 1001;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, sequence_number[0]));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kStartBitrate);
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 2000;

  // Receive a high REMB, test bitrate increase.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

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

  // REMBs during the first 2 seconds apply immediately.
  int64_t time_ms = 1001;
  webrtc::ReportBlockList report_blocks;
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 1));
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(0u, bitrate_observer_1.last_rtt_);
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  report_blocks.clear();
  time_ms += 2000;

  // Receive a high remb, test bitrate inc.
  // Test too low start bitrate, hence lower than sum of min.
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);

  // Test bitrate increase 8% per second, distributed equally.
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 21));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(112500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_1.last_rtt_);
  time_ms += 1000;

  EXPECT_EQ(212500u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50u, bitrate_observer_2.last_rtt_);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 41));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(126000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(226000u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 61));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(140580u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(240580u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  // Check that the bitrate sum honor our REMB.
  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 101));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(250000u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  // Remove REMB cap, higher than sum of max.
  bandwidth_observer_->OnReceivedEstimatedBitrate(700000);

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 121));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(166500u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(266500u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 141));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(184320u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(284320u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 161));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(207130u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);  // Max cap.
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 181));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(248700u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 201));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
  EXPECT_EQ(293596u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_);
  time_ms += 1000;

  report_blocks.clear();
  report_blocks.push_back(CreateReportBlock(1, 2, 0, 221));
  bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, 50, time_ms);
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

TEST_F(BitrateControllerTest, SetReservedBitrate) {
  TestBitrateObserver bitrate_observer;
  controller_->SetBitrateObserver(&bitrate_observer, 200000, 100000, 300000);

  // Receive successively lower REMBs, verify the reserved bitrate is deducted.

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(400000);
  EXPECT_EQ(150000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(250000);
  EXPECT_EQ(150000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(30000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(200000);
  EXPECT_EQ(170000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(160000);
  EXPECT_EQ(160000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(30000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(160000);
  EXPECT_EQ(130000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(120000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(10000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(110000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(0);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(120000u, bitrate_observer.last_bitrate_);
  controller_->SetReservedBitrate(50000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(120000);
  EXPECT_EQ(100000u, bitrate_observer.last_bitrate_);

  controller_->SetReservedBitrate(10000);
  bandwidth_observer_->OnReceivedEstimatedBitrate(0);
  EXPECT_EQ(100000u, bitrate_observer.last_bitrate_);

  controller_->RemoveBitrateObserver(&bitrate_observer);
}

class BitrateControllerTestNoEnforceMin : public BitrateControllerTest {
 protected:
  BitrateControllerTestNoEnforceMin() : BitrateControllerTest() {
    enforce_min_bitrate_ = false;
  }
};

// The following three tests verify that the EnforceMinBitrate() method works
// as intended.
TEST_F(BitrateControllerTestNoEnforceMin, OneBitrateObserver) {
  TestBitrateObserver bitrate_observer_1;
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 400000);

  // High REMB.
  bandwidth_observer_->OnReceivedEstimatedBitrate(150000);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_);

  // Low REMB.
  bandwidth_observer_->OnReceivedEstimatedBitrate(10000);
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);

  // Keeps at least 10 kbps.
  bandwidth_observer_->OnReceivedEstimatedBitrate(9000);
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);

  controller_->RemoveBitrateObserver(&bitrate_observer_1);
}

TEST_F(BitrateControllerTestNoEnforceMin, SetReservedBitrate) {
  TestBitrateObserver bitrate_observer_1;
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 400000);
  controller_->SetReservedBitrate(10000);

  // High REMB.
  bandwidth_observer_->OnReceivedEstimatedBitrate(150000);
  EXPECT_EQ(140000u, bitrate_observer_1.last_bitrate_);

  // Low REMB.
  bandwidth_observer_->OnReceivedEstimatedBitrate(15000);
  EXPECT_EQ(5000u, bitrate_observer_1.last_bitrate_);

  // Keeps at least 10 kbps.
  bandwidth_observer_->OnReceivedEstimatedBitrate(9000);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_);

  controller_->RemoveBitrateObserver(&bitrate_observer_1);
}

TEST_F(BitrateControllerTestNoEnforceMin, ThreeBitrateObservers) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  // Set up the observers with min bitrates at 100000, 200000, and 300000.
  // Note: The start bitrate of bitrate_observer_1 (700000) is used as the
  // overall start bitrate.
  controller_->SetBitrateObserver(&bitrate_observer_1, 700000, 100000, 400000);
  controller_->SetBitrateObserver(&bitrate_observer_2, 200000, 200000, 400000);
  controller_->SetBitrateObserver(&bitrate_observer_3, 200000, 300000, 400000);

  // High REMB. Make sure the controllers get a fair share of the surplus
  // (i.e., what is left after each controller gets its min rate).
  bandwidth_observer_->OnReceivedEstimatedBitrate(690000);
  // Verify that each observer gets its min rate (sum of min rates is 600000),
  // and that the remaining 90000 is divided equally among the three.
  EXPECT_EQ(130000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(230000u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(330000u, bitrate_observer_3.last_bitrate_);

  // High REMB, but below the sum of min bitrates.
  bandwidth_observer_->OnReceivedEstimatedBitrate(500000);
  // Verify that the first and second observers get their min bitrates, and the
  // third gets the remainder.
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_3.last_bitrate_);  // Remainder.

  // Low REMB.
  bandwidth_observer_->OnReceivedEstimatedBitrate(10000);
  // Verify that the first observer gets all the rate, and the rest get zero.
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_);

  // Verify it keeps an estimate of at least 10kbps.
  bandwidth_observer_->OnReceivedEstimatedBitrate(9000);
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_);

  controller_->RemoveBitrateObserver(&bitrate_observer_1);
  controller_->RemoveBitrateObserver(&bitrate_observer_2);
  controller_->RemoveBitrateObserver(&bitrate_observer_3);
}

TEST_F(BitrateControllerTest, ThreeBitrateObserversLowRembEnforceMin) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  controller_->SetBitrateObserver(&bitrate_observer_1, 200000, 100000, 300000);
  controller_->SetBitrateObserver(&bitrate_observer_2, 200000, 200000, 300000);
  controller_->SetBitrateObserver(&bitrate_observer_3, 200000, 300000, 300000);

  // Low REMB. Verify that all observers still get their respective min bitrate.
  bandwidth_observer_->OnReceivedEstimatedBitrate(1000);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);  // Min cap.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);  // Min cap.
  EXPECT_EQ(300000u, bitrate_observer_3.last_bitrate_);  // Min cap.

  controller_->RemoveBitrateObserver(&bitrate_observer_1);
  controller_->RemoveBitrateObserver(&bitrate_observer_2);
  controller_->RemoveBitrateObserver(&bitrate_observer_3);
}
