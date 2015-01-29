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

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/pacing/include/mock/mock_paced_sender.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace {
const int kTransmissionTimeOffsetExtensionId = 1;
const int kAbsoluteSendTimeExtensionId = 14;
const int kPayload = 100;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const int kTimeOffset = 22222;
const int kMaxPacketLength = 1500;
const uint32_t kAbsoluteSendTime = 0x00aabbcc;
const uint8_t kAudioLevel = 0x5a;
const uint8_t kAudioLevelExtensionId = 9;
const int kAudioPayload = 103;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
}  // namespace

using testing::_;

const uint8_t* GetPayloadData(const RTPHeader& rtp_header,
                              const uint8_t* packet) {
  return packet + rtp_header.headerLength;
}

uint16_t GetPayloadDataLength(const RTPHeader& rtp_header,
                              const uint16_t packet_length) {
  uint16_t length = packet_length - rtp_header.headerLength -
      rtp_header.paddingLength;
  return static_cast<uint16_t>(length);
}

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return 0x00fffffful & ((time_ms << 18) / 1000);
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest()
      : packets_sent_(0), last_sent_packet_len_(0), total_bytes_sent_(0) {}
  virtual int SendPacket(int channel, const void *data, int len) OVERRIDE {
    packets_sent_++;
    memcpy(last_sent_packet_, data, len);
    last_sent_packet_len_ = len;
    total_bytes_sent_ += static_cast<size_t>(len);
    return len;
  }
  virtual int SendRTCPPacket(int channel, const void *data, int len) OVERRIDE {
    return -1;
  }
  int packets_sent_;
  int last_sent_packet_len_;
  size_t total_bytes_sent_;
  uint8_t last_sent_packet_[kMaxPacketLength];
};

class RtpSenderTest : public ::testing::Test {
 protected:
  RtpSenderTest()
      : fake_clock_(kStartTime),
        mock_paced_sender_(),
        rtp_sender_(),
        payload_(kPayload),
        transport_(),
        kMarkerBit(true) {
    EXPECT_CALL(mock_paced_sender_,
        SendPacket(_, _, _, _, _, _)).WillRepeatedly(testing::Return(true));
  }

  virtual void SetUp() OVERRIDE {
    rtp_sender_.reset(new RTPSender(0, false, &fake_clock_, &transport_, NULL,
                                    &mock_paced_sender_, NULL, NULL, NULL));
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }

  SimulatedClock fake_clock_;
  MockPacedSender mock_paced_sender_;
  scoped_ptr<RTPSender> rtp_sender_;
  int payload_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  uint8_t packet_[kMaxPacketLength];

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header) {
    EXPECT_EQ(kMarkerBit, rtp_header.markerBit);
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(0, rtp_header.numCSRCs);
    EXPECT_EQ(0, rtp_header.paddingLength);
  }

  void SendPacket(int64_t capture_time_ms, int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                     kPayload,
                                                     kMarkerBit,
                                                     timestamp,
                                                     capture_time_ms);

    // Packet should be stored in a send bucket.
    EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                            payload_length,
                                            rtp_length,
                                            capture_time_ms,
                                            kAllowRetransmission,
                                            PacedSender::kNormalPriority));
  }
};

TEST_F(RtpSenderTest, RegisterRtpTransmissionTimeOffsetHeaderExtension) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, RegisterRtpAbsoluteSendTimeHeaderExtension) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kAbsoluteSendTimeLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, RegisterRtpAudioLevelHeaderExtension) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kAudioLevelLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, RegisterRtpHeaderExtensions) {
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength +
      kAbsoluteSendTimeLength, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength +
      kAbsoluteSendTimeLength + kAudioLevelLength,
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(kRtpOneByteHeaderLength + kAbsoluteSendTimeLength +
      kAudioLevelLength, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(kRtpOneByteHeaderLength + kAudioLevelLength,
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTest, BuildRTPPacket) {
  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize, length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, NULL);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_FALSE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header.extension.hasAudioLevel);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header.extension.absoluteSendTime);
  EXPECT_EQ(0u, rtp_header.extension.audioLevel);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithTransmissionOffsetExtension) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
      length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithNegativeTransmissionOffsetExtension) {
  const int kNegTimeOffset = -500;
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kNegTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
      length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kNegTimeOffset, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithAbsoluteSendTimeExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
      length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
      length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  // Expect kAudioLevel + 0x80 because we set "voiced" to true in the call to
  // UpdateAudioLevel(), above.
  EXPECT_EQ(kAudioLevel + 0x80u, rtp_header.extension.audioLevel);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithHeaderExtensions) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
      length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);
  EXPECT_EQ(kAudioLevel + 0x80u, rtp_header.extension.audioLevel);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
}

