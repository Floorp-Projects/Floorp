/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_video/test/frame_generator.h"
#include "webrtc/modules/video_coding/codecs/interface/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "webrtc/modules/video_coding/main/interface/mock/mock_vcm_callbacks.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using std::vector;
using webrtc::test::FrameGenerator;

namespace webrtc {
namespace vcm {
namespace {

class EmptyFrameGenerator : public FrameGenerator {
 public:
  virtual I420VideoFrame& NextFrame() OVERRIDE { return frame_; }

 private:
  I420VideoFrame frame_;
};

class PacketizationCallback : public VCMPacketizationCallback {
 public:
  PacketizationCallback(Clock* clock)
      : clock_(clock), start_time_ms_(clock_->TimeInMilliseconds()) {}

  virtual ~PacketizationCallback() {}

  virtual int32_t SendData(FrameType frame_type,
                           uint8_t payload_type,
                           uint32_t timestamp,
                           int64_t capture_time_ms,
                           const uint8_t* payload_data,
                           uint32_t payload_size,
                           const RTPFragmentationHeader& fragmentation_header,
                           const RTPVideoHeader* rtp_video_header) {
    assert(rtp_video_header);
    frame_data_.push_back(FrameData(payload_size, *rtp_video_header));
    return 0;
  }

  void Reset() {
    frame_data_.clear();
    start_time_ms_ = clock_->TimeInMilliseconds();
  }

  float FramerateFpsWithinTemporalLayer(int temporal_layer) {
    return CountFramesWithinTemporalLayer(temporal_layer) *
           (1000.0 / interval_ms());
  }

  float BitrateKbpsWithinTemporalLayer(int temporal_layer) {
    return SumPayloadBytesWithinTemporalLayer(temporal_layer) * 8.0 /
           interval_ms();
  }

 private:
  struct FrameData {
    FrameData() {}

    FrameData(uint32_t payload_size, const RTPVideoHeader& rtp_video_header)
        : payload_size(payload_size), rtp_video_header(rtp_video_header) {}

    uint32_t payload_size;
    RTPVideoHeader rtp_video_header;
  };

  int64_t interval_ms() {
    int64_t diff = (clock_->TimeInMilliseconds() - start_time_ms_);
    EXPECT_GT(diff, 0);
    return diff;
  }

  int CountFramesWithinTemporalLayer(int temporal_layer) {
    int frames = 0;
    for (size_t i = 0; i < frame_data_.size(); ++i) {
      EXPECT_EQ(kRtpVideoVp8, frame_data_[i].rtp_video_header.codec);
      if (frame_data_[i].rtp_video_header.codecHeader.VP8.temporalIdx <=
          temporal_layer) {
        frames++;
      }
    }
    return frames;
  }

  int SumPayloadBytesWithinTemporalLayer(int temporal_layer) {
    int payload_size = 0;
    for (size_t i = 0; i < frame_data_.size(); ++i) {
      EXPECT_EQ(kRtpVideoVp8, frame_data_[i].rtp_video_header.codec);
      if (frame_data_[i].rtp_video_header.codecHeader.VP8.temporalIdx <=
          temporal_layer) {
        payload_size += frame_data_[i].payload_size;
      }
    }
    return payload_size;
  }

  Clock* clock_;
  int64_t start_time_ms_;
  vector<FrameData> frame_data_;
};

class TestVideoSender : public ::testing::Test {
 protected:
  // Note: simulated clock starts at 1 seconds, since parts of webrtc use 0 as
  // a special case (e.g. frame rate in media optimization).
  TestVideoSender() : clock_(1000), packetization_callback_(&clock_) {}

  virtual void SetUp() {
    sender_.reset(new VideoSender(0, &clock_));
    EXPECT_EQ(0, sender_->InitializeSender());
    EXPECT_EQ(0, sender_->RegisterTransportCallback(&packetization_callback_));
  }

  void AddFrame() {
    assert(generator_.get());
    sender_->AddVideoFrame(generator_->NextFrame(), NULL, NULL);
  }

  SimulatedClock clock_;
  PacketizationCallback packetization_callback_;
  scoped_ptr<VideoSender> sender_;
  scoped_ptr<FrameGenerator> generator_;
};

class TestVideoSenderWithMockEncoder : public TestVideoSender {
 protected:
  static const int kDefaultWidth = 1280;
  static const int kDefaultHeight = 720;
  static const int kNumberOfStreams = 3;
  static const int kNumberOfLayers = 3;
  static const int kUnusedPayloadType = 10;

  virtual void SetUp() {
    TestVideoSender::SetUp();
    generator_.reset(new EmptyFrameGenerator());
    EXPECT_EQ(
        0,
        sender_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType, false));
    memset(&settings_, 0, sizeof(settings_));
    EXPECT_EQ(0, VideoCodingModule::Codec(kVideoCodecVP8, &settings_));
    settings_.numberOfSimulcastStreams = kNumberOfStreams;
    ConfigureStream(kDefaultWidth / 4,
                    kDefaultHeight / 4,
                    100,
                    &settings_.simulcastStream[0]);
    ConfigureStream(kDefaultWidth / 2,
                    kDefaultHeight / 2,
                    500,
                    &settings_.simulcastStream[1]);
    ConfigureStream(
        kDefaultWidth, kDefaultHeight, 1200, &settings_.simulcastStream[2]);
    settings_.plType = kUnusedPayloadType;  // Use the mocked encoder.
    EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  }

  virtual void TearDown() { sender_.reset(); }

