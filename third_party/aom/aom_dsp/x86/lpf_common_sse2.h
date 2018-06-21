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

#include "config/aom_config.h"

static INLINE void highbd_transpose6x6_sse2(__m128i *x0, __m128i *x1,
                                            __m128i *x2, __m128i *x3,
                                            __m128i *x4, __m128i *x5,
                                            __m128i *d0, __m128i *d1,
                                            __m128i *d2, __m128i *d3,
                                            __m128i *d4, __m128i *d5) {
  __m128i w0, w1, w2, w3, w4, w5, ww0;

  // 00 01 02 03 04 05 xx xx
  // 10 11 12 13 14 15 xx xx
  // 20 21 22 23 24 25 xx xx
  // 30 31 32 33 34 35 xx xx
  // 40 41 42 43 44 45 xx xx
  // 50 51 52 53 54 55 xx xx

  w0 = _mm_unpacklo_epi16(*x0, *x1);  // 00 10 01 11 02 12 03 13
  w1 = _mm_unpacklo_epi16(*x2, *x3);  // 20 30 21 31 22 32 23 33
  w2 = _mm_unpacklo_epi16(*x4, *x5);  // 40 50 41 51 42 52 43 53

  ww0 = _mm_unpacklo_epi32(w0, w1);   // 00 10 20 30 01 11 21 31
  *d0 = _mm_unpacklo_epi64(ww0, w2);  // 00 10 20 30 40 50 41 51
  *d1 = _mm_unpackhi_epi64(ww0,
                           _mm_srli_si128(w2, 4));  // 01 11 21 31 41 51 xx xx

  ww0 = _mm_unpackhi_epi32(w0, w1);  // 02 12 22 32 03 13 23 33
  *d2 = _mm_unpacklo_epi64(ww0,
                           _mm_srli_si128(w2, 8));  // 02 12 22 32 42 52 xx xx

  w3 = _mm_unpackhi_epi16(*x0, *x1);  // 04 14 05 15 xx xx xx xx
  w4 = _mm_unpackhi_epi16(*x2, *x3);  // 24 34 25 35 xx xx xx xx
  w5 = _mm_unpackhi_epi16(*x4, *x5);  // 44 54 45 55 xx xx xx xx

  *d3 = _mm_unpackhi_epi64(ww0, _mm_srli_si128(w2, 4));  // 03 13 23 33 43 53

  ww0 = _mm_unpacklo_epi32(w3, w4);   //  04 14 24 34 05 15 25 35
  *d4 = _mm_unpacklo_epi64(ww0, w5);  //  04 14 24 34 44 54 45 55
  *d5 = _mm_unpackhi_epi64(ww0,
                           _mm_slli_si128(w5, 4));  // 05 15 25 35 45 55 xx xx
}

static INLINE void highbd_transpose4x8_8x4_low_sse2(__m128i *x0, __m128i *x1,
                                                    __m128i *x2, __m128i *x3,
                                                    __m128i *d0, __m128i *d1,
                                                    __m128i *d2, __m128i *d3) {
  __m128i zero = _mm_setzero_si128();
  __m128i w0, w1, ww0, ww1;

  w0 = _mm_unpacklo_epi16(*x0, *x1);  // 00 10 01 11 02 12 03 13
  w1 = _mm_unpacklo_epi16(*x2, *x3);  // 20 30 21 31 22 32 23 33

  ww0 = _mm_unpacklo_epi32(w0, w1);  // 00 10 20 30 01 11 21 31
  ww1 = _mm_unpackhi_epi32(w0, w1);  // 02 12 22 32 03 13 23 33

  *d0 = _mm_unpacklo_epi64(ww0, zero);  // 00 10 20 30 xx xx xx xx
  *d1 = _mm_unpackhi_epi64(ww0, zero);  // 01 11 21 31 xx xx xx xx
  *d2 = _mm_unpacklo_epi64(ww1, zero);  // 02 12 22 32 xx xx xx xx
  *d3 = _mm_unpackhi_epi64(ww1, zero);  // 03 13 23 33 xx xx xx xx
}

