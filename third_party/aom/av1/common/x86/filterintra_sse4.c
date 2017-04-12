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

#include <smmintrin.h>

#include "./av1_rtcd.h"
#include "aom_ports/mem.h"
#include "av1/common/enums.h"
#include "av1/common/reconintra.h"

#if USE_3TAP_INTRA_FILTER
void filterintra_sse4_3tap_dummy_func(void);
void filterintra_sse4_3tap_dummy_func(void) {}
#else

static INLINE void AddPixelsSmall(const uint8_t *above, const uint8_t *left,
                                  __m128i *sum) {
  const __m128i a = _mm_loadu_si128((const __m128i *)above);
  const __m128i l = _mm_loadu_si128((const __m128i *)left);
  const __m128i zero = _mm_setzero_si128();

  __m128i u0 = _mm_unpacklo_epi8(a, zero);
  __m128i u1 = _mm_unpacklo_epi8(l, zero);

  sum[0] = _mm_add_epi16(u0, u1);
}

static INLINE int GetMeanValue4x4(const uint8_t *above, const uint8_t *left,
                                  __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint16_t sum_value;

  AddPixelsSmall(above, left, &sum_vector);

  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values
  u = _mm_srli_si128(sum_vector, 2);
  sum_vector = _mm_add_epi16(sum_vector, u);

  sum_value = _mm_extract_epi16(sum_vector, 0);
  sum_value += 4;
  sum_value >>= 3;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

static INLINE int GetMeanValue8x8(const uint8_t *above, const uint8_t *left,
                                  __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint16_t sum_value;

  AddPixelsSmall(above, left, &sum_vector);

  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 4 values
  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values

  u = _mm_srli_si128(sum_vector, 2);
  sum_vector = _mm_add_epi16(sum_vector, u);

  sum_value = _mm_extract_epi16(sum_vector, 0);
  sum_value += 8;
  sum_value >>= 4;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

static INLINE void AddPixelsLarge(const uint8_t *above, const uint8_t *left,
                                  __m128i *sum) {
  const __m128i a = _mm_loadu_si128((const __m128i *)above);
  const __m128i l = _mm_loadu_si128((const __m128i *)left);
  const __m128i zero = _mm_setzero_si128();

  __m128i u0 = _mm_unpacklo_epi8(a, zero);
  __m128i u1 = _mm_unpacklo_epi8(l, zero);

  sum[0] = _mm_add_epi16(u0, u1);

  u0 = _mm_unpackhi_epi8(a, zero);
  u1 = _mm_unpackhi_epi8(l, zero);

  sum[0] = _mm_add_epi16(sum[0], u0);
  sum[0] = _mm_add_epi16(sum[0], u1);
}

static INLINE int GetMeanValue16x16(const uint8_t *above, const uint8_t *left,
                                    __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint16_t sum_value;

  AddPixelsLarge(above, left, &sum_vector);

  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 4 values
  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values

  u = _mm_srli_si128(sum_vector, 2);
  sum_vector = _mm_add_epi16(sum_vector, u);

  sum_value = _mm_extract_epi16(sum_vector, 0);
  sum_value += 16;
  sum_value >>= 5;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

static INLINE int GetMeanValue32x32(const uint8_t *above, const uint8_t *left,
                                    __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector[2], u;
  uint16_t sum_value;

  AddPixelsLarge(above, left, &sum_vector[0]);
  AddPixelsLarge(above + 16, left + 16, &sum_vector[1]);

  sum_vector[0] = _mm_add_epi16(sum_vector[0], sum_vector[1]);
  sum_vector[0] = _mm_hadd_epi16(sum_vector[0], zero);  // still has 4 values
  sum_vector[0] = _mm_hadd_epi16(sum_vector[0], zero);  // still has 2 values

  u = _mm_srli_si128(sum_vector[0], 2);
  sum_vector[0] = _mm_add_epi16(sum_vector[0], u);

  sum_value = _mm_extract_epi16(sum_vector[0], 0);
  sum_value += 32;
  sum_value >>= 6;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

// Note:
//  params[4] : mean value, 4 int32_t repetition
//
static INLINE int CalcRefPixelsMeanValue(const uint8_t *above,
                                         const uint8_t *left, int bs,
                                         __m128i *params) {
  int meanValue = 0;
  switch (bs) {
    case 4: meanValue = GetMeanValue4x4(above, left, params); break;
    case 8: meanValue = GetMeanValue8x8(above, left, params); break;
    case 16: meanValue = GetMeanValue16x16(above, left, params); break;
    case 32: meanValue = GetMeanValue32x32(above, left, params); break;
    default: assert(0);
  }
  return meanValue;
}

// Note:
//  params[0-3] : 4-tap filter coefficients (int32_t per coefficient)
//
static INLINE void GetIntraFilterParams(int bs, int mode, __m128i *params) {
  const TX_SIZE tx_size =
      (bs == 32) ? TX_32X32
                 : ((bs == 16) ? TX_16X16 : ((bs == 8) ? TX_8X8 : (TX_4X4)));
  // c0
  params[0] = _mm_set_epi32(av1_filter_intra_taps_4[tx_size][mode][0],
                            av1_filter_intra_taps_4[tx_size][mode][0],
                            av1_filter_intra_taps_4[tx_size][mode][0],
                            av1_filter_intra_taps_4[tx_size][mode][0]);
  // c1
  params[1] = _mm_set_epi32(av1_filter_intra_taps_4[tx_size][mode][1],
                            av1_filter_intra_taps_4[tx_size][mode][1],
                            av1_filter_intra_taps_4[tx_size][mode][1],
                            av1_filter_intra_taps_4[tx_size][mode][1]);
  // c2
  params[2] = _mm_set_epi32(av1_filter_intra_taps_4[tx_size][mode][2],
                            av1_filter_intra_taps_4[tx_size][mode][2],
                            av1_filter_intra_taps_4[tx_size][mode][2],
                            av1_filter_intra_taps_4[tx_size][mode][2]);
  // c3
  params[3] = _mm_set_epi32(av1_filter_intra_taps_4[tx_size][mode][3],
                            av1_filter_intra_taps_4[tx_size][mode][3],
                            av1_filter_intra_taps_4[tx_size][mode][3],
                            av1_filter_intra_taps_4[tx_size][mode][3]);
}

static const int maxBlkSize = 32;

static INLINE void SavePred4x4(int *pred, const __m128i *mean, uint8_t *dst,
                               ptrdiff_t stride) {
  const int predStride = (maxBlkSize << 1) + 1;
  __m128i p0 = _mm_loadu_si128((const __m128i *)pred);
  __m128i p1 = _mm_loadu_si128((const __m128i *)(pred + predStride));
  __m128i p2 = _mm_loadu_si128((const __m128i *)(pred + 2 * predStride));
  __m128i p3 = _mm_loadu_si128((const __m128i *)(pred + 3 * predStride));

  p0 = _mm_add_epi32(p0, mean[0]);
  p1 = _mm_add_epi32(p1, mean[0]);
  p2 = _mm_add_epi32(p2, mean[0]);
  p3 = _mm_add_epi32(p3, mean[0]);

  p0 = _mm_packus_epi32(p0, p1);
  p1 = _mm_packus_epi32(p2, p3);
  p0 = _mm_packus_epi16(p0, p1);

  *((int *)dst) = _mm_cvtsi128_si32(p0);
  p0 = _mm_srli_si128(p0, 4);
  *((int *)(dst + stride)) = _mm_cvtsi128_si32(p0);
  p0 = _mm_srli_si128(p0, 4);
  *((int *)(dst + 2 * stride)) = _mm_cvtsi128_si32(p0);
  p0 = _mm_srli_si128(p0, 4);
  *((int *)(dst + 3 * stride)) = _mm_cvtsi128_si32(p0);
}

static void SavePred8x8(int *pred, const __m128i *mean, uint8_t *dst,
                        ptrdiff_t stride) {
  const int predStride = (maxBlkSize << 1) + 1;
  __m128i p0, p1, p2, p3;
  int r = 0;

  while (r < 8) {
    p0 = _mm_loadu_si128((const __m128i *)(pred + r * predStride));
    p1 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 4));
    r += 1;
    p2 = _mm_loadu_si128((const __m128i *)(pred + r * predStride));
    p3 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 4));

    p0 = _mm_add_epi32(p0, mean[0]);
    p1 = _mm_add_epi32(p1, mean[0]);
    p2 = _mm_add_epi32(p2, mean[0]);
    p3 = _mm_add_epi32(p3, mean[0]);

    p0 = _mm_packus_epi32(p0, p1);
    p1 = _mm_packus_epi32(p2, p3);
    p0 = _mm_packus_epi16(p0, p1);

    _mm_storel_epi64((__m128i *)dst, p0);
    dst += stride;
    p0 = _mm_srli_si128(p0, 8);
    _mm_storel_epi64((__m128i *)dst, p0);
    dst += stride;
    r += 1;
  }
}

