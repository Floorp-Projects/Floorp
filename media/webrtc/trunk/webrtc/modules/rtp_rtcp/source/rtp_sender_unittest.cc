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

#include <list>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/stl_util.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace {
const int kTransmissionTimeOffsetExtensionId = 1;
const int kAbsoluteSendTimeExtensionId = 14;
const int kTransportSequenceNumberExtensionId = 13;
const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const int kTimeOffset = 22222;
const int kMaxPacketLength = 1500;
const uint32_t kAbsoluteSendTime = 0x00aabbcc;
const uint8_t kAudioLevel = 0x5a;
const uint16_t kTransportSequenceNumber = 0xaabbu;
const uint8_t kAudioLevelExtensionId = 9;
const int kAudioPayload = 103;
const uint64_t kStartTime = 123456789;
const size_t kMaxPaddingSize = 224u;
const int kVideoRotationExtensionId = 5;
const VideoRotation kRotation = kVideoRotation_270;

using testing::_;

const uint8_t* GetPayloadData(const RTPHeader& rtp_header,
                              const uint8_t* packet) {
  return packet + rtp_header.headerLength;
}

size_t GetPayloadDataLength(const RTPHeader& rtp_header,
                            const size_t packet_length) {
  return packet_length - rtp_header.headerLength - rtp_header.paddingLength;
}

uint64_t ConvertMsToAbsSendTime(int64_t time_ms) {
  return (((time_ms << 18) + 500) / 1000) & 0x00ffffff;
}

class LoopbackTransportTest : public webrtc::Transport {
 public:
  LoopbackTransportTest()
      : packets_sent_(0),
        last_sent_packet_len_(0),
        total_bytes_sent_(0),
        last_sent_packet_(nullptr) {}

  ~LoopbackTransportTest() {
    STLDeleteContainerPointers(sent_packets_.begin(), sent_packets_.end());
  }
  bool SendRtp(const uint8_t* data,
               size_t len,
               const PacketOptions& options) override {
    packets_sent_++;
    rtc::Buffer* buffer =
        new rtc::Buffer(reinterpret_cast<const uint8_t*>(data), len);
    last_sent_packet_ = buffer->data();
    last_sent_packet_len_ = len;
    total_bytes_sent_ += len;
    sent_packets_.push_back(buffer);
    return true;
  }
  bool SendRtcp(const uint8_t* data, size_t len) override { return false; }
  int packets_sent_;
  size_t last_sent_packet_len_;
  size_t total_bytes_sent_;
  uint8_t* last_sent_packet_;
  std::vector<rtc::Buffer*> sent_packets_;
};

}  // namespace

class MockRtpPacketSender : public RtpPacketSender {
 public:
  MockRtpPacketSender() {}
  virtual ~MockRtpPacketSender() {}

  MOCK_METHOD6(InsertPacket,
               void(Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission));
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
    EXPECT_CALL(mock_paced_sender_, InsertPacket(_, _, _, _, _, _))
        .WillRepeatedly(testing::Return());
  }

  void SetUp() override { SetUpRtpSender(true); }

  void SetUpRtpSender(bool pacer) {
    rtp_sender_.reset(new RTPSender(false, &fake_clock_, &transport_, nullptr,
                                    pacer ? &mock_paced_sender_ : nullptr,
                                    nullptr, nullptr, nullptr, nullptr,
                                    nullptr));
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }

  SimulatedClock fake_clock_;
  MockRtpPacketSender mock_paced_sender_;
  rtc::scoped_ptr<RTPSender> rtp_sender_;
  int payload_;
  LoopbackTransportTest transport_;
  const bool kMarkerBit;
  uint8_t packet_[kMaxPacketLength];

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header) {
    VerifyRTPHeaderCommon(rtp_header, kMarkerBit);
  }

  void VerifyRTPHeaderCommon(const RTPHeader& rtp_header, bool marker_bit) {
    EXPECT_EQ(marker_bit, rtp_header.markerBit);
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(kSeqNum, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(0, rtp_header.numCSRCs);
    EXPECT_EQ(0U, rtp_header.paddingLength);
  }

  void SendPacket(int64_t capture_time_ms, int payload_length) {
    uint32_t timestamp = capture_time_ms * 90;
    int32_t rtp_length = rtp_sender_->BuildRTPheader(
        packet_, kPayload, kMarkerBit, timestamp, capture_time_ms);
    ASSERT_GE(rtp_length, 0);

    // Packet should be stored in a send bucket.
    EXPECT_EQ(0, rtp_sender_->SendToNetwork(
                     packet_, payload_length, rtp_length, capture_time_ms,
                     kAllowRetransmission, RtpPacketSender::kNormalPriority));
  }
};

