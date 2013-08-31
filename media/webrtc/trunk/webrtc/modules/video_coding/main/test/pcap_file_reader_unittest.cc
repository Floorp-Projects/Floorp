/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/video_coding/main/test/pcap_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace rtpplayer {

typedef std::map<uint32_t, int> PacketsPerSsrc;

class TestPcapFileReader : public ::testing::Test {
 public:
  void Init(const std::string& filename) {
    std::string filepath =
        test::ResourcePath("video_coding/" + filename, "pcap");
    rtp_packet_source_.reset(CreatePcapFileReader(filepath));
    ASSERT_TRUE(rtp_packet_source_.get() != NULL);
  }

  int CountRtpPackets() {
    const uint32_t kBufferSize = 4096;
    uint8_t data[kBufferSize];
    uint32_t length = kBufferSize;
    uint32_t dummy_time_ms = 0;
    int c = 0;
    while (rtp_packet_source_->NextPacket(data, &length, &dummy_time_ms) == 0) {
      EXPECT_GE(kBufferSize, length);
      length = kBufferSize;
      c++;
    }
    return c;
  }

  PacketsPerSsrc CountRtpPacketsPerSsrc() {
    const uint32_t kBufferSize = 4096;
    uint8_t data[kBufferSize];
    uint32_t length = kBufferSize;
    uint32_t dummy_time_ms = 0;
    PacketsPerSsrc pps;
    while (rtp_packet_source_->NextPacket(data, &length, &dummy_time_ms) == 0) {
      EXPECT_GE(kBufferSize, length);
      length = kBufferSize;

      ModuleRTPUtility::RTPHeaderParser rtp_header_parser(data, length);
      webrtc::RTPHeader header;
      if (!rtp_header_parser.RTCP() && rtp_header_parser.Parse(header, NULL)) {
        pps[header.ssrc]++;
      }
    }
    return pps;
  }

 private:
  scoped_ptr<RtpPacketSourceInterface> rtp_packet_source_;
};

TEST_F(TestPcapFileReader, TestEthernetIIFrame) {
  Init("frame-ethernet-ii");
  EXPECT_EQ(368, CountRtpPackets());
}

TEST_F(TestPcapFileReader, TestLoopbackFrame) {
  Init("frame-loopback");
  EXPECT_EQ(491, CountRtpPackets());
}

TEST_F(TestPcapFileReader, TestTwoSsrc) {
  Init("ssrcs-2");
  PacketsPerSsrc pps = CountRtpPacketsPerSsrc();
  EXPECT_EQ(2UL, pps.size());
  EXPECT_EQ(370, pps[0x78d48f61]);
  EXPECT_EQ(60, pps[0xae94130b]);
}

TEST_F(TestPcapFileReader, TestThreeSsrc) {
  Init("ssrcs-3");
  PacketsPerSsrc pps = CountRtpPacketsPerSsrc();
  EXPECT_EQ(3UL, pps.size());
  EXPECT_EQ(162, pps[0x938c5eaa]);
  EXPECT_EQ(113, pps[0x59fe6ef0]);
  EXPECT_EQ(61, pps[0xed2bd2ac]);
}

}  // namespace rtpplayer
}  // namespace webrtc