  void ExpectIntraRequest(int stream) {
    if (stream == -1) {
      // No intra request expected.
      EXPECT_CALL(
          encoder_,
          Encode(_,
                 _,
                 Pointee(ElementsAre(kDeltaFrame, kDeltaFrame, kDeltaFrame))))
          .Times(1).WillRepeatedly(Return(0));
      return;
    }
    assert(stream >= 0);
    assert(stream < kNumberOfStreams);
    std::vector<VideoFrameType> frame_types(kNumberOfStreams, kDeltaFrame);
    frame_types[stream] = kKeyFrame;
    EXPECT_CALL(
        encoder_,
        Encode(_,
               _,
               Pointee(ElementsAreArray(&frame_types[0], frame_types.size()))))
        .Times(1).WillRepeatedly(Return(0));
  }

  static void ConfigureStream(int width,
                              int height,
                              int max_bitrate,
                              SimulcastStream* stream) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->numberOfTemporalLayers = kNumberOfLayers;
    stream->qpMax = 45;
  }

  VideoCodec settings_;
  NiceMock<MockVideoEncoder> encoder_;
};

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequests) {
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(0);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(1);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  ExpectIntraRequest(2);
  AddFrame();
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
  ExpectIntraRequest(-1);
  AddFrame();

  EXPECT_EQ(-1, sender_->IntraFrameRequest(-1));
  ExpectIntraRequest(-1);
  AddFrame();
}

TEST_F(TestVideoSenderWithMockEncoder, TestIntraRequestsInternalCapture) {
  // De-register current external encoder.
  EXPECT_EQ(0,
            sender_->RegisterExternalEncoder(NULL, kUnusedPayloadType, false));
  // Register encoder with internal capture.
  EXPECT_EQ(
      0, sender_->RegisterExternalEncoder(&encoder_, kUnusedPayloadType, true));
  EXPECT_EQ(0, sender_->RegisterSendCodec(&settings_, 1, 1200));
  ExpectIntraRequest(0);
  EXPECT_EQ(0, sender_->IntraFrameRequest(0));
  ExpectIntraRequest(1);
  EXPECT_EQ(0, sender_->IntraFrameRequest(1));
  ExpectIntraRequest(2);
  EXPECT_EQ(0, sender_->IntraFrameRequest(2));
  // No requests expected since these indices are out of bounds.
  EXPECT_EQ(-1, sender_->IntraFrameRequest(3));
  EXPECT_EQ(-1, sender_->IntraFrameRequest(-1));
}

class TestVideoSenderWithVp8 : public TestVideoSender {
 public:
  TestVideoSenderWithVp8()
      : codec_bitrate_kbps_(300), available_bitrate_kbps_(1000) {}

  virtual void SetUp() {
    TestVideoSender::SetUp();

    const char* input_video = "foreman_cif";
    const int width = 352;
    const int height = 288;
    generator_.reset(FrameGenerator::CreateFromYuvFile(
        test::ResourcePath(input_video, "yuv").c_str(), width, height));

    codec_ = MakeVp8VideoCodec(width, height, 3);
    codec_.minBitrate = 10;
    codec_.startBitrate = codec_bitrate_kbps_;
    codec_.maxBitrate = codec_bitrate_kbps_;
    EXPECT_EQ(0, sender_->RegisterSendCodec(&codec_, 1, 1200));
  }

  static VideoCodec MakeVp8VideoCodec(int width,
                                      int height,
                                      int temporal_layers) {
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    EXPECT_EQ(0, VideoCodingModule::Codec(kVideoCodecVP8, &codec));
    codec.width = width;
    codec.height = height;
    codec.codecSpecific.VP8.numberOfTemporalLayers = temporal_layers;
    return codec;
  }

  void InsertFrames(float framerate, float seconds) {
    for (int i = 0; i < seconds * framerate; ++i) {
      clock_.AdvanceTimeMilliseconds(1000.0f / framerate);
      AddFrame();

      // SetChannelParameters needs to be called frequently to propagate
      // framerate from the media optimization into the encoder.
      // Note: SetChannelParameters fails if less than 2 frames are in the
      // buffer since it will fail to calculate the framerate.
      if (i != 0) {
        EXPECT_EQ(VCM_OK,
                  sender_->SetChannelParameters(
                      available_bitrate_kbps_ * 1000, 0, 200));
      }
    }
  }

 protected:
  VideoCodec codec_;
  int codec_bitrate_kbps_;
  int available_bitrate_kbps_;
};

TEST_F(TestVideoSenderWithVp8,
       DISABLED_ON_ANDROID(FixedTemporalLayersStrategy)) {
  // It appears that this 5 seconds simulation are need to allow
  // bitrate and framerate to stabilize.
  // TODO(andresp): the framerate calculation should be improved.
  double framerate = 30.0;
  InsertFrames(framerate, 5.0);
  packetization_callback_.Reset();

  // Need to simulate for 10 seconds due to VP8 bitrate controller.
  InsertFrames(framerate, 10.0);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(2), 30.0, 0.5);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(1), 15.0, 0.5);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(0), 7.5, 0.5);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(2),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][2],
              10);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(1),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][1],
              10);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(0),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][0],
              10);

  framerate = 15.0;
  InsertFrames(framerate, 5.0);
  packetization_callback_.Reset();

  InsertFrames(15.0, 10.0);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(2), 15.0, 0.5);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(1), 7.5, 0.5);
  EXPECT_NEAR(
      packetization_callback_.FramerateFpsWithinTemporalLayer(0), 3.75, 0.5);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(2),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][2],
              10);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(1),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][1],
              10);
  EXPECT_NEAR(packetization_callback_.BitrateKbpsWithinTemporalLayer(0),
              codec_bitrate_kbps_ * kVp8LayerRateAlloction[2][0],
              10);
}
}  // namespace
}  // namespace vcm
}  // namespace webrtc