TEST_F(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, kSeqNum, _, _, _)).
                  WillOnce(testing::Return(false));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                   kPayload,
                                                   kMarkerBit,
                                                   kTimestamp,
                                                   capture_time_ms);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission,
                                          PacedSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent_);

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms, false);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                                 rtp_length);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, kSeqNum, _, _, _)).
                  WillOnce(testing::Return(false));

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                   kPayload,
                                                   kMarkerBit,
                                                   kTimestamp,
                                                   capture_time_ms);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission,
                                          PacedSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent_);

  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kHighPriority, _, kSeqNum, _, _, _)).
                  WillOnce(testing::Return(false));

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(rtp_length, rtp_sender_->ReSendPacket(kSeqNum));
  EXPECT_EQ(0, transport_.packets_sent_);

  rtp_sender_->TimeToSendPacket(kSeqNum, capture_time_ms, false);

  // Process send bucket. Packet should now be sent.
  EXPECT_EQ(1, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);

  // Parse sent packet.
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                                 rtp_length);
  webrtc::RTPHeader rtp_header;
  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

// This test sends 1 regular video packet, then 4 padding packets, and then
// 1 more regular packet.
TEST_F(RtpSenderTest, SendPadding) {
  // Make all (non-padding) packets go to send queue.
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, _, _, _, _)).
                  WillRepeatedly(testing::Return(false));

  uint16_t seq_num = kSeqNum;
  uint32_t timestamp = kTimestamp;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int32_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  // Create and set up parser.
  scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != NULL);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  webrtc::RTPHeader rtp_header;

  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int32_t rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                                   kPayload,
                                                   kMarkerBit,
                                                   timestamp,
                                                   capture_time_ms);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission,
                                          PacedSender::kNormalPriority));

  int total_packets_sent = 0;
  EXPECT_EQ(total_packets_sent, transport_.packets_sent_);

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);
  rtp_sender_->TimeToSendPacket(seq_num++, capture_time_ms, false);
  // Packet should now be sent. This test doesn't verify the regular video
  // packet, since it is tested in another test.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
  timestamp += 90 * kStoredTimeInMs;

  // Send padding 4 times, waiting 50 ms between each.
  for (int i = 0; i < 4; ++i) {
    const int kPaddingPeriodMs = 50;
    const int kPaddingBytes = 100;
    const int kMaxPaddingLength = 224;  // Value taken from rtp_sender.cc.
    // Padding will be forced to full packets.
    EXPECT_EQ(kMaxPaddingLength, rtp_sender_->TimeToSendPadding(kPaddingBytes));

    // Process send bucket. Padding should now be sent.
    EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
    EXPECT_EQ(kMaxPaddingLength + rtp_header_len,
              transport_.last_sent_packet_len_);
    // Parse sent packet.
    ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_, kPaddingBytes,
                                  &rtp_header));

    // Verify sequence number and timestamp.
    EXPECT_EQ(seq_num++, rtp_header.sequenceNumber);
    EXPECT_EQ(timestamp, rtp_header.timestamp);
    // Verify transmission time offset.
    EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
    uint64_t expected_send_time =
        ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
    EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
    fake_clock_.AdvanceTimeMilliseconds(kPaddingPeriodMs);
    timestamp += 90 * kPaddingPeriodMs;
  }

  // Send a regular video packet again.
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  rtp_length = rtp_sender_->BuildRTPheader(packet_,
                                           kPayload,
                                           kMarkerBit,
                                           timestamp,
                                           capture_time_ms);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_,
                                          0,
                                          rtp_length,
                                          capture_time_ms,
                                          kAllowRetransmission,
                                          PacedSender::kNormalPriority));

  rtp_sender_->TimeToSendPacket(seq_num, capture_time_ms, false);
  // Process send bucket.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_, rtp_length,
                                &rtp_header));

  // Verify sequence number and timestamp.
  EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
  EXPECT_EQ(timestamp, rtp_header.timestamp);
  // Verify transmission time offset. This packet is sent without delay.
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, SendRedundantPayloads) {
  MockTransport transport;
  rtp_sender_.reset(new RTPSender(0, false, &fake_clock_, &transport, NULL,
                                  &mock_paced_sender_, NULL, NULL, NULL));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  // Make all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, _, _, _, _)).
                  WillRepeatedly(testing::Return(false));

  uint16_t seq_num = kSeqNum;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int32_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  rtp_sender_->SetRTXStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender_->SetRtxSsrc(1234);

  // Create and set up parser.
  scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != NULL);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  rtp_sender_->SetTargetBitrate(300000);
  const size_t kNumPayloadSizes = 10;
  const size_t kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700, 750,
      800, 850, 900, 950};
  // Send 10 packets of increasing size.
  for (size_t i = 0; i < kNumPayloadSizes; ++i) {
    int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
    EXPECT_CALL(transport, SendPacket(_, _, _))
        .WillOnce(testing::ReturnArg<2>());
    SendPacket(capture_time_ms, kPayloadSizes[i]);
    rtp_sender_->TimeToSendPacket(seq_num++, capture_time_ms, false);
    fake_clock_.AdvanceTimeMilliseconds(33);
  }
  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport,
              SendPacket(_, _, kMaxPaddingSize + rtp_header_len))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kMaxPaddingSize,
            static_cast<size_t>(rtp_sender_->TimeToSendPadding(49)));

  const int kRtxHeaderSize = 2;
  EXPECT_CALL(transport, SendPacket(_, _, kPayloadSizes[0] +
                                    rtp_header_len + kRtxHeaderSize))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kPayloadSizes[0],
            static_cast<size_t>(rtp_sender_->TimeToSendPadding(500)));

  EXPECT_CALL(transport, SendPacket(_, _, kPayloadSizes[kNumPayloadSizes - 1] +
                                    rtp_header_len + kRtxHeaderSize))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_CALL(transport, SendPacket(_, _, kMaxPaddingSize + rtp_header_len))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kMaxPaddingSize,
            static_cast<size_t>(rtp_sender_->TimeToSendPadding(999)));
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

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  const uint8_t* payload_data = GetPayloadData(rtp_header,
      transport_.last_sent_packet_);
  uint8_t generic_header = *payload_data++;

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

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

  RtpUtility::RtpHeaderParser rtp_parser2(transport_.last_sent_packet_,
                                          transport_.last_sent_packet_len_);
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  payload_data = GetPayloadData(rtp_header, transport_.last_sent_packet_);
  generic_header = *payload_data++;

  EXPECT_FALSE(generic_header & RtpFormatVideoGeneric::kKeyFrameBit);
  EXPECT_TRUE(generic_header & RtpFormatVideoGeneric::kFirstPacketBit);

  ASSERT_EQ(sizeof(payload) + sizeof(generic_header),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

TEST_F(RtpSenderTest, FrameCountCallbacks) {
  class TestCallback : public FrameCountObserver {
   public:
    TestCallback()
      : FrameCountObserver(), num_calls_(0), ssrc_(0),
        key_frames_(0), delta_frames_(0) {}
    virtual ~TestCallback() {}

    virtual void FrameCountUpdated(FrameType frame_type,
                                   uint32_t frame_count,
                                   const unsigned int ssrc) OVERRIDE {
      ++num_calls_;
      ssrc_ = ssrc;
      switch (frame_type) {
        case kVideoFrameDelta:
          delta_frames_ = frame_count;
          break;
        case kVideoFrameKey:
          key_frames_ = frame_count;
          break;
        default:
          break;
      }
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    uint32_t key_frames_;
    uint32_t delta_frames_;
  } callback;

  rtp_sender_.reset(new RTPSender(0, false, &fake_clock_, &transport_, NULL,
                                  &mock_paced_sender_, NULL, &callback, NULL));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                             4321, payload, sizeof(payload),
                                             NULL));

  EXPECT_EQ(1U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1U, callback.key_frames_);
  EXPECT_EQ(0U, callback.delta_frames_);

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameDelta,
                                             payload_type, 1234, 4321, payload,
                                             sizeof(payload), NULL));

  EXPECT_EQ(2U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1U, callback.key_frames_);
  EXPECT_EQ(1U, callback.delta_frames_);

  rtp_sender_.reset();
}

