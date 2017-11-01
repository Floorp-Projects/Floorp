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

#include "./av1_rtcd.h"
#include "./cdef_block.h"

/* partial A is a 16-bit vector of the form:
   [x8 x7 x6 x5 x4 x3 x2 x1] and partial B has the form:
   [0  y1 y2 y3 y4 y5 y6 y7].
   This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
   (x7^2+y2^7)*C7 + (x8^2+0^2)*C8 where the C1..C8 constants are in const1
   and const2. */
static INLINE v128 fold_mul_and_sum(v128 partiala, v128 partialb, v128 const1,
                                    v128 const2) {
  v128 tmp;
  /* Reverse partial B. */
  partialb = v128_shuffle_8(
      partialb, v128_from_32(0x0f0e0100, 0x03020504, 0x07060908, 0x0b0a0d0c));
  /* Interleave the x and y values of identical indices and pair x8 with 0. */
  tmp = partiala;
  partiala = v128_ziplo_16(partialb, partiala);
  partialb = v128_ziphi_16(partialb, tmp);
  /* Square and add the corresponding x and y values. */
  partiala = v128_madd_s16(partiala, partiala);
  partialb = v128_madd_s16(partialb, partialb);
  /* Multiply by constant. */
  partiala = v128_mullo_s32(partiala, const1);
  partialb = v128_mullo_s32(partialb, const2);
  /* Sum all results. */
  partiala = v128_add_32(partiala, partialb);
  return partiala;
}

static INLINE v128 hsum4(v128 x0, v128 x1, v128 x2, v128 x3) {
  v128 t0, t1, t2, t3;
  t0 = v128_ziplo_32(x1, x0);
  t1 = v128_ziplo_32(x3, x2);
  t2 = v128_ziphi_32(x1, x0);
  t3 = v128_ziphi_32(x3, x2);
  x0 = v128_ziplo_64(t1, t0);
  x1 = v128_ziphi_64(t1, t0);
  x2 = v128_ziplo_64(t3, t2);
  x3 = v128_ziphi_64(t3, t2);
  return v128_add_32(v128_add_32(x0, x1), v128_add_32(x2, x3));
}

/* Computes cost for directions 0, 5, 6 and 7. We can call this function again
   to compute the remaining directions. */
static INLINE v128 compute_directions(v128 lines[8], int32_t tmp_cost1[4]) {
  v128 partial4a, partial4b, partial5a, partial5b, partial7a, partial7b;
  v128 partial6;
  v128 tmp;
  /* Partial sums for lines 0 and 1. */
  partial4a = v128_shl_n_byte(lines[0], 14);
  partial4b = v128_shr_n_byte(lines[0], 2);
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[1], 12));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[1], 4));
  tmp = v128_add_16(lines[0], lines[1]);
  partial5a = v128_shl_n_byte(tmp, 10);
  partial5b = v128_shr_n_byte(tmp, 6);
  partial7a = v128_shl_n_byte(tmp, 4);
  partial7b = v128_shr_n_byte(tmp, 12);
  partial6 = tmp;

  /* Partial sums for lines 2 and 3. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[2], 10));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[2], 6));
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[3], 8));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[3], 8));
  tmp = v128_add_16(lines[2], lines[3]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 8));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 8));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 6));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 10));
  partial6 = v128_add_16(partial6, tmp);

  /* Partial sums for lines 4 and 5. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[4], 6));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[4], 10));
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[5], 4));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[5], 12));
  tmp = v128_add_16(lines[4], lines[5]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 6));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 10));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 8));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 8));
  partial6 = v128_add_16(partial6, tmp);

  /* Partial sums for lines 6 and 7. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[6], 2));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[6], 14));
  partial4a = v128_add_16(partial4a, lines[7]);
  tmp = v128_add_16(lines[6], lines[7]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 4));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 12));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 10));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 6));
  partial6 = v128_add_16(partial6, tmp);

  /* Compute costs in terms of partial sums. */
  partial4a =
      fold_mul_and_sum(partial4a, partial4b, v128_from_32(210, 280, 420, 840),
                       v128_from_32(105, 120, 140, 168));
  partial7a =
      fold_mul_and_sum(partial7a, partial7b, v128_from_32(210, 420, 0, 0),
                       v128_from_32(105, 105, 105, 140));
  partial5a =
      fold_mul_and_sum(partial5a, partial5b, v128_from_32(210, 420, 0, 0),
                       v128_from_32(105, 105, 105, 140));
  partial6 = v128_madd_s16(partial6, partial6);
  partial6 = v128_mullo_s32(partial6, v128_dup_32(105));

  partial4a = hsum4(partial4a, partial5a, partial6, partial7a);
  v128_store_unaligned(tmp_cost1, partial4a);
  return partial4a;
}