static void SavePred16x16(int *pred, const __m128i *mean, uint8_t *dst,
                          ptrdiff_t stride) {
  const int predStride = (maxBlkSize << 1) + 1;
  __m128i p0, p1, p2, p3;
  int r = 0;

  while (r < 16) {
    p0 = _mm_loadu_si128((const __m128i *)(pred + r * predStride));
    p1 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 4));
    p2 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 8));
    p3 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 12));

    p0 = _mm_add_epi32(p0, mean[0]);
    p1 = _mm_add_epi32(p1, mean[0]);
    p2 = _mm_add_epi32(p2, mean[0]);
    p3 = _mm_add_epi32(p3, mean[0]);

    p0 = _mm_packus_epi32(p0, p1);
    p1 = _mm_packus_epi32(p2, p3);
    p0 = _mm_packus_epi16(p0, p1);

    _mm_storel_epi64((__m128i *)dst, p0);
    p0 = _mm_srli_si128(p0, 8);
    _mm_storel_epi64((__m128i *)(dst + 8), p0);
    dst += stride;
    r += 1;
  }
}

static void SavePred32x32(int *pred, const __m128i *mean, uint8_t *dst,
                          ptrdiff_t stride) {
  const int predStride = (maxBlkSize << 1) + 1;
  __m128i p0, p1, p2, p3, p4, p5, p6, p7;
  int r = 0;

  while (r < 32) {
    p0 = _mm_loadu_si128((const __m128i *)(pred + r * predStride));
    p1 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 4));
    p2 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 8));
    p3 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 12));

    p4 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 16));
    p5 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 20));
    p6 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 24));
    p7 = _mm_loadu_si128((const __m128i *)(pred + r * predStride + 28));

    p0 = _mm_add_epi32(p0, mean[0]);
    p1 = _mm_add_epi32(p1, mean[0]);
    p2 = _mm_add_epi32(p2, mean[0]);
    p3 = _mm_add_epi32(p3, mean[0]);

    p4 = _mm_add_epi32(p4, mean[0]);
    p5 = _mm_add_epi32(p5, mean[0]);
    p6 = _mm_add_epi32(p6, mean[0]);
    p7 = _mm_add_epi32(p7, mean[0]);

    p0 = _mm_packus_epi32(p0, p1);
    p1 = _mm_packus_epi32(p2, p3);
    p0 = _mm_packus_epi16(p0, p1);

    p4 = _mm_packus_epi32(p4, p5);
    p5 = _mm_packus_epi32(p6, p7);
    p4 = _mm_packus_epi16(p4, p5);

    _mm_storel_epi64((__m128i *)dst, p0);
    p0 = _mm_srli_si128(p0, 8);
    _mm_storel_epi64((__m128i *)(dst + 8), p0);

    _mm_storel_epi64((__m128i *)(dst + 16), p4);
    p4 = _mm_srli_si128(p4, 8);
    _mm_storel_epi64((__m128i *)(dst + 24), p4);

    dst += stride;
    r += 1;
  }
}

