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

#include <immintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/x86/common_avx2.h"
#include "aom_dsp/x86/lpf_common_sse2.h"
#include "aom/aom_integer.h"

#if !CONFIG_PARALLEL_DEBLOCKING || !CONFIG_CB4X4
static INLINE void get_limit(const uint8_t *bl, const uint8_t *l,
                             const uint8_t *t, int bd, __m256i *blt,
                             __m256i *lt, __m256i *thr) {
  const int shift = bd - 8;
  const __m128i zero = _mm_setzero_si128();

  __m128i x = _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)bl), zero);
  __m256i y = _mm256_inserti128_si256(_mm256_castsi128_si256(x), x, 1);
  *blt = _mm256_slli_epi16(y, shift);

  x = _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)l), zero);
  y = _mm256_inserti128_si256(_mm256_castsi128_si256(x), x, 1);
  *lt = _mm256_slli_epi16(y, shift);

  x = _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)t), zero);
  y = _mm256_inserti128_si256(_mm256_castsi128_si256(x), x, 1);
  *thr = _mm256_slli_epi16(y, shift);
}

static INLINE void load_highbd_pixel(const uint16_t *s, int size, int pitch,
                                     __m256i *p, __m256i *q) {
  int i;
  for (i = 0; i < size; i++) {
    p[i] = _mm256_loadu_si256((__m256i *)(s - (i + 1) * pitch));
    q[i] = _mm256_loadu_si256((__m256i *)(s + i * pitch));
  }
}

static INLINE void highbd_hev_mask(const __m256i *p, const __m256i *q,
                                   const __m256i *t, __m256i *hev) {
  const __m256i abs_p1p0 = _mm256_abs_epi16(_mm256_sub_epi16(p[1], p[0]));
  const __m256i abs_q1q0 = _mm256_abs_epi16(_mm256_sub_epi16(q[1], q[0]));
  __m256i h = _mm256_max_epi16(abs_p1p0, abs_q1q0);
  h = _mm256_subs_epu16(h, *t);

  const __m256i ffff = _mm256_set1_epi16(0xFFFF);
  const __m256i zero = _mm256_setzero_si256();
  *hev = _mm256_xor_si256(_mm256_cmpeq_epi16(h, zero), ffff);
}

static INLINE void highbd_filter_mask(const __m256i *p, const __m256i *q,
                                      const __m256i *l, const __m256i *bl,
                                      __m256i *mask) {
  __m256i abs_p0q0 = _mm256_abs_epi16(_mm256_sub_epi16(p[0], q[0]));
  __m256i abs_p1q1 = _mm256_abs_epi16(_mm256_sub_epi16(p[1], q[1]));
  abs_p0q0 = _mm256_adds_epu16(abs_p0q0, abs_p0q0);
  abs_p1q1 = _mm256_srli_epi16(abs_p1q1, 1);

  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i ffff = _mm256_set1_epi16(0xFFFF);
  __m256i max = _mm256_subs_epu16(_mm256_adds_epu16(abs_p0q0, abs_p1q1), *bl);
  max = _mm256_xor_si256(_mm256_cmpeq_epi16(max, zero), ffff);
  max = _mm256_and_si256(max, _mm256_adds_epu16(*l, one));

  int i;
  for (i = 1; i < 4; ++i) {
    max = _mm256_max_epi16(max,
                           _mm256_abs_epi16(_mm256_sub_epi16(p[i], p[i - 1])));
    max = _mm256_max_epi16(max,
                           _mm256_abs_epi16(_mm256_sub_epi16(q[i], q[i - 1])));
  }
  max = _mm256_subs_epu16(max, *l);
  *mask = _mm256_cmpeq_epi16(max, zero);  // return ~mask
}

static INLINE void flat_mask_internal(const __m256i *th, const __m256i *p,
                                      const __m256i *q, int bd, int start,
                                      int end, __m256i *flat) {
  __m256i max = _mm256_setzero_si256();
  int i;
  for (i = start; i < end; ++i) {
    max = _mm256_max_epi16(max, _mm256_abs_epi16(_mm256_sub_epi16(p[i], p[0])));
    max = _mm256_max_epi16(max, _mm256_abs_epi16(_mm256_sub_epi16(q[i], q[0])));
  }

  __m256i ft;
  if (bd == 8)
    ft = _mm256_subs_epu16(max, *th);
  else if (bd == 10)
    ft = _mm256_subs_epu16(max, _mm256_slli_epi16(*th, 2));
  else  // bd == 12
    ft = _mm256_subs_epu16(max, _mm256_slli_epi16(*th, 4));

  const __m256i zero = _mm256_setzero_si256();
  *flat = _mm256_cmpeq_epi16(ft, zero);
}

// Note:
//  Access p[3-1], p[0], and q[3-1], q[0]
static INLINE void highbd_flat_mask4(const __m256i *th, const __m256i *p,
                                     const __m256i *q, __m256i *flat, int bd) {
  // check the distance 1,2,3 against 0
  flat_mask_internal(th, p, q, bd, 1, 4, flat);
}

// Note:
//  access p[7-4], p[0], and q[7-4], q[0]
static INLINE void highbd_flat_mask5(const __m256i *th, const __m256i *p,
                                     const __m256i *q, __m256i *flat, int bd) {
  flat_mask_internal(th, p, q, bd, 4, 8, flat);
}

