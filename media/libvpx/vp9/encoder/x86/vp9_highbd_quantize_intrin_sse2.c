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

#include "vpx_ports/mem.h"
#include "vp9/common/vp9_common.h"

#if CONFIG_VP9_HIGHBITDEPTH
// from vp9_idct.h: typedef int32_t tran_low_t;
void vp9_highbd_quantize_b_sse2(const tran_low_t *coeff_ptr,
                                intptr_t count,
                                int skip_block,
                                const int16_t *zbin_ptr,
                                const int16_t *round_ptr,
                                const int16_t *quant_ptr,
                                const int16_t *quant_shift_ptr,
                                tran_low_t *qcoeff_ptr,
                                tran_low_t *dqcoeff_ptr,
                                const int16_t *dequant_ptr,
                                uint16_t *eob_ptr,
                                const int16_t *scan,
                                const int16_t *iscan) {
  int i, j, non_zero_regs = (int)count / 4, eob_i = -1;
  __m128i zbins[2];
  __m128i nzbins[2];

  zbins[0] = _mm_set_epi32((int)zbin_ptr[1],
                           (int)zbin_ptr[1],
                           (int)zbin_ptr[1],
                           (int)zbin_ptr[0]);
  zbins[1] = _mm_set1_epi32((int)zbin_ptr[1]);

  nzbins[0] = _mm_setzero_si128();
  nzbins[1] = _mm_setzero_si128();
  nzbins[0] = _mm_sub_epi32(nzbins[0], zbins[0]);
  nzbins[1] = _mm_sub_epi32(nzbins[1], zbins[1]);

  (void)scan;

  memset(qcoeff_ptr, 0, count * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, count * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Pre-scan pass
    for (i = ((int)count / 4) - 1; i >= 0; i--) {
      __m128i coeffs, cmp1, cmp2;
      int test;
      coeffs = _mm_load_si128((const __m128i *)(coeff_ptr + i * 4));
      cmp1 = _mm_cmplt_epi32(coeffs, zbins[i != 0]);
      cmp2 = _mm_cmpgt_epi32(coeffs, nzbins[i != 0]);
      cmp1 = _mm_and_si128(cmp1, cmp2);
      test = _mm_movemask_epi8(cmp1);
      if (test == 0xffff)
        non_zero_regs--;
      else
        break;
    }

    // Quantization pass:
    for (i = 0; i < non_zero_regs; i++) {
      __m128i coeffs, coeffs_sign, tmp1, tmp2;
      int test;
      int abs_coeff[4];
      int coeff_sign[4];

      coeffs = _mm_load_si128((const __m128i *)(coeff_ptr + i * 4));
      coeffs_sign = _mm_srai_epi32(coeffs, 31);
      coeffs = _mm_sub_epi32(
            _mm_xor_si128(coeffs, coeffs_sign), coeffs_sign);
      tmp1 = _mm_cmpgt_epi32(coeffs, zbins[i != 0]);
      tmp2 = _mm_cmpeq_epi32(coeffs, zbins[i != 0]);
      tmp1 = _mm_or_si128(tmp1, tmp2);
      test = _mm_movemask_epi8(tmp1);
      _mm_storeu_si128((__m128i*)abs_coeff, coeffs);
      _mm_storeu_si128((__m128i*)coeff_sign, coeffs_sign);

      for (j = 0; j < 4; j++) {
        if (test & (1 << (4 * j))) {
          int k = 4 * i + j;
          int64_t tmp = clamp(abs_coeff[j] + round_ptr[k != 0],
                              INT32_MIN, INT32_MAX);
          tmp = ((((tmp * quant_ptr[k != 0]) >> 16) + tmp) *
                    quant_shift_ptr[k != 0]) >> 16;  // quantization
          qcoeff_ptr[k] = (tmp ^ coeff_sign[j]) - coeff_sign[j];
          dqcoeff_ptr[k] = qcoeff_ptr[k] * dequant_ptr[k != 0];
          if (tmp)
            eob_i = iscan[k] > eob_i ? iscan[k] : eob_i;
        }
      }
    }
  }
  *eob_ptr = eob_i + 1;
}


void vp9_highbd_quantize_b_32x32_sse2(const tran_low_t *coeff_ptr,
                                      intptr_t n_coeffs,
                                      int skip_block,
                                      const int16_t *zbin_ptr,
                                      const int16_t *round_ptr,
                                      const int16_t *quant_ptr,
                                      const int16_t *quant_shift_ptr,
                                      tran_low_t *qcoeff_ptr,
                                      tran_low_t *dqcoeff_ptr,
                                      const int16_t *dequant_ptr,
                                      uint16_t *eob_ptr,
                                      const int16_t *scan,
                                      const int16_t *iscan) {
  __m128i zbins[2];
  __m128i nzbins[2];
  int idx = 0;
  int idx_arr[1024];
  int i, eob = -1;
  const int zbin0_tmp = ROUND_POWER_OF_TWO(zbin_ptr[0], 1);
  const int zbin1_tmp = ROUND_POWER_OF_TWO(zbin_ptr[1], 1);
  (void)scan;
  zbins[0] = _mm_set_epi32(zbin1_tmp,
                           zbin1_tmp,
                           zbin1_tmp,
                           zbin0_tmp);
  zbins[1] = _mm_set1_epi32(zbin1_tmp);

  nzbins[0] = _mm_setzero_si128();
  nzbins[1] = _mm_setzero_si128();
  nzbins[0] = _mm_sub_epi32(nzbins[0], zbins[0]);
  nzbins[1] = _mm_sub_epi32(nzbins[1], zbins[1]);

  memset(qcoeff_ptr, 0, n_coeffs * sizeof(*qcoeff_ptr));
  memset(dqcoeff_ptr, 0, n_coeffs * sizeof(*dqcoeff_ptr));

  if (!skip_block) {
    // Pre-scan pass
    for (i = 0; i < n_coeffs / 4; i++) {
      __m128i coeffs, cmp1, cmp2;
      int test;
      coeffs = _mm_load_si128((const __m128i *)(coeff_ptr + i * 4));
      cmp1 = _mm_cmplt_epi32(coeffs, zbins[i != 0]);
      cmp2 = _mm_cmpgt_epi32(coeffs, nzbins[i != 0]);
      cmp1 = _mm_and_si128(cmp1, cmp2);
      test = _mm_movemask_epi8(cmp1);
      if (!(test & 0xf))
        idx_arr[idx++] = i * 4;
      if (!(test & 0xf0))
        idx_arr[idx++] = i * 4 + 1;
      if (!(test & 0xf00))
        idx_arr[idx++] = i * 4 + 2;
      if (!(test & 0xf000))
        idx_arr[idx++] = i * 4 + 3;
    }

    // Quantization pass: only process the coefficients selected in
    // pre-scan pass. Note: idx can be zero.
    for (i = 0; i < idx; i++) {
      const int rc = idx_arr[i];
      const int coeff = coeff_ptr[rc];
      const int coeff_sign = (coeff >> 31);
      int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      int64_t tmp = clamp(abs_coeff +
                          ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1),
                          INT32_MIN, INT32_MAX);
      tmp = ((((tmp * quant_ptr[rc != 0]) >> 16) + tmp) *
               quant_shift_ptr[rc != 0]) >> 15;

      qcoeff_ptr[rc] = (tmp ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0] / 2;

      if (tmp)
        eob = iscan[idx_arr[i]] > eob ? iscan[idx_arr[i]] : eob;
    }
  }
  *eob_ptr = eob + 1;
}
#endif
