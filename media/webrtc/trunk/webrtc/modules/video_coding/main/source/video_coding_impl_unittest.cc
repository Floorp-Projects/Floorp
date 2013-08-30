/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/codecs/interface/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/mock/mock_vcm_callbacks.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;

namespace webrtc {

class TestVideoCodingModule : public ::testing::Test {
 protected:
  static const int kDefaultWidth = 1280;
  static const int kDefaultHeight = 720;
  static const int kNumberOfStreams = 3;
  static const int kNumberOfLayers = 3;
  static const int kUnusedPayloadType = 10;

  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    vcm_ = VideoCodingModule::Create(0, clock_.get(), &event_factory_);
    EXPECT_EQ(0, vcm_->InitializeReceiver());
    EXPECT_EQ(0, vcm_->InitializeSender());
    EXPECT_EQ(0, vcm_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType,
                                               false));
    EXPECT_EQ(0, vcm_->RegisterExternalDecoder(&decoder_, kUnusedPayloadType,
                                               true));
    const size_t kMaxNackListSize = 250;
    const int kMaxPacketAgeToNack = 450;
    vcm_->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);
    memset(&settings_, 0, sizeof(settings_));
    EXPECT_EQ(0, vcm_->Codec(kVideoCodecVP8, &settings_));
    settings_.numberOfSimulcastStreams = kNumberOfStreams;
    ConfigureStream(kDefaultWidth / 4, kDefaultHeight / 4, 100,
                    &settings_.simulcastStream[0]);
    ConfigureStream(kDefaultWidth / 2, kDefaultHeight / 2, 500,
                    &settings_.simulcastStream[1]);
    ConfigureStream(kDefaultWidth, kDefaultHeight, 1200,
                    &settings_.simulcastStream[2]);
    settings_.plType = kUnusedPayloadType;  // Use the mocked encoder.
    EXPECT_EQ(0, vcm_->RegisterSendCodec(&settings_, 1, 1200));
    EXPECT_EQ(0, vcm_->RegisterReceiveCodec(&settings_, 1, true));
  }

  virtual void TearDown() {
    VideoCodingModule::Destroy(vcm_);
  }

  void ExpectIntraRequest(int stream) {
    if (stream == -1) {
      // No intra request expected.
      EXPECT_CALL(encoder_, Encode(
          _, _, Pointee(ElementsAre(kDeltaFrame, kDeltaFrame, kDeltaFrame))))
          .Times(1)
          .WillRepeatedly(Return(0));
      return;
    }
    assert(stream >= 0);
    assert(stream < kNumberOfStreams);
    std::vector<VideoFrameType> frame_types(kNumberOfStreams, kDeltaFrame);
    frame_types[stream] = kKeyFrame;
    EXPECT_CALL(encoder_, Encode(
        _, _, Pointee(ElementsAreArray(&frame_types[0], frame_types.size()))))
        .Times(1)
        .WillRepeatedly(Return(0));
  }

  static void ConfigureStream(int width, int height, int max_bitrate,
                              SimulcastStream* stream) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->numberOfTemporalLayers = kNumberOfLayers;
    stream->qpMax = 45;
  }

  void InsertAndVerifyPaddingFrame(const uint8_t* payload, int length,
                                   WebRtcRTPHeader* header) {
    ASSERT_TRUE(header != NULL);
    for (int j = 0; j < 5; ++j) {
      // Padding only packets are passed to the VCM with payload size 0.
      EXPECT_EQ(0, vcm_->IncomingPacket(payload, 0, *header));
      ++header->header.sequenceNumber;
    }
    EXPECT_EQ(0, vcm_->Process());
    EXPECT_CALL(decoder_, Decode(_, _, _, _, _))
        .Times(0);
    EXPECT_EQ(VCM_FRAME_NOT_READY, vcm_->Decode(0));
  }

  void InsertAndVerifyDecodableFrame(const uint8_t* payload, int length,
                                     WebRtcRTPHeader* header) {
    ASSERT_TRUE(header != NULL);
    EXPECT_EQ(0, vcm_->IncomingPacket(payload, length, *header));
    ++header->header.sequenceNumber;
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _))
        .Times(0);
    EXPECT_EQ(0, vcm_->Process());
    EXPECT_CALL(decoder_, Decode(_, _, _, _, _))
        .Times(1);
    EXPECT_EQ(0, vcm_->Decode(0));
  }

  VideoCodingModule* vcm_;
  scoped_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
  NiceMock<MockVideoDecoder> decoder_;
  NiceMock<MockVideoEncoder> encoder_;
  I420VideoFrame input_frame_;
  VideoCodec settings_;
  NiceMock<MockPacketRequestCallback> packet_request_callback_;
};

