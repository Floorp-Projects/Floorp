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

#ifndef _AOM_DSP_X86_LPF_COMMON_X86_H
#define _AOM_DSP_X86_LPF_COMMON_X86_H

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"

static INLINE void highbd_transpose(uint16_t *src[], int in_p, uint16_t *dst[],
                                    int out_p, int num_8x8_to_transpose) {
  int idx8x8 = 0;
  __m128i p0, p1, p2, p3, p4, p5, p6, p7, x0, x1, x2, x3, x4, x5, x6, x7;
  do {
    uint16_t *in = src[idx8x8];
    uint16_t *out = dst[idx8x8];

    p0 =
        _mm_loadu_si128((__m128i *)(in + 0 * in_p));  // 00 01 02 03 04 05 06 07
    p1 =
        _mm_loadu_si128((__m128i *)(in + 1 * in_p));  // 10 11 12 13 14 15 16 17
    p2 =
        _mm_loadu_si128((__m128i *)(in + 2 * in_p));  // 20 21 22 23 24 25 26 27
    p3 =
        _mm_loadu_si128((__m128i *)(in + 3 * in_p));  // 30 31 32 33 34 35 36 37
    p4 =
        _mm_loadu_si128((__m128i *)(in + 4 * in_p));  // 40 41 42 43 44 45 46 47
    p5 =
        _mm_loadu_si128((__m128i *)(in + 5 * in_p));  // 50 51 52 53 54 55 56 57
    p6 =
        _mm_loadu_si128((__m128i *)(in + 6 * in_p));  // 60 61 62 63 64 65 66 67
    p7 =
        _mm_loadu_si128((__m128i *)(in + 7 * in_p));  // 70 71 72 73 74 75 76 77
    // 00 10 01 11 02 12 03 13
    x0 = _mm_unpacklo_epi16(p0, p1);
    // 20 30 21 31 22 32 23 33
    x1 = _mm_unpacklo_epi16(p2, p3);
    // 40 50 41 51 42 52 43 53
    x2 = _mm_unpacklo_epi16(p4, p5);
    // 60 70 61 71 62 72 63 73
    x3 = _mm_unpacklo_epi16(p6, p7);
    // 00 10 20 30 01 11 21 31
    x4 = _mm_unpacklo_epi32(x0, x1);
    // 40 50 60 70 41 51 61 71
    x5 = _mm_unpacklo_epi32(x2, x3);
    // 00 10 20 30 40 50 60 70
    x6 = _mm_unpacklo_epi64(x4, x5);
    // 01 11 21 31 41 51 61 71
    x7 = _mm_unpackhi_epi64(x4, x5);

    _mm_storeu_si128((__m128i *)(out + 0 * out_p), x6);
    // 00 10 20 30 40 50 60 70
    _mm_storeu_si128((__m128i *)(out + 1 * out_p), x7);
    // 01 11 21 31 41 51 61 71

    // 02 12 22 32 03 13 23 33
    x4 = _mm_unpackhi_epi32(x0, x1);
    // 42 52 62 72 43 53 63 73
    x5 = _mm_unpackhi_epi32(x2, x3);
    // 02 12 22 32 42 52 62 72
    x6 = _mm_unpacklo_epi64(x4, x5);
    // 03 13 23 33 43 53 63 73
    x7 = _mm_unpackhi_epi64(x4, x5);

    _mm_storeu_si128((__m128i *)(out + 2 * out_p), x6);
    // 02 12 22 32 42 52 62 72
    _mm_storeu_si128((__m128i *)(out + 3 * out_p), x7);
    // 03 13 23 33 43 53 63 73

    // 04 14 05 15 06 16 07 17
    x0 = _mm_unpackhi_epi16(p0, p1);
    // 24 34 25 35 26 36 27 37
    x1 = _mm_unpackhi_epi16(p2, p3);
    // 44 54 45 55 46 56 47 57
    x2 = _mm_unpackhi_epi16(p4, p5);
    // 64 74 65 75 66 76 67 77
    x3 = _mm_unpackhi_epi16(p6, p7);
    // 04 14 24 34 05 15 25 35
    x4 = _mm_unpacklo_epi32(x0, x1);
    // 44 54 64 74 45 55 65 75
    x5 = _mm_unpacklo_epi32(x2, x3);
    // 04 14 24 34 44 54 64 74
    x6 = _mm_unpacklo_epi64(x4, x5);
    // 05 15 25 35 45 55 65 75
    x7 = _mm_unpackhi_epi64(x4, x5);

    _mm_storeu_si128((__m128i *)(out + 4 * out_p), x6);
    // 04 14 24 34 44 54 64 74
    _mm_storeu_si128((__m128i *)(out + 5 * out_p), x7);
    // 05 15 25 35 45 55 65 75

    // 06 16 26 36 07 17 27 37
    x4 = _mm_unpackhi_epi32(x0, x1);
    // 46 56 66 76 47 57 67 77
    x5 = _mm_unpackhi_epi32(x2, x3);
    // 06 16 26 36 46 56 66 76
    x6 = _mm_unpacklo_epi64(x4, x5);
    // 07 17 27 37 47 57 67 77
    x7 = _mm_unpackhi_epi64(x4, x5);

    _mm_storeu_si128((__m128i *)(out + 6 * out_p), x6);
    // 06 16 26 36 46 56 66 76
    _mm_storeu_si128((__m128i *)(out + 7 * out_p), x7);
    // 07 17 27 37 47 57 67 77
  } while (++idx8x8 < num_8x8_to_transpose);
}

static INLINE void highbd_transpose8x16(uint16_t *in0, uint16_t *in1, int in_p,
                                        uint16_t *out, int out_p) {
  uint16_t *src0[1];
  uint16_t *src1[1];
  uint16_t *dest0[1];
  uint16_t *dest1[1];
  src0[0] = in0;
  src1[0] = in1;
  dest0[0] = out;
  dest1[0] = out + 8;
  highbd_transpose(src0, in_p, dest0, out_p, 1);
  highbd_transpose(src1, in_p, dest1, out_p, 1);
}
#endif  // _AOM_DSP_X86_LPF_COMMON_X86_H