TEST_F(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback() : BitrateStatisticsObserver(), num_calls_(0), ssrc_(0) {}
    virtual ~TestCallback() {}

    virtual void Notify(const BitrateStatistics& total_stats,
                        const BitrateStatistics& retransmit_stats,
                        uint32_t ssrc) OVERRIDE {
      ++num_calls_;
      ssrc_ = ssrc;
      total_stats_ = total_stats;
      retransmit_stats_ = retransmit_stats;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    BitrateStatistics total_stats_;
    BitrateStatistics retransmit_stats_;
  } callback;
  rtp_sender_.reset(new RTPSender(0, false, &fake_clock_, &transport_, NULL,
                                  &mock_paced_sender_, &callback, NULL, NULL));

  // Simulate kNumPackets sent with kPacketInterval ms intervals.
  const uint32_t kNumPackets = 15;
  const uint32_t kPacketInterval = 20;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(
      0,
      rtp_sender_->RegisterPayload(payload_name, payload_type, 90000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  // Initial process call so we get a new time window.
  rtp_sender_->ProcessBitrate();
  uint64_t start_time = fake_clock_.CurrentNtpInMilliseconds();

  // Send a few frames.
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    ASSERT_EQ(0,
              rtp_sender_->SendOutgoingData(kVideoFrameKey,
                                            payload_type,
                                            1234,
                                            4321,
                                            payload,
                                            sizeof(payload),
                                            0));
    fake_clock_.AdvanceTimeMilliseconds(kPacketInterval);
  }

  rtp_sender_->ProcessBitrate();

  const uint32_t expected_packet_rate = 1000 / kPacketInterval;

  // We get one call for every stats updated, thus two calls since both the
  // stream stats and the retransmit stats are updated once.
  EXPECT_EQ(2u, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(start_time + (kNumPackets * kPacketInterval),
            callback.total_stats_.timestamp_ms);
  EXPECT_EQ(expected_packet_rate, callback.total_stats_.packet_rate);
  EXPECT_EQ((kPacketOverhead + sizeof(payload)) * 8 * expected_packet_rate,
            callback.total_stats_.bitrate_bps);

  rtp_sender_.reset();
}

class RtpSenderAudioTest : public RtpSenderTest {
 protected:
  RtpSenderAudioTest() {}

  virtual void SetUp() OVERRIDE {
    payload_ = kAudioPayload;
    rtp_sender_.reset(new RTPSender(0, true, &fake_clock_, &transport_, NULL,
                                    &mock_paced_sender_, NULL, NULL, NULL));
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }
};

TEST_F(RtpSenderTest, StreamDataCountersCallbacks) {
  class TestCallback : public StreamDataCountersCallback {
   public:
    TestCallback()
      : StreamDataCountersCallback(), ssrc_(0), counters_() {}
    virtual ~TestCallback() {}

    virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                     uint32_t ssrc) OVERRIDE {
      ssrc_ = ssrc;
      counters_ = counters;
    }

    uint32_t ssrc_;
    StreamDataCounters counters_;
    bool Matches(uint32_t ssrc, uint32_t bytes, uint32_t header_bytes,
                 uint32_t padding, uint32_t packets, uint32_t retransmits,
                 uint32_t fec) {
      return ssrc_ == ssrc &&
          counters_.bytes == bytes &&
          counters_.header_bytes == header_bytes &&
          counters_.padding_bytes == padding &&
          counters_.packets == packets &&
          counters_.retransmitted_packets == retransmits &&
          counters_.fec_packets == fec;
    }

  } callback;

  const uint8_t kRedPayloadType = 96;
  const uint8_t kUlpfecPayloadType = 97;
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  rtp_sender_->RegisterRtpStatisticsCallback(&callback);

  // Send a frame.
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                             4321, payload, sizeof(payload),
                                             NULL));

  // {bytes = 6, header = 12, padding = 0, packets = 1, retrans = 0, fec = 0}
  EXPECT_TRUE(callback.Matches(ssrc, 6, 12, 0, 1, 0, 0));

  // Retransmit a frame.
  uint16_t seqno = rtp_sender_->SequenceNumber() - 1;
  rtp_sender_->ReSendPacket(seqno, 0);

  // bytes = 6, header = 12, padding = 0, packets = 2, retrans = 1, fec = 0}
  EXPECT_TRUE(callback.Matches(ssrc, 6, 12, 0, 2, 1, 0));

  // Send padding.
  rtp_sender_->TimeToSendPadding(kMaxPaddingSize);
  // {bytes = 6, header = 24, padding = 224, packets = 3, retrans = 1, fec = 0}
  EXPECT_TRUE(callback.Matches(ssrc, 6, 24, kMaxPaddingSize, 3, 1, 0));

  // Send FEC.
  rtp_sender_->SetGenericFECStatus(true, kRedPayloadType, kUlpfecPayloadType);
  FecProtectionParams fec_params;
  fec_params.fec_mask_type = kFecMaskRandom;
  fec_params.fec_rate = 1;
  fec_params.max_fec_frames = 1;
  fec_params.use_uep_protection = false;
  rtp_sender_->SetFecParameters(&fec_params, &fec_params);
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameDelta, payload_type,
                                             1234, 4321, payload,
                                             sizeof(payload), NULL));

  // {bytes = 34, header = 48, padding = 224, packets = 5, retrans = 1, fec = 1}
  EXPECT_TRUE(callback.Matches(ssrc, 34, 48, kMaxPaddingSize, 5, 1, 1));

  rtp_sender_->RegisterRtpStatisticsCallback(NULL);
}

