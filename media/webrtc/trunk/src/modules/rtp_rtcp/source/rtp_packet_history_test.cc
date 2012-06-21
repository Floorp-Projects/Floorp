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
 * This file includes unit tests for the RTPPacketHistory.
 */

#include <gtest/gtest.h>

#include "rtp_packet_history.h"
#include "rtp_rtcp_defines.h"
#include "typedefs.h"

namespace webrtc {

class FakeClock : public RtpRtcpClock {
 public:
  FakeClock() {
    time_in_ms_ = 123456;
  }
  // Return a timestamp in milliseconds relative to some arbitrary
  // source; the source is fixed for this clock.
  virtual WebRtc_UWord32 GetTimeInMS() {
    return time_in_ms_;
  }
  // Retrieve an NTP absolute timestamp.
  virtual void CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac) {
    secs = time_in_ms_ / 1000;
    frac = (time_in_ms_ % 1000) * 4294967;
  }
  void IncrementTime(WebRtc_UWord32 time_increment_ms) {
    time_in_ms_ += time_increment_ms;
  }
 private:
  WebRtc_UWord32 time_in_ms_;
};

class RtpPacketHistoryTest : public ::testing::Test {
 protected:
  RtpPacketHistoryTest()
     : hist_(new RTPPacketHistory(&fake_clock_)) {
  }
  ~RtpPacketHistoryTest() {
    delete hist_;
  }
  
  FakeClock fake_clock_;
  RTPPacketHistory* hist_;
  enum {kPayload = 127};
  enum {kSsrc = 12345678};
  enum {kSeqNum = 88};
  enum {kTimestamp = 127};
  enum {kMaxPacketLength = 1500};
  uint8_t packet_[kMaxPacketLength];
  uint8_t packet_out_[kMaxPacketLength];

  void CreateRtpPacket(uint16_t seq_num, uint32_t ssrc, uint8_t payload,
      uint32_t timestamp, uint8_t* array, uint16_t* cur_pos) {
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
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kAllowRetransmission));
  // Packet should not be stored.
  len = kMaxPacketLength;
  uint32_t time;
  StorageType type;
  EXPECT_FALSE(hist_->GetRTPPacket(kSeqNum, 0, packet_, &len, &time, &type));
}

TEST_F(RtpPacketHistoryTest, DontStore) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength, kDontStore));

  // Packet should not be stored.
  len = kMaxPacketLength;
  uint32_t time;
  StorageType type;
  EXPECT_FALSE(hist_->GetRTPPacket(kSeqNum, 0, packet_, &len, &time, &type));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket_TooLargePacketLength) {
  hist_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(-1, hist_->PutRTPPacket(packet_,
                                    kMaxPacketLength + 1,
                                    kMaxPacketLength,
                                    kAllowRetransmission));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_TooSmallBuffer) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kAllowRetransmission));
  uint16_t len_out = len - 1;
  uint32_t time;
  StorageType type;
  EXPECT_FALSE(hist_->GetRTPPacket(kSeqNum, 0, packet_, &len_out, &time,
                                   &type));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = kMaxPacketLength;
  uint32_t time;
  StorageType type;
  EXPECT_FALSE(hist_->GetRTPPacket(0, 0, packet_, &len, &time, &type));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);

  EXPECT_FALSE(hist_->HasRTPPacket(kSeqNum));
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kAllowRetransmission));
  EXPECT_TRUE(hist_->HasRTPPacket(kSeqNum));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kAllowRetransmission));

  uint16_t len_out = kMaxPacketLength;
  uint32_t time;
  StorageType type;
  EXPECT_TRUE(hist_->GetRTPPacket(kSeqNum, 0, packet_out_, &len_out, &time,
                                  &type));
  EXPECT_EQ(len, len_out);
  EXPECT_EQ(kAllowRetransmission, type);
  for (int i = 0; i < len; i++)  {
    EXPECT_EQ(packet_[i], packet_out_[i]);
  }
}

TEST_F(RtpPacketHistoryTest, DontRetransmit) {
  hist_->SetStorePacketsStatus(true, 10);
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kDontRetransmit));

  uint16_t len_out = kMaxPacketLength;
  uint32_t time;
  StorageType type;
  EXPECT_TRUE(hist_->GetRTPPacket(kSeqNum, 0, packet_out_, &len_out, &time,
                                  &type));
  EXPECT_EQ(len, len_out);
  EXPECT_EQ(kDontRetransmit, type);
}

TEST_F(RtpPacketHistoryTest, MinResendTime) {
  hist_->SetStorePacketsStatus(true, 10);
  WebRtc_UWord32 store_time = fake_clock_.GetTimeInMS();
  uint16_t len = 0;
  CreateRtpPacket(kSeqNum, kSsrc, kPayload, kTimestamp, packet_, &len);
  EXPECT_EQ(0, hist_->PutRTPPacket(packet_, len, kMaxPacketLength,
                                   kAllowRetransmission));

  hist_->UpdateResendTime(kSeqNum);
  fake_clock_.IncrementTime(100);

  // Time has elapsed.
  len = kMaxPacketLength;
  StorageType type;
  uint32_t time;
  EXPECT_TRUE(hist_->GetRTPPacket(kSeqNum, 100, packet_, &len, &time, &type));
  EXPECT_GT(len, 0);
  EXPECT_EQ(store_time, time);

  // Time has not elapsed. Packet should be found, but no bytes copied.
  len = kMaxPacketLength;
  EXPECT_TRUE(hist_->GetRTPPacket(kSeqNum, 101, packet_, &len, &time, &type));
  EXPECT_EQ(0, len);
}
}  // namespace webrtc
