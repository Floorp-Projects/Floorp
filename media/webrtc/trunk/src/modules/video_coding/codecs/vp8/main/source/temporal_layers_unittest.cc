/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "gtest/gtest.h"
#include "temporal_layers.h"
#include "video_codec_interface.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

namespace webrtc {

enum {
  kTemporalUpdateLast = VP8_EFLAG_NO_UPD_GF |
                        VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_REF_GF |
                        VP8_EFLAG_NO_REF_ARF,
  kTemporalUpdateGoldenWithoutDependency = VP8_EFLAG_NO_REF_GF |
                                           VP8_EFLAG_NO_REF_ARF |
                                           VP8_EFLAG_NO_UPD_ARF |
                                           VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGolden = VP8_EFLAG_NO_REF_ARF |
                          VP8_EFLAG_NO_UPD_ARF |
                          VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltrefWithoutDependency = VP8_EFLAG_NO_REF_ARF |
                                           VP8_EFLAG_NO_REF_GF |
                                           VP8_EFLAG_NO_UPD_GF |
                                           VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltref = VP8_EFLAG_NO_UPD_GF |
                          VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateNone = VP8_EFLAG_NO_UPD_GF |
                        VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_UPD_LAST |
                        VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefAltRef = VP8_EFLAG_NO_REF_ARF |
                                   VP8_EFLAG_NO_UPD_GF |
                                   VP8_EFLAG_NO_UPD_ARF |
                                   VP8_EFLAG_NO_UPD_LAST |
                                   VP8_EFLAG_NO_UPD_ENTROPY,
};

TEST(TemporalLayersTest, 2Layers) {
  TemporalLayers tl(2);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.ConfigureBitrates(500, &cfg);

  int expected_flags[16] = { kTemporalUpdateLast,
                             kTemporalUpdateGoldenWithoutDependency,
                             kTemporalUpdateLast,
                             kTemporalUpdateGolden,
                             kTemporalUpdateLast,
                             kTemporalUpdateGolden,
                             kTemporalUpdateLast,
                             kTemporalUpdateNoneNoRefAltRef,
                             kTemporalUpdateLast,
                             kTemporalUpdateGoldenWithoutDependency,
                             kTemporalUpdateLast,
                             kTemporalUpdateGolden,
                             kTemporalUpdateLast,
                             kTemporalUpdateGolden,
                             kTemporalUpdateLast,
                             kTemporalUpdateNoneNoRefAltRef
  };
  int expected_temporal_idx[16] =
      { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };

  bool expected_layer_sync[16] =
      { false, true, false, false, false, false, false, false,
        false, true, false, false, false, false, false, false };

  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(expected_flags[i], tl.EncodeFlags());
    tl.PopulateCodecSpecific(false, &vp8_info);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    bool expected_sync = expected_layer_sync[i];
    EXPECT_EQ(expected_sync, vp8_info.layerSync);
  }
}

TEST(TemporalLayersTest, 3Layers) {
  TemporalLayers tl(3);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.ConfigureBitrates(500, &cfg);

  int expected_flags[16] = { kTemporalUpdateLast,
                             kTemporalUpdateAltrefWithoutDependency,
                             kTemporalUpdateGoldenWithoutDependency,
                             kTemporalUpdateAltref,
                             kTemporalUpdateLast,
                             kTemporalUpdateAltref,
                             kTemporalUpdateGolden,
                             kTemporalUpdateNone,
                             kTemporalUpdateLast,
                             kTemporalUpdateAltrefWithoutDependency,
                             kTemporalUpdateGoldenWithoutDependency,
                             kTemporalUpdateAltref,
                             kTemporalUpdateLast,
                             kTemporalUpdateAltref,
                             kTemporalUpdateGolden,
                             kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] =
      { 0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 2 };

  bool expected_layer_sync[16] =
      { false, true, true, false, false, false, false, false,
        false, true, true, false, false, false, false, false };

  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(expected_flags[i], tl.EncodeFlags());
    tl.PopulateCodecSpecific(false, &vp8_info);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    bool expected_sync = expected_layer_sync[i];
    EXPECT_EQ(expected_sync, vp8_info.layerSync);
  }
}

TEST(TemporalLayersTest, 4Layers) {
  TemporalLayers tl(4);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.ConfigureBitrates(500, &cfg);
  int expected_flags[16] = {
      kTemporalUpdateLast,
      kTemporalUpdateNone,
      kTemporalUpdateAltrefWithoutDependency,
      kTemporalUpdateNone,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateLast,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
      kTemporalUpdateGolden,
      kTemporalUpdateNone,
      kTemporalUpdateAltref,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[16] =
      { 0, 3, 2, 3, 1, 3, 2, 3, 0, 3, 2, 3, 1, 3, 2, 3 };

  bool expected_layer_sync[16] =
      { false, true, true, true, true, true, false, true,
        false, true, false, true, false, true, false, true };

  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(expected_flags[i], tl.EncodeFlags());
    tl.PopulateCodecSpecific(false, &vp8_info);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    bool expected_sync = expected_layer_sync[i];
    EXPECT_EQ(expected_sync, vp8_info.layerSync);
  }
}

TEST(TemporalLayersTest, KeyFrame) {
  TemporalLayers tl(3);
  vpx_codec_enc_cfg_t cfg;
  CodecSpecificInfoVP8 vp8_info;
  tl.ConfigureBitrates(500, &cfg);

  int expected_flags[8] = {
      kTemporalUpdateLast,
      kTemporalUpdateAltrefWithoutDependency,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateAltref,
      kTemporalUpdateLast,
      kTemporalUpdateAltref,
      kTemporalUpdateGolden,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[8] =
      { 0, 0, 0, 0, 0, 0, 0, 2};

  bool expected_layer_sync[8] =
      { false, true, true, false, false, false, false, false };

  for (int i = 0; i < 7; ++i) {
    EXPECT_EQ(expected_flags[i], tl.EncodeFlags());
    tl.PopulateCodecSpecific(true, &vp8_info);
    EXPECT_EQ(expected_temporal_idx[i], vp8_info.temporalIdx);
    bool expected_sync = expected_layer_sync[i];
    EXPECT_EQ(expected_sync, vp8_info.layerSync);
  }
  EXPECT_EQ(expected_flags[7], tl.EncodeFlags());
  tl.PopulateCodecSpecific(false, &vp8_info);
  EXPECT_EQ(expected_temporal_idx[7], vp8_info.temporalIdx);
  bool expected_sync = expected_layer_sync[7];
  EXPECT_EQ(expected_sync, vp8_info.layerSync);
}
}  // namespace webrtc
