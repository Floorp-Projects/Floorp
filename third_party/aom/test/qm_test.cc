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
#include "config/aom_config.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

class QMTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  QMTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~QMTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_ENABLE_QM, 1);
      encoder->Control(AV1E_SET_QM_MIN, qm_min_);
      encoder->Control(AV1E_SET_QM_MAX, qm_max_);

      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT, 100);
    }
  }

  void DoTest(int qm_min, int qm_max) {
    qm_min_ = qm_min;
    qm_max_ = qm_max;
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
  int qm_min_;
  int qm_max_;
};

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM1) { DoTest(5, 9); }

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM2) { DoTest(0, 8); }

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM3) { DoTest(9, 15); }

AV1_INSTANTIATE_TEST_CASE(QMTest,
                          ::testing::Values(::libaom_test::kRealTime,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(5, 9));
}  // namespace
