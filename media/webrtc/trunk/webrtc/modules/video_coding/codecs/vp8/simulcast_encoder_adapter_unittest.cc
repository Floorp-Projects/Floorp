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
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
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
      : TestVp8Simulcast(CreateTestEncoderAdapter(), VP8Decoder::Create()) {}

 protected:
  virtual void SetUp() { TestVp8Simulcast::SetUp(); }
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
TEST_F(TestSimulcastEncoderAdapter, DISABLED_TestSkipEncodingUnusedStreams) {
  TestVp8Simulcast::TestSkipEncodingUnusedStreams();
}

TEST_F(TestSimulcastEncoderAdapter, DISABLED_TestRPSIEncoder) {
  TestVp8Simulcast::TestRPSIEncoder();
}

class MockVideoEncoder : public VideoEncoder {
 public:
  int32_t InitEncode(const VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) override {
    codec_ = *codecSettings;
    return 0;
  }

  int32_t Encode(const VideoFrame& inputImage,
                 const CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<FrameType>* frame_types) override {
    return 0;
  }

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    callback_ = callback;
    return 0;
  }

  int32_t Release() override { return 0; }

  int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) override {
    return 0;
  }

  MOCK_METHOD2(SetChannelParameters, int32_t(uint32_t packetLoss, int64_t rtt));

  bool SupportsNativeHandle() const override { return supports_native_handle_; }

  virtual ~MockVideoEncoder() {}

  const VideoCodec& codec() const { return codec_; }

  void SendEncodedImage(int width, int height) {
    // Sends a fake image of the given width/height.
    EncodedImage image;
    image._encodedWidth = width;
    image._encodedHeight = height;
    CodecSpecificInfo codecSpecificInfo;
    memset(&codecSpecificInfo, 0, sizeof(codecSpecificInfo));
    callback_->Encoded(image, &codecSpecificInfo, NULL);
  }

  void set_supports_native_handle(bool enabled) {
    supports_native_handle_ = enabled;
  }

  MOCK_CONST_METHOD0(ImplementationName, const char*());

 private:
  bool supports_native_handle_ = false;
  VideoCodec codec_;
  EncodedImageCallback* callback_;
};

class MockVideoEncoderFactory : public VideoEncoderFactory {
 public:
  VideoEncoder* Create() override {
    MockVideoEncoder* encoder = new MockVideoEncoder();
    const char* encoder_name = encoder_names_.empty()
                                   ? "codec_implementation_name"
                                   : encoder_names_[encoders_.size()];
    ON_CALL(*encoder, ImplementationName()).WillByDefault(Return(encoder_name));
    encoders_.push_back(encoder);
    return encoder;
  }

  void Destroy(VideoEncoder* encoder) override { delete encoder; }

  virtual ~MockVideoEncoderFactory() {}

  const std::vector<MockVideoEncoder*>& encoders() const { return encoders_; }
  void SetEncoderNames(const std::vector<const char*>& encoder_names) {
    encoder_names_ = encoder_names;
  }

 private:
  std::vector<MockVideoEncoder*> encoders_;
  std::vector<const char*> encoder_names_;
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
                  SetChannelParameters(packetLoss, rtt))
          .Times(1);
    }
  }

  MockVideoEncoderFactory* factory() { return factory_; }

 private:
  MockVideoEncoderFactory* factory_;
};

static const int kTestTemporalLayerProfile[3] = {3, 2, 1};

