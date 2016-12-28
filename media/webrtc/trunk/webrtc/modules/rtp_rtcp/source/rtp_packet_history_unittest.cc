/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * This file includes unit tests for the RTPPacketHistory.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpPacketHistoryTest : public ::testing::Test {
 protected:
  RtpPacketHistoryTest()
     : fake_clock_(123456),
       hist_(new RTPPacketHistory(&fake_clock_)) {
  }
  ~RtpPacketHistoryTest() {
    delete hist_;
  }

  SimulatedClock fake_clock_;
  RTPPacketHistory* hist_;
  enum {kPayload = 127};
  enum {kSsrc = 12345678};
  enum {kSeqNum = 88};
  enum {kTimestamp = 127};
  enum {kMaxPacketLength = 1500};
  uint8_t packet_[kMaxPacketLength];
  uint8_t packet_out_[kMaxPacketLength];

  void CreateRtpPacket(uint16_t seq_num, uint32_t ssrc, uint8_t payload,
      uint32_t timestamp, uint8_t* array, size_t* cur_pos) {
    array[(*cur_pos)++] = 0x80;
    array[(*cur_pos)++] = payload;
    array[(*cur_pos)++] = seq_num >> 8;
    array[(*cur_pos)++] = seq_num;
    array[(*cur_pos)++] = timestamp >> 24;
    array[(*cur_pos)++] = timestamp >> 16;
    array[(*cur_pos)++] = timestamp >> 8;
    array[(*cur_pos)++] = timestamp;
    array[(*cur_pos)++] = ssrc >> 24;
    array[(*cur_pos)++] = ssrc >> 16;
    array[(*cur_pos)++] = ssrc >> 8;
    array[(*cur_pos)++] = ssrc;
  }
};

TEST_F(RtpPacketHistoryTest, SetStoreStatus) {
  EXPECT_FALSE(hist_->StorePackets());
  hist_->SetStorePacketsStatus(true, 10);
  EXPECT_TRUE(hist_->StorePackets());
  hist_->SetStorePacketsStatus(false, 0);
  EXPECT_FALSE(hist_->StorePackets());
}

TEST_F(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_FALSE(hist_->StorePackets());
  size_t len = 0;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                   kAllowRetransmission));
  // Packet should not be stored.
  len = kMaxPacketLength;
  int64_t time;
  EXPECT_FALSE(hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_, &len,
                                              &time));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket_TooLargePacketLength) {
  hist_->SetStorePacketsStatus(true, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_EQ(-1, hist_->PutRTPPacket(packet_, kMaxPacketLength + 1,
                                    capture_time_ms, kAllowRetransmission));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len = kMaxPacketLength;
  int64_t time;
  EXPECT_FALSE(hist_->GetPacketAndSetSendTime(0, 0, false, packet_, &len,
                                              &time));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);

  EXPECT_FALSE(hist_->HasRTPPacket(kSeqNum));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                   kAllowRetransmission));
  EXPECT_TRUE(hist_->HasRTPPacket(kSeqNum));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  int64_t capture_time_ms = 1;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                   kAllowRetransmission));

  size_t len_out = kMaxPacketLength;
  int64_t time;
  EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_out_,
                                             &len_out, &time));
  EXPECT_EQ(len, len_out);
  EXPECT_EQ(capture_time_ms, time);
  for (size_t i = 0; i < len; i++)  {
    EXPECT_EQ(packet_[i], packet_out_[i]);
  }
}

TEST_F(RtpPacketHistoryTest, NoCaptureTime) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, -1, kAllowRetransmission));

  size_t len_out = kMaxPacketLength;
  int64_t time;
  EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_out_,
                                             &len_out, &time));
  EXPECT_EQ(len, len_out);
  EXPECT_EQ(capture_time_ms, time);
  for (size_t i = 0; i < len; i++)  {
    EXPECT_EQ(packet_[i], packet_out_[i]);
  }
}

TEST_F(RtpPacketHistoryTest, DontRetransmit) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(
      0, hist_->PutRTPPacket(packet_, len, capture_time_ms, kDontRetransmit));

  size_t len_out = kMaxPacketLength;
  int64_t time;
  EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_out_,
                                             &len_out, &time));
  EXPECT_EQ(len, len_out);
  EXPECT_EQ(capture_time_ms, time);
}

