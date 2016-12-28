/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::ExtendedJitterReport;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

class RtcpPacketExtendedJitterReportTest : public ::testing::Test {
 protected:
  void BuildPacket() { packet = ij.Build(); }
  void ParsePacket() {
    RtcpCommonHeader header;
    EXPECT_TRUE(
        RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header));
    EXPECT_EQ(header.BlockSize(), packet->Length());
    EXPECT_TRUE(parsed_.Parse(
        header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
  }

  ExtendedJitterReport ij;
  rtc::scoped_ptr<RawPacket> packet;
  const ExtendedJitterReport& parsed() { return parsed_; }

 private:
  ExtendedJitterReport parsed_;
};

TEST_F(RtcpPacketExtendedJitterReportTest, NoItem) {
  // No initialization because packet is empty.
  BuildPacket();
  ParsePacket();

  EXPECT_EQ(0u, parsed().jitters_count());
}

TEST_F(RtcpPacketExtendedJitterReportTest, OneItem) {
  EXPECT_TRUE(ij.WithJitter(0x11121314));

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(1u, parsed().jitters_count());
  EXPECT_EQ(0x11121314U, parsed().jitter(0));
}

TEST_F(RtcpPacketExtendedJitterReportTest, TwoItems) {
  EXPECT_TRUE(ij.WithJitter(0x11121418));
  EXPECT_TRUE(ij.WithJitter(0x22242628));

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(2u, parsed().jitters_count());
  EXPECT_EQ(0x11121418U, parsed().jitter(0));
  EXPECT_EQ(0x22242628U, parsed().jitter(1));
}

TEST_F(RtcpPacketExtendedJitterReportTest, TooManyItems) {
  const int kMaxIjItems = (1 << 5) - 1;
  for (int i = 0; i < kMaxIjItems; ++i) {
    EXPECT_TRUE(ij.WithJitter(i));
  }
  EXPECT_FALSE(ij.WithJitter(kMaxIjItems));
}

TEST_F(RtcpPacketExtendedJitterReportTest, ParseFailWithTooManyItems) {
  ij.WithJitter(0x11121418);
  BuildPacket();
  RtcpCommonHeader header;
  RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header);
  header.count_or_format++;  // Damage package.

  ExtendedJitterReport parsed;

  EXPECT_FALSE(parsed.Parse(
      header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
}

}  // namespace
}  // namespace webrtc