TEST_F(RtpSenderAudioTest, SendAudio) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kAudioFrameCN, payload_type, 1234,
                                             4321, payload, sizeof(payload),
                                             NULL));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  const uint8_t* payload_data = GetPayloadData(rtp_header,
      transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload), GetPayloadDataLength(rtp_header,
            transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

TEST_F(RtpSenderAudioTest, SendAudioWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kAudioFrameCN, payload_type, 1234,
                                             4321, payload, sizeof(payload),
                                             NULL));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(rtp_header));

  const uint8_t* payload_data = GetPayloadData(rtp_header,
                                               transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload), GetPayloadDataLength(
      rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));

  uint8_t extension[] = { 0xbe, 0xde, 0x00, 0x01,
                          (kAudioLevelExtensionId << 4) + 0, // ID + length.
                          kAudioLevel,                       // Data.
                          0x00, 0x00                         // Padding.
                        };

  EXPECT_EQ(0, memcmp(extension, payload_data - sizeof(extension),
                      sizeof(extension)));
}

// As RFC4733, named telephone events are carried as part of the audio stream
// and must use the same sequence number and timestamp base as the regular
// audio channel.
// This test checks the marker bit for the first packet and the consequent
// packets of the same telephone event. Since it is specifically for DTMF
// events, ignoring audio packets and sending kFrameEmpty instead of those.
TEST_F(RtpSenderAudioTest, CheckMarkerBitForTelephoneEvents) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "telephone-event";
  uint8_t payload_type = 126;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 0,
                                            0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  strcpy(payload_name, "payload_name");
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 8000,
                                            1, 0));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  rtp_sender_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kFrameEmpty, payload_type,
                                             capture_time_ms,
                                             0, NULL, 0,
                                             NULL));
  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kFrameEmpty, payload_type,
                                             capture_time_ms+2000,
                                             0, NULL, 0,
                                             NULL));
  scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != NULL);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_,
                                &rtp_header));
  // Marker Bit should be set to 1 for first packet.
  EXPECT_TRUE(rtp_header.markerBit);

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kFrameEmpty, payload_type,
                                             capture_time_ms+4000,
                                             0, NULL, 0,
                                             NULL));
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_,
                                &rtp_header));
  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_FALSE(rtp_header.markerBit);
}