/* transpose and reverse the order of the lines -- equivalent to a 90-degree
   counter-clockwise rotation of the pixels. */
static INLINE void array_reverse_transpose_8x8(v128 *in, v128 *res) {
  const v128 tr0_0 = v128_ziplo_16(in[1], in[0]);
  const v128 tr0_1 = v128_ziplo_16(in[3], in[2]);
  const v128 tr0_2 = v128_ziphi_16(in[1], in[0]);
  const v128 tr0_3 = v128_ziphi_16(in[3], in[2]);
  const v128 tr0_4 = v128_ziplo_16(in[5], in[4]);
  const v128 tr0_5 = v128_ziplo_16(in[7], in[6]);
  const v128 tr0_6 = v128_ziphi_16(in[5], in[4]);
  const v128 tr0_7 = v128_ziphi_16(in[7], in[6]);

  const v128 tr1_0 = v128_ziplo_32(tr0_1, tr0_0);
  const v128 tr1_1 = v128_ziplo_32(tr0_5, tr0_4);
  const v128 tr1_2 = v128_ziphi_32(tr0_1, tr0_0);
  const v128 tr1_3 = v128_ziphi_32(tr0_5, tr0_4);
  const v128 tr1_4 = v128_ziplo_32(tr0_3, tr0_2);
  const v128 tr1_5 = v128_ziplo_32(tr0_7, tr0_6);
  const v128 tr1_6 = v128_ziphi_32(tr0_3, tr0_2);
  const v128 tr1_7 = v128_ziphi_32(tr0_7, tr0_6);

  res[7] = v128_ziplo_64(tr1_1, tr1_0);
  res[6] = v128_ziphi_64(tr1_1, tr1_0);
  res[5] = v128_ziplo_64(tr1_3, tr1_2);
  res[4] = v128_ziphi_64(tr1_3, tr1_2);
  res[3] = v128_ziplo_64(tr1_5, tr1_4);
  res[2] = v128_ziphi_64(tr1_5, tr1_4);
  res[1] = v128_ziplo_64(tr1_7, tr1_6);
  res[0] = v128_ziphi_64(tr1_7, tr1_6);
}

int SIMD_FUNC(cdef_find_dir)(const uint16_t *img, int stride, int32_t *var,
                             int coeff_shift) {
  int i;
  int32_t cost[8];
  int32_t best_cost = 0;
  int best_dir = 0;
  v128 lines[8];
  for (i = 0; i < 8; i++) {
    lines[i] = v128_load_unaligned(&img[i * stride]);
    lines[i] =
        v128_sub_16(v128_shr_s16(lines[i], coeff_shift), v128_dup_16(128));
  }

#if defined(__SSE4_1__)
  /* Compute "mostly vertical" directions. */
  __m128i dir47 = compute_directions(lines, cost + 4);

  array_reverse_transpose_8x8(lines, lines);

  /* Compute "mostly horizontal" directions. */
  __m128i dir03 = compute_directions(lines, cost);

  __m128i max = _mm_max_epi32(dir03, dir47);
  max = _mm_max_epi32(max, _mm_shuffle_epi32(max, _MM_SHUFFLE(1, 0, 3, 2)));
  max = _mm_max_epi32(max, _mm_shuffle_epi32(max, _MM_SHUFFLE(2, 3, 0, 1)));
  best_cost = _mm_cvtsi128_si32(max);
  __m128i t =
      _mm_packs_epi32(_mm_cmpeq_epi32(max, dir03), _mm_cmpeq_epi32(max, dir47));
  best_dir = _mm_movemask_epi8(_mm_packs_epi16(t, t));
  best_dir = get_msb(best_dir ^ (best_dir - 1));  // Count trailing zeros
#else
  /* Compute "mostly vertical" directions. */
  compute_directions(lines, cost + 4);

  array_reverse_transpose_8x8(lines, lines);

  /* Compute "mostly horizontal" directions. */
  compute_directions(lines, cost);

  for (i = 0; i < 8; i++) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      best_dir = i;
    }
  }
#endif

  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var = best_cost - cost[(best_dir + 4) & 7];
  /* We'd normally divide by 840, but dividing by 1024 is close enough
     for what we're going to do with this. */
  *var >>= 10;
  return best_dir;
}

// sign(a-b) * min(abs(a-b), max(0, threshold - (abs(a-b) >> adjdamp)))
SIMD_INLINE v128 constrain16(v128 a, v128 b, unsigned int threshold,
                             unsigned int adjdamp) {
  v128 diff = v128_sub_16(a, b);
  const v128 sign = v128_shr_n_s16(diff, 15);
  diff = v128_abs_s16(diff);
  const v128 s =
      v128_ssub_u16(v128_dup_16(threshold), v128_shr_u16(diff, adjdamp));
  return v128_xor(v128_add_16(sign, v128_min_s16(diff, s)), sign);
}