static INLINE void pixel_clamp(const __m256i *min, const __m256i *max,
                               __m256i *pixel) {
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(*pixel, *max);
  clamped = _mm256_andnot_si256(mask, *pixel);
  mask = _mm256_and_si256(mask, *max);
  clamped = _mm256_or_si256(mask, clamped);

  mask = _mm256_cmpgt_epi16(clamped, *min);
  clamped = _mm256_and_si256(mask, clamped);
  mask = _mm256_andnot_si256(mask, *min);
  *pixel = _mm256_or_si256(clamped, mask);
}

static INLINE void highbd_filter4(__m256i *p, __m256i *q, const __m256i *mask,
                                  const __m256i *th, int bd, __m256i *ps,
                                  __m256i *qs) {
  __m256i t80;
  if (bd == 8)
    t80 = _mm256_set1_epi16(0x80);
  else if (bd == 10)
    t80 = _mm256_set1_epi16(0x200);
  else  // bd == 12
    t80 = _mm256_set1_epi16(0x800);

  __m256i ps0 = _mm256_subs_epi16(p[0], t80);
  __m256i ps1 = _mm256_subs_epi16(p[1], t80);
  __m256i qs0 = _mm256_subs_epi16(q[0], t80);
  __m256i qs1 = _mm256_subs_epi16(q[1], t80);

  const __m256i one = _mm256_set1_epi16(1);
  const __m256i pmax = _mm256_subs_epi16(
      _mm256_subs_epi16(_mm256_slli_epi16(one, bd), one), t80);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i pmin = _mm256_subs_epi16(zero, t80);

  __m256i filter = _mm256_subs_epi16(ps1, qs1);
  pixel_clamp(&pmin, &pmax, &filter);

  __m256i hev;
  highbd_hev_mask(p, q, th, &hev);
  filter = _mm256_and_si256(filter, hev);

  const __m256i x = _mm256_subs_epi16(qs0, ps0);
  filter = _mm256_adds_epi16(filter, x);
  filter = _mm256_adds_epi16(filter, x);
  filter = _mm256_adds_epi16(filter, x);
  pixel_clamp(&pmin, &pmax, &filter);
  filter = _mm256_and_si256(filter, *mask);

  const __m256i t3 = _mm256_set1_epi16(3);
  const __m256i t4 = _mm256_set1_epi16(4);

  __m256i filter1 = _mm256_adds_epi16(filter, t4);
  __m256i filter2 = _mm256_adds_epi16(filter, t3);
  pixel_clamp(&pmin, &pmax, &filter1);
  pixel_clamp(&pmin, &pmax, &filter2);
  filter1 = _mm256_srai_epi16(filter1, 3);
  filter2 = _mm256_srai_epi16(filter2, 3);

  qs0 = _mm256_subs_epi16(qs0, filter1);
  pixel_clamp(&pmin, &pmax, &qs0);
  ps0 = _mm256_adds_epi16(ps0, filter2);
  pixel_clamp(&pmin, &pmax, &ps0);

  qs[0] = _mm256_adds_epi16(qs0, t80);
  ps[0] = _mm256_adds_epi16(ps0, t80);

  filter = _mm256_adds_epi16(filter1, one);
  filter = _mm256_srai_epi16(filter, 1);
  filter = _mm256_andnot_si256(hev, filter);

  qs1 = _mm256_subs_epi16(qs1, filter);
  pixel_clamp(&pmin, &pmax, &qs1);
  ps1 = _mm256_adds_epi16(ps1, filter);
  pixel_clamp(&pmin, &pmax, &ps1);

  qs[1] = _mm256_adds_epi16(qs1, t80);
  ps[1] = _mm256_adds_epi16(ps1, t80);
}
#endif  // #if !CONFIG_PARALLEL_DEBLOCKING || !CONFIG_CB4X4

#if CONFIG_PARALLEL_DEBLOCKING && CONFIG_CB4X4
void aom_highbd_lpf_horizontal_edge_16_avx2(uint16_t *s, int p,
                                            const uint8_t *blt,
                                            const uint8_t *lt,
                                            const uint8_t *thr, int bd) {
  aom_highbd_lpf_horizontal_edge_16_sse2(s, p, blt, lt, thr, bd);
}

void aom_highbd_lpf_vertical_16_dual_avx2(uint16_t *s, int p,
                                          const uint8_t *blt, const uint8_t *lt,
                                          const uint8_t *thr, int bd) {
  aom_highbd_lpf_vertical_16_dual_sse2(s, p, blt, lt, thr, bd);
}

void aom_highbd_lpf_horizontal_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_4_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                        limit1, thresh1, bd);
}