// TODO(pbos): Move tests over from WithoutPacer to RtpSenderTest as this is our
// default code path.
class RtpSenderTestWithoutPacer : public RtpSenderTest {
 public:
  void SetUp() override { SetUpRtpSender(false); }
};

class RtpSenderVideoTest : public RtpSenderTest {
 protected:
  void SetUp() override {
    // TODO(pbos): Set up to use pacer.
    SetUpRtpSender(false);
    rtp_sender_video_.reset(
        new RTPSenderVideo(&fake_clock_, rtp_sender_.get()));
  }
  rtc::scoped_ptr<RTPSenderVideo> rtp_sender_video_;

  void VerifyCVOPacket(uint8_t* data,
                       size_t len,
                       bool expect_cvo,
                       RtpHeaderExtensionMap* map,
                       uint16_t seq_num,
                       VideoRotation rotation) {
    webrtc::RtpUtility::RtpHeaderParser rtp_parser(data, len);

    webrtc::RTPHeader rtp_header;
    size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
        packet_, kPayload, expect_cvo /* marker_bit */, kTimestamp, 0));
    if (expect_cvo) {
      ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
                length);
    } else {
      ASSERT_EQ(kRtpHeaderSize, length);
    }
    ASSERT_TRUE(rtp_parser.Parse(&rtp_header, map));
    ASSERT_FALSE(rtp_parser.RTCP());
    EXPECT_EQ(payload_, rtp_header.payloadType);
    EXPECT_EQ(seq_num, rtp_header.sequenceNumber);
    EXPECT_EQ(kTimestamp, rtp_header.timestamp);
    EXPECT_EQ(rtp_sender_->SSRC(), rtp_header.ssrc);
    EXPECT_EQ(0, rtp_header.numCSRCs);
    EXPECT_EQ(0U, rtp_header.paddingLength);
    EXPECT_EQ(ConvertVideoRotationToCVOByte(rotation),
              rtp_header.extension.videoRotation);
  }
};

TEST_F(RtpSenderTestWithoutPacer,
       RegisterRtpTransmissionTimeOffsetHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength,
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpAbsoluteSendTimeHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAbsoluteSendTimeLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpAudioLevelHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kAudioLevelLength),
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0,
            rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpHeaderExtensions) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength +
                                    kAbsoluteSendTimeLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(RtpUtility::Word32Align(
                kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength +
                kAbsoluteSendTimeLength + kAudioLevelLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kTransmissionTimeOffsetLength +
                                    kAbsoluteSendTimeLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());

  // Deregister starts.
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAbsoluteSendTimeLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->DeregisterRtpHeaderExtension(
                   kRtpExtensionAbsoluteSendTime));
  EXPECT_EQ(RtpUtility::Word32Align(kRtpOneByteHeaderLength +
                                    kAudioLevelLength + kVideoRotationLength),
            rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0,
            rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionAudioLevel));
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(
      0, rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionVideoRotation));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTestWithoutPacer, RegisterRtpVideoRotationHeaderExtension) {
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());

  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());
  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionTotalLength());
  EXPECT_EQ(
      0, rtp_sender_->DeregisterRtpHeaderExtension(kRtpExtensionVideoRotation));
  EXPECT_EQ(0u, rtp_sender_->RtpHeaderExtensionTotalLength());
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacket) {
  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize, length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, nullptr);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_FALSE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header.extension.hasAudioLevel);
  EXPECT_EQ(0, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header.extension.absoluteSendTime);
  EXPECT_FALSE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header.extension.audioLevel);
  EXPECT_EQ(0u, rtp_header.extension.videoRotation);
}

TEST_F(RtpSenderTestWithoutPacer,
       BuildRTPPacketWithTransmissionOffsetExtension) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTestWithoutPacer,
       BuildRTPPacketWithNegativeTransmissionOffsetExtension) {
  const int kNegTimeOffset = -500;
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kNegTimeOffset));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(kNegTimeOffset, rtp_header.extension.transmissionTimeOffset);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithAbsoluteSendTimeExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
}