#if CONFIG_CDEF_SINGLEPASS
// sign(a - b) * min(abs(a - b), max(0, strength - (abs(a - b) >> adjdamp)))
SIMD_INLINE v128 constrain(v256 a, v256 b, unsigned int strength,
                           unsigned int adjdamp) {
  const v256 diff16 = v256_sub_16(a, b);
  v128 diff = v128_pack_s16_s8(v256_high_v128(diff16), v256_low_v128(diff16));
  const v128 sign = v128_cmplt_s8(diff, v128_zero());
  diff = v128_abs_s8(diff);
  return v128_xor(
      v128_add_8(sign,
                 v128_min_u8(diff, v128_ssub_u8(v128_dup_8(strength),
                                                v128_shr_u8(diff, adjdamp)))),
      sign);
}

#if CDEF_CAP
void SIMD_FUNC(cdef_filter_block_4x4_8)(uint8_t *dst, int dstride,
                                        const uint16_t *in, int pri_strength,
                                        int sec_strength, int dir,
                                        int pri_damping, int sec_damping,
                                        UNUSED int max_unused)
#else
void SIMD_FUNC(cdef_filter_block_4x4_8)(uint8_t *dst, int dstride,
                                        const uint16_t *in, int pri_strength,
                                        int sec_strength, int dir,
                                        int pri_damping, int sec_damping,
                                        int max)
#endif
{
  v128 p0, p1, p2, p3;
  v256 sum, row, tap, res;
#if CDEF_CAP
  v256 max, min, large = v256_dup_16(CDEF_VERY_LARGE);
#endif
  int po1 = cdef_directions[dir][0];
  int po2 = cdef_directions[dir][1];
#if CDEF_FULL
  int po3 = cdef_directions[dir][2];
#endif
  int s1o1 = cdef_directions[(dir + 2) & 7][0];
  int s1o2 = cdef_directions[(dir + 2) & 7][1];
  int s2o1 = cdef_directions[(dir + 6) & 7][0];
  int s2o2 = cdef_directions[(dir + 6) & 7][1];

  const int *pri_taps = cdef_pri_taps[pri_strength & 1];
  const int *sec_taps = cdef_sec_taps[pri_strength & 1];

  if (pri_strength)
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  if (sec_strength)
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));

  sum = v256_zero();
  row = v256_from_v64(v64_load_aligned(&in[0 * CDEF_BSTRIDE]),
                      v64_load_aligned(&in[1 * CDEF_BSTRIDE]),
                      v64_load_aligned(&in[2 * CDEF_BSTRIDE]),
                      v64_load_aligned(&in[3 * CDEF_BSTRIDE]));
#if CDEF_CAP
  max = min = row;
#endif

  if (pri_strength) {
    // Primary near taps
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + po1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + po1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + po1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + po1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - po1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - po1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - po1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - po1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[0] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[0]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));

    // Primary far taps
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + po2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + po2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + po2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + po2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - po2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - po2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - po2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - po2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[1] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[1]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));

#if CDEF_FULL
    // Primary extra taps
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + po3]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + po3]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + po3]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + po3]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - po3]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - po3]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - po3]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - po3]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[2] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[2]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));
#endif
  }

  if (sec_strength) {
    // Secondary near taps
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + s1o1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + s1o1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + s1o1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + s1o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - s1o1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - s1o1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - s1o1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - s1o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + s2o1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + s2o1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + s2o1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + s2o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p2 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - s2o1]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - s2o1]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - s2o1]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - s2o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p3 = constrain(tap, row, sec_strength, sec_damping);

    // sum += sec_taps[0] * (p0 + p1 + p2 + p3)
    p0 = v128_add_8(p0, p1);
    p2 = v128_add_8(p2, p3);
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(sec_taps[0]),
                                         v256_from_v128(v128_ziphi_8(p0, p2),
                                                        v128_ziplo_8(p0, p2))));

    // Secondary far taps
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + s1o2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + s1o2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + s1o2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + s1o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - s1o2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - s1o2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - s1o2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - s1o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE + s2o2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE + s2o2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE + s2o2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE + s2o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p2 = constrain(tap, row, sec_strength, sec_damping);
    tap = v256_from_v64(v64_load_unaligned(&in[0 * CDEF_BSTRIDE - s2o2]),
                        v64_load_unaligned(&in[1 * CDEF_BSTRIDE - s2o2]),
                        v64_load_unaligned(&in[2 * CDEF_BSTRIDE - s2o2]),
                        v64_load_unaligned(&in[3 * CDEF_BSTRIDE - s2o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p3 = constrain(tap, row, sec_strength, sec_damping);

    // sum += sec_taps[1] * (p0 + p1 + p2 + p3)
    p0 = v128_add_8(p0, p1);
    p2 = v128_add_8(p2, p3);

    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(sec_taps[1]),
                                         v256_from_v128(v128_ziphi_8(p0, p2),
                                                        v128_ziplo_8(p0, p2))));
  }

  // res = row + ((sum - (sum < 0) + 8) >> 4)
  sum = v256_add_16(sum, v256_cmplt_s16(sum, v256_zero()));
  res = v256_add_16(sum, v256_dup_16(8));
  res = v256_shr_n_s16(res, 4);
  res = v256_add_16(row, res);
