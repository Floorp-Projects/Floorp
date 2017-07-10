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

#include "./av1_rtcd.h"

#include "aom_ports/mem.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/transform_test_base.h"
#include "test/util.h"

#if CONFIG_TX64X64

using libaom_test::ACMRandom;

namespace {
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);
using std::tr1::tuple;
using libaom_test::FhtFunc;
typedef tuple<FhtFunc, IhtFunc, int, aom_bit_depth_t, int> Ht64x64Param;

void fht64x64_ref(const int16_t *in, tran_low_t *out, int stride,
                  TxfmParam *txfm_param) {
  av1_fht64x64_c(in, out, stride, txfm_param);
}

void iht64x64_ref(const tran_low_t *in, uint8_t *dest, int stride,
                  const TxfmParam *txfm_param) {
  av1_iht64x64_4096_add_c(in, dest, stride, txfm_param);
}

class AV1Trans64x64HT : public libaom_test::TransformTestBase,
                        public ::testing::TestWithParam<Ht64x64Param> {
 public:
  virtual ~AV1Trans64x64HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 64;
    height_ = 64;
    fwd_txfm_ref = fht64x64_ref;
    inv_txfm_ref = iht64x64_ref;
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

TEST_P(AV1Trans64x64HT, AccuracyCheck) { RunAccuracyCheck(4, 0.2); }
TEST_P(AV1Trans64x64HT, CoeffCheck) { RunCoeffCheck(); }
TEST_P(AV1Trans64x64HT, MemCheck) { RunMemCheck(); }
TEST_P(AV1Trans64x64HT, InvCoeffCheck) { RunInvCoeffCheck(); }
TEST_P(AV1Trans64x64HT, InvAccuracyCheck) { RunInvAccuracyCheck(4); }

using std::tr1::make_tuple;

const Ht64x64Param kArrayHt64x64Param_c[] = {
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 0, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 1, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 2, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 3, AOM_BITS_8, 4096),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 4, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 5, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 6, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 7, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 8, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 9, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 10, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 11, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 12, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 13, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 14, AOM_BITS_8, 4096),
  make_tuple(&av1_fht64x64_c, &av1_iht64x64_4096_add_c, 15, AOM_BITS_8, 4096)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(C, AV1Trans64x64HT,
                        ::testing::ValuesIn(kArrayHt64x64Param_c));

}  // namespace

#endif  // CONFIG_TX64X64
