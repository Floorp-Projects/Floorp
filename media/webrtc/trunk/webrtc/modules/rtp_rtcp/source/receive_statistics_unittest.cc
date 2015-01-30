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

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest() :
      clock_(0),
      receive_statistics_(ReceiveStatistics::Create(&clock_)) {
    memset(&header1_, 0, sizeof(header1_));
    header1_.ssrc = kSsrc1;
    header1_.sequenceNumber = 100;
    memset(&header2_, 0, sizeof(header2_));
    header2_.ssrc = kSsrc2;
    header2_.sequenceNumber = 100;
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

TEST_F(ReceiveStatisticsTest, RtcpCallbacks) {
  class TestCallback : public RtcpStatisticsCallback {
   public:
    TestCallback()
        : RtcpStatisticsCallback(), num_calls_(0), ssrc_(0), stats_() {}
    virtual ~TestCallback() {}

    virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                   uint32_t ssrc) {
      ssrc_ = ssrc;
      stats_ = statistics;
      ++num_calls_;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    RtcpStatistics stats_;
  } callback;

  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Add some arbitrary data, with loss and jitter.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber += 2;
  clock_.AdvanceTimeMilliseconds(9);
  header1_.timestamp += 9;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  --header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(13);
  header1_.timestamp += 47;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  header1_.sequenceNumber += 3;
  clock_.AdvanceTimeMilliseconds(11);
  header1_.timestamp += 17;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;

  EXPECT_EQ(0u, callback.num_calls_);

  // Call GetStatistics, simulating a timed rtcp sender thread.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  EXPECT_EQ(1u, callback.num_calls_);
  EXPECT_EQ(callback.ssrc_, kSsrc1);
  EXPECT_EQ(statistics.cumulative_lost, callback.stats_.cumulative_lost);
  EXPECT_EQ(statistics.extended_max_sequence_number,
            callback.stats_.extended_max_sequence_number);
  EXPECT_EQ(statistics.fraction_lost, callback.stats_.fraction_lost);
  EXPECT_EQ(statistics.jitter, callback.stats_.jitter);
  EXPECT_EQ(51, statistics.fraction_lost);
  EXPECT_EQ(1u, statistics.cumulative_lost);
  EXPECT_EQ(5u, statistics.extended_max_sequence_number);
  EXPECT_EQ(4u, statistics.jitter);

  receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  // Add some more data.
  header1_.sequenceNumber = 1;
  clock_.AdvanceTimeMilliseconds(7);
  header1_.timestamp += 3;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  header1_.sequenceNumber += 2;
  clock_.AdvanceTimeMilliseconds(9);
  header1_.timestamp += 9;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  --header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(13);
  header1_.timestamp += 47;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, true);
  header1_.sequenceNumber += 3;
  clock_.AdvanceTimeMilliseconds(11);
  header1_.timestamp += 17;
  receive_statistics_->IncomingPacket(header1_, kPacketSize1, false);
  ++header1_.sequenceNumber;

  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  // Should not have been called after deregister.
  EXPECT_EQ(1u, callback.num_calls_);
}

class RtpTestCallback : public StreamDataCountersCallback {
 public:
  RtpTestCallback()
      : StreamDataCountersCallback(), num_calls_(0), ssrc_(0), stats_() {}
  virtual ~RtpTestCallback() {}

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) {
    ssrc_ = ssrc;
    stats_ = counters;
    ++num_calls_;
  }

  void ExpectMatches(uint32_t num_calls,
                     uint32_t ssrc,
                     uint32_t bytes,
                     uint32_t padding,
                     uint32_t packets,
                     uint32_t retransmits,
                     uint32_t fec) {
    EXPECT_EQ(num_calls, num_calls_);
    EXPECT_EQ(ssrc, ssrc_);
    EXPECT_EQ(bytes, stats_.bytes);
    EXPECT_EQ(padding, stats_.padding_bytes);
    EXPECT_EQ(packets, stats_.packets);
    EXPECT_EQ(retransmits, stats_.retransmitted_packets);
    EXPECT_EQ(fec, stats_.fec_packets);
  }

  uint32_t num_calls_;
  uint32_t ssrc_;
  StreamDataCounters stats_;
};

TEST_F(ReceiveStatisticsTest, RtpCallbacks) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  const uint32_t kHeaderLength = 20;
  const uint32_t kPaddingLength = 9;

  // One packet of size kPacketSize1.
  header1_.headerLength = kHeaderLength;
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
  callback.ExpectMatches(1, kSsrc1, kPacketSize1, 0, 1, 0, 0);

  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  header1_.paddingLength = 9;
  // Another packet of size kPacketSize1 with 9 bytes padding.
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength + kPaddingLength, false);
  callback.ExpectMatches(2, kSsrc1, 2 * kPacketSize1, kPaddingLength, 2, 0, 0);

  clock_.AdvanceTimeMilliseconds(5);
  // Retransmit last packet.
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength + kPaddingLength, true);
  callback.ExpectMatches(
      3, kSsrc1, 3 * kPacketSize1, kPaddingLength * 2, 3, 1, 0);

  header1_.paddingLength = 0;
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  // One recovered packet.
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
  receive_statistics_->FecPacketReceived(kSsrc1);
  callback.ExpectMatches(
      5, kSsrc1, 4 * kPacketSize1, kPaddingLength * 2, 4, 1, 1);

  receive_statistics_->RegisterRtpStatisticsCallback(NULL);

  // New stats, but callback should not be called.
  ++header1_.sequenceNumber;
  clock_.AdvanceTimeMilliseconds(5);
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, true);
  callback.ExpectMatches(
      5, kSsrc1, 4 * kPacketSize1, kPaddingLength * 2, 4, 1, 1);
}

TEST_F(ReceiveStatisticsTest, RtpCallbacksFecFirst) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  const uint32_t kHeaderLength = 20;

  // If first packet is FEC, ignore it.
  receive_statistics_->FecPacketReceived(kSsrc1);
  EXPECT_EQ(0u, callback.num_calls_);

  header1_.headerLength = kHeaderLength;
  receive_statistics_->IncomingPacket(
      header1_, kPacketSize1 + kHeaderLength, false);
  callback.ExpectMatches(1, kSsrc1, kPacketSize1, 0, 1, 0, 0);

  receive_statistics_->FecPacketReceived(kSsrc1);
  callback.ExpectMatches(2, kSsrc1, kPacketSize1, 0, 1, 0, 1);
}
}  // namespace webrtc
