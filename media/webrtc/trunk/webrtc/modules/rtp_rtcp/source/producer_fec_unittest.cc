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
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"

namespace webrtc {

void VerifyHeader(uint16_t seq_num,
                  uint32_t timestamp,
                  int red_pltype,
                  int fec_pltype,
                  RedPacket* packet,
                  bool marker_bit) {
  EXPECT_GT(packet->length(), kRtpHeaderSize);
  EXPECT_TRUE(packet->data() != NULL);
  uint8_t* data = packet->data();
  // Marker bit not set.
  EXPECT_EQ(marker_bit ? 0x80 : 0, data[1] & 0x80);
  EXPECT_EQ(red_pltype, data[1] & 0x7F);
  EXPECT_EQ(seq_num, (data[2] << 8) + data[3]);
  uint32_t parsed_timestamp = (data[4] << 24) + (data[5] << 16) +
      (data[6] << 8) + data[7];
  EXPECT_EQ(timestamp, parsed_timestamp);
  EXPECT_EQ(static_cast<uint8_t>(fec_pltype), data[kRtpHeaderSize]);
}

class ProducerFecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fec_ = new ForwardErrorCorrection();
    producer_ = new ProducerFec(fec_);
    generator_ = new FrameGenerator;
  }

  virtual void TearDown() {
    delete producer_;
    delete fec_;
    delete generator_;
  }
  ForwardErrorCorrection* fec_;
  ProducerFec* producer_;
  FrameGenerator* generator_;
};

// Verifies bug found via fuzzing, where a gap in the packet sequence caused us
// to move past the end of the current FEC packet mask byte without moving to
// the next byte. That likely caused us to repeatedly read from the same byte,
// and if that byte didn't protect packets we would generate empty FEC.
TEST_F(ProducerFecTest, NoEmptyFecWithSeqNumGaps) {
  struct Packet {
    size_t header_size;
    size_t payload_size;
    uint16_t seq_num;
    bool marker_bit;
  };
  std::vector<Packet> protected_packets;
  protected_packets.push_back({15, 3, 41, 0});
  protected_packets.push_back({14, 1, 43, 0});
  protected_packets.push_back({19, 0, 48, 0});
  protected_packets.push_back({19, 0, 50, 0});
  protected_packets.push_back({14, 3, 51, 0});
  protected_packets.push_back({13, 8, 52, 0});
  protected_packets.push_back({19, 2, 53, 0});
  protected_packets.push_back({12, 3, 54, 0});
  protected_packets.push_back({21, 0, 55, 0});
  protected_packets.push_back({13, 3, 57, 1});
  FecProtectionParams params = {117, 0, 3, kFecMaskBursty};
  producer_->SetFecParameters(&params, 0);
  uint8_t packet[28] = {0};
  for (Packet p : protected_packets) {
    if (p.marker_bit) {
      packet[1] |= 0x80;
    } else {
      packet[1] &= ~0x80;
    }
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], p.seq_num);
    producer_->AddRtpPacketAndGenerateFec(packet, p.payload_size,
                                          p.header_size);
    uint16_t num_fec_packets = producer_->NumAvailableFecPackets();
    std::vector<RedPacket*> fec_packets;
    if (num_fec_packets > 0) {
      fec_packets =
          producer_->GetFecPackets(kRedPayloadType, 99, 100, p.header_size);
      EXPECT_EQ(num_fec_packets, fec_packets.size());
    }
    for (RedPacket* fec_packet : fec_packets) {
      delete fec_packet;
    }
  }
}