TEST_F(RtpSenderTest, BytesReportedCorrectly) {
  const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender_->SetSSRC(1234);
  rtp_sender_->SetRtxSsrc(4321);
  rtp_sender_->SetRtxPayloadType(kPayloadType - 1);
  rtp_sender_->SetRTXStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  ASSERT_EQ(
      0,
      rtp_sender_->RegisterPayload(kPayloadName, kPayloadType, 90000, 0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(0,
            rtp_sender_->SendOutgoingData(kVideoFrameKey,
                                          kPayloadType,
                                          1234,
                                          4321,
                                          payload,
                                          sizeof(payload),
                                          0));

  // Will send 2 full-size padding packets.
  rtp_sender_->TimeToSendPadding(1);
  rtp_sender_->TimeToSendPadding(1);

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload + 1-byte generic header.
  EXPECT_EQ(rtp_stats.bytes, sizeof(payload) + 1);
  EXPECT_EQ(rtp_stats.header_bytes, 12u);
  EXPECT_EQ(rtp_stats.padding_bytes, 0u);
  EXPECT_EQ(rtx_stats.bytes, 0u);
  EXPECT_EQ(rtx_stats.header_bytes, 24u);
  EXPECT_EQ(rtx_stats.padding_bytes, 2 * kMaxPaddingSize);

  EXPECT_EQ(transport_.total_bytes_sent_,
            rtp_stats.bytes + rtp_stats.header_bytes + rtp_stats.padding_bytes +
                rtx_stats.bytes + rtx_stats.header_bytes +
                rtx_stats.padding_bytes);
}
}  // namespace webrtc