static void SavePrediction(int *pred, const __m128i *mean, int bs, uint8_t *dst,
                           ptrdiff_t stride) {
  switch (bs) {
    case 4: SavePred4x4(pred, mean, dst, stride); break;
    case 8: SavePred8x8(pred, mean, dst, stride); break;
    case 16: SavePred16x16(pred, mean, dst, stride); break;
    case 32: SavePred32x32(pred, mean, dst, stride); break;
    default: assert(0);
  }
}

typedef void (*ProducePixelsFunc)(__m128i *p, const __m128i *prm, int *pred,
                                  const int predStride);

static void ProduceFourPixels(__m128i *p, const __m128i *prm, int *pred,
                              const int predStride) {
  __m128i u0, u1, u2;
  int c0 = _mm_extract_epi32(prm[1], 0);
  int x = *(pred + predStride);
  int sum;

  u0 = _mm_mullo_epi32(p[0], prm[2]);
  u1 = _mm_mullo_epi32(p[1], prm[0]);
  u2 = _mm_mullo_epi32(p[2], prm[3]);

  u0 = _mm_add_epi32(u0, u1);
  u0 = _mm_add_epi32(u0, u2);

  sum = _mm_extract_epi32(u0, 0);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 1) = x;

  sum = _mm_extract_epi32(u0, 1);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 2) = x;

  sum = _mm_extract_epi32(u0, 2);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 3) = x;

  sum = _mm_extract_epi32(u0, 3);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 4) = x;
}