// Test CVO header extension is only set when marker bit is true.
TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithVideoRotation_MarkerBit) {
  rtp_sender_->SetVideoRotation(kRotation);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  size_t length = static_cast<size_t>(
      rtp_sender_->BuildRTPheader(packet_, kPayload, true, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  ASSERT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasVideoRotation);
  EXPECT_EQ(ConvertVideoRotationToCVOByte(kRotation),
            rtp_header.extension.videoRotation);
}

// Test CVO header extension is not set when marker bit is false.
TEST_F(RtpSenderTestWithoutPacer,
       DISABLED_BuildRTPPacketWithVideoRotation_NoMarkerBit) {
  rtp_sender_->SetVideoRotation(kRotation);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  size_t length = static_cast<size_t>(
      rtp_sender_->BuildRTPheader(packet_, kPayload, false, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize, length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  ASSERT_TRUE(rtp_parser.Parse(&rtp_header, &map));
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header, false);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_FALSE(rtp_header.extension.hasVideoRotation);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(&rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_TRUE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, rtp_header.extension.audioLevel);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_FALSE(rtp_header2.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
}

TEST_F(RtpSenderTestWithoutPacer, BuildRTPPacketWithHeaderExtensions) {
  EXPECT_EQ(0, rtp_sender_->SetTransmissionTimeOffset(kTimeOffset));
  EXPECT_EQ(0, rtp_sender_->SetAbsoluteSendTime(kAbsoluteSendTime));
  EXPECT_EQ(0,
            rtp_sender_->SetTransportSequenceNumber(kTransportSequenceNumber));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransportSequenceNumber,
                   kTransportSequenceNumberExtensionId));

  size_t length = static_cast<size_t>(rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, 0));
  ASSERT_EQ(kRtpHeaderSize + rtp_sender_->RtpHeaderExtensionTotalLength(),
            length);

  // Verify
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(packet_, length);
  webrtc::RTPHeader rtp_header;

  // Updating audio level is done in RTPSenderAudio, so simulate it here.
  rtp_parser.Parse(&rtp_header);
  rtp_sender_->UpdateAudioLevel(packet_, length, rtp_header, true, kAudioLevel);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionTransmissionTimeOffset,
               kTransmissionTimeOffsetExtensionId);
  map.Register(kRtpExtensionAbsoluteSendTime, kAbsoluteSendTimeExtensionId);
  map.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  map.Register(kRtpExtensionTransportSequenceNumber,
               kTransportSequenceNumberExtensionId);
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);

  ASSERT_TRUE(valid_rtp_header);
  ASSERT_FALSE(rtp_parser.RTCP());
  VerifyRTPHeaderCommon(rtp_header);
  EXPECT_EQ(length, rtp_header.headerLength);
  EXPECT_TRUE(rtp_header.extension.hasTransmissionTimeOffset);
  EXPECT_TRUE(rtp_header.extension.hasAbsoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.hasAudioLevel);
  EXPECT_TRUE(rtp_header.extension.hasTransportSequenceNumber);
  EXPECT_EQ(kTimeOffset, rtp_header.extension.transmissionTimeOffset);
  EXPECT_EQ(kAbsoluteSendTime, rtp_header.extension.absoluteSendTime);
  EXPECT_TRUE(rtp_header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, rtp_header.extension.audioLevel);
  EXPECT_EQ(kTransportSequenceNumber,
            rtp_header.extension.transportSequenceNumber);

  // Parse without map extension
  webrtc::RTPHeader rtp_header2;
  const bool valid_rtp_header2 = rtp_parser.Parse(&rtp_header2, nullptr);

  ASSERT_TRUE(valid_rtp_header2);
  VerifyRTPHeaderCommon(rtp_header2);
  EXPECT_EQ(length, rtp_header2.headerLength);
  EXPECT_FALSE(rtp_header2.extension.hasTransmissionTimeOffset);
  EXPECT_FALSE(rtp_header2.extension.hasAbsoluteSendTime);
  EXPECT_FALSE(rtp_header2.extension.hasAudioLevel);
  EXPECT_FALSE(rtp_header2.extension.hasTransportSequenceNumber);

  EXPECT_EQ(0, rtp_header2.extension.transmissionTimeOffset);
  EXPECT_EQ(0u, rtp_header2.extension.absoluteSendTime);
  EXPECT_FALSE(rtp_header2.extension.voiceActivity);
  EXPECT_EQ(0u, rtp_header2.extension.audioLevel);
  EXPECT_EQ(0u, rtp_header2.extension.transportSequenceNumber);
}

TEST_F(RtpSenderTest, TrafficSmoothingWithExtensions) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _))
      .WillRepeatedly(testing::Return());

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                          capture_time_ms, kAllowRetransmission,
                                          RtpPacketSender::kNormalPriority));

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
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);
  ASSERT_TRUE(valid_rtp_header);

  // Verify transmission time offset.
  EXPECT_EQ(kStoredTimeInMs * 90, rtp_header.extension.transmissionTimeOffset);
  uint64_t expected_send_time =
      ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
}

