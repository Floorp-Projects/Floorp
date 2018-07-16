/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/x86/synonyms.h"
#include "av1/common/enums.h"
#include "av1/common/reconintra.h"

void av1_filter_intra_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride,
                                       TX_SIZE tx_size, const uint8_t *above,
                                       const uint8_t *left, int mode) {
  int r, c;
  uint8_t buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  assert(bw <= 32 && bh <= 32);

  // The initialization is just for silencing Jenkins static analysis warnings
  for (r = 0; r < bh + 1; ++r)
    memset(buffer[r], 0, (bw + 1) * sizeof(buffer[0][0]));

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = left[r];
  memcpy(buffer[0], &above[-1], (bw + 1) * sizeof(uint8_t));

  const __m128i f1f0 = xx_load_128(av1_filter_intra_taps[mode][0]);
  const __m128i f3f2 = xx_load_128(av1_filter_intra_taps[mode][2]);
  const __m128i f5f4 = xx_load_128(av1_filter_intra_taps[mode][4]);
  const __m128i f7f6 = xx_load_128(av1_filter_intra_taps[mode][6]);
  const __m128i filter_intra_scale_bits =
      _mm_set1_epi16(1 << (15 - FILTER_INTRA_SCALE_BITS));

  for (r = 1; r < bh + 1; r += 2) {
    for (c = 1; c < bw + 1; c += 4) {
      DECLARE_ALIGNED(16, uint8_t, p[8]);
      memcpy(p, &buffer[r - 1][c - 1], 5 * sizeof(uint8_t));
      p[5] = buffer[r][c - 1];
      p[6] = buffer[r + 1][c - 1];
      p[7] = 0;
      const __m128i p_b = xx_loadl_64(p);
      const __m128i in = _mm_unpacklo_epi64(p_b, p_b);
      const __m128i out_01 = _mm_maddubs_epi16(in, f1f0);
      const __m128i out_23 = _mm_maddubs_epi16(in, f3f2);
      const __m128i out_45 = _mm_maddubs_epi16(in, f5f4);
      const __m128i out_67 = _mm_maddubs_epi16(in, f7f6);
      const __m128i out_0123 = _mm_hadd_epi16(out_01, out_23);
      const __m128i out_4567 = _mm_hadd_epi16(out_45, out_67);
      const __m128i out_01234567 = _mm_hadd_epi16(out_0123, out_4567);
      // Rounding
      const __m128i round_w =
          _mm_mulhrs_epi16(out_01234567, filter_intra_scale_bits);
      const __m128i out_r = _mm_packus_epi16(round_w, round_w);
      const __m128i out_r1 = _mm_srli_si128(out_r, 4);
      // Storing
      xx_storel_32(&buffer[r][c], out_r);
      xx_storel_32(&buffer[r + 1][c], out_r1);
    }
  }

  for (r = 0; r < bh; ++r) {
    memcpy(dst, &buffer[r + 1][1], bw * sizeof(uint8_t));
    dst += stride;
  }
}
