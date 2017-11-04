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

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/transform_test_base.h"
#include "test/util.h"
#include "aom_ports/mem.h"

using libaom_test::ACMRandom;

namespace {
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);

using libaom_test::FhtFunc;
using std::tr1::tuple;
typedef tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t, int> Ht8x8Param;

void fht8x8_ref(const int16_t *in, tran_low_t *out, int stride,
                TxfmParam *txfm_param) {
  av1_fht8x8_c(in, out, stride, txfm_param);
}

void iht8x8_ref(const tran_low_t *in, uint8_t *out, int stride,
                const TxfmParam *txfm_param) {
  av1_iht8x8_64_add_c(in, out, stride, txfm_param);
}

#if CONFIG_HIGHBITDEPTH
typedef void (*IHbdHtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                           TX_TYPE tx_type, int bd);
typedef void (*HbdHtFunc)(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd);
// Target optimized function, tx_type, bit depth
typedef tuple<HbdHtFunc, TX_TYPE, int> HighbdHt8x8Param;

void highbd_fht8x8_ref(const int16_t *in, int32_t *out, int stride,
                       TX_TYPE tx_type, int bd) {
  av1_fwd_txfm2d_8x8_c(in, out, stride, tx_type, bd);
}
#endif  // CONFIG_HIGHBITDEPTH

class AV1Trans8x8HT : public libaom_test::TransformTestBase,
                      public ::testing::TestWithParam<Ht8x8Param> {
 public:
  virtual ~AV1Trans8x8HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 8;
    height_ = 8;
    fwd_txfm_ref = fht8x8_ref;
    inv_txfm_ref = iht8x8_ref;
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

TEST_P(AV1Trans8x8HT, MemCheck) { RunMemCheck(); }
TEST_P(AV1Trans8x8HT, CoeffCheck) { RunCoeffCheck(); }
// Note:
//  TODO(luoyi): Add tx_type, 9-15 for inverse transform.
//  Need cleanup since same tests may be done in fdct8x8_test.cc
// TEST_P(AV1Trans8x8HT, AccuracyCheck) { RunAccuracyCheck(0); }
// TEST_P(AV1Trans8x8HT, InvAccuracyCheck) { RunInvAccuracyCheck(0); }
// TEST_P(AV1Trans8x8HT, InvCoeffCheck) { RunInvCoeffCheck(); }

#if CONFIG_HIGHBITDEPTH
class AV1HighbdTrans8x8HT : public ::testing::TestWithParam<HighbdHt8x8Param> {
 public:
  virtual ~AV1HighbdTrans8x8HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    fwd_txfm_ref_ = highbd_fht8x8_ref;
    tx_type_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = 64;

    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(int16_t) * num_coeffs_));
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(int32_t) * num_coeffs_));
    output_ref_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(int32_t) * num_coeffs_));
  }

  virtual void TearDown() {
    aom_free(input_);
    aom_free(output_);
    aom_free(output_ref_);
    libaom_test::ClearSystemState();
  }

 protected:
  void RunBitexactCheck();

 private:
  HbdHtFunc fwd_txfm_;
  HbdHtFunc fwd_txfm_ref_;
  TX_TYPE tx_type_;
  int bit_depth_;
  int mask_;
  int num_coeffs_;
  int16_t *input_;
  int32_t *output_;
  int32_t *output_ref_;
};

void AV1HighbdTrans8x8HT::RunBitexactCheck() {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int i, j;
  const int stride = 8;
  const int num_tests = 1000;
  const int num_coeffs = 64;

  for (i = 0; i < num_tests; ++i) {
    for (j = 0; j < num_coeffs; ++j) {
      input_[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);
    }

    fwd_txfm_ref_(input_, output_ref_, stride, tx_type_, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        fwd_txfm_(input_, output_, stride, tx_type_, bit_depth_));

    for (j = 0; j < num_coeffs; ++j) {
      EXPECT_EQ(output_ref_[j], output_[j])
          << "Not bit-exact result at index: " << j << " at test block: " << i;
    }
  }
}

TEST_P(AV1HighbdTrans8x8HT, HighbdCoeffCheck) { RunBitexactCheck(); }
#endif  // CONFIG_HIGHBITDEPTH

using std::tr1::make_tuple;

#if HAVE_SSE2 && !CONFIG_DAALA_DCT8
const Ht8x8Param kArrayHt8x8Param_sse2[] = {
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, DCT_DCT, AOM_BITS_8,
             64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, ADST_DCT, AOM_BITS_8,
             64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, DCT_ADST, AOM_BITS_8,
             64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, ADST_ADST, AOM_BITS_8,
             64),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, FLIPADST_DCT,
             AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, DCT_FLIPADST,
             AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, FLIPADST_FLIPADST,
             AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, ADST_FLIPADST,
             AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, FLIPADST_ADST,
             AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, IDTX, AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, V_DCT, AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, H_DCT, AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, V_ADST, AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, H_ADST, AOM_BITS_8, 64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, V_FLIPADST, AOM_BITS_8,
             64),
  make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2, H_FLIPADST, AOM_BITS_8,
             64)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(SSE2, AV1Trans8x8HT,
                        ::testing::ValuesIn(kArrayHt8x8Param_sse2));
#endif  // HAVE_SSE2

#if HAVE_SSE4_1 && CONFIG_HIGHBITDEPTH && !CONFIG_DAALA_DCT8
const HighbdHt8x8Param kArrayHBDHt8x8Param_sse4_1[] = {
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_DCT, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_DCT, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_DCT, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_DCT, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_ADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_ADST, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_ADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_ADST, 12),
#if CONFIG_EXT_TX
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_DCT, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_DCT, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_FLIPADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, DCT_FLIPADST, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_FLIPADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_FLIPADST, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_FLIPADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, ADST_FLIPADST, 12),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_ADST, 10),
  make_tuple(&av1_fwd_txfm2d_8x8_sse4_1, FLIPADST_ADST, 12),
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(SSE4_1, AV1HighbdTrans8x8HT,
                        ::testing::ValuesIn(kArrayHBDHt8x8Param_sse4_1));
#endif  // HAVE_SSE4_1 && CONFIG_HIGHBITDEPTH && !CONFIG_DAALA_DCT8

}  // namespace
