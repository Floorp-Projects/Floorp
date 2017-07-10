/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_config.h"
#include "./av1_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using libaom_test::ACMRandom;

typedef int64_t (*BlockErrorFunc)(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff, intptr_t size,
                                  int64_t *ssz);
#if CONFIG_HIGHBITDEPTH
typedef int64_t (*HbdBlockErrorFunc)(const tran_low_t *coeff,
                                     const tran_low_t *dqcoeff, intptr_t size,
                                     int64_t *ssz, int bd);
#endif

typedef std::tr1::tuple<BlockErrorFunc, BlockErrorFunc, TX_SIZE,
                        aom_bit_depth_t>
    BlockErrorParam;

const int kTestNum = 10000;

class BlockErrorTest : public ::testing::TestWithParam<BlockErrorParam> {
 public:
  BlockErrorTest()
      : blk_err_ref_(GET_PARAM(0)), blk_err_(GET_PARAM(1)),
        tx_size_(GET_PARAM(2)), bd_(GET_PARAM(3)) {}

  virtual ~BlockErrorTest() {}

  virtual void SetUp() {
    const intptr_t block_size = getCoeffNum();
    coeff_ = reinterpret_cast<tran_low_t *>(
        aom_memalign(16, 2 * block_size * sizeof(tran_low_t)));
  }

  virtual void TearDown() {
    aom_free(coeff_);
    coeff_ = NULL;
    libaom_test::ClearSystemState();
  }

  void BlockErrorRun(int testNum) {
    int i;
    int64_t error_ref, error;
    int64_t sse_ref, sse;
    const intptr_t block_size = getCoeffNum();
    tran_low_t *dqcoeff = coeff_ + block_size;
    for (i = 0; i < testNum; ++i) {
      FillRandomData();

      error_ref = blk_err_ref_(coeff_, dqcoeff, block_size, &sse_ref);
      ASM_REGISTER_STATE_CHECK(error =
                                   blk_err_(coeff_, dqcoeff, block_size, &sse));

      EXPECT_EQ(error_ref, error) << "Error doesn't match on test: " << i;
      EXPECT_EQ(sse_ref, sse) << "SSE doesn't match on test: " << i;
    }
  }

  intptr_t getCoeffNum() { return tx_size_2d[tx_size_]; }

  void FillRandomData() {
    const intptr_t block_size = getCoeffNum();
    tran_low_t *dqcoeff = coeff_ + block_size;
    intptr_t i;
    int16_t margin = 512;
    for (i = 0; i < block_size; ++i) {
      coeff_[i] = GetRandomNumWithRange(INT16_MIN + margin, INT16_MAX - margin);
      dqcoeff[i] = coeff_[i] + GetRandomDeltaWithRange(margin);
    }
  }

  void FillConstantData() {
    const intptr_t block_size = getCoeffNum();
    tran_low_t *dqcoeff = coeff_ + block_size;
    intptr_t i;
    for (i = 0; i < block_size; ++i) {
      coeff_[i] = 5;
      dqcoeff[i] = 7;
    }
  }

  tran_low_t GetRandomNumWithRange(int16_t min, int16_t max) {
    return clamp((int16_t)rnd_.Rand16(), min, max);
  }

  tran_low_t GetRandomDeltaWithRange(int16_t delta) {
    tran_low_t value = (int16_t)rnd_.Rand16();
    value %= delta;
    return value;
  }

  BlockErrorFunc blk_err_ref_;
  BlockErrorFunc blk_err_;
  TX_SIZE tx_size_;
  aom_bit_depth_t bd_;
  ACMRandom rnd_;
  tran_low_t *coeff_;
};

TEST_P(BlockErrorTest, BitExact) { BlockErrorRun(kTestNum); }

using std::tr1::make_tuple;

#if !CONFIG_HIGHBITDEPTH && HAVE_SSE2
const BlockErrorParam kBlkErrParamArraySse2[] = { make_tuple(
    &av1_block_error_c, &av1_block_error_sse2, TX_32X32, AOM_BITS_8) };
INSTANTIATE_TEST_CASE_P(SSE2, BlockErrorTest,
                        ::testing::ValuesIn(kBlkErrParamArraySse2));
#endif

#if HAVE_AVX2
const BlockErrorParam kBlkErrParamArrayAvx2[] = { make_tuple(
    &av1_block_error_c, &av1_block_error_avx2, TX_32X32, AOM_BITS_8) };
INSTANTIATE_TEST_CASE_P(AVX2, BlockErrorTest,
                        ::testing::ValuesIn(kBlkErrParamArrayAvx2));
#endif
}  // namespace
