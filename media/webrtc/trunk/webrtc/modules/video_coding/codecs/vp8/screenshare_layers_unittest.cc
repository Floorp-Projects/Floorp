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
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "webrtc/modules/video_coding/utility/mock/mock_frame_dropper.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

// 5 frames per second at 90 kHz.
const uint32_t kTimestampDelta5Fps = 90000 / 5;
const int kDefaultQp = 54;
const int kDefaultTl0BitrateKbps = 200;
const int kDefaultTl1BitrateKbps = 2000;
const int kFrameRate = 5;
const int kSyncPeriodSeconds = 5;
const int kMaxSyncPeriodSeconds = 10;

class ScreenshareLayerTest : public ::testing::Test {
 protected:
  ScreenshareLayerTest() : min_qp_(2), max_qp_(kDefaultQp), frame_size_(-1) {}
  virtual ~ScreenshareLayerTest() {}

  void EncodeFrame(uint32_t timestamp,
                   bool base_sync,
                   CodecSpecificInfoVP8* vp8_info,
                   int* flags) {
    *flags = layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(base_sync, vp8_info, timestamp);
    ASSERT_NE(-1, frame_size_);
    layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
  }

  void ConfigureBitrates() {
    vpx_codec_enc_cfg_t vpx_cfg;
    memset(&vpx_cfg, 0, sizeof(vpx_codec_enc_cfg_t));
    vpx_cfg.rc_min_quantizer = min_qp_;
    vpx_cfg.rc_max_quantizer = max_qp_;
    EXPECT_TRUE(layers_->ConfigureBitrates(
        kDefaultTl0BitrateKbps, kDefaultTl1BitrateKbps, kFrameRate, &vpx_cfg));
    frame_size_ = ((vpx_cfg.rc_target_bitrate * 1000) / 8) / kFrameRate;
  }

  void WithQpLimits(int min_qp, int max_qp) {
    min_qp_ = min_qp;
    max_qp_ = max_qp;
  }

  int RunGracePeriod() {
    int flags = 0;
    uint32_t timestamp = 0;
    CodecSpecificInfoVP8 vp8_info;
    bool got_tl0 = false;
    bool got_tl1 = false;
    for (int i = 0; i < 10; ++i) {
      EncodeFrame(timestamp, false, &vp8_info, &flags);
      timestamp += kTimestampDelta5Fps;
      if (vp8_info.temporalIdx == 0) {
        got_tl0 = true;
      } else {
        got_tl1 = true;
      }
      if (got_tl0 && got_tl1)
        return timestamp;
    }
    ADD_FAILURE() << "Frames from both layers not received in time.";
    return 0;
  }

  int SkipUntilTl(int layer, int timestamp) {
    CodecSpecificInfoVP8 vp8_info;
    for (int i = 0; i < 5; ++i) {
      layers_->EncodeFlags(timestamp);
      timestamp += kTimestampDelta5Fps;
      layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
      if (vp8_info.temporalIdx != layer) {
        layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
      } else {
        return timestamp;
      }
    }
    ADD_FAILURE() << "Did not get a frame of TL" << layer << " in time.";
    return 0;
  }

  int min_qp_;
  int max_qp_;
  int frame_size_;
  rtc::scoped_ptr<ScreenshareLayers> layers_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(new ScreenshareLayers(1, 0));
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  // One layer screenshare should not use the frame dropper as all frames will
  // belong to the base layer.
  const int kSingleLayerFlags = 0;
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(kSingleLayerFlags, flags);
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
  flags = layers_->EncodeFlags(timestamp);
  EXPECT_EQ(kSingleLayerFlags, flags);
  timestamp += kTimestampDelta5Fps;
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx), vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(kNoTl0PicIdx, vp8_info.tl0PicIdx);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
}

TEST_F(ScreenshareLayerTest, 2Layer) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  uint8_t expected_tl0_idx = 0;
  CodecSpecificInfoVP8 vp8_info;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  // Insert 5 frames, cover grace period. All should be in TL0.
  for (int i = 0; i < 5; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    EXPECT_EQ(0, vp8_info.temporalIdx);
    EXPECT_FALSE(vp8_info.layerSync);
    ++expected_tl0_idx;
    EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
  }

  // First frame in TL0.
  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
  EXPECT_EQ(0, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  ++expected_tl0_idx;
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  // Drop two frames from TL0, thus being coded in TL1.
  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  // First frame is sync frame.
  EXPECT_EQ(ScreenshareLayers::kTl1SyncFlags, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_TRUE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);

  timestamp += kTimestampDelta5Fps;
  EncodeFrame(timestamp, false, &vp8_info, &flags);
  EXPECT_EQ(ScreenshareLayers::kTl1Flags, flags);
  EXPECT_EQ(1, vp8_info.temporalIdx);
  EXPECT_FALSE(vp8_info.layerSync);
  EXPECT_EQ(expected_tl0_idx, vp8_info.tl0PicIdx);
}

