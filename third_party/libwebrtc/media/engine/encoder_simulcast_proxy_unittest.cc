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

#include <memory>
#include <string>
#include <utility>

#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace testing {
namespace {

const VideoEncoder::Capabilities kCapabilities(false);
const VideoEncoder::Settings kSettings(kCapabilities, 4, 1200);
constexpr const char* kImplementationName = "Fake";
constexpr const char* kSimulcastAdaptedImplementationName =
    "SimulcastEncoderAdapter (Fake, Fake, Fake)";

VideoCodec CreateSimulcastVideoCodec() {
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  codec_settings.simulcastStream[0] = {.width = test::kTestWidth,
                                       .height = test::kTestHeight,
                                       .maxFramerate = test::kTestFrameRate,
                                       .numberOfTemporalLayers = 2,
                                       .maxBitrate = 2000,
                                       .targetBitrate = 1000,
                                       .minBitrate = 1000,
                                       .qpMax = 56,
                                       .active = true};
  codec_settings.simulcastStream[1] = {.width = test::kTestWidth,
                                       .height = test::kTestHeight,
                                       .maxFramerate = test::kTestFrameRate,
                                       .numberOfTemporalLayers = 2,
                                       .maxBitrate = 3000,
                                       .targetBitrate = 1000,
                                       .minBitrate = 1000,
                                       .qpMax = 56,
                                       .active = true};
  codec_settings.simulcastStream[2] = {.width = test::kTestWidth,
                                       .height = test::kTestHeight,
                                       .maxFramerate = test::kTestFrameRate,
                                       .numberOfTemporalLayers = 2,
                                       .maxBitrate = 5000,
                                       .targetBitrate = 1000,
                                       .minBitrate = 1000,
                                       .qpMax = 56,
                                       .active = true};
  codec_settings.numberOfSimulcastStreams = 3;
  return codec_settings;
}

}  // namespace

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

TEST(EncoderSimulcastProxy, ImplementationSupportsSimulcast) {
  VideoCodec codec_settings = CreateSimulcastVideoCodec();

  auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.supports_simulcast = true;
  encoder_info.implementation_name = kImplementationName;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo())
      .WillRepeatedly(Return(encoder_info));

  NiceMock<MockVideoEncoderFactory> simulcast_factory;
  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder))));

  EncoderSimulcastProxy simulcast_proxy(&simulcast_factory,
                                        SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_proxy.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(kImplementationName,
            simulcast_proxy.GetEncoderInfo().implementation_name);

  // Cleanup.
  simulcast_proxy.Release();
}

TEST(EncoderSimulcastProxy, ImplementationDoesNotSupportSimulcast) {
  VideoCodec codec_settings = CreateSimulcastVideoCodec();

  NiceMock<MockVideoEncoderFactory> simulcast_factory;
  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(4)
      .WillRepeatedly([&] {
        auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
        EXPECT_CALL(*mock_encoder, InitEncode(_, _))
            .WillRepeatedly(Return(WEBRTC_VIDEO_CODEC_OK));
        VideoEncoder::EncoderInfo encoder_info;
        encoder_info.supports_simulcast = false;
        encoder_info.implementation_name = kImplementationName;
        EXPECT_CALL(*mock_encoder, GetEncoderInfo())
            .WillRepeatedly(Return(encoder_info));
        return mock_encoder;
      });

  EncoderSimulcastProxy simulcast_proxy(&simulcast_factory,
                                        SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_proxy.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(kSimulcastAdaptedImplementationName,
            simulcast_proxy.GetEncoderInfo().implementation_name);

  // Cleanup.
  simulcast_proxy.Release();
}

TEST(EncoderSimulcastProxy, ImplementationFailsToInitSimulcast) {
  VideoCodec codec_settings = CreateSimulcastVideoCodec();

  NiceMock<MockVideoEncoderFactory> encoder_factory;
  EXPECT_CALL(encoder_factory, CreateVideoEncoder).Times(4).WillRepeatedly([&] {
    // This encoder claims to have simulcast support and thus will be
    // attempted, but fails during InitEncode().
    auto mock_encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
    EXPECT_CALL(*mock_encoder, InitEncode(_, _))
        .WillRepeatedly(Invoke([](const VideoCodec* codec_settings,
                                  const VideoEncoder::Settings& settings) {
          if (codec_settings->numberOfSimulcastStreams > 1) {
            return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
          }
          return WEBRTC_VIDEO_CODEC_OK;
        }));
    VideoEncoder::EncoderInfo encoder_info;
    encoder_info.supports_simulcast = true;
    encoder_info.implementation_name = kImplementationName;
    ON_CALL(*mock_encoder, GetEncoderInfo).WillByDefault(Return(encoder_info));
    return mock_encoder;
  });

  EncoderSimulcastProxy simulcast_proxy(&encoder_factory,
                                        SdpVideoFormat("VP8"));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_proxy.InitEncode(&codec_settings, kSettings));
  EXPECT_EQ(kSimulcastAdaptedImplementationName,
            simulcast_proxy.GetEncoderInfo().implementation_name);

  // Cleanup.
  simulcast_proxy.Release();
}

TEST(EncoderSimulcastProxy, ForwardsTrustedSetting) {
  auto mock_encoder_owned = std::make_unique<NiceMock<MockVideoEncoder>>();
  auto* mock_encoder = mock_encoder_owned.get();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder_owned))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));

  VideoEncoder::EncoderInfo info;
  info.has_trusted_rate_controller = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillRepeatedly(Return(info));

  EXPECT_TRUE(
      simulcast_enabled_proxy.GetEncoderInfo().has_trusted_rate_controller);
}

TEST(EncoderSimulcastProxy, ForwardsHardwareAccelerated) {
  auto mock_encoder_owned = std::make_unique<NiceMock<MockVideoEncoder>>();
  NiceMock<MockVideoEncoder>* mock_encoder = mock_encoder_owned.get();
  NiceMock<MockVideoEncoderFactory> simulcast_factory;

  EXPECT_CALL(*mock_encoder, InitEncode(_, _))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  EXPECT_CALL(simulcast_factory, CreateVideoEncoder)
      .Times(1)
      .WillOnce(Return(ByMove(std::move(mock_encoder_owned))));

  EncoderSimulcastProxy simulcast_enabled_proxy(&simulcast_factory,
                                                SdpVideoFormat("VP8"));
  VideoCodec codec_settings;
  webrtc::test::CodecSettings(kVideoCodecVP8, &codec_settings);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            simulcast_enabled_proxy.InitEncode(&codec_settings, kSettings));

  VideoEncoder::EncoderInfo info;

  info.is_hardware_accelerated = false;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_FALSE(
      simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);

  info.is_hardware_accelerated = true;
  EXPECT_CALL(*mock_encoder, GetEncoderInfo()).WillOnce(Return(info));
  EXPECT_TRUE(simulcast_enabled_proxy.GetEncoderInfo().is_hardware_accelerated);
}

}  // namespace testing
}  // namespace webrtc
