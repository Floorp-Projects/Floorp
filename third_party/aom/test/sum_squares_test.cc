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

#include <cmath>
#include <cstdlib>
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "test/function_equivalence_test.h"

using libaom_test::ACMRandom;
using libaom_test::FunctionEquivalenceTest;

namespace {
const int kNumIterations = 10000;

static const int16_t kInt13Max = (1 << 12) - 1;

typedef uint64_t (*SSI16Func)(const int16_t *src, int stride, int width,
                              int height);
typedef libaom_test::FuncParam<SSI16Func> TestFuncs;

class SumSquaresTest : public ::testing::TestWithParam<TestFuncs> {
 public:
  virtual ~SumSquaresTest() {}
  virtual void SetUp() { params_ = this->GetParam(); }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  TestFuncs params_;
};

TEST_P(SumSquaresTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, int16_t, src[256 * 256]);

  int failed = 0;

  const int msb = 11;  // Up to 12 bit input
  const int limit = 1 << (msb + 1);

  for (int k = 0; k < kNumIterations; k++) {
    int width = 4 * rnd(32);   // Up to 128x128
    int height = 4 * rnd(32);  // Up to 128x128
    int stride = 4 << rnd(7);  // Up to 256 stride
    while (stride < width) {   // Make sure it's valid
      stride = 4 << rnd(7);
    }

    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src[ii * stride + jj] = rnd(2) ? rnd(limit) : -rnd(limit);
      }
    }

    const uint64_t res_ref = params_.ref_func(src, stride, width, height);
    uint64_t res_tst;
    ASM_REGISTER_STATE_CHECK(res_tst =
                                 params_.tst_func(src, stride, width, height));

    if (!failed) {
      failed = res_ref != res_tst;
      EXPECT_EQ(res_ref, res_tst)
          << "Error: Sum Squares Test"
          << " C output does not match optimized output.";
    }
  }
}

TEST_P(SumSquaresTest, ExtremeValues) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, int16_t, src[256 * 256]);

  int failed = 0;

  const int msb = 11;  // Up to 12 bit input
  const int limit = 1 << (msb + 1);

  for (int k = 0; k < kNumIterations; k++) {
    int width = 4 * rnd(32);   // Up to 128x128
    int height = 4 * rnd(32);  // Up to 128x128
    int stride = 4 << rnd(7);  // Up to 256 stride
    while (stride < width) {   // Make sure it's valid
      stride = 4 << rnd(7);
    }

    int val = rnd(2) ? limit - 1 : -(limit - 1);
    for (int ii = 0; ii < height; ii++) {
      for (int jj = 0; jj < width; jj++) {
        src[ii * stride + jj] = val;
      }
    }

    const uint64_t res_ref = params_.ref_func(src, stride, width, height);
    uint64_t res_tst;
    ASM_REGISTER_STATE_CHECK(res_tst =
                                 params_.tst_func(src, stride, width, height));

    if (!failed) {
      failed = res_ref != res_tst;
      EXPECT_EQ(res_ref, res_tst)
          << "Error: Sum Squares Test"
          << " C output does not match optimized output.";
    }
  }
}

#if HAVE_SSE2

INSTANTIATE_TEST_CASE_P(
    SSE2, SumSquaresTest,
    ::testing::Values(TestFuncs(&aom_sum_squares_2d_i16_c,
                                &aom_sum_squares_2d_i16_sse2)));

#endif  // HAVE_SSE2

//////////////////////////////////////////////////////////////////////////////
// 1D version
//////////////////////////////////////////////////////////////////////////////

typedef uint64_t (*F1D)(const int16_t *src, uint32_t N);
typedef libaom_test::FuncParam<F1D> TestFuncs1D;

class SumSquares1DTest : public FunctionEquivalenceTest<F1D> {
 protected:
  static const int kIterations = 1000;
  static const int kMaxSize = 256;
};

TEST_P(SumSquares1DTest, RandomValues) {
  DECLARE_ALIGNED(16, int16_t, src[kMaxSize * kMaxSize]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < kMaxSize * kMaxSize; ++i)
      src[i] = rng_(kInt13Max * 2 + 1) - kInt13Max;

    const int N = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, N);
    uint64_t tst_res;
    ASM_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, N));

    ASSERT_EQ(ref_res, tst_res);
  }
}

TEST_P(SumSquares1DTest, ExtremeValues) {
  DECLARE_ALIGNED(16, int16_t, src[kMaxSize * kMaxSize]);

  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    if (rng_(2)) {
      for (int i = 0; i < kMaxSize * kMaxSize; ++i) src[i] = kInt13Max;
    } else {
      for (int i = 0; i < kMaxSize * kMaxSize; ++i) src[i] = -kInt13Max;
    }

    const int N = rng_(2) ? rng_(kMaxSize * kMaxSize + 1 - kMaxSize) + kMaxSize
                          : rng_(kMaxSize) + 1;

    const uint64_t ref_res = params_.ref_func(src, N);
    uint64_t tst_res;
    ASM_REGISTER_STATE_CHECK(tst_res = params_.tst_func(src, N));

    ASSERT_EQ(ref_res, tst_res);
  }
}

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, SumSquares1DTest,
                        ::testing::Values(TestFuncs1D(
                            aom_sum_squares_i16_c, aom_sum_squares_i16_sse2)));

#endif  // HAVE_SSE2
}  // namespace
