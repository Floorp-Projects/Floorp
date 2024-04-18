/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>
#include <string.h>

#include "config/av1_rtcd.h"

#include "av1/common/resize.h"

#include "aom_dsp/x86/synonyms.h"

#define CAST_HI(x) _mm256_castsi128_si256(x)
#define CAST_LOW(x) _mm256_castsi256_si128(x)

#define PROCESS_RESIZE_Y_WD16                                               \
  const int idx1 = AOMMIN(height - 1, i + 5);                               \
  const int idx2 = AOMMIN(height - 1, i + 6);                               \
  l6 = l10;                                                                 \
  l7 = l11;                                                                 \
  l8 = _mm_loadu_si128((__m128i *)(data + idx1 * stride));                  \
  l9 = _mm_loadu_si128((__m128i *)(data + idx2 * stride));                  \
                                                                            \
  /* g0... g15 | i0... i15 */                                               \
  const __m256i s68 =                                                       \
      _mm256_permute2x128_si256(CAST_HI(l6), CAST_HI(l8), 0x20);            \
  /* h0... h15 | j0... j15 */                                               \
  const __m256i s79 =                                                       \
      _mm256_permute2x128_si256(CAST_HI(l7), CAST_HI(l9), 0x20);            \
                                                                            \
  /* g0h0... g7g7 | i0j0... i7j */                                          \
  s[3] = _mm256_unpacklo_epi8(s68, s79);                                    \
  /* g8h8... g15g15 | i8j8... i15j15 */                                     \
  s[8] = _mm256_unpackhi_epi8(s68, s79);                                    \
                                                                            \
  __m256i res_out[2] = { 0 };                                               \
  resize_y_convolve(s, coeffs_y, res_out);                                  \
                                                                            \
  /* r00... r07 */                                                          \
  __m256i res_a_round_1 = _mm256_add_epi32(res_out[0], round_const_bits);   \
  /* r20... r27 */                                                          \
  __m256i res_a_round_2 = _mm256_add_epi32(res_out[1], round_const_bits);   \
                                                                            \
  res_a_round_1 = _mm256_sra_epi32(res_a_round_1, round_shift_bits);        \
  res_a_round_2 = _mm256_sra_epi32(res_a_round_2, round_shift_bits);        \
                                                                            \
  __m256i res_out_b[2] = { 0 };                                             \
  resize_y_convolve(s + 5, coeffs_y, res_out_b);                            \
                                                                            \
  /* r08... r015 */                                                         \
  __m256i res_b_round_1 = _mm256_add_epi32(res_out_b[0], round_const_bits); \
  /* r28... r215 */                                                         \
  __m256i res_b_round_2 = _mm256_add_epi32(res_out_b[1], round_const_bits); \
  res_b_round_1 = _mm256_sra_epi32(res_b_round_1, round_shift_bits);        \
  res_b_round_2 = _mm256_sra_epi32(res_b_round_2, round_shift_bits);        \
                                                                            \
  /* r00... r03 r20... r23 | r04... r07 r24... r27 */                       \
  __m256i res_8bit0 = _mm256_packus_epi32(res_a_round_1, res_a_round_2);    \
  /* r08... r012 r28... r212 | r013... r015 r213... r215 */                 \
  __m256i res_8bit1 = _mm256_packus_epi32(res_b_round_1, res_b_round_2);    \
  /* r00... r07 | r20... r27 */                                             \
  res_8bit0 = _mm256_permute4x64_epi64(res_8bit0, 0xd8);                    \
  /* r08... r015 | r28... r215 */                                           \
  res_8bit1 = _mm256_permute4x64_epi64(res_8bit1, 0xd8);                    \
  /* r00... r015 | r20... r215 */                                           \
  res_8bit1 = _mm256_packus_epi16(res_8bit0, res_8bit1);                    \
  res_8bit0 = _mm256_min_epu8(res_8bit1, clip_pixel);                       \
  res_8bit0 = _mm256_max_epu8(res_8bit0, zero);

