/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "test/util.h"
#include "aom/aomcx.h"
#include "aom/aom_encoder.h"

namespace {

TEST(EncodeAPI, InvalidParams) {
  static const aom_codec_iface_t *kCodecs[] = {
#if CONFIG_AV1_ENCODER
    &aom_codec_av1_cx_algo,
#endif
  };
  uint8_t buf[1] = { 0 };
  aom_image_t img;
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;

  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, 1, 1, 1, buf));

  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(NULL, NULL, NULL, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_enc_init(&enc, NULL, NULL, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_encode(NULL, NULL, 0, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_encode(NULL, &img, 0, 0, 0, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_destroy(NULL));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_config_default(NULL, NULL, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_enc_config_default(NULL, &cfg, 0));
  EXPECT_TRUE(aom_codec_error(NULL) != NULL);

  for (int i = 0; i < NELEMENTS(kCodecs); ++i) {
    SCOPED_TRACE(aom_codec_iface_name(kCodecs[i]));
    EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
              aom_codec_enc_init(NULL, kCodecs[i], NULL, 0));
    EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
              aom_codec_enc_init(&enc, kCodecs[i], NULL, 0));
    EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
              aom_codec_enc_config_default(kCodecs[i], &cfg, 1));

    EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(kCodecs[i], &cfg, 0));
    EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, kCodecs[i], &cfg, 0));
    EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, NULL, 0, 0, 0, 0));

    EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
  }
}

}  // namespace