void aom_highbd_lpf_horizontal_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_8_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                        limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_4_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                      limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_8_dual_sse2(s, p, blimit0, limit0, thresh0, blimit1,
                                      limit1, thresh1, bd);
}
#else
void aom_highbd_lpf_horizontal_edge_16_avx2(uint16_t *s, int pitch,
                                            const uint8_t *blt,
                                            const uint8_t *lt,
                                            const uint8_t *thr, int bd) {
  __m256i blimit, limit, thresh;
  get_limit(blt, lt, thr, bd, &blimit, &limit, &thresh);

  __m256i p[8], q[8];
  load_highbd_pixel(s, 8, pitch, p, q);

  __m256i mask;
  highbd_filter_mask(p, q, &limit, &blimit, &mask);

  __m256i flat, flat2;
  const __m256i one = _mm256_set1_epi16(1);
  highbd_flat_mask4(&one, p, q, &flat, bd);
  highbd_flat_mask5(&one, p, q, &flat2, bd);

  flat = _mm256_and_si256(flat, mask);
  flat2 = _mm256_and_si256(flat2, flat);

  __m256i ps[2], qs[2];
  highbd_filter4(p, q, &mask, &thresh, bd, ps, qs);

  // flat and wide flat calculations
  __m256i flat_p[3], flat_q[3];
  __m256i flat2_p[7], flat2_q[7];
  {
    const __m256i eight = _mm256_set1_epi16(8);
    const __m256i four = _mm256_set1_epi16(4);

    __m256i sum_p = _mm256_add_epi16(_mm256_add_epi16(p[6], p[5]),
                                     _mm256_add_epi16(p[4], p[3]));
    __m256i sum_q = _mm256_add_epi16(_mm256_add_epi16(q[6], q[5]),
                                     _mm256_add_epi16(q[4], q[3]));

    __m256i sum_lp = _mm256_add_epi16(p[0], _mm256_add_epi16(p[2], p[1]));
    sum_p = _mm256_add_epi16(sum_p, sum_lp);

    __m256i sum_lq = _mm256_add_epi16(q[0], _mm256_add_epi16(q[2], q[1]));
    sum_q = _mm256_add_epi16(sum_q, sum_lq);
    sum_p = _mm256_add_epi16(eight, _mm256_add_epi16(sum_p, sum_q));
    sum_lp = _mm256_add_epi16(four, _mm256_add_epi16(sum_lp, sum_lq));

    flat2_p[0] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_p, _mm256_add_epi16(p[7], p[0])), 4);
    flat2_q[0] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_p, _mm256_add_epi16(q[7], q[0])), 4);
    flat_p[0] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lp, _mm256_add_epi16(p[3], p[0])), 3);
    flat_q[0] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lp, _mm256_add_epi16(q[3], q[0])), 3);

    __m256i sum_p7 = _mm256_add_epi16(p[7], p[7]);
    __m256i sum_q7 = _mm256_add_epi16(q[7], q[7]);
    __m256i sum_p3 = _mm256_add_epi16(p[3], p[3]);
    __m256i sum_q3 = _mm256_add_epi16(q[3], q[3]);

    sum_q = _mm256_sub_epi16(sum_p, p[6]);
    sum_p = _mm256_sub_epi16(sum_p, q[6]);
    flat2_p[1] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_p, _mm256_add_epi16(sum_p7, p[1])), 4);
    flat2_q[1] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_q, _mm256_add_epi16(sum_q7, q[1])), 4);

    sum_lq = _mm256_sub_epi16(sum_lp, p[2]);
    sum_lp = _mm256_sub_epi16(sum_lp, q[2]);
    flat_p[1] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lp, _mm256_add_epi16(sum_p3, p[1])), 3);
    flat_q[1] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lq, _mm256_add_epi16(sum_q3, q[1])), 3);

    sum_p7 = _mm256_add_epi16(sum_p7, p[7]);
    sum_q7 = _mm256_add_epi16(sum_q7, q[7]);
    sum_p3 = _mm256_add_epi16(sum_p3, p[3]);
    sum_q3 = _mm256_add_epi16(sum_q3, q[3]);

    sum_p = _mm256_sub_epi16(sum_p, q[5]);
    sum_q = _mm256_sub_epi16(sum_q, p[5]);
    flat2_p[2] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_p, _mm256_add_epi16(sum_p7, p[2])), 4);
    flat2_q[2] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_q, _mm256_add_epi16(sum_q7, q[2])), 4);

    sum_lp = _mm256_sub_epi16(sum_lp, q[1]);
    sum_lq = _mm256_sub_epi16(sum_lq, p[1]);
    flat_p[2] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lp, _mm256_add_epi16(sum_p3, p[2])), 3);
    flat_q[2] = _mm256_srli_epi16(
        _mm256_add_epi16(sum_lq, _mm256_add_epi16(sum_q3, q[2])), 3);

    int i;
    for (i = 3; i < 7; ++i) {
      sum_p7 = _mm256_add_epi16(sum_p7, p[7]);
      sum_q7 = _mm256_add_epi16(sum_q7, q[7]);
      sum_p = _mm256_sub_epi16(sum_p, q[7 - i]);
      sum_q = _mm256_sub_epi16(sum_q, p[7 - i]);
      flat2_p[i] = _mm256_srli_epi16(
          _mm256_add_epi16(sum_p, _mm256_add_epi16(sum_p7, p[i])), 4);
      flat2_q[i] = _mm256_srli_epi16(
          _mm256_add_epi16(sum_q, _mm256_add_epi16(sum_q7, q[i])), 4);
    }
  }

  // highbd_filter8
  p[2] = _mm256_andnot_si256(flat, p[2]);
  //  p2 remains unchanged if !(flat && mask)
  flat_p[2] = _mm256_and_si256(flat, flat_p[2]);
  //  when (flat && mask)
  p[2] = _mm256_or_si256(p[2], flat_p[2]);  // full list of p2 values
  q[2] = _mm256_andnot_si256(flat, q[2]);
  flat_q[2] = _mm256_and_si256(flat, flat_q[2]);
  q[2] = _mm256_or_si256(q[2], flat_q[2]);  // full list of q2 values

  int i;
  for (i = 1; i >= 0; i--) {
    ps[i] = _mm256_andnot_si256(flat, ps[i]);
    flat_p[i] = _mm256_and_si256(flat, flat_p[i]);
    p[i] = _mm256_or_si256(ps[i], flat_p[i]);
    qs[i] = _mm256_andnot_si256(flat, qs[i]);
    flat_q[i] = _mm256_and_si256(flat, flat_q[i]);
    q[i] = _mm256_or_si256(qs[i], flat_q[i]);
  }

  // highbd_filter16

  for (i = 6; i >= 0; i--) {
    //  p[i] remains unchanged if !(flat2 && flat && mask)
    p[i] = _mm256_andnot_si256(flat2, p[i]);
    flat2_p[i] = _mm256_and_si256(flat2, flat2_p[i]);
    //  get values for when (flat2 && flat && mask)
    p[i] = _mm256_or_si256(p[i], flat2_p[i]);  // full list of p values

    q[i] = _mm256_andnot_si256(flat2, q[i]);
    flat2_q[i] = _mm256_and_si256(flat2, flat2_q[i]);
    q[i] = _mm256_or_si256(q[i], flat2_q[i]);
    _mm256_storeu_si256((__m256i *)(s - (i + 1) * pitch), p[i]);
    _mm256_storeu_si256((__m256i *)(s + i * pitch), q[i]);
  }
}

