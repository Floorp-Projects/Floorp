/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/bye.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

using ::testing::ElementsAre;

using webrtc::rtcp::Bye;
using webrtc::rtcp::RawPacket;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kCsrc1 = 0x22232425;
const uint32_t kCsrc2 = 0x33343536;

class RtcpPacketByeTest : public ::testing::Test {
 protected:
  void BuildPacket() { packet = bye.Build(); }
  void ParsePacket() {
    RtcpCommonHeader header;
    EXPECT_TRUE(
        RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header));
    // Check that there is exactly one RTCP packet in the buffer.
    EXPECT_EQ(header.BlockSize(), packet->Length());
    EXPECT_TRUE(parsed_bye.Parse(
        header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
  }

  Bye bye;
  rtc::scoped_ptr<RawPacket> packet;
  Bye parsed_bye;
};

TEST_F(RtcpPacketByeTest, Bye) {
  bye.From(kSenderSsrc);

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
  EXPECT_TRUE(parsed_bye.csrcs().empty());
  EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST_F(RtcpPacketByeTest, WithCsrcs) {
  bye.From(kSenderSsrc);
  EXPECT_TRUE(bye.WithCsrc(kCsrc1));
  EXPECT_TRUE(bye.WithCsrc(kCsrc2));
  EXPECT_TRUE(bye.reason().empty());

  BuildPacket();
  EXPECT_EQ(16u, packet->Length());  // Header: 4, 3xSRCs: 12, Reason: 0.

  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
  EXPECT_THAT(parsed_bye.csrcs(), ElementsAre(kCsrc1, kCsrc2));
  EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST_F(RtcpPacketByeTest, WithCsrcsAndReason) {
  const std::string kReason = "Some Reason";

  bye.From(kSenderSsrc);
  EXPECT_TRUE(bye.WithCsrc(kCsrc1));
  EXPECT_TRUE(bye.WithCsrc(kCsrc2));
  bye.WithReason(kReason);

  BuildPacket();
  EXPECT_EQ(28u, packet->Length());  // Header: 4, 3xSRCs: 12, Reason: 12.

  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
  EXPECT_THAT(parsed_bye.csrcs(), ElementsAre(kCsrc1, kCsrc2));
  EXPECT_EQ(kReason, parsed_bye.reason());
}

TEST_F(RtcpPacketByeTest, WithTooManyCsrcs) {
  bye.From(kSenderSsrc);
  const int kMaxCsrcs = (1 << 5) - 2;  // 5 bit len, first item is sender SSRC.
  for (int i = 0; i < kMaxCsrcs; ++i) {
    EXPECT_TRUE(bye.WithCsrc(i));
  }
  EXPECT_FALSE(bye.WithCsrc(kMaxCsrcs));
}

TEST_F(RtcpPacketByeTest, WithAReason) {
  const std::string kReason = "Some Random Reason";

  bye.From(kSenderSsrc);
  bye.WithReason(kReason);

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(kSenderSsrc, parsed_bye.sender_ssrc());
  EXPECT_TRUE(parsed_bye.csrcs().empty());
  EXPECT_EQ(kReason, parsed_bye.reason());
}

TEST_F(RtcpPacketByeTest, WithReasons) {
  // Test that packet creation/parsing behave with reasons of different length
  // both when it require padding and when it does not.
  for (size_t reminder = 0; reminder < 4; ++reminder) {
    const std::string kReason(4 + reminder, 'a' + reminder);
    bye.From(kSenderSsrc);
    bye.WithReason(kReason);

    BuildPacket();
    ParsePacket();

    EXPECT_EQ(kReason, parsed_bye.reason());
  }
}

TEST_F(RtcpPacketByeTest, ParseEmptyPacket) {
  RtcpCommonHeader header;
  header.packet_type = Bye::kPacketType;
  header.count_or_format = 0;
  header.payload_size_bytes = 0;
  uint8_t empty_payload[1];

  EXPECT_TRUE(parsed_bye.Parse(header, empty_payload + 1));
  EXPECT_EQ(0u, parsed_bye.sender_ssrc());
  EXPECT_TRUE(parsed_bye.csrcs().empty());
  EXPECT_TRUE(parsed_bye.reason().empty());
}

TEST_F(RtcpPacketByeTest, ParseFailOnInvalidSrcCount) {
  bye.From(kSenderSsrc);

  BuildPacket();

  RtcpCommonHeader header;
  RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header);
  header.count_or_format = 2;  // Lie there are 2 ssrcs, not one.

  EXPECT_FALSE(parsed_bye.Parse(
      header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
}

TEST_F(RtcpPacketByeTest, ParseFailOnInvalidReasonLength) {
  bye.From(kSenderSsrc);
  bye.WithReason("18 characters long");

  BuildPacket();

  RtcpCommonHeader header;
  RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header);
  header.payload_size_bytes -= 4;  // Payload is usually 32bit aligned.

  EXPECT_FALSE(parsed_bye.Parse(
      header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
}

}  // namespace
}  // namespace webrtc