#define PROCESS_RESIZE_Y_WD8                                              \
  const int idx1 = AOMMIN(height - 1, i + 5);                             \
  const int idx2 = AOMMIN(height - 1, i + 6);                             \
  l6 = l10;                                                               \
  l7 = l11;                                                               \
  l8 = _mm_loadl_epi64((__m128i *)(data + idx1 * stride));                \
  l9 = _mm_loadl_epi64((__m128i *)(data + idx2 * stride));                \
                                                                          \
  /* g0h0... g7h7 */                                                      \
  s67 = _mm_unpacklo_epi8(l6, l7);                                        \
  /* i0j0...i7j7 */                                                       \
  __m128i s89 = _mm_unpacklo_epi8(l8, l9);                                \
                                                                          \
  /* g0h0...g7g7 | i0j0...i7j7 */                                         \
  s[3] = _mm256_permute2x128_si256(CAST_HI(s67), CAST_HI(s89), 0x20);     \
                                                                          \
  __m256i res_out[2] = { 0 };                                             \
  resize_y_convolve(s, coeffs_y, res_out);                                \
                                                                          \
  /* r00... r07 */                                                        \
  __m256i res_a_round_1 = _mm256_add_epi32(res_out[0], round_const_bits); \
  /* r20...r27 */                                                         \
  __m256i res_a_round_2 = _mm256_add_epi32(res_out[1], round_const_bits); \
  res_a_round_1 = _mm256_sra_epi32(res_a_round_1, round_shift_bits);      \
  res_a_round_2 = _mm256_sra_epi32(res_a_round_2, round_shift_bits);      \
                                                                          \
  /* r00...r03 r20...r23 | r04...r07 r24...r27 */                         \
  res_a_round_1 = _mm256_packus_epi32(res_a_round_1, res_a_round_2);      \
  /* r00...r07 | r20...r27 */                                             \
  res_a_round_1 = _mm256_permute4x64_epi64(res_a_round_1, 0xd8);          \
  res_a_round_1 = _mm256_packus_epi16(res_a_round_1, res_a_round_1);      \
  res_a_round_1 = _mm256_min_epu8(res_a_round_1, clip_pixel);             \
  res_a_round_1 = _mm256_max_epu8(res_a_round_1, zero);

static INLINE void resize_y_convolve(const __m256i *const s,
                                     const __m256i *const coeffs,
                                     __m256i *res_out) {
  const __m256i res_0 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_maddubs_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_maddubs_epi16(s[3], coeffs[3]);

  const __m256i dst_0 = _mm256_add_epi16(res_0, res_1);
  const __m256i dst_1 = _mm256_add_epi16(res_2, res_3);
  // The sum of convolve operation crosses signed 16bit. Hence, the addition
  // should happen in 32bit.
  const __m256i dst_00 = _mm256_cvtepi16_epi32(CAST_LOW(dst_0));
  const __m256i dst_01 =
      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(dst_0, 1));
  const __m256i dst_10 = _mm256_cvtepi16_epi32(CAST_LOW(dst_1));
  const __m256i dst_11 =
      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(dst_1, 1));

  res_out[0] = _mm256_add_epi32(dst_00, dst_10);
  res_out[1] = _mm256_add_epi32(dst_01, dst_11);
}

