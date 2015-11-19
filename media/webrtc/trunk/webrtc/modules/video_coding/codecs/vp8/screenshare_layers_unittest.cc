/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "webrtc/modules/video_coding/utility/include/mock/mock_frame_dropper.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

enum { kTimestampDelta5Fps = 90000 / 5 };  // 5 frames per second at 90 kHz.
enum { kTimestampDelta30Fps = 90000 / 30 };  // 30 frames per second at 90 kHz.
enum { kFrameSize = 2500 };

const int kFlagsTL0 = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
const int kFlagsTL1 = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
    VP8_EFLAG_NO_UPD_LAST;
const int kFlagsTL1Sync = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF |
    VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;

class ScreenshareLayersFT : public ScreenshareLayers {
 public:
  ScreenshareLayersFT(int num_temporal_layers,
                      uint8_t initial_tl0_pic_idx,
                      FrameDropper* tl0_frame_dropper,
                      FrameDropper* tl1_frame_dropper)
      : ScreenshareLayers(num_temporal_layers,
                          initial_tl0_pic_idx,
                          tl0_frame_dropper,
                          tl1_frame_dropper) {}
  virtual ~ScreenshareLayersFT() {}
};

class ScreenshareLayerTest : public ::testing::Test {
 protected:
  void SetEncodeExpectations(bool drop_tl0, bool drop_tl1, int framerate) {
    EXPECT_CALL(tl0_frame_dropper_, DropFrame())
        .Times(1)
        .WillRepeatedly(Return(drop_tl0));
    if (drop_tl0) {
      EXPECT_CALL(tl1_frame_dropper_, DropFrame())
          .Times(1)
          .WillRepeatedly(Return(drop_tl1));
    }
    EXPECT_CALL(tl0_frame_dropper_, Leak(framerate))
        .Times(1);
    EXPECT_CALL(tl1_frame_dropper_, Leak(framerate))
        .Times(1);
    if (drop_tl0) {
      EXPECT_CALL(tl0_frame_dropper_, Fill(_, _))
          .Times(0);
      if (drop_tl1) {
        EXPECT_CALL(tl1_frame_dropper_, Fill(_, _))
              .Times(0);
      } else {
        EXPECT_CALL(tl1_frame_dropper_, Fill(kFrameSize, true))
            .Times(1);
      }
    } else {
      EXPECT_CALL(tl0_frame_dropper_, Fill(kFrameSize, true))
          .Times(1);
      EXPECT_CALL(tl1_frame_dropper_, Fill(kFrameSize, true))
          .Times(1);
    }
  }

  void EncodeFrame(uint32_t timestamp,
                   bool base_sync,
                   CodecSpecificInfoVP8* vp8_info,
                   int* flags) {
    *flags = layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(base_sync, vp8_info, timestamp);
    layers_->FrameEncoded(kFrameSize, timestamp);
  }

  NiceMock<MockFrameDropper> tl0_frame_dropper_;
  NiceMock<MockFrameDropper> tl1_frame_dropper_;
  rtc::scoped_ptr<ScreenshareLayersFT> layers_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(
      new ScreenshareLayersFT(1, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, 5, NULL));
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  // One layer screenshare should not use the frame dropper as all frames will
  // belong to the base layer.
  EXPECT_CALL(tl0_frame_dropper_, DropFrame())
      .Times(0);
  EXPECT_CALL(tl1_frame_dropper_, DropFrame())
      .Times(0);
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(0, flags);
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(kFrameSize, timestamp);

  EXPECT_CALL(tl0_frame_dropper_, DropFrame())
      .Times(0);
  EXPECT_CALL(tl1_frame_dropper_, DropFrame())
      .Times(0);
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(0, flags);
  timestamp += kTimestampDelta5Fps;
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(kFrameSize, timestamp);
}

