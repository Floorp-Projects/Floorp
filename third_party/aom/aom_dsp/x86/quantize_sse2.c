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

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

static INLINE __m128i load_coefficients(const tran_low_t *coeff_ptr) {
  if (sizeof(tran_low_t) == 4) {
    return _mm_setr_epi16((int16_t)coeff_ptr[0], (int16_t)coeff_ptr[1],
                          (int16_t)coeff_ptr[2], (int16_t)coeff_ptr[3],
                          (int16_t)coeff_ptr[4], (int16_t)coeff_ptr[5],
                          (int16_t)coeff_ptr[6], (int16_t)coeff_ptr[7]);
  } else {
    return _mm_load_si128((const __m128i *)coeff_ptr);
  }
}

static INLINE void store_coefficients(__m128i coeff_vals,
                                      tran_low_t *coeff_ptr) {
  if (sizeof(tran_low_t) == 4) {
    __m128i one = _mm_set1_epi16(1);
    __m128i coeff_vals_hi = _mm_mulhi_epi16(coeff_vals, one);
    __m128i coeff_vals_lo = _mm_mullo_epi16(coeff_vals, one);
    __m128i coeff_vals_1 = _mm_unpacklo_epi16(coeff_vals_lo, coeff_vals_hi);
    __m128i coeff_vals_2 = _mm_unpackhi_epi16(coeff_vals_lo, coeff_vals_hi);
    _mm_store_si128((__m128i *)(coeff_ptr), coeff_vals_1);
    _mm_store_si128((__m128i *)(coeff_ptr + 4), coeff_vals_2);
  } else {
    _mm_store_si128((__m128i *)(coeff_ptr), coeff_vals);
  }
}