#if CDEF_CAP
  res = v256_min_s16(v256_max_s16(res, min), max);
#else
  res = v256_min_s16(v256_max_s16(res, v256_zero()), v256_dup_16(max));
#endif
  res = v256_pack_s16_u8(res, res);

  p0 = v256_low_v128(res);
  u32_store_aligned(&dst[0 * dstride], v64_high_u32(v128_high_v64(p0)));
  u32_store_aligned(&dst[1 * dstride], v64_low_u32(v128_high_v64(p0)));
  u32_store_aligned(&dst[2 * dstride], v64_high_u32(v128_low_v64(p0)));
  u32_store_aligned(&dst[3 * dstride], v64_low_u32(v128_low_v64(p0)));
}

#if CDEF_CAP
void SIMD_FUNC(cdef_filter_block_8x8_8)(uint8_t *dst, int dstride,
                                        const uint16_t *in, int pri_strength,
                                        int sec_strength, int dir,
                                        int pri_damping, int sec_damping,
                                        UNUSED int max_unused)
#else
void SIMD_FUNC(cdef_filter_block_8x8_8)(uint8_t *dst, int dstride,
                                        const uint16_t *in, int pri_strength,
                                        int sec_strength, int dir,
                                        int pri_damping, int sec_damping,
                                        int max)
#endif
{
  int i;
  v128 p0, p1, p2, p3;
  v256 sum, row, res, tap;
#if CDEF_CAP
  v256 max, min, large = v256_dup_16(CDEF_VERY_LARGE);
#endif
  int po1 = cdef_directions[dir][0];
  int po2 = cdef_directions[dir][1];
#if CDEF_FULL
  int po3 = cdef_directions[dir][2];
#endif
  int s1o1 = cdef_directions[(dir + 2) & 7][0];
  int s1o2 = cdef_directions[(dir + 2) & 7][1];
  int s2o1 = cdef_directions[(dir + 6) & 7][0];
  int s2o2 = cdef_directions[(dir + 6) & 7][1];

  const int *pri_taps = cdef_pri_taps[pri_strength & 1];
  const int *sec_taps = cdef_sec_taps[pri_strength & 1];

  if (pri_strength)
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  if (sec_strength)
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  for (i = 0; i < 8; i += 2) {
    sum = v256_zero();
    row = v256_from_v128(v128_load_aligned(&in[i * CDEF_BSTRIDE]),
                         v128_load_aligned(&in[(i + 1) * CDEF_BSTRIDE]));

#if CDEF_CAP
    max = min = row;
#endif
    // Primary near taps
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + po1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - po1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[0] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[0]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));

    // Primary far taps
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + po2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - po2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[1] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[1]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));

#if CDEF_FULL
    // Primary extra taps
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + po3]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po3]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, pri_strength, pri_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - po3]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po3]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, pri_strength, pri_damping);

    // sum += pri_taps[2] * (p0 + p1)
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(pri_taps[2]),
                                         v256_from_v128(v128_ziphi_8(p0, p1),
                                                        v128_ziplo_8(p0, p1))));
#endif

    // Secondary near taps
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + s1o1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s1o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - s1o1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s1o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + s2o1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s2o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p2 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - s2o1]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s2o1]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p3 = constrain(tap, row, sec_strength, sec_damping);

    // sum += sec_taps[0] * (p0 + p1 + p2 + p3)
    p0 = v128_add_8(p0, p1);
    p2 = v128_add_8(p2, p3);
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(sec_taps[0]),
                                         v256_from_v128(v128_ziphi_8(p0, p2),
                                                        v128_ziplo_8(p0, p2))));

    // Secondary far taps
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + s1o2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s1o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p0 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - s1o2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s1o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p1 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE + s2o2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s2o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p2 = constrain(tap, row, sec_strength, sec_damping);
    tap =
        v256_from_v128(v128_load_unaligned(&in[i * CDEF_BSTRIDE - s2o2]),
                       v128_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s2o2]));
#if CDEF_CAP
    max = v256_max_s16(max, v256_andn(tap, v256_cmpeq_16(tap, large)));
    min = v256_min_s16(min, tap);
