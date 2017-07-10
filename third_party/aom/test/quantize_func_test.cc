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
#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "aom/aom_codec.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/av1_quantize.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
using libaom_test::ACMRandom;

#define QUAN_PARAM_LIST                                                   \
  const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block,         \
      const int16_t *zbin_ptr, const int16_t *round_ptr,                  \
      const int16_t *quant_ptr, const int16_t *quant_shift_ptr,           \
      tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,                    \
      const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, \
      const int16_t *iscan

typedef void (*QuantizeFunc)(QUAN_PARAM_LIST);
typedef void (*QuantizeFuncHbd)(QUAN_PARAM_LIST, int log_scale);

#define HBD_QUAN_FUNC                                                      \
  fn(coeff_ptr, n_coeffs, skip_block, zbin_ptr, round_ptr, quant_ptr,      \
     quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, \
     iscan, log_scale)

template <QuantizeFuncHbd fn>
void highbd_quan16x16_wrapper(QUAN_PARAM_LIST) {
  const int log_scale = 0;
  HBD_QUAN_FUNC;
}

template <QuantizeFuncHbd fn>
void highbd_quan32x32_wrapper(QUAN_PARAM_LIST) {
  const int log_scale = 1;
  HBD_QUAN_FUNC;
}

template <QuantizeFuncHbd fn>
void highbd_quan64x64_wrapper(QUAN_PARAM_LIST) {
  const int log_scale = 2;
  HBD_QUAN_FUNC;
}

typedef enum { TYPE_B, TYPE_DC, TYPE_FP } QuantType;

