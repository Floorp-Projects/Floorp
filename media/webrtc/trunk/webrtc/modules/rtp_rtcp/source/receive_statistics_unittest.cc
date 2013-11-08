/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

const int kPacketSize1 = 100;
const int kPacketSize2 = 300;
const uint32_t kSsrc1 = 1;
const uint32_t kSsrc2 = 2;
const uint32_t kSsrc3 = 3;

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest() :
      clock_(0),
      receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    memset(&header1_, 0, sizeof(header1_));
    header1_.ssrc = kSsrc1;
    header1_.sequenceNumber = 0;
    memset(&header2_, 0, sizeof(header2_));
    header2_.ssrc = kSsrc2;
    header2_.sequenceNumber = 0;
  }

 protected:
  SimulatedClock clock_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;
  RTPHeader header1_;
  RTPHeader header2_;
};

TEST_F(ReceiveStatisticsTest, TwoIncomingSsrcs) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(100);
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  uint32_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  statistician =
      receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(600u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(2u, statisticians.size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;

  statisticians[kSsrc1]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  statisticians[kSsrc2]->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);

  receive_statistics_->GetStatistician(kSsrc1)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  receive_statistics_->GetStatistician(kSsrc2)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);
}

TEST_F(ReceiveStatisticsTest, ActiveStatisticians) {
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->IncomingPacket(header2_, kPacketSize2, false);
  ++header2_.sequenceNumber;
  StatisticianMap statisticians = receive_statistics_->GetActiveStatisticians();
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(0u, statisticians.size());

  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;
  // kSsrc1 should be active again and the data counters should have survived.
  statisticians = receive_statistics_->GetActiveStatisticians();
  EXPECT_EQ(1u, statisticians.size());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  uint32_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);
}
}  // namespace webrtc
