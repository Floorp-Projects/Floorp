/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_unittest.h"
#include "webrtc/modules/video_coding/codecs/vp8/vp8_factory.h"

namespace webrtc {
namespace testing {

static VP8Encoder* CreateTestEncoderAdapter() {
  VP8EncoderFactoryConfig::set_use_simulcast_adapter(true);
  return VP8Encoder::Create();
}

class TestSimulcastEncoderAdapter : public TestVp8Simulcast {
 public:
  TestSimulcastEncoderAdapter()
     : TestVp8Simulcast(CreateTestEncoderAdapter(),
                        VP8Decoder::Create()) {}
 protected:
  virtual void SetUp() {
    TestVp8Simulcast::SetUp();
  }
  virtual void TearDown() {
    TestVp8Simulcast::TearDown();
    VP8EncoderFactoryConfig::set_use_simulcast_adapter(false);
  }
};

TEST_F(TestSimulcastEncoderAdapter, TestKeyFrameRequestsOnAllStreams) {
  TestVp8Simulcast::TestKeyFrameRequestsOnAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingAllStreams) {
  TestVp8Simulcast::TestPaddingAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingTwoStreams) {
  TestVp8Simulcast::TestPaddingTwoStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingTwoStreamsOneMaxedOut) {
  TestVp8Simulcast::TestPaddingTwoStreamsOneMaxedOut();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingOneStream) {
  TestVp8Simulcast::TestPaddingOneStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestPaddingOneStreamTwoMaxedOut) {
  TestVp8Simulcast::TestPaddingOneStreamTwoMaxedOut();
}

TEST_F(TestSimulcastEncoderAdapter, TestSendAllStreams) {
  TestVp8Simulcast::TestSendAllStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestDisablingStreams) {
  TestVp8Simulcast::TestDisablingStreams();
}

TEST_F(TestSimulcastEncoderAdapter, TestSwitchingToOneStream) {
  TestVp8Simulcast::TestSwitchingToOneStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestSwitchingToOneOddStream) {
  TestVp8Simulcast::TestSwitchingToOneOddStream();
}

TEST_F(TestSimulcastEncoderAdapter, TestRPSIEncodeDecode) {
  TestVp8Simulcast::TestRPSIEncodeDecode();
}

TEST_F(TestSimulcastEncoderAdapter, TestStrideEncodeDecode) {
  TestVp8Simulcast::TestStrideEncodeDecode();
}

TEST_F(TestSimulcastEncoderAdapter, TestSaptioTemporalLayers333PatternEncoder) {
  TestVp8Simulcast::TestSaptioTemporalLayers333PatternEncoder();
}

TEST_F(TestSimulcastEncoderAdapter, TestSpatioTemporalLayers321PatternEncoder) {
  TestVp8Simulcast::TestSpatioTemporalLayers321PatternEncoder();
}

// TODO(ronghuawu): Enable this test when SkipEncodingUnusedStreams option is
// implemented for SimulcastEncoderAdapter.
TEST_F(TestSimulcastEncoderAdapter,
    DISABLED_TestSkipEncodingUnusedStreams) {
  TestVp8Simulcast::TestSkipEncodingUnusedStreams();
}

TEST_F(TestSimulcastEncoderAdapter, DISABLED_TestRPSIEncoder) {
  TestVp8Simulcast::TestRPSIEncoder();
}

class MockVideoEncoder : public VideoEncoder {
 public:
  int32_t InitEncode(const VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) {
    codec_ = *codecSettings;
    return 0;
  }

  int32_t Encode(const I420VideoFrame& inputImage,
                 const CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<VideoFrameType>* frame_types) { return 0; }

  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback* callback) {
    return 0;
  }

  int32_t Release() {
    return 0;
  }

  int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) {
    return 0;
  }

  MOCK_METHOD2(SetChannelParameters,
      int32_t(uint32_t packetLoss, int64_t rtt));

  virtual ~MockVideoEncoder() {
  }

  const VideoCodec& codec() const { return codec_; }

 private:
  VideoCodec codec_;
};

class MockVideoEncoderFactory : public VideoEncoderFactory {
 public:
  VideoEncoder* Create() override {
    MockVideoEncoder* encoder = new MockVideoEncoder();
    encoders_.push_back(encoder);
    return encoder;
  }

  void Destroy(VideoEncoder* encoder) override { delete encoder; }

  virtual ~MockVideoEncoderFactory() {}

  const std::vector<MockVideoEncoder*>& encoders() const { return encoders_; }

 private:
  std::vector<MockVideoEncoder*> encoders_;
};

class TestSimulcastEncoderAdapterFakeHelper {
 public:
  TestSimulcastEncoderAdapterFakeHelper()
      : factory_(new MockVideoEncoderFactory()) {}

  // Can only be called once as the SimulcastEncoderAdapter will take the
  // ownership of |factory_|.
  VP8Encoder* CreateMockEncoderAdapter() {
    return new SimulcastEncoderAdapter(factory_);
  }

  void ExpectCallSetChannelParameters(uint32_t packetLoss, int64_t rtt) {
    EXPECT_TRUE(!factory_->encoders().empty());
    for (size_t i = 0; i < factory_->encoders().size(); ++i) {
      EXPECT_CALL(*factory_->encoders()[i],
                  SetChannelParameters(packetLoss, rtt)).Times(1);
    }
  }

  MockVideoEncoderFactory* factory() { return factory_; }

 private:
  MockVideoEncoderFactory* factory_;
};

static const int kTestTemporalLayerProfile[3] = {3, 2, 1};

