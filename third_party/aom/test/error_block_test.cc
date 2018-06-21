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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"

using libaom_test::ACMRandom;

namespace {
const int kNumIterations = 1000;

typedef int64_t (*ErrorBlockFunc)(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff,
                                  intptr_t block_size, int64_t *ssz, int bps);

typedef ::testing::tuple<ErrorBlockFunc, ErrorBlockFunc, aom_bit_depth_t>
    ErrorBlockParam;

class ErrorBlockTest : public ::testing::TestWithParam<ErrorBlockParam> {
 public:
  virtual ~ErrorBlockTest() {}
  virtual void SetUp() {
    error_block_op_ = GET_PARAM(0);
    ref_error_block_op_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  aom_bit_depth_t bit_depth_;
  ErrorBlockFunc error_block_op_;
  ErrorBlockFunc ref_error_block_op_;
};

TEST_P(ErrorBlockTest, OperationCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff[4096]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[4096]);
  int err_count_total = 0;
  int first_failure = -1;
  intptr_t block_size;
  int64_t ssz;
  int64_t ret;
  int64_t ref_ssz;
  int64_t ref_ret;
  const int msb = bit_depth_ + 8 - 1;
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      // coeff and dqcoeff will always have at least the same sign, and this
      // can be used for optimization, so generate test input precisely.
      if (rnd(2)) {
        // Positive number
        coeff[j] = rnd(1 << msb);
        dqcoeff[j] = rnd(1 << msb);
      } else {
        // Negative number
        coeff[j] = -rnd(1 << msb);
        dqcoeff[j] = -rnd(1 << msb);
      }
    }
    ref_ret =
        ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        ret = error_block_op_(coeff, dqcoeff, block_size, &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match optimized output. "
      << "First failed at test case " << first_failure;
}

TEST_P(ErrorBlockTest, ExtremeValues) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, tran_low_t, coeff[4096]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[4096]);
  int err_count_total = 0;
  int first_failure = -1;
  intptr_t block_size;
  int64_t ssz;
  int64_t ret;
  int64_t ref_ssz;
  int64_t ref_ret;
  const int msb = bit_depth_ + 8 - 1;
  int max_val = ((1 << msb) - 1);
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    int k = (i / 9) % 9;

    // Change the maximum coeff value, to test different bit boundaries
    if (k == 8 && (i % 9) == 0) {
      max_val >>= 1;
    }
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      if (k < 4) {
        // Test at positive maximum values
        coeff[j] = k % 2 ? max_val : 0;
        dqcoeff[j] = (k >> 1) % 2 ? max_val : 0;
      } else if (k < 8) {
        // Test at negative maximum values
        coeff[j] = k % 2 ? -max_val : 0;
        dqcoeff[j] = (k >> 1) % 2 ? -max_val : 0;
      } else {
        if (rnd(2)) {
          // Positive number
          coeff[j] = rnd(1 << 14);
          dqcoeff[j] = rnd(1 << 14);
        } else {
          // Negative number
          coeff[j] = -rnd(1 << 14);
          dqcoeff[j] = -rnd(1 << 14);
        }
      }
    }
    ref_ret =
        ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        ret = error_block_op_(coeff, dqcoeff, block_size, &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match optimized output. "
      << "First failed at test case " << first_failure;
}

#if (HAVE_SSE2 || HAVE_AVX)
using ::testing::make_tuple;

INSTANTIATE_TEST_CASE_P(
    SSE2, ErrorBlockTest,
    ::testing::Values(make_tuple(&av1_highbd_block_error_sse2,
                                 &av1_highbd_block_error_c, AOM_BITS_10),
                      make_tuple(&av1_highbd_block_error_sse2,
                                 &av1_highbd_block_error_c, AOM_BITS_12),
                      make_tuple(&av1_highbd_block_error_sse2,
                                 &av1_highbd_block_error_c, AOM_BITS_8)));
#endif  // HAVE_SSE2
}  // namespace