TEST_F(RtpSenderTest, TrafficSmoothingRetransmits) {
  EXPECT_CALL(mock_paced_sender_, InsertPacket(RtpPacketSender::kNormalPriority,
                                               _, kSeqNum, _, _, _))
      .WillRepeatedly(testing::Return());

  rtp_sender_->SetStorePacketsStatus(true, 10);
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, kTimestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                          capture_time_ms, kAllowRetransmission,
                                          RtpPacketSender::kNormalPriority));

  EXPECT_EQ(0, transport_.packets_sent_);

  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kHighPriority, _, kSeqNum, _, _, _))
      .WillRepeatedly(testing::Return());

  const int kStoredTimeInMs = 100;
  fake_clock_.AdvanceTimeMilliseconds(kStoredTimeInMs);

  EXPECT_EQ(rtp_length_int, rtp_sender_->ReSendPacket(kSeqNum));
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
  const bool valid_rtp_header = rtp_parser.Parse(&rtp_header, &map);
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
              InsertPacket(RtpPacketSender::kNormalPriority, _, _, _, _, _))
      .WillRepeatedly(testing::Return());

  uint16_t seq_num = kSeqNum;
  uint32_t timestamp = kTimestamp;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  size_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionTransmissionTimeOffset,
                   kTransmissionTimeOffsetExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  // Create and set up parser.
  rtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  webrtc::RTPHeader rtp_header;

  rtp_sender_->SetTargetBitrate(300000);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  int rtp_length_int = rtp_sender_->BuildRTPheader(
      packet_, kPayload, kMarkerBit, timestamp, capture_time_ms);
  const uint32_t media_packet_timestamp = timestamp;
  ASSERT_NE(-1, rtp_length_int);
  size_t rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                          capture_time_ms, kAllowRetransmission,
                                          RtpPacketSender::kNormalPriority));

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
    const size_t kPaddingBytes = 100;
    const size_t kMaxPaddingLength = 224;  // Value taken from rtp_sender.cc.
    // Padding will be forced to full packets.
    EXPECT_EQ(kMaxPaddingLength, rtp_sender_->TimeToSendPadding(kPaddingBytes));

    // Process send bucket. Padding should now be sent.
    EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
    EXPECT_EQ(kMaxPaddingLength + rtp_header_len,
              transport_.last_sent_packet_len_);
    // Parse sent packet.
    ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                  transport_.last_sent_packet_len_,
                                  &rtp_header));
    EXPECT_EQ(kMaxPaddingLength, rtp_header.paddingLength);

    // Verify sequence number and timestamp. The timestamp should be the same
    // as the last media packet.
    EXPECT_EQ(seq_num++, rtp_header.sequenceNumber);
    EXPECT_EQ(media_packet_timestamp, rtp_header.timestamp);
    // Verify transmission time offset.
    int offset = timestamp - media_packet_timestamp;
    EXPECT_EQ(offset, rtp_header.extension.transmissionTimeOffset);
    uint64_t expected_send_time =
        ConvertMsToAbsSendTime(fake_clock_.TimeInMilliseconds());
    EXPECT_EQ(expected_send_time, rtp_header.extension.absoluteSendTime);
    fake_clock_.AdvanceTimeMilliseconds(kPaddingPeriodMs);
    timestamp += 90 * kPaddingPeriodMs;
  }

  // Send a regular video packet again.
  capture_time_ms = fake_clock_.TimeInMilliseconds();
  rtp_length_int = rtp_sender_->BuildRTPheader(packet_, kPayload, kMarkerBit,
                                               timestamp, capture_time_ms);
  ASSERT_NE(-1, rtp_length_int);
  rtp_length = static_cast<size_t>(rtp_length_int);

  // Packet should be stored in a send bucket.
  EXPECT_EQ(0, rtp_sender_->SendToNetwork(packet_, 0, rtp_length,
                                          capture_time_ms, kAllowRetransmission,
                                          RtpPacketSender::kNormalPriority));

  rtp_sender_->TimeToSendPacket(seq_num, capture_time_ms, false);
  // Process send bucket.
  EXPECT_EQ(++total_packets_sent, transport_.packets_sent_);
  EXPECT_EQ(rtp_length, transport_.last_sent_packet_len_);
  // Parse sent packet.
  ASSERT_TRUE(
      rtp_parser->Parse(transport_.last_sent_packet_, rtp_length, &rtp_header));

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
  rtp_sender_.reset(new RTPSender(false, &fake_clock_, &transport, nullptr,
                                  &mock_paced_sender_, nullptr, nullptr,
                                  nullptr, nullptr, nullptr));
  rtp_sender_->SetSequenceNumber(kSeqNum);
  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);
  // Make all packets go through the pacer.
  EXPECT_CALL(mock_paced_sender_,
              InsertPacket(RtpPacketSender::kNormalPriority, _, _, _, _, _))
      .WillRepeatedly(testing::Return());

  uint16_t seq_num = kSeqNum;
  rtp_sender_->SetStorePacketsStatus(true, 10);
  int32_t rtp_header_len = kRtpHeaderSize;
  EXPECT_EQ(
      0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                                 kAbsoluteSendTimeExtensionId));
  rtp_header_len += 4;  // 4 bytes extension.
  rtp_header_len += 4;  // 4 extra bytes common to all extension headers.

  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  rtp_sender_->SetRtxSsrc(1234);

  // Create and set up parser.
  rtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                         kTransmissionTimeOffsetExtensionId);
  rtp_parser->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                         kAbsoluteSendTimeExtensionId);
  rtp_sender_->SetTargetBitrate(300000);
  const size_t kNumPayloadSizes = 10;
  const size_t kPayloadSizes[kNumPayloadSizes] = {500, 550, 600, 650, 700,
                                                  750, 800, 850, 900, 950};
  // Send 10 packets of increasing size.
  for (size_t i = 0; i < kNumPayloadSizes; ++i) {
    int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
    EXPECT_CALL(transport, SendRtp(_, _, _)).WillOnce(testing::Return(true));
    SendPacket(capture_time_ms, kPayloadSizes[i]);
    rtp_sender_->TimeToSendPacket(seq_num++, capture_time_ms, false);
    fake_clock_.AdvanceTimeMilliseconds(33);
  }
  // The amount of padding to send it too small to send a payload packet.
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kMaxPaddingSize, rtp_sender_->TimeToSendPadding(49));

  EXPECT_CALL(transport,
              SendRtp(_, kPayloadSizes[0] + rtp_header_len + kRtxHeaderSize, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kPayloadSizes[0], rtp_sender_->TimeToSendPadding(500));

  EXPECT_CALL(transport, SendRtp(_, kPayloadSizes[kNumPayloadSizes - 1] +
                                        rtp_header_len + kRtxHeaderSize,
                                 _))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(transport, SendRtp(_, kMaxPaddingSize + rtp_header_len, _))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(kPayloadSizes[kNumPayloadSizes - 1] + kMaxPaddingSize,
            rtp_sender_->TimeToSendPadding(999));
}