class TestSimulcastEncoderAdapterFake : public ::testing::Test {
 public:
  TestSimulcastEncoderAdapterFake()
     : helper_(new TestSimulcastEncoderAdapterFakeHelper()),
       adapter_(helper_->CreateMockEncoderAdapter()) {}
  virtual ~TestSimulcastEncoderAdapterFake() {}

  void SetupCodec() {
    TestVp8Simulcast::DefaultSettings(
      &codec_,
      static_cast<const int*>(kTestTemporalLayerProfile));
    EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  }

  void VerifyCodec(const VideoCodec& ref, int stream_index) {
    const VideoCodec& target =
        helper_->factory()->encoders()[stream_index]->codec();
    EXPECT_EQ(ref.codecType, target.codecType);
    EXPECT_EQ(0, strcmp(ref.plName, target.plName));
    EXPECT_EQ(ref.plType, target.plType);
    EXPECT_EQ(ref.width, target.width);
    EXPECT_EQ(ref.height, target.height);
    EXPECT_EQ(ref.startBitrate, target.startBitrate);
    EXPECT_EQ(ref.maxBitrate, target.maxBitrate);
    EXPECT_EQ(ref.minBitrate, target.minBitrate);
    EXPECT_EQ(ref.maxFramerate, target.maxFramerate);
    EXPECT_EQ(ref.codecSpecific.VP8.pictureLossIndicationOn,
              target.codecSpecific.VP8.pictureLossIndicationOn);
    EXPECT_EQ(ref.codecSpecific.VP8.feedbackModeOn,
              target.codecSpecific.VP8.feedbackModeOn);
    EXPECT_EQ(ref.codecSpecific.VP8.complexity,
              target.codecSpecific.VP8.complexity);
    EXPECT_EQ(ref.codecSpecific.VP8.resilience,
              target.codecSpecific.VP8.resilience);
    EXPECT_EQ(ref.codecSpecific.VP8.numberOfTemporalLayers,
              target.codecSpecific.VP8.numberOfTemporalLayers);
    EXPECT_EQ(ref.codecSpecific.VP8.denoisingOn,
              target.codecSpecific.VP8.denoisingOn);
    EXPECT_EQ(ref.codecSpecific.VP8.errorConcealmentOn,
              target.codecSpecific.VP8.errorConcealmentOn);
    EXPECT_EQ(ref.codecSpecific.VP8.automaticResizeOn,
              target.codecSpecific.VP8.automaticResizeOn);
    EXPECT_EQ(ref.codecSpecific.VP8.frameDroppingOn,
              target.codecSpecific.VP8.frameDroppingOn);
    EXPECT_EQ(ref.codecSpecific.VP8.keyFrameInterval,
              target.codecSpecific.VP8.keyFrameInterval);
    EXPECT_EQ(ref.qpMax, target.qpMax);
    EXPECT_EQ(0, target.numberOfSimulcastStreams);
    EXPECT_EQ(ref.mode, target.mode);
    EXPECT_EQ(ref.extra_options, target.extra_options);

    // No need to compare simulcastStream as numberOfSimulcastStreams should
    // always be 0.
  }

  void InitRefCodec(int stream_index, VideoCodec* ref_codec) {
    *ref_codec = codec_;
    ref_codec->codecSpecific.VP8.numberOfTemporalLayers =
        kTestTemporalLayerProfile[stream_index];
    ref_codec->width = codec_.simulcastStream[stream_index].width;
    ref_codec->height = codec_.simulcastStream[stream_index].height;
    ref_codec->maxBitrate = codec_.simulcastStream[stream_index].maxBitrate;
    ref_codec->minBitrate = codec_.simulcastStream[stream_index].minBitrate;
    ref_codec->qpMax = codec_.simulcastStream[stream_index].qpMax;
  }

  void VerifyCodecSettings() {
    EXPECT_EQ(3u, helper_->factory()->encoders().size());
    VideoCodec ref_codec;

    // stream 0, the lowest resolution stream.
    InitRefCodec(0, &ref_codec);
    ref_codec.qpMax = 45;
    ref_codec.codecSpecific.VP8.complexity = webrtc::kComplexityHigher;
    ref_codec.codecSpecific.VP8.denoisingOn = false;
    ref_codec.startBitrate = 100;  // Should equal to the target bitrate.
    VerifyCodec(ref_codec, 0);

    // stream 1
    InitRefCodec(1, &ref_codec);
    ref_codec.codecSpecific.VP8.denoisingOn = false;
    ref_codec.startBitrate = 300;
    VerifyCodec(ref_codec, 1);

    // stream 2, the biggest resolution stream.
    InitRefCodec(2, &ref_codec);
    ref_codec.startBitrate = 600;
    VerifyCodec(ref_codec, 2);
  }

 protected:
  rtc::scoped_ptr<TestSimulcastEncoderAdapterFakeHelper> helper_;
  rtc::scoped_ptr<VP8Encoder> adapter_;
  VideoCodec codec_;
};

TEST_F(TestSimulcastEncoderAdapterFake, InitEncode) {
  SetupCodec();
  VerifyCodecSettings();
}

TEST_F(TestSimulcastEncoderAdapterFake, SetChannelParameters) {
  SetupCodec();
  const uint32_t packetLoss = 5;
  const int64_t rtt = 30;
  helper_->ExpectCallSetChannelParameters(packetLoss, rtt);
  adapter_->SetChannelParameters(packetLoss, rtt);
}

}  // namespace testing
}  // namespace webrtc
