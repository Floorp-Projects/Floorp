/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace rtpplayer {

class TestRtpFileReader : public ::testing::Test {
 public:
  void Init(const std::string& filename) {
    std::string filepath =
        test::ResourcePath("video_coding/" + filename, "rtp");
    rtp_packet_source_.reset(CreateRtpFileReader(filepath));
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

 private:
  scoped_ptr<RtpPacketSourceInterface> rtp_packet_source_;
};

TEST_F(TestRtpFileReader, Test60Packets) {
  Init("pltype103");
  EXPECT_EQ(60, CountRtpPackets());
}

}  // namespace rtpplayer
}  // namespace webrtc