TEST_F(ScreenshareLayerTest, 2LayersPeriodicSync) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = kSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync) {
      sync_times.push_back(timestamp);
    }
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterTimeout) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = kMaxSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    // Simulate TL1 being at least 8 qp steps better.
    if (vp8_info.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
    }

    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync)
      sync_times.push_back(timestamp);
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kMaxSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterSimilarQP) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  std::vector<int> sync_times;

  const int kNumFrames = (kSyncPeriodSeconds +
                          ((kMaxSyncPeriodSeconds - kSyncPeriodSeconds) / 2)) *
                         kFrameRate;
  for (int i = 0; i < kNumFrames; ++i) {
    timestamp += kTimestampDelta5Fps;
    layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    // Simulate TL1 being at least 8 qp steps better.
    if (vp8_info.temporalIdx == 0) {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
    }

    if (vp8_info.temporalIdx == 1 && vp8_info.layerSync)
      sync_times.push_back(timestamp);
  }

  ASSERT_EQ(1u, sync_times.size());

  bool bumped_tl0_quality = false;
  for (int i = 0; i < 3; ++i) {
    timestamp += kTimestampDelta5Fps;
    int flags = layers_->EncodeFlags(timestamp);
    layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);

    if (vp8_info.temporalIdx == 0) {
      // Bump TL0 to same quality as TL1.
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
      bumped_tl0_quality = true;
    } else {
      layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp - 8);
      if (bumped_tl0_quality) {
        EXPECT_TRUE(vp8_info.layerSync);
        EXPECT_EQ(ScreenshareLayers::kTl1SyncFlags, flags);
        return;
      }
    }
  }
  ADD_FAILURE() << "No TL1 frame arrived within time limit.";
}

TEST_F(ScreenshareLayerTest, 2LayersToggling) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  int flags = 0;
  CodecSpecificInfoVP8 vp8_info;
  uint32_t timestamp = RunGracePeriod();

  // Insert 50 frames. 2/5 should be TL0.
  int tl0_frames = 0;
  int tl1_frames = 0;
  for (int i = 0; i < 50; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    switch (vp8_info.temporalIdx) {
      case 0:
        ++tl0_frames;
        break;
      case 1:
        ++tl1_frames;
        break;
      default:
        abort();
    }
  }
  EXPECT_EQ(20, tl0_frames);
  EXPECT_EQ(30, tl1_frames);
}

TEST_F(ScreenshareLayerTest, AllFitsLayer0) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  frame_size_ = ((kDefaultTl0BitrateKbps * 1000) / 8) / kFrameRate;

  int flags = 0;
  uint32_t timestamp = 0;
  CodecSpecificInfoVP8 vp8_info;
  // Insert 50 frames, small enough that all fits in TL0.
  for (int i = 0; i < 50; ++i) {
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    timestamp += kTimestampDelta5Fps;
    EXPECT_EQ(ScreenshareLayers::kTl0Flags, flags);
    EXPECT_EQ(0, vp8_info.temporalIdx);
  }
}

TEST_F(ScreenshareLayerTest, TooHighBitrate) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  frame_size_ = 2 * ((kDefaultTl1BitrateKbps * 1000) / 8) / kFrameRate;
  int flags = 0;
  CodecSpecificInfoVP8 vp8_info;
  uint32_t timestamp = RunGracePeriod();

  // Insert 100 frames. Half should be dropped.
  int tl0_frames = 0;
  int tl1_frames = 0;
  int dropped_frames = 0;
  for (int i = 0; i < 100; ++i) {
    timestamp += kTimestampDelta5Fps;
    EncodeFrame(timestamp, false, &vp8_info, &flags);
    if (flags == -1) {
      ++dropped_frames;
    } else {
      switch (vp8_info.temporalIdx) {
        case 0:
          ++tl0_frames;
          break;
        case 1:
          ++tl1_frames;
          break;
        default:
          abort();
      }
    }
  }

  EXPECT_EQ(5, tl0_frames);
  EXPECT_EQ(45, tl1_frames);
  EXPECT_EQ(50, dropped_frames);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL0) {
  layers_.reset(new ScreenshareLayers(2, 0));

  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 1000, 5, &cfg);

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * 100 + 0.5),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  layers_.reset(new ScreenshareLayers(2, 0));
  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 450, 5, &cfg);

  EXPECT_EQ(static_cast<unsigned int>(
                450 / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  layers_.reset(new ScreenshareLayers(2, 0));
  vpx_codec_enc_cfg_t cfg;
  layers_->ConfigureBitrates(100, 100, 5, &cfg);

  EXPECT_EQ(100U, cfg.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, EncoderDrop) {
  layers_.reset(new ScreenshareLayers(2, 0));
  ConfigureBitrates();
  CodecSpecificInfoVP8 vp8_info;
  vpx_codec_enc_cfg_t cfg;
  cfg.rc_max_quantizer = kDefaultQp;

  uint32_t timestamp = RunGracePeriod();
  timestamp = SkipUntilTl(0, timestamp);

  // Size 0 indicates dropped frame.
  layers_->FrameEncoded(0, timestamp, kDefaultQp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg));
  EXPECT_EQ(ScreenshareLayers::kTl0Flags, layers_->EncodeFlags(timestamp));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  timestamp = SkipUntilTl(0, timestamp);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  EXPECT_LT(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  layers_->EncodeFlags(timestamp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  // Next drop in TL1.

  timestamp = SkipUntilTl(1, timestamp);
  layers_->FrameEncoded(0, timestamp, kDefaultQp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_FALSE(layers_->UpdateConfiguration(&cfg));
  EXPECT_EQ(ScreenshareLayers::kTl1Flags, layers_->EncodeFlags(timestamp));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  timestamp = SkipUntilTl(1, timestamp);
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  EXPECT_LT(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);

  layers_->EncodeFlags(timestamp);
  timestamp += kTimestampDelta5Fps;
  EXPECT_TRUE(layers_->UpdateConfiguration(&cfg));
  layers_->PopulateCodecSpecific(false, &vp8_info, timestamp);
  EXPECT_EQ(cfg.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->FrameEncoded(frame_size_, timestamp, kDefaultQp);
}

}  // namespace webrtc
