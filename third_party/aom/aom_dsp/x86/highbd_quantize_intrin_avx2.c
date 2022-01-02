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

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

static INLINE void init_one_qp(const __m128i *p, __m256i *qp) {
  const __m128i sign = _mm_srai_epi16(*p, 15);
  const __m128i dc = _mm_unpacklo_epi16(*p, sign);
  const __m128i ac = _mm_unpackhi_epi16(*p, sign);
  *qp = _mm256_insertf128_si256(_mm256_castsi128_si256(dc), ac, 1);
}

static INLINE void update_qp(__m256i *qp) {
  int i;
  for (i = 0; i < 5; ++i) {
    qp[i] = _mm256_permute2x128_si256(qp[i], qp[i], 0x11);
  }
}

static INLINE void init_qp(const int16_t *zbin_ptr, const int16_t *round_ptr,
                           const int16_t *quant_ptr, const int16_t *dequant_ptr,
                           const int16_t *quant_shift_ptr, __m256i *qp) {
  const __m128i zbin = _mm_loadu_si128((const __m128i *)zbin_ptr);
  const __m128i round = _mm_loadu_si128((const __m128i *)round_ptr);
  const __m128i quant = _mm_loadu_si128((const __m128i *)quant_ptr);
  const __m128i dequant = _mm_loadu_si128((const __m128i *)dequant_ptr);
  const __m128i quant_shift = _mm_loadu_si128((const __m128i *)quant_shift_ptr);
  init_one_qp(&zbin, &qp[0]);
  init_one_qp(&round, &qp[1]);
  init_one_qp(&quant, &qp[2]);
  init_one_qp(&dequant, &qp[3]);
  init_one_qp(&quant_shift, &qp[4]);
}

// Note:
// *x is vector multiplied by *y which is 16 int32_t parallel multiplication
// and right shift 16.  The output, 16 int32_t is save in *p.
static INLINE void mm256_mul_shift_epi32(const __m256i *x, const __m256i *y,
                                         __m256i *p) {
  __m256i prod_lo = _mm256_mul_epi32(*x, *y);
  __m256i prod_hi = _mm256_srli_epi64(*x, 32);
  const __m256i mult_hi = _mm256_srli_epi64(*y, 32);
  prod_hi = _mm256_mul_epi32(prod_hi, mult_hi);

  prod_lo = _mm256_srli_epi64(prod_lo, 16);
  const __m256i mask = _mm256_set_epi32(0, -1, 0, -1, 0, -1, 0, -1);
  prod_lo = _mm256_and_si256(prod_lo, mask);
  prod_hi = _mm256_srli_epi64(prod_hi, 16);

  prod_hi = _mm256_slli_epi64(prod_hi, 32);
  *p = _mm256_or_si256(prod_lo, prod_hi);
}

static INLINE void quantize(const __m256i *qp, __m256i *c,
                            const int16_t *iscan_ptr, tran_low_t *qcoeff,
                            tran_low_t *dqcoeff, __m256i *eob) {
  const __m256i abs = _mm256_abs_epi32(*c);
  const __m256i flag1 = _mm256_cmpgt_epi32(abs, qp[0]);
  __m256i flag2 = _mm256_cmpeq_epi32(abs, qp[0]);
  flag2 = _mm256_or_si256(flag1, flag2);
  const int32_t nzflag = _mm256_movemask_epi8(flag2);

  if (LIKELY(nzflag)) {
    __m256i q = _mm256_add_epi32(abs, qp[1]);
    __m256i tmp;
    mm256_mul_shift_epi32(&q, &qp[2], &tmp);
    q = _mm256_add_epi32(tmp, q);

    mm256_mul_shift_epi32(&q, &qp[4], &q);
    __m256i dq = _mm256_mullo_epi32(q, qp[3]);

    q = _mm256_sign_epi32(q, *c);
    dq = _mm256_sign_epi32(dq, *c);
    q = _mm256_and_si256(q, flag2);
    dq = _mm256_and_si256(dq, flag2);

    _mm256_storeu_si256((__m256i *)qcoeff, q);
    _mm256_storeu_si256((__m256i *)dqcoeff, dq);

    const __m128i isc = _mm_loadu_si128((const __m128i *)iscan_ptr);
    const __m128i zr = _mm_setzero_si128();
    const __m128i lo = _mm_unpacklo_epi16(isc, zr);
    const __m128i hi = _mm_unpackhi_epi16(isc, zr);
    const __m256i iscan =
        _mm256_insertf128_si256(_mm256_castsi128_si256(lo), hi, 1);

    const __m256i zero = _mm256_setzero_si256();
    const __m256i zc = _mm256_cmpeq_epi32(dq, zero);
    const __m256i nz = _mm256_cmpeq_epi32(zc, zero);
    __m256i cur_eob = _mm256_sub_epi32(iscan, nz);
    cur_eob = _mm256_and_si256(cur_eob, nz);
    *eob = _mm256_max_epi32(cur_eob, *eob);
  } else {
    const __m256i zero = _mm256_setzero_si256();
    _mm256_storeu_si256((__m256i *)qcoeff, zero);
    _mm256_storeu_si256((__m256i *)dqcoeff, zero);
  }
}

void aom_highbd_quantize_b_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr, uint16_t *eob_ptr,
                                const int16_t *scan, const int16_t *iscan) {
  (void)scan;
  const unsigned int step = 8;

  __m256i qp[5], coeff;
  init_qp(zbin_ptr, round_ptr, quant_ptr, dequant_ptr, quant_shift_ptr, qp);
  coeff = _mm256_loadu_si256((const __m256i *)coeff_ptr);

  __m256i eob = _mm256_setzero_si256();
  quantize(qp, &coeff, iscan, qcoeff_ptr, dqcoeff_ptr, &eob);

  coeff_ptr += step;
  qcoeff_ptr += step;
  dqcoeff_ptr += step;
  iscan += step;
  n_coeffs -= step;

  update_qp(qp);

  while (n_coeffs > 0) {
    coeff = _mm256_loadu_si256((const __m256i *)coeff_ptr);
    quantize(qp, &coeff, iscan, qcoeff_ptr, dqcoeff_ptr, &eob);

    coeff_ptr += step;
    qcoeff_ptr += step;
    dqcoeff_ptr += step;
    iscan += step;
    n_coeffs -= step;
  }
  {
    __m256i eob_s;
    eob_s = _mm256_shuffle_epi32(eob, 0xe);
    eob = _mm256_max_epi16(eob, eob_s);
    eob_s = _mm256_shufflelo_epi16(eob, 0xe);
    eob = _mm256_max_epi16(eob, eob_s);
    eob_s = _mm256_shufflelo_epi16(eob, 1);
    eob = _mm256_max_epi16(eob, eob_s);
    const __m128i final_eob = _mm_max_epi16(_mm256_castsi256_si128(eob),
                                            _mm256_extractf128_si256(eob, 1));
    *eob_ptr = _mm_extract_epi16(final_eob, 0);
  }
}
