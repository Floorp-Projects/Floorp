/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/function_equivalence_test.h"
#include "test/register_state_check.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/encoder/pickrst.h"
using libaom_test::FunctionEquivalenceTest;

#define MAX_DATA_BLOCK 384

namespace {
static const int kIterations = 100;

typedef int64_t (*lowbd_pixel_proj_error_func)(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params);

typedef libaom_test::FuncParam<lowbd_pixel_proj_error_func> TestFuncs;

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

typedef ::testing::tuple<const lowbd_pixel_proj_error_func>
    PixelProjErrorTestParam;

class PixelProjErrorTest
    : public ::testing::TestWithParam<PixelProjErrorTestParam> {
 public:
  virtual void SetUp() {
    target_func_ = GET_PARAM(0);
    src_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(uint8_t)));
    dgd_ = (uint8_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                  sizeof(uint8_t)));
    flt0_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(int32_t)));
    flt1_ = (int32_t *)(aom_malloc(MAX_DATA_BLOCK * MAX_DATA_BLOCK *
                                   sizeof(int32_t)));
  }
  virtual void TearDown() {
    aom_free(src_);
    aom_free(dgd_);
    aom_free(flt0_);
    aom_free(flt1_);
  }
  void runPixelProjErrorTest(int32_t run_times);
  void runPixelProjErrorTest_ExtremeValues();

 private:
  lowbd_pixel_proj_error_func target_func_;
  ACMRandom rng_;
  uint8_t *src_;
  uint8_t *dgd_;
  int32_t *flt0_;
  int32_t *flt1_;
};

void PixelProjErrorTest::runPixelProjErrorTest(int32_t run_times) {
  int h_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  int v_end = run_times != 1 ? 128 : (rng_.Rand16() % MAX_DATA_BLOCK) + 1;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = run_times == 1 ? kIterations : 4;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = rng_.Rand8();
      src_[i] = rng_.Rand8();
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.r[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    params.s[0] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter % 2);
    params.s[1] = run_times == 1 ? (rng_.Rand8() % MAX_RADIUS) : (iter / 2);
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_ref = av1_lowbd_pixel_proj_error_c(src, h_end, v_end, src_stride, dgd,
                                             dgd_stride, flt0_, flt0_stride,
                                             flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      err_test =
          target_func_(src, h_end, v_end, src_stride, dgd, dgd_stride, flt0_,
                       flt0_stride, flt1_, flt1_stride, xq, &params);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("r0 %d r1 %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", params.r[0],
             params.r[1], h_end, v_end, time1, time2, time1 / time2);
    }
    ASSERT_EQ(err_ref, err_test);
  }
}

void PixelProjErrorTest::runPixelProjErrorTest_ExtremeValues() {
  const int h_start = 0;
  int h_end = 192;
  const int v_start = 0;
  int v_end = 192;
  const int dgd_stride = MAX_DATA_BLOCK;
  const int src_stride = MAX_DATA_BLOCK;
  const int flt0_stride = MAX_DATA_BLOCK;
  const int flt1_stride = MAX_DATA_BLOCK;
  sgr_params_type params;
  int xq[2];
  const int iters = kIterations;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    int64_t err_ref = 0, err_test = 1;
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_[i] = 0;
      src_[i] = 255;
      flt0_[i] = rng_.Rand15Signed();
      flt1_[i] = rng_.Rand15Signed();
    }
    xq[0] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    xq[1] = rng_.Rand8() % (1 << SGRPROJ_PRJ_BITS);
    params.r[0] = rng_.Rand8() % MAX_RADIUS;
    params.r[1] = rng_.Rand8() % MAX_RADIUS;
    params.s[0] = rng_.Rand8() % MAX_RADIUS;
    params.s[1] = rng_.Rand8() % MAX_RADIUS;
    uint8_t *dgd = dgd_;
    uint8_t *src = src_;

    err_ref = av1_lowbd_pixel_proj_error_c(
        src, h_end - h_start, v_end - v_start, src_stride, dgd, dgd_stride,
        flt0_, flt0_stride, flt1_, flt1_stride, xq, &params);

    err_test = target_func_(src, h_end - h_start, v_end - v_start, src_stride,
                            dgd, dgd_stride, flt0_, flt0_stride, flt1_,
                            flt1_stride, xq, &params);

    ASSERT_EQ(err_ref, err_test);
  }
}

TEST_P(PixelProjErrorTest, RandomValues) { runPixelProjErrorTest(1); }

TEST_P(PixelProjErrorTest, ExtremeValues) {
  runPixelProjErrorTest_ExtremeValues();
}

TEST_P(PixelProjErrorTest, DISABLED_Speed) { runPixelProjErrorTest(200000); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, PixelProjErrorTest,
                        ::testing::Values(av1_lowbd_pixel_proj_error_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_CASE_P(AVX2, PixelProjErrorTest,
                        ::testing::Values(av1_lowbd_pixel_proj_error_avx2));
#endif  // HAVE_AVX2

}  // namespace