TEST_F(TestVideoCodingModule, TestIntraRequests) {
  EXPECT_EQ(0, vcm_->IntraFrameRequest(0));
  ExpectIntraRequest(0);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(0, vcm_->IntraFrameRequest(1));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(0, vcm_->IntraFrameRequest(2));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(-1, vcm_->IntraFrameRequest(3));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));

  EXPECT_EQ(-1, vcm_->IntraFrameRequest(-1));
  ExpectIntraRequest(-1);
  EXPECT_EQ(0, vcm_->AddVideoFrame(input_frame_, NULL, NULL));
}

TEST_F(TestVideoCodingModule, TestIntraRequestsInternalCapture) {
  // De-register current external encoder.
  EXPECT_EQ(0, vcm_->RegisterExternalEncoder(NULL, kUnusedPayloadType, false));
  // Register encoder with internal capture.
  EXPECT_EQ(0, vcm_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType,
                                             true));
  EXPECT_EQ(0, vcm_->RegisterSendCodec(&settings_, 1, 1200));
  ExpectIntraRequest(0);
  EXPECT_EQ(0, vcm_->IntraFrameRequest(0));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, vcm_->IntraFrameRequest(1));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, vcm_->IntraFrameRequest(2));
  // No requests expected since these indices are out of bounds.
  EXPECT_EQ(-1, vcm_->IntraFrameRequest(3));
  EXPECT_EQ(-1, vcm_->IntraFrameRequest(-1));
}

TEST_F(TestVideoCodingModule, PaddingOnlyFrames) {
  EXPECT_EQ(0, vcm_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(0, vcm_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kPaddingSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRTPVideoVP8;
  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _))
        .Times(0);
    InsertAndVerifyPaddingFrame(payload, 0, &header);
    clock_->AdvanceTimeMilliseconds(33);
    header.header.timestamp += 3000;
  }
}

TEST_F(TestVideoCodingModule, PaddingOnlyFramesWithLosses) {
  EXPECT_EQ(0, vcm_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(0, vcm_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kFrameSize = 1200;
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kFrameSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRTPVideoVP8;
  // Insert one video frame to get one frame decoded.
  header.frameType = kVideoFrameKey;
  header.type.Video.isFirstPacket = true;
  header.header.markerBit = true;
  InsertAndVerifyDecodableFrame(payload, kFrameSize, &header);
  clock_->AdvanceTimeMilliseconds(33);
  header.header.timestamp += 3000;

  header.frameType = kFrameEmpty;
  header.type.Video.isFirstPacket = false;
  header.header.markerBit = false;
  // Insert padding frames.
  for (int i = 0; i < 10; ++i) {
    // Lose one packet from the 6th frame.
    if (i == 5) {
      ++header.header.sequenceNumber;
    }
    // Lose the 4th frame.
    if (i == 3) {
      header.header.sequenceNumber += 5;
    } else {
      if (i > 3 && i < 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 5))
            .Times(1);
      } else if (i >= 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 6))
            .Times(1);
      } else {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, _))
            .Times(0);
      }
      InsertAndVerifyPaddingFrame(payload, 0, &header);
    }
    clock_->AdvanceTimeMilliseconds(33);
    header.header.timestamp += 3000;
  }
}

TEST_F(TestVideoCodingModule, PaddingOnlyAndVideo) {
  EXPECT_EQ(0, vcm_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(0, vcm_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kFrameSize = 1200;
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kFrameSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.type.Video.isFirstPacket = false;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRTPVideoVP8;
  header.type.Video.codecHeader.VP8.pictureId = -1;
  header.type.Video.codecHeader.VP8.tl0PicIdx = -1;
  for (int i = 0; i < 3; ++i) {
    // Insert 2 video frames.
    for (int j = 0; j < 2; ++j) {
      if (i == 0 && j == 0)  // First frame should be a key frame.
        header.frameType = kVideoFrameKey;
      else
        header.frameType = kVideoFrameDelta;
      header.type.Video.isFirstPacket = true;
      header.header.markerBit = true;
      InsertAndVerifyDecodableFrame(payload, kFrameSize, &header);
      clock_->AdvanceTimeMilliseconds(33);
      header.header.timestamp += 3000;
    }

    // Insert 2 padding only frames.
    header.frameType = kFrameEmpty;
    header.type.Video.isFirstPacket = false;
    header.header.markerBit = false;
    for (int j = 0; j < 2; ++j) {
      InsertAndVerifyPaddingFrame(payload, 0, &header);
      clock_->AdvanceTimeMilliseconds(33);
      header.header.timestamp += 3000;
    }
  }
}

TEST_F(TestVideoCodingModule, ReceiverDelay) {
  EXPECT_EQ(0, vcm_->SetMinReceiverDelay(0));
  EXPECT_EQ(0, vcm_->SetMinReceiverDelay(5000));
  EXPECT_EQ(-1, vcm_->SetMinReceiverDelay(-100));
  EXPECT_EQ(-1, vcm_->SetMinReceiverDelay(10010));
}

}  // namespace webrtc