static INLINE void highbd_transpose16x16(uint16_t *src, int src_p,
                                         uint16_t *dst, int dst_p) {
  __m256i x[16];
  int i;
  for (i = 0; i < 16; ++i) {
    x[i] = _mm256_loadu_si256((const __m256i *)src);
    src += src_p;
  }
  mm256_transpose_16x16(x, x);
  for (i = 0; i < 16; ++i) {
    _mm256_storeu_si256((__m256i *)dst, x[i]);
    dst += dst_p;
  }
}

void aom_highbd_lpf_vertical_16_dual_avx2(uint16_t *s, int p,
                                          const uint8_t *blimit,
                                          const uint8_t *limit,
                                          const uint8_t *thresh, int bd) {
  DECLARE_ALIGNED(16, uint16_t, t_dst[256]);

  //  Transpose 16x16
  highbd_transpose16x16(s - 8, p, t_dst, 16);

  //  Loop filtering
  aom_highbd_lpf_horizontal_edge_16_avx2(t_dst + 8 * 16, 16, blimit, limit,
                                         thresh, bd);

  //  Transpose back
  highbd_transpose16x16(t_dst, 16, s - 8, p);
}

static INLINE void get_dual_limit(const uint8_t *b0, const uint8_t *l0,
                                  const uint8_t *t0, const uint8_t *b1,
                                  const uint8_t *l1, const uint8_t *t1, int bd,
                                  __m256i *blt, __m256i *lt, __m256i *thr) {
  const __m128i z128 = _mm_setzero_si128();
  const __m128i blimit0 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)b0), z128);
  const __m128i limit0 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)l0), z128);
  const __m128i thresh0 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)t0), z128);
  const __m128i blimit1 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)b1), z128);
  const __m128i limit1 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)l1), z128);
  const __m128i thresh1 =
      _mm_unpacklo_epi8(_mm_load_si128((const __m128i *)t1), z128);

  *blt = _mm256_inserti128_si256(_mm256_castsi128_si256(blimit0), blimit1, 1);
  *lt = _mm256_inserti128_si256(_mm256_castsi128_si256(limit0), limit1, 1);
  *thr = _mm256_inserti128_si256(_mm256_castsi128_si256(thresh0), thresh1, 1);

  int shift = bd - 8;
  *blt = _mm256_slli_epi16(*blt, shift);
  *lt = _mm256_slli_epi16(*lt, shift);
  *thr = _mm256_slli_epi16(*thr, shift);
}

