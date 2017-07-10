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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "av1/common/scan.h"
#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"

using libaom_test::ACMRandom;

namespace {
typedef void (*FwdTxfmFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*InvTxfmFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*InvTxfmWithBdFunc)(const tran_low_t *in, uint8_t *out,
                                  int stride, int bd);

template <InvTxfmFunc fn>
void wrapper(const tran_low_t *in, uint8_t *out, int stride, int bd) {
  (void)bd;
  fn(in, out, stride);
}

typedef std::tr1::tuple<FwdTxfmFunc, InvTxfmWithBdFunc, InvTxfmWithBdFunc,
                        TX_SIZE, int, int, int>
    PartialInvTxfmParam;
const int kMaxNumCoeffs = 1024;
const int kCountTestBlock = 10000;

class PartialIDctTest : public ::testing::TestWithParam<PartialInvTxfmParam> {
 public:
  virtual ~PartialIDctTest() {}
  virtual void SetUp() {
    rnd_.Reset(ACMRandom::DeterministicSeed());
    ftxfm_ = GET_PARAM(0);
    full_itxfm_ = GET_PARAM(1);
    partial_itxfm_ = GET_PARAM(2);
    tx_size_ = GET_PARAM(3);
    last_nonzero_ = GET_PARAM(4);
    bit_depth_ = GET_PARAM(5);
    pixel_size_ = GET_PARAM(6);
    mask_ = (1 << bit_depth_) - 1;

    switch (tx_size_) {
      case TX_4X4: size_ = 4; break;
      case TX_8X8: size_ = 8; break;
      case TX_16X16: size_ = 16; break;
      case TX_32X32: size_ = 32; break;
      default: FAIL() << "Wrong Size!"; break;
    }

    // Randomize stride_ to a value less than or equal to 1024
    stride_ = rnd_(1024) + 1;
    if (stride_ < size_) {
      stride_ = size_;
    }
    // Align stride_ to 16 if it's bigger than 16.
    if (stride_ > 16) {
      stride_ &= ~15;
    }

    input_block_size_ = size_ * size_;
    output_block_size_ = size_ * stride_;

    input_block_ = reinterpret_cast<tran_low_t *>(
        aom_memalign(16, sizeof(*input_block_) * input_block_size_));
    output_block_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, pixel_size_ * output_block_size_));
    output_block_ref_ = reinterpret_cast<uint8_t *>(
        aom_memalign(16, pixel_size_ * output_block_size_));
  }

  virtual void TearDown() {
    aom_free(input_block_);
    input_block_ = NULL;
    aom_free(output_block_);
    output_block_ = NULL;
    aom_free(output_block_ref_);
    output_block_ref_ = NULL;
    libaom_test::ClearSystemState();
  }

  void InitMem() {
    memset(input_block_, 0, sizeof(*input_block_) * input_block_size_);
    if (pixel_size_ == 1) {
      for (int j = 0; j < output_block_size_; ++j) {
        output_block_[j] = output_block_ref_[j] = rnd_.Rand16() & mask_;
      }
    } else {
      ASSERT_EQ(2, pixel_size_);
      uint16_t *const output = reinterpret_cast<uint16_t *>(output_block_);
      uint16_t *const output_ref =
          reinterpret_cast<uint16_t *>(output_block_ref_);
      for (int j = 0; j < output_block_size_; ++j) {
        output[j] = output_ref[j] = rnd_.Rand16() & mask_;
      }
    }
  }

  void InitInput() {
    const int max_coeff = 32766 / 4;
    int max_energy_leftover = max_coeff * max_coeff;
    for (int j = 0; j < last_nonzero_; ++j) {
      int16_t coeff = static_cast<int16_t>(sqrt(1.0 * max_energy_leftover) *
                                           (rnd_.Rand16() - 32768) / 65536);
      max_energy_leftover -= coeff * coeff;
      if (max_energy_leftover < 0) {
        max_energy_leftover = 0;
        coeff = 0;
      }
      input_block_[av1_default_scan_orders[tx_size_].scan[j]] = coeff;
    }
  }

 protected:
  int last_nonzero_;
  TX_SIZE tx_size_;
  tran_low_t *input_block_;
  uint8_t *output_block_;
  uint8_t *output_block_ref_;
  int size_;
  int stride_;
  int pixel_size_;
  int input_block_size_;
  int output_block_size_;
  int bit_depth_;
  int mask_;
  FwdTxfmFunc ftxfm_;
  InvTxfmWithBdFunc full_itxfm_;
  InvTxfmWithBdFunc partial_itxfm_;
  ACMRandom rnd_;
};

