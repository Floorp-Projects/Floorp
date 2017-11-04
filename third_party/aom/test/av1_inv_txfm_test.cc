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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/av1_txfm_test.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/blockd.h"
#include "av1/common/scan.h"
#include "aom/aom_integer.h"
#include "aom_dsp/inv_txfm.h"

using libaom_test::ACMRandom;

namespace {

typedef void (*IdctFunc)(const tran_low_t *in, tran_low_t *out);

class TransTestBase {
 public:
  virtual ~TransTestBase() {}

 protected:
  void RunInvAccuracyCheck() {
    tran_low_t input[64];
    tran_low_t output[64];
    double ref_input[64];
    double ref_output[64];

    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    for (int ti = 0; ti < count_test_block; ++ti) {
      for (int ni = 0; ni < txfm_size_; ++ni) {
        input[ni] = rnd.Rand8() - rnd.Rand8();
        ref_input[ni] = static_cast<double>(input[ni]);
      }

      inv_txfm_(input, output);
      libaom_test::reference_idct_1d(ref_input, ref_output, txfm_size_);

      for (int ni = 0; ni < txfm_size_; ++ni) {
        EXPECT_LE(
            abs(output[ni] - static_cast<tran_low_t>(round(ref_output[ni]))),
            max_error_);
      }
    }
  }

