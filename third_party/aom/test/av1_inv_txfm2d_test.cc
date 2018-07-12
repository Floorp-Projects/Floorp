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
  }

  void RunRoundtripCheck() {
    int tx_w = tx_size_wide[tx_size_];
    int tx_h = tx_size_high[tx_size_];
    int txfm2d_size = tx_w * tx_h;
    const Fwd_Txfm2d_Func fwd_txfm_func =
        libaom_test::fwd_txfm_func_ls[tx_size_];
    const Inv_Txfm2d_Func inv_txfm_func =
        libaom_test::inv_txfm_func_ls[tx_size_];
    double avg_abs_error = 0;
    ACMRandom rnd(ACMRandom::DeterministicSeed());

    const int count = 500;

    for (int ci = 0; ci < count; ci++) {
      int16_t expected[64 * 64] = { 0 };
      ASSERT_LT(txfm2d_size, NELEMENTS(expected));

      for (int ni = 0; ni < txfm2d_size; ++ni) {
        if (ci == 0) {
          int extreme_input = input_base - 1;
          expected[ni] = extreme_input;  // extreme case
        } else {
          expected[ni] = rnd.Rand16() % input_base;
        }
      }

      int32_t coeffs[64 * 64] = { 0 };
      ASSERT_LT(txfm2d_size, NELEMENTS(coeffs));
      fwd_txfm_func(expected, coeffs, tx_w, tx_type_, bd);

      uint16_t actual[64 * 64] = { 0 };
      ASSERT_LT(txfm2d_size, NELEMENTS(actual));
      inv_txfm_func(coeffs, actual, tx_w, tx_type_, bd);

      for (int ni = 0; ni < txfm2d_size; ++ni) {
        EXPECT_GE(max_error_, abs(expected[ni] - actual[ni]));
      }
      avg_abs_error += compute_avg_abs_error<int16_t, uint16_t>(
          expected, actual, txfm2d_size);
    }

    avg_abs_error /= count;
    // max_abs_avg_error comes from upper bound of
    // printf("txfm1d_size: %d accuracy_avg_abs_error: %f\n",
    // txfm1d_size_, avg_abs_error);
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << " tx_w: " << tx_w << " tx_h " << tx_h << " tx_type: " << tx_type_;
  }

 private:
  int max_error_;
  double max_avg_error_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
};

TEST_P(AV1InvTxfm2d, RunRoundtripCheck) { RunRoundtripCheck(); }

const AV1InvTxfm2dParam av1_inv_txfm2d_param[] = {
#if CONFIG_EXT_TX
#if CONFIG_RECT_TX
  AV1InvTxfm2dParam(DCT_DCT, TX_4X8, 2, 0.007),
  AV1InvTxfm2dParam(ADST_DCT, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(DCT_ADST, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(ADST_ADST, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_4X8, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_4X8, 2, 0.012),

  AV1InvTxfm2dParam(DCT_DCT, TX_8X4, 2, 0.007),
  AV1InvTxfm2dParam(ADST_DCT, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(DCT_ADST, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(ADST_ADST, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_8X4, 2, 0.007),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_8X4, 2, 0.012),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_8X4, 2, 0.012),

  AV1InvTxfm2dParam(DCT_DCT, TX_8X16, 2, 0.025),
  AV1InvTxfm2dParam(ADST_DCT, TX_8X16, 2, 0.020),
  AV1InvTxfm2dParam(DCT_ADST, TX_8X16, 2, 0.027),
  AV1InvTxfm2dParam(ADST_ADST, TX_8X16, 2, 0.023),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_8X16, 2, 0.020),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_8X16, 2, 0.027),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_8X16, 2, 0.032),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_8X16, 2, 0.023),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_8X16, 2, 0.023),

  AV1InvTxfm2dParam(DCT_DCT, TX_16X8, 2, 0.007),
  AV1InvTxfm2dParam(ADST_DCT, TX_16X8, 2, 0.012),
  AV1InvTxfm2dParam(DCT_ADST, TX_16X8, 2, 0.024),
  AV1InvTxfm2dParam(ADST_ADST, TX_16X8, 2, 0.033),
  AV1InvTxfm2dParam(FLIPADST_DCT, TX_16X8, 2, 0.015),
  AV1InvTxfm2dParam(DCT_FLIPADST, TX_16X8, 2, 0.032),
  AV1InvTxfm2dParam(FLIPADST_FLIPADST, TX_16X8, 2, 0.032),
  AV1InvTxfm2dParam(ADST_FLIPADST, TX_16X8, 2, 0.033),
  AV1InvTxfm2dParam(FLIPADST_ADST, TX_16X8, 2, 0.032),
#endif
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

TEST(AV1InvTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    // TODO(angiebird): include rect txfm in this test
    for (int tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
        TXFM_2D_FLIP_CFG cfg = av1_get_inv_txfm_cfg(
            static_cast<TX_TYPE>(tx_type), static_cast<TX_SIZE>(tx_size));
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_inv_stage_range(stage_range_col, stage_range_row, &cfg,
                                fwd_shift_sum[tx_size], bd);
        const TXFM_1D_CFG *col_cfg = cfg.col_cfg;
        const TXFM_1D_CFG *row_cfg = cfg.row_cfg;
        libaom_test::txfm_stage_range_check(stage_range_col, col_cfg->stage_num,
                                            col_cfg->cos_bit, low_range,
                                            high_range);
        libaom_test::txfm_stage_range_check(stage_range_row, row_cfg->stage_num,
                                            row_cfg->cos_bit, low_range,
                                            high_range);
      }
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

}  // namespace
