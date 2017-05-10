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
#include <stdio.h>
#include <stdlib.h>

#include "./av1_rtcd.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"

using libaom_test::ACMRandom;
using libaom_test::input_base;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::Fwd_Txfm2d_Func;
using libaom_test::Inv_Txfm2d_Func;

namespace {

#if CONFIG_HIGHBITDEPTH
// AV1InvTxfm2dParam argument list:
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tr1::tuple<TX_TYPE, TX_SIZE, int, double> AV1InvTxfm2dParam;

class AV1InvTxfm2d : public ::testing::TestWithParam<AV1InvTxfm2dParam> {
 public:
  virtual void SetUp() {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
    txfm1d_size_ = libaom_test::get_txfm1d_size(tx_size_);
    txfm2d_size_ = txfm1d_size_ * txfm1d_size_;
    count_ = 500;

    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(int16_t) * txfm2d_size_));
    ref_input_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * txfm2d_size_));
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(int32_t) * txfm2d_size_));
  }

  void RunRoundtripCheck() {
    const Fwd_Txfm2d_Func fwd_txfm_func =
        libaom_test::fwd_txfm_func_ls[tx_size_];
    const Inv_Txfm2d_Func inv_txfm_func =
        libaom_test::inv_txfm_func_ls[tx_size_];
    double avg_abs_error = 0;
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    for (int ci = 0; ci < count_; ci++) {
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        if (ci == 0) {
          int extreme_input = input_base - 1;
          input_[ni] = extreme_input;  // extreme case
          ref_input_[ni] = 0;
        } else {
          input_[ni] = rnd.Rand16() % input_base;
          ref_input_[ni] = 0;
        }
      }

      fwd_txfm_func(input_, output_, txfm1d_size_, tx_type_, bd);
      inv_txfm_func(output_, ref_input_, txfm1d_size_, tx_type_, bd);

      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        EXPECT_GE(max_error_, abs(input_[ni] - ref_input_[ni]));
      }
      avg_abs_error += compute_avg_abs_error<int16_t, uint16_t>(
          input_, ref_input_, txfm2d_size_);
    }

    avg_abs_error /= count_;
    // max_abs_avg_error comes from upper bound of
    // printf("txfm1d_size: %d accuracy_avg_abs_error: %f\n",
    // txfm1d_size_, avg_abs_error);
    EXPECT_GE(max_avg_error_, avg_abs_error);
  }

  virtual void TearDown() {
    aom_free(input_);
    aom_free(output_);
    aom_free(ref_input_);
  }

 private:
  int count_;
  int max_error_;
  double max_avg_error_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
  int txfm1d_size_;
  int txfm2d_size_;
  int16_t *input_;
  uint16_t *ref_input_;
  int32_t *output_;
};

TEST_P(AV1InvTxfm2d, RunRoundtripCheck) { RunRoundtripCheck(); }

const AV1InvTxfm2dParam av1_inv_txfm2d_param[] = {
#if CONFIG_EXT_TX
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_16X16, 11, 0.04),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_32X32, 4, 0.4),
#endif
  AV1InvTxfm2dParam(DCT_DCT, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(ADST_DCT, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(DCT_ADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(ADST_ADST, TX_4X4, 2, 0.002),
  AV1InvTxfm2dParam(DCT_DCT, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(ADST_DCT, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(DCT_ADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(ADST_ADST, TX_8X8, 2, 0.02),
  AV1InvTxfm2dParam(DCT_DCT, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(ADST_DCT, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(DCT_ADST, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(ADST_ADST, TX_16X16, 2, 0.04),
  AV1InvTxfm2dParam(DCT_DCT, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(ADST_DCT, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(DCT_ADST, TX_32X32, 4, 0.4),
  AV1InvTxfm2dParam(ADST_ADST, TX_32X32, 4, 0.4)
};

INSTANTIATE_TEST_CASE_P(C, AV1InvTxfm2d,
                        ::testing::ValuesIn(av1_inv_txfm2d_param));

#endif  // CONFIG_HIGHBITDEPTH

}  // namespace
