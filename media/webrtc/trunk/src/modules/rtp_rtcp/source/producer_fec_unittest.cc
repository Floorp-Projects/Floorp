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

#include "gtest/gtest.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/producer_fec.h"

namespace webrtc {

void VerifyHeader(uint16_t seq_num,
                  uint32_t timestamp,
                  int red_pltype,
                  int fec_pltype,
                  RedPacket* packet,
                  bool marker_bit) {
  EXPECT_GT(packet->length(), static_cast<int>(kRtpHeaderSize));
  EXPECT_TRUE(packet->data() != NULL);
  uint8_t* data = packet->data();
  // Marker bit not set.
  EXPECT_EQ(marker_bit ? 0x80 : 0, data[1] & 0x80);
  EXPECT_EQ(red_pltype, data[1] & 0x7F);
  EXPECT_EQ(seq_num, (data[2] << 8) + data[3]);
  uint32_t parsed_timestamp = (data[4] << 24) + (data[5] << 16) +
      (data[6] << 8) + data[7];
  EXPECT_EQ(timestamp, parsed_timestamp);
  EXPECT_EQ(fec_pltype, data[kRtpHeaderSize]);
}

class ProducerFecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fec_ = new ForwardErrorCorrection(0);
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

TEST_F(ProducerFecTest, OneFrameFec) {
  const int kNumPackets = 4;
  FecProtectionParams params = {5, false, 3};
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
  RedPacket* packet = producer_->GetFecPacket(kRedPayloadType,
                                              kFecPayloadType,
                                              seq_num);
  EXPECT_FALSE(producer_->FecAvailable());
  ASSERT_TRUE(packet != NULL);
  VerifyHeader(seq_num, last_timestamp,
               kRedPayloadType, kFecPayloadType, packet, false);
  while (!rtp_packets.empty()) {
    delete rtp_packets.front();
    rtp_packets.pop_front();
  }
  delete packet;
}

TEST_F(ProducerFecTest, TwoFrameFec) {
  const int kNumPackets = 2;
  const int kNumFrames = 2;
  FecProtectionParams params = {5, 0, 3};
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
  RedPacket* packet = producer_->GetFecPacket(kRedPayloadType,
                                              kFecPayloadType,
                                              seq_num);
  EXPECT_FALSE(producer_->FecAvailable());
  EXPECT_TRUE(packet != NULL);
  VerifyHeader(seq_num, last_timestamp,
               kRedPayloadType, kFecPayloadType, packet, false);
  while (!rtp_packets.empty()) {
    delete rtp_packets.front();
    rtp_packets.pop_front();
  }
  delete packet;
}

TEST_F(ProducerFecTest, BuildRedPacket) {
  generator_->NewFrame(1);
  RtpPacket* packet = generator_->NextPacket(0, 10);
  RedPacket* red_packet = producer_->BuildRedPacket(packet->data,
                                                    packet->length -
                                                    kRtpHeaderSize,
                                                    kRtpHeaderSize,
                                                    kRedPayloadType);
  EXPECT_EQ(packet->length + 1, red_packet->length());
  VerifyHeader(packet->header.header.sequenceNumber,
               packet->header.header.timestamp,
               kRedPayloadType,
               packet->header.header.payloadType,
               red_packet,
               true);  // Marker bit set.
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, red_packet->data()[kRtpHeaderSize + 1 + i]);
  delete red_packet;
  delete packet;
}

}  // namespace webrtc