#endif
    p3 = constrain(tap, row, sec_strength, sec_damping);

    // sum += sec_taps[1] * (p0 + p1 + p2 + p3)
    p0 = v128_add_8(p0, p1);
    p2 = v128_add_8(p2, p3);
    sum = v256_add_16(sum, v256_madd_us8(v256_dup_8(sec_taps[1]),
                                         v256_from_v128(v128_ziphi_8(p0, p2),
                                                        v128_ziplo_8(p0, p2))));

    // res = row + ((sum - (sum < 0) + 8) >> 4)
    sum = v256_add_16(sum, v256_cmplt_s16(sum, v256_zero()));
    res = v256_add_16(sum, v256_dup_16(8));
    res = v256_shr_n_s16(res, 4);
    res = v256_add_16(row, res);
#if CDEF_CAP
    res = v256_min_s16(v256_max_s16(res, min), max);
#else
    res = v256_min_s16(v256_max_s16(res, v256_zero()), v256_dup_16(max));
#endif
    res = v256_pack_s16_u8(res, res);

    p0 = v256_low_v128(res);
    v64_store_aligned(&dst[i * dstride], v128_high_v64(p0));
    v64_store_aligned(&dst[(i + 1) * dstride], v128_low_v64(p0));
  }
}

#if CDEF_CAP
void SIMD_FUNC(cdef_filter_block_4x4_16)(uint16_t *dst, int dstride,
                                         const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir,
                                         int pri_damping, int sec_damping,
                                         UNUSED int max_unused)
#else
void SIMD_FUNC(cdef_filter_block_4x4_16)(uint16_t *dst, int dstride,
                                         const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir,
                                         int pri_damping, int sec_damping,
                                         int max)
#endif
{
  int i;
  v128 p0, p1, p2, p3, sum, row, res;
#if CDEF_CAP
  v128 max, min, large = v128_dup_16(CDEF_VERY_LARGE);
#endif
  int po1 = cdef_directions[dir][0];
  int po2 = cdef_directions[dir][1];
#if CDEF_FULL
  int po3 = cdef_directions[dir][2];
#endif
  int s1o1 = cdef_directions[(dir + 2) & 7][0];
  int s1o2 = cdef_directions[(dir + 2) & 7][1];
  int s2o1 = cdef_directions[(dir + 6) & 7][0];
  int s2o2 = cdef_directions[(dir + 6) & 7][1];

  const int *pri_taps = cdef_pri_taps[pri_strength & 1];
  const int *sec_taps = cdef_sec_taps[pri_strength & 1];

  if (pri_strength)
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  if (sec_strength)
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));
  for (i = 0; i < 4; i += 2) {
    sum = v128_zero();
    row = v128_from_v64(v64_load_aligned(&in[i * CDEF_BSTRIDE]),
                        v64_load_aligned(&in[(i + 1) * CDEF_BSTRIDE]));
#if CDEF_CAP
    min = max = row;
#endif

    // Primary near taps
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + po1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po1]));
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - po1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po1]));
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[0] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[0]), v128_add_16(p0, p1)));

    // Primary far taps
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + po2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po2]));
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - po2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po2]));
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[1] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[1]), v128_add_16(p0, p1)));

#if CDEF_FULL
    // Primary extra taps
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + po3]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + po3]));
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - po3]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - po3]));
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[2] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[2]), v128_add_16(p0, p1)));
#endif

    // Secondary near taps
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + s1o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s1o1]));
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - s1o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s1o1]));
    p2 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + s2o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s2o1]));
    p3 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - s2o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s2o1]));
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p2, v128_cmpeq_16(p2, large))),
                     v128_andn(p3, v128_cmpeq_16(p3, large)));
    min = v128_min_s16(
        v128_min_s16(v128_min_s16(v128_min_s16(min, p0), p1), p2), p3);
#endif
    p0 = constrain16(p0, row, sec_strength, sec_damping);
    p1 = constrain16(p1, row, sec_strength, sec_damping);
    p2 = constrain16(p2, row, sec_strength, sec_damping);
    p3 = constrain16(p3, row, sec_strength, sec_damping);

    // sum += sec_taps[0] * (p0 + p1 + p2 + p3)
    sum = v128_add_16(sum, v128_mullo_s16(v128_dup_16(sec_taps[0]),
                                          v128_add_16(v128_add_16(p0, p1),
                                                      v128_add_16(p2, p3))));

    // Secondary far taps
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + s1o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s1o2]));
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - s1o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s1o2]));
    p2 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + s2o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + s2o2]));
    p3 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - s2o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - s2o2]));
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p2, v128_cmpeq_16(p2, large))),
                     v128_andn(p3, v128_cmpeq_16(p3, large)));
    min = v128_min_s16(
        v128_min_s16(v128_min_s16(v128_min_s16(min, p0), p1), p2), p3);