TEST_F(RtpSenderTestWithoutPacer, SendGenericVideo) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  // Send keyframe
  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234, 4321,
                                       payload, sizeof(payload), nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);
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
                                             sizeof(payload), nullptr));

  RtpUtility::RtpHeaderParser rtp_parser2(transport_.last_sent_packet_,
                                          transport_.last_sent_packet_len_);
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

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
    TestCallback() : FrameCountObserver(), num_calls_(0), ssrc_(0) {}
    virtual ~TestCallback() {}

    void FrameCountUpdated(const FrameCounts& frame_counts,
                           uint32_t ssrc) override {
      ++num_calls_;
      ssrc_ = ssrc;
      frame_counts_ = frame_counts;
    }

    uint32_t num_calls_;
    uint32_t ssrc_;
    FrameCounts frame_counts_;
  } callback;

  rtp_sender_.reset(new RTPSender(false, &fake_clock_, &transport_, nullptr,
                                  &mock_paced_sender_, nullptr, nullptr,
                                  nullptr, &callback, nullptr));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234, 4321,
                                       payload, sizeof(payload), nullptr));

  EXPECT_EQ(1U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(0, callback.frame_counts_.delta_frames);

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kVideoFrameDelta, payload_type,
                                             1234, 4321, payload,
                                             sizeof(payload), nullptr));

  EXPECT_EQ(2U, callback.num_calls_);
  EXPECT_EQ(ssrc, callback.ssrc_);
  EXPECT_EQ(1, callback.frame_counts_.key_frames);
  EXPECT_EQ(1, callback.frame_counts_.delta_frames);

  rtp_sender_.reset();
}

