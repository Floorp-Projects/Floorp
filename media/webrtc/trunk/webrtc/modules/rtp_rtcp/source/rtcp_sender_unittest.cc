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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

TEST(NACKStringBuilderTest, TestCase1) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase2) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5-7,9-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase3) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  builder.PushNACK(21);
  EXPECT_EQ(std::string("5,7,9-12,15,18-19,21"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase4) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7-12,15,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase5) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-16,18-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase6) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(15);
  builder.PushNACK(16);
  builder.PushNACK(17);
  builder.PushNACK(18);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9-12,15-19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase7) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(11);
  builder.PushNACK(12);
  builder.PushNACK(13);
  builder.PushNACK(14);
  builder.PushNACK(15);
  EXPECT_EQ(std::string("5-8,11-15"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase8) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(7);
  builder.PushNACK(9);
  builder.PushNACK(11);
  builder.PushNACK(15);
  builder.PushNACK(17);
  builder.PushNACK(19);
  EXPECT_EQ(std::string("5,7,9,11,15,17,19"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase9) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(7);
  builder.PushNACK(8);
  builder.PushNACK(9);
  builder.PushNACK(10);
  builder.PushNACK(11);
  builder.PushNACK(12);
  EXPECT_EQ(std::string("5-12"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase10) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  EXPECT_EQ(std::string("5"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase11) {
  NACKStringBuilder builder;
  EXPECT_EQ(std::string(""), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase12) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  EXPECT_EQ(std::string("5-6"), builder.GetResult());
}

TEST(NACKStringBuilderTest, TestCase13) {
  NACKStringBuilder builder;
  builder.PushNACK(5);
  builder.PushNACK(6);
  builder.PushNACK(9);
  EXPECT_EQ(std::string("5-6,9"), builder.GetResult());
}

void CreateRtpPacket(const bool marker_bit, const uint8_t payload,
    const uint16_t seq_num, const uint32_t timestamp,
    const uint32_t ssrc, uint8_t* array,
    uint16_t* cur_pos) {
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
                      public NullRtpData {
 public:
  TestTransport()
      : rtcp_receiver_(NULL) {
  }
  void SetRTCPReceiver(RTCPReceiver* rtcp_receiver) {
    rtcp_receiver_ = rtcp_receiver;
  }
  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    return -1;
  }

  virtual int SendRTCPPacket(int /*ch*/, const void *packet, int packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser((uint8_t*)packet,
                                         (int32_t)packet_len,
                                         true); // Allow non-compound RTCP

    EXPECT_TRUE(rtcpParser.IsValid());
    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));
    rtcp_packet_info_.rtcpPacketTypeFlags =
        rtcpPacketInformation.rtcpPacketTypeFlags;
    rtcp_packet_info_.remoteSSRC = rtcpPacketInformation.remoteSSRC;
    rtcp_packet_info_.applicationSubType =
        rtcpPacketInformation.applicationSubType;
    rtcp_packet_info_.applicationName = rtcpPacketInformation.applicationName;
    rtcp_packet_info_.report_blocks = rtcpPacketInformation.report_blocks;
    rtcp_packet_info_.rtt = rtcpPacketInformation.rtt;
    rtcp_packet_info_.interArrivalJitter =
        rtcpPacketInformation.interArrivalJitter;
    rtcp_packet_info_.sliPictureId = rtcpPacketInformation.sliPictureId;
    rtcp_packet_info_.rpsiPictureId = rtcpPacketInformation.rpsiPictureId;
    rtcp_packet_info_.receiverEstimatedMaxBitrate =
        rtcpPacketInformation.receiverEstimatedMaxBitrate;
    rtcp_packet_info_.ntp_secs = rtcpPacketInformation.ntp_secs;
    rtcp_packet_info_.ntp_frac = rtcpPacketInformation.ntp_frac;
    rtcp_packet_info_.rtp_timestamp = rtcpPacketInformation.rtp_timestamp;

    return packet_len;
  }

  virtual int OnReceivedPayloadData(const uint8_t* payloadData,
                                    const uint16_t payloadSize,
                                    const WebRtcRTPHeader* rtpHeader) {
    return 0;
  }
  RTCPReceiver* rtcp_receiver_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
};

class RtcpSenderTest : public ::testing::Test {
 protected:
  RtcpSenderTest()
      : over_use_detector_options_(),
        system_clock_(Clock::GetRealTimeClock()),
        rtp_payload_registry_(new RTPPayloadRegistry(
            0, RTPPayloadStrategy::CreateStrategy(false))),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            RemoteBitrateEstimatorFactory().Create(
                &remote_bitrate_observer_,
                system_clock_)),
        receive_statistics_(ReceiveStatistics::Create(system_clock_)) {
    test_transport_ = new TestTransport();

    RtpRtcp::Configuration configuration;
    configuration.id = 0;
    configuration.audio = false;
    configuration.clock = system_clock_;
    configuration.outgoing_transport = test_transport_;
    configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();

    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
    rtp_receiver_.reset(RtpReceiver::CreateVideoReceiver(
        0, system_clock_, test_transport_, NULL, rtp_payload_registry_.get()));
    rtcp_sender_ =
        new RTCPSender(0, false, system_clock_, receive_statistics_.get());
    rtcp_receiver_ = new RTCPReceiver(0, system_clock_, rtp_rtcp_impl_);
    test_transport_->SetRTCPReceiver(rtcp_receiver_);
    // Initialize
    EXPECT_EQ(0, rtcp_sender_->Init());
    EXPECT_EQ(0, rtcp_sender_->RegisterSendTransport(test_transport_));
  }
  ~RtcpSenderTest() {
    delete rtcp_sender_;
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
  }

  // Helper function: Incoming RTCP has a specific packet type.
  bool gotPacketType(RTCPPacketType packet_type) {
    return ((test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags) &
            packet_type) != 0U;
  }

  OverUseDetectorOptions over_use_detector_options_;
  Clock* system_clock_;
  scoped_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  scoped_ptr<RtpReceiver> rtp_receiver_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPSender* rtcp_sender_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
  scoped_ptr<ReceiveStatistics> receive_statistics_;

  enum {kMaxPacketLength = 1500};
  uint8_t packet_[kMaxPacketLength];
};

TEST_F(RtcpSenderTest, RtcpOff) {
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpOff));
  RTCPSender::FeedbackState feedback_state(rtp_rtcp_impl_);
  EXPECT_EQ(-1, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
}

