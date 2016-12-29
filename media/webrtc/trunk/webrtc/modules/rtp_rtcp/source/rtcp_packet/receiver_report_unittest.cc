/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"

#include "testing/gtest/include/gtest/gtest.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ReceiverReport;
using webrtc::rtcp::ReportBlock;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kFractionLost = 55;
const uint32_t kCumulativeLost = 0x111213;
const uint32_t kExtHighestSeqNum = 0x22232425;
const uint32_t kJitter = 0x33343536;
const uint32_t kLastSr = 0x44454647;
const uint32_t kDelayLastSr = 0x55565758;
// Manually created ReceiverReport with one ReportBlock matching constants
// above.
// Having this block allows to test Create and Parse separately.
const uint8_t kPacket[] = {0x81, 201,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89, 55,   0x11, 0x12, 0x13,
                           0x22, 0x23, 0x24, 0x25, 0x33, 0x34, 0x35, 0x36,
                           0x44, 0x45, 0x46, 0x47, 0x55, 0x56, 0x57, 0x58};
const size_t kPacketLength = sizeof(kPacket);

class RtcpPacketReceiverReportTest : public ::testing::Test {
 protected:
  void BuildPacket() { packet = rr.Build(); }
  void ParsePacket() {
    RtcpCommonHeader header;
    EXPECT_TRUE(
        RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header));
    EXPECT_EQ(header.BlockSize(), packet->Length());
    EXPECT_TRUE(parsed_.Parse(
        header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
  }

  ReceiverReport rr;
  rtc::scoped_ptr<RawPacket> packet;
  const ReceiverReport& parsed() { return parsed_; }

 private:
  ReceiverReport parsed_;
};

TEST_F(RtcpPacketReceiverReportTest, Parse) {
  RtcpCommonHeader header;
  RtcpParseCommonHeader(kPacket, kPacketLength, &header);
  EXPECT_TRUE(rr.Parse(header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
  const ReceiverReport& parsed = rr;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(1u, parsed.report_blocks().size());
  const ReportBlock& rb = parsed.report_blocks().front();
  EXPECT_EQ(kRemoteSsrc, rb.source_ssrc());
  EXPECT_EQ(kFractionLost, rb.fraction_lost());
  EXPECT_EQ(kCumulativeLost, rb.cumulative_lost());
  EXPECT_EQ(kExtHighestSeqNum, rb.extended_high_seq_num());
  EXPECT_EQ(kJitter, rb.jitter());
  EXPECT_EQ(kLastSr, rb.last_sr());
  EXPECT_EQ(kDelayLastSr, rb.delay_since_last_sr());
}

TEST_F(RtcpPacketReceiverReportTest, ParseFailsOnIncorrectSize) {
  RtcpCommonHeader header;
  RtcpParseCommonHeader(kPacket, kPacketLength, &header);
  header.count_or_format++;  // Damage the packet.
  EXPECT_FALSE(rr.Parse(header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
}

TEST_F(RtcpPacketReceiverReportTest, Create) {
  rr.From(kSenderSsrc);
  ReportBlock rb;
  rb.To(kRemoteSsrc);
  rb.WithFractionLost(kFractionLost);
  rb.WithCumulativeLost(kCumulativeLost);
  rb.WithExtHighestSeqNum(kExtHighestSeqNum);
  rb.WithJitter(kJitter);
  rb.WithLastSr(kLastSr);
  rb.WithDelayLastSr(kDelayLastSr);
  rr.WithReportBlock(rb);

  BuildPacket();

  ASSERT_EQ(kPacketLength, packet->Length());
  EXPECT_EQ(0, memcmp(kPacket, packet->Buffer(), kPacketLength));
}

TEST_F(RtcpPacketReceiverReportTest, WithoutReportBlocks) {
  rr.From(kSenderSsrc);

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed().sender_ssrc());
  EXPECT_EQ(0u, parsed().report_blocks().size());
}

TEST_F(RtcpPacketReceiverReportTest, WithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.To(kRemoteSsrc);
  ReportBlock rb2;
  rb2.To(kRemoteSsrc + 1);

  rr.From(kSenderSsrc);
  EXPECT_TRUE(rr.WithReportBlock(rb1));
  EXPECT_TRUE(rr.WithReportBlock(rb2));

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed().sender_ssrc());
  EXPECT_EQ(2u, parsed().report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed().report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1, parsed().report_blocks()[1].source_ssrc());
}

TEST_F(RtcpPacketReceiverReportTest, WithTooManyReportBlocks) {
  rr.From(kSenderSsrc);
  const size_t kMaxReportBlocks = (1 << 5) - 1;
  ReportBlock rb;
  for (size_t i = 0; i < kMaxReportBlocks; ++i) {
    rb.To(kRemoteSsrc + i);
    EXPECT_TRUE(rr.WithReportBlock(rb));
  }
  rb.To(kRemoteSsrc + kMaxReportBlocks);
  EXPECT_FALSE(rr.WithReportBlock(rb));
}

}  // namespace
}  // namespace webrtc
