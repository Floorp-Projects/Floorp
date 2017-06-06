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
#include "aom/aom_codec.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/av1_quantize.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using libaom_test::ACMRandom;

#if !CONFIG_AOM_QM
typedef void (*QuantizeFunc)(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                             int skip_block, const int16_t *zbin_ptr,
                             const int16_t *round_ptr, const int16_t *quant_ptr,
                             const int16_t *quant_shift_ptr,
                             tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                             const int16_t *dequant_ptr, uint16_t *eob_ptr,
                             const int16_t *scan, const int16_t *iscan);
#else
typedef void (*QuantizeFunc)(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                             int skip_block, const int16_t *zbin_ptr,
                             const int16_t *round_ptr, const int16_t *quant_ptr,
                             const int16_t *quant_shift_ptr,
                             tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                             const int16_t *dequant_ptr, uint16_t *eob_ptr,
                             const int16_t *scan, const int16_t *iscan,
                             const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr);
#endif

typedef std::tr1::tuple<QuantizeFunc, QuantizeFunc, TX_SIZE, aom_bit_depth_t>
    QuantizeParam;

typedef struct {
  QUANTS quant;
  Dequants dequant;
} QuanTable;

const int kTestNum = 1000;

class QuantizeTest : public ::testing::TestWithParam<QuantizeParam> {
 protected:
  QuantizeTest()
      : quant_ref_(GET_PARAM(0)), quant_(GET_PARAM(1)), tx_size_(GET_PARAM(2)),
        bd_(GET_PARAM(3)) {}

  virtual ~QuantizeTest() {}

  virtual void SetUp() {
    qtab_ = reinterpret_cast<QuanTable *>(aom_memalign(16, sizeof(*qtab_)));
    const int n_coeffs = getCoeffNum();
    coeff_ = reinterpret_cast<tran_low_t *>(
        aom_memalign(16, 6 * n_coeffs * sizeof(tran_low_t)));
    InitQuantizer();
  }

  virtual void TearDown() {
    aom_free(qtab_);
    qtab_ = NULL;
    aom_free(coeff_);
    coeff_ = NULL;
    libaom_test::ClearSystemState();
  }

  void InitQuantizer() {
    av1_build_quantizer(bd_, 0, 0, 0, &qtab_->quant, &qtab_->dequant);
  }

  void QuantizeRun(bool isLoop, int q = 0, int testNum = 1) {
    tran_low_t *coeff_ptr = coeff_;
    const intptr_t n_coeffs = getCoeffNum();
    const int skip_block = 0;

    tran_low_t *qcoeff_ref = coeff_ptr + n_coeffs;
    tran_low_t *dqcoeff_ref = qcoeff_ref + n_coeffs;

    tran_low_t *qcoeff = dqcoeff_ref + n_coeffs;
    tran_low_t *dqcoeff = qcoeff + n_coeffs;
    uint16_t *eob = (uint16_t *)(dqcoeff + n_coeffs);

    // Testing uses 2-D DCT scan order table
    const SCAN_ORDER *const sc = get_default_scan(tx_size_, DCT_DCT, 0);

    // Testing uses luminance quantization table
    const int16_t *zbin = qtab_->quant.y_zbin[q];
    const int16_t *round_fp = qtab_->quant.y_round_fp[q];
    const int16_t *quant_fp = qtab_->quant.y_quant_fp[q];
    const int16_t *quant_shift = qtab_->quant.y_quant_shift[q];
    const int16_t *dequant = qtab_->dequant.y_dequant[q];
    const size_t bufferSize = n_coeffs;

    int i = 0;
    while (i < testNum) {
      if (isLoop) FillCoeffRandom();

      memset(qcoeff_ref, 0, 5 * n_coeffs * sizeof(*qcoeff_ref));

      quant_ref_(coeff_ptr, n_coeffs, skip_block, zbin, round_fp, quant_fp,
                 quant_shift, qcoeff_ref, dqcoeff_ref, dequant, &eob[0],
                 sc->scan, sc->iscan);

      ASM_REGISTER_STATE_CHECK(quant_(
          coeff_ptr, n_coeffs, skip_block, zbin, round_fp, quant_fp,
          quant_shift, qcoeff, dqcoeff, dequant, &eob[1], sc->scan, sc->iscan));

      CompareResults(qcoeff_ref, qcoeff, bufferSize, "Qcoeff", q, i);
      CompareResults(dqcoeff_ref, dqcoeff, bufferSize, "Dqcoeff", q, i);
      ASSERT_EQ(eob[0], eob[1]) << "eobs mismatch on test: " << i;

      i++;
    }
  }