TEST_F(ProducerFecTest, OneFrameFec) {
  // The number of media packets (|kNumPackets|), number of frames (one for
  // this test), and the protection factor (|params->fec_rate|) are set to make
  // sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over 1 frame
  // of packets is within |kMaxExcessOverhead|, and (2) the total number of
  // media packets for 1 frame is at least |minimum_media_packets_fec_|.
  const int kNumPackets = 4;
  FecProtectionParams params = {15, false, 3};
  std::list<RtpPacket*> rtp_packets;
  generator_->NewFrame(kNumPackets);
  producer_->SetFecParameters(&params, 0);  // Expecting one FEC packet.
  uint32_t last_timestamp = 0;
  for (int i = 0; i < kNumPackets; ++i) {
    RtpPacket* rtp_packet = generator_->NextPacket(i, 10);
    rtp_packets.push_back(rtp_packet);
    EXPECT_EQ(0, producer_->AddRtpPacketAndGenerateFec(rtp_packet->data,
                                                       rtp_packet->length,
                                                       kRtpHeaderSize));
    last_timestamp = rtp_packet->header.header.timestamp;
  }
  EXPECT_TRUE(producer_->FecAvailable());
  uint16_t seq_num = generator_->NextSeqNum();
  std::vector<RedPacket*> packets = producer_->GetFecPackets(kRedPayloadType,
                                                             kFecPayloadType,
                                                             seq_num,
                                                             kRtpHeaderSize);
  EXPECT_FALSE(producer_->FecAvailable());
  ASSERT_EQ(1u, packets.size());
  VerifyHeader(seq_num, last_timestamp,
               kRedPayloadType, kFecPayloadType, packets.front(), false);
  while (!rtp_packets.empty()) {
    delete rtp_packets.front();
    rtp_packets.pop_front();
  }
  delete packets.front();
}

TEST_F(ProducerFecTest, TwoFrameFec) {
  // The number of media packets/frame (|kNumPackets|), the number of frames
  // (|kNumFrames|), and the protection factor (|params->fec_rate|) are set to
  // make sure the conditions for generating FEC are satisfied. This means:
  // (1) protection factor is high enough so that actual overhead over
  // |kNumFrames| is within |kMaxExcessOverhead|, and (2) the total number of
  // media packets for |kNumFrames| frames is at least
  // |minimum_media_packets_fec_|.
  const int kNumPackets = 2;
  const int kNumFrames = 2;

  FecProtectionParams params = {15, 0, 3};
  std::list<RtpPacket*> rtp_packets;
  producer_->SetFecParameters(&params, 0);  // Expecting one FEC packet.
  uint32_t last_timestamp = 0;
  for (int i = 0; i < kNumFrames; ++i) {
    generator_->NewFrame(kNumPackets);
    for (int j = 0; j < kNumPackets; ++j) {
      RtpPacket* rtp_packet = generator_->NextPacket(i * kNumPackets + j, 10);
      rtp_packets.push_back(rtp_packet);
      EXPECT_EQ(0, producer_->AddRtpPacketAndGenerateFec(rtp_packet->data,
                                           rtp_packet->length,
                                           kRtpHeaderSize));
      last_timestamp = rtp_packet->header.header.timestamp;
    }
  }
  EXPECT_TRUE(producer_->FecAvailable());
  uint16_t seq_num = generator_->NextSeqNum();
  std::vector<RedPacket*> packets = producer_->GetFecPackets(kRedPayloadType,
                                                             kFecPayloadType,
                                                             seq_num,
                                                             kRtpHeaderSize);
  EXPECT_FALSE(producer_->FecAvailable());
  ASSERT_EQ(1u, packets.size());
  VerifyHeader(seq_num, last_timestamp, kRedPayloadType, kFecPayloadType,
               packets.front(), false);
  while (!rtp_packets.empty()) {
    delete rtp_packets.front();
    rtp_packets.pop_front();
  }
  delete packets.front();
}

TEST_F(ProducerFecTest, BuildRedPacket) {
  generator_->NewFrame(1);
  RtpPacket* packet = generator_->NextPacket(0, 10);
  rtc::scoped_ptr<RedPacket> red_packet(producer_->BuildRedPacket(
      packet->data, packet->length - kRtpHeaderSize, kRtpHeaderSize,
      kRedPayloadType));
  EXPECT_EQ(packet->length + 1, red_packet->length());
  VerifyHeader(packet->header.header.sequenceNumber,
               packet->header.header.timestamp,
               kRedPayloadType,
               packet->header.header.payloadType,
               red_packet.get(),
               true);  // Marker bit set.
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, red_packet->data()[kRtpHeaderSize + 1 + i]);
  delete packet;
}

}  // namespace webrtc