TEST_F(RtpSenderTest, BitrateCallbacks) {
  class TestCallback : public BitrateStatisticsObserver {
   public:
    TestCallback() : BitrateStatisticsObserver(), num_calls_(0), ssrc_(0) {}
    virtual ~TestCallback() {}

    void Notify(const BitrateStatistics& total_stats,
                const BitrateStatistics& retransmit_stats,
                uint32_t ssrc) override {
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
  rtp_sender_.reset(new RTPSender(false, &fake_clock_, &transport_, nullptr,
                                  nullptr, nullptr, nullptr, &callback, nullptr,
                                  nullptr));

  // Simulate kNumPackets sent with kPacketInterval ms intervals.
  const uint32_t kNumPackets = 15;
  const uint32_t kPacketInterval = 20;
  // Overhead = 12 bytes RTP header + 1 byte generic header.
  const uint32_t kPacketOverhead = 13;

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "GENERIC";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};
  rtp_sender_->SetStorePacketsStatus(true, 1);
  uint32_t ssrc = rtp_sender_->SSRC();

  // Initial process call so we get a new time window.
  rtp_sender_->ProcessBitrate();
  uint64_t start_time = fake_clock_.CurrentNtpInMilliseconds();

  // Send a few frames.
  for (uint32_t i = 0; i < kNumPackets; ++i) {
    ASSERT_EQ(0,
              rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234,
                                            4321, payload, sizeof(payload), 0));
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

  void SetUp() override {
    payload_ = kAudioPayload;
    rtp_sender_.reset(new RTPSender(true, &fake_clock_, &transport_, nullptr,
                                    nullptr, nullptr, nullptr, nullptr, nullptr,
                                    nullptr));
    rtp_sender_->SetSequenceNumber(kSeqNum);
  }
};

TEST_F(RtpSenderTestWithoutPacer, StreamDataCountersCallbacks) {
  class TestCallback : public StreamDataCountersCallback {
   public:
    TestCallback() : StreamDataCountersCallback(), ssrc_(0), counters_() {}
    virtual ~TestCallback() {}

    void DataCountersUpdated(const StreamDataCounters& counters,
                             uint32_t ssrc) override {
      ssrc_ = ssrc;
      counters_ = counters;
    }

    uint32_t ssrc_;
    StreamDataCounters counters_;

    void MatchPacketCounter(const RtpPacketCounter& expected,
                            const RtpPacketCounter& actual) {
      EXPECT_EQ(expected.payload_bytes, actual.payload_bytes);
      EXPECT_EQ(expected.header_bytes, actual.header_bytes);
      EXPECT_EQ(expected.padding_bytes, actual.padding_bytes);
      EXPECT_EQ(expected.packets, actual.packets);
    }

    void Matches(uint32_t ssrc, const StreamDataCounters& counters) {
      EXPECT_EQ(ssrc, ssrc_);
      MatchPacketCounter(counters.transmitted, counters_.transmitted);
      MatchPacketCounter(counters.retransmitted, counters_.retransmitted);
      EXPECT_EQ(counters.fec.packets, counters_.fec.packets);
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
  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kVideoFrameKey, payload_type, 1234, 4321,
                                       payload, sizeof(payload), nullptr));
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = 6;
  expected.transmitted.header_bytes = 12;
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.retransmitted.payload_bytes = 0;
  expected.retransmitted.header_bytes = 0;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 0;
  expected.fec.packets = 0;
  callback.Matches(ssrc, expected);

  // Retransmit a frame.
  uint16_t seqno = rtp_sender_->SequenceNumber() - 1;
  rtp_sender_->ReSendPacket(seqno, 0);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 24;
  expected.transmitted.packets = 2;
  expected.retransmitted.payload_bytes = 6;
  expected.retransmitted.header_bytes = 12;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 1;
  callback.Matches(ssrc, expected);

  // Send padding.
  rtp_sender_->TimeToSendPadding(kMaxPaddingSize);
  expected.transmitted.payload_bytes = 12;
  expected.transmitted.header_bytes = 36;
  expected.transmitted.padding_bytes = kMaxPaddingSize;
  expected.transmitted.packets = 3;
  callback.Matches(ssrc, expected);

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
                                             sizeof(payload), nullptr));
  expected.transmitted.payload_bytes = 40;
  expected.transmitted.header_bytes = 60;
  expected.transmitted.packets = 5;
  expected.fec.packets = 1;
  callback.Matches(ssrc, expected);

  rtp_sender_->RegisterRtpStatisticsCallback(nullptr);
}

TEST_F(RtpSenderAudioTest, SendAudio) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kAudioFrameCN, payload_type, 1234, 4321,
                                       payload, sizeof(payload), nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));
}