void aom_highbd_lpf_horizontal_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *_blimit0, const uint8_t *_limit0,
    const uint8_t *_thresh0, const uint8_t *_blimit1, const uint8_t *_limit1,
    const uint8_t *_thresh1, int bd) {
  __m256i p3 = _mm256_loadu_si256((__m256i *)(s - 4 * p));
  __m256i p2 = _mm256_loadu_si256((__m256i *)(s - 3 * p));
  __m256i p1 = _mm256_loadu_si256((__m256i *)(s - 2 * p));
  __m256i p0 = _mm256_loadu_si256((__m256i *)(s - 1 * p));
  __m256i q0 = _mm256_loadu_si256((__m256i *)(s - 0 * p));
  __m256i q1 = _mm256_loadu_si256((__m256i *)(s + 1 * p));
  __m256i q2 = _mm256_loadu_si256((__m256i *)(s + 2 * p));
  __m256i q3 = _mm256_loadu_si256((__m256i *)(s + 3 * p));

  const __m256i abs_p1p0 = _mm256_abs_epi16(_mm256_sub_epi16(p1, p0));
  const __m256i abs_q1q0 = _mm256_abs_epi16(_mm256_sub_epi16(q1, q0));

  __m256i abs_p0q0 = _mm256_abs_epi16(_mm256_sub_epi16(p0, q0));
  __m256i abs_p1q1 = _mm256_abs_epi16(_mm256_sub_epi16(p1, q1));

  __m256i blimit, limit, thresh;
  get_dual_limit(_blimit0, _limit0, _thresh0, _blimit1, _limit1, _thresh1, bd,
                 &blimit, &limit, &thresh);

  __m256i t80, tff80, tffe0, t1f, t7f;
  if (bd == 8) {
    t80 = _mm256_set1_epi16(0x80);
    tff80 = _mm256_set1_epi16(0xff80);
    tffe0 = _mm256_set1_epi16(0xffe0);
    t1f = _mm256_srli_epi16(_mm256_set1_epi16(0x1fff), 8);
    t7f = _mm256_srli_epi16(_mm256_set1_epi16(0x7fff), 8);
  } else if (bd == 10) {
    t80 = _mm256_slli_epi16(_mm256_set1_epi16(0x80), 2);
    tff80 = _mm256_slli_epi16(_mm256_set1_epi16(0xff80), 2);
    tffe0 = _mm256_slli_epi16(_mm256_set1_epi16(0xffe0), 2);
    t1f = _mm256_srli_epi16(_mm256_set1_epi16(0x1fff), 6);
    t7f = _mm256_srli_epi16(_mm256_set1_epi16(0x7fff), 6);
  } else {  // bd == 12
    t80 = _mm256_slli_epi16(_mm256_set1_epi16(0x80), 4);
    tff80 = _mm256_slli_epi16(_mm256_set1_epi16(0xff80), 4);
    tffe0 = _mm256_slli_epi16(_mm256_set1_epi16(0xffe0), 4);
    t1f = _mm256_srli_epi16(_mm256_set1_epi16(0x1fff), 4);
    t7f = _mm256_srli_epi16(_mm256_set1_epi16(0x7fff), 4);
  }

  __m256i ps1 =
      _mm256_subs_epi16(_mm256_loadu_si256((__m256i *)(s - 2 * p)), t80);
  __m256i ps0 =
      _mm256_subs_epi16(_mm256_loadu_si256((__m256i *)(s - 1 * p)), t80);
  __m256i qs0 =
      _mm256_subs_epi16(_mm256_loadu_si256((__m256i *)(s + 0 * p)), t80);
  __m256i qs1 =
      _mm256_subs_epi16(_mm256_loadu_si256((__m256i *)(s + 1 * p)), t80);

  // filter_mask and hev_mask
  const __m256i zero = _mm256_setzero_si256();
  __m256i flat = _mm256_max_epi16(abs_p1p0, abs_q1q0);
  __m256i hev = _mm256_subs_epu16(flat, thresh);
  const __m256i ffff = _mm256_set1_epi16(0xFFFF);
  hev = _mm256_xor_si256(_mm256_cmpeq_epi16(hev, zero), ffff);

  abs_p0q0 = _mm256_adds_epu16(abs_p0q0, abs_p0q0);
  abs_p1q1 = _mm256_srli_epi16(abs_p1q1, 1);
  __m256i mask =
      _mm256_subs_epu16(_mm256_adds_epu16(abs_p0q0, abs_p1q1), blimit);
  mask = _mm256_xor_si256(_mm256_cmpeq_epi16(mask, zero), ffff);
  // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
  // So taking maximums continues to work:
  const __m256i one = _mm256_set1_epi16(1);
  mask = _mm256_and_si256(mask, _mm256_adds_epu16(limit, one));
  mask = _mm256_max_epi16(flat, mask);
  // mask |= (abs(p1 - p0) > limit) * -1;
  // mask |= (abs(q1 - q0) > limit) * -1;
  __m256i work = _mm256_max_epi16(
      _mm256_or_si256(_mm256_subs_epu16(p2, p1), _mm256_subs_epu16(p1, p2)),
      _mm256_or_si256(_mm256_subs_epu16(p3, p2), _mm256_subs_epu16(p2, p3)));
  mask = _mm256_max_epi16(work, mask);
  work = _mm256_max_epi16(
      _mm256_or_si256(_mm256_subs_epu16(q2, q1), _mm256_subs_epu16(q1, q2)),
      _mm256_or_si256(_mm256_subs_epu16(q3, q2), _mm256_subs_epu16(q2, q3)));
  mask = _mm256_max_epi16(work, mask);
  mask = _mm256_subs_epu16(mask, limit);
  mask = _mm256_cmpeq_epi16(mask, zero);

  // filter4
  const __m256i pmax = _mm256_subs_epi16(
      _mm256_subs_epi16(_mm256_slli_epi16(one, bd), one), t80);
  const __m256i pmin = _mm256_subs_epi16(zero, t80);

  __m256i filt = _mm256_subs_epi16(ps1, qs1);
  pixel_clamp(&pmin, &pmax, &filt);
  filt = _mm256_and_si256(filt, hev);
  __m256i work_a = _mm256_subs_epi16(qs0, ps0);
  filt = _mm256_adds_epi16(filt, work_a);
  filt = _mm256_adds_epi16(filt, work_a);
  filt = _mm256_adds_epi16(filt, work_a);
  pixel_clamp(&pmin, &pmax, &filt);

  // (aom_filter + 3 * (qs0 - ps0)) & mask
  filt = _mm256_and_si256(filt, mask);

  const __m256i t4 = _mm256_set1_epi16(4);
  const __m256i t3 = _mm256_set1_epi16(3);

  __m256i filter1 = _mm256_adds_epi16(filt, t4);
  pixel_clamp(&pmin, &pmax, &filter1);
  __m256i filter2 = _mm256_adds_epi16(filt, t3);
  pixel_clamp(&pmin, &pmax, &filter2);

  // Filter1 >> 3
  work_a = _mm256_cmpgt_epi16(zero, filter1);  // get the values that are <0
  filter1 = _mm256_srli_epi16(filter1, 3);
  work_a = _mm256_and_si256(work_a, tffe0);    // sign bits for the values < 0
  filter1 = _mm256_and_si256(filter1, t1f);    // clamp the range
  filter1 = _mm256_or_si256(filter1, work_a);  // reinsert the sign bits

  // Filter2 >> 3
  work_a = _mm256_cmpgt_epi16(zero, filter2);
  filter2 = _mm256_srli_epi16(filter2, 3);
  work_a = _mm256_and_si256(work_a, tffe0);
  filter2 = _mm256_and_si256(filter2, t1f);
  filter2 = _mm256_or_si256(filter2, work_a);

  // filt >> 1
  // equivalent to shifting 0x1f left by bitdepth - 8
  // and setting new bits to 1
  filt = _mm256_adds_epi16(filter1, one);
  work_a = _mm256_cmpgt_epi16(zero, filt);
  filt = _mm256_srli_epi16(filt, 1);
  work_a = _mm256_and_si256(work_a, tff80);
  filt = _mm256_and_si256(filt, t7f);
  filt = _mm256_or_si256(filt, work_a);

  filt = _mm256_andnot_si256(hev, filt);

  filter1 = _mm256_subs_epi16(qs0, filter1);
  pixel_clamp(&pmin, &pmax, &filter1);
  q0 = _mm256_adds_epi16(filter1, t80);

  filter1 = _mm256_subs_epi16(qs1, filt);
  pixel_clamp(&pmin, &pmax, &filter1);
  q1 = _mm256_adds_epi16(filter1, t80);

  filter2 = _mm256_adds_epi16(ps0, filter2);
  pixel_clamp(&pmin, &pmax, &filter2);
  p0 = _mm256_adds_epi16(filter2, t80);

  filter2 = _mm256_adds_epi16(ps1, filt);
  pixel_clamp(&pmin, &pmax, &filter2);
  p1 = _mm256_adds_epi16(filter2, t80);

  _mm256_storeu_si256((__m256i *)(s - 2 * p), p1);
  _mm256_storeu_si256((__m256i *)(s - 1 * p), p0);
  _mm256_storeu_si256((__m256i *)(s + 0 * p), q0);
  _mm256_storeu_si256((__m256i *)(s + 1 * p), q1);
}