#endif
    p0 = constrain16(p0, row, sec_strength, sec_damping);
    p1 = constrain16(p1, row, sec_strength, sec_damping);
    p2 = constrain16(p2, row, sec_strength, sec_damping);
    p3 = constrain16(p3, row, sec_strength, sec_damping);

    // sum += sec_taps[1] * (p0 + p1 + p2 + p3)
    sum = v128_add_16(sum, v128_mullo_s16(v128_dup_16(sec_taps[1]),
                                          v128_add_16(v128_add_16(p0, p1),
                                                      v128_add_16(p2, p3))));

    // res = row + ((sum - (sum < 0) + 8) >> 4)
    sum = v128_add_16(sum, v128_cmplt_s16(sum, v128_zero()));
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    res = v128_add_16(row, res);
#if CDEF_CAP
    res = v128_min_s16(v128_max_s16(res, min), max);
#else
    res = v128_min_s16(v128_max_s16(res, v128_zero()), v128_dup_16(max));
#endif
    v64_store_aligned(&dst[i * dstride], v128_high_v64(res));
    v64_store_aligned(&dst[(i + 1) * dstride], v128_low_v64(res));
  }
}

#if CDEF_CAP
void SIMD_FUNC(cdef_filter_block_8x8_16)(uint16_t *dst, int dstride,
                                         const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir,
                                         int pri_damping, int sec_damping,
                                         UNUSED int max_unused)
#else
void SIMD_FUNC(cdef_filter_block_8x8_16)(uint16_t *dst, int dstride,
                                         const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir,
                                         int pri_damping, int sec_damping,
                                         int max)
#endif
{
  int i;
  v128 sum, p0, p1, p2, p3, row, res;
#if CDEF_CAP
  v128 max, min, large = v128_dup_16(CDEF_VERY_LARGE);
#endif
  int po1 = cdef_directions[dir][0];
  int po2 = cdef_directions[dir][1];
#if CDEF_FULL
  int po3 = cdef_directions[dir][2];
#endif
  int s1o1 = cdef_directions[(dir + 2) & 7][0];
  int s1o2 = cdef_directions[(dir + 2) & 7][1];
  int s2o1 = cdef_directions[(dir + 6) & 7][0];
  int s2o2 = cdef_directions[(dir + 6) & 7][1];

  const int *pri_taps = cdef_pri_taps[pri_strength & 1];
  const int *sec_taps = cdef_sec_taps[pri_strength & 1];

  if (pri_strength)
    pri_damping = AOMMAX(0, pri_damping - get_msb(pri_strength));
  if (sec_strength)
    sec_damping = AOMMAX(0, sec_damping - get_msb(sec_strength));

  for (i = 0; i < 8; i++) {
    sum = v128_zero();
    row = v128_load_aligned(&in[i * CDEF_BSTRIDE]);

#if CDEF_CAP
    min = max = row;
#endif
    // Primary near taps
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + po1]);
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - po1]);
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[0] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[0]), v128_add_16(p0, p1)));

    // Primary far taps
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + po2]);
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - po2]);
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[1] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[1]), v128_add_16(p0, p1)));

#if CDEF_FULL
    // Primary extra taps
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + po3]);
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - po3]);
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    min = v128_min_s16(v128_min_s16(min, p0), p1);
#endif
    p0 = constrain16(p0, row, pri_strength, pri_damping);
    p1 = constrain16(p1, row, pri_strength, pri_damping);

    // sum += pri_taps[2] * (p0 + p1)
    sum = v128_add_16(
        sum, v128_mullo_s16(v128_dup_16(pri_taps[2]), v128_add_16(p0, p1)));
#endif

    // Secondary near taps
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + s1o1]);
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - s1o1]);
    p2 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + s2o1]);
    p3 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - s2o1]);
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p2, v128_cmpeq_16(p2, large))),
                     v128_andn(p3, v128_cmpeq_16(p3, large)));
    min = v128_min_s16(
        v128_min_s16(v128_min_s16(v128_min_s16(min, p0), p1), p2), p3);
#endif
    p0 = constrain16(p0, row, sec_strength, sec_damping);
    p1 = constrain16(p1, row, sec_strength, sec_damping);
    p2 = constrain16(p2, row, sec_strength, sec_damping);
    p3 = constrain16(p3, row, sec_strength, sec_damping);

    // sum += sec_taps[0] * (p0 + p1 + p2 + p3)
    sum = v128_add_16(sum, v128_mullo_s16(v128_dup_16(sec_taps[0]),
                                          v128_add_16(v128_add_16(p0, p1),
                                                      v128_add_16(p2, p3))));

    // Secondary far taps
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + s1o2]);
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - s1o2]);
    p2 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + s2o2]);
    p3 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - s2o2]);