static void ProduceThreePixels(__m128i *p, const __m128i *prm, int *pred,
                               const int predStride) {
  __m128i u0, u1, u2;
  int c0 = _mm_extract_epi32(prm[1], 0);
  int x = *(pred + predStride);
  int sum;

  u0 = _mm_mullo_epi32(p[0], prm[2]);
  u1 = _mm_mullo_epi32(p[1], prm[0]);
  u2 = _mm_mullo_epi32(p[2], prm[3]);

  u0 = _mm_add_epi32(u0, u1);
  u0 = _mm_add_epi32(u0, u2);

  sum = _mm_extract_epi32(u0, 0);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 1) = x;

  sum = _mm_extract_epi32(u0, 1);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 2) = x;

  sum = _mm_extract_epi32(u0, 2);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 3) = x;
}

static void ProduceTwoPixels(__m128i *p, const __m128i *prm, int *pred,
                             const int predStride) {
  __m128i u0, u1, u2;
  int c0 = _mm_extract_epi32(prm[1], 0);
  int x = *(pred + predStride);
  int sum;

  u0 = _mm_mullo_epi32(p[0], prm[2]);
  u1 = _mm_mullo_epi32(p[1], prm[0]);
  u2 = _mm_mullo_epi32(p[2], prm[3]);

  u0 = _mm_add_epi32(u0, u1);
  u0 = _mm_add_epi32(u0, u2);

  sum = _mm_extract_epi32(u0, 0);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 1) = x;

  sum = _mm_extract_epi32(u0, 1);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 2) = x;
}

static void ProduceOnePixels(__m128i *p, const __m128i *prm, int *pred,
                             const int predStride) {
  __m128i u0, u1, u2;
  int c0 = _mm_extract_epi32(prm[1], 0);
  int x = *(pred + predStride);
  int sum;

  u0 = _mm_mullo_epi32(p[0], prm[2]);
  u1 = _mm_mullo_epi32(p[1], prm[0]);
  u2 = _mm_mullo_epi32(p[2], prm[3]);

  u0 = _mm_add_epi32(u0, u1);
  u0 = _mm_add_epi32(u0, u2);

  sum = _mm_extract_epi32(u0, 0);
  sum += c0 * x;
  x = ROUND_POWER_OF_TWO_SIGNED(sum, FILTER_INTRA_PREC_BITS);
  *(pred + predStride + 1) = x;
}

static ProducePixelsFunc prodPixelsFuncTab[4] = {
  ProduceOnePixels, ProduceTwoPixels, ProduceThreePixels, ProduceFourPixels
};

