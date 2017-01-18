/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace {

#define NELEMENTS(x) static_cast<int>(sizeof(x) / sizeof(x[0]))

TEST(EncodeAPI, InvalidParams) {
  static const vpx_codec_iface_t *kCodecs[] = {
#if CONFIG_VP8_ENCODER
    &vpx_codec_vp8_cx_algo,
#endif
#if CONFIG_VP9_ENCODER
    &vpx_codec_vp9_cx_algo,
#endif
  };
  uint8_t buf[1] = { 0 };
  vpx_image_t img;
  vpx_codec_ctx_t enc;
  vpx_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, vpx_img_wrap(&img, VPX_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_enc_init(NULL, NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_enc_init(&enc, NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_encode(NULL, NULL, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_encode(NULL, &img, 0, 0, 0, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM, vpx_codec_destroy(NULL));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(NULL, NULL, 0));
  EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
            vpx_codec_enc_config_default(NULL, &cfg, 0));
  EXPECT_TRUE(vpx_codec_error(NULL) != NULL);

  for (int i = 0; i < NELEMENTS(kCodecs); ++i) {
    SCOPED_TRACE(vpx_codec_iface_name(kCodecs[i]));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(NULL, kCodecs[i], NULL, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_init(&enc, kCodecs[i], NULL, 0));
    EXPECT_EQ(VPX_CODEC_INVALID_PARAM,
              vpx_codec_enc_config_default(kCodecs[i], &cfg, 1));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_config_default(kCodecs[i], &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_enc_init(&enc, kCodecs[i], &cfg, 0));
    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_encode(&enc, NULL, 0, 0, 0, 0));

    EXPECT_EQ(VPX_CODEC_OK, vpx_codec_destroy(&enc));
  }
}

}  // namespace
