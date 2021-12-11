/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "media/engine/encoder_simulcast_proxy.h"
#include <string>

#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockEncoder : public VideoEncoder {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  MockEncoder() {}
  virtual ~MockEncoder() {}

  MOCK_METHOD3(InitEncode,
               int32_t(const VideoCodec* codec_settings,
                       int32_t number_of_cores,
                       size_t max_payload_size));

  MOCK_METHOD1(RegisterEncodeCompleteCallback, int32_t(EncodedImageCallback*));

  MOCK_METHOD0(Release, int32_t());

  MOCK_METHOD3(
      Encode,
      int32_t(const VideoFrame& inputImage,
              const CodecSpecificInfo* codecSpecificInfo,
              const std::vector<FrameType>* frame_types) /* override */);

  MOCK_CONST_METHOD0(GetEncoderInfo, VideoEncoder::EncoderInfo(void));
};

TEST(EncoderSimulcastProxy, ChoosesCorrectImplementation) {
  const std::string kImplementationName = "Fake";
  const std::string kSimulcastAdaptedImplementationName =
      "SimulcastEncoderAdapter (Fake, Fake, Fake)";
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  codec_settings.simulcastStream[0] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       2000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.simulcastStream[1] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       3000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.simulcastStream[2] = {test::kTestWidth,
                                       test::kTestHeight,
                                       test::kTestFrameRate,
                                       2,
                                       5000,
                                       1000,
                                       1000,
                                       56};
  codec_settings.numberOfSimulcastStreams = 3;

  NiceMock<MockEncoder>* mock_encoder = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.implementation_name = kImplementationName;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoderProxy(_))
      .Times(1)
      .WillOnce(Return(mock_encoder));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kImplementationName,
            simulcast_enabled_proxy.GetEncoderInfo().implementation_name);

  NiceMock<MockEncoder>* mock_encoder1 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder2 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder3 = new NiceMock<MockEncoder>();
  NiceMock<MockEncoder>* mock_encoder4 = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> nonsimulcast_factory;

  EXPECT_CALL(*mock_encoder1, InitEncode(_, _, _))
      .WillOnce(
          Return(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED));
  EXPECT_CALL(*mock_encoder1, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(*mock_encoder2, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder2, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(*mock_encoder3, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder3, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(*mock_encoder4, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  EXPECT_CALL(*mock_encoder4, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  EXPECT_CALL(nonsimulcast_factory, CreateVideoEncoderProxy(_))
      .Times(4)
      .WillOnce(Return(mock_encoder1))
      .WillOnce(Return(mock_encoder2))
      .WillOnce(Return(mock_encoder3))
      .WillOnce(Return(mock_encoder4));

  EncoderSimulcastProxy simulcast_disabled_proxy(&nonsimulcast_factory,
                                                 SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_disabled_proxy.InitEncode(&codec_settings, 4, 1200));
  EXPECT_EQ(kSimulcastAdaptedImplementationName,
            simulcast_disabled_proxy.GetEncoderInfo().implementation_name);

  // Cleanup.
  simulcast_enabled_proxy.Release();
  simulcast_disabled_proxy.Release();
}

TEST(EncoderSimulcastProxy, ForwardsTrustedSetting) {
  NiceMock<MockEncoder>* mock_encoder = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoderProxy(_))
      .Times(1)
      .WillOnce(Return(mock_encoder));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));

  VideoEncoder::EncoderInfo info;
  info.has_trusted_rate_controller = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillRepeatedly(Return(info));

  EXPECT_TRUE(
      simulcast_enabled_proxy.GetEncoderInfo().has_trusted_rate_controller);
}

TEST(EncoderSimulcastProxy, ForwardsHardwareAccelerated) {
  NiceMock<MockEncoder>* mock_encoder = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoderProxy(_))
      .Times(1)
      .WillOnce(Return(mock_encoder));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));

  VideoEncoder::EncoderInfo info;

  info.is_hardware_accelerated = false;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_FALSE(
      simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);

  info.is_hardware_accelerated = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_TRUE(simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);
}

TEST(EncoderSimulcastProxy, ForwardsInternalSource) {
  NiceMock<MockEncoder>* mock_encoder = new NiceMock<MockEncoder>();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoderProxy(_))
      .Times(1)
      .WillOnce(Return(mock_encoder));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, 4, 1200));

  VideoEncoder::EncoderInfo info;

  info.has_internal_source = false;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_FALSE(simulcast_enabled_proxy.GetEncoderInfo().has_internal_source);

  info.has_internal_source = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_TRUE(simulcast_enabled_proxy.GetEncoderInfo().has_internal_source);
}

}  // namespace testing
}  // namespace webrtc
