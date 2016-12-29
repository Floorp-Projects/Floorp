/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

using webrtc::ForwardErrorCorrection;

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
const uint8_t kTransportOverhead = 28;

// Maximum number of media packets used in the FEC (RFC 5109).
const uint8_t kMaxNumberMediaPackets = ForwardErrorCorrection::kMaxMediaPackets;

typedef std::list<ForwardErrorCorrection::Packet*> PacketList;
typedef std::list<ForwardErrorCorrection::ReceivedPacket*> ReceivedPacketList;
typedef std::list<ForwardErrorCorrection::RecoveredPacket*> RecoveredPacketList;

template <typename T> void ClearList(std::list<T*>* my_list) {
  T* packet = NULL;
  while (!my_list->empty()) {
    packet = my_list->front();
    delete packet;
    my_list->pop_front();
  }
}

class RtpFecTest : public ::testing::Test {
 protected:
  RtpFecTest()
      : random_(0xfec133700742),
        fec_(new ForwardErrorCorrection()),
        ssrc_(random_.Rand<uint32_t>()),
        fec_seq_num_(0) {}

  webrtc::Random random_;
  ForwardErrorCorrection* fec_;
  int ssrc_;
  uint16_t fec_seq_num_;

  PacketList media_packet_list_;
  PacketList fec_packet_list_;
  ReceivedPacketList received_packet_list_;
  RecoveredPacketList recovered_packet_list_;

  // Media packet "i" is lost if media_loss_mask_[i] = 1,
  // received if media_loss_mask_[i] = 0.
  int media_loss_mask_[kMaxNumberMediaPackets];

  // FEC packet "i" is lost if fec_loss_mask_[i] = 1,
  // received if fec_loss_mask_[i] = 0.
  int fec_loss_mask_[kMaxNumberMediaPackets];

  // Construct the media packet list, up to |num_media_packets| packets.
  // Returns the next sequence number after the last media packet.
  // (this will be the sequence of the first FEC packet)
  int ConstructMediaPacketsSeqNum(int num_media_packets, int start_seq_num);
  int ConstructMediaPackets(int num_media_packets);

  // Construct the received packet list: a subset of the media and FEC packets.
  void NetworkReceivedPackets();

  // Add packet from |packet_list| to list of received packets, using the
  // |loss_mask|.
  // The |packet_list| may be a media packet list (is_fec = false), or a
  // FEC packet list (is_fec = true).
  void ReceivedPackets(const PacketList& packet_list, int* loss_mask,
                       bool is_fec);

  // Check for complete recovery after FEC decoding.
  bool IsRecoveryComplete();

  // Delete the received packets.
  void FreeRecoveredPacketList();

  // Delete the media and FEC packets.
  void TearDown();
};

TEST_F(RtpFecTest, FecRecoveryNoLoss) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 4;
  uint8_t kProtectionFactor = 60;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // No packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLoss) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 4;
  uint8_t kProtectionFactor = 60;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // 1 media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 2 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

// Verify that we don't use an old FEC packet for FEC decoding.
TEST_F(RtpFecTest, FecRecoveryWithSeqNumGapTwoFrames) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  uint8_t kProtectionFactor = 20;

  // Two frames: first frame (old) with two media packets and 1 FEC packet.
  // Second frame (new) with 3 media packets, and no FEC packets.
  //       ---Frame 1----                     ----Frame 2------
  //  #0(media) #1(media) #2(FEC)     #65535(media) #0(media) #1(media).
  // If we lose either packet 0 or 1 of second frame, FEC decoding should not
  // try to decode using "old" FEC packet #2.

  // Construct media packets for first frame, starting at sequence number 0.
  fec_seq_num_ = ConstructMediaPacketsSeqNum(2, 0);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));
  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));
  // Add FEC packet (seq#2) of this first frame to received list (i.e., assume
  // the two media packet were lost).
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, true);

  // Construct media packets for second frame, with sequence number wrap.
  ClearList(&media_packet_list_);
  fec_seq_num_ = ConstructMediaPacketsSeqNum(3, 65535);

  // Expect 3 media packets for this frame.
  EXPECT_EQ(3, static_cast<int>(media_packet_list_.size()));

  // Second media packet lost (seq#0).
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  media_loss_mask_[1] = 1;
  // Add packets #65535, and #1 to received list.
  ReceivedPackets(media_packet_list_, media_loss_mask_, false);

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // Expect that no decoding is done to get missing packet (seq#0) of second
  // frame, using old FEC packet (seq#2) from first (old) frame. So number of
  // recovered packets is 2, and not equal to number of media packets (=3).
  EXPECT_EQ(2, static_cast<int>(recovered_packet_list_.size()));
  EXPECT_TRUE(recovered_packet_list_.size() != media_packet_list_.size());
  FreeRecoveredPacketList();
}