static INLINE void prepare_filter_coeffs(const int16_t *filter,
                                         __m256i *const coeffs /* [4] */) {
  // f0 f1 f2 f3 x x x x
  const __m128i sym_even_filter = _mm_loadl_epi64((__m128i *)filter);
  // f0 f1 f2 f3 f0 f1 f2 f3
  const __m128i tmp0 = _mm_shuffle_epi32(sym_even_filter, 0x44);
  // f0 f1 f2 f3 f1 f0 f3 f2
  const __m128i tmp1 = _mm_shufflehi_epi16(tmp0, 0xb1);

  const __m128i filter_8bit = _mm_packs_epi16(tmp1, tmp1);

  // f0 f1 f0 f1 ..
  coeffs[2] = _mm256_broadcastw_epi16(filter_8bit);
  // f2 f3 f2 f3 ..
  coeffs[3] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 2));
  // f3 f2 f3 f2 ..
  coeffs[0] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 6));
  // f1 f0 f1 f0 ..
  coeffs[1] = _mm256_broadcastw_epi16(_mm_bsrli_si128(filter_8bit, 4));
}

bool resize_vert_dir_avx2(uint8_t *intbuf, uint8_t *output, int out_stride,
                          int height, int height2, int stride, int start_col) {
  assert(start_col <= stride);
  // For the GM tool, the input layer height or width is assured to be an even
  // number. Hence the function 'down2_symodd()' is not invoked and SIMD
  // optimization of the same is not implemented.
  // When the input height is less than 8 and even, the potential input
  // heights are limited to 2, 4, or 6. These scenarios require seperate
  // handling due to padding requirements. Invoking the C function here will
  // eliminate the need for conditional statements within the subsequent SIMD
  // code to manage these cases.
  if (height & 1 || height < 8) {
    return resize_vert_dir_c(intbuf, output, out_stride, height, height2,
                             stride, start_col);
  }

  __m256i s[10], coeffs_y[4];
  const int bits = FILTER_BITS;

  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const uint8_t max_pixel = 255;
  const __m256i clip_pixel = _mm256_set1_epi8(max_pixel);
  const __m256i zero = _mm256_setzero_si256();

  prepare_filter_coeffs(av1_down2_symeven_half_filter, coeffs_y);

  const int num_col16 = stride / 16;
  int remain_col = stride % 16;
  // The core vertical SIMD processes 4 input rows simultaneously to generate
  // output corresponding to 2 rows. To streamline the core loop and eliminate
  // the need for conditional checks, the remaining rows (4 or 6) are processed
  // separately.
  const int remain_row = (height % 4 == 0) ? 4 : 6;

  for (int j = start_col; j < stride - remain_col; j += 16) {
    const uint8_t *data = &intbuf[j];
    const __m128i l3 = _mm_loadu_si128((__m128i *)(data + 0 * stride));
    // Padding top 3 rows with the last available row at the top.
    const __m128i l0 = l3;
    const __m128i l1 = l3;
    const __m128i l2 = l3;
    const __m128i l4 = _mm_loadu_si128((__m128i *)(data + 1 * stride));

    __m128i l6, l7, l8, l9;
    __m128i l5 = _mm_loadu_si128((__m128i *)(data + 2 * stride));
    __m128i l10 = _mm_loadu_si128((__m128i *)(data + 3 * stride));
    __m128i l11 = _mm_loadu_si128((__m128i *)(data + 4 * stride));

    // a0...a15 | c0...c15
    const __m256i s02 =
        _mm256_permute2x128_si256(CAST_HI(l0), CAST_HI(l2), 0x20);
    // b0...b15 | d0...d15
    const __m256i s13 =
        _mm256_permute2x128_si256(CAST_HI(l1), CAST_HI(l3), 0x20);
    // c0...c15 | e0...e15
    const __m256i s24 =
        _mm256_permute2x128_si256(CAST_HI(l2), CAST_HI(l4), 0x20);
    // d0...d15 | f0...f15
    const __m256i s35 =
        _mm256_permute2x128_si256(CAST_HI(l3), CAST_HI(l5), 0x20);
    // e0...e15 | g0...g15
    const __m256i s46 =
        _mm256_permute2x128_si256(CAST_HI(l4), CAST_HI(l10), 0x20);
    // f0...f15 | h0...h15
    const __m256i s57 =
        _mm256_permute2x128_si256(CAST_HI(l5), CAST_HI(l11), 0x20);

    // a0b0...a7b7 | c0d0...c7d7
    s[0] = _mm256_unpacklo_epi8(s02, s13);
    // c0d0...c7d7 | e0f0...e7f7
    s[1] = _mm256_unpacklo_epi8(s24, s35);
    // e0f0...e7f7 | g0h0...g7h7
    s[2] = _mm256_unpacklo_epi8(s46, s57);

    // a8b8...a15b15 | c8d8...c15d15
    s[5] = _mm256_unpackhi_epi8(s02, s13);
    // c8d8...c15d15 | e8f8...e15f15
    s[6] = _mm256_unpackhi_epi8(s24, s35);
    // e8f8...e15f15 | g8h8...g15h15
    s[7] = _mm256_unpackhi_epi8(s46, s57);

    // height to be processed here
    const int process_ht = height - remain_row;
    for (int i = 0; i < process_ht; i += 4) {
      PROCESS_RESIZE_Y_WD16

      _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                       CAST_LOW(res_8bit0));

      _mm_storeu_si128(
          (__m128i *)&output[(i / 2) * out_stride + j + out_stride],
          _mm256_extracti128_si256(res_8bit0, 1));

      // Load the required data for processing of next 4 input rows.
      const int idx7 = AOMMIN(height - 1, i + 7);
      const int idx8 = AOMMIN(height - 1, i + 8);
      l10 = _mm_loadu_si128((__m128i *)(data + idx7 * stride));
      l11 = _mm_loadu_si128((__m128i *)(data + idx8 * stride));

      const __m256i s810 =
          _mm256_permute2x128_si256(CAST_HI(l8), CAST_HI(l10), 0x20);
      const __m256i s911 =
          _mm256_permute2x128_si256(CAST_HI(l9), CAST_HI(l11), 0x20);
      // i0j0... i7j7 | k0l0... k7l7
      s[4] = _mm256_unpacklo_epi8(s810, s911);
      // i8j8... i15j15 | k8l8... k15l15
      s[9] = _mm256_unpackhi_epi8(s810, s911);

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];

      s[5] = s[7];
      s[6] = s[8];
      s[7] = s[9];
    }

    // Process the remaining last 4 or 6 rows here.
    int i = process_ht;
    while (i < height - 1) {
      PROCESS_RESIZE_Y_WD16

      _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                       CAST_LOW(res_8bit0));
      i += 2;

      const int is_store_valid = (i < height - 1);
      if (is_store_valid)
        _mm_storeu_si128((__m128i *)&output[(i / 2) * out_stride + j],
                         _mm256_extracti128_si256(res_8bit0, 1));
      i += 2;

      // Check if there is any remaining height to process. If so, perform the
      // necessary data loading for processing the next row.
      if (i < height - 1) {
        l10 = l11 = l9;
        const __m256i s810 =
            _mm256_permute2x128_si256(CAST_HI(l8), CAST_HI(l10), 0x20);
        const __m256i s911 =
            _mm256_permute2x128_si256(CAST_HI(l9), CAST_HI(l11), 0x20);
        // i0j0... i7j7 | k0l0... k7l7
        s[4] = _mm256_unpacklo_epi8(s810, s911);
        // i8j8... i15j15 | k8l8... k15l15
        s[9] = _mm256_unpackhi_epi8(s810, s911);

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];

        s[5] = s[7];
        s[6] = s[8];
        s[7] = s[9];
      }
    }
  }

  if (remain_col > 7) {
    const int processed_wd = num_col16 * 16;
    remain_col = stride % 8;

    const uint8_t *data = &intbuf[processed_wd];

    const __m128i l3 = _mm_loadl_epi64((__m128i *)(data + 0 * stride));
    // Padding top 3 rows with available top-most row.
    const __m128i l0 = l3;
    const __m128i l1 = l3;
    const __m128i l2 = l3;
    const __m128i l4 = _mm_loadl_epi64((__m128i *)(data + 1 * stride));

    __m128i l6, l7, l8, l9;
    __m128i l5 = _mm_loadl_epi64((__m128i *)(data + 2 * stride));
    __m128i l10 = _mm_loadl_epi64((__m128i *)(data + 3 * stride));
    __m128i l11 = _mm_loadl_epi64((__m128i *)(data + 4 * stride));

    // a0b0...a7b7
    const __m128i s01 = _mm_unpacklo_epi8(l0, l1);
    // c0d0...c7d7
    const __m128i s23 = _mm_unpacklo_epi8(l2, l3);
    // e0f0...e7f7
    const __m128i s45 = _mm_unpacklo_epi8(l4, l5);
    // g0h0...g7h7
    __m128i s67 = _mm_unpacklo_epi8(l10, l11);

    // a0b0...a7b7 | c0d0...c7d7
    s[0] = _mm256_permute2x128_si256(CAST_HI(s01), CAST_HI(s23), 0x20);
    // c0d0...c7d7 | e0f0...e7f7
    s[1] = _mm256_permute2x128_si256(CAST_HI(s23), CAST_HI(s45), 0x20);
    // e0f0...e7f7 | g0h0...g7h7
    s[2] = _mm256_permute2x128_si256(CAST_HI(s45), CAST_HI(s67), 0x20);

    // height to be processed here
    const int process_ht = height - remain_row;
    for (int i = 0; i < process_ht; i += 4) {
      PROCESS_RESIZE_Y_WD8

      _mm_storel_epi64((__m128i *)&output[(i / 2) * out_stride + processed_wd],
                       CAST_LOW(res_a_round_1));

      _mm_storel_epi64(
          (__m128i *)&output[(i / 2) * out_stride + processed_wd + out_stride],
          _mm256_extracti128_si256(res_a_round_1, 1));

      const int idx7 = AOMMIN(height - 1, i + 7);
      const int idx8 = AOMMIN(height - 1, i + 8);
      l10 = _mm_loadl_epi64((__m128i *)(data + idx7 * stride));
      l11 = _mm_loadl_epi64((__m128i *)(data + idx8 * stride));

      // k0l0... k7l7
      const __m128i s10s11 = _mm_unpacklo_epi8(l10, l11);
      // i0j0... i7j7 | k0l0... k7l7
      s[4] = _mm256_permute2x128_si256(CAST_HI(s89), CAST_HI(s10s11), 0x20);

      s[0] = s[2];
      s[1] = s[3];
      s[2] = s[4];
    }

    // Process the remaining last 4 or 6 rows here.
    int i = process_ht;
    while (i < height - 1) {
      PROCESS_RESIZE_Y_WD8

      _mm_storel_epi64((__m128i *)&output[(i / 2) * out_stride + processed_wd],
                       CAST_LOW(res_a_round_1));

      i += 2;

      const int is_store_valid = (i < height - 1);
      if (is_store_valid)
        _mm_storel_epi64(
            (__m128i *)&output[(i / 2) * out_stride + processed_wd],
            _mm256_extracti128_si256(res_a_round_1, 1));
      i += 2;

      // Check rows are still remaining for processing. If yes do the required
      // load of data for the next iteration.
      if (i < height - 1) {
        l10 = l11 = l9;
        // k0l0... k7l7
        const __m128i s10s11 = _mm_unpacklo_epi8(l10, l11);
        // i0j0... i7j7 | k0l0... k7l7
        s[4] = _mm256_permute2x128_si256(CAST_HI(s89), CAST_HI(s10s11), 0x20);

        s[0] = s[2];
        s[1] = s[3];
        s[2] = s[4];
      }
    }
  }

  if (remain_col)
    return resize_vert_dir_c(intbuf, output, out_stride, height, height2,
                             stride, stride - remain_col);

  return true;
}