static INLINE void highbd_transpose4x8_8x4_high_sse2(__m128i *x0, __m128i *x1,
                                                     __m128i *x2, __m128i *x3,
                                                     __m128i *d4, __m128i *d5,
                                                     __m128i *d6, __m128i *d7) {
  __m128i w0, w1, ww2, ww3;
  __m128i zero = _mm_setzero_si128();

  w0 = _mm_unpackhi_epi16(*x0, *x1);  // 04 14 05 15 06 16 07 17
  w1 = _mm_unpackhi_epi16(*x2, *x3);  // 24 34 25 35 26 36 27 37

  ww2 = _mm_unpacklo_epi32(w0, w1);  //  04 14 24 34 05 15 25 35
  ww3 = _mm_unpackhi_epi32(w0, w1);  //  06 16 26 36 07 17 27 37

  *d4 = _mm_unpacklo_epi64(ww2, zero);  // 04 14 24 34 xx xx xx xx
  *d5 = _mm_unpackhi_epi64(ww2, zero);  // 05 15 25 35 xx xx xx xx
  *d6 = _mm_unpacklo_epi64(ww3, zero);  // 06 16 26 36 xx xx xx xx
  *d7 = _mm_unpackhi_epi64(ww3, zero);  // 07 17 27 37 xx xx xx xx
}

// here in and out pointers (x and d) should be different! we don't store their
// values inside
static INLINE void highbd_transpose4x8_8x4_sse2(__m128i *x0, __m128i *x1,
                                                __m128i *x2, __m128i *x3,
                                                __m128i *d0, __m128i *d1,
                                                __m128i *d2, __m128i *d3,
                                                __m128i *d4, __m128i *d5,
                                                __m128i *d6, __m128i *d7) {
  // input
  // x0 00 01 02 03 04 05 06 07
  // x1 10 11 12 13 14 15 16 17
  // x2 20 21 22 23 24 25 26 27
  // x3 30 31 32 33 34 35 36 37
  // output
  // 00 10 20 30 xx xx xx xx
  // 01 11 21 31 xx xx xx xx
  // 02 12 22 32 xx xx xx xx
  // 03 13 23 33 xx xx xx xx
  // 04 14 24 34 xx xx xx xx
  // 05 15 25 35 xx xx xx xx
  // 06 16 26 36 xx xx xx xx
  // 07 17 27 37 xx xx xx xx
  highbd_transpose4x8_8x4_low_sse2(x0, x1, x2, x3, d0, d1, d2, d3);
  highbd_transpose4x8_8x4_high_sse2(x0, x1, x2, x3, d4, d5, d6, d7);
}

static INLINE void highbd_transpose8x8_low_sse2(__m128i *x0, __m128i *x1,
                                                __m128i *x2, __m128i *x3,
                                                __m128i *x4, __m128i *x5,
                                                __m128i *x6, __m128i *x7,
                                                __m128i *d0, __m128i *d1,
                                                __m128i *d2, __m128i *d3) {
  __m128i w0, w1, w2, w3, ww0, ww1;
  // x0 00 01 02 03 04 05 06 07
  // x1 10 11 12 13 14 15 16 17
  // x2 20 21 22 23 24 25 26 27
  // x3 30 31 32 33 34 35 36 37
  // x4 40 41 42 43 44 45 46 47
  // x5 50 51 52 53 54 55 56 57
  // x6 60 61 62 63 64 65 66 67
  // x7 70 71 72 73 74 75 76 77

  w0 = _mm_unpacklo_epi16(*x0, *x1);  // 00 10 01 11 02 12 03 13
  w1 = _mm_unpacklo_epi16(*x2, *x3);  // 20 30 21 31 22 32 23 33
  w2 = _mm_unpacklo_epi16(*x4, *x5);  // 40 50 41 51 42 52 43 53
  w3 = _mm_unpacklo_epi16(*x6, *x7);  // 60 70 61 71 62 72 63 73

  ww0 = _mm_unpacklo_epi32(w0, w1);  // 00 10 20 30 01 11 21 31
  ww1 = _mm_unpacklo_epi32(w2, w3);  // 40 50 60 70 41 51 61 71

  *d0 = _mm_unpacklo_epi64(ww0, ww1);  // 00 10 20 30 40 50 60 70
  *d1 = _mm_unpackhi_epi64(ww0, ww1);  // 01 11 21 31 41 51 61 71

  ww0 = _mm_unpackhi_epi32(w0, w1);  // 02 12 22 32 03 13 23 33
  ww1 = _mm_unpackhi_epi32(w2, w3);  // 42 52 62 72 43 53 63 73

  *d2 = _mm_unpacklo_epi64(ww0, ww1);  // 02 12 22 32 42 52 62 72
  *d3 = _mm_unpackhi_epi64(ww0, ww1);  // 03 13 23 33 43 53 63 73
}