TEST_P(PartialIDctTest, RunQuantCheck) {
  DECLARE_ALIGNED(16, int16_t, input_extreme_block[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kMaxNumCoeffs]);

  InitMem();
  for (int i = 0; i < kCountTestBlock; ++i) {
    // Initialize a test block with input range [-mask_, mask_].
    if (i == 0) {
      for (int k = 0; k < input_block_size_; ++k) {
        input_extreme_block[k] = mask_;
      }
    } else if (i == 1) {
      for (int k = 0; k < input_block_size_; ++k) {
        input_extreme_block[k] = -mask_;
      }
    } else {
      for (int k = 0; k < input_block_size_; ++k) {
        input_extreme_block[k] = rnd_.Rand8() % 2 ? mask_ : -mask_;
      }
    }

    ftxfm_(input_extreme_block, output_ref_block, size_);

    // quantization with minimum allowed step sizes
    input_block_[0] = (output_ref_block[0] / 4) * 4;
    for (int k = 1; k < last_nonzero_; ++k) {
      const int pos = av1_default_scan_orders[tx_size_].scan[k];
      input_block_[pos] = (output_ref_block[pos] / 4) * 4;
    }

    ASM_REGISTER_STATE_CHECK(
        full_itxfm_(input_block_, output_block_ref_, stride_, bit_depth_));
    ASM_REGISTER_STATE_CHECK(
        partial_itxfm_(input_block_, output_block_, stride_, bit_depth_));
    ASSERT_EQ(0, memcmp(output_block_ref_, output_block_,
                        pixel_size_ * output_block_size_))
        << "Error: partial inverse transform produces different results";
  }
}

TEST_P(PartialIDctTest, ResultsMatch) {
  for (int i = 0; i < kCountTestBlock; ++i) {
    InitMem();
    InitInput();

    ASM_REGISTER_STATE_CHECK(
        full_itxfm_(input_block_, output_block_ref_, stride_, bit_depth_));
    ASM_REGISTER_STATE_CHECK(
        partial_itxfm_(input_block_, output_block_, stride_, bit_depth_));
    ASSERT_EQ(0, memcmp(output_block_ref_, output_block_,
                        pixel_size_ * output_block_size_))
        << "Error: partial inverse transform produces different results";
  }
}

TEST_P(PartialIDctTest, AddOutputBlock) {
  for (int i = 0; i < kCountTestBlock; ++i) {
    InitMem();
    for (int j = 0; j < last_nonzero_; ++j) {
      input_block_[av1_default_scan_orders[tx_size_].scan[j]] = 10;
    }

    ASM_REGISTER_STATE_CHECK(
        full_itxfm_(input_block_, output_block_ref_, stride_, bit_depth_));
    ASM_REGISTER_STATE_CHECK(
        partial_itxfm_(input_block_, output_block_, stride_, bit_depth_));
    ASSERT_EQ(0, memcmp(output_block_ref_, output_block_,
                        pixel_size_ * output_block_size_))
        << "Error: Transform results are not correctly added to output.";
  }
}

TEST_P(PartialIDctTest, SingleExtremeCoeff) {
  const int16_t max_coeff = INT16_MAX;
  const int16_t min_coeff = INT16_MIN;
  for (int i = 0; i < last_nonzero_; ++i) {
    memset(input_block_, 0, sizeof(*input_block_) * input_block_size_);
    // Run once for min and once for max.
    for (int j = 0; j < 2; ++j) {
      const int coeff = j ? min_coeff : max_coeff;

      memset(output_block_, 0, pixel_size_ * output_block_size_);
      memset(output_block_ref_, 0, pixel_size_ * output_block_size_);
      input_block_[av1_default_scan_orders[tx_size_].scan[i]] = coeff;

      ASM_REGISTER_STATE_CHECK(
          full_itxfm_(input_block_, output_block_ref_, stride_, bit_depth_));
      ASM_REGISTER_STATE_CHECK(
          partial_itxfm_(input_block_, output_block_, stride_, bit_depth_));
      ASSERT_EQ(0, memcmp(output_block_ref_, output_block_,
                          pixel_size_ * output_block_size_))
          << "Error: Fails with single coeff of " << coeff << " at " << i
          << ".";
    }
  }
}

TEST_P(PartialIDctTest, DISABLED_Speed) {
  // Keep runtime stable with transform size.
  const int kCountSpeedTestBlock = 500000000 / input_block_size_;
  InitMem();
  InitInput();

  for (int i = 0; i < kCountSpeedTestBlock; ++i) {
    ASM_REGISTER_STATE_CHECK(
        full_itxfm_(input_block_, output_block_ref_, stride_, bit_depth_));
  }
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < kCountSpeedTestBlock; ++i) {
    partial_itxfm_(input_block_, output_block_, stride_, bit_depth_);
  }
  libaom_test::ClearSystemState();
  aom_usec_timer_mark(&timer);
  const int elapsed_time =
      static_cast<int>(aom_usec_timer_elapsed(&timer) / 1000);
  printf("idct%dx%d_%d (bitdepth %d) time: %5d ms\n", size_, size_,
         last_nonzero_, bit_depth_, elapsed_time);

  ASSERT_EQ(0, memcmp(output_block_ref_, output_block_,
                      pixel_size_ * output_block_size_))
      << "Error: partial inverse transform produces different results";
}

using std::tr1::make_tuple;

const PartialInvTxfmParam c_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_c>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_135_add_c>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_c>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_c>, TX_32X32, 1, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_256_add_c>, TX_16X16, 256, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_38_add_c>, TX_16X16, 38, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_c>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_c>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_64_add_c>, TX_8X8, 64, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_c>, TX_8X8, 12, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_1_add_c>, TX_8X8, 1, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_16_add_c>, TX_4X4, 16, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_1_add_c>, TX_4X4, 1, 8, 1)
};

