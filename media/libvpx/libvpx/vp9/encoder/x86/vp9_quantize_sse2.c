/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <emmintrin.h>
#include <xmmintrin.h>

#include "./vp9_rtcd.h"
#include "vpx/vpx_integer.h"

void vp9_quantize_fp_sse2(const int16_t *coeff_ptr, intptr_t n_coeffs,
                          int skip_block, const int16_t *zbin_ptr,
                          const int16_t *round_ptr, const int16_t *quant_ptr,
                          const int16_t *quant_shift_ptr, int16_t *qcoeff_ptr,
                          int16_t *dqcoeff_ptr, const int16_t *dequant_ptr,
                          uint16_t *eob_ptr, const int16_t *scan_ptr,
                          const int16_t *iscan_ptr) {
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
        coeff0 = _mm_load_si128((const __m128i *)(coeff_ptr + n_coeffs));
        coeff1 = _mm_load_si128((const __m128i *)(coeff_ptr + n_coeffs) + 1);

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

        _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs), qcoeff0);
        _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs) + 1, qcoeff1);

        coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
        dequant = _mm_unpackhi_epi64(dequant, dequant);
        coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

        _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs), coeff0);
        _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs) + 1, coeff1);
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

        coeff0 = _mm_load_si128((const __m128i *)(coeff_ptr + n_coeffs));
        coeff1 = _mm_load_si128((const __m128i *)(coeff_ptr + n_coeffs) + 1);

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

          _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs), qcoeff0);
          _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs) + 1, qcoeff1);

          coeff0 = _mm_mullo_epi16(qcoeff0, dequant);
          coeff1 = _mm_mullo_epi16(qcoeff1, dequant);

          _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs), coeff0);
          _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs) + 1, coeff1);
        } else {
          _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs), zero);
          _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs) + 1, zero);

          _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs), zero);
          _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs) + 1, zero);
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
      _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs), zero);
      _mm_store_si128((__m128i *)(dqcoeff_ptr + n_coeffs) + 1, zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs), zero);
      _mm_store_si128((__m128i *)(qcoeff_ptr + n_coeffs) + 1, zero);
      n_coeffs += 8 * 2;
    } while (n_coeffs < 0);
    *eob_ptr = 0;
  }
}
