/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/pli.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

using webrtc::rtcp::Pli;
using webrtc::rtcp::RawPacket;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
// Manually created Pli packet matching constants above.
const uint8_t kPacket[] = {0x81, 206,  0x00, 0x02,
                           0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89};
const size_t kPacketLength = sizeof(kPacket);

TEST(RtcpPacketPliTest, Parse) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  Pli mutable_parsed;
  EXPECT_TRUE(mutable_parsed.Parse(
      header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
  const Pli& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
}

TEST(RtcpPacketPliTest, Create) {
  Pli pli;
  pli.From(kSenderSsrc);
  pli.To(kRemoteSsrc);

  rtc::scoped_ptr<RawPacket> packet(pli.Build());

  ASSERT_EQ(kPacketLength, packet->Length());
  EXPECT_EQ(0, memcmp(kPacket, packet->Buffer(), kPacketLength));
}

TEST(RtcpPacketPliTest, ParseFailsOnTooSmallPacket) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  header.payload_size_bytes--;

  Pli parsed;
  EXPECT_FALSE(
      parsed.Parse(header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
}

}  // namespace
}  // namespace webrtc
