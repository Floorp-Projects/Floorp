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
 * This file includes unit tests for the RTPSender.
 */

#include <gtest/gtest.h>

#include "webrtc/modules/pacing/include/mock/mock_paced_sender.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace {
const int kId = 1;
const int kTypeLength = TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
const int kPayload = 100;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const int kTimeOffset = 22222;
const int kMaxPacketLength = 1500;
}  // namespace

using testing::_;

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest()
    : packets_sent_(0),
      last_sent_packet_len_(0) {
  }
  virtual int SendPacket(int channel, const void *data, int len) {
    packets_sent_++;
    memcpy(last_sent_packet_, data, len);
    last_sent_packet_len_ = len;
    return len;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) {
    return -1;
  }
  int packets_sent_;
  int last_sent_packet_len_;
  uint8_t last_sent_packet_[kMaxPacketLength];
};

class RtpSenderTest : public ::testing::Test {
 protected:
  RtpSenderTest()
    : fake_clock_(123456),
      mock_paced_sender_(),
      rtp_sender_(new RTPSender(0, false, &fake_clock_, &transport_, NULL,
                                &mock_paced_sender_)),
      kMarkerBit(true),
      kType(kRtpExtensionTransmissionTimeOffset) {
    rtp_sender_->SetSequenceNumber(kSeqNum);
    EXPECT_CALL(mock_paced_sender_,
        SendPacket(_, _, _, _, _)).WillRepeatedly(testing::Return(true));
  }
  SimulatedClock fake_clock_;
  MockPacedSender mock_paced_sender_;
  scoped_ptr<RTPSender> rtp_sender_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  RTPExtensionType kType;
  uint8_t packet_[kMaxPacketLength];

  void VerifyRTPHeaderCommon(const WebRtcRTPHeader& rtp_header) {
    EXPECT_EQ(kMarkerBit, rtp_header.header.markerBit);
    EXPECT_EQ(kPayload, rtp_header.header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.header.ssrc);
    EXPECT_EQ(0, rtp_header.header.numCSRCs);
    EXPECT_EQ(0, rtp_header.header.paddingLength);
  }
};

TEST_F(RtpSenderTest, RegisterRtpHeaderExtension) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));
  EXPECT_EQ(RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES + kTypeLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(kType));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, BuildRTPPacket) {
  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp);
  EXPECT_EQ(12, length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::WebRtcRTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.header.headerLength);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithTransmissionOffsetExtension) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp);
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::WebRtcRTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.header.headerLength);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::WebRtcRTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.header.headerLength);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithNegativeTransmissionOffsetExtension) {
  const int kNegTimeOffset = -500;
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kNegTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp);
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::WebRtcRTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.header.headerLength);
  EXPECT_EQ(kNegTimeOffset, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, TrafficSmoothingWithTimeOffset) {
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, kSeqNum, _, _)).
                  WillOnce(testing::Return(false));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));
  rtp_sender_->SetTargetSendBitrate(300000);
  int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                   kPayload,
                                                   kMarkerBit,
                                                   kTimestamp);

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission));

  EXPECT_EQ(0, transport_.packets_sent_);

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(
      transport_.last_sent_packet_, rtp_length);
  webrtc::WebRtcRTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, kSeqNum, _, _)).
                  WillOnce(testing::Return(false));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kType, kId));
  rtp_sender_->SetTargetSendBitrate(300000);
  int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                   kPayload,
                                                   kMarkerBit,
                                                   kTimestamp);

  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission));

  EXPECT_EQ(0, transport_.packets_sent_);

  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kHighPriority, _, kSeqNum, _, _)).
                  WillOnce(testing::Return(false));

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(0, rtp_sender_->ReSendPacket(kSeqNum));
  EXPECT_EQ(0, transport_.packets_sent_);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);

  // Parse sent packet.
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(
      transport_.last_sent_packet_, rtp_length);
  webrtc::WebRtcRTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kType, kId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, SendGenericVideo) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                             4321, payload, sizeof(payload),
                                             NULL));

  ModuleRTPUtility::RTPHeaderParser rtp_parser(transport_.last_sent_packet_,
      transport_.last_sent_packet_len_);
  webrtc::WebRtcRTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  const uint8_t* payload_data = ModuleRTPUtility::GetPayloadData(&rtp_header,
      transport_.last_sent_packet_);
  uint8_t generic_header = *payload_data++;

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            ModuleRTPUtility::GetPayloadDataLength(&rtp_header,
            transport_.last_sent_packet_len_));

  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));

  // Send delta frame
  payload[0] = 13;
  payload[1] = 42;
  payload[4] = 13;

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameDelta, payload_type,
                                             1234, 4321, payload,
                                             sizeof(payload), NULL));

  ModuleRTPUtility::RTPHeaderParser rtp_parser2(transport_.last_sent_packet_,
      transport_.last_sent_packet_len_);
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  payload_data = ModuleRTPUtility::GetPayloadData(&rtp_header,
      transport_.last_sent_packet_);
  generic_header = *payload_data++;

  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            ModuleRTPUtility::GetPayloadDataLength(&rtp_header,
      transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

}  // namespace webrtc

