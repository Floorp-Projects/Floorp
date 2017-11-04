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

#include <emmintrin.h>
#include <xmmintrin.h>

#include "./av1_rtcd.h"
#include "aom/aom_integer.h"

static INLINE void read_coeff(const tran_low_t *coeff, intptr_t offset,
                              __m128i *c0, __m128i *c1) {
  const tran_low_t *addr = coeff + offset;
  if (sizeof(tran_low_t) == 4) {
    const __m128i x0 = _mm_load_si128((const __m128i *)addr);
    const __m128i x1 = _mm_load_si128((const __m128i *)addr + 1);
    const __m128i x2 = _mm_load_si128((const __m128i *)addr + 2);
    const __m128i x3 = _mm_load_si128((const __m128i *)addr + 3);
    *c0 = _mm_packs_epi32(x0, x1);
    *c1 = _mm_packs_epi32(x2, x3);
  } else {
    *c0 = _mm_load_si128((const __m128i *)addr);
    *c1 = _mm_load_si128((const __m128i *)addr + 1);
  }
}

static INLINE void write_qcoeff(const __m128i *qc0, const __m128i *qc1,
                                tran_low_t *qcoeff, intptr_t offset) {
  tran_low_t *addr = qcoeff + offset;
  if (sizeof(tran_low_t) == 4) {
    const __m128i zero = _mm_setzero_si128();
    __m128i sign_bits = _mm_cmplt_epi16(*qc0, zero);
    __m128i y0 = _mm_unpacklo_epi16(*qc0, sign_bits);
    __m128i y1 = _mm_unpackhi_epi16(*qc0, sign_bits);
    _mm_store_si128((__m128i *)addr, y0);
    _mm_store_si128((__m128i *)addr + 1, y1);

    sign_bits = _mm_cmplt_epi16(*qc1, zero);
    y0 = _mm_unpacklo_epi16(*qc1, sign_bits);
    y1 = _mm_unpackhi_epi16(*qc1, sign_bits);
    _mm_store_si128((__m128i *)addr + 2, y0);
    _mm_store_si128((__m128i *)addr + 3, y1);
  } else {
    _mm_store_si128((__m128i *)addr, *qc0);
    _mm_store_si128((__m128i *)addr + 1, *qc1);
  }
}

static INLINE void write_zero(tran_low_t *qcoeff, intptr_t offset) {
  const __m128i zero = _mm_setzero_si128();
  tran_low_t *addr = qcoeff + offset;
  if (sizeof(tran_low_t) == 4) {
    _mm_store_si128((__m128i *)addr, zero);
    _mm_store_si128((__m128i *)addr + 1, zero);
    _mm_store_si128((__m128i *)addr + 2, zero);
    _mm_store_si128((__m128i *)addr + 3, zero);
  } else {
    _mm_store_si128((__m128i *)addr, zero);
    _mm_store_si128((__m128i *)addr + 1, zero);
  }
}