typedef std::tr1::tuple<QuantizeFunc, QuantizeFunc, TX_SIZE, QuantType,
                        aom_bit_depth_t>
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
        type_(GET_PARAM(3)), bd_(GET_PARAM(4)) {}

  virtual ~QuantizeTest() {}

  virtual void SetUp() {
    qtab_ = reinterpret_cast<QuanTable *>(aom_memalign(32, sizeof(*qtab_)));
    const int n_coeffs = coeff_num();
    coeff_ = reinterpret_cast<tran_low_t *>(
        aom_memalign(32, 6 * n_coeffs * sizeof(tran_low_t)));
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

  void QuantizeRun(bool is_loop, int q = 0, int test_num = 1) {
    tran_low_t *coeff_ptr = coeff_;
    const intptr_t n_coeffs = coeff_num();
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

    const int16_t *round = 0;
    const int16_t *quant = 0;
    if (type_ == TYPE_B) {
      round = qtab_->quant.y_round[q];
      quant = qtab_->quant.y_quant[q];
    } else if (type_ == TYPE_FP) {
      round = qtab_->quant.y_round_fp[q];
      quant = qtab_->quant.y_quant_fp[q];
    }

    const int16_t *quant_shift = qtab_->quant.y_quant_shift[q];
    const int16_t *dequant = qtab_->dequant.y_dequant[q];

    for (int i = 0; i < test_num; ++i) {
      if (is_loop) FillCoeffRandom();

      memset(qcoeff_ref, 0, 5 * n_coeffs * sizeof(*qcoeff_ref));

      quant_ref_(coeff_ptr, n_coeffs, skip_block, zbin, round, quant,
                 quant_shift, qcoeff_ref, dqcoeff_ref, dequant, &eob[0],
                 sc->scan, sc->iscan);

      ASM_REGISTER_STATE_CHECK(quant_(
          coeff_ptr, n_coeffs, skip_block, zbin, round, quant, quant_shift,
          qcoeff, dqcoeff, dequant, &eob[1], sc->scan, sc->iscan));

      for (int j = 0; j < n_coeffs; ++j) {
        ASSERT_EQ(qcoeff_ref[j], qcoeff[j])
            << "Q mismatch on test: " << i << " at position: " << j
            << " Q: " << q << " coeff: " << coeff_ptr[j];
      }

      for (int j = 0; j < n_coeffs; ++j) {
        ASSERT_EQ(dqcoeff_ref[j], dqcoeff[j])
            << "Dq mismatch on test: " << i << " at position: " << j
            << " Q: " << q << " coeff: " << coeff_ptr[j];
      }

      ASSERT_EQ(eob[0], eob[1]) << "eobs mismatch on test: " << i
                                << " Q: " << q;
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

  int coeff_num() const { return tx_size_2d[tx_size_]; }

  void FillCoeff(tran_low_t c) {
    const int n_coeffs = coeff_num();
    for (int i = 0; i < n_coeffs; ++i) {
      coeff_[i] = c;
    }
  }

  void FillCoeffRandom() {
    const int n_coeffs = coeff_num();
    FillCoeffZero();
    int num = rnd_.Rand16() % n_coeffs;
    for (int i = 0; i < num; ++i) {
      coeff_[i] = GetRandomCoeff();
    }
  }

  void FillCoeffZero() { FillCoeff(0); }

  void FillCoeffConstant() {
    tran_low_t c = GetRandomCoeff();
    FillCoeff(c);
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

  tran_low_t GetRandomCoeff() {
    tran_low_t coeff;
    if (bd_ == AOM_BITS_8) {
      coeff =
          clamp(static_cast<int16_t>(rnd_.Rand16()), INT16_MIN + 1, INT16_MAX);
    } else {
      tran_low_t min = -(1 << (7 + bd_));
      tran_low_t max = -min - 1;
      coeff = clamp(static_cast<tran_low_t>(rnd_.Rand31()), min, max);
    }
    return coeff;
  }

  ACMRandom rnd_;
  QuanTable *qtab_;
  tran_low_t *coeff_;
  QuantizeFunc quant_ref_;
  QuantizeFunc quant_;
  TX_SIZE tx_size_;
  QuantType type_;
  aom_bit_depth_t bd_;
};

TEST_P(QuantizeTest, ZeroInput) {
  FillCoeffZero();
  QuantizeRun(false);
}

TEST_P(QuantizeTest, LargeNegativeInput) {
  FillDcLargeNegative();
  QuantizeRun(false, 0, 1);
}

TEST_P(QuantizeTest, DcOnlyInput) {
  FillDcOnly();
  QuantizeRun(false, 0, 1);
}

TEST_P(QuantizeTest, RandomInput) { QuantizeRun(true, 0, kTestNum); }

TEST_P(QuantizeTest, MultipleQ) {
  for (int q = 0; q < QINDEX_RANGE; ++q) {
    QuantizeRun(true, q, kTestNum);
  }
}

TEST_P(QuantizeTest, DISABLED_Speed) {
  tran_low_t *coeff_ptr = coeff_;
  const intptr_t n_coeffs = coeff_num();
  const int skip_block = 0;

  tran_low_t *qcoeff_ref = coeff_ptr + n_coeffs;
  tran_low_t *dqcoeff_ref = qcoeff_ref + n_coeffs;

  tran_low_t *qcoeff = dqcoeff_ref + n_coeffs;
  tran_low_t *dqcoeff = qcoeff + n_coeffs;
  uint16_t *eob = (uint16_t *)(dqcoeff + n_coeffs);

  // Testing uses 2-D DCT scan order table
  const SCAN_ORDER *const sc = get_default_scan(tx_size_, DCT_DCT, 0);

  // Testing uses luminance quantization table
  const int q = 22;
  const int16_t *zbin = qtab_->quant.y_zbin[q];
  const int16_t *round_fp = qtab_->quant.y_round_fp[q];
  const int16_t *quant_fp = qtab_->quant.y_quant_fp[q];
  const int16_t *quant_shift = qtab_->quant.y_quant_shift[q];
  const int16_t *dequant = qtab_->dequant.y_dequant[q];
  const int kNumTests = 5000000;
  aom_usec_timer timer;

  FillCoeffRandom();

  aom_usec_timer_start(&timer);
  for (int n = 0; n < kNumTests; ++n) {
    quant_(coeff_ptr, n_coeffs, skip_block, zbin, round_fp, quant_fp,
           quant_shift, qcoeff, dqcoeff, dequant, eob, sc->scan, sc->iscan);
  }
  aom_usec_timer_mark(&timer);

  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("Elapsed time: %d us\n", elapsed_time);
}

using std::tr1::make_tuple;

#if HAVE_AVX2
const QuantizeParam kQParamArrayAvx2[] = {
  make_tuple(&av1_quantize_fp_c, &av1_quantize_fp_avx2, TX_16X16, TYPE_FP,
             AOM_BITS_8),
  make_tuple(&av1_quantize_fp_32x32_c, &av1_quantize_fp_32x32_avx2, TX_32X32,
             TYPE_FP, AOM_BITS_8),
#if CONFIG_HIGHBITDEPTH
  make_tuple(&highbd_quan16x16_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan16x16_wrapper<av1_highbd_quantize_fp_avx2>, TX_16X16,
             TYPE_FP, AOM_BITS_8),
  make_tuple(&highbd_quan16x16_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan16x16_wrapper<av1_highbd_quantize_fp_avx2>, TX_16X16,
             TYPE_FP, AOM_BITS_10),
  make_tuple(&highbd_quan16x16_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan16x16_wrapper<av1_highbd_quantize_fp_avx2>, TX_16X16,
             TYPE_FP, AOM_BITS_12),
  make_tuple(&highbd_quan32x32_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan32x32_wrapper<av1_highbd_quantize_fp_avx2>, TX_32X32,
             TYPE_FP, AOM_BITS_8),
  make_tuple(&highbd_quan32x32_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan32x32_wrapper<av1_highbd_quantize_fp_avx2>, TX_32X32,
             TYPE_FP, AOM_BITS_10),
  make_tuple(&highbd_quan32x32_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan32x32_wrapper<av1_highbd_quantize_fp_avx2>, TX_32X32,
             TYPE_FP, AOM_BITS_12),
#if CONFIG_TX64X64
  make_tuple(&highbd_quan64x64_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan64x64_wrapper<av1_highbd_quantize_fp_avx2>, TX_64X64,
             TYPE_FP, AOM_BITS_8),
  make_tuple(&highbd_quan64x64_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan64x64_wrapper<av1_highbd_quantize_fp_avx2>, TX_64X64,
             TYPE_FP, AOM_BITS_10),
  make_tuple(&highbd_quan64x64_wrapper<av1_highbd_quantize_fp_c>,
             &highbd_quan64x64_wrapper<av1_highbd_quantize_fp_avx2>, TX_64X64,
             TYPE_FP, AOM_BITS_12),
#endif  // CONFIG_TX64X64
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_avx2, TX_16X16,
             TYPE_B, AOM_BITS_8),
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_avx2, TX_16X16,
             TYPE_B, AOM_BITS_10),
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_avx2, TX_16X16,
             TYPE_B, AOM_BITS_12),
#endif  // CONFIG_HIGHBITDEPTH
};

