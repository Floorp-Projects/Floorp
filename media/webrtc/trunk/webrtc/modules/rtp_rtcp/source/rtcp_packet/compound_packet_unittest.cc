/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::Bye;
using webrtc::rtcp::CompoundPacket;
using webrtc::rtcp::Fir;
using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::SenderReport;
using webrtc::test::RtcpPacketParser;

namespace webrtc {

const uint32_t kSenderSsrc = 0x12345678;

TEST(RtcpCompoundPacketTest, AppendPacket) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb));
  rr.Append(&fir);

  rtc::scoped_ptr<RawPacket> packet(rr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.receiver_report()->Ssrc());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpCompoundPacketTest, AppendPacketOnEmpty) {
  CompoundPacket empty;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  empty.Append(&rr);

  rtc::scoped_ptr<RawPacket> packet(empty.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(0, parser.report_block()->num_packets());
}

TEST(RtcpCompoundPacketTest, AppendPacketWithOwnAppendedPacket) {
  Fir fir;
  Bye bye;
  ReportBlock rb;

  ReceiverReport rr;
  EXPECT_TRUE(rr.WithReportBlock(rb));
  rr.Append(&fir);

  SenderReport sr;
  sr.Append(&bye);
  sr.Append(&rr);

  rtc::scoped_ptr<RawPacket> packet(sr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.sender_report()->num_packets());
  EXPECT_EQ(1, parser.receiver_report()->num_packets());
  EXPECT_EQ(1, parser.report_block()->num_packets());
  EXPECT_EQ(1, parser.bye()->num_packets());
  EXPECT_EQ(1, parser.fir()->num_packets());
}

TEST(RtcpCompoundPacketTest, BuildWithInputBuffer) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb));
  rr.Append(&fir);

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;
  const size_t kFirLength = 20;

  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
   public:
    void OnPacketReady(uint8_t* data, size_t length) override {
      RtcpPacketParser parser;
      parser.Parse(data, length);
      EXPECT_EQ(1, parser.receiver_report()->num_packets());
      EXPECT_EQ(1, parser.report_block()->num_packets());
      EXPECT_EQ(1, parser.fir()->num_packets());
      ++packets_created_;
    }

    int packets_created_ = 0;
  } verifier;
  const size_t kBufferSize = kRrLength + kReportBlockLength + kFirLength;
  uint8_t buffer[kBufferSize];
  EXPECT_TRUE(rr.BuildExternalBuffer(buffer, kBufferSize, &verifier));
  EXPECT_EQ(1, verifier.packets_created_);
}

TEST(RtcpCompoundPacketTest, BuildWithTooSmallBuffer_FragmentedSend) {
  Fir fir;
  ReportBlock rb;
  ReceiverReport rr;
  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb));
  rr.Append(&fir);

  const size_t kRrLength = 8;
  const size_t kReportBlockLength = 24;

  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
   public:
    void OnPacketReady(uint8_t* data, size_t length) override {
      RtcpPacketParser parser;
      parser.Parse(data, length);
      switch (packets_created_++) {
        case 0:
          EXPECT_EQ(1, parser.receiver_report()->num_packets());
          EXPECT_EQ(1, parser.report_block()->num_packets());
          EXPECT_EQ(0, parser.fir()->num_packets());
          break;
        case 1:
          EXPECT_EQ(0, parser.receiver_report()->num_packets());
          EXPECT_EQ(0, parser.report_block()->num_packets());
          EXPECT_EQ(1, parser.fir()->num_packets());
          break;
        default:
          ADD_FAILURE() << "OnPacketReady not expected to be called "
                        << packets_created_ << " times.";
      }
    }

    int packets_created_ = 0;
  } verifier;
  const size_t kBufferSize = kRrLength + kReportBlockLength;
  uint8_t buffer[kBufferSize];
  EXPECT_TRUE(rr.BuildExternalBuffer(buffer, kBufferSize, &verifier));
  EXPECT_EQ(2, verifier.packets_created_);
}

}  // namespace webrtc