#if CDEF_CAP
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p0, v128_cmpeq_16(p0, large))),
                     v128_andn(p1, v128_cmpeq_16(p1, large)));
    max =
        v128_max_s16(v128_max_s16(max, v128_andn(p2, v128_cmpeq_16(p2, large))),
                     v128_andn(p3, v128_cmpeq_16(p3, large)));
    min = v128_min_s16(
        v128_min_s16(v128_min_s16(v128_min_s16(min, p0), p1), p2), p3);
#endif
    p0 = constrain16(p0, row, sec_strength, sec_damping);
    p1 = constrain16(p1, row, sec_strength, sec_damping);
    p2 = constrain16(p2, row, sec_strength, sec_damping);
    p3 = constrain16(p3, row, sec_strength, sec_damping);

    // sum += sec_taps[1] * (p0 + p1 + p2 + p3)
    sum = v128_add_16(sum, v128_mullo_s16(v128_dup_16(sec_taps[1]),
                                          v128_add_16(v128_add_16(p0, p1),
                                                      v128_add_16(p2, p3))));

    // res = row + ((sum - (sum < 0) + 8) >> 4)
    sum = v128_add_16(sum, v128_cmplt_s16(sum, v128_zero()));
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    res = v128_add_16(row, res);
#if CDEF_CAP
    res = v128_min_s16(v128_max_s16(res, min), max);
#else
    res = v128_min_s16(v128_max_s16(res, v128_zero()), v128_dup_16(max));
#endif
    v128_store_unaligned(&dst[i * dstride], res);
  }
}

void SIMD_FUNC(cdef_filter_block)(uint8_t *dst8, uint16_t *dst16, int dstride,
                                  const uint16_t *in, int pri_strength,
                                  int sec_strength, int dir, int pri_damping,
                                  int sec_damping, int bsize, int max) {
  if (dst8)
    (bsize == BLOCK_8X8 ? SIMD_FUNC(cdef_filter_block_8x8_8)
                        : SIMD_FUNC(cdef_filter_block_4x4_8))(
        dst8, dstride, in, pri_strength, sec_strength, dir, pri_damping,
        sec_damping, max);
  else
    (bsize == BLOCK_8X8 ? SIMD_FUNC(cdef_filter_block_8x8_16)
                        : SIMD_FUNC(cdef_filter_block_4x4_16))(
        dst16, dstride, in, pri_strength, sec_strength, dir, pri_damping,
        sec_damping, max);
}

#else

void SIMD_FUNC(cdef_direction_4x4)(uint16_t *y, int ystride, const uint16_t *in,
                                   int threshold, int dir, int damping) {
  int i;
  v128 p0, p1, sum, row, res;
  int o1 = cdef_directions[dir][0];
  int o2 = cdef_directions[dir][1];

  if (threshold) damping -= get_msb(threshold);
  for (i = 0; i < 4; i += 2) {
    sum = v128_zero();
    row = v128_from_v64(v64_load_aligned(&in[i * CDEF_BSTRIDE]),
                        v64_load_aligned(&in[(i + 1) * CDEF_BSTRIDE]));

    // p0 = constrain16(in[i*CDEF_BSTRIDE + offset], row, threshold, damping)
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + o1]));
    p0 = constrain16(p0, row, threshold, damping);

    // p1 = constrain16(in[i*CDEF_BSTRIDE - offset], row, threshold, damping)
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - o1]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - o1]));
    p1 = constrain16(p1, row, threshold, damping);

    // sum += 4 * (p0 + p1)
    sum = v128_add_16(sum, v128_shl_n_16(v128_add_16(p0, p1), 2));

    // p0 = constrain16(in[i*CDEF_BSTRIDE + offset], row, threshold, damping)
    p0 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE + o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE + o2]));
    p0 = constrain16(p0, row, threshold, damping);

    // p1 = constrain16(in[i*CDEF_BSTRIDE - offset], row, threshold, damping)
    p1 = v128_from_v64(v64_load_unaligned(&in[i * CDEF_BSTRIDE - o2]),
                       v64_load_unaligned(&in[(i + 1) * CDEF_BSTRIDE - o2]));
    p1 = constrain16(p1, row, threshold, damping);

    // sum += 1 * (p0 + p1)
    sum = v128_add_16(sum, v128_add_16(p0, p1));

    // res = row + ((sum + 8) >> 4)
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    res = v128_add_16(row, res);
    v64_store_aligned(&y[i * ystride], v128_high_v64(res));
    v64_store_aligned(&y[(i + 1) * ystride], v128_low_v64(res));
  }
}