TEST_F(RtpSenderAudioTest, SendAudioWithAudioLevelExtension) {
  EXPECT_EQ(0, rtp_sender_->SetAudioLevel(kAudioLevel));
  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(kRtpExtensionAudioLevel,
                                                       kAudioLevelExtensionId));

  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "PAYLOAD_NAME";
  const uint8_t payload_type = 127;
  ASSERT_EQ(0, rtp_sender_->RegisterPayload(payload_name, payload_type, 48000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kAudioFrameCN, payload_type, 1234, 4321,
                                       payload, sizeof(payload), nullptr));

  RtpUtility::RtpHeaderParser rtp_parser(transport_.last_sent_packet_,
                                         transport_.last_sent_packet_len_);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser.Parse(&rtp_header));

  const uint8_t* payload_data =
      GetPayloadData(rtp_header, transport_.last_sent_packet_);

  ASSERT_EQ(sizeof(payload),
            GetPayloadDataLength(rtp_header, transport_.last_sent_packet_len_));

  EXPECT_EQ(0, memcmp(payload, payload_data, sizeof(payload)));

  uint8_t extension[] = {
      0xbe, 0xde, 0x00, 0x01,
      (kAudioLevelExtensionId << 4) + 0,  // ID + length.
      kAudioLevel,                        // Data.
      0x00, 0x00                          // Padding.
  };

  EXPECT_EQ(0, memcmp(extension, payload_data - sizeof(extension),
                      sizeof(extension)));
}

// As RFC4733, named telephone events are carried as part of the audio stream
// and must use the same sequence number and timestamp base as the regular
// audio channel.
// This test checks the marker bit for the first packet and the consequent
// packets of the same telephone event. Since it is specifically for DTMF
// events, ignoring audio packets and sending kEmptyFrame instead of those.
TEST_F(RtpSenderAudioTest, CheckMarkerBitForTelephoneEvents) {
  char payload_name[RTP_PAYLOAD_NAME_SIZE] = "telephone-event";
  uint8_t payload_type = 126;
  ASSERT_EQ(0,
            rtp_sender_->RegisterPayload(payload_name, payload_type, 0, 0, 0));
  // For Telephone events, payload is not added to the registered payload list,
  // it will register only the payload used for audio stream.
  // Registering the payload again for audio stream with different payload name.
  const char kPayloadName[] = "payload_name";
  ASSERT_EQ(
      0, rtp_sender_->RegisterPayload(kPayloadName, payload_type, 8000, 1, 0));
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  // DTMF event key=9, duration=500 and attenuationdB=10
  rtp_sender_->SendTelephoneEvent(9, 500, 10);
  // During start, it takes the starting timestamp as last sent timestamp.
  // The duration is calculated as the difference of current and last sent
  // timestamp. So for first call it will skip since the duration is zero.
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kEmptyFrame, payload_type,
                                             capture_time_ms, 0, nullptr, 0,
                                             nullptr));
  // DTMF Sample Length is (Frequency/1000) * Duration.
  // So in this case, it is (8000/1000) * 500 = 4000.
  // Sending it as two packets.
  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kEmptyFrame, payload_type,
                                             capture_time_ms + 2000, 0, nullptr,
                                             0, nullptr));
  rtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(rtp_parser.get() != nullptr);
  webrtc::RTPHeader rtp_header;
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_, &rtp_header));
  // Marker Bit should be set to 1 for first packet.
  EXPECT_TRUE(rtp_header.markerBit);

  ASSERT_EQ(0, rtp_sender_->SendOutgoingData(kEmptyFrame, payload_type,
                                             capture_time_ms + 4000, 0, nullptr,
                                             0, nullptr));
  ASSERT_TRUE(rtp_parser->Parse(transport_.last_sent_packet_,
                                transport_.last_sent_packet_len_, &rtp_header));
  // Marker Bit should be set to 0 for rest of the packets.
  EXPECT_FALSE(rtp_header.markerBit);
}

