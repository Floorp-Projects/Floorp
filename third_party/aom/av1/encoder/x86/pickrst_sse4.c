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

#include <assert.h>
#include <emmintrin.h>
#include "aom_dsp/x86/synonyms.h"

#include "config/av1_rtcd.h"
#include "av1/common/restoration.h"
#include "av1/encoder/pickrst.h"

static INLINE void acc_stat_sse41(int32_t *dst, const uint8_t *src,
                                  const __m128i *shuffle, const __m128i *kl) {
  const __m128i s = _mm_shuffle_epi8(xx_loadu_128(src), *shuffle);
  const __m128i d0 = _mm_madd_epi16(*kl, _mm_cvtepu8_epi16(s));
  const __m128i d1 =
      _mm_madd_epi16(*kl, _mm_cvtepu8_epi16(_mm_srli_si128(s, 8)));
  const __m128i dst0 = xx_loadu_128(dst);
  const __m128i dst1 = xx_loadu_128(dst + 4);
  const __m128i r0 = _mm_add_epi32(dst0, d0);
  const __m128i r1 = _mm_add_epi32(dst1, d1);
  xx_storeu_128(dst, r0);
  xx_storeu_128(dst + 4, r1);
}

static INLINE void acc_stat_win7_one_line_sse4_1(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end,
    int dgd_stride, const __m128i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN][WIENER_WIN], int32_t M_int[WIENER_WIN][WIENER_WIN],
    int32_t H_int[WIENER_WIN2][WIENER_WIN * 8]) {
  const int wiener_win = 7;
  int j, k, l;
  for (j = h_start; j < h_end; j += 2) {
    const uint8_t *dgd_ij = dgd + j;
    const uint8_t X1 = src[j];
    const uint8_t X2 = src[j + 1];
    *sumX += X1 + X2;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        const uint8_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        const __m128i kl =
            _mm_cvtepu8_epi16(_mm_set1_epi16(*((uint16_t *)(dgd_ijk + l))));
        acc_stat_sse41(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 5 * 8, dgd_ij + 5 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 6 * 8, dgd_ij + 6 * dgd_stride, shuffle, &kl);
      }
    }
  }
}

static INLINE void compute_stats_win7_opt_sse4_1(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end, int v_start,
    int v_end, int dgd_stride, int src_stride, double *M, double *H) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int32_t M_int32[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int64_t M_int64[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int32_t H_int32[WIENER_WIN2][WIENER_WIN * 8] = { { 0 } };
  int64_t H_int64[WIENER_WIN2][WIENER_WIN * 8] = { { 0 } };
  int32_t sumY[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  const __m128i shuffle = xx_loadu_128(g_shuffle_stats_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i++) {
      acc_stat_win7_one_line_sse4_1(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX, sumY, M_int32, H_int32);
    }
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        M_int64[k][l] += M_int32[k][l];
        M_int32[k][l] = 0;
      }
    }
    for (k = 0; k < WIENER_WIN2; ++k) {
      for (l = 0; l < WIENER_WIN * 8; ++l) {
        H_int64[k][l] += H_int32[k][l];
        H_int32[k][l] = 0;
      }
    }
  }

  const double avg_square_sum = avg * avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] = M_int64[k][l] + avg_square_sum - avg * (sumX + sumY[k][l]);
      double *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int64[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] = H_int_[n * 8 + m] + avg_square_sum -
                                   avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

static INLINE void acc_stat_win5_one_line_sse4_1(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end,
    int dgd_stride, const __m128i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int32_t M_int[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int32_t H_int[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) {
  const int wiener_win = WIENER_WIN_CHROMA;
  int j, k, l;
  for (j = h_start; j < h_end; j += 2) {
    const uint8_t *dgd_ij = dgd + j;
    const uint8_t X1 = src[j];
    const uint8_t X2 = src[j + 1];
    *sumX += X1 + X2;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        const uint8_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        const __m128i kl =
            _mm_cvtepu8_epi16(_mm_set1_epi16(*((uint16_t *)(dgd_ijk + l))));
        acc_stat_sse41(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_sse41(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
      }
    }
  }
}

static INLINE void compute_stats_win5_opt_sse4_1(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end, int v_start,
    int v_end, int dgd_stride, int src_stride, double *M, double *H) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN_CHROMA;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int32_t M_int32[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int64_t M_int64[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int32_t H_int32[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8] = { { 0 } };
  int64_t H_int64[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8] = { { 0 } };
  int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  const __m128i shuffle = xx_loadu_128(g_shuffle_stats_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i++) {
      acc_stat_win5_one_line_sse4_1(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX, sumY, M_int32, H_int32);
    }
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        M_int64[k][l] += M_int32[k][l];
        M_int32[k][l] = 0;
      }
    }
    for (k = 0; k < WIENER_WIN_CHROMA * WIENER_WIN_CHROMA; ++k) {
      for (l = 0; l < WIENER_WIN_CHROMA * 8; ++l) {
        H_int64[k][l] += H_int32[k][l];
        H_int32[k][l] = 0;
      }
    }
  }

  const double avg_square_sum = avg * avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] = M_int64[k][l] + avg_square_sum - avg * (sumX + sumY[k][l]);
      double *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int64[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] = H_int_[n * 8 + m] + avg_square_sum -
                                   avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}
