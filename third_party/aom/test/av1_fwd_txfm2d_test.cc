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

#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_txfm.h"
#include "./av1_rtcd.h"

using libaom_test::ACMRandom;
using libaom_test::input_base;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::Fwd_Txfm2d_Func;
using libaom_test::TYPE_TXFM;

namespace {
#if CONFIG_HIGHBITDEPTH
// tx_type_, tx_size_, max_error_, max_avg_error_
typedef std::tr1::tuple<TX_TYPE, TX_SIZE, double, double> AV1FwdTxfm2dParam;

class AV1FwdTxfm2d : public ::testing::TestWithParam<AV1FwdTxfm2dParam> {
 public:
  virtual void SetUp() {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
    count_ = 500;
    TXFM_2D_FLIP_CFG fwd_txfm_flip_cfg =
        av1_get_fwd_txfm_cfg(tx_type_, tx_size_);
    // TODO(sarahparker) this test will need to be updated when these
    // functions are extended to support rectangular transforms
    int amplify_bit = fwd_txfm_flip_cfg.row_cfg->shift[0] +
                      fwd_txfm_flip_cfg.row_cfg->shift[1] +
                      fwd_txfm_flip_cfg.row_cfg->shift[2];
    ud_flip_ = fwd_txfm_flip_cfg.ud_flip;
    lr_flip_ = fwd_txfm_flip_cfg.lr_flip;
    amplify_factor_ =
        amplify_bit >= 0 ? (1 << amplify_bit) : (1.0 / (1 << -amplify_bit));

    fwd_txfm_ = libaom_test::fwd_txfm_func_ls[tx_size_];
    txfm1d_size_ = libaom_test::get_txfm1d_size(tx_size_);
    txfm2d_size_ = txfm1d_size_ * txfm1d_size_;
    get_txfm1d_type(tx_type_, &type0_, &type1_);
    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(input_[0]) * txfm2d_size_));
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(output_[0]) * txfm2d_size_));
    ref_input_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_input_[0]) * txfm2d_size_));
    ref_output_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_output_[0]) * txfm2d_size_));
  }

  void RunFwdAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    double avg_abs_error = 0;
    for (int ci = 0; ci < count_; ci++) {
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        input_[ni] = rnd.Rand16() % input_base;
        ref_input_[ni] = static_cast<double>(input_[ni]);
        output_[ni] = 0;
        ref_output_[ni] = 0;
      }

      fwd_txfm_(input_, output_, txfm1d_size_, tx_type_, bd);

      if (lr_flip_ && ud_flip_)
        libaom_test::fliplrud(ref_input_, txfm1d_size_, txfm1d_size_);
      else if (lr_flip_)
        libaom_test::fliplr(ref_input_, txfm1d_size_, txfm1d_size_);
      else if (ud_flip_)
        libaom_test::flipud(ref_input_, txfm1d_size_, txfm1d_size_);

      reference_hybrid_2d(ref_input_, ref_output_, txfm1d_size_, type0_,
                          type1_);

      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        ref_output_[ni] = round(ref_output_[ni] * amplify_factor_);
        EXPECT_GE(max_error_,
                  fabs(output_[ni] - ref_output_[ni]) / amplify_factor_);
      }
      avg_abs_error += compute_avg_abs_error<int32_t, double>(
          output_, ref_output_, txfm2d_size_);
    }

    avg_abs_error /= amplify_factor_;
    avg_abs_error /= count_;
    // max_abs_avg_error comes from upper bound of avg_abs_error
    // printf("type0: %d type1: %d txfm_size: %d accuracy_avg_abs_error:
    // %f\n", type0_, type1_, txfm1d_size_, avg_abs_error);
    EXPECT_GE(max_avg_error_, avg_abs_error);
  }

  virtual void TearDown() {
    aom_free(input_);
    aom_free(output_);
    aom_free(ref_input_);
    aom_free(ref_output_);
  }

 private:
  double max_error_;
  double max_avg_error_;
  int count_;
  double amplify_factor_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
  int txfm1d_size_;
  int txfm2d_size_;
  Fwd_Txfm2d_Func fwd_txfm_;
  TYPE_TXFM type0_;
  TYPE_TXFM type1_;
  int16_t *input_;
  int32_t *output_;
  double *ref_input_;
  double *ref_output_;
  int ud_flip_;  // flip upside down
  int lr_flip_;  // flip left to right
};

TEST_P(AV1FwdTxfm2d, RunFwdAccuracyCheck) { RunFwdAccuracyCheck(); }
const AV1FwdTxfm2dParam av1_fwd_txfm2d_param_c[] = {
#if CONFIG_EXT_TX
  AV1FwdTxfm2dParam(FLIPADST_DCT, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(DCT_FLIPADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(FLIPADST_FLIPADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(ADST_FLIPADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(FLIPADST_ADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(FLIPADST_DCT, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(DCT_FLIPADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(FLIPADST_FLIPADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(ADST_FLIPADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(FLIPADST_ADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(FLIPADST_DCT, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(DCT_FLIPADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(FLIPADST_FLIPADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(ADST_FLIPADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(FLIPADST_ADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(FLIPADST_DCT, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(DCT_FLIPADST, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(FLIPADST_FLIPADST, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(ADST_FLIPADST, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(FLIPADST_ADST, TX_32X32, 70, 7),
#endif
  AV1FwdTxfm2dParam(DCT_DCT, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(ADST_DCT, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(DCT_ADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(ADST_ADST, TX_4X4, 2, 0.2),
  AV1FwdTxfm2dParam(DCT_DCT, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(ADST_DCT, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(DCT_ADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(ADST_ADST, TX_8X8, 5, 0.6),
  AV1FwdTxfm2dParam(DCT_DCT, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(ADST_DCT, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(DCT_ADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(ADST_ADST, TX_16X16, 11, 1.5),
  AV1FwdTxfm2dParam(DCT_DCT, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(ADST_DCT, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(DCT_ADST, TX_32X32, 70, 7),
  AV1FwdTxfm2dParam(ADST_ADST, TX_32X32, 70, 7)
};

INSTANTIATE_TEST_CASE_P(C, AV1FwdTxfm2d,
                        ::testing::ValuesIn(av1_fwd_txfm2d_param_c));

TEST(AV1FwdTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    // TODO(angiebird): include rect txfm in this test
    for (int tx_size = 0; tx_size < TX_SIZES; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
        TXFM_2D_FLIP_CFG cfg = av1_get_fwd_txfm_cfg(
            static_cast<TX_TYPE>(tx_type), static_cast<TX_SIZE>(tx_size));
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_fwd_stage_range(stage_range_col, stage_range_row, &cfg, bd);
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
