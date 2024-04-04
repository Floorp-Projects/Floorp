/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/multiplex_codec_factory.h"

#include <memory>
#include <utility>

#include "api/environment/environment_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "test/gtest.h"

namespace webrtc {

TEST(MultiplexDecoderFactoryTest, CreateVideoDecoder) {
  std::unique_ptr<VideoDecoderFactory> internal_factory =
      std::make_unique<InternalDecoderFactory>();
  MultiplexDecoderFactory factory(std::move(internal_factory));
  std::unique_ptr<VideoDecoder> decoder = factory.Create(
      CreateEnvironment(),
      SdpVideoFormat(
          cricket::kMultiplexCodecName,
          {{cricket::kCodecParamAssociatedCodecName, cricket::kVp9CodecName}}));
  EXPECT_TRUE(decoder);
}

TEST(MultiplexEncoderFactory, CreateVideoEncoder) {
  std::unique_ptr<VideoEncoderFactory> internal_factory(
      new InternalEncoderFactory());
  MultiplexEncoderFactory factory(std::move(internal_factory));
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(
          cricket::kMultiplexCodecName,
          {{cricket::kCodecParamAssociatedCodecName, cricket::kVp9CodecName}}));
  EXPECT_TRUE(encoder);
}

}  // namespace webrtc