INSTANTIATE_TEST_CASE_P(AVX2, QuantizeTest,
                        ::testing::ValuesIn(kQParamArrayAvx2));
#endif  // HAVE_AVX2

#if HAVE_SSE2
const QuantizeParam kQParamArraySSE2[] = {
  make_tuple(&av1_quantize_fp_c, &av1_quantize_fp_sse2, TX_16X16, TYPE_FP,
             AOM_BITS_8),
#if CONFIG_HIGHBITDEPTH
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_sse2, TX_16X16,
             TYPE_B, AOM_BITS_8),
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_sse2, TX_16X16,
             TYPE_B, AOM_BITS_10),
  make_tuple(&aom_highbd_quantize_b_c, &aom_highbd_quantize_b_sse2, TX_16X16,
             TYPE_B, AOM_BITS_12),
  make_tuple(&aom_highbd_quantize_b_32x32_c, &aom_highbd_quantize_b_32x32_sse2,
             TX_32X32, TYPE_B, AOM_BITS_8),
  make_tuple(&aom_highbd_quantize_b_32x32_c, &aom_highbd_quantize_b_32x32_sse2,
             TX_32X32, TYPE_B, AOM_BITS_10),
  make_tuple(&aom_highbd_quantize_b_32x32_c, &aom_highbd_quantize_b_32x32_sse2,
             TX_32X32, TYPE_B, AOM_BITS_12),
#endif
};

INSTANTIATE_TEST_CASE_P(SSE2, QuantizeTest,
                        ::testing::ValuesIn(kQParamArraySSE2));
#endif

#if !CONFIG_HIGHBITDEPTH && HAVE_SSSE3 && ARCH_X86_64
const QuantizeParam kQ16x16ParamArraySSSE3[] = {
  make_tuple(&av1_quantize_fp_c, &av1_quantize_fp_ssse3, TX_16X16, TYPE_FP,
             AOM_BITS_8),
};
INSTANTIATE_TEST_CASE_P(SSSE3, QuantizeTest,
                        ::testing::ValuesIn(kQ16x16ParamArraySSSE3));

// TODO(any):
//  The following test does not pass yet
const QuantizeParam kQ32x32ParamArraySSSE3[] = { make_tuple(
    &av1_quantize_fp_32x32_c, &av1_quantize_fp_32x32_ssse3, TX_32X32, TYPE_FP,
    AOM_BITS_8) };
INSTANTIATE_TEST_CASE_P(DISABLED_SSSE3, QuantizeTest,
                        ::testing::ValuesIn(kQ32x32ParamArraySSSE3));
#endif

}  // namespace
