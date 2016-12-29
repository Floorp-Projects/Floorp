/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbn.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::Tmmbn;
using webrtc::test::RtcpPacketParser;

namespace webrtc {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketTest, TmmbnWithNoItem) {
  Tmmbn tmmbn;
  tmmbn.From(kSenderSsrc);

  rtc::scoped_ptr<RawPacket> packet(tmmbn.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.tmmbn()->Ssrc());
  EXPECT_EQ(0, parser.tmmbn_items()->num_packets());
}

TEST(RtcpPacketTest, TmmbnWithOneItem) {
  Tmmbn tmmbn;
  tmmbn.From(kSenderSsrc);
  EXPECT_TRUE(tmmbn.WithTmmbr(kRemoteSsrc, 312, 60));

  rtc::scoped_ptr<RawPacket> packet(tmmbn.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.tmmbn()->Ssrc());
  EXPECT_EQ(1, parser.tmmbn_items()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.tmmbn_items()->Ssrc(0));
  EXPECT_EQ(312U, parser.tmmbn_items()->BitrateKbps(0));
  EXPECT_EQ(60U, parser.tmmbn_items()->Overhead(0));
}

TEST(RtcpPacketTest, TmmbnWithTwoItems) {
  Tmmbn tmmbn;
  tmmbn.From(kSenderSsrc);
  EXPECT_TRUE(tmmbn.WithTmmbr(kRemoteSsrc, 312, 60));
  EXPECT_TRUE(tmmbn.WithTmmbr(kRemoteSsrc + 1, 1288, 40));

  rtc::scoped_ptr<RawPacket> packet(tmmbn.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.tmmbn()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.tmmbn()->Ssrc());
  EXPECT_EQ(2, parser.tmmbn_items()->num_packets());
  EXPECT_EQ(kRemoteSsrc, parser.tmmbn_items()->Ssrc(0));
  EXPECT_EQ(312U, parser.tmmbn_items()->BitrateKbps(0));
  EXPECT_EQ(60U, parser.tmmbn_items()->Overhead(0));
  EXPECT_EQ(kRemoteSsrc + 1, parser.tmmbn_items()->Ssrc(1));
  EXPECT_EQ(1288U, parser.tmmbn_items()->BitrateKbps(1));
  EXPECT_EQ(40U, parser.tmmbn_items()->Overhead(1));
}

TEST(RtcpPacketTest, TmmbnWithTooManyItems) {
  Tmmbn tmmbn;
  tmmbn.From(kSenderSsrc);
  const int kMaxTmmbrItems = 50;
  for (int i = 0; i < kMaxTmmbrItems; ++i)
    EXPECT_TRUE(tmmbn.WithTmmbr(kRemoteSsrc + i, 312, 60));

  EXPECT_FALSE(tmmbn.WithTmmbr(kRemoteSsrc + kMaxTmmbrItems, 312, 60));
}

}  // namespace webrtc