void SIMD_FUNC(cdef_direction_8x8)(uint16_t *y, int ystride, const uint16_t *in,
                                   int threshold, int dir, int damping) {
  int i;
  v128 sum, p0, p1, row, res;
  int o1 = cdef_directions[dir][0];
  int o2 = cdef_directions[dir][1];
  int o3 = cdef_directions[dir][2];

  if (threshold) damping -= get_msb(threshold);
  for (i = 0; i < 8; i++) {
    sum = v128_zero();
    row = v128_load_aligned(&in[i * CDEF_BSTRIDE]);

    // p0 = constrain16(in[i*CDEF_BSTRIDE + offset], row, threshold, damping)
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + o1]);
    p0 = constrain16(p0, row, threshold, damping);

    // p1 = constrain16(in[i*CDEF_BSTRIDE - offset], row, threshold, damping)
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - o1]);
    p1 = constrain16(p1, row, threshold, damping);

    // sum += 3 * (p0 + p1)
    p0 = v128_add_16(p0, p1);
    p0 = v128_add_16(p0, v128_shl_n_16(p0, 1));
    sum = v128_add_16(sum, p0);

    // p0 = constrain16(in[i*CDEF_BSTRIDE + offset], row, threshold, damping)
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + o2]);
    p0 = constrain16(p0, row, threshold, damping);

    // p1 = constrain16(in[i*CDEF_BSTRIDE - offset], row, threshold, damping)
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - o2]);
    p1 = constrain16(p1, row, threshold, damping);

    // sum += 2 * (p0 + p1)
    p0 = v128_shl_n_16(v128_add_16(p0, p1), 1);
    sum = v128_add_16(sum, p0);

    // p0 = constrain16(in[i*CDEF_BSTRIDE + offset], row, threshold, damping)
    p0 = v128_load_unaligned(&in[i * CDEF_BSTRIDE + o3]);
    p0 = constrain16(p0, row, threshold, damping);

    // p1 = constrain16(in[i*CDEF_BSTRIDE - offset], row, threshold, damping)
    p1 = v128_load_unaligned(&in[i * CDEF_BSTRIDE - o3]);
    p1 = constrain16(p1, row, threshold, damping);

    // sum += (p0 + p1)
    p0 = v128_add_16(p0, p1);
    sum = v128_add_16(sum, p0);

    // res = row + ((sum + 8) >> 4)
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    res = v128_add_16(row, res);
    v128_store_unaligned(&y[i * ystride], res);
  }
}

void SIMD_FUNC(copy_8x8_16bit_to_8bit)(uint8_t *dst, int dstride,
                                       const uint16_t *src, int sstride) {
  int i;
  for (i = 0; i < 8; i++) {
    v128 row = v128_load_unaligned(&src[i * sstride]);
    row = v128_pack_s16_u8(row, row);
    v64_store_unaligned(&dst[i * dstride], v128_low_v64(row));
  }
}

void SIMD_FUNC(copy_4x4_16bit_to_8bit)(uint8_t *dst, int dstride,
                                       const uint16_t *src, int sstride) {
  int i;
  for (i = 0; i < 4; i++) {
    v128 row = v128_load_unaligned(&src[i * sstride]);
    row = v128_pack_s16_u8(row, row);
    u32_store_unaligned(&dst[i * dstride], v128_low_u32(row));
  }
}

void SIMD_FUNC(copy_8x8_16bit_to_16bit)(uint16_t *dst, int dstride,
                                        const uint16_t *src, int sstride) {
  int i;
  for (i = 0; i < 8; i++) {
    v128 row = v128_load_unaligned(&src[i * sstride]);
    v128_store_unaligned(&dst[i * dstride], row);
  }
}

void SIMD_FUNC(copy_4x4_16bit_to_16bit)(uint16_t *dst, int dstride,
                                        const uint16_t *src, int sstride) {
  int i;
  for (i = 0; i < 4; i++) {
    v64 row = v64_load_unaligned(&src[i * sstride]);
    v64_store_unaligned(&dst[i * dstride], row);
  }
}
#endif

void SIMD_FUNC(copy_rect8_8bit_to_16bit)(uint16_t *dst, int dstride,
                                         const uint8_t *src, int sstride, int v,
                                         int h) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < (h & ~0x7); j += 8) {
      v64 row = v64_load_unaligned(&src[i * sstride + j]);
      v128_store_unaligned(&dst[i * dstride + j], v128_unpack_u8_s16(row));
    }
    for (; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void SIMD_FUNC(copy_rect8_16bit_to_16bit)(uint16_t *dst, int dstride,
                                          const uint16_t *src, int sstride,
                                          int v, int h) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < (h & ~0x7); j += 8) {
      v128 row = v128_load_unaligned(&src[i * sstride + j]);
      v128_store_unaligned(&dst[i * dstride + j], row);
    }
    for (; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}
