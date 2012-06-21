/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RTCPSender.
 */

#include <gtest/gtest.h>

#include "common_types.h"
#include "rtp_utility.h"
#include "rtcp_sender.h"
#include "rtcp_receiver.h"
#include "rtp_rtcp_impl.h"

namespace webrtc {

void CreateRtpPacket(const bool marker_bit, const WebRtc_UWord8 payload,
    const WebRtc_UWord16 seq_num, const WebRtc_UWord32 timestamp,
    const WebRtc_UWord32 ssrc, WebRtc_UWord8* array,
    WebRtc_UWord16* cur_pos) {
  ASSERT_TRUE(payload <= 127);
  array[(*cur_pos)++] = 0x80;
  array[(*cur_pos)++] = payload | (marker_bit ? 0x80 : 0);
  array[(*cur_pos)++] = seq_num >> 8;
  array[(*cur_pos)++] = seq_num;
  array[(*cur_pos)++] = timestamp >> 24;
  array[(*cur_pos)++] = timestamp >> 16;
  array[(*cur_pos)++] = timestamp >> 8;
  array[(*cur_pos)++] = timestamp;
  array[(*cur_pos)++] = ssrc >> 24;
  array[(*cur_pos)++] = ssrc >> 16;
  array[(*cur_pos)++] = ssrc >> 8;
  array[(*cur_pos)++] = ssrc;
  // VP8 payload header
  array[(*cur_pos)++] = 0x90;  // X bit = 1
  array[(*cur_pos)++] = 0x20;  // T bit = 1
  array[(*cur_pos)++] = 0x00;  // TID = 0
  array[(*cur_pos)++] = 0x00;  // Key frame
  array[(*cur_pos)++] = 0x00;
  array[(*cur_pos)++] = 0x00;
  array[(*cur_pos)++] = 0x9d;
  array[(*cur_pos)++] = 0x01;
  array[(*cur_pos)++] = 0x2a;
  array[(*cur_pos)++] = 128;
  array[(*cur_pos)++] = 0;
  array[(*cur_pos)++] = 96;
  array[(*cur_pos)++] = 0;
}

class TestTransport : public Transport,
                      public RtpData {
 public:
  TestTransport(RTCPReceiver* rtcp_receiver) :
    rtcp_receiver_(rtcp_receiver) {
  }

  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    return -1;
  }

  virtual int SendRTCPPacket(int /*ch*/, const void *packet, int packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser((WebRtc_UWord8*)packet,
                                         (WebRtc_Word32)packet_len,
                                         true); // Allow non-compound RTCP

    EXPECT_TRUE(rtcpParser.IsValid());
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                   &rtcpParser));
    rtcp_packet_info_ = rtcpPacketInformation;

    return packet_len;
  }

  virtual int OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadSize,
                                    const WebRtcRTPHeader* rtpHeader)
                                    {return 0;}
  RTCPReceiver* rtcp_receiver_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
};

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest() {
    system_clock_ = ModuleRTPUtility::GetSystemClock();
    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(0, false, system_clock_);
    rtcp_sender_ = new RTCPSender(0, false, system_clock_, rtp_rtcp_impl_);
    rtcp_receiver_ = new RTCPReceiver(0, system_clock_, rtp_rtcp_impl_);
    test_transport_ = new TestTransport(rtcp_receiver_);
    // Initialize
    EXPECT_EQ(0, rtcp_sender_->Init());
    EXPECT_EQ(0, rtcp_sender_->RegisterSendTransport(test_transport_));
    EXPECT_EQ(0, rtp_rtcp_impl_->RegisterIncomingDataCallback(test_transport_));
  }
  ~RtcpSenderTest() {
    delete rtcp_sender_;
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
    delete system_clock_;
  }

  RtpRtcpClock* system_clock_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;

  enum {kMaxPacketLength = 1500};
  uint8_t packet_[kMaxPacketLength];
};

TEST_F(RtcpSenderTest, RtcpOff) {
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpOff));
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(kRtcpSr));
}

TEST_F(RtcpSenderTest, IJStatus) {
  ASSERT_FALSE(rtcp_sender_->IJ());
  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  ASSERT_TRUE(rtcp_sender_->IJ());
}

TEST_F(RtcpSenderTest, TestCompound) {
  const bool marker_bit = false;
  const WebRtc_UWord8 payload = 100;
  const WebRtc_UWord16 seq_num = 11111;
  const WebRtc_UWord32 timestamp = 1234567;
  const WebRtc_UWord32 ssrc = 0x11111111;
  WebRtc_UWord16 packet_length = 0;
  CreateRtpPacket(marker_bit, payload, seq_num, timestamp, ssrc, packet_,
      &packet_length);
  EXPECT_EQ(25, packet_length);

  VideoCodec codec_inst;
  strncpy(codec_inst.plName, "VP8", webrtc::kPayloadNameSize - 1);
  codec_inst.codecType = webrtc::kVideoCodecVP8;
  codec_inst.plType = payload;
  EXPECT_EQ(0, rtp_rtcp_impl_->RegisterReceivePayload(codec_inst));

  // Make sure RTP packet has been received.
  EXPECT_EQ(0, rtp_rtcp_impl_->IncomingPacket(packet_, packet_length));

  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(kRtcpRr));

  // Transmission time offset packet should be received.
  ASSERT_TRUE(test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags &
      kRtcpTransmissionTimeOffset);
}

TEST_F(RtcpSenderTest, TestCompound_NoRtpReceived) {
  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(kRtcpRr));

  // Transmission time offset packet should not be received.
  ASSERT_FALSE(test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags &
      kRtcpTransmissionTimeOffset);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
}  // namespace webrtc