void aom_quantize_b_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs,
                         int skip_block, const int16_t *zbin_ptr,
                         const int16_t *round_ptr, const int16_t *quant_ptr,
                         const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
                         tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                         uint16_t *eob_ptr, const int16_t *scan_ptr,
                         const int16_t *iscan_ptr) {
  __m128i zero;
  (void)scan_ptr;

  coeff_ptr += n_coeffs;
  iscan_ptr += n_coeffs;
  qcoeff_ptr += n_coeffs;
  dqcoeff_ptr += n_coeffs;
  n_coeffs = -n_coeffs;
  zero = _mm_setzero_si128();
  if (!skip_block) {
    __m128i eob;
    __m128i zbin;
    __m128i round, quant, dequant, shift;
    {
      __m128i coeff0, coeff1;

      // Setup global values
      {
        __m128i pw_1;
        zbin = _mm_load_si128((const __m128i *)zbin_ptr);
        round = _mm_load_si128((const __m128i *)round_ptr);
        quant = _mm_load_si128((const __m128i *)quant_ptr);
        pw_1 = _mm_set1_epi16(1);
        zbin = _mm_sub_epi16(zbin, pw_1);
        dequant = _mm_load_si128((const __m128i *)dequant_ptr);
        shift = _mm_load_si128((const __m128i *)quant_shift_ptr);
      }

      {
        __m128i coeff0_sign, coeff1_sign;
        __m128i qcoeff0, qcoeff1;
        __m128i qtmp0, qtmp1;
        __m128i cmp_mask0, cmp_mask1;
        // Do DC and first 15 AC
        coeff0 = load_coefficients(coeff_ptr + n_coeffs);
        coeff1 = load_coefficients(coeff_ptr + n_coeffs + 8);

        // Poor man's sign extract
        coeff0_sign = _mm_srai_epi16(coeff0, 15);
        coeff1_sign = _mm_srai_epi16(coeff1, 15);
        qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
        zbin = _mm_unpackhi_epi64(zbin, zbin);  // Switch DC to AC
        cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);
        qcoeff0 = _mm_adds_epi16(qcoeff0, round);
        round = _mm_unpackhi_epi64(round, round);
        qcoeff1 = _mm_adds_epi16(qcoeff1, round);
        qtmp0 = _mm_mulhi_epi16(qcoeff0, quant);
        quant = _mm_unpackhi_epi64(quant, quant);
        qtmp1 = _mm_mulhi_epi16(qcoeff1, quant);
        qtmp0 = _mm_add_epi16(qtmp0, qcoeff0);
        qtmp1 = _mm_add_epi16(qtmp1, qcoeff1);
        qcoeff0 = _mm_mulhi_epi16(qtmp0, shift);
        shift = _mm_unpackhi_epi64(shift, shift);
        qcoeff1 = _mm_mulhi_epi16(qtmp1, shift);

        // Reinsert signs
        qcoeff0 = _mm_xor_si128(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(qcoeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        // Mask out zbin threshold coeffs
        qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
        qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

        store_coefficients(qcoeff0, qcoeff_ptr + n_coeffs);
        store_coefficients(qcoeff1, qcoeff_ptr + n_coeffs + 8);

        coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
        dequant = _mm_unpackhi_epi64(dequant, dequant);
        coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

        store_coefficients(coeff0, dqcoeff_ptr + n_coeffs);
        store_coefficients(coeff1, dqcoeff_ptr + n_coeffs + 8);
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

    // AC only loop
    while (n_coeffs < 0) {
      __m128i coeff0, coeff1;
      {
        __m128i coeff0_sign, coeff1_sign;
        __m128i qcoeff0, qcoeff1;
        __m128i qtmp0, qtmp1;
        __m128i cmp_mask0, cmp_mask1;

        coeff0 = load_coefficients(coeff_ptr + n_coeffs);
        coeff1 = load_coefficients(coeff_ptr + n_coeffs + 8);

        // Poor man's sign extract
        coeff0_sign = _mm_srai_epi16(coeff0, 15);
        coeff1_sign = _mm_srai_epi16(coeff1, 15);
        qcoeff0 = _mm_xor_si128(coeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(coeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        cmp_mask0 = _mm_cmpgt_epi16(qcoeff0, zbin);
        cmp_mask1 = _mm_cmpgt_epi16(qcoeff1, zbin);
        qcoeff0 = _mm_adds_epi16(qcoeff0, round);
        qcoeff1 = _mm_adds_epi16(qcoeff1, round);
        qtmp0 = _mm_mulhi_epi16(qcoeff0, quant);
        qtmp1 = _mm_mulhi_epi16(qcoeff1, quant);
        qtmp0 = _mm_add_epi16(qtmp0, qcoeff0);
        qtmp1 = _mm_add_epi16(qtmp1, qcoeff1);
        qcoeff0 = _mm_mulhi_epi16(qtmp0, shift);
        qcoeff1 = _mm_mulhi_epi16(qtmp1, shift);

        // Reinsert signs
        qcoeff0 = _mm_xor_si128(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_xor_si128(qcoeff1, coeff1_sign);
        qcoeff0 = _mm_sub_epi16(qcoeff0, coeff0_sign);
        qcoeff1 = _mm_sub_epi16(qcoeff1, coeff1_sign);

        // Mask out zbin threshold coeffs
        qcoeff0 = _mm_and_si128(qcoeff0, cmp_mask0);
        qcoeff1 = _mm_and_si128(qcoeff1, cmp_mask1);

        store_coefficients(qcoeff0, qcoeff_ptr + n_coeffs);
        store_coefficients(qcoeff1, qcoeff_ptr + n_coeffs + 8);

        coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
        coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

        store_coefficients(coeff0, dqcoeff_ptr + n_coeffs);
        store_coefficients(coeff1, dqcoeff_ptr + n_coeffs + 8);
      }

      {
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
      store_coefficients(zero, dqcoeff_ptr + n_coeffs);
      store_coefficients(zero, dqcoeff_ptr + n_coeffs + 8);
      store_coefficients(zero, qcoeff_ptr + n_coeffs);
      store_coefficients(zero, qcoeff_ptr + n_coeffs + 8);
      n_coeffs += 8 * 2;
    } while (n_coeffs < 0);
    *eob_ptr = 0;
  }
}