static void ProducePixels(int *pred, const __m128i *prm, int remain) {
  __m128i p[3];
  const int predStride = (maxBlkSize << 1) + 1;
  int index;

  p[0] = _mm_loadu_si128((const __m128i *)pred);
  p[1] = _mm_loadu_si128((const __m128i *)(pred + 1));
  p[2] = _mm_loadu_si128((const __m128i *)(pred + 2));

  if (remain <= 2) {
    return;
  }
  if (remain > 5) {
    index = 3;
  } else {
    index = remain - 3;
  }
  prodPixelsFuncTab[index](p, prm, pred, predStride);
}

// Note:
//  At column index c, the remaining pixels are R = 2 * bs + 1 - r - c
//  the number of pixels to produce is R - 2 = 2 * bs - r - c - 1
static void GeneratePrediction(const uint8_t *above, const uint8_t *left,
                               const int bs, const __m128i *prm, int meanValue,
                               uint8_t *dst, ptrdiff_t stride) {
  int pred[33][65];
  int r, c, colBound;
  int remainings;

  for (r = 0; r < bs; ++r) {
    pred[r + 1][0] = (int)left[r] - meanValue;
  }

  above -= 1;
  for (c = 0; c < 2 * bs + 1; ++c) {
    pred[0][c] = (int)above[c] - meanValue;
  }

  r = 0;
  c = 0;
  while (r < bs) {
    colBound = (bs << 1) - r;
    for (c = 0; c < colBound; c += 4) {
      remainings = colBound - c + 1;
      ProducePixels(&pred[r][c], prm, remainings);
    }
    r += 1;
  }

  SavePrediction(&pred[1][1], &prm[4], bs, dst, stride);
}

static void FilterPrediction(const uint8_t *above, const uint8_t *left, int bs,
                             __m128i *prm, uint8_t *dst, ptrdiff_t stride) {
  int meanValue = 0;
  meanValue = CalcRefPixelsMeanValue(above, left, bs, &prm[4]);
  GeneratePrediction(above, left, bs, prm, meanValue, dst, stride);
}

void av1_dc_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, DC_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_v_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, V_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_h_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, H_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d45_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D45_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d135_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D135_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d117_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D117_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d153_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D153_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d207_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D207_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_d63_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D63_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

void av1_tm_filter_predictor_sse4_1(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i prm[5];
  GetIntraFilterParams(bs, TM_PRED, &prm[0]);
  FilterPrediction(above, left, bs, prm, dst, stride);
}