void aom_highbd_lpf_horizontal_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *_blimit0, const uint8_t *_limit0,
    const uint8_t *_thresh0, const uint8_t *_blimit1, const uint8_t *_limit1,
    const uint8_t *_thresh1, int bd) {
  DECLARE_ALIGNED(16, uint16_t, flat_op2[16]);
  DECLARE_ALIGNED(16, uint16_t, flat_op1[16]);
  DECLARE_ALIGNED(16, uint16_t, flat_op0[16]);
  DECLARE_ALIGNED(16, uint16_t, flat_oq2[16]);
  DECLARE_ALIGNED(16, uint16_t, flat_oq1[16]);
  DECLARE_ALIGNED(16, uint16_t, flat_oq0[16]);

  __m256i p3 = _mm256_loadu_si256((__m256i *)(s - 4 * p));
  __m256i q3 = _mm256_loadu_si256((__m256i *)(s + 3 * p));
  __m256i p2 = _mm256_loadu_si256((__m256i *)(s - 3 * p));
  __m256i q2 = _mm256_loadu_si256((__m256i *)(s + 2 * p));
  __m256i p1 = _mm256_loadu_si256((__m256i *)(s - 2 * p));
  __m256i q1 = _mm256_loadu_si256((__m256i *)(s + 1 * p));
  __m256i p0 = _mm256_loadu_si256((__m256i *)(s - 1 * p));
  __m256i q0 = _mm256_loadu_si256((__m256i *)(s + 0 * p));

  __m256i blimit, limit, thresh;
  get_dual_limit(_blimit0, _limit0, _thresh0, _blimit1, _limit1, _thresh1, bd,
                 &blimit, &limit, &thresh);

  __m256i t80;
  if (bd == 8) {
    t80 = _mm256_set1_epi16(0x80);
  } else if (bd == 10) {
    t80 = _mm256_set1_epi16(0x200);
  } else {  // bd == 12
    t80 = _mm256_set1_epi16(0x800);
  }

  __m256i ps1, ps0, qs0, qs1;
  ps1 = _mm256_subs_epi16(p1, t80);
  ps0 = _mm256_subs_epi16(p0, t80);
  qs0 = _mm256_subs_epi16(q0, t80);
  qs1 = _mm256_subs_epi16(q1, t80);

  // filter_mask and hev_mask
  __m256i abs_p1q1, abs_p0q0, abs_q1q0, abs_p1p0, work;
  abs_p1p0 = _mm256_abs_epi16(_mm256_sub_epi16(p1, p0));
  abs_q1q0 = _mm256_abs_epi16(_mm256_sub_epi16(q1, q0));

  abs_p0q0 = _mm256_abs_epi16(_mm256_sub_epi16(p0, q0));
  abs_p1q1 = _mm256_abs_epi16(_mm256_sub_epi16(p1, q1));
  __m256i flat = _mm256_max_epi16(abs_p1p0, abs_q1q0);
  __m256i hev = _mm256_subs_epu16(flat, thresh);
  const __m256i zero = _mm256_set1_epi16(0);
  const __m256i ffff = _mm256_set1_epi16(0xFFFF);
  hev = _mm256_xor_si256(_mm256_cmpeq_epi16(hev, zero), ffff);

  abs_p0q0 = _mm256_adds_epu16(abs_p0q0, abs_p0q0);
  abs_p1q1 = _mm256_srli_epi16(abs_p1q1, 1);
  __m256i mask =
      _mm256_subs_epu16(_mm256_adds_epu16(abs_p0q0, abs_p1q1), blimit);
  mask = _mm256_xor_si256(_mm256_cmpeq_epi16(mask, zero), ffff);
  // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
  // So taking maximums continues to work:

  const __m256i one = _mm256_set1_epi16(1);
  mask = _mm256_and_si256(mask, _mm256_adds_epu16(limit, one));
  mask = _mm256_max_epi16(abs_p1p0, mask);
  // mask |= (abs(p1 - p0) > limit) * -1;
  mask = _mm256_max_epi16(abs_q1q0, mask);
  // mask |= (abs(q1 - q0) > limit) * -1;

  work = _mm256_max_epi16(_mm256_abs_epi16(_mm256_sub_epi16(p2, p1)),
                          _mm256_abs_epi16(_mm256_sub_epi16(q2, q1)));
  mask = _mm256_max_epi16(work, mask);
  work = _mm256_max_epi16(_mm256_abs_epi16(_mm256_sub_epi16(p3, p2)),
                          _mm256_abs_epi16(_mm256_sub_epi16(q3, q2)));
  mask = _mm256_max_epi16(work, mask);
  mask = _mm256_subs_epu16(mask, limit);
  mask = _mm256_cmpeq_epi16(mask, zero);

  // flat_mask4
  flat = _mm256_max_epi16(_mm256_abs_epi16(_mm256_sub_epi16(p2, p0)),
                          _mm256_abs_epi16(_mm256_sub_epi16(q2, q0)));
  work = _mm256_max_epi16(_mm256_abs_epi16(_mm256_sub_epi16(p3, p0)),
                          _mm256_abs_epi16(_mm256_sub_epi16(q3, q0)));
  flat = _mm256_max_epi16(work, flat);
  flat = _mm256_max_epi16(abs_p1p0, flat);
  flat = _mm256_max_epi16(abs_q1q0, flat);

  if (bd == 8)
    flat = _mm256_subs_epu16(flat, one);
  else if (bd == 10)
    flat = _mm256_subs_epu16(flat, _mm256_slli_epi16(one, 2));
  else  // bd == 12
    flat = _mm256_subs_epu16(flat, _mm256_slli_epi16(one, 4));

  flat = _mm256_cmpeq_epi16(flat, zero);
  flat = _mm256_and_si256(flat, mask);  // flat & mask

  // Added before shift for rounding part of ROUND_POWER_OF_TWO
  __m256i workp_a, workp_b, workp_shft;
  workp_a =
      _mm256_add_epi16(_mm256_add_epi16(p3, p3), _mm256_add_epi16(p2, p1));
  const __m256i four = _mm256_set1_epi16(4);
  workp_a = _mm256_add_epi16(_mm256_add_epi16(workp_a, four), p0);
  workp_b = _mm256_add_epi16(_mm256_add_epi16(q0, p2), p3);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_op2[0], workp_shft);

  workp_b = _mm256_add_epi16(_mm256_add_epi16(q0, q1), p1);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_op1[0], workp_shft);

  workp_a = _mm256_add_epi16(_mm256_sub_epi16(workp_a, p3), q2);
  workp_b = _mm256_add_epi16(_mm256_sub_epi16(workp_b, p1), p0);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_op0[0], workp_shft);

  workp_a = _mm256_add_epi16(_mm256_sub_epi16(workp_a, p3), q3);
  workp_b = _mm256_add_epi16(_mm256_sub_epi16(workp_b, p0), q0);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_oq0[0], workp_shft);

  workp_a = _mm256_add_epi16(_mm256_sub_epi16(workp_a, p2), q3);
  workp_b = _mm256_add_epi16(_mm256_sub_epi16(workp_b, q0), q1);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_oq1[0], workp_shft);

  workp_a = _mm256_add_epi16(_mm256_sub_epi16(workp_a, p1), q3);
  workp_b = _mm256_add_epi16(_mm256_sub_epi16(workp_b, q1), q2);
  workp_shft = _mm256_srli_epi16(_mm256_add_epi16(workp_a, workp_b), 3);
  _mm256_storeu_si256((__m256i *)&flat_oq2[0], workp_shft);

  // lp filter
  const __m256i pmax = _mm256_subs_epi16(
      _mm256_subs_epi16(_mm256_slli_epi16(one, bd), one), t80);
  const __m256i pmin = _mm256_subs_epi16(zero, t80);

  __m256i filt, filter1, filter2, work_a;
  filt = _mm256_subs_epi16(ps1, qs1);
  pixel_clamp(&pmin, &pmax, &filt);
  filt = _mm256_and_si256(filt, hev);
  work_a = _mm256_subs_epi16(qs0, ps0);
  filt = _mm256_adds_epi16(filt, work_a);
  filt = _mm256_adds_epi16(filt, work_a);
  filt = _mm256_adds_epi16(filt, work_a);
  // (aom_filter + 3 * (qs0 - ps0)) & mask
  pixel_clamp(&pmin, &pmax, &filt);
  filt = _mm256_and_si256(filt, mask);

  const __m256i t4 = _mm256_set1_epi16(4);
  const __m256i t3 = _mm256_set1_epi16(3);

  filter1 = _mm256_adds_epi16(filt, t4);
  filter2 = _mm256_adds_epi16(filt, t3);

  // Filter1 >> 3
  pixel_clamp(&pmin, &pmax, &filter1);
  filter1 = _mm256_srai_epi16(filter1, 3);

  // Filter2 >> 3
  pixel_clamp(&pmin, &pmax, &filter2);
  filter2 = _mm256_srai_epi16(filter2, 3);

  // filt >> 1
  filt = _mm256_adds_epi16(filter1, one);
  filt = _mm256_srai_epi16(filt, 1);
  // filter = ROUND_POWER_OF_TWO(filter1, 1) & ~hev;
  filt = _mm256_andnot_si256(hev, filt);

  work_a = _mm256_subs_epi16(qs0, filter1);
  pixel_clamp(&pmin, &pmax, &work_a);
  work_a = _mm256_adds_epi16(work_a, t80);
  q0 = _mm256_loadu_si256((__m256i *)flat_oq0);
  work_a = _mm256_andnot_si256(flat, work_a);
  q0 = _mm256_and_si256(flat, q0);
  q0 = _mm256_or_si256(work_a, q0);

  work_a = _mm256_subs_epi16(qs1, filt);
  pixel_clamp(&pmin, &pmax, &work_a);
  work_a = _mm256_adds_epi16(work_a, t80);
  q1 = _mm256_loadu_si256((__m256i *)flat_oq1);
  work_a = _mm256_andnot_si256(flat, work_a);
  q1 = _mm256_and_si256(flat, q1);
  q1 = _mm256_or_si256(work_a, q1);

  work_a = _mm256_loadu_si256((__m256i *)(s + 2 * p));
  q2 = _mm256_loadu_si256((__m256i *)flat_oq2);
  work_a = _mm256_andnot_si256(flat, work_a);
  q2 = _mm256_and_si256(flat, q2);
  q2 = _mm256_or_si256(work_a, q2);

  work_a = _mm256_adds_epi16(ps0, filter2);
  pixel_clamp(&pmin, &pmax, &work_a);
  work_a = _mm256_adds_epi16(work_a, t80);
  p0 = _mm256_loadu_si256((__m256i *)flat_op0);
  work_a = _mm256_andnot_si256(flat, work_a);
  p0 = _mm256_and_si256(flat, p0);
  p0 = _mm256_or_si256(work_a, p0);

  work_a = _mm256_adds_epi16(ps1, filt);
  pixel_clamp(&pmin, &pmax, &work_a);
  work_a = _mm256_adds_epi16(work_a, t80);
  p1 = _mm256_loadu_si256((__m256i *)flat_op1);
  work_a = _mm256_andnot_si256(flat, work_a);
  p1 = _mm256_and_si256(flat, p1);
  p1 = _mm256_or_si256(work_a, p1);

  work_a = _mm256_loadu_si256((__m256i *)(s - 3 * p));
  p2 = _mm256_loadu_si256((__m256i *)flat_op2);
  work_a = _mm256_andnot_si256(flat, work_a);
  p2 = _mm256_and_si256(flat, p2);
  p2 = _mm256_or_si256(work_a, p2);

  _mm256_storeu_si256((__m256i *)(s - 3 * p), p2);
  _mm256_storeu_si256((__m256i *)(s - 2 * p), p1);
  _mm256_storeu_si256((__m256i *)(s - 1 * p), p0);
  _mm256_storeu_si256((__m256i *)(s + 0 * p), q0);
  _mm256_storeu_si256((__m256i *)(s + 1 * p), q1);
  _mm256_storeu_si256((__m256i *)(s + 2 * p), q2);
}

