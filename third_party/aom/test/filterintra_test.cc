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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/enums.h"

namespace {

using ::testing::tuple;
using libaom_test::ACMRandom;

typedef void (*Predictor)(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                          const uint8_t *above, const uint8_t *left, int mode);

// Note:
//  Test parameter list:
//  Reference predictor, optimized predictor, prediction mode, tx size
//
typedef tuple<Predictor, Predictor, int> PredFuncMode;
typedef tuple<PredFuncMode, TX_SIZE> PredParams;

const int MaxTxSize = 32;

const int MaxTestNum = 100;

class AV1FilterIntraPredTest : public ::testing::TestWithParam<PredParams> {
 public:
  virtual ~AV1FilterIntraPredTest() {}
  virtual void SetUp() {
    PredFuncMode funcMode = GET_PARAM(0);
    predFuncRef_ = ::testing::get<0>(funcMode);
    predFunc_ = ::testing::get<1>(funcMode);
    mode_ = ::testing::get<2>(funcMode);
    txSize_ = GET_PARAM(1);

    alloc_ = new uint8_t[2 * MaxTxSize + 1];
    predRef_ = new uint8_t[MaxTxSize * MaxTxSize];
    pred_ = new uint8_t[MaxTxSize * MaxTxSize];
  }

  virtual void TearDown() {
    delete[] alloc_;
    delete[] predRef_;
    delete[] pred_;
    libaom_test::ClearSystemState();
  }

 protected:
  void RunTest() const {
    int tstIndex = 0;
    int stride = tx_size_wide[txSize_];
    uint8_t *left = alloc_;
    uint8_t *above = alloc_ + MaxTxSize;
    while (tstIndex < MaxTestNum) {
      PrepareBuffer();
      predFuncRef_(predRef_, stride, txSize_, &above[1], left, mode_);
      ASM_REGISTER_STATE_CHECK(
          predFunc_(pred_, stride, txSize_, &above[1], left, mode_));
      DiffPred(tstIndex);
      tstIndex += 1;
    }
  }

 private:
  void PrepareBuffer() const {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    int i = 0;
    while (i < (2 * MaxTxSize + 1)) {
      alloc_[i] = rnd.Rand8();
      i++;
    }
  }

  void DiffPred(int testNum) const {
    int i = 0;
    while (i < tx_size_wide[txSize_] * tx_size_high[txSize_]) {
      EXPECT_EQ(predRef_[i], pred_[i]) << "Error at position: " << i << " "
                                       << "Tx size: " << tx_size_wide[txSize_]
                                       << "x" << tx_size_high[txSize_] << " "
                                       << "Test number: " << testNum;
      i++;
    }
  }

  Predictor predFunc_;
  Predictor predFuncRef_;
  int mode_;
  TX_SIZE txSize_;
  uint8_t *alloc_;
  uint8_t *pred_;
  uint8_t *predRef_;
};

TEST_P(AV1FilterIntraPredTest, BitExactCheck) { RunTest(); }

using ::testing::make_tuple;

const PredFuncMode kPredFuncMdArray[] = {
  make_tuple(&av1_filter_intra_predictor_c, &av1_filter_intra_predictor_sse4_1,
             FILTER_DC_PRED),
  make_tuple(&av1_filter_intra_predictor_c, &av1_filter_intra_predictor_sse4_1,
             FILTER_V_PRED),
  make_tuple(&av1_filter_intra_predictor_c, &av1_filter_intra_predictor_sse4_1,
             FILTER_H_PRED),
  make_tuple(&av1_filter_intra_predictor_c, &av1_filter_intra_predictor_sse4_1,
             FILTER_D157_PRED),
  make_tuple(&av1_filter_intra_predictor_c, &av1_filter_intra_predictor_sse4_1,
             FILTER_PAETH_PRED),
};

const TX_SIZE kTxSize[] = { TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_4X8,
                            TX_8X4,  TX_8X16, TX_16X8,  TX_16X32, TX_32X16,
                            TX_4X16, TX_16X4, TX_8X32,  TX_32X8 };

INSTANTIATE_TEST_CASE_P(
    SSE4_1, AV1FilterIntraPredTest,
    ::testing::Combine(::testing::ValuesIn(kPredFuncMdArray),
                       ::testing::ValuesIn(kTxSize)));
}  // namespace