class TestSimulcastEncoderAdapterFake : public ::testing::Test,
                                        public EncodedImageCallback {
 public:
  TestSimulcastEncoderAdapterFake()
      : helper_(new TestSimulcastEncoderAdapterFakeHelper()),
        adapter_(helper_->CreateMockEncoderAdapter()),
        last_encoded_image_width_(-1),
        last_encoded_image_height_(-1),
        last_encoded_image_simulcast_index_(-1) {}
  virtual ~TestSimulcastEncoderAdapterFake() {}

  int32_t Encoded(const EncodedImage& encodedImage,
                  const CodecSpecificInfo* codecSpecificInfo = NULL,
                  const RTPFragmentationHeader* fragmentation = NULL) override {
    last_encoded_image_width_ = encodedImage._encodedWidth;
    last_encoded_image_height_ = encodedImage._encodedHeight;
    if (codecSpecificInfo) {
      last_encoded_image_simulcast_index_ =
          codecSpecificInfo->codecSpecific.VP8.simulcastIdx;
    }
    return 0;
  }

  bool GetLastEncodedImageInfo(int* out_width,
                               int* out_height,
                               int* out_simulcast_index) {
    if (last_encoded_image_width_ == -1) {
      return false;
    }
    *out_width = last_encoded_image_width_;
    *out_height = last_encoded_image_height_;
    *out_simulcast_index = last_encoded_image_simulcast_index_;
    return true;
  }

  void SetupCodec() {
    TestVp8Simulcast::DefaultSettings(
        &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
    EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
    adapter_->RegisterEncodeCompleteCallback(this);
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
    // The start bitrate (300kbit) minus what we have for the lower layers
    // (100kbit).
    ref_codec.startBitrate = 200;
    VerifyCodec(ref_codec, 1);

    // stream 2, the biggest resolution stream.
    InitRefCodec(2, &ref_codec);
    // We don't have enough bits to send this, so the adapter should have
    // configured it to use the min bitrate for this layer (600kbit) but turn
    // off sending.
    ref_codec.startBitrate = 600;
    VerifyCodec(ref_codec, 2);
  }

 protected:
  rtc::scoped_ptr<TestSimulcastEncoderAdapterFakeHelper> helper_;
  rtc::scoped_ptr<VP8Encoder> adapter_;
  VideoCodec codec_;
  int last_encoded_image_width_;
  int last_encoded_image_height_;
  int last_encoded_image_simulcast_index_;
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

TEST_F(TestSimulcastEncoderAdapterFake, EncodedCallbackForDifferentEncoders) {
  SetupCodec();

  // Set bitrates so that we send all layers.
  adapter_->SetRates(1200, 30);

  // At this point, the simulcast encoder adapter should have 3 streams: HD,
  // quarter HD, and quarter quarter HD. We're going to mostly ignore the exact
  // resolutions, to test that the adapter forwards on the correct resolution
  // and simulcast index values, going only off the encoder that generates the
  // image.
  EXPECT_EQ(3u, helper_->factory()->encoders().size());
  helper_->factory()->encoders()[0]->SendEncodedImage(1152, 704);
  int width;
  int height;
  int simulcast_index;
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(1152, width);
  EXPECT_EQ(704, height);
  EXPECT_EQ(0, simulcast_index);

  helper_->factory()->encoders()[1]->SendEncodedImage(300, 620);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(300, width);
  EXPECT_EQ(620, height);
  EXPECT_EQ(1, simulcast_index);

  helper_->factory()->encoders()[2]->SendEncodedImage(120, 240);
  EXPECT_TRUE(GetLastEncodedImageInfo(&width, &height, &simulcast_index));
  EXPECT_EQ(120, width);
  EXPECT_EQ(240, height);
  EXPECT_EQ(2, simulcast_index);
}

TEST_F(TestSimulcastEncoderAdapterFake, SupportsNativeHandleForSingleStreams) {
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.numberOfSimulcastStreams = 1;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(1u, helper_->factory()->encoders().size());
  helper_->factory()->encoders()[0]->set_supports_native_handle(true);
  EXPECT_TRUE(adapter_->SupportsNativeHandle());
  helper_->factory()->encoders()[0]->set_supports_native_handle(false);
  EXPECT_FALSE(adapter_->SupportsNativeHandle());
}

TEST_F(TestSimulcastEncoderAdapterFake, SupportsImplementationName) {
  EXPECT_STREQ("SimulcastEncoderAdapter", adapter_->ImplementationName());
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  std::vector<const char*> encoder_names;
  encoder_names.push_back("codec1");
  encoder_names.push_back("codec2");
  encoder_names.push_back("codec3");
  helper_->factory()->SetEncoderNames(encoder_names);
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  EXPECT_STREQ("SimulcastEncoderAdapter (codec1, codec2, codec3)",
               adapter_->ImplementationName());
}

TEST_F(TestSimulcastEncoderAdapterFake,
       SupportsNativeHandleDisabledForMultipleStreams) {
  // TODO(pbos): Implement actual test (verify that it works) when implemented
  // for multiple streams.
  TestVp8Simulcast::DefaultSettings(
      &codec_, static_cast<const int*>(kTestTemporalLayerProfile));
  codec_.numberOfSimulcastStreams = 3;
  EXPECT_EQ(0, adapter_->InitEncode(&codec_, 1, 1200));
  adapter_->RegisterEncodeCompleteCallback(this);
  ASSERT_EQ(3u, helper_->factory()->encoders().size());
  for (MockVideoEncoder* encoder : helper_->factory()->encoders())
    encoder->set_supports_native_handle(true);
  EXPECT_FALSE(adapter_->SupportsNativeHandle());
}

}  // namespace testing
}  // namespace webrtc