TEST_F(RtpSenderTestWithoutPacer, BytesReportedCorrectly) {
  const char* kPayloadName = "GENERIC";
  const uint8_t kPayloadType = 127;
  rtp_sender_->SetSSRC(1234);
  rtp_sender_->SetRtxSsrc(4321);
  rtp_sender_->SetRtxPayloadType(kPayloadType - 1, kPayloadType);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);

  ASSERT_EQ(0, rtp_sender_->RegisterPayload(kPayloadName, kPayloadType, 90000,
                                            0, 1500));
  uint8_t payload[] = {47, 11, 32, 93, 89};

  ASSERT_EQ(
      0, rtp_sender_->SendOutgoingData(kVideoFrameKey, kPayloadType, 1234, 4321,
                                       payload, sizeof(payload), 0));

  // Will send 2 full-size padding packets.
  rtp_sender_->TimeToSendPadding(1);
  rtp_sender_->TimeToSendPadding(1);

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_->GetDataCounters(&rtp_stats, &rtx_stats);

  // Payload + 1-byte generic header.
  EXPECT_GT(rtp_stats.first_packet_time_ms, -1);
  EXPECT_EQ(rtp_stats.transmitted.payload_bytes, sizeof(payload) + 1);
  EXPECT_EQ(rtp_stats.transmitted.header_bytes, 12u);
  EXPECT_EQ(rtp_stats.transmitted.padding_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.payload_bytes, 0u);
  EXPECT_EQ(rtx_stats.transmitted.header_bytes, 24u);
  EXPECT_EQ(rtx_stats.transmitted.padding_bytes, 2 * kMaxPaddingSize);

  EXPECT_EQ(rtp_stats.transmitted.TotalBytes(),
            rtp_stats.transmitted.payload_bytes +
                rtp_stats.transmitted.header_bytes +
                rtp_stats.transmitted.padding_bytes);
  EXPECT_EQ(rtx_stats.transmitted.TotalBytes(),
            rtx_stats.transmitted.payload_bytes +
                rtx_stats.transmitted.header_bytes +
                rtx_stats.transmitted.padding_bytes);

  EXPECT_EQ(
      transport_.total_bytes_sent_,
      rtp_stats.transmitted.TotalBytes() + rtx_stats.transmitted.TotalBytes());
}

TEST_F(RtpSenderTestWithoutPacer, RespectsNackBitrateLimit) {
  const int32_t kPacketSize = 1400;
  const int32_t kNumPackets = 30;

  rtp_sender_->SetStorePacketsStatus(true, kNumPackets);
  // Set bitrate (in kbps) to fit kNumPackets á kPacketSize bytes in one second.
  rtp_sender_->SetTargetBitrate(kNumPackets * kPacketSize * 8);
  const uint16_t kStartSequenceNumber = rtp_sender_->SequenceNumber();
  std::list<uint16_t> sequence_numbers;
  for (int32_t i = 0; i < kNumPackets; ++i) {
    sequence_numbers.push_back(kStartSequenceNumber + i);
    fake_clock_.AdvanceTimeMilliseconds(1);
    SendPacket(fake_clock_.TimeInMilliseconds(), kPacketSize);
  }
  EXPECT_EQ(kNumPackets, transport_.packets_sent_);

  fake_clock_.AdvanceTimeMilliseconds(1000 - kNumPackets);

  // Resending should work - brings the bandwidth up to the limit.
  // NACK bitrate is capped to the same bitrate as the encoder, since the max
  // protection overhead is 50% (see MediaOptimization::SetTargetRates).
  rtp_sender_->OnReceivedNACK(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent_);

  // Resending should not work, bandwidth exceeded.
  rtp_sender_->OnReceivedNACK(sequence_numbers, 0);
  EXPECT_EQ(kNumPackets * 2, transport_.packets_sent_);
}

// Verify that all packets of a frame have CVO byte set.
TEST_F(RtpSenderVideoTest, SendVideoWithCVO) {
  RTPVideoHeader hdr = {0};
  hdr.rotation = kVideoRotation_90;

  EXPECT_EQ(0, rtp_sender_->RegisterRtpHeaderExtension(
                   kRtpExtensionVideoRotation, kVideoRotationExtensionId));
  EXPECT_TRUE(rtp_sender_->ActivateCVORtpHeaderExtension());

  EXPECT_EQ(
      RtpUtility::Word32Align(kRtpOneByteHeaderLength + kVideoRotationLength),
      rtp_sender_->RtpHeaderExtensionTotalLength());

  rtp_sender_video_->SendVideo(kRtpVideoGeneric, kVideoFrameKey, kPayload,
                               kTimestamp, 0, packet_, sizeof(packet_), nullptr,
                               &hdr);

  RtpHeaderExtensionMap map;
  map.Register(kRtpExtensionVideoRotation, kVideoRotationExtensionId);

  // Verify that this packet does have CVO byte.
  VerifyCVOPacket(
      reinterpret_cast<uint8_t*>(transport_.sent_packets_[0]->data()),
      transport_.sent_packets_[0]->size(), true, &map, kSeqNum, hdr.rotation);

  // Verify that this packet does have CVO byte.
  VerifyCVOPacket(
      reinterpret_cast<uint8_t*>(transport_.sent_packets_[1]->data()),
      transport_.sent_packets_[1]->size(), true, &map, kSeqNum + 1,
      hdr.rotation);
}
}  // namespace webrtc
