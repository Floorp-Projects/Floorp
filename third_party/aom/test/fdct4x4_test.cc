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
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/transform_test_base.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

using libaom_test::ACMRandom;

namespace {
typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);
using libaom_test::FhtFunc;

typedef std::tr1::tuple<FdctFunc, IdctFunc, TX_TYPE, aom_bit_depth_t, int>
    Dct4x4Param;
typedef std::tr1::tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t, int>
    Ht4x4Param;

void fdct4x4_ref(const int16_t *in, tran_low_t *out, int stride,
                 TxfmParam * /*txfm_param*/) {
  aom_fdct4x4_c(in, out, stride);
}

void fht4x4_ref(const int16_t *in, tran_low_t *out, int stride,
                TxfmParam *txfm_param) {
  av1_fht4x4_c(in, out, stride, txfm_param);
}

void fwht4x4_ref(const int16_t *in, tran_low_t *out, int stride,
                 TxfmParam * /*txfm_param*/) {
  av1_fwht4x4_c(in, out, stride);
}

#if CONFIG_HIGHBITDEPTH
void fht4x4_10(const int16_t *in, tran_low_t *out, int stride,
               TxfmParam *txfm_param) {
  av1_fwd_txfm2d_4x4_c(in, out, stride, txfm_param->tx_type, 10);
}

void fht4x4_12(const int16_t *in, tran_low_t *out, int stride,
               TxfmParam *txfm_param) {
  av1_fwd_txfm2d_4x4_c(in, out, stride, txfm_param->tx_type, 12);
}

void iht4x4_10(const tran_low_t *in, uint8_t *out, int stride,
               const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_4x4_c(in, CONVERT_TO_SHORTPTR(out), stride,
                           txfm_param->tx_type, 10);
}

void iht4x4_12(const tran_low_t *in, uint8_t *out, int stride,
               const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_4x4_c(in, CONVERT_TO_SHORTPTR(out), stride,
                           txfm_param->tx_type, 12);
}

void iwht4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_iwht4x4_16_add_c(in, out, stride, 10);
}

void iwht4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  aom_highbd_iwht4x4_16_add_c(in, out, stride, 12);
}
#endif  // CONFIG_HIGHBITDEPTH

class Trans4x4DCT : public libaom_test::TransformTestBase,
                    public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 4;
    height_ = 4;
    fwd_txfm_ref = fdct4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = GET_PARAM(4);
    txfm_param_.tx_type = GET_PARAM(2);
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4DCT, AccuracyCheck) { RunAccuracyCheck(0, 0.00001); }

TEST_P(Trans4x4DCT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4DCT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4DCT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

class Trans4x4HT : public libaom_test::TransformTestBase,
                   public ::testing::TestWithParam<Ht4x4Param> {
 public:
  virtual ~Trans4x4HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 4;
    height_ = 4;
    fwd_txfm_ref = fht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = GET_PARAM(4);
    txfm_param_.tx_type = GET_PARAM(2);
#if CONFIG_HIGHBITDEPTH
    switch (bit_depth_) {
      case AOM_BITS_10: fwd_txfm_ref = fht4x4_10; break;
      case AOM_BITS_12: fwd_txfm_ref = fht4x4_12; break;
      default: fwd_txfm_ref = fht4x4_ref; break;
    }
#endif
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

TEST_P(Trans4x4HT, AccuracyCheck) { RunAccuracyCheck(1, 0.005); }

TEST_P(Trans4x4HT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4HT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4HT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

class Trans4x4WHT : public libaom_test::TransformTestBase,
                    public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4WHT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 4;
    height_ = 4;
    fwd_txfm_ref = fwht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    num_coeffs_ = GET_PARAM(4);
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4WHT, AccuracyCheck) { RunAccuracyCheck(0, 0.00001); }

TEST_P(Trans4x4WHT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans4x4WHT, MemCheck) { RunMemCheck(); }

TEST_P(Trans4x4WHT, InvAccuracyCheck) { RunInvAccuracyCheck(0); }
using std::tr1::make_tuple;

INSTANTIATE_TEST_CASE_P(C, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_c,
                                                     &aom_idct4x4_16_add_c,
                                                     DCT_DCT, AOM_BITS_8, 16)));

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    DISABLED_C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&fht4x4_12, &iht4x4_12, DCT_DCT, AOM_BITS_12, 16),
        make_tuple(&fht4x4_12, &iht4x4_12, ADST_DCT, AOM_BITS_12, 16),
        make_tuple(&fht4x4_12, &iht4x4_12, DCT_ADST, AOM_BITS_12, 16),
        make_tuple(&fht4x4_12, &iht4x4_12, ADST_ADST, AOM_BITS_12, 16)));

INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&fht4x4_10, &iht4x4_10, DCT_DCT, AOM_BITS_10, 16),
        make_tuple(&fht4x4_10, &iht4x4_10, ADST_DCT, AOM_BITS_10, 16),
        make_tuple(&fht4x4_10, &iht4x4_10, DCT_ADST, AOM_BITS_10, 16),
        make_tuple(&fht4x4_10, &iht4x4_10, ADST_ADST, AOM_BITS_10, 16),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, DCT_DCT, AOM_BITS_8,
                   16),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, ADST_DCT, AOM_BITS_8,
                   16),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, DCT_ADST, AOM_BITS_8,
                   16),
        make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, ADST_ADST, AOM_BITS_8,
                   16)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, DCT_DCT,
                                 AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, ADST_DCT,
                                 AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, DCT_ADST,
                                 AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_c, ADST_ADST,
                                 AOM_BITS_8, 16)));
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4WHT,
    ::testing::Values(make_tuple(&av1_highbd_fwht4x4_c, &iwht4x4_10, DCT_DCT,
                                 AOM_BITS_10, 16),
                      make_tuple(&av1_highbd_fwht4x4_c, &iwht4x4_12, DCT_DCT,
                                 AOM_BITS_12, 16),
                      make_tuple(&av1_fwht4x4_c, &aom_iwht4x4_16_add_c, DCT_DCT,
                                 AOM_BITS_8, 16)));
#else
INSTANTIATE_TEST_CASE_P(C, Trans4x4WHT,
                        ::testing::Values(make_tuple(&av1_fwht4x4_c,
                                                     &aom_iwht4x4_16_add_c,
                                                     DCT_DCT, AOM_BITS_8, 16)));
#endif  // CONFIG_HIGHBITDEPTH

#if HAVE_NEON_ASM && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(NEON, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_c,
                                                     &aom_idct4x4_16_add_neon,
                                                     DCT_DCT, AOM_BITS_8, 16)));
#endif  // HAVE_NEON_ASM && !CONFIG_HIGHBITDEPTH

#if HAVE_NEON && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    NEON, Trans4x4HT,
    ::testing::Values(make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon,
                                 DCT_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon,
                                 ADST_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon,
                                 DCT_ADST, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_c, &av1_iht4x4_16_add_neon,
                                 ADST_ADST, AOM_BITS_8, 16)));
#endif  // HAVE_NEON && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2 && !CONFIG_DAALA_DCT4
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4WHT,
    ::testing::Values(make_tuple(&av1_fwht4x4_c, &aom_iwht4x4_16_add_c, DCT_DCT,
                                 AOM_BITS_8, 16),
                      make_tuple(&av1_fwht4x4_c, &aom_iwht4x4_16_add_sse2,
                                 DCT_DCT, AOM_BITS_8, 16)));
#endif

#if HAVE_SSE2 && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_sse2,
                                                     &aom_idct4x4_16_add_sse2,
                                                     DCT_DCT, AOM_BITS_8, 16)));
#if !CONFIG_DAALA_DCT4
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2,
                                 DCT_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2,
                                 ADST_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2,
                                 DCT_ADST, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_sse2,
                                 ADST_ADST, AOM_BITS_8, 16)));
#endif  // !CONFIG_DAALA_DCT4
#endif  // HAVE_SSE2 && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2 && CONFIG_HIGHBITDEPTH && !CONFIG_DAALA_DCT4
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c,
                                 DCT_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c,
                                 ADST_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c,
                                 DCT_ADST, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_sse2, &av1_iht4x4_16_add_c,
                                 ADST_ADST, AOM_BITS_8, 16)));
#endif  // HAVE_SSE2 && CONFIG_HIGHBITDEPTH && !CONFIG_DAALA_DCT4

#if HAVE_MSA && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(MSA, Trans4x4DCT,
                        ::testing::Values(make_tuple(&aom_fdct4x4_msa,
                                                     &aom_idct4x4_16_add_msa,
                                                     DCT_DCT, AOM_BITS_8, 16)));
#if !CONFIG_EXT_TX && !CONFIG_DAALA_DCT4
INSTANTIATE_TEST_CASE_P(
    MSA, Trans4x4HT,
    ::testing::Values(make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa,
                                 DCT_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa,
                                 ADST_DCT, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa,
                                 DCT_ADST, AOM_BITS_8, 16),
                      make_tuple(&av1_fht4x4_msa, &av1_iht4x4_16_add_msa,
                                 ADST_ADST, AOM_BITS_8, 16)));
#endif  // !CONFIG_EXT_TX && && !CONFIG_DAALA_DCT4
#endif  // HAVE_MSA && !CONFIG_HIGHBITDEPTH
}  // namespace