TEST_F(RtcpSenderTest, IJStatus) {
  ASSERT_FALSE(rtcp_sender_->IJ());
  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  ASSERT_TRUE(rtcp_sender_->IJ());
}

TEST_F(RtcpSenderTest, TestCompound) {
  const bool marker_bit = false;
  const uint8_t payload = 100;
  const uint16_t seq_num = 11111;
  const uint32_t timestamp = 1234567;
  const uint32_t ssrc = 0x11111111;
  uint16_t packet_length = 0;
  CreateRtpPacket(marker_bit, payload, seq_num, timestamp, ssrc, packet_,
      &packet_length);
  EXPECT_EQ(25, packet_length);

  VideoCodec codec_inst;
  strncpy(codec_inst.plName, "VP8", webrtc::kPayloadNameSize - 1);
  codec_inst.codecType = webrtc::kVideoCodecVP8;
  codec_inst.plType = payload;
  EXPECT_EQ(0, rtp_receiver_->RegisterReceivePayload(codec_inst.plName,
                                                     codec_inst.plType,
                                                     90000,
                                                     0,
                                                     codec_inst.maxBitrate));

  // Make sure RTP packet has been received.
  scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
  RTPHeader header;
  EXPECT_TRUE(parser->Parse(packet_, packet_length, &header));
  PayloadUnion payload_specific;
  EXPECT_TRUE(rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                        &payload_specific));
  receive_statistics_->IncomingPacket(header, packet_length, false);
  EXPECT_TRUE(rtp_receiver_->IncomingRtpPacket(header, packet_, packet_length,
                                               payload_specific, true));

  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  RTCPSender::FeedbackState feedback_state(rtp_rtcp_impl_);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRr));

  // Transmission time offset packet should be received.
  ASSERT_TRUE(test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags &
      kRtcpTransmissionTimeOffset);
}

TEST_F(RtcpSenderTest, TestCompound_NoRtpReceived) {
  EXPECT_EQ(0, rtcp_sender_->SetIJStatus(true));
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  RTCPSender::FeedbackState feedback_state(rtp_rtcp_impl_);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpRr));

  // Transmission time offset packet should not be received.
  ASSERT_FALSE(test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags &
      kRtcpTransmissionTimeOffset);
}

// This test is written to verify actual behaviour. It does not seem
// to make much sense to send an empty TMMBN, since there is no place
// to put an actual limit here. It's just information that no limit
// is set, which is kind of the starting assumption.
// See http://code.google.com/p/webrtc/issues/detail?id=468 for one
// situation where this caused confusion.
TEST_F(RtcpSenderTest, SendsTmmbnIfSetAndEmpty) {
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  TMMBRSet bounding_set;
  EXPECT_EQ(0, rtcp_sender_->SetTMMBN(&bounding_set, 3));
  ASSERT_EQ(0U, test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags);
  RTCPSender::FeedbackState feedback_state(rtp_rtcp_impl_);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state,kRtcpSr));
  // We now expect the packet to show up in the rtcp_packet_info_ of
  // test_transport_.
  ASSERT_NE(0U, test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_TRUE(gotPacketType(kRtcpTmmbn));
  TMMBRSet* incoming_set = NULL;
  bool owner = false;
  // The BoundingSet function returns the number of members of the
  // bounding set, and touches the incoming set only if there's > 1.
  EXPECT_EQ(0, test_transport_->rtcp_receiver_->BoundingSet(owner,
      incoming_set));
}

TEST_F(RtcpSenderTest, SendsTmmbnIfSetAndValid) {
  EXPECT_EQ(0, rtcp_sender_->SetRTCPStatus(kRtcpCompound));
  TMMBRSet bounding_set;
  bounding_set.VerifyAndAllocateSet(1);
  const uint32_t kSourceSsrc = 12345;
  bounding_set.AddEntry(32768, 0, kSourceSsrc);

  EXPECT_EQ(0, rtcp_sender_->SetTMMBN(&bounding_set, 3));
  ASSERT_EQ(0U, test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags);
  RTCPSender::FeedbackState feedback_state(rtp_rtcp_impl_);
  EXPECT_EQ(0, rtcp_sender_->SendRTCP(feedback_state, kRtcpSr));
  // We now expect the packet to show up in the rtcp_packet_info_ of
  // test_transport_.
  ASSERT_NE(0U, test_transport_->rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_TRUE(gotPacketType(kRtcpTmmbn));
  TMMBRSet incoming_set;
  bool owner = false;
  // We expect 1 member of the incoming set.
  EXPECT_EQ(1, test_transport_->rtcp_receiver_->BoundingSet(owner,
      &incoming_set));
  EXPECT_EQ(kSourceSsrc, incoming_set.Ssrc(0));
}
}  // namespace webrtc
