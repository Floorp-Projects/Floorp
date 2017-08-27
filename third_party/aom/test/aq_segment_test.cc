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

#include "./aom_config.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

class AqSegmentTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AqSegmentTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~AqSegmentTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    aq_mode_ = 0;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
#if CONFIG_EXT_DELTA_Q
      encoder->Control(AV1E_SET_DELTAQ_MODE, deltaq_mode_);
#endif
      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT, 100);
    }
  }

  void DoTest(int aq_mode) {
    aq_mode_ = aq_mode;
#if CONFIG_EXT_DELTA_Q
    deltaq_mode_ = 0;
#endif
    cfg_.kf_max_dist = 12;
    cfg_.rc_min_quantizer = 8;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 6;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_target_bitrate = 300;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 15);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int set_cpu_used_;
  int aq_mode_;
#if CONFIG_EXT_DELTA_Q
  int deltaq_mode_;
#endif
};

// Validate that this AQ segmentation mode (AQ=1, variance_ap)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ1) { DoTest(1); }

// Validate that this AQ segmentation mode (AQ=2, complexity_aq)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ2) { DoTest(2); }

// Validate that this AQ segmentation mode (AQ=3, cyclic_refresh_aq)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ3) { DoTest(3); }

class AqSegmentTestLarge : public AqSegmentTest {};

TEST_P(AqSegmentTestLarge, TestNoMisMatchAQ1) { DoTest(1); }

TEST_P(AqSegmentTestLarge, TestNoMisMatchAQ2) { DoTest(2); }

TEST_P(AqSegmentTestLarge, TestNoMisMatchAQ3) { DoTest(3); }

#if CONFIG_DELTA_Q & !CONFIG_EXT_DELTA_Q
// Validate that this AQ mode (AQ=4, delta q)
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchAQ4) {
  cfg_.rc_end_usage = AOM_CQ;
  aq_mode_ = 4;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 15);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

#if CONFIG_EXT_DELTA_Q
// Validate that this delta q mode
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchExtDeltaQ) {
  cfg_.rc_end_usage = AOM_CQ;
  aq_mode_ = 0;
  deltaq_mode_ = 2;
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 15);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

AV1_INSTANTIATE_TEST_CASE(AqSegmentTest,
                          ::testing::Values(::libaom_test::kRealTime,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(5, 9));
AV1_INSTANTIATE_TEST_CASE(AqSegmentTestLarge,
                          ::testing::Values(::libaom_test::kRealTime,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(3, 5));
}  // namespace