  double max_error_;
  int txfm_size_;
  IdctFunc inv_txfm_;
};

typedef std::tr1::tuple<IdctFunc, int, int> IdctParam;
class AV1InvTxfm : public TransTestBase,
                   public ::testing::TestWithParam<IdctParam> {
 public:
  virtual void SetUp() {
    inv_txfm_ = GET_PARAM(0);
    txfm_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
  }
  virtual void TearDown() {}
};

TEST_P(AV1InvTxfm, RunInvAccuracyCheck) { RunInvAccuracyCheck(); }

INSTANTIATE_TEST_CASE_P(C, AV1InvTxfm,
                        ::testing::Values(IdctParam(&aom_idct4_c, 4, 1),
                                          IdctParam(&aom_idct8_c, 8, 2),
                                          IdctParam(&aom_idct16_c, 16, 4),
                                          IdctParam(&aom_idct32_c, 32, 6)));

#if CONFIG_AV1_ENCODER
typedef void (*FwdTxfmFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*InvTxfmFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef std::tr1::tuple<FwdTxfmFunc, InvTxfmFunc, InvTxfmFunc, TX_SIZE, int>
    PartialInvTxfmParam;
#if !CONFIG_ADAPT_SCAN
const int kMaxNumCoeffs = 1024;
#endif
class AV1PartialIDctTest
    : public ::testing::TestWithParam<PartialInvTxfmParam> {
 public:
  virtual ~AV1PartialIDctTest() {}
  virtual void SetUp() {
    ftxfm_ = GET_PARAM(0);
    full_itxfm_ = GET_PARAM(1);
    partial_itxfm_ = GET_PARAM(2);
    tx_size_ = GET_PARAM(3);
    last_nonzero_ = GET_PARAM(4);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int last_nonzero_;
  TX_SIZE tx_size_;
  FwdTxfmFunc ftxfm_;
  InvTxfmFunc full_itxfm_;
  InvTxfmFunc partial_itxfm_;
};

#if !CONFIG_ADAPT_SCAN
static MB_MODE_INFO get_mbmi() {
  MB_MODE_INFO mbmi;
  mbmi.ref_frame[0] = LAST_FRAME;
  assert(is_inter_block(&mbmi));
  return mbmi;
}

TEST_P(AV1PartialIDctTest, RunQuantCheck) {
  int size;
  switch (tx_size_) {
    case TX_4X4: size = 4; break;
    case TX_8X8: size = 8; break;
    case TX_16X16: size = 16; break;
    case TX_32X32: size = 32; break;
    default: FAIL() << "Wrong Size!"; break;
  }
  DECLARE_ALIGNED(16, tran_low_t, test_coef_block1[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, test_coef_block2[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst1[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst2[kMaxNumCoeffs]);

  const int count_test_block = 1000;
  const int block_size = size * size;

  DECLARE_ALIGNED(16, int16_t, input_extreme_block[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kMaxNumCoeffs]);

  int max_error = 0;
  for (int m = 0; m < count_test_block; ++m) {
    // clear out destination buffer
    memset(dst1, 0, sizeof(*dst1) * block_size);
    memset(dst2, 0, sizeof(*dst2) * block_size);
    memset(test_coef_block1, 0, sizeof(*test_coef_block1) * block_size);
    memset(test_coef_block2, 0, sizeof(*test_coef_block2) * block_size);

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (int n = 0; n < count_test_block; ++n) {
      // Initialize a test block with input range [-255, 255].
      if (n == 0) {
        for (int j = 0; j < block_size; ++j) input_extreme_block[j] = 255;
      } else if (n == 1) {
        for (int j = 0; j < block_size; ++j) input_extreme_block[j] = -255;
      } else {
        for (int j = 0; j < block_size; ++j) {
          input_extreme_block[j] = rnd.Rand8() % 2 ? 255 : -255;
        }
      }

      ftxfm_(input_extreme_block, output_ref_block, size);

      // quantization with maximum allowed step sizes
      test_coef_block1[0] = (output_ref_block[0] / 1336) * 1336;
      MB_MODE_INFO mbmi = get_mbmi();
      for (int j = 1; j < last_nonzero_; ++j)
        test_coef_block1[get_scan((const AV1_COMMON *)NULL, tx_size_, DCT_DCT,
                                  &mbmi)
                             ->scan[j]] = (output_ref_block[j] / 1828) * 1828;
    }

    ASM_REGISTER_STATE_CHECK(full_itxfm_(test_coef_block1, dst1, size));
    ASM_REGISTER_STATE_CHECK(partial_itxfm_(test_coef_block1, dst2, size));

    for (int j = 0; j < block_size; ++j) {
      const int diff = dst1[j] - dst2[j];
      const int error = diff * diff;
      if (max_error < error) max_error = error;
    }
  }

  EXPECT_EQ(0, max_error)
      << "Error: partial inverse transform produces different results";
}

TEST_P(AV1PartialIDctTest, ResultsMatch) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int size;
  switch (tx_size_) {
    case TX_4X4: size = 4; break;
    case TX_8X8: size = 8; break;
    case TX_16X16: size = 16; break;
    case TX_32X32: size = 32; break;
    default: FAIL() << "Wrong Size!"; break;
  }
  DECLARE_ALIGNED(16, tran_low_t, test_coef_block1[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, test_coef_block2[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst1[kMaxNumCoeffs]);
  DECLARE_ALIGNED(16, uint8_t, dst2[kMaxNumCoeffs]);
  const int count_test_block = 1000;
  const int max_coeff = 32766 / 4;
  const int block_size = size * size;
  int max_error = 0;
  for (int i = 0; i < count_test_block; ++i) {
    // clear out destination buffer
    memset(dst1, 0, sizeof(*dst1) * block_size);
    memset(dst2, 0, sizeof(*dst2) * block_size);
    memset(test_coef_block1, 0, sizeof(*test_coef_block1) * block_size);
    memset(test_coef_block2, 0, sizeof(*test_coef_block2) * block_size);
    int max_energy_leftover = max_coeff * max_coeff;
    for (int j = 0; j < last_nonzero_; ++j) {
      int16_t coef = static_cast<int16_t>(sqrt(1.0 * max_energy_leftover) *
                                          (rnd.Rand16() - 32768) / 65536);
      max_energy_leftover -= coef * coef;
      if (max_energy_leftover < 0) {
        max_energy_leftover = 0;
        coef = 0;
      }
      MB_MODE_INFO mbmi = get_mbmi();
      test_coef_block1[get_scan((const AV1_COMMON *)NULL, tx_size_, DCT_DCT,
                                &mbmi)
                           ->scan[j]] = coef;
    }

    memcpy(test_coef_block2, test_coef_block1,
           sizeof(*test_coef_block2) * block_size);

    ASM_REGISTER_STATE_CHECK(full_itxfm_(test_coef_block1, dst1, size));
    ASM_REGISTER_STATE_CHECK(partial_itxfm_(test_coef_block2, dst2, size));

    for (int j = 0; j < block_size; ++j) {
      const int diff = dst1[j] - dst2[j];
      const int error = diff * diff;
      if (max_error < error) max_error = error;
    }
  }

  EXPECT_EQ(0, max_error)
      << "Error: partial inverse transform produces different results";
}
#endif
using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(
    C, AV1PartialIDctTest,
    ::testing::Values(make_tuple(&aom_fdct32x32_c, &aom_idct32x32_1024_add_c,
                                 &aom_idct32x32_34_add_c, TX_32X32, 34),
                      make_tuple(&aom_fdct32x32_c, &aom_idct32x32_1024_add_c,
                                 &aom_idct32x32_1_add_c, TX_32X32, 1),
                      make_tuple(&aom_fdct16x16_c, &aom_idct16x16_256_add_c,
                                 &aom_idct16x16_10_add_c, TX_16X16, 10),
                      make_tuple(&aom_fdct16x16_c, &aom_idct16x16_256_add_c,
                                 &aom_idct16x16_1_add_c, TX_16X16, 1),
                      make_tuple(&aom_fdct8x8_c, &aom_idct8x8_64_add_c,
                                 &aom_idct8x8_12_add_c, TX_8X8, 12),
                      make_tuple(&aom_fdct8x8_c, &aom_idct8x8_64_add_c,
                                 &aom_idct8x8_1_add_c, TX_8X8, 1),
                      make_tuple(&aom_fdct4x4_c, &aom_idct4x4_16_add_c,
                                 &aom_idct4x4_1_add_c, TX_4X4, 1)));
#endif  // CONFIG_AV1_ENCODER
}  // namespace