// ============== High Bit Depth ==============
#if CONFIG_HIGHBITDEPTH
static INLINE int HighbdGetMeanValue4x4(const uint16_t *above,
                                        const uint16_t *left, const int bd,
                                        __m128i *params) {
  const __m128i a = _mm_loadu_si128((const __m128i *)above);
  const __m128i l = _mm_loadu_si128((const __m128i *)left);
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint16_t sum_value;
  (void)bd;

  sum_vector = _mm_add_epi16(a, l);

  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values
  u = _mm_srli_si128(sum_vector, 2);
  sum_vector = _mm_add_epi16(sum_vector, u);

  sum_value = _mm_extract_epi16(sum_vector, 0);
  sum_value += 4;
  sum_value >>= 3;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

static INLINE int HighbdGetMeanValue8x8(const uint16_t *above,
                                        const uint16_t *left, const int bd,
                                        __m128i *params) {
  const __m128i a = _mm_loadu_si128((const __m128i *)above);
  const __m128i l = _mm_loadu_si128((const __m128i *)left);
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint16_t sum_value;
  (void)bd;

  sum_vector = _mm_add_epi16(a, l);

  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 4 values
  sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values

  u = _mm_srli_si128(sum_vector, 2);
  sum_vector = _mm_add_epi16(sum_vector, u);

  sum_value = _mm_extract_epi16(sum_vector, 0);
  sum_value += 8;
  sum_value >>= 4;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

// Note:
//  Process 16 pixels above and left, 10-bit depth
//  Add to the last 8 pixels sum
static INLINE void AddPixels10bit(const uint16_t *above, const uint16_t *left,
                                  __m128i *sum) {
  __m128i a = _mm_loadu_si128((const __m128i *)above);
  __m128i l = _mm_loadu_si128((const __m128i *)left);
  sum[0] = _mm_add_epi16(a, l);
  a = _mm_loadu_si128((const __m128i *)(above + 8));
  l = _mm_loadu_si128((const __m128i *)(left + 8));
  sum[0] = _mm_add_epi16(sum[0], a);
  sum[0] = _mm_add_epi16(sum[0], l);
}

// Note:
//  Process 16 pixels above and left, 12-bit depth
//  Add to the last 8 pixels sum
static INLINE void AddPixels12bit(const uint16_t *above, const uint16_t *left,
                                  __m128i *sum) {
  __m128i a = _mm_loadu_si128((const __m128i *)above);
  __m128i l = _mm_loadu_si128((const __m128i *)left);
  const __m128i zero = _mm_setzero_si128();
  __m128i v0, v1;

  v0 = _mm_unpacklo_epi16(a, zero);
  v1 = _mm_unpacklo_epi16(l, zero);
  sum[0] = _mm_add_epi32(v0, v1);

  v0 = _mm_unpackhi_epi16(a, zero);
  v1 = _mm_unpackhi_epi16(l, zero);
  sum[0] = _mm_add_epi32(sum[0], v0);
  sum[0] = _mm_add_epi32(sum[0], v1);

  a = _mm_loadu_si128((const __m128i *)(above + 8));
  l = _mm_loadu_si128((const __m128i *)(left + 8));

  v0 = _mm_unpacklo_epi16(a, zero);
  v1 = _mm_unpacklo_epi16(l, zero);
  sum[0] = _mm_add_epi32(sum[0], v0);
  sum[0] = _mm_add_epi32(sum[0], v1);

  v0 = _mm_unpackhi_epi16(a, zero);
  v1 = _mm_unpackhi_epi16(l, zero);
  sum[0] = _mm_add_epi32(sum[0], v0);
  sum[0] = _mm_add_epi32(sum[0], v1);
}

static INLINE int HighbdGetMeanValue16x16(const uint16_t *above,
                                          const uint16_t *left, const int bd,
                                          __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector, u;
  uint32_t sum_value = 0;

  if (10 == bd) {
    AddPixels10bit(above, left, &sum_vector);
    sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 4 values
    sum_vector = _mm_hadd_epi16(sum_vector, zero);  // still has 2 values

    u = _mm_srli_si128(sum_vector, 2);
    sum_vector = _mm_add_epi16(sum_vector, u);
    sum_value = _mm_extract_epi16(sum_vector, 0);
  } else if (12 == bd) {
    AddPixels12bit(above, left, &sum_vector);

    sum_vector = _mm_hadd_epi32(sum_vector, zero);
    u = _mm_srli_si128(sum_vector, 4);
    sum_vector = _mm_add_epi32(u, sum_vector);
    sum_value = _mm_extract_epi32(sum_vector, 0);
  }

  sum_value += 16;
  sum_value >>= 5;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

static INLINE int HighbdGetMeanValue32x32(const uint16_t *above,
                                          const uint16_t *left, const int bd,
                                          __m128i *params) {
  const __m128i zero = _mm_setzero_si128();
  __m128i sum_vector[2], u;
  uint32_t sum_value = 0;

  if (10 == bd) {
    AddPixels10bit(above, left, &sum_vector[0]);
    AddPixels10bit(above + 16, left + 16, &sum_vector[1]);

    sum_vector[0] = _mm_add_epi16(sum_vector[0], sum_vector[1]);
    sum_vector[0] = _mm_hadd_epi16(sum_vector[0], zero);  // still has 4 values
    sum_vector[0] = _mm_hadd_epi16(sum_vector[0], zero);  // still has 2 values

    u = _mm_srli_si128(sum_vector[0], 2);
    sum_vector[0] = _mm_add_epi16(sum_vector[0], u);
    sum_value = _mm_extract_epi16(sum_vector[0], 0);
  } else if (12 == bd) {
    AddPixels12bit(above, left, &sum_vector[0]);
    AddPixels12bit(above + 16, left + 16, &sum_vector[1]);

    sum_vector[0] = _mm_add_epi32(sum_vector[0], sum_vector[1]);
    sum_vector[0] = _mm_hadd_epi32(sum_vector[0], zero);
    u = _mm_srli_si128(sum_vector[0], 4);
    sum_vector[0] = _mm_add_epi32(u, sum_vector[0]);
    sum_value = _mm_extract_epi32(sum_vector[0], 0);
  }

  sum_value += 32;
  sum_value >>= 6;
  *params = _mm_set1_epi32(sum_value);
  return sum_value;
}

// Note:
//  params[4] : mean value, 4 int32_t repetition
//
static INLINE int HighbdCalcRefPixelsMeanValue(const uint16_t *above,
                                               const uint16_t *left, int bs,
                                               const int bd, __m128i *params) {
  int meanValue = 0;
  switch (bs) {
    case 4: meanValue = HighbdGetMeanValue4x4(above, left, bd, params); break;
    case 8: meanValue = HighbdGetMeanValue8x8(above, left, bd, params); break;
    case 16:
      meanValue = HighbdGetMeanValue16x16(above, left, bd, params);
      break;
    case 32:
      meanValue = HighbdGetMeanValue32x32(above, left, bd, params);
      break;
    default: assert(0);
  }
  return meanValue;
}

// Note:
//  At column index c, the remaining pixels are R = 2 * bs + 1 - r - c
//  the number of pixels to produce is R - 2 = 2 * bs - r - c - 1
static void HighbdGeneratePrediction(const uint16_t *above,
                                     const uint16_t *left, const int bs,
                                     const int bd, const __m128i *prm,
                                     int meanValue, uint16_t *dst,
                                     ptrdiff_t stride) {
  int pred[33][65];
  int r, c, colBound;
  int remainings;
  int ipred;

  for (r = 0; r < bs; ++r) {
    pred[r + 1][0] = (int)left[r] - meanValue;
  }

  above -= 1;
  for (c = 0; c < 2 * bs + 1; ++c) {
    pred[0][c] = (int)above[c] - meanValue;
  }

  r = 0;
  c = 0;
  while (r < bs) {
    colBound = (bs << 1) - r;
    for (c = 0; c < colBound; c += 4) {
      remainings = colBound - c + 1;
      ProducePixels(&pred[r][c], prm, remainings);
    }
    r += 1;
  }

  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      ipred = pred[r + 1][c + 1] + meanValue;
      dst[c] = clip_pixel_highbd(ipred, bd);
    }
    dst += stride;
  }
}

static void HighbdFilterPrediction(const uint16_t *above, const uint16_t *left,
                                   int bs, const int bd, __m128i *prm,
                                   uint16_t *dst, ptrdiff_t stride) {
  int meanValue = 0;
  meanValue = HighbdCalcRefPixelsMeanValue(above, left, bs, bd, &prm[4]);
  HighbdGeneratePrediction(above, left, bs, bd, prm, meanValue, dst, stride);
}

void av1_highbd_dc_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, DC_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_v_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                          int bs, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, V_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_h_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                          int bs, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, H_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d45_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                            int bs, const uint16_t *above,
                                            const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D45_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d135_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                             int bs, const uint16_t *above,
                                             const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D135_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d117_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                             int bs, const uint16_t *above,
                                             const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D117_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d153_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                             int bs, const uint16_t *above,
                                             const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D153_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d207_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                             int bs, const uint16_t *above,
                                             const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D207_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_d63_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                            int bs, const uint16_t *above,
                                            const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, D63_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}

void av1_highbd_tm_filter_predictor_sse4_1(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  __m128i prm[5];
  GetIntraFilterParams(bs, TM_PRED, &prm[0]);
  HighbdFilterPrediction(above, left, bs, bd, prm, dst, stride);
}
#endif  // CONFIG_HIGHBITDEPTH

#endif  // USE_3TAP_INTRA_FILTER
