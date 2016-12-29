/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

using webrtc::rtcp::App;
using webrtc::rtcp::RawPacket;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kName = ((uint32_t)'n' << 24) | ((uint32_t)'a' << 16) |
                       ((uint32_t)'m' << 8) | (uint32_t)'e';
const uint32_t kSenderSsrc = 0x12345678;

class RtcpPacketAppTest : public ::testing::Test {
 protected:
  void BuildPacket() { packet = app.Build(); }
  void ParsePacket() {
    RtcpCommonHeader header;
    EXPECT_TRUE(
        RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header));
    // Check there is exactly one RTCP packet in the buffer.
    EXPECT_EQ(header.BlockSize(), packet->Length());
    EXPECT_TRUE(parsed_.Parse(
        header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));
  }

  App app;
  rtc::scoped_ptr<RawPacket> packet;
  const App& parsed() { return parsed_; }

 private:
  App parsed_;
};

TEST_F(RtcpPacketAppTest, WithNoData) {
  app.WithSubType(30);
  app.WithName(kName);

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(30U, parsed().sub_type());
  EXPECT_EQ(kName, parsed().name());
  EXPECT_EQ(0u, parsed().data_size());
}

TEST_F(RtcpPacketAppTest, WithData) {
  app.From(kSenderSsrc);
  app.WithSubType(30);
  app.WithName(kName);
  const uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
  const size_t kDataLength = sizeof(kData) / sizeof(kData[0]);
  app.WithData(kData, kDataLength);

  BuildPacket();
  ParsePacket();

  EXPECT_EQ(30U, parsed().sub_type());
  EXPECT_EQ(kName, parsed().name());
  EXPECT_EQ(kDataLength, parsed().data_size());
  EXPECT_EQ(0, memcmp(kData, parsed().data(), kDataLength));
}

}  // namespace
}  // namespace webrtc
