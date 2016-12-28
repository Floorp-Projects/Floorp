/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::RawPacket;
using webrtc::rtcp::Tmmbr;
using webrtc::test::RtcpPacketParser;

namespace webrtc {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

TEST(RtcpPacketTest, Tmmbr) {
  Tmmbr tmmbr;
  tmmbr.From(kSenderSsrc);
  tmmbr.To(kRemoteSsrc);
  tmmbr.WithBitrateKbps(312);
  tmmbr.WithOverhead(60);

  rtc::scoped_ptr<RawPacket> packet(tmmbr.Build());
  RtcpPacketParser parser;
  parser.Parse(packet->Buffer(), packet->Length());
  EXPECT_EQ(1, parser.tmmbr()->num_packets());
  EXPECT_EQ(kSenderSsrc, parser.tmmbr()->Ssrc());
  EXPECT_EQ(1, parser.tmmbr_item()->num_packets());
  EXPECT_EQ(312U, parser.tmmbr_item()->BitrateKbps());
  EXPECT_EQ(60U, parser.tmmbr_item()->Overhead());
}

}  // namespace webrtc
