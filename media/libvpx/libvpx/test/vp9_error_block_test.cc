/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <cstdlib>
#include <string>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_entropy.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"

using libvpx_test::ACMRandom;

namespace {
#if CONFIG_VP9_HIGHBITDEPTH
const int kNumIterations = 1000;

typedef int64_t (*ErrorBlockFunc)(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff,
                                  intptr_t block_size,
                                  int64_t *ssz, int bps);

typedef std::tr1::tuple<ErrorBlockFunc, ErrorBlockFunc, vpx_bit_depth_t>
                        ErrorBlockParam;

class ErrorBlockTest
  : public ::testing::TestWithParam<ErrorBlockParam> {
 public:
  virtual ~ErrorBlockTest() {}
  virtual void SetUp() {
    error_block_op_     = GET_PARAM(0);
    ref_error_block_op_ = GET_PARAM(1);
    bit_depth_  = GET_PARAM(2);
  }

  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  vpx_bit_depth_t bit_depth_;
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
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      coeff[j]   = rnd(2 << 20) - (1 << 20);
      dqcoeff[j] = rnd(2 << 20) - (1 << 20);
    }
    ref_ret = ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz,
                                  bit_depth_);
    ASM_REGISTER_STATE_CHECK(ret = error_block_op_(coeff, dqcoeff, block_size,
                                                   &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match SSE2 output. "
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
  int max_val = ((1 << 20) - 1);
  for (int i = 0; i < kNumIterations; ++i) {
    int err_count = 0;
    int k = (i / 9) % 5;

    // Change the maximum coeff value, to test different bit boundaries
    if ( k == 4 && (i % 9) == 0 ) {
      max_val >>= 1;
    }
    block_size = 16 << (i % 9);  // All block sizes from 4x4, 8x4 ..64x64
    for (int j = 0; j < block_size; j++) {
      if (k < 4) {  // Test at maximum values
        coeff[j]   = k % 2 ? max_val : -max_val;
        dqcoeff[j] = (k >> 1) % 2 ? max_val : -max_val;
      } else {
        coeff[j]   = rnd(2 << 14) - (1 << 14);
        dqcoeff[j] = rnd(2 << 14) - (1 << 14);
      }
    }
    ref_ret = ref_error_block_op_(coeff, dqcoeff, block_size, &ref_ssz,
                                  bit_depth_);
    ASM_REGISTER_STATE_CHECK(ret = error_block_op_(coeff, dqcoeff, block_size,
                                                   &ssz, bit_depth_));
    err_count += (ref_ret != ret) | (ref_ssz != ssz);
    if (err_count && !err_count_total) {
      first_failure = i;
    }
    err_count_total += err_count;
  }
  EXPECT_EQ(0, err_count_total)
      << "Error: Error Block Test, C output doesn't match SSE2 output. "
      << "First failed at test case " << first_failure;
}

using std::tr1::make_tuple;

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, ErrorBlockTest,
    ::testing::Values(
        make_tuple(&vp9_highbd_block_error_sse2,
                   &vp9_highbd_block_error_c, VPX_BITS_10),
        make_tuple(&vp9_highbd_block_error_sse2,
                   &vp9_highbd_block_error_c, VPX_BITS_12),
        make_tuple(&vp9_highbd_block_error_sse2,
                   &vp9_highbd_block_error_c, VPX_BITS_8)));
#endif  // HAVE_SSE2
#endif  // CONFIG_VP9_HIGHBITDEPTH
}  // namespace