INSTANTIATE_TEST_CASE_P(C, PartialIDctTest,
                        ::testing::ValuesIn(c_partial_idct_tests));

#if HAVE_NEON && !CONFIG_HIGHBITDEPTH
const PartialInvTxfmParam neon_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_neon>, TX_32X32, 1, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_neon>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_neon>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_neon>, TX_8X8, 12, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_1_add_neon>, TX_8X8, 1, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_1_add_neon>, TX_4X4, 1, 8, 1)
};

INSTANTIATE_TEST_CASE_P(NEON, PartialIDctTest,
                        ::testing::ValuesIn(neon_partial_idct_tests));
#endif  // HAVE_NEON && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2
const PartialInvTxfmParam sse2_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_sse2>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_sse2>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_sse2>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_sse2>, TX_32X32, 1, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_256_add_sse2>, TX_16X16, 256, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_sse2>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_sse2>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_64_add_sse2>, TX_8X8, 64, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_sse2>, TX_8X8, 12, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_1_add_sse2>, TX_8X8, 1, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_16_add_sse2>, TX_4X4, 16, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_1_add_sse2>, TX_4X4, 1, 8, 1)
};

INSTANTIATE_TEST_CASE_P(SSE2, PartialIDctTest,
                        ::testing::ValuesIn(sse2_partial_idct_tests));

#endif  // HAVE_SSE2

#if HAVE_SSSE3
const PartialInvTxfmParam ssse3_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_ssse3>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_135_add_ssse3>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_ssse3>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_64_add_ssse3>, TX_8X8, 64, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_ssse3>, TX_8X8, 12, 8, 1)
};

INSTANTIATE_TEST_CASE_P(SSSE3, PartialIDctTest,
                        ::testing::ValuesIn(ssse3_partial_idct_tests));
#endif  // HAVE_SSSE3

#if HAVE_AVX2
const PartialInvTxfmParam avx2_partial_idct_tests[] = {
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_256_add_avx2>, TX_16X16, 256, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_38_add_avx2>, TX_16X16, 38, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_avx2>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_avx2>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_avx2>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_135_add_avx2>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_avx2>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_avx2>, TX_32X32, 1, 8, 1),
};

INSTANTIATE_TEST_CASE_P(AVX2, PartialIDctTest,
                        ::testing::ValuesIn(avx2_partial_idct_tests));
#endif  // HAVE_AVX2

#if HAVE_DSPR2 && !CONFIG_HIGHBITDEPTH
const PartialInvTxfmParam dspr2_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_dspr2>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_dspr2>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_dspr2>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_dspr2>, TX_32X32, 1, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_256_add_dspr2>, TX_16X16, 256, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_dspr2>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_dspr2>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_64_add_dspr2>, TX_8X8, 64, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_dspr2>, TX_8X8, 12, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_1_add_dspr2>, TX_8X8, 1, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_16_add_dspr2>, TX_4X4, 16, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_1_add_dspr2>, TX_4X4, 1, 8, 1)
};

INSTANTIATE_TEST_CASE_P(DSPR2, PartialIDctTest,
                        ::testing::ValuesIn(dspr2_partial_idct_tests));
#endif  // HAVE_DSPR2 && !CONFIG_HIGHBITDEPTH

#if HAVE_MSA && !CONFIG_HIGHBITDEPTH
const PartialInvTxfmParam msa_partial_idct_tests[] = {
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_msa>, TX_32X32, 1024, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1024_add_msa>, TX_32X32, 135, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_34_add_msa>, TX_32X32, 34, 8, 1),
  make_tuple(&aom_fdct32x32_c, &wrapper<aom_idct32x32_1024_add_c>,
             &wrapper<aom_idct32x32_1_add_msa>, TX_32X32, 1, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_256_add_msa>, TX_16X16, 256, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_10_add_msa>, TX_16X16, 10, 8, 1),
  make_tuple(&aom_fdct16x16_c, &wrapper<aom_idct16x16_256_add_c>,
             &wrapper<aom_idct16x16_1_add_msa>, TX_16X16, 1, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_64_add_msa>, TX_8X8, 64, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_12_add_msa>, TX_8X8, 12, 8, 1),
  make_tuple(&aom_fdct8x8_c, &wrapper<aom_idct8x8_64_add_c>,
             &wrapper<aom_idct8x8_1_add_msa>, TX_8X8, 1, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_16_add_msa>, TX_4X4, 16, 8, 1),
  make_tuple(&aom_fdct4x4_c, &wrapper<aom_idct4x4_16_add_c>,
             &wrapper<aom_idct4x4_1_add_msa>, TX_4X4, 1, 8, 1)
};

INSTANTIATE_TEST_CASE_P(MSA, PartialIDctTest,
                        ::testing::ValuesIn(msa_partial_idct_tests));
#endif  // HAVE_MSA && !CONFIG_HIGHBITDEPTH

}  // namespace
