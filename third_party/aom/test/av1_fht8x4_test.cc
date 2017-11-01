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

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"

#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/transform_test_base.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);
using std::tr1::tuple;
using libaom_test::FhtFunc;
typedef tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t, int> Ht8x4Param;

void fht8x4_ref(const int16_t *in, tran_low_t *out, int stride,
                TxfmParam *txfm_param) {
  av1_fht8x4_c(in, out, stride, txfm_param);
}

void iht8x4_ref(const tran_low_t *in, uint8_t *out, int stride,
                const TxfmParam *txfm_param) {
  av1_iht8x4_32_add_c(in, out, stride, txfm_param);
}

class AV1Trans8x4HT : public libaom_test::TransformTestBase,
                      public ::testing::TestWithParam<Ht8x4Param> {
 public:
  virtual ~AV1Trans8x4HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 8;
    height_ = 4;
    fwd_txfm_ref = fht8x4_ref;
    inv_txfm_ref = iht8x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = GET_PARAM(4);
    txfm_param_.tx_type = GET_PARAM(2);
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride, &txfm_param_);
  }

  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, &txfm_param_);
  }

  FhtFunc fwd_txfm_;
  IhtFunc inv_txfm_;
};

TEST_P(AV1Trans8x4HT, AccuracyCheck) { RunAccuracyCheck(0, 0.00001); }
TEST_P(AV1Trans8x4HT, CoeffCheck) { RunCoeffCheck(); }
TEST_P(AV1Trans8x4HT, MemCheck) { RunMemCheck(); }
TEST_P(AV1Trans8x4HT, InvCoeffCheck) { RunInvCoeffCheck(); }
TEST_P(AV1Trans8x4HT, InvAccuracyCheck) { RunInvAccuracyCheck(0); }

using std::tr1::make_tuple;

const Ht8x4Param kArrayHt8x4Param_c[] = {
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, DCT_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, ADST_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, DCT_ADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, ADST_ADST, AOM_BITS_8, 32),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, FLIPADST_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, DCT_FLIPADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, FLIPADST_FLIPADST, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, ADST_FLIPADST, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, FLIPADST_ADST, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, IDTX, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, V_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, H_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, V_ADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, H_ADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, V_FLIPADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_c, &av1_iht8x4_32_add_c, H_FLIPADST, AOM_BITS_8, 32)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(C, AV1Trans8x4HT,
                        ::testing::ValuesIn(kArrayHt8x4Param_c));

#if HAVE_SSE2
const Ht8x4Param kArrayHt8x4Param_sse2[] = {
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, DCT_DCT, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, ADST_DCT, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, DCT_ADST, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, ADST_ADST, AOM_BITS_8,
             32),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, FLIPADST_DCT,
             AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, DCT_FLIPADST,
             AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, FLIPADST_FLIPADST,
             AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, ADST_FLIPADST,
             AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, FLIPADST_ADST,
             AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, IDTX, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, V_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, H_DCT, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, V_ADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, H_ADST, AOM_BITS_8, 32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, V_FLIPADST, AOM_BITS_8,
             32),
  make_tuple(&av1_fht8x4_sse2, &av1_iht8x4_32_add_sse2, H_FLIPADST, AOM_BITS_8,
             32)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(SSE2, AV1Trans8x4HT,
                        ::testing::ValuesIn(kArrayHt8x4Param_sse2));
#endif  // HAVE_SSE2

}  // namespace