void av1_quantize_fp_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                          int skip_block, const int16_t *zbin_ptr,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr,
                          tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
                          const int16_t *dequant_ptr, uint16_t *eob_ptr,
                          const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  __m128i zero;
  __m128i thr;
  int16_t nzflag;
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  coeff_ptr += n_coeffs;
  iscan_ptr += n_coeffs;
  qcoeff_ptr += n_coeffs;
  dqcoeff_ptr += n_coeffs;
  n_coeffs = -n_coeffs;
  zero = _mm_setzero_si128();

  if (!skip_block) {
    __m128i eob;
    __m128i round, quant, dequant;
    {
      __m128i coeff0, coeff1;

      // Setup global values
      {
        round = _mm_load_si128((const __m128i *)round_ptr);
        quant = _mm_load_si128((const __m128i *)quant_ptr);
        dequant = _mm_load_si128((const __m128i *)dequant_ptr);
      }

      {
        __m128i coeff0_sign, coeff1_sign;
        __m128i qcoeff0, qcoeff1;
        __m128i qtmp0, qtmp1;
        // Do DC and first 15 AC
        read_coeff(coeff_ptr, n_coeffs, &coeff0, &coeff1);

        // Poor man's sign extract
        coeff0_sign = _mm_srai_epi16(coeff0, 15);
        coeff1_sign = _mm_srai_epi16(coeff1, 15);
        qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        qcoeff0 = _mm_adds_epi16(qcoeff0, round);
        round = _mm_unpackhi_epi64(round, round);
        qcoeff1 = _mm_adds_epi16(qcoeff1, round);
        qtmp0 = _mm_mulhi_epi16(qcoeff0, quant);
        quant = _mm_unpackhi_epi64(quant, quant);
        qtmp1 = _mm_mulhi_epi16(qcoeff1, quant);

        // Reinsert signs
        qcoeff0 = _mm_xor_si128(qtmp0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(qtmp1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        write_qcoeff(&qcoeff0, &qcoeff1, qcoeff_ptr, n_coeffs);

        coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
        dequant = _mm_unpackhi_epi64(dequant, dequant);
        coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

        write_qcoeff(&coeff0, &coeff1, dqcoeff_ptr, n_coeffs);
      }

      {
        // Scan for eob
        __m128i zero_coeff0, zero_coeff1;
        __m128i nzero_coeff0, nzero_coeff1;
        __m128i iscan0, iscan1;
        __m128i eob1;
        zero_coeff0 = _mm_cmpeq_epi16(coeff0, zero);
        zero_coeff1 = _mm_cmpeq_epi16(coeff1, zero);
        nzero_coeff0 = _mm_cmpeq_epi16(zero_coeff0, zero);
        nzero_coeff1 = _mm_cmpeq_epi16(zero_coeff1, zero);
        iscan0 = _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs));
        iscan1 = _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs) + 1);
        // Add one to convert from indices to counts
        iscan0 = _mm_sub_epi16(iscan0, nzero_coeff0);
        iscan1 = _mm_sub_epi16(iscan1, nzero_coeff1);
        eob = _mm_and_si128(iscan0, nzero_coeff0);
        eob1 = _mm_and_si128(iscan1, nzero_coeff1);
        eob = _mm_max_epi16(eob, eob1);
      }
      n_coeffs += 8 * 2;
    }

    thr = _mm_srai_epi16(dequant, 1);

    // AC only loop
    while (n_coeffs < 0) {
      __m128i coeff0, coeff1;
      {
        __m128i coeff0_sign, coeff1_sign;
        __m128i qcoeff0, qcoeff1;
        __m128i qtmp0, qtmp1;

        read_coeff(coeff_ptr, n_coeffs, &coeff0, &coeff1);

        // Poor man's sign extract
        coeff0_sign = _mm_srai_epi16(coeff0, 15);
        coeff1_sign = _mm_srai_epi16(coeff1, 15);
        qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        nzflag = _mm_movemask_epi8(_mm_cmpgt_epi16(qcoeff0, thr)) |
                 _mm_movemask_epi8(_mm_cmpgt_epi16(qcoeff1, thr));

        if (nzflag) {
          qcoeff0 = _mm_adds_epi16(qcoeff0, round);
          qcoeff1 = _mm_adds_epi16(qcoeff1, round);
          qtmp0 = _mm_mulhi_epi16(qcoeff0, quant);
          qtmp1 = _mm_mulhi_epi16(qcoeff1, quant);

          // Reinsert signs
          qcoeff0 = _mm_xor_si128(qtmp0, coeff0_sign);
          qcoeff1 = _mm_xor_si128(qtmp1, coeff1_sign);
          qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
          qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

          write_qcoeff(&qcoeff0, &qcoeff1, qcoeff_ptr, n_coeffs);

          coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
          coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

          write_qcoeff(&coeff0, &coeff1, dqcoeff_ptr, n_coeffs);
        } else {
          write_zero(qcoeff_ptr, n_coeffs);
          write_zero(dqcoeff_ptr, n_coeffs);
        }
      }

      if (nzflag) {
        // Scan for eob
        __m128i zero_coeff0, zero_coeff1;
        __m128i nzero_coeff0, nzero_coeff1;
        __m128i iscan0, iscan1;
        __m128i eob0, eob1;
        zero_coeff0 = _mm_cmpeq_epi16(coeff0, zero);
        zero_coeff1 = _mm_cmpeq_epi16(coeff1, zero);
        nzero_coeff0 = _mm_cmpeq_epi16(zero_coeff0, zero);
        nzero_coeff1 = _mm_cmpeq_epi16(zero_coeff1, zero);
        iscan0 = _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs));
        iscan1 = _mm_load_si128((const __m128i *)(iscan_ptr + n_coeffs) + 1);
        // Add one to convert from indices to counts
        iscan0 = _mm_sub_epi16(iscan0, nzero_coeff0);
        iscan1 = _mm_sub_epi16(iscan1, nzero_coeff1);
        eob0 = _mm_and_si128(iscan0, nzero_coeff0);
        eob1 = _mm_and_si128(iscan1, nzero_coeff1);
        eob0 = _mm_max_epi16(eob0, eob1);
        eob = _mm_max_epi16(eob, eob0);
      }
      n_coeffs += 8 * 2;
    }

    // Accumulate EOB
    {
      __m128i eob_shuffled;
      eob_shuffled = _mm_shuffle_epi32(eob, 0xe);
      eob = _mm_max_epi16(eob, eob_shuffled);
      eob_shuffled = _mm_shufflelo_epi16(eob, 0xe);
      eob = _mm_max_epi16(eob, eob_shuffled);
      eob_shuffled = _mm_shufflelo_epi16(eob, 0x1);
      eob = _mm_max_epi16(eob, eob_shuffled);
      *eob_ptr = _mm_extract_epi16(eob, 1);
    }
  } else {
    do {
      write_zero(dqcoeff_ptr, n_coeffs);
      write_zero(qcoeff_ptr, n_coeffs);
      n_coeffs += 8 * 2;
    } while (n_coeffs < 0);
    *eob_ptr = 0;
  }
}
