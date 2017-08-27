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
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {
class LevelTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  LevelTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), min_gf_internal_(24), target_level_(0),
        level_(0) {}
  virtual ~LevelTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
    if (encoding_mode_ != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 25;
      cfg_.rc_end_usage = AOM_VBR;
    } else {
      cfg_.g_lag_in_frames = 0;
      cfg_.rc_end_usage = AOM_CBR;
    }
    cfg_.rc_2pass_vbr_minsection_pct = 5;
    cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_min_quantizer = 0;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_TARGET_LEVEL, target_level_);
      encoder->Control(AV1E_SET_MIN_GF_INTERVAL, min_gf_internal_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }
    encoder->Control(AV1E_GET_LEVEL, &level_);
    ASSERT_LE(level_, 51);
    ASSERT_GE(level_, 0);
  }

  ::libaom_test::TestMode encoding_mode_;
  int cpu_used_;
  int min_gf_internal_;
  int target_level_;
  int level_;
};

// Test for keeping level stats only
TEST_P(LevelTest, TestTargetLevel0) {
  ::libaom_test::I420VideoSource video("hantro_odd.yuv", 208, 144, 30, 1, 0,
                                       40);
  target_level_ = 0;
  min_gf_internal_ = 4;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(11, level_);

  cfg_.rc_target_bitrate = 1600;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(20, level_);
}

// Test for level control being turned off
TEST_P(LevelTest, TestTargetLevel255) {
  ::libaom_test::I420VideoSource video("hantro_odd.yuv", 208, 144, 30, 1, 0,
                                       30);
  target_level_ = 255;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_P(LevelTest, TestTargetLevelApi) {
  ::libaom_test::I420VideoSource video("hantro_odd.yuv", 208, 144, 30, 1, 0, 1);
  static const aom_codec_iface_t *codec = &aom_codec_av1_cx_algo;
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  for (int level = 0; level <= 256; ++level) {
    if (level == 10 || level == 11 || level == 20 || level == 21 ||
        level == 30 || level == 31 || level == 40 || level == 41 ||
        level == 50 || level == 51 || level == 52 || level == 60 ||
        level == 61 || level == 62 || level == 0 || level == 255)
      EXPECT_EQ(AOM_CODEC_OK,
                aom_codec_control(&enc, AV1E_SET_TARGET_LEVEL, level));
    else
      EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
                aom_codec_control(&enc, AV1E_SET_TARGET_LEVEL, level));
  }
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

AV1_INSTANTIATE_TEST_CASE(LevelTest,
                          ::testing::Values(::libaom_test::kTwoPassGood,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(0, 9));
}  // namespace