// Verify we can still recovery frame if sequence number wrap occurs within
// the frame and FEC packet following wrap is received after media packets.
TEST_F(RtpFecTest, FecRecoveryWithSeqNumGapOneFrameRecovery) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  uint8_t kProtectionFactor = 20;

  // One frame, with sequence number wrap in media packets.
  //         -----Frame 1----
  //  #65534(media) #65535(media) #0(media) #1(FEC).
  fec_seq_num_ = ConstructMediaPacketsSeqNum(3, 65534);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // Lose one media packet (seq# 65535).
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  ReceivedPackets(media_packet_list_, media_loss_mask_, false);
  // Add FEC packet to received list following the media packets.
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, true);

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // Expect 3 media packets in recovered list, and complete recovery.
  // Wrap-around won't remove FEC packet, as it follows the wrap.
  EXPECT_EQ(3, static_cast<int>(recovered_packet_list_.size()));
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();
}

// Sequence number wrap occurs within the FEC packets for the frame.
// In this case we will discard FEC packet and full recovery is not expected.
// Same problem will occur if wrap is within media packets but FEC packet is
// received before the media packets. This may be improved if timing information
// is used to detect old FEC packets.
// TODO(marpan): Update test if wrap-around handling changes in FEC decoding.
TEST_F(RtpFecTest, FecRecoveryWithSeqNumGapOneFrameNoRecovery) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  uint8_t kProtectionFactor = 200;

  // 1 frame: 3 media packets and 2 FEC packets.
  // Sequence number wrap in FEC packets.
  //           -----Frame 1----
  // #65532(media) #65533(media) #65534(media) #65535(FEC) #0(FEC).
  fec_seq_num_ = ConstructMediaPacketsSeqNum(3, 65532);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 2 FEC packets.
  EXPECT_EQ(2, static_cast<int>(fec_packet_list_.size()));

  // Lose the last two media packets (seq# 65533, 65534).
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  ReceivedPackets(media_packet_list_, media_loss_mask_, false);
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, true);

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // The two FEC packets are received and should allow for complete recovery,
  // but because of the wrap the second FEC packet will be discarded, and only
  // one media packet is recoverable. So exepct 2 media packets on recovered
  // list and no complete recovery.
  EXPECT_EQ(2, static_cast<int>(recovered_packet_list_.size()));
  EXPECT_TRUE(recovered_packet_list_.size() != media_packet_list_.size());
  EXPECT_FALSE(IsRecoveryComplete());
  FreeRecoveredPacketList();
}

// Verify we can still recovery frame if FEC is received before media packets.
TEST_F(RtpFecTest, FecRecoveryWithFecOutOfOrder) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  uint8_t kProtectionFactor = 20;

  // One frame: 3 media packets, 1 FEC packet.
  //         -----Frame 1----
  //  #0(media) #1(media) #2(media) #3(FEC).
  fec_seq_num_ = ConstructMediaPacketsSeqNum(3, 0);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // Lose one media packet (seq# 1).
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  // Add FEC packet to received list before the media packets.
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, true);
  // Add media packets to received list.
  ReceivedPackets(media_packet_list_, media_loss_mask_, false);

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // Expect 3 media packets in recovered list, and complete recovery.
  EXPECT_EQ(3, static_cast<int>(recovered_packet_list_.size()));
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();
}