void aom_highbd_lpf_vertical_4_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  DECLARE_ALIGNED(16, uint16_t, t_dst[16 * 8]);
  uint16_t *src[2];
  uint16_t *dst[2];

  // Transpose 8x16
  highbd_transpose8x16(s - 4, s - 4 + p * 8, p, t_dst, 16);

  // Loop filtering
  aom_highbd_lpf_horizontal_4_dual_avx2(t_dst + 4 * 16, 16, blimit0, limit0,
                                        thresh0, blimit1, limit1, thresh1, bd);
  src[0] = t_dst;
  src[1] = t_dst + 8;
  dst[0] = s - 4;
  dst[1] = s - 4 + p * 8;

  // Transpose back
  highbd_transpose(src, 16, dst, p, 2);
}

void aom_highbd_lpf_vertical_8_dual_avx2(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  DECLARE_ALIGNED(16, uint16_t, t_dst[16 * 8]);
  uint16_t *src[2];
  uint16_t *dst[2];

  // Transpose 8x16
  highbd_transpose8x16(s - 4, s - 4 + p * 8, p, t_dst, 16);

  // Loop filtering
  aom_highbd_lpf_horizontal_8_dual_avx2(t_dst + 4 * 16, 16, blimit0, limit0,
                                        thresh0, blimit1, limit1, thresh1, bd);
  src[0] = t_dst;
  src[1] = t_dst + 8;

  dst[0] = s - 4;
  dst[1] = s - 4 + p * 8;

  // Transpose back
  highbd_transpose(src, 16, dst, p, 2);
}
#endif  // CONFIG_PARALLEL_DEBLOCKING && CONFIG_CB4X4