static INLINE void highbd_transpose8x8_high_sse2(__m128i *x0, __m128i *x1,
                                                 __m128i *x2, __m128i *x3,
                                                 __m128i *x4, __m128i *x5,
                                                 __m128i *x6, __m128i *x7,
                                                 __m128i *d4, __m128i *d5,
                                                 __m128i *d6, __m128i *d7) {
  __m128i w0, w1, w2, w3, ww0, ww1;
  // x0 00 01 02 03 04 05 06 07
  // x1 10 11 12 13 14 15 16 17
  // x2 20 21 22 23 24 25 26 27
  // x3 30 31 32 33 34 35 36 37
  // x4 40 41 42 43 44 45 46 47
  // x5 50 51 52 53 54 55 56 57
  // x6 60 61 62 63 64 65 66 67
  // x7 70 71 72 73 74 75 76 77
  w0 = _mm_unpackhi_epi16(*x0, *x1);  // 04 14 05 15 06 16 07 17
  w1 = _mm_unpackhi_epi16(*x2, *x3);  // 24 34 25 35 26 36 27 37
  w2 = _mm_unpackhi_epi16(*x4, *x5);  // 44 54 45 55 46 56 47 57
  w3 = _mm_unpackhi_epi16(*x6, *x7);  // 64 74 65 75 66 76 67 77

  ww0 = _mm_unpacklo_epi32(w0, w1);  // 04 14 24 34 05 15 25 35
  ww1 = _mm_unpacklo_epi32(w2, w3);  // 44 54 64 74 45 55 65 75

  *d4 = _mm_unpacklo_epi64(ww0, ww1);  // 04 14 24 34 44 54 64 74
  *d5 = _mm_unpackhi_epi64(ww0, ww1);  // 05 15 25 35 45 55 65 75

  ww0 = _mm_unpackhi_epi32(w0, w1);  // 06 16 26 36 07 17 27 37
  ww1 = _mm_unpackhi_epi32(w2, w3);  // 46 56 66 76 47 57 67 77

  *d6 = _mm_unpacklo_epi64(ww0, ww1);  // 06 16 26 36 46 56 66 76
  *d7 = _mm_unpackhi_epi64(ww0, ww1);  // 07 17 27 37 47 57 67 77
}

// here in and out pointers (x and d) should be different! we don't store their
// values inside
static INLINE void highbd_transpose8x8_sse2(
    __m128i *x0, __m128i *x1, __m128i *x2, __m128i *x3, __m128i *x4,
    __m128i *x5, __m128i *x6, __m128i *x7, __m128i *d0, __m128i *d1,
    __m128i *d2, __m128i *d3, __m128i *d4, __m128i *d5, __m128i *d6,
    __m128i *d7) {
  highbd_transpose8x8_low_sse2(x0, x1, x2, x3, x4, x5, x6, x7, d0, d1, d2, d3);
  highbd_transpose8x8_high_sse2(x0, x1, x2, x3, x4, x5, x6, x7, d4, d5, d6, d7);
}

// here in and out pointers (x and d arrays) should be different! we don't store
// their values inside
static INLINE void highbd_transpose8x16_sse2(
    __m128i *x0, __m128i *x1, __m128i *x2, __m128i *x3, __m128i *x4,
    __m128i *x5, __m128i *x6, __m128i *x7, __m128i *d0, __m128i *d1,
    __m128i *d2, __m128i *d3, __m128i *d4, __m128i *d5, __m128i *d6,
    __m128i *d7) {
  highbd_transpose8x8_sse2(x0, x1, x2, x3, x4, x5, x6, x7, d0, d1, d2, d3, d4,
                           d5, d6, d7);
  highbd_transpose8x8_sse2(x0 + 1, x1 + 1, x2 + 1, x3 + 1, x4 + 1, x5 + 1,
                           x6 + 1, x7 + 1, d0 + 1, d1 + 1, d2 + 1, d3 + 1,
                           d4 + 1, d5 + 1, d6 + 1, d7 + 1);
}

#endif  // _AOM_DSP_X86_LPF_COMMON_X86_H