// Test 50% protection with random mask type: Two cases are considered:
// a 50% non-consecutive loss which can be fully recovered, and a 50%
// consecutive loss which cannot be fully recovered.
TEST_F(RtpFecTest, FecRecoveryWithLoss50percRandomMask) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 4;
  const uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,0) code, from random mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 0)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          1        0          0
  // fec#1:    1          0        1          0
  // fec#2:    0          0        1          1
  // fec#3:    0          1        0          1
  //

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskRandom, &fec_packet_list_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4, static_cast<int>(fec_packet_list_.size()));

  // 4 packets lost: 3 media packets (0, 2, 3), and one FEC packet (0) lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[0] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // With media packet#1 and FEC packets #1, #2, #3, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 4 consecutive packets lost: media packets 0, 1, 2, 3.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[0] = 1;
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Cannot get complete recovery for this loss configuration with random mask.
  EXPECT_FALSE(IsRecoveryComplete());
}

// Test 50% protection with bursty type: Three cases are considered:
// two 50% consecutive losses which can be fully recovered, and one
// non-consecutive which cannot be fully recovered.
TEST_F(RtpFecTest, FecRecoveryWithLoss50percBurstyMask) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 4;
  const uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,0) code, from bursty mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 0)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          0        0          0
  // fec#1:    1          1        0          0
  // fec#2:    0          1        1          0
  // fec#3:    0          0        1          1
  //

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4, static_cast<int>(fec_packet_list_.size()));

  // 4 consecutive packets lost: media packets 0,1,2,3.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[0] = 1;
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Expect complete recovery for consecutive packet loss <= 50%.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 4 consecutive packets lost: media packets 1,2, 3, and FEC packet 0.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[0] = 1;
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Expect complete recovery for consecutive packet loss <= 50%.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 4 packets lost (non-consecutive loss): media packets 0, 3, and FEC# 0, 3.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[0] = 1;
  fec_loss_mask_[3] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryNoLossUep) {
  const int kNumImportantPackets = 2;
  const bool kUseUnequalProtection = true;
  const int kNumMediaPackets = 4;
  const uint8_t kProtectionFactor = 60;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // No packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  NetworkReceivedPackets();

  EXPECT_EQ(0,
            fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryWithLossUep) {
  const int kNumImportantPackets = 2;
  const bool kUseUnequalProtection = true;
  const int kNumMediaPackets = 4;
  const uint8_t kProtectionFactor = 60;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // 1 media packet lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 2 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

// Test 50% protection with random mask type for UEP on.
TEST_F(RtpFecTest, FecRecoveryWithLoss50percUepRandomMask) {
  const int kNumImportantPackets = 1;
  const bool kUseUnequalProtection = true;
  const int kNumMediaPackets = 4;
  const uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,1) code, from random mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 1)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          0        0          0
  // fec#1:    1          1        0          0
  // fec#2:    1          0        1          1
  // fec#3:    0          1        1          0
  //

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_->GenerateFEC(media_packet_list_, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskRandom, &fec_packet_list_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4, static_cast<int>(fec_packet_list_.size()));

  // 4 packets lost: 3 media packets and FEC packet#1 lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[1] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // With media packet#3 and FEC packets #0, #1, #3, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 5 packets lost: 4 media packets and one FEC packet#2 lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  fec_loss_mask_[2] = 1;
  media_loss_mask_[0] = 1;
  media_loss_mask_[1] = 1;
  media_loss_mask_[2] = 1;
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryNonConsecutivePackets) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 5;
  uint8_t kProtectionFactor = 60;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  PacketList protected_media_packets;
  int i = 0;
  for (PacketList::iterator it = media_packet_list_.begin();
       it != media_packet_list_.end(); ++it, ++i) {
    if (i % 2 == 0) protected_media_packets.push_back(*it);
  }

  EXPECT_EQ(0, fec_->GenerateFEC(protected_media_packets, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1, static_cast<int>(fec_packet_list_.size()));

  // 1 protected media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[2] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // Unprotected packet lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[1] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 2 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[0] = 1;
  media_loss_mask_[2] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // 2 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryNonConsecutivePacketsExtension) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 21;
  uint8_t kProtectionFactor = 127;

  fec_seq_num_ = ConstructMediaPackets(kNumMediaPackets);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  PacketList protected_media_packets;
  int i = 0;
  for (PacketList::iterator it = media_packet_list_.begin();
       it != media_packet_list_.end(); ++it, ++i) {
    if (i % 2 == 0) protected_media_packets.push_back(*it);
  }

  // Zero column insertion will have to extend the size of the packet
  // mask since the number of actual packets are 21, while the number
  // of protected packets are 11.
  EXPECT_EQ(0, fec_->GenerateFEC(protected_media_packets, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 5 FEC packet.
  EXPECT_EQ(5, static_cast<int>(fec_packet_list_.size()));

  // Last protected media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 1] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // Last unprotected packet lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 2] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 6 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 11] = 1;
  media_loss_mask_[kNumMediaPackets - 9] = 1;
  media_loss_mask_[kNumMediaPackets - 7] = 1;
  media_loss_mask_[kNumMediaPackets - 5] = 1;
  media_loss_mask_[kNumMediaPackets - 3] = 1;
  media_loss_mask_[kNumMediaPackets - 1] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // 5 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

TEST_F(RtpFecTest, FecRecoveryNonConsecutivePacketsWrap) {
  const int kNumImportantPackets = 0;
  const bool kUseUnequalProtection = false;
  const int kNumMediaPackets = 21;
  uint8_t kProtectionFactor = 127;

  fec_seq_num_ = ConstructMediaPacketsSeqNum(kNumMediaPackets, 0xFFFF - 5);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  PacketList protected_media_packets;
  int i = 0;
  for (PacketList::iterator it = media_packet_list_.begin();
       it != media_packet_list_.end(); ++it, ++i) {
    if (i % 2 == 0) protected_media_packets.push_back(*it);
  }

  // Zero column insertion will have to extend the size of the packet
  // mask since the number of actual packets are 21, while the number
  // of protected packets are 11.
  EXPECT_EQ(0, fec_->GenerateFEC(protected_media_packets, kProtectionFactor,
                                 kNumImportantPackets, kUseUnequalProtection,
                                 webrtc::kFecMaskBursty, &fec_packet_list_));

  // Expect 5 FEC packet.
  EXPECT_EQ(5, static_cast<int>(fec_packet_list_.size()));

  // Last protected media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 1] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // Last unprotected packet lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 2] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(IsRecoveryComplete());
  FreeRecoveredPacketList();

  // 6 media packets lost.
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[kNumMediaPackets - 11] = 1;
  media_loss_mask_[kNumMediaPackets - 9] = 1;
  media_loss_mask_[kNumMediaPackets - 7] = 1;
  media_loss_mask_[kNumMediaPackets - 5] = 1;
  media_loss_mask_[kNumMediaPackets - 3] = 1;
  media_loss_mask_[kNumMediaPackets - 1] = 1;
  NetworkReceivedPackets();

  EXPECT_EQ(0, fec_->DecodeFEC(&received_packet_list_,
                               &recovered_packet_list_));

  // 5 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(IsRecoveryComplete());
}

void RtpFecTest::TearDown() {
  fec_->ResetState(&recovered_packet_list_);
  delete fec_;
  FreeRecoveredPacketList();
  ClearList(&media_packet_list_);
  EXPECT_TRUE(media_packet_list_.empty());
}

void RtpFecTest::FreeRecoveredPacketList() {
  ClearList(&recovered_packet_list_);
}

bool RtpFecTest::IsRecoveryComplete() {
  // Check that the number of media and recovered packets are equal.
  if (media_packet_list_.size() != recovered_packet_list_.size()) {
    return false;
  }

  ForwardErrorCorrection::Packet* media_packet;
  ForwardErrorCorrection::RecoveredPacket* recovered_packet;

  bool recovery = true;

  PacketList::iterator media_packet_list_item = media_packet_list_.begin();
  RecoveredPacketList::iterator recovered_packet_list_item =
      recovered_packet_list_.begin();
  while (media_packet_list_item != media_packet_list_.end()) {
    if (recovered_packet_list_item == recovered_packet_list_.end()) {
      return false;
    }
    media_packet = *media_packet_list_item;
    recovered_packet = *recovered_packet_list_item;
    if (recovered_packet->pkt->length != media_packet->length) {
      return false;
    }
    if (memcmp(recovered_packet->pkt->data, media_packet->data,
               media_packet->length) != 0) {
      return false;
    }
    media_packet_list_item++;
    recovered_packet_list_item++;
  }
  return recovery;
}

void RtpFecTest::NetworkReceivedPackets() {
  const bool kFecPacket = true;
  ReceivedPackets(media_packet_list_, media_loss_mask_, !kFecPacket);
  ReceivedPackets(fec_packet_list_, fec_loss_mask_, kFecPacket);
}

void RtpFecTest::ReceivedPackets(const PacketList& packet_list, int* loss_mask,
                                 bool is_fec) {
  ForwardErrorCorrection::Packet* packet;
  ForwardErrorCorrection::ReceivedPacket* received_packet;
  int seq_num = fec_seq_num_;
  int packet_idx = 0;

  PacketList::const_iterator packet_list_item = packet_list.begin();

  while (packet_list_item != packet_list.end()) {
    packet = *packet_list_item;
    if (loss_mask[packet_idx] == 0) {
      received_packet = new ForwardErrorCorrection::ReceivedPacket;
      received_packet->pkt = new ForwardErrorCorrection::Packet;
      received_packet_list_.push_back(received_packet);
      received_packet->pkt->length = packet->length;
      memcpy(received_packet->pkt->data, packet->data, packet->length);
      received_packet->is_fec = is_fec;
      if (!is_fec) {
        // For media packets, the sequence number and marker bit is
        // obtained from RTP header. These were set in ConstructMediaPackets().
        received_packet->seq_num =
            webrtc::ByteReader<uint16_t>::ReadBigEndian(&packet->data[2]);
      } else {
        // The sequence number, marker bit, and ssrc number are defined in the
        // RTP header of the FEC packet, which is not constructed in this test.
        // So we set these values below based on the values generated in
        // ConstructMediaPackets().
        received_packet->seq_num = seq_num;
        // The ssrc value for FEC packets is set to the one used for the
        // media packets in ConstructMediaPackets().
        received_packet->ssrc = ssrc_;
      }
    }
    packet_idx++;
    packet_list_item++;
    // Sequence number of FEC packets are defined as increment by 1 from
    // last media packet in frame.
    if (is_fec) seq_num++;
  }
}

int RtpFecTest::ConstructMediaPacketsSeqNum(int num_media_packets,
                                            int start_seq_num) {
  assert(num_media_packets > 0);
  ForwardErrorCorrection::Packet* media_packet = NULL;
  int sequence_number = start_seq_num;
  int time_stamp = random_.Rand<int>();

  for (int i = 0; i < num_media_packets; ++i) {
    media_packet = new ForwardErrorCorrection::Packet;
    media_packet_list_.push_back(media_packet);
    const uint32_t kMinPacketSize = kRtpHeaderSize;
    const uint32_t kMaxPacketSize = IP_PACKET_SIZE - kRtpHeaderSize -
                                    kTransportOverhead -
                                    ForwardErrorCorrection::PacketOverhead();
    media_packet->length = random_.Rand(kMinPacketSize, kMaxPacketSize);

    // Generate random values for the first 2 bytes
    media_packet->data[0] = random_.Rand<uint8_t>();
    media_packet->data[1] = random_.Rand<uint8_t>();

    // The first two bits are assumed to be 10 by the FEC encoder.
    // In fact the FEC decoder will set the two first bits to 10 regardless of
    // what they actually were. Set the first two bits to 10 so that a memcmp
    // can be performed for the whole restored packet.
    media_packet->data[0] |= 0x80;
    media_packet->data[0] &= 0xbf;

    // FEC is applied to a whole frame.
    // A frame is signaled by multiple packets without the marker bit set
    // followed by the last packet of the frame for which the marker bit is set.
    // Only push one (fake) frame to the FEC.
    media_packet->data[1] &= 0x7f;

    webrtc::ByteWriter<uint16_t>::WriteBigEndian(&media_packet->data[2],
                                                 sequence_number);
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&media_packet->data[4],
                                                 time_stamp);
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&media_packet->data[8], ssrc_);

    // Generate random values for payload.
    for (size_t j = 12; j < media_packet->length; ++j) {
      media_packet->data[j] = random_.Rand<uint8_t>();
    }
    sequence_number++;
  }
  // Last packet, set marker bit.
  assert(media_packet != NULL);
  media_packet->data[1] |= 0x80;
  return sequence_number;
}

int RtpFecTest::ConstructMediaPackets(int num_media_packets) {
  return ConstructMediaPacketsSeqNum(num_media_packets, random_.Rand<int>());
}
