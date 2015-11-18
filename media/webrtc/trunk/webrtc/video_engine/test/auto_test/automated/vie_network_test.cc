/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace {

class RtcpCollectorTransport : public webrtc::Transport {
 public:
  RtcpCollectorTransport() : packets_() {}
  virtual ~RtcpCollectorTransport() {}

  int SendPacket(int /*channel*/,
                 const void* /*data*/,
                 size_t /*len*/) override {
    EXPECT_TRUE(false);
    return 0;
  }
  int SendRTCPPacket(int channel, const void* data, size_t len) override {
    const uint8_t* buf = static_cast<const uint8_t*>(data);
    webrtc::RtpUtility::RtpHeaderParser parser(buf, len);
    if (parser.RTCP()) {
      Packet p;
      p.channel = channel;
      p.length = len;
      if (parser.ParseRtcp(&p.header)) {
        if (p.header.payloadType == 201 && len >= 20) {
          buf += 20;
          len -= 20;
        } else {
          return 0;
        }
        if (TryParseREMB(buf, len, &p)) {
          packets_.push_back(p);
        }
      }
    }
    return 0;
  }

  bool FindREMBFor(uint32_t ssrc, double min_rate) const {
    for (std::vector<Packet>::const_iterator it = packets_.begin();
        it != packets_.end(); ++it) {
      if (it->remb_bitrate >= min_rate && it->remb_ssrc.end() !=
          std::find(it->remb_ssrc.begin(), it->remb_ssrc.end(), ssrc)) {
        return true;
      }
    }
    return false;
  }

 private:
  struct Packet {
    Packet() : channel(-1), length(0), header(), remb_bitrate(0), remb_ssrc() {}
    int channel;
    size_t length;
    webrtc::RTPHeader header;
    double remb_bitrate;
    std::vector<uint32_t> remb_ssrc;
  };

  bool TryParseREMB(const uint8_t* buf, size_t length, Packet* p) {
    if (length < 8) {
      return false;
    }
    if (buf[0] != 'R' || buf[1] != 'E' || buf[2] != 'M' || buf[3] != 'B') {
      return false;
    }
    size_t ssrcs = buf[4];
    uint8_t exp = buf[5] >> 2;
    uint32_t mantissa = ((buf[5] & 0x03) << 16) + (buf[6] << 8) + buf[7];
    double bitrate = mantissa * static_cast<double>(1 << exp);
    p->remb_bitrate = bitrate;

    if (length < (8 + 4 * ssrcs)) {
      return false;
    }
    buf += 8;
    for (size_t i = 0; i < ssrcs; ++i) {
      uint32_t ssrc = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
      p->remb_ssrc.push_back(ssrc);
      buf += 4;
    }
    return true;
  }

  std::vector<Packet> packets_;
};

class ViENetworkTest : public testing::Test {
 protected:
  ViENetworkTest() : vie_("ViENetworkTest"), channel_(-1), transport() {}
  virtual ~ViENetworkTest() {}

  void SetUp() override {
    EXPECT_EQ(0, vie_.base->CreateChannel(channel_));
    EXPECT_EQ(0, vie_.rtp_rtcp->SetRembStatus(channel_, false, true));
    EXPECT_EQ(0, vie_.network->RegisterSendTransport(channel_, transport));
  }

  void TearDown() override {
    EXPECT_EQ(0, vie_.network->DeregisterSendTransport(channel_));
  }

  void ReceiveASTPacketsForBWE() {
    for (int i = 0; i < kPacketCount; ++i) {
      int64_t time = webrtc::TickTime::MillisecondTimestamp();
      webrtc::RTPHeader header;
      header.ssrc = kSsrc1;
      header.timestamp = i * 45000;
      header.extension.hasAbsoluteSendTime = true;
      header.extension.absoluteSendTime = i << (18 - 6);
      EXPECT_EQ(0, vie_.network->ReceivedBWEPacket(channel_, time, kPacketSize,
                                                   header));
      webrtc::SleepMs(kIntervalMs);
    }
  }

  enum {
    kSsrc1 = 667,
    kSsrc2 = 668,
    kPacketCount = 100,
    kPacketSize = 1000,
    kIntervalMs = 22
  };
  TbInterfaces vie_;
  int channel_;
  RtcpCollectorTransport transport;
};

TEST_F(ViENetworkTest, ReceiveBWEPacket_NoExtension)  {
  for (int i = 0; i < kPacketCount; ++i) {
    int64_t time = webrtc::TickTime::MillisecondTimestamp();
    webrtc::RTPHeader header;
    header.ssrc = kSsrc1;
    header.timestamp = i * 45000;
    EXPECT_EQ(0, vie_.network->ReceivedBWEPacket(channel_, time, kPacketSize,
                                                 header));
    webrtc::SleepMs(kIntervalMs);
  }
  EXPECT_FALSE(transport.FindREMBFor(kSsrc1, 0.0));
  unsigned int bandwidth = 0;
  EXPECT_EQ(0, vie_.rtp_rtcp->GetEstimatedReceiveBandwidth(channel_,
                                                           &bandwidth));
}

TEST_F(ViENetworkTest, ReceiveBWEPacket_TOF)  {
  EXPECT_EQ(0, vie_.rtp_rtcp->SetReceiveTimestampOffsetStatus(channel_, true,
                                                              1));
  for (int i = 0; i < kPacketCount; ++i) {
    int64_t time = webrtc::TickTime::MillisecondTimestamp();
    webrtc::RTPHeader header;
    header.ssrc = kSsrc1;
    header.timestamp = i * 45000;
    header.extension.hasTransmissionTimeOffset = true;
    header.extension.transmissionTimeOffset = 17;
    EXPECT_EQ(0, vie_.network->ReceivedBWEPacket(channel_, time, kPacketSize,
                                                 header));
    webrtc::SleepMs(kIntervalMs);
  }
  EXPECT_FALSE(transport.FindREMBFor(kSsrc1, 0.0));
  unsigned int bandwidth = 0;
  EXPECT_EQ(0, vie_.rtp_rtcp->GetEstimatedReceiveBandwidth(channel_,
                                                           &bandwidth));
}

TEST_F(ViENetworkTest, ReceiveBWEPacket_AST)  {
  EXPECT_EQ(0, vie_.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(channel_, true,
                                                               1));
  ReceiveASTPacketsForBWE();
  EXPECT_TRUE(transport.FindREMBFor(kSsrc1, 100000.0));
  unsigned int bandwidth = 0;
  EXPECT_EQ(0, vie_.rtp_rtcp->GetEstimatedReceiveBandwidth(channel_,
                                                           &bandwidth));
  EXPECT_GT(bandwidth, 0u);
}

TEST_F(ViENetworkTest, ReceiveBWEPacket_ASTx2)  {
  EXPECT_EQ(0, vie_.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(channel_, true,
                                                               1));
  for (int i = 0; i < kPacketCount; ++i) {
    int64_t time = webrtc::TickTime::MillisecondTimestamp();
    webrtc::RTPHeader header;
    header.ssrc = kSsrc1;
    header.timestamp = i * 45000;
    header.extension.hasAbsoluteSendTime = true;
    header.extension.absoluteSendTime = i << (18 - 6);
    EXPECT_EQ(0, vie_.network->ReceivedBWEPacket(channel_, time, kPacketSize,
                                                 header));
    header.ssrc = kSsrc2;
    header.timestamp += 171717;
    EXPECT_EQ(0, vie_.network->ReceivedBWEPacket(channel_, time, kPacketSize,
                                                 header));
    webrtc::SleepMs(kIntervalMs);
  }
  EXPECT_TRUE(transport.FindREMBFor(kSsrc1, 200000.0));
  EXPECT_TRUE(transport.FindREMBFor(kSsrc2, 200000.0));
  unsigned int bandwidth = 0;
  EXPECT_EQ(0, vie_.rtp_rtcp->GetEstimatedReceiveBandwidth(channel_,
                                                           &bandwidth));
  EXPECT_GT(bandwidth, 0u);
}

TEST_F(ViENetworkTest, ReceiveBWEPacket_AST_DisabledReceive)  {
  EXPECT_EQ(0, vie_.rtp_rtcp->SetReceiveAbsoluteSendTimeStatus(channel_, false,
                                                               1));
  ReceiveASTPacketsForBWE();
  EXPECT_FALSE(transport.FindREMBFor(kSsrc1, 0.0));
  unsigned int bandwidth = 0;
  EXPECT_EQ(0, vie_.rtp_rtcp->GetEstimatedReceiveBandwidth(channel_,
                                                           &bandwidth));
}
}  // namespace
