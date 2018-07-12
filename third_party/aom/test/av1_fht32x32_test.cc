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
using std::tr1::tuple;
using libaom_test::FhtFunc;
typedef tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t, int> Ht32x32Param;

void fht32x32_ref(const int16_t *in, tran_low_t *out, int stride,
                  TxfmParam *txfm_param) {
  av1_fht32x32_c(in, out, stride, txfm_param);
}

#if CONFIG_HIGHBITDEPTH
typedef void (*IHbdHtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                           TX_TYPE tx_type, int bd);
typedef void (*HbdHtFunc)(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd);

// Target optimized function, tx_type, bit depth
typedef tuple<HbdHtFunc, TX_TYPE, int> HighbdHt32x32Param;

void highbd_fht32x32_ref(const int16_t *in, int32_t *out, int stride,
                         TX_TYPE tx_type, int bd) {
  av1_fwd_txfm2d_32x32_c(in, out, stride, tx_type, bd);
}
#endif  // CONFIG_HIGHBITDEPTH

#if (HAVE_SSE2 || HAVE_AVX2) && !CONFIG_DAALA_DCT32
void dummy_inv_txfm(const tran_low_t *in, uint8_t *out, int stride,
                    const TxfmParam *txfm_param) {
  (void)in;
  (void)out;
  (void)stride;
  (void)txfm_param;
}
#endif

class AV1Trans32x32HT : public libaom_test::TransformTestBase,
                        public ::testing::TestWithParam<Ht32x32Param> {
 public:
  virtual ~AV1Trans32x32HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 32;
    height_ = 32;
    fwd_txfm_ref = fht32x32_ref;
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

TEST_P(AV1Trans32x32HT, CoeffCheck) { RunCoeffCheck(); }
TEST_P(AV1Trans32x32HT, MemCheck) { RunMemCheck(); }

#if CONFIG_HIGHBITDEPTH
class AV1HighbdTrans32x32HT
    : public ::testing::TestWithParam<HighbdHt32x32Param> {
 public:
  virtual ~AV1HighbdTrans32x32HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    fwd_txfm_ref_ = highbd_fht32x32_ref;
    tx_type_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(2);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = 1024;

    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(32, sizeof(int16_t) * num_coeffs_));
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(32, sizeof(int32_t) * num_coeffs_));
    output_ref_ = reinterpret_cast<int32_t *>(
        aom_memalign(32, sizeof(int32_t) * num_coeffs_));
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

void AV1HighbdTrans32x32HT::RunBitexactCheck() {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  int i, j;
  const int stride = 32;
  const int num_tests = 1000;

  for (i = 0; i < num_tests; ++i) {
    for (j = 0; j < num_coeffs_; ++j) {
      input_[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);
    }

    fwd_txfm_ref_(input_, output_ref_, stride, tx_type_, bit_depth_);
    ASM_REGISTER_STATE_CHECK(
        fwd_txfm_(input_, output_, stride, tx_type_, bit_depth_));

    for (j = 0; j < num_coeffs_; ++j) {
      EXPECT_EQ(output_ref_[j], output_[j])
          << "Not bit-exact result at index: " << j << " at test block: " << i;
    }
  }
}

TEST_P(AV1HighbdTrans32x32HT, HighbdCoeffCheck) { RunBitexactCheck(); }
#endif  // CONFIG_HIGHBITDEPTH

using std::tr1::make_tuple;

#if HAVE_SSE2 && !CONFIG_DAALA_DCT32
const Ht32x32Param kArrayHt32x32Param_sse2[] = {
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, DCT_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, ADST_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, DCT_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, ADST_ADST, AOM_BITS_8, 1024),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, FLIPADST_DCT, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, DCT_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, FLIPADST_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, ADST_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, FLIPADST_ADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, IDTX, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, V_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, H_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, V_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, H_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, V_FLIPADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_sse2, &dummy_inv_txfm, H_FLIPADST, AOM_BITS_8, 1024)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(SSE2, AV1Trans32x32HT,
                        ::testing::ValuesIn(kArrayHt32x32Param_sse2));
#endif  // HAVE_SSE2 && !CONFIG_DAALA_DCT32

#if HAVE_AVX2 && !CONFIG_DAALA_DCT32
const Ht32x32Param kArrayHt32x32Param_avx2[] = {
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, DCT_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, ADST_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, DCT_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, ADST_ADST, AOM_BITS_8, 1024),
#if CONFIG_EXT_TX
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, FLIPADST_DCT, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, DCT_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, FLIPADST_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, ADST_FLIPADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, FLIPADST_ADST, AOM_BITS_8,
             1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, IDTX, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, V_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, H_DCT, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, V_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, H_ADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, V_FLIPADST, AOM_BITS_8, 1024),
  make_tuple(&av1_fht32x32_avx2, &dummy_inv_txfm, H_FLIPADST, AOM_BITS_8, 1024)
#endif  // CONFIG_EXT_TX
};
INSTANTIATE_TEST_CASE_P(AVX2, AV1Trans32x32HT,
                        ::testing::ValuesIn(kArrayHt32x32Param_avx2));
#endif  // HAVE_AVX2 && !CONFIG_DAALA_DCT32
}  // namespace
