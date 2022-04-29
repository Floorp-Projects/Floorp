/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/bitrate_constraint.h"

#include <limits>
#include <utility>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "test/gtest.h"

namespace webrtc {

using ResolutionBitrateLimits = VideoEncoder::ResolutionBitrateLimits;

namespace {

void FillCodecConfig(VideoCodec* video_codec,
                     VideoEncoderConfig* encoder_config,
                     int width_px,
                     int height_px,
                     std::vector<bool> active_flags) {
  size_t num_layers = active_flags.size();
  video_codec->codecType = kVideoCodecVP8;
  video_codec->numberOfSimulcastStreams = num_layers;

  encoder_config->number_of_streams = num_layers;
  encoder_config->simulcast_layers.resize(num_layers);

  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    int layer_width_px = width_px >> (num_layers - 1 - layer_idx);
    int layer_height_px = height_px >> (num_layers - 1 - layer_idx);

    video_codec->simulcastStream[layer_idx].active = active_flags[layer_idx];
    video_codec->simulcastStream[layer_idx].width = layer_width_px;
    video_codec->simulcastStream[layer_idx].height = layer_height_px;

    encoder_config->simulcast_layers[layer_idx].active =
        active_flags[layer_idx];
    encoder_config->simulcast_layers[layer_idx].width = layer_width_px;
    encoder_config->simulcast_layers[layer_idx].height = layer_height_px;
  }
}

VideoEncoder::EncoderInfo MakeEncoderInfo() {
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.resolution_bitrate_limits = std::vector<ResolutionBitrateLimits>(
      {ResolutionBitrateLimits(640 * 360, 500000, 0, 5000000),
       ResolutionBitrateLimits(1280 * 720, 1000000, 0, 5000000),
       ResolutionBitrateLimits(1920 * 1080, 2000000, 0, 5000000)});
  return encoder_info;
}
}  // namespace

TEST(BitrateConstraintTest, AdaptUpAllowedAtSinglecastIfBitrateIsEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{true});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest, AdaptUpDisallowedAtSinglecastIfBitrateIsNotEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{true});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // 1 bps less than needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_FALSE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     AdaptUpAllowedAtSinglecastUpperLayerActiveIfBitrateIsEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{false, true});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     AdaptUpDisallowedAtSinglecastUpperLayerActiveIfBitrateIsNotEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{false, true});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // 1 bps less than needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_FALSE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest,
     AdaptUpAllowedAtSinglecastLowestLayerActiveIfBitrateIsNotEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{true, false});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // 1 bps less than needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

TEST(BitrateConstraintTest, AdaptUpAllowedAtSimulcastIfBitrateIsNotEnough) {
  VideoCodec video_codec;
  VideoEncoderConfig encoder_config;
  FillCodecConfig(&video_codec, &encoder_config,
                  /*width_px=*/640, /*height_px=*/360,
                  /*active_flags=*/{true, true});

  EncoderSettings encoder_settings(MakeEncoderInfo(), std::move(encoder_config),
                                   video_codec);

  BitrateConstraint bitrate_constraint;
  bitrate_constraint.OnEncoderSettingsUpdated(encoder_settings);
  // 1 bps less than needed for 720p.
  bitrate_constraint.OnEncoderTargetBitrateUpdated(1000 * 1000 - 1);

  VideoSourceRestrictions restrictions_before(
      /*max_pixels_per_frame=*/640 * 360, /*target_pixels_per_frame=*/640 * 360,
      /*max_frame_rate=*/30);
  VideoSourceRestrictions restrictions_after(
      /*max_pixels_per_frame=*/1280 * 720,
      /*target_pixels_per_frame=*/1280 * 720, /*max_frame_rate=*/30);

  EXPECT_TRUE(bitrate_constraint.IsAdaptationUpAllowed(
      VideoStreamInputState(), restrictions_before, restrictions_after));
}

}  // namespace webrtc