  void CompareResults(const tran_low_t *buf_ref, const tran_low_t *buf,
                      int size, const char *text, int q, int number) {
    int i;
    for (i = 0; i < size; ++i) {
      ASSERT_EQ(buf_ref[i], buf[i]) << text << " mismatch on test: " << number
                                    << " at position: " << i << " Q: " << q;
    }
  }

  int getCoeffNum() { return tx_size_2d[tx_size_]; }

  void FillCoeffGeneric(bool isConstant, tran_low_t c = 0) {
    const int n_coeffs = getCoeffNum();
    int i;
    if (isConstant) {
      for (i = 0; i < n_coeffs; ++i) {
        coeff_[i] = c;
      }
    } else {
      FillCoeffZero();
      int num = rnd_.Rand16() % n_coeffs;
      for (i = 0; i < num; ++i) {
        coeff_[i] = GetRandomCoeff();
      }
    }
  }

  void FillCoeffZero() { FillCoeffGeneric(true); }

  void FillCoeffConstant() {
    tran_low_t c = GetRandomCoeff();
    FillCoeffGeneric(true, c);
  }

  void FillDcOnly() {
    FillCoeffZero();
    coeff_[0] = GetRandomCoeff();
  }

  void FillDcLargeNegative() {
    FillCoeffZero();
    // Generate a qcoeff which contains 512/-512 (0x0100/0xFE00) to catch issues
    // like BUG=883 where the constant being compared was incorrectly
    // initialized.
    coeff_[0] = -8191;
  }

  void FillCoeffRandom() { FillCoeffGeneric(false); }

  tran_low_t GetRandomCoeff() {
    return clamp((int16_t)rnd_.Rand16(), INT16_MIN + 1, INT16_MAX);
  }

  ACMRandom rnd_;
  QuanTable *qtab_;
  tran_low_t *coeff_;
  QuantizeFunc quant_ref_;
  QuantizeFunc quant_;
  TX_SIZE tx_size_;
  aom_bit_depth_t bd_;
};

TEST_P(QuantizeTest, ZeroInput) {
  FillCoeffZero();
  QuantizeRun(false);
}

TEST_P(QuantizeTest, LargeNegativeInput) {
  FillDcLargeNegative();
  QuantizeRun(false);
}

TEST_P(QuantizeTest, DcOnlyInput) {
  FillDcOnly();
  QuantizeRun(false);
}

TEST_P(QuantizeTest, RandomInput) { QuantizeRun(true, 0, kTestNum); }

TEST_P(QuantizeTest, MultipleQ) {
  for (int q = 0; q < QINDEX_RANGE; ++q) {
    QuantizeRun(true, q, kTestNum);
  }
}

using std::tr1::make_tuple;

#if HAVE_SSE2
const QuantizeParam kQParamArraySSE2[] = { make_tuple(
    &av1_quantize_fp_c, &av1_quantize_fp_sse2, TX_16X16, AOM_BITS_8) };

INSTANTIATE_TEST_CASE_P(SSE2, QuantizeTest,
                        ::testing::ValuesIn(kQParamArraySSE2));
#endif

#if !CONFIG_HIGHBITDEPTH && HAVE_SSSE3 && ARCH_X86_64
const QuantizeParam kQParamArraySSSE3[] = {
  make_tuple(&av1_quantize_fp_c, &av1_quantize_fp_ssse3, TX_16X16, AOM_BITS_8),
  // TODO(any):
  //  The following test couldn't pass yet
  // make_tuple(av1_quantize_fp_c, av1_quantize_fp_32x32_ssse3, TX_32X32,
  // AOM_BITS_8)
};
INSTANTIATE_TEST_CASE_P(SSSE3, QuantizeTest,
                        ::testing::ValuesIn(kQParamArraySSSE3));
#endif

}  // namespace