void av1_compute_stats_sse4_1(int wiener_win, const uint8_t *dgd,
                              const uint8_t *src, int h_start, int h_end,
                              int v_start, int v_end, int dgd_stride,
                              int src_stride, double *M, double *H) {
  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_opt_sse4_1(dgd, src, h_start, h_end, v_start, v_end,
                                  dgd_stride, src_stride, M, H);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win5_opt_sse4_1(dgd, src, h_start, h_end, v_start, v_end,
                                  dgd_stride, src_stride, M, H);
  } else {
    av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                        dgd_stride, src_stride, M, H);
  }
}

static INLINE __m128i pair_set_epi16(uint16_t a, uint16_t b) {
  return _mm_set1_epi32((int32_t)(((uint16_t)(a)) | (((uint32_t)(b)) << 16)));
}

int64_t av1_lowbd_pixel_proj_error_sse4_1(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int i, j, k;
  const int32_t shift = SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS;
  const __m128i rounding = _mm_set1_epi32(1 << (shift - 1));
  __m128i sum64 = _mm_setzero_si128();
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {
    __m128i xq_coeff = pair_set_epi16(xq[0], xq[1]);
    for (i = 0; i < height; ++i) {
      __m128i sum32 = _mm_setzero_si128();
      for (j = 0; j < width - 8; j += 8) {
        const __m128i d0 = _mm_cvtepu8_epi16(xx_loadl_64(dat + j));
        const __m128i s0 = _mm_cvtepu8_epi16(xx_loadl_64(src + j));
        const __m128i flt0_16b =
            _mm_packs_epi32(xx_loadu_128(flt0 + j), xx_loadu_128(flt0 + j + 4));
        const __m128i flt1_16b =
            _mm_packs_epi32(xx_loadu_128(flt1 + j), xx_loadu_128(flt1 + j + 4));
        const __m128i u0 = _mm_slli_epi16(d0, SGRPROJ_RST_BITS);
        const __m128i flt0_0_sub_u = _mm_sub_epi16(flt0_16b, u0);
        const __m128i flt1_0_sub_u = _mm_sub_epi16(flt1_16b, u0);
        const __m128i v0 = _mm_madd_epi16(
            xq_coeff, _mm_unpacklo_epi16(flt0_0_sub_u, flt1_0_sub_u));
        const __m128i v1 = _mm_madd_epi16(
            xq_coeff, _mm_unpackhi_epi16(flt0_0_sub_u, flt1_0_sub_u));
        const __m128i vr0 = _mm_srai_epi32(_mm_add_epi32(v0, rounding), shift);
        const __m128i vr1 = _mm_srai_epi32(_mm_add_epi32(v1, rounding), shift);
        const __m128i e0 =
            _mm_sub_epi16(_mm_add_epi16(_mm_packs_epi32(vr0, vr1), d0), s0);
        const __m128i err0 = _mm_madd_epi16(e0, e0);
        sum32 = _mm_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq[0] * (flt0[k] - u) + xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
      const __m128i sum64_0 = _mm_cvtepi32_epi64(sum32);
      const __m128i sum64_1 = _mm_cvtepi32_epi64(_mm_srli_si128(sum32, 8));
      sum64 = _mm_add_epi64(sum64, sum64_0);
      sum64 = _mm_add_epi64(sum64, sum64_1);
    }
  } else if (params->r[0] > 0) {
    __m128i xq_coeff = pair_set_epi16(xq[0], -(xq[0] << SGRPROJ_RST_BITS));
    for (i = 0; i < height; ++i) {
      __m128i sum32 = _mm_setzero_si128();
      for (j = 0; j < width - 8; j += 8) {
        const __m128i d0 = _mm_cvtepu8_epi16(xx_loadl_64(dat + j));
        const __m128i s0 = _mm_cvtepu8_epi16(xx_loadl_64(src + j));
        const __m128i flt0_16b =
            _mm_packs_epi32(xx_loadu_128(flt0 + j), xx_loadu_128(flt0 + j + 4));
        const __m128i v0 =
            _mm_madd_epi16(xq_coeff, _mm_unpacklo_epi16(flt0_16b, d0));
        const __m128i v1 =
            _mm_madd_epi16(xq_coeff, _mm_unpackhi_epi16(flt0_16b, d0));
        const __m128i vr0 = _mm_srai_epi32(_mm_add_epi32(v0, rounding), shift);
        const __m128i vr1 = _mm_srai_epi32(_mm_add_epi32(v1, rounding), shift);
        const __m128i e0 =
            _mm_sub_epi16(_mm_add_epi16(_mm_packs_epi32(vr0, vr1), d0), s0);
        const __m128i err0 = _mm_madd_epi16(e0, e0);
        sum32 = _mm_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq[0] * (flt0[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      const __m128i sum64_0 = _mm_cvtepi32_epi64(sum32);
      const __m128i sum64_1 = _mm_cvtepi32_epi64(_mm_srli_si128(sum32, 8));
      sum64 = _mm_add_epi64(sum64, sum64_0);
      sum64 = _mm_add_epi64(sum64, sum64_1);
    }
  } else if (params->r[1] > 0) {
    __m128i xq_coeff = pair_set_epi16(xq[1], -(xq[1] << SGRPROJ_RST_BITS));
    for (i = 0; i < height; ++i) {
      __m128i sum32 = _mm_setzero_si128();
      for (j = 0; j < width - 8; j += 8) {
        const __m128i d0 = _mm_cvtepu8_epi16(xx_loadl_64(dat + j));
        const __m128i s0 = _mm_cvtepu8_epi16(xx_loadl_64(src + j));
        const __m128i flt1_16b =
            _mm_packs_epi32(xx_loadu_128(flt1 + j), xx_loadu_128(flt1 + j + 4));
        const __m128i v0 =
            _mm_madd_epi16(xq_coeff, _mm_unpacklo_epi16(flt1_16b, d0));
        const __m128i v1 =
            _mm_madd_epi16(xq_coeff, _mm_unpackhi_epi16(flt1_16b, d0));
        const __m128i vr0 = _mm_srai_epi32(_mm_add_epi32(v0, rounding), shift);
        const __m128i vr1 = _mm_srai_epi32(_mm_add_epi32(v1, rounding), shift);
        const __m128i e0 =
            _mm_sub_epi16(_mm_add_epi16(_mm_packs_epi32(vr0, vr1), d0), s0);
        const __m128i err0 = _mm_madd_epi16(e0, e0);
        sum32 = _mm_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt1 += flt1_stride;
      const __m128i sum64_0 = _mm_cvtepi32_epi64(sum32);
      const __m128i sum64_1 = _mm_cvtepi32_epi64(_mm_srli_si128(sum32, 8));
      sum64 = _mm_add_epi64(sum64, sum64_0);
      sum64 = _mm_add_epi64(sum64, sum64_1);
    }
  } else {
    __m128i sum32 = _mm_setzero_si128();
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width - 16; j += 16) {
        const __m128i d = xx_loadu_128(dat + j);
        const __m128i s = xx_loadu_128(src + j);
        const __m128i d0 = _mm_cvtepu8_epi16(d);
        const __m128i d1 = _mm_cvtepu8_epi16(_mm_srli_si128(d, 8));
        const __m128i s0 = _mm_cvtepu8_epi16(s);
        const __m128i s1 = _mm_cvtepu8_epi16(_mm_srli_si128(s, 8));
        const __m128i diff0 = _mm_sub_epi16(d0, s0);
        const __m128i diff1 = _mm_sub_epi16(d1, s1);
        const __m128i err0 = _mm_madd_epi16(diff0, diff0);
        const __m128i err1 = _mm_madd_epi16(diff1, diff1);
        sum32 = _mm_add_epi32(sum32, err0);
        sum32 = _mm_add_epi32(sum32, err1);
      }
      for (k = j; k < width; ++k) {
        const int32_t e = (int32_t)(dat[k]) - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
    }
    const __m128i sum64_0 = _mm_cvtepi32_epi64(sum32);
    const __m128i sum64_1 = _mm_cvtepi32_epi64(_mm_srli_si128(sum32, 8));
    sum64 = _mm_add_epi64(sum64_0, sum64_1);
  }
  int64_t sum[2];
  xx_storeu_128(sum, sum64);
  err += sum[0] + sum[1];
  return err;
}
