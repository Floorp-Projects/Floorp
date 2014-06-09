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
      : fake_clock_(kStartTime),
        mock_paced_sender_(),
        rtp_sender_(),
        payload_(kPayload),
        transport_(),
        kMarkerBit(true) {
    EXPECT_CALL(mock_paced_sender_,
        SendPacket(_, _, _, _, _, _)).WillRepeatedly(testing::Return(true));
  }

  virtual void SetUp() {
    rtp_sender_.reset(new RTPSender(0, false, &fake_clock_, &transport_, NULL,
                                    &mock_paced_sender_));
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
  // Accounted size for audio level is zero because it is currently specially
  // treated by RTPSenderAudio.
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
  // EXPECT_EQ(kRtpOneByteHeaderLength + kAudioLevelLength,
  //           rtp_sender_->RtpHeaderExtensionTotalLength());
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
      kAbsoluteSendTimeLength, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(kRtpOneByteHeaderLength + kAbsoluteSendTimeLength,
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());
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
  EXPECT_EQ(12, length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header.extension.absoluteSendTime);
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
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
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
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
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
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, BuildRTPPacketWithHeaderExtensions) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionTransmissionTimeOffset, kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
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
  rtp_sender_->SetTargetSendBitrate(300000);
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
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(
      transport_.last_sent_packet_, rtp_length);
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
  rtp_sender_->SetTargetSendBitrate(300000);
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
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(
      transport_.last_sent_packet_, rtp_length);
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
  int rtp_header_len = 12;
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

  rtp_sender_->SetTargetSendBitrate(300000);
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
                                  &mock_paced_sender_));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  // Make all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              SendPacket(PacedSender::kNormalPriority, _, _, _, _, _)).
                  WillRepeatedly(testing::Return(false));

  uint16_t seq_num = kSeqNum;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int rtp_header_len = 12;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  rtp_sender_->SetRTXStatus(kRtxRetransmitted | kRtxRedundantPayloads, true,
                            1234);

  // Create and set up parser.
  scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != NULL);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  rtp_sender_->SetTargetSendBitrate(300000);
  const size_t kNumPayloadSizes = 10;
  const int kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700, 750,
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
  const int kPaddingPayloadSize = 224;
  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport, SendPacket(_, _, kPaddingPayloadSize + rtp_header_len))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kPaddingPayloadSize, rtp_sender_->TimeToSendPadding(49));

  const int kRtxHeaderSize = 2;
  EXPECT_CALL(transport, SendPacket(_, _, kPayloadSizes[0] +
                                    rtp_header_len + kRtxHeaderSize))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kPayloadSizes[0], rtp_sender_->TimeToSendPadding(500));

  EXPECT_CALL(transport, SendPacket(_, _, kPayloadSizes[kNumPayloadSizes - 1] +
                                    rtp_header_len + kRtxHeaderSize))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_CALL(transport, SendPacket(_, _, kPaddingPayloadSize + rtp_header_len))
      .WillOnce(testing::ReturnArg<2>());
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kPaddingPayloadSize,
            rtp_sender_->TimeToSendPadding(999));
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

  ModuleRTPUtility::RTPHeaderParser rtp_parser2(transport_.last_sent_packet_,
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
                                   const unsigned int ssrc) {
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

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  rtp_sender_->RegisterFrameCountObserver(&callback);

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

  rtp_sender_->RegisterFrameCountObserver(NULL);
}

TEST_F(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback()
        : BitrateStatisticsObserver(), num_calls_(0), ssrc_(0), bitrate_() {}
    virtual ~TestCallback() {}

    virtual void Notify(const BitrateStatistics& stats, uint32_t ssrc) {
      ++num_calls_;
      ssrc_ = ssrc;
      bitrate_ = stats;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    BitrateStatistics bitrate_;
  } callback;

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

  rtp_sender_->RegisterBitrateObserver(&callback);

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

  EXPECT_EQ(1U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(start_time + (kNumPackets * kPacketInterval),
            callback.bitrate_.timestamp_ms);
  EXPECT_EQ(expected_packet_rate, callback.bitrate_.packet_rate);
  EXPECT_EQ((kPacketOverhead + sizeof(payload)) * 8 * expected_packet_rate,
            callback.bitrate_.bitrate_bps);

  rtp_sender_->RegisterBitrateObserver(NULL);
}

class RtpSenderAudioTest : public RtpSenderTest {
 protected:
  RtpSenderAudioTest() {}

  virtual void SetUp() {
    payload_ = kAudioPayload;
    rtp_sender_.reset(new RTPSender(0, true, &fake_clock_, &transport_, NULL,
                                    &mock_paced_sender_));
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
                                     uint32_t ssrc) {
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
  const uint32_t kMaxPaddingSize = 224;
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
  EXPECT_TRUE(callback.Matches(ssrc, 6, 24, 224, 3, 1, 0));

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
  EXPECT_TRUE(callback.Matches(ssrc, 34, 48, 224, 5, 1, 1));

  rtp_sender_->RegisterRtpStatisticsCallback(NULL);
}

TEST_F(RtpSenderAudioTest, BuildRTPPacketWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAudioLevelIndicationStatus(true,
      kAudioLevelExtensionId));
  EXPECT_EQ(0, rtp_sender_->SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
      kRtpExtensionAudioLevel, kAudioLevelExtensionId));

  int32_t length = rtp_sender_->BuildRTPheader(packet_,
                                               kAudioPayload,
                                               kMarkerBit,
                                               kTimestamp,
                                               0);
  EXPECT_EQ(12 + rtp_sender_->RtpHeaderExtensionTotalLength(), length);

  // Currently, no space is added by for header extension by BuildRTPHeader().
  EXPECT_EQ(0, rtp_sender_->RtpHeaderExtensionTotalLength());

  // Verify
  webrtc::ModuleRTPUtility::RTPHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  // TODO(solenberg): Should verify that we got audio level in header extension.

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(rtp_header2, NULL);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  // TODO(solenberg): Should verify that we didn't get audio level.
  EXPECT_EQ(0, rtp_sender_->SetAudioLevelIndicationStatus(false, 0));
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

  ModuleRTPUtility::RTPHeaderParser rtp_parser(transport_.last_sent_packet_,
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
  EXPECT_EQ(0, rtp_sender_->SetAudioLevelIndicationStatus(true,
      kAudioLevelExtensionId));
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

  ModuleRTPUtility::RTPHeaderParser rtp_parser(transport_.last_sent_packet_,
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
  EXPECT_EQ(0, rtp_sender_->SetAudioLevelIndicationStatus(false, 0));
}

}  // namespace webrtc