TEST_F(RtpPacketHistoryTest, MinResendTime) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                   kAllowRetransmission));

  // First transmission: TimeToSendPacket() call from pacer.
  int64_t time;
  len = kMaxPacketLength;
  EXPECT_TRUE(
      hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_, &len, &time));

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs);
  // Time has elapsed.
  len = kMaxPacketLength;
  EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs,
                                             true, packet_, &len, &time));
  EXPECT_GT(len, 0u);
  EXPECT_EQ(capture_time_ms, time);

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed. Packet should be found, but no bytes copied.
  len = kMaxPacketLength;
  EXPECT_FALSE(hist_->GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs,
                                              true, packet_, &len, &time));
}

TEST_F(RtpPacketHistoryTest, EarlyFirstResend) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_->SetStorePacketsStatus(true, 10);
  size_t len = 0;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                   kAllowRetransmission));

  // First transmission: TimeToSendPacket() call from pacer.
  int64_t time;
  len = kMaxPacketLength;
  EXPECT_TRUE(
      hist_->GetPacketAndSetSendTime(kSeqNum, 0, false, packet_, &len, &time));

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed, but this is the first retransmission request so
  // allow anyway.
  len = kMaxPacketLength;
  EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs,
                                             true, packet_, &len, &time));
  EXPECT_GT(len, 0u);
  EXPECT_EQ(capture_time_ms, time);

  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  // Time has not elapsed. Packet should be found, but no bytes copied.
  len = kMaxPacketLength;
  EXPECT_FALSE(hist_->GetPacketAndSetSendTime(kSeqNum, kMinRetransmitIntervalMs,
                                              true, packet_, &len, &time));
}

TEST_F(RtpPacketHistoryTest, DynamicExpansion) {
  hist_->SetStorePacketsStatus(true, 10);
  size_t len;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int64_t time;

  // Add 4 packets, and then send them.
  for (int i = 0; i < 4; ++i) {
    len = 0;
    CreateRtpPacket(kSeqNum + i, kSsrc, kPayload, kTimestamp, packet_, &len);
    EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                     kAllowRetransmission));
  }
  for (int i = 0; i < 4; ++i) {
    len = kMaxPacketLength;
    EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum + i, 100, false, packet_,
                                               &len, &time));
  }
  capture_time_ms += 33;

  // Add 16 packets, and then send them. History should expand to make this
  // work.
  for (int i = 4; i < 20; ++i) {
    len = 0;
    CreateRtpPacket(kSeqNum + i, kSsrc, kPayload, kTimestamp, packet_, &len);
    EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                     kAllowRetransmission));
  }
  for (int i = 4; i < 20; ++i) {
    len = kMaxPacketLength;
    EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum + i, 100, false, packet_,
                                               &len, &time));
  }

  fake_clock_.AdvanceTimeMilliseconds(100);

  // Retransmit last 16 packets.
  for (int i = 4; i < 20; ++i) {
    len = kMaxPacketLength;
    EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum + i, 100, false, packet_,
                                               &len, &time));
  }
}

TEST_F(RtpPacketHistoryTest, FullExpansion) {
  static const int kSendSidePacketHistorySize = 600;
  hist_->SetStorePacketsStatus(true, kSendSidePacketHistorySize);
  size_t len;
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int64_t time;
  for (size_t i = 0; i < kMaxHistoryCapacity + 1; ++i) {
    len = 0;
    CreateRtpPacket(kSeqNum + i, kSsrc, kPayload, kTimestamp, packet_, &len);
    EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, capture_time_ms,
                                     kAllowRetransmission));
  }

  fake_clock_.AdvanceTimeMilliseconds(100);

  // Retransmit all packets currently in buffer.
  for (size_t i = 1; i < kMaxHistoryCapacity + 1; ++i) {
    len = kMaxPacketLength;
    EXPECT_TRUE(hist_->GetPacketAndSetSendTime(kSeqNum + i, 100, false, packet_,
                                               &len, &time));
  }
}

}  // namespace webrtc