TEST_F(ScreenshareLayerTest, 2Layer) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, 5, NULL));
  int flags = 0;
  uint32_t timestamp = 0;
  uint8_t expected_tl0_idx = 0;
  CodecSpecificInfoVP8 vp8_info;
  SetEncodeExpectations(false, false, 1);
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(kFlagsTL0, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  EXPECT_CALL(tl1_frame_dropper_, SetRates(1000, 1))
      .Times(1);
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, -1, NULL));
  // Insert 5 frames at 30 fps. All should belong to TL0.
  for (int i = 0; i < 5; ++i) {
    timestamp += kTimestampDelta30Fps;
    // First iteration has a framerate based on a single frame, thus 1.
    SetEncodeExpectations(false, false, 30);
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    EXPECT_EQ(0, vp8_info.temporalIdx);
    EXPECT_FALSE(vp8_info.layerSync);
    ++expected_tl0_idx;
    EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
  }
  // Drop two frames from TL0, thus being coded in TL1.
  timestamp += kTimestampDelta30Fps;
  SetEncodeExpectations(true, false, 30);
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(kFlagsTL1Sync, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_TRUE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  timestamp += kTimestampDelta30Fps;
  SetEncodeExpectations(true, false, 30);
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(kFlagsTL1, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
}

TEST_F(ScreenshareLayerTest, 2LayersPeriodicSync) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, 5, NULL));
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  const int kNumFrames = 10;
  const bool kDrops[kNumFrames] = {false, true, true, true, true,
      true, true, true, true, true};
  const int kExpectedFramerates[kNumFrames] = {1, 5, 5, 5, 5, 5, 5, 5, 5, 5};
  const bool kExpectedSyncs[kNumFrames] = {false, true, false, false, false,
      false, false, true, false, false};
  const int kExpectedTemporalIdx[kNumFrames] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    SetEncodeExpectations(kDrops[i], false, kExpectedFramerates[i]);
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    EXPECT_EQ(kExpectedTemporalIdx[i], vp8_info.temporalIdx);
    EXPECT_EQ(kExpectedSyncs[i], vp8_info.layerSync) << "Iteration: " << i;
    EXPECT_EQ(1, vp8_info.tl0PicIdx);
  }
}

TEST_F(ScreenshareLayerTest, 2LayersToggling) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, 5, NULL));
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  const int kNumFrames = 10;
  const bool kDrops[kNumFrames] = {false, true, false, true, false,
      true, false, true, false, true};
  const int kExpectedFramerates[kNumFrames] = {1, 5, 5, 5, 5, 5, 5, 5, 5, 5};
  const bool kExpectedSyncs[kNumFrames] = {false, true, false, false, false,
      false, false, true, false, false};
  const int kExpectedTemporalIdx[kNumFrames] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
  const int kExpectedTl0Idx[kNumFrames] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    SetEncodeExpectations(kDrops[i], false, kExpectedFramerates[i]);
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    EXPECT_EQ(kExpectedTemporalIdx[i], vp8_info.temporalIdx);
    EXPECT_EQ(kExpectedSyncs[i], vp8_info.layerSync) << "Iteration: " << i;
    EXPECT_EQ(kExpectedTl0Idx[i], vp8_info.tl0PicIdx);
  }
}

TEST_F(ScreenshareLayerTest, 2LayersBothDrops) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  EXPECT_TRUE(layers_->ConfigureBitrates(100, 1000, 5, NULL));
  int flags = 0;
  uint32_t timestamp = 0;
  uint8_t expected_tl0_idx = 0;
  CodecSpecificInfoVP8 vp8_info;
  SetEncodeExpectations(false, false, 1);
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(kFlagsTL0, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  timestamp += kTimestampDelta5Fps;
  SetEncodeExpectations(true, false, 5);
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(kFlagsTL1Sync, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_TRUE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  timestamp += kTimestampDelta5Fps;
  SetEncodeExpectations(true, true, 5);
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(-1, flags);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL0) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));

  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 1000, 5, &cfg);

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * 100 + 0.5),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 450, 5, &cfg);

  EXPECT_EQ(static_cast<unsigned int>(
                450 / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  layers_.reset(
      new ScreenshareLayersFT(2, 0, &tl0_frame_dropper_, &tl1_frame_dropper_));
  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 100, 5, &cfg);

  EXPECT_EQ(100U, cfg.rc_target_bitrate);
}

}  // namespace webrtc
