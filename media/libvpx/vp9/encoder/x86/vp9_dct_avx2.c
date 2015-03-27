/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>  // AVX2
#include "vp9/common/vp9_idct.h"  // for cospi constants
#include "vpx_ports/mem.h"

void vp9_fdct4x4_avx2(const int16_t *input, int16_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows. To achieve that,
  // as the first pass results are transposed, we tranpose the columns (that
  // is the transposed rows) and transpose the results (so that it goes back
  // in normal/row positions).
  int pass;
  // Constants
  //    When we use them, in one case, they are all the same. In all others
  //    it's a pair of them that we need to repeat four times. This is done
  //    by constructing the 32 bit constant corresponding to that pair.
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i k__nonzero_bias_a = _mm_setr_epi16(0, 1, 1, 1, 1, 1, 1, 1);
  const __m128i k__nonzero_bias_b = _mm_setr_epi16(1, 0, 0, 0, 0, 0, 0, 0);
  const __m128i kOne = _mm_set1_epi16(1);
  __m128i in0, in1, in2, in3;
  // Load inputs.
  {
    in0  = _mm_loadl_epi64((const __m128i *)(input +  0 * stride));
    in1  = _mm_loadl_epi64((const __m128i *)(input +  1 * stride));
    in2  = _mm_loadl_epi64((const __m128i *)(input +  2 * stride));
    in3  = _mm_loadl_epi64((const __m128i *)(input +  3 * stride));
    // x = x << 4
    in0 = _mm_slli_epi16(in0, 4);
    in1 = _mm_slli_epi16(in1, 4);
    in2 = _mm_slli_epi16(in2, 4);
    in3 = _mm_slli_epi16(in3, 4);
    // if (i == 0 && input[0]) input[0] += 1;
    {
      // The mask will only contain wether the first value is zero, all
      // other comparison will fail as something shifted by 4 (above << 4)
      // can never be equal to one. To increment in the non-zero case, we
      // add the mask and one for the first element:
      //   - if zero, mask = -1, v = v - 1 + 1 = v
      //   - if non-zero, mask = 0, v = v + 0 + 1 = v + 1
      __m128i mask = _mm_cmpeq_epi16(in0, k__nonzero_bias_a);
      in0 = _mm_add_epi16(in0, mask);
      in0 = _mm_add_epi16(in0, k__nonzero_bias_b);
    }
  }
  // Do the two transform/transpose passes
  for (pass = 0; pass < 2; ++pass) {
    // Transform 1/2: Add/substract
    const __m128i r0 = _mm_add_epi16(in0, in3);
    const __m128i r1 = _mm_add_epi16(in1, in2);
    const __m128i r2 = _mm_sub_epi16(in1, in2);
    const __m128i r3 = _mm_sub_epi16(in0, in3);
    // Transform 1/2: Interleave to do the multiply by constants which gets us
    //                into 32 bits.
    const __m128i t0 = _mm_unpacklo_epi16(r0, r1);
    const __m128i t2 = _mm_unpacklo_epi16(r2, r3);
    const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_p16);
    const __m128i u2 = _mm_madd_epi16(t0, k__cospi_p16_m16);
    const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p24_p08);
    const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m08_p24);
    const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
    const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
    const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
    const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
    const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
    const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
    const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
    const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
    // Combine and transpose
    const __m128i res0 = _mm_packs_epi32(w0, w2);
    const __m128i res1 = _mm_packs_epi32(w4, w6);
    // 00 01 02 03 20 21 22 23
    // 10 11 12 13 30 31 32 33
    const __m128i tr0_0 = _mm_unpacklo_epi16(res0, res1);
    const __m128i tr0_1 = _mm_unpackhi_epi16(res0, res1);
    // 00 10 01 11 02 12 03 13
    // 20 30 21 31 22 32 23 33
    in0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
    in2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
    // 00 10 20 30 01 11 21 31      in0 contains 0 followed by 1
    // 02 12 22 32 03 13 23 33      in2 contains 2 followed by 3
    if (0 == pass) {
      // Extract values in the high part for second pass as transform code
      // only uses the first four values.
      in1 = _mm_unpackhi_epi64(in0, in0);
      in3 = _mm_unpackhi_epi64(in2, in2);
    } else {
      // Post-condition output and store it (v + 1) >> 2, taking advantage
      // of the fact 1/3 are stored just after 0/2.
      __m128i out01 = _mm_add_epi16(in0, kOne);
      __m128i out23 = _mm_add_epi16(in2, kOne);
      out01 = _mm_srai_epi16(out01, 2);
      out23 = _mm_srai_epi16(out23, 2);
      _mm_storeu_si128((__m128i *)(output + 0 * 4), out01);
      _mm_storeu_si128((__m128i *)(output + 2 * 4), out23);
    }
  }
}

static INLINE void load_buffer_4x4_avx2(const int16_t *input, __m128i *in,
                                   int stride) {
  const __m128i k__nonzero_bias_a = _mm_setr_epi16(0, 1, 1, 1, 1, 1, 1, 1);
  const __m128i k__nonzero_bias_b = _mm_setr_epi16(1, 0, 0, 0, 0, 0, 0, 0);
  __m128i mask;

  in[0] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
  in[1] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
  in[2] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
  in[3] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));

  in[0] = _mm_slli_epi16(in[0], 4);
  in[1] = _mm_slli_epi16(in[1], 4);
  in[2] = _mm_slli_epi16(in[2], 4);
  in[3] = _mm_slli_epi16(in[3], 4);

  mask = _mm_cmpeq_epi16(in[0], k__nonzero_bias_a);
  in[0] = _mm_add_epi16(in[0], mask);
  in[0] = _mm_add_epi16(in[0], k__nonzero_bias_b);
}

static INLINE void write_buffer_4x4_avx2(int16_t *output, __m128i *res) {
  const __m128i kOne = _mm_set1_epi16(1);
  __m128i in01 = _mm_unpacklo_epi64(res[0], res[1]);
  __m128i in23 = _mm_unpacklo_epi64(res[2], res[3]);
  __m128i out01 = _mm_add_epi16(in01, kOne);
  __m128i out23 = _mm_add_epi16(in23, kOne);
  out01 = _mm_srai_epi16(out01, 2);
  out23 = _mm_srai_epi16(out23, 2);
  _mm_store_si128((__m128i *)(output + 0 * 8), out01);
  _mm_store_si128((__m128i *)(output + 1 * 8), out23);
}

static INLINE void transpose_4x4_avx2(__m128i *res) {
  // Combine and transpose
  // 00 01 02 03 20 21 22 23
  // 10 11 12 13 30 31 32 33
  const __m128i tr0_0 = _mm_unpacklo_epi16(res[0], res[1]);
  const __m128i tr0_1 = _mm_unpackhi_epi16(res[0], res[1]);

  // 00 10 01 11 02 12 03 13
  // 20 30 21 31 22 32 23 33
  res[0] = _mm_unpacklo_epi32(tr0_0, tr0_1);
  res[2] = _mm_unpackhi_epi32(tr0_0, tr0_1);

  // 00 10 20 30 01 11 21 31
  // 02 12 22 32 03 13 23 33
  // only use the first 4 16-bit integers
  res[1] = _mm_unpackhi_epi64(res[0], res[0]);
  res[3] = _mm_unpackhi_epi64(res[2], res[2]);
}

void fdct4_avx2(__m128i *in) {
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i u[4], v[4];
  u[0]=_mm_unpacklo_epi16(in[0], in[1]);
  u[1]=_mm_unpacklo_epi16(in[3], in[2]);

  v[0] = _mm_add_epi16(u[0], u[1]);
  v[1] = _mm_sub_epi16(u[0], u[1]);

  u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);  // 0
  u[1] = _mm_madd_epi16(v[0], k__cospi_p16_m16);  // 2
  u[2] = _mm_madd_epi16(v[1], k__cospi_p08_p24);  // 1
  u[3] = _mm_madd_epi16(v[1], k__cospi_p24_m08);  // 3

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[1]);
  in[1] = _mm_packs_epi32(u[2], u[3]);
  transpose_4x4_avx2(in);
}

void fadst4_avx2(__m128i *in) {
  const __m128i k__sinpi_p01_p02 = pair_set_epi16(sinpi_1_9, sinpi_2_9);
  const __m128i k__sinpi_p04_m01 = pair_set_epi16(sinpi_4_9, -sinpi_1_9);
  const __m128i k__sinpi_p03_p04 = pair_set_epi16(sinpi_3_9, sinpi_4_9);
  const __m128i k__sinpi_m03_p02 = pair_set_epi16(-sinpi_3_9, sinpi_2_9);
  const __m128i k__sinpi_p03_p03 = _mm_set1_epi16(sinpi_3_9);
  const __m128i kZero = _mm_set1_epi16(0);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  __m128i u[8], v[8];
  __m128i in7 = _mm_add_epi16(in[0], in[1]);

  u[0] = _mm_unpacklo_epi16(in[0], in[1]);
  u[1] = _mm_unpacklo_epi16(in[2], in[3]);
  u[2] = _mm_unpacklo_epi16(in7, kZero);
  u[3] = _mm_unpacklo_epi16(in[2], kZero);
  u[4] = _mm_unpacklo_epi16(in[3], kZero);

  v[0] = _mm_madd_epi16(u[0], k__sinpi_p01_p02);  // s0 + s2
  v[1] = _mm_madd_epi16(u[1], k__sinpi_p03_p04);  // s4 + s5
  v[2] = _mm_madd_epi16(u[2], k__sinpi_p03_p03);  // x1
  v[3] = _mm_madd_epi16(u[0], k__sinpi_p04_m01);  // s1 - s3
  v[4] = _mm_madd_epi16(u[1], k__sinpi_m03_p02);  // -s4 + s6
  v[5] = _mm_madd_epi16(u[3], k__sinpi_p03_p03);  // s4
  v[6] = _mm_madd_epi16(u[4], k__sinpi_p03_p03);

  u[0] = _mm_add_epi32(v[0], v[1]);
  u[1] = _mm_sub_epi32(v[2], v[6]);
  u[2] = _mm_add_epi32(v[3], v[4]);
  u[3] = _mm_sub_epi32(u[2], u[0]);
  u[4] = _mm_slli_epi32(v[5], 2);
  u[5] = _mm_sub_epi32(u[4], v[5]);
  u[6] = _mm_add_epi32(u[3], u[5]);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[2]);
  in[1] = _mm_packs_epi32(u[1], u[3]);
  transpose_4x4_avx2(in);
}

void vp9_fht4x4_avx2(const int16_t *input, int16_t *output,
                     int stride, int tx_type) {
  __m128i in[4];

  switch (tx_type) {
    case DCT_DCT:
      vp9_fdct4x4_avx2(input, output, stride);
      break;
    case ADST_DCT:
      load_buffer_4x4_avx2(input, in, stride);
      fadst4_avx2(in);
      fdct4_avx2(in);
      write_buffer_4x4_avx2(output, in);
      break;
    case DCT_ADST:
      load_buffer_4x4_avx2(input, in, stride);
      fdct4_avx2(in);
      fadst4_avx2(in);
      write_buffer_4x4_avx2(output, in);
      break;
    case ADST_ADST:
      load_buffer_4x4_avx2(input, in, stride);
      fadst4_avx2(in);
      fadst4_avx2(in);
      write_buffer_4x4_avx2(output, in);
      break;
    default:
      assert(0);
      break;
  }
}

void vp9_fdct8x8_avx2(const int16_t *input, int16_t *output, int stride) {
  int pass;
  // Constants
  //    When we use them, in one case, they are all the same. In all others
  //    it's a pair of them that we need to repeat four times. This is done
  //    by constructing the 32 bit constant corresponding to that pair.
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  // Load input
  __m128i in0  = _mm_load_si128((const __m128i *)(input + 0 * stride));
  __m128i in1  = _mm_load_si128((const __m128i *)(input + 1 * stride));
  __m128i in2  = _mm_load_si128((const __m128i *)(input + 2 * stride));
  __m128i in3  = _mm_load_si128((const __m128i *)(input + 3 * stride));
  __m128i in4  = _mm_load_si128((const __m128i *)(input + 4 * stride));
  __m128i in5  = _mm_load_si128((const __m128i *)(input + 5 * stride));
  __m128i in6  = _mm_load_si128((const __m128i *)(input + 6 * stride));
  __m128i in7  = _mm_load_si128((const __m128i *)(input + 7 * stride));
  // Pre-condition input (shift by two)
  in0 = _mm_slli_epi16(in0, 2);
  in1 = _mm_slli_epi16(in1, 2);
  in2 = _mm_slli_epi16(in2, 2);
  in3 = _mm_slli_epi16(in3, 2);
  in4 = _mm_slli_epi16(in4, 2);
  in5 = _mm_slli_epi16(in5, 2);
  in6 = _mm_slli_epi16(in6, 2);
  in7 = _mm_slli_epi16(in7, 2);

  // We do two passes, first the columns, then the rows. The results of the
  // first pass are transposed so that the same column code can be reused. The
  // results of the second pass are also transposed so that the rows (processed
  // as columns) are put back in row positions.
  for (pass = 0; pass < 2; pass++) {
    // To store results of each pass before the transpose.
    __m128i res0, res1, res2, res3, res4, res5, res6, res7;
    // Add/substract
    const __m128i q0 = _mm_add_epi16(in0, in7);
    const __m128i q1 = _mm_add_epi16(in1, in6);
    const __m128i q2 = _mm_add_epi16(in2, in5);
    const __m128i q3 = _mm_add_epi16(in3, in4);
    const __m128i q4 = _mm_sub_epi16(in3, in4);
    const __m128i q5 = _mm_sub_epi16(in2, in5);
    const __m128i q6 = _mm_sub_epi16(in1, in6);
    const __m128i q7 = _mm_sub_epi16(in0, in7);
    // Work on first four results
    {
      // Add/substract
      const __m128i r0 = _mm_add_epi16(q0, q3);
      const __m128i r1 = _mm_add_epi16(q1, q2);
      const __m128i r2 = _mm_sub_epi16(q1, q2);
      const __m128i r3 = _mm_sub_epi16(q0, q3);
      // Interleave to do the multiply by constants which gets us into 32bits
      const __m128i t0 = _mm_unpacklo_epi16(r0, r1);
      const __m128i t1 = _mm_unpackhi_epi16(r0, r1);
      const __m128i t2 = _mm_unpacklo_epi16(r2, r3);
      const __m128i t3 = _mm_unpackhi_epi16(r2, r3);
      const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_p16);
      const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p16_p16);
      const __m128i u2 = _mm_madd_epi16(t0, k__cospi_p16_m16);
      const __m128i u3 = _mm_madd_epi16(t1, k__cospi_p16_m16);
      const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p24_p08);
      const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p24_p08);
      const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m08_p24);
      const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m08_p24);
      // dct_const_round_shift
      const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
      const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
      const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
      const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
      const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
      const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
      const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
      const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
      const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
      const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
      const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
      const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
      const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
      const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
      const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
      const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
      // Combine
      res0 = _mm_packs_epi32(w0, w1);
      res4 = _mm_packs_epi32(w2, w3);
      res2 = _mm_packs_epi32(w4, w5);
      res6 = _mm_packs_epi32(w6, w7);
    }
    // Work on next four results
    {
      // Interleave to do the multiply by constants which gets us into 32bits
      const __m128i d0 = _mm_unpacklo_epi16(q6, q5);
      const __m128i d1 = _mm_unpackhi_epi16(q6, q5);
      const __m128i e0 = _mm_madd_epi16(d0, k__cospi_p16_m16);
      const __m128i e1 = _mm_madd_epi16(d1, k__cospi_p16_m16);
      const __m128i e2 = _mm_madd_epi16(d0, k__cospi_p16_p16);
      const __m128i e3 = _mm_madd_epi16(d1, k__cospi_p16_p16);
      // dct_const_round_shift
      const __m128i f0 = _mm_add_epi32(e0, k__DCT_CONST_ROUNDING);
      const __m128i f1 = _mm_add_epi32(e1, k__DCT_CONST_ROUNDING);
      const __m128i f2 = _mm_add_epi32(e2, k__DCT_CONST_ROUNDING);
      const __m128i f3 = _mm_add_epi32(e3, k__DCT_CONST_ROUNDING);
      const __m128i s0 = _mm_srai_epi32(f0, DCT_CONST_BITS);
      const __m128i s1 = _mm_srai_epi32(f1, DCT_CONST_BITS);
      const __m128i s2 = _mm_srai_epi32(f2, DCT_CONST_BITS);
      const __m128i s3 = _mm_srai_epi32(f3, DCT_CONST_BITS);
      // Combine
      const __m128i r0 = _mm_packs_epi32(s0, s1);
      const __m128i r1 = _mm_packs_epi32(s2, s3);
      // Add/substract
      const __m128i x0 = _mm_add_epi16(q4, r0);
      const __m128i x1 = _mm_sub_epi16(q4, r0);
      const __m128i x2 = _mm_sub_epi16(q7, r1);
      const __m128i x3 = _mm_add_epi16(q7, r1);
      // Interleave to do the multiply by constants which gets us into 32bits
      const __m128i t0 = _mm_unpacklo_epi16(x0, x3);
      const __m128i t1 = _mm_unpackhi_epi16(x0, x3);
      const __m128i t2 = _mm_unpacklo_epi16(x1, x2);
      const __m128i t3 = _mm_unpackhi_epi16(x1, x2);
      const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p28_p04);
      const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p28_p04);
      const __m128i u2 = _mm_madd_epi16(t0, k__cospi_m04_p28);
      const __m128i u3 = _mm_madd_epi16(t1, k__cospi_m04_p28);
      const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p12_p20);
      const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p12_p20);
      const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m20_p12);
      const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m20_p12);
      // dct_const_round_shift
      const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
      const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
      const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
      const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
      const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
      const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
      const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
      const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
      const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
      const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
      const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
      const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
      const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
      const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
      const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
      const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
      // Combine
      res1 = _mm_packs_epi32(w0, w1);
      res7 = _mm_packs_epi32(w2, w3);
      res5 = _mm_packs_epi32(w4, w5);
      res3 = _mm_packs_epi32(w6, w7);
    }
    // Transpose the 8x8.
    {
      // 00 01 02 03 04 05 06 07
      // 10 11 12 13 14 15 16 17
      // 20 21 22 23 24 25 26 27
      // 30 31 32 33 34 35 36 37
      // 40 41 42 43 44 45 46 47
      // 50 51 52 53 54 55 56 57
      // 60 61 62 63 64 65 66 67
      // 70 71 72 73 74 75 76 77
      const __m128i tr0_0 = _mm_unpacklo_epi16(res0, res1);
      const __m128i tr0_1 = _mm_unpacklo_epi16(res2, res3);
      const __m128i tr0_2 = _mm_unpackhi_epi16(res0, res1);
      const __m128i tr0_3 = _mm_unpackhi_epi16(res2, res3);
      const __m128i tr0_4 = _mm_unpacklo_epi16(res4, res5);
      const __m128i tr0_5 = _mm_unpacklo_epi16(res6, res7);
      const __m128i tr0_6 = _mm_unpackhi_epi16(res4, res5);
      const __m128i tr0_7 = _mm_unpackhi_epi16(res6, res7);
      // 00 10 01 11 02 12 03 13
      // 20 30 21 31 22 32 23 33
      // 04 14 05 15 06 16 07 17
      // 24 34 25 35 26 36 27 37
      // 40 50 41 51 42 52 43 53
      // 60 70 61 71 62 72 63 73
      // 54 54 55 55 56 56 57 57
      // 64 74 65 75 66 76 67 77
      const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
      const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
      const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
      const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
      const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
      const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
      const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
      const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
      // 00 10 20 30 01 11 21 31
      // 40 50 60 70 41 51 61 71
      // 02 12 22 32 03 13 23 33
      // 42 52 62 72 43 53 63 73
      // 04 14 24 34 05 15 21 36
      // 44 54 64 74 45 55 61 76
      // 06 16 26 36 07 17 27 37
      // 46 56 66 76 47 57 67 77
      in0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
      in1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
      in2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
      in3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
      in4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
      in5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
      in6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
      in7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
      // 00 10 20 30 40 50 60 70
      // 01 11 21 31 41 51 61 71
      // 02 12 22 32 42 52 62 72
      // 03 13 23 33 43 53 63 73
      // 04 14 24 34 44 54 64 74
      // 05 15 25 35 45 55 65 75
      // 06 16 26 36 46 56 66 76
      // 07 17 27 37 47 57 67 77
    }
  }
  // Post-condition output and store it
  {
    // Post-condition (division by two)
    //    division of two 16 bits signed numbers using shifts
    //    n / 2 = (n - (n >> 15)) >> 1
    const __m128i sign_in0 = _mm_srai_epi16(in0, 15);
    const __m128i sign_in1 = _mm_srai_epi16(in1, 15);
    const __m128i sign_in2 = _mm_srai_epi16(in2, 15);
    const __m128i sign_in3 = _mm_srai_epi16(in3, 15);
    const __m128i sign_in4 = _mm_srai_epi16(in4, 15);
    const __m128i sign_in5 = _mm_srai_epi16(in5, 15);
    const __m128i sign_in6 = _mm_srai_epi16(in6, 15);
    const __m128i sign_in7 = _mm_srai_epi16(in7, 15);
    in0 = _mm_sub_epi16(in0, sign_in0);
    in1 = _mm_sub_epi16(in1, sign_in1);
    in2 = _mm_sub_epi16(in2, sign_in2);
    in3 = _mm_sub_epi16(in3, sign_in3);
    in4 = _mm_sub_epi16(in4, sign_in4);
    in5 = _mm_sub_epi16(in5, sign_in5);
    in6 = _mm_sub_epi16(in6, sign_in6);
    in7 = _mm_sub_epi16(in7, sign_in7);
    in0 = _mm_srai_epi16(in0, 1);
    in1 = _mm_srai_epi16(in1, 1);
    in2 = _mm_srai_epi16(in2, 1);
    in3 = _mm_srai_epi16(in3, 1);
    in4 = _mm_srai_epi16(in4, 1);
    in5 = _mm_srai_epi16(in5, 1);
    in6 = _mm_srai_epi16(in6, 1);
    in7 = _mm_srai_epi16(in7, 1);
    // store results
    _mm_store_si128((__m128i *)(output + 0 * 8), in0);
    _mm_store_si128((__m128i *)(output + 1 * 8), in1);
    _mm_store_si128((__m128i *)(output + 2 * 8), in2);
    _mm_store_si128((__m128i *)(output + 3 * 8), in3);
    _mm_store_si128((__m128i *)(output + 4 * 8), in4);
    _mm_store_si128((__m128i *)(output + 5 * 8), in5);
    _mm_store_si128((__m128i *)(output + 6 * 8), in6);
    _mm_store_si128((__m128i *)(output + 7 * 8), in7);
  }
}

// load 8x8 array
static INLINE void load_buffer_8x8_avx2(const int16_t *input, __m128i *in,
                                   int stride) {
  in[0]  = _mm_load_si128((const __m128i *)(input + 0 * stride));
  in[1]  = _mm_load_si128((const __m128i *)(input + 1 * stride));
  in[2]  = _mm_load_si128((const __m128i *)(input + 2 * stride));
  in[3]  = _mm_load_si128((const __m128i *)(input + 3 * stride));
  in[4]  = _mm_load_si128((const __m128i *)(input + 4 * stride));
  in[5]  = _mm_load_si128((const __m128i *)(input + 5 * stride));
  in[6]  = _mm_load_si128((const __m128i *)(input + 6 * stride));
  in[7]  = _mm_load_si128((const __m128i *)(input + 7 * stride));

  in[0] = _mm_slli_epi16(in[0], 2);
  in[1] = _mm_slli_epi16(in[1], 2);
  in[2] = _mm_slli_epi16(in[2], 2);
  in[3] = _mm_slli_epi16(in[3], 2);
  in[4] = _mm_slli_epi16(in[4], 2);
  in[5] = _mm_slli_epi16(in[5], 2);
  in[6] = _mm_slli_epi16(in[6], 2);
  in[7] = _mm_slli_epi16(in[7], 2);
}

// right shift and rounding
static INLINE void right_shift_8x8_avx2(__m128i *res, int const bit) {
  const __m128i kOne = _mm_set1_epi16(1);
  const int bit_m02 = bit - 2;
  __m128i sign0 = _mm_srai_epi16(res[0], 15);
  __m128i sign1 = _mm_srai_epi16(res[1], 15);
  __m128i sign2 = _mm_srai_epi16(res[2], 15);
  __m128i sign3 = _mm_srai_epi16(res[3], 15);
  __m128i sign4 = _mm_srai_epi16(res[4], 15);
  __m128i sign5 = _mm_srai_epi16(res[5], 15);
  __m128i sign6 = _mm_srai_epi16(res[6], 15);
  __m128i sign7 = _mm_srai_epi16(res[7], 15);

  if (bit_m02 >= 0) {
    __m128i k_const_rounding = _mm_slli_epi16(kOne, bit_m02);
    res[0] = _mm_add_epi16(res[0], k_const_rounding);
    res[1] = _mm_add_epi16(res[1], k_const_rounding);
    res[2] = _mm_add_epi16(res[2], k_const_rounding);
    res[3] = _mm_add_epi16(res[3], k_const_rounding);
    res[4] = _mm_add_epi16(res[4], k_const_rounding);
    res[5] = _mm_add_epi16(res[5], k_const_rounding);
    res[6] = _mm_add_epi16(res[6], k_const_rounding);
    res[7] = _mm_add_epi16(res[7], k_const_rounding);
  }

  res[0] = _mm_sub_epi16(res[0], sign0);
  res[1] = _mm_sub_epi16(res[1], sign1);
  res[2] = _mm_sub_epi16(res[2], sign2);
  res[3] = _mm_sub_epi16(res[3], sign3);
  res[4] = _mm_sub_epi16(res[4], sign4);
  res[5] = _mm_sub_epi16(res[5], sign5);
  res[6] = _mm_sub_epi16(res[6], sign6);
  res[7] = _mm_sub_epi16(res[7], sign7);

  res[0] = _mm_srai_epi16(res[0], bit);
  res[1] = _mm_srai_epi16(res[1], bit);
  res[2] = _mm_srai_epi16(res[2], bit);
  res[3] = _mm_srai_epi16(res[3], bit);
  res[4] = _mm_srai_epi16(res[4], bit);
  res[5] = _mm_srai_epi16(res[5], bit);
  res[6] = _mm_srai_epi16(res[6], bit);
  res[7] = _mm_srai_epi16(res[7], bit);
}

// write 8x8 array
static INLINE void write_buffer_8x8_avx2(int16_t *output, __m128i *res, int stride) {
  _mm_store_si128((__m128i *)(output + 0 * stride), res[0]);
  _mm_store_si128((__m128i *)(output + 1 * stride), res[1]);
  _mm_store_si128((__m128i *)(output + 2 * stride), res[2]);
  _mm_store_si128((__m128i *)(output + 3 * stride), res[3]);
  _mm_store_si128((__m128i *)(output + 4 * stride), res[4]);
  _mm_store_si128((__m128i *)(output + 5 * stride), res[5]);
  _mm_store_si128((__m128i *)(output + 6 * stride), res[6]);
  _mm_store_si128((__m128i *)(output + 7 * stride), res[7]);
}

// perform in-place transpose
static INLINE void array_transpose_8x8_avx2(__m128i *in, __m128i *res) {
  const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
  const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
  const __m128i tr0_2 = _mm_unpackhi_epi16(in[0], in[1]);
  const __m128i tr0_3 = _mm_unpackhi_epi16(in[2], in[3]);
  const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
  const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);
  const __m128i tr0_6 = _mm_unpackhi_epi16(in[4], in[5]);
  const __m128i tr0_7 = _mm_unpackhi_epi16(in[6], in[7]);
  // 00 10 01 11 02 12 03 13
  // 20 30 21 31 22 32 23 33
  // 04 14 05 15 06 16 07 17
  // 24 34 25 35 26 36 27 37
  // 40 50 41 51 42 52 43 53
  // 60 70 61 71 62 72 63 73
  // 44 54 45 55 46 56 47 57
  // 64 74 65 75 66 76 67 77
  const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
  const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_4, tr0_5);
  const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
  const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_4, tr0_5);
  const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_2, tr0_3);
  const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
  const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_2, tr0_3);
  const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
  // 00 10 20 30 01 11 21 31
  // 40 50 60 70 41 51 61 71
  // 02 12 22 32 03 13 23 33
  // 42 52 62 72 43 53 63 73
  // 04 14 24 34 05 15 25 35
  // 44 54 64 74 45 55 65 75
  // 06 16 26 36 07 17 27 37
  // 46 56 66 76 47 57 67 77
  res[0] = _mm_unpacklo_epi64(tr1_0, tr1_1);
  res[1] = _mm_unpackhi_epi64(tr1_0, tr1_1);
  res[2] = _mm_unpacklo_epi64(tr1_2, tr1_3);
  res[3] = _mm_unpackhi_epi64(tr1_2, tr1_3);
  res[4] = _mm_unpacklo_epi64(tr1_4, tr1_5);
  res[5] = _mm_unpackhi_epi64(tr1_4, tr1_5);
  res[6] = _mm_unpacklo_epi64(tr1_6, tr1_7);
  res[7] = _mm_unpackhi_epi64(tr1_6, tr1_7);
  // 00 10 20 30 40 50 60 70
  // 01 11 21 31 41 51 61 71
  // 02 12 22 32 42 52 62 72
  // 03 13 23 33 43 53 63 73
  // 04 14 24 34 44 54 64 74
  // 05 15 25 35 45 55 65 75
  // 06 16 26 36 46 56 66 76
  // 07 17 27 37 47 57 67 77
}

void fdct8_avx2(__m128i *in) {
  // constants
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;
  __m128i s0, s1, s2, s3, s4, s5, s6, s7;

  // stage 1
  s0 = _mm_add_epi16(in[0], in[7]);
  s1 = _mm_add_epi16(in[1], in[6]);
  s2 = _mm_add_epi16(in[2], in[5]);
  s3 = _mm_add_epi16(in[3], in[4]);
  s4 = _mm_sub_epi16(in[3], in[4]);
  s5 = _mm_sub_epi16(in[2], in[5]);
  s6 = _mm_sub_epi16(in[1], in[6]);
  s7 = _mm_sub_epi16(in[0], in[7]);

  u0 = _mm_add_epi16(s0, s3);
  u1 = _mm_add_epi16(s1, s2);
  u2 = _mm_sub_epi16(s1, s2);
  u3 = _mm_sub_epi16(s0, s3);
  // interleave and perform butterfly multiplication/addition
  v0 = _mm_unpacklo_epi16(u0, u1);
  v1 = _mm_unpackhi_epi16(u0, u1);
  v2 = _mm_unpacklo_epi16(u2, u3);
  v3 = _mm_unpackhi_epi16(u2, u3);

  u0 = _mm_madd_epi16(v0, k__cospi_p16_p16);
  u1 = _mm_madd_epi16(v1, k__cospi_p16_p16);
  u2 = _mm_madd_epi16(v0, k__cospi_p16_m16);
  u3 = _mm_madd_epi16(v1, k__cospi_p16_m16);
  u4 = _mm_madd_epi16(v2, k__cospi_p24_p08);
  u5 = _mm_madd_epi16(v3, k__cospi_p24_p08);
  u6 = _mm_madd_epi16(v2, k__cospi_m08_p24);
  u7 = _mm_madd_epi16(v3, k__cospi_m08_p24);

  // shift and rounding
  v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u0, u1);
  in[2] = _mm_packs_epi32(u4, u5);
  in[4] = _mm_packs_epi32(u2, u3);
  in[6] = _mm_packs_epi32(u6, u7);

  // stage 2
  // interleave and perform butterfly multiplication/addition
  u0 = _mm_unpacklo_epi16(s6, s5);
  u1 = _mm_unpackhi_epi16(s6, s5);
  v0 = _mm_madd_epi16(u0, k__cospi_p16_m16);
  v1 = _mm_madd_epi16(u1, k__cospi_p16_m16);
  v2 = _mm_madd_epi16(u0, k__cospi_p16_p16);
  v3 = _mm_madd_epi16(u1, k__cospi_p16_p16);

  // shift and rounding
  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);

  u0 = _mm_packs_epi32(v0, v1);
  u1 = _mm_packs_epi32(v2, v3);

  // stage 3
  s0 = _mm_add_epi16(s4, u0);
  s1 = _mm_sub_epi16(s4, u0);
  s2 = _mm_sub_epi16(s7, u1);
  s3 = _mm_add_epi16(s7, u1);

  // stage 4
  u0 = _mm_unpacklo_epi16(s0, s3);
  u1 = _mm_unpackhi_epi16(s0, s3);
  u2 = _mm_unpacklo_epi16(s1, s2);
  u3 = _mm_unpackhi_epi16(s1, s2);

  v0 = _mm_madd_epi16(u0, k__cospi_p28_p04);
  v1 = _mm_madd_epi16(u1, k__cospi_p28_p04);
  v2 = _mm_madd_epi16(u2, k__cospi_p12_p20);
  v3 = _mm_madd_epi16(u3, k__cospi_p12_p20);
  v4 = _mm_madd_epi16(u2, k__cospi_m20_p12);
  v5 = _mm_madd_epi16(u3, k__cospi_m20_p12);
  v6 = _mm_madd_epi16(u0, k__cospi_m04_p28);
  v7 = _mm_madd_epi16(u1, k__cospi_m04_p28);

  // shift and rounding
  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

  in[1] = _mm_packs_epi32(v0, v1);
  in[3] = _mm_packs_epi32(v4, v5);
  in[5] = _mm_packs_epi32(v2, v3);
  in[7] = _mm_packs_epi32(v6, v7);

  // transpose
  array_transpose_8x8_avx2(in, in);
}

void fadst8_avx2(__m128i *in) {
  // Constants
  const __m128i k__cospi_p02_p30 = pair_set_epi16(cospi_2_64, cospi_30_64);
  const __m128i k__cospi_p30_m02 = pair_set_epi16(cospi_30_64, -cospi_2_64);
  const __m128i k__cospi_p10_p22 = pair_set_epi16(cospi_10_64, cospi_22_64);
  const __m128i k__cospi_p22_m10 = pair_set_epi16(cospi_22_64, -cospi_10_64);
  const __m128i k__cospi_p18_p14 = pair_set_epi16(cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p14_m18 = pair_set_epi16(cospi_14_64, -cospi_18_64);
  const __m128i k__cospi_p26_p06 = pair_set_epi16(cospi_26_64, cospi_6_64);
  const __m128i k__cospi_p06_m26 = pair_set_epi16(cospi_6_64, -cospi_26_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__const_0 = _mm_set1_epi16(0);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  __m128i u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
  __m128i w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;
  __m128i s0, s1, s2, s3, s4, s5, s6, s7;
  __m128i in0, in1, in2, in3, in4, in5, in6, in7;

  // properly aligned for butterfly input
  in0  = in[7];
  in1  = in[0];
  in2  = in[5];
  in3  = in[2];
  in4  = in[3];
  in5  = in[4];
  in6  = in[1];
  in7  = in[6];

  // column transformation
  // stage 1
  // interleave and multiply/add into 32-bit integer
  s0 = _mm_unpacklo_epi16(in0, in1);
  s1 = _mm_unpackhi_epi16(in0, in1);
  s2 = _mm_unpacklo_epi16(in2, in3);
  s3 = _mm_unpackhi_epi16(in2, in3);
  s4 = _mm_unpacklo_epi16(in4, in5);
  s5 = _mm_unpackhi_epi16(in4, in5);
  s6 = _mm_unpacklo_epi16(in6, in7);
  s7 = _mm_unpackhi_epi16(in6, in7);

  u0 = _mm_madd_epi16(s0, k__cospi_p02_p30);
  u1 = _mm_madd_epi16(s1, k__cospi_p02_p30);
  u2 = _mm_madd_epi16(s0, k__cospi_p30_m02);
  u3 = _mm_madd_epi16(s1, k__cospi_p30_m02);
  u4 = _mm_madd_epi16(s2, k__cospi_p10_p22);
  u5 = _mm_madd_epi16(s3, k__cospi_p10_p22);
  u6 = _mm_madd_epi16(s2, k__cospi_p22_m10);
  u7 = _mm_madd_epi16(s3, k__cospi_p22_m10);
  u8 = _mm_madd_epi16(s4, k__cospi_p18_p14);
  u9 = _mm_madd_epi16(s5, k__cospi_p18_p14);
  u10 = _mm_madd_epi16(s4, k__cospi_p14_m18);
  u11 = _mm_madd_epi16(s5, k__cospi_p14_m18);
  u12 = _mm_madd_epi16(s6, k__cospi_p26_p06);
  u13 = _mm_madd_epi16(s7, k__cospi_p26_p06);
  u14 = _mm_madd_epi16(s6, k__cospi_p06_m26);
  u15 = _mm_madd_epi16(s7, k__cospi_p06_m26);

  // addition
  w0 = _mm_add_epi32(u0, u8);
  w1 = _mm_add_epi32(u1, u9);
  w2 = _mm_add_epi32(u2, u10);
  w3 = _mm_add_epi32(u3, u11);
  w4 = _mm_add_epi32(u4, u12);
  w5 = _mm_add_epi32(u5, u13);
  w6 = _mm_add_epi32(u6, u14);
  w7 = _mm_add_epi32(u7, u15);
  w8 = _mm_sub_epi32(u0, u8);
  w9 = _mm_sub_epi32(u1, u9);
  w10 = _mm_sub_epi32(u2, u10);
  w11 = _mm_sub_epi32(u3, u11);
  w12 = _mm_sub_epi32(u4, u12);
  w13 = _mm_sub_epi32(u5, u13);
  w14 = _mm_sub_epi32(u6, u14);
  w15 = _mm_sub_epi32(u7, u15);

  // shift and rounding
  v0 = _mm_add_epi32(w0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(w1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(w2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(w3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(w4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(w5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(w6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(w7, k__DCT_CONST_ROUNDING);
  v8 = _mm_add_epi32(w8, k__DCT_CONST_ROUNDING);
  v9 = _mm_add_epi32(w9, k__DCT_CONST_ROUNDING);
  v10 = _mm_add_epi32(w10, k__DCT_CONST_ROUNDING);
  v11 = _mm_add_epi32(w11, k__DCT_CONST_ROUNDING);
  v12 = _mm_add_epi32(w12, k__DCT_CONST_ROUNDING);
  v13 = _mm_add_epi32(w13, k__DCT_CONST_ROUNDING);
  v14 = _mm_add_epi32(w14, k__DCT_CONST_ROUNDING);
  v15 = _mm_add_epi32(w15, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
  u8 = _mm_srai_epi32(v8, DCT_CONST_BITS);
  u9 = _mm_srai_epi32(v9, DCT_CONST_BITS);
  u10 = _mm_srai_epi32(v10, DCT_CONST_BITS);
  u11 = _mm_srai_epi32(v11, DCT_CONST_BITS);
  u12 = _mm_srai_epi32(v12, DCT_CONST_BITS);
  u13 = _mm_srai_epi32(v13, DCT_CONST_BITS);
  u14 = _mm_srai_epi32(v14, DCT_CONST_BITS);
  u15 = _mm_srai_epi32(v15, DCT_CONST_BITS);

  // back to 16-bit and pack 8 integers into __m128i
  in[0] = _mm_packs_epi32(u0, u1);
  in[1] = _mm_packs_epi32(u2, u3);
  in[2] = _mm_packs_epi32(u4, u5);
  in[3] = _mm_packs_epi32(u6, u7);
  in[4] = _mm_packs_epi32(u8, u9);
  in[5] = _mm_packs_epi32(u10, u11);
  in[6] = _mm_packs_epi32(u12, u13);
  in[7] = _mm_packs_epi32(u14, u15);

  // stage 2
  s0 = _mm_add_epi16(in[0], in[2]);
  s1 = _mm_add_epi16(in[1], in[3]);
  s2 = _mm_sub_epi16(in[0], in[2]);
  s3 = _mm_sub_epi16(in[1], in[3]);
  u0 = _mm_unpacklo_epi16(in[4], in[5]);
  u1 = _mm_unpackhi_epi16(in[4], in[5]);
  u2 = _mm_unpacklo_epi16(in[6], in[7]);
  u3 = _mm_unpackhi_epi16(in[6], in[7]);

  v0 = _mm_madd_epi16(u0, k__cospi_p08_p24);
  v1 = _mm_madd_epi16(u1, k__cospi_p08_p24);
  v2 = _mm_madd_epi16(u0, k__cospi_p24_m08);
  v3 = _mm_madd_epi16(u1, k__cospi_p24_m08);
  v4 = _mm_madd_epi16(u2, k__cospi_m24_p08);
  v5 = _mm_madd_epi16(u3, k__cospi_m24_p08);
  v6 = _mm_madd_epi16(u2, k__cospi_p08_p24);
  v7 = _mm_madd_epi16(u3, k__cospi_p08_p24);

  w0 = _mm_add_epi32(v0, v4);
  w1 = _mm_add_epi32(v1, v5);
  w2 = _mm_add_epi32(v2, v6);
  w3 = _mm_add_epi32(v3, v7);
  w4 = _mm_sub_epi32(v0, v4);
  w5 = _mm_sub_epi32(v1, v5);
  w6 = _mm_sub_epi32(v2, v6);
  w7 = _mm_sub_epi32(v3, v7);

  v0 = _mm_add_epi32(w0, k__DCT_CONST_ROUNDING);
  v1 = _mm_add_epi32(w1, k__DCT_CONST_ROUNDING);
  v2 = _mm_add_epi32(w2, k__DCT_CONST_ROUNDING);
  v3 = _mm_add_epi32(w3, k__DCT_CONST_ROUNDING);
  v4 = _mm_add_epi32(w4, k__DCT_CONST_ROUNDING);
  v5 = _mm_add_epi32(w5, k__DCT_CONST_ROUNDING);
  v6 = _mm_add_epi32(w6, k__DCT_CONST_ROUNDING);
  v7 = _mm_add_epi32(w7, k__DCT_CONST_ROUNDING);

  u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
  u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
  u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
  u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
  u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
  u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
  u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
  u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

  // back to 16-bit intergers
  s4 = _mm_packs_epi32(u0, u1);
  s5 = _mm_packs_epi32(u2, u3);
  s6 = _mm_packs_epi32(u4, u5);
  s7 = _mm_packs_epi32(u6, u7);

  // stage 3
  u0 = _mm_unpacklo_epi16(s2, s3);
  u1 = _mm_unpackhi_epi16(s2, s3);
  u2 = _mm_unpacklo_epi16(s6, s7);
  u3 = _mm_unpackhi_epi16(s6, s7);

  v0 = _mm_madd_epi16(u0, k__cospi_p16_p16);
  v1 = _mm_madd_epi16(u1, k__cospi_p16_p16);
  v2 = _mm_madd_epi16(u0, k__cospi_p16_m16);
  v3 = _mm_madd_epi16(u1, k__cospi_p16_m16);
  v4 = _mm_madd_epi16(u2, k__cospi_p16_p16);
  v5 = _mm_madd_epi16(u3, k__cospi_p16_p16);
  v6 = _mm_madd_epi16(u2, k__cospi_p16_m16);
  v7 = _mm_madd_epi16(u3, k__cospi_p16_m16);

  u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
  u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
  u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
  u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
  u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
  u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
  u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
  u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

  v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
  v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
  v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
  v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
  v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
  v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
  v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
  v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

  s2 = _mm_packs_epi32(v0, v1);
  s3 = _mm_packs_epi32(v2, v3);
  s6 = _mm_packs_epi32(v4, v5);
  s7 = _mm_packs_epi32(v6, v7);

  // FIXME(jingning): do subtract using bit inversion?
  in[0] = s0;
  in[1] = _mm_sub_epi16(k__const_0, s4);
  in[2] = s6;
  in[3] = _mm_sub_epi16(k__const_0, s2);
  in[4] = s3;
  in[5] = _mm_sub_epi16(k__const_0, s7);
  in[6] = s5;
  in[7] = _mm_sub_epi16(k__const_0, s1);

  // transpose
  array_transpose_8x8_avx2(in, in);
}

void vp9_fht8x8_avx2(const int16_t *input, int16_t *output,
                     int stride, int tx_type) {
  __m128i in[8];

  switch (tx_type) {
    case DCT_DCT:
      vp9_fdct8x8_avx2(input, output, stride);
      break;
    case ADST_DCT:
      load_buffer_8x8_avx2(input, in, stride);
      fadst8_avx2(in);
      fdct8_avx2(in);
      right_shift_8x8_avx2(in, 1);
      write_buffer_8x8_avx2(output, in, 8);
      break;
    case DCT_ADST:
      load_buffer_8x8_avx2(input, in, stride);
      fdct8_avx2(in);
      fadst8_avx2(in);
      right_shift_8x8_avx2(in, 1);
      write_buffer_8x8_avx2(output, in, 8);
      break;
    case ADST_ADST:
      load_buffer_8x8_avx2(input, in, stride);
      fadst8_avx2(in);
      fadst8_avx2(in);
      right_shift_8x8_avx2(in, 1);
      write_buffer_8x8_avx2(output, in, 8);
      break;
    default:
      assert(0);
      break;
  }
}

void vp9_fdct16x16_avx2(const int16_t *input, int16_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows. To achieve that,
  // as the first pass results are transposed, we tranpose the columns (that
  // is the transposed rows) and transpose the results (so that it goes back
  // in normal/row positions).
  int pass;
  // We need an intermediate buffer between passes.
  DECLARE_ALIGNED_ARRAY(16, int16_t, intermediate, 256);
  const int16_t *in = input;
  int16_t *out = intermediate;
  // Constants
  //    When we use them, in one case, they are all the same. In all others
  //    it's a pair of them that we need to repeat four times. This is done
  //    by constructing the 32 bit constant corresponding to that pair.
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
  const __m128i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
  const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
  const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
  const __m128i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
  const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
  const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i kOne = _mm_set1_epi16(1);
  // Do the two transform/transpose passes
  for (pass = 0; pass < 2; ++pass) {
    // We process eight columns (transposed rows in second pass) at a time.
    int column_start;
    for (column_start = 0; column_start < 16; column_start += 8) {
      __m128i in00, in01, in02, in03, in04, in05, in06, in07;
      __m128i in08, in09, in10, in11, in12, in13, in14, in15;
      __m128i input0, input1, input2, input3, input4, input5, input6, input7;
      __m128i step1_0, step1_1, step1_2, step1_3;
      __m128i step1_4, step1_5, step1_6, step1_7;
      __m128i step2_1, step2_2, step2_3, step2_4, step2_5, step2_6;
      __m128i step3_0, step3_1, step3_2, step3_3;
      __m128i step3_4, step3_5, step3_6, step3_7;
      __m128i res00, res01, res02, res03, res04, res05, res06, res07;
      __m128i res08, res09, res10, res11, res12, res13, res14, res15;
      // Load and pre-condition input.
      if (0 == pass) {
        in00  = _mm_load_si128((const __m128i *)(in +  0 * stride));
        in01  = _mm_load_si128((const __m128i *)(in +  1 * stride));
        in02  = _mm_load_si128((const __m128i *)(in +  2 * stride));
        in03  = _mm_load_si128((const __m128i *)(in +  3 * stride));
        in04  = _mm_load_si128((const __m128i *)(in +  4 * stride));
        in05  = _mm_load_si128((const __m128i *)(in +  5 * stride));
        in06  = _mm_load_si128((const __m128i *)(in +  6 * stride));
        in07  = _mm_load_si128((const __m128i *)(in +  7 * stride));
        in08  = _mm_load_si128((const __m128i *)(in +  8 * stride));
        in09  = _mm_load_si128((const __m128i *)(in +  9 * stride));
        in10  = _mm_load_si128((const __m128i *)(in + 10 * stride));
        in11  = _mm_load_si128((const __m128i *)(in + 11 * stride));
        in12  = _mm_load_si128((const __m128i *)(in + 12 * stride));
        in13  = _mm_load_si128((const __m128i *)(in + 13 * stride));
        in14  = _mm_load_si128((const __m128i *)(in + 14 * stride));
        in15  = _mm_load_si128((const __m128i *)(in + 15 * stride));
        // x = x << 2
        in00 = _mm_slli_epi16(in00, 2);
        in01 = _mm_slli_epi16(in01, 2);
        in02 = _mm_slli_epi16(in02, 2);
        in03 = _mm_slli_epi16(in03, 2);
        in04 = _mm_slli_epi16(in04, 2);
        in05 = _mm_slli_epi16(in05, 2);
        in06 = _mm_slli_epi16(in06, 2);
        in07 = _mm_slli_epi16(in07, 2);
        in08 = _mm_slli_epi16(in08, 2);
        in09 = _mm_slli_epi16(in09, 2);
        in10 = _mm_slli_epi16(in10, 2);
        in11 = _mm_slli_epi16(in11, 2);
        in12 = _mm_slli_epi16(in12, 2);
        in13 = _mm_slli_epi16(in13, 2);
        in14 = _mm_slli_epi16(in14, 2);
        in15 = _mm_slli_epi16(in15, 2);
      } else {
        in00  = _mm_load_si128((const __m128i *)(in +  0 * 16));
        in01  = _mm_load_si128((const __m128i *)(in +  1 * 16));
        in02  = _mm_load_si128((const __m128i *)(in +  2 * 16));
        in03  = _mm_load_si128((const __m128i *)(in +  3 * 16));
        in04  = _mm_load_si128((const __m128i *)(in +  4 * 16));
        in05  = _mm_load_si128((const __m128i *)(in +  5 * 16));
        in06  = _mm_load_si128((const __m128i *)(in +  6 * 16));
        in07  = _mm_load_si128((const __m128i *)(in +  7 * 16));
        in08  = _mm_load_si128((const __m128i *)(in +  8 * 16));
        in09  = _mm_load_si128((const __m128i *)(in +  9 * 16));
        in10  = _mm_load_si128((const __m128i *)(in + 10 * 16));
        in11  = _mm_load_si128((const __m128i *)(in + 11 * 16));
        in12  = _mm_load_si128((const __m128i *)(in + 12 * 16));
        in13  = _mm_load_si128((const __m128i *)(in + 13 * 16));
        in14  = _mm_load_si128((const __m128i *)(in + 14 * 16));
        in15  = _mm_load_si128((const __m128i *)(in + 15 * 16));
        // x = (x + 1) >> 2
        in00 = _mm_add_epi16(in00, kOne);
        in01 = _mm_add_epi16(in01, kOne);
        in02 = _mm_add_epi16(in02, kOne);
        in03 = _mm_add_epi16(in03, kOne);
        in04 = _mm_add_epi16(in04, kOne);
        in05 = _mm_add_epi16(in05, kOne);
        in06 = _mm_add_epi16(in06, kOne);
        in07 = _mm_add_epi16(in07, kOne);
        in08 = _mm_add_epi16(in08, kOne);
        in09 = _mm_add_epi16(in09, kOne);
        in10 = _mm_add_epi16(in10, kOne);
        in11 = _mm_add_epi16(in11, kOne);
        in12 = _mm_add_epi16(in12, kOne);
        in13 = _mm_add_epi16(in13, kOne);
        in14 = _mm_add_epi16(in14, kOne);
        in15 = _mm_add_epi16(in15, kOne);
        in00 = _mm_srai_epi16(in00, 2);
        in01 = _mm_srai_epi16(in01, 2);
        in02 = _mm_srai_epi16(in02, 2);
        in03 = _mm_srai_epi16(in03, 2);
        in04 = _mm_srai_epi16(in04, 2);
        in05 = _mm_srai_epi16(in05, 2);
        in06 = _mm_srai_epi16(in06, 2);
        in07 = _mm_srai_epi16(in07, 2);
        in08 = _mm_srai_epi16(in08, 2);
        in09 = _mm_srai_epi16(in09, 2);
        in10 = _mm_srai_epi16(in10, 2);
        in11 = _mm_srai_epi16(in11, 2);
        in12 = _mm_srai_epi16(in12, 2);
        in13 = _mm_srai_epi16(in13, 2);
        in14 = _mm_srai_epi16(in14, 2);
        in15 = _mm_srai_epi16(in15, 2);
      }
      in += 8;
      // Calculate input for the first 8 results.
      {
        input0 = _mm_add_epi16(in00, in15);
        input1 = _mm_add_epi16(in01, in14);
        input2 = _mm_add_epi16(in02, in13);
        input3 = _mm_add_epi16(in03, in12);
        input4 = _mm_add_epi16(in04, in11);
        input5 = _mm_add_epi16(in05, in10);
        input6 = _mm_add_epi16(in06, in09);
        input7 = _mm_add_epi16(in07, in08);
      }
      // Calculate input for the next 8 results.
      {
        step1_0 = _mm_sub_epi16(in07, in08);
        step1_1 = _mm_sub_epi16(in06, in09);
        step1_2 = _mm_sub_epi16(in05, in10);
        step1_3 = _mm_sub_epi16(in04, in11);
        step1_4 = _mm_sub_epi16(in03, in12);
        step1_5 = _mm_sub_epi16(in02, in13);
        step1_6 = _mm_sub_epi16(in01, in14);
        step1_7 = _mm_sub_epi16(in00, in15);
      }
      // Work on the first eight values; fdct8(input, even_results);
      {
        // Add/substract
        const __m128i q0 = _mm_add_epi16(input0, input7);
        const __m128i q1 = _mm_add_epi16(input1, input6);
        const __m128i q2 = _mm_add_epi16(input2, input5);
        const __m128i q3 = _mm_add_epi16(input3, input4);
        const __m128i q4 = _mm_sub_epi16(input3, input4);
        const __m128i q5 = _mm_sub_epi16(input2, input5);
        const __m128i q6 = _mm_sub_epi16(input1, input6);
        const __m128i q7 = _mm_sub_epi16(input0, input7);
        // Work on first four results
        {
          // Add/substract
          const __m128i r0 = _mm_add_epi16(q0, q3);
          const __m128i r1 = _mm_add_epi16(q1, q2);
          const __m128i r2 = _mm_sub_epi16(q1, q2);
          const __m128i r3 = _mm_sub_epi16(q0, q3);
          // Interleave to do the multiply by constants which gets us
          // into 32 bits.
          const __m128i t0 = _mm_unpacklo_epi16(r0, r1);
          const __m128i t1 = _mm_unpackhi_epi16(r0, r1);
          const __m128i t2 = _mm_unpacklo_epi16(r2, r3);
          const __m128i t3 = _mm_unpackhi_epi16(r2, r3);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_p16);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p16_p16);
          const __m128i u2 = _mm_madd_epi16(t0, k__cospi_p16_m16);
          const __m128i u3 = _mm_madd_epi16(t1, k__cospi_p16_m16);
          const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p24_p08);
          const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p24_p08);
          const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m08_p24);
          const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m08_p24);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
          const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
          const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
          const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
          const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
          const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
          const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
          // Combine
          res00 = _mm_packs_epi32(w0, w1);
          res08 = _mm_packs_epi32(w2, w3);
          res04 = _mm_packs_epi32(w4, w5);
          res12 = _mm_packs_epi32(w6, w7);
        }
        // Work on next four results
        {
          // Interleave to do the multiply by constants which gets us
          // into 32 bits.
          const __m128i d0 = _mm_unpacklo_epi16(q6, q5);
          const __m128i d1 = _mm_unpackhi_epi16(q6, q5);
          const __m128i e0 = _mm_madd_epi16(d0, k__cospi_p16_m16);
          const __m128i e1 = _mm_madd_epi16(d1, k__cospi_p16_m16);
          const __m128i e2 = _mm_madd_epi16(d0, k__cospi_p16_p16);
          const __m128i e3 = _mm_madd_epi16(d1, k__cospi_p16_p16);
          // dct_const_round_shift
          const __m128i f0 = _mm_add_epi32(e0, k__DCT_CONST_ROUNDING);
          const __m128i f1 = _mm_add_epi32(e1, k__DCT_CONST_ROUNDING);
          const __m128i f2 = _mm_add_epi32(e2, k__DCT_CONST_ROUNDING);
          const __m128i f3 = _mm_add_epi32(e3, k__DCT_CONST_ROUNDING);
          const __m128i s0 = _mm_srai_epi32(f0, DCT_CONST_BITS);
          const __m128i s1 = _mm_srai_epi32(f1, DCT_CONST_BITS);
          const __m128i s2 = _mm_srai_epi32(f2, DCT_CONST_BITS);
          const __m128i s3 = _mm_srai_epi32(f3, DCT_CONST_BITS);
          // Combine
          const __m128i r0 = _mm_packs_epi32(s0, s1);
          const __m128i r1 = _mm_packs_epi32(s2, s3);
          // Add/substract
          const __m128i x0 = _mm_add_epi16(q4, r0);
          const __m128i x1 = _mm_sub_epi16(q4, r0);
          const __m128i x2 = _mm_sub_epi16(q7, r1);
          const __m128i x3 = _mm_add_epi16(q7, r1);
          // Interleave to do the multiply by constants which gets us
          // into 32 bits.
          const __m128i t0 = _mm_unpacklo_epi16(x0, x3);
          const __m128i t1 = _mm_unpackhi_epi16(x0, x3);
          const __m128i t2 = _mm_unpacklo_epi16(x1, x2);
          const __m128i t3 = _mm_unpackhi_epi16(x1, x2);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p28_p04);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p28_p04);
          const __m128i u2 = _mm_madd_epi16(t0, k__cospi_m04_p28);
          const __m128i u3 = _mm_madd_epi16(t1, k__cospi_m04_p28);
          const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p12_p20);
          const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p12_p20);
          const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m20_p12);
          const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m20_p12);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
          const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
          const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
          const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
          const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
          const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
          const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
          // Combine
          res02 = _mm_packs_epi32(w0, w1);
          res14 = _mm_packs_epi32(w2, w3);
          res10 = _mm_packs_epi32(w4, w5);
          res06 = _mm_packs_epi32(w6, w7);
        }
      }
      // Work on the next eight values; step1 -> odd_results
      {
        // step 2
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_5, step1_2);
          const __m128i t1 = _mm_unpackhi_epi16(step1_5, step1_2);
          const __m128i t2 = _mm_unpacklo_epi16(step1_4, step1_3);
          const __m128i t3 = _mm_unpackhi_epi16(step1_4, step1_3);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_m16);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p16_m16);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_p16_m16);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_p16_m16);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          step2_2 = _mm_packs_epi32(w0, w1);
          step2_3 = _mm_packs_epi32(w2, w3);
        }
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_5, step1_2);
          const __m128i t1 = _mm_unpackhi_epi16(step1_5, step1_2);
          const __m128i t2 = _mm_unpacklo_epi16(step1_4, step1_3);
          const __m128i t3 = _mm_unpackhi_epi16(step1_4, step1_3);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_p16);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p16_p16);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_p16_p16);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_p16_p16);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          step2_5 = _mm_packs_epi32(w0, w1);
          step2_4 = _mm_packs_epi32(w2, w3);
        }
        // step 3
        {
          step3_0 = _mm_add_epi16(step1_0, step2_3);
          step3_1 = _mm_add_epi16(step1_1, step2_2);
          step3_2 = _mm_sub_epi16(step1_1, step2_2);
          step3_3 = _mm_sub_epi16(step1_0, step2_3);
          step3_4 = _mm_sub_epi16(step1_7, step2_4);
          step3_5 = _mm_sub_epi16(step1_6, step2_5);
          step3_6 = _mm_add_epi16(step1_6, step2_5);
          step3_7 = _mm_add_epi16(step1_7, step2_4);
        }
        // step 4
        {
          const __m128i t0 = _mm_unpacklo_epi16(step3_1, step3_6);
          const __m128i t1 = _mm_unpackhi_epi16(step3_1, step3_6);
          const __m128i t2 = _mm_unpacklo_epi16(step3_2, step3_5);
          const __m128i t3 = _mm_unpackhi_epi16(step3_2, step3_5);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_m08_p24);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_m08_p24);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_m24_m08);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_m24_m08);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          step2_1 = _mm_packs_epi32(w0, w1);
          step2_2 = _mm_packs_epi32(w2, w3);
        }
        {
          const __m128i t0 = _mm_unpacklo_epi16(step3_1, step3_6);
          const __m128i t1 = _mm_unpackhi_epi16(step3_1, step3_6);
          const __m128i t2 = _mm_unpacklo_epi16(step3_2, step3_5);
          const __m128i t3 = _mm_unpackhi_epi16(step3_2, step3_5);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p24_p08);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p24_p08);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_m08_p24);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_m08_p24);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          step2_6 = _mm_packs_epi32(w0, w1);
          step2_5 = _mm_packs_epi32(w2, w3);
        }
        // step 5
        {
          step1_0 = _mm_add_epi16(step3_0, step2_1);
          step1_1 = _mm_sub_epi16(step3_0, step2_1);
          step1_2 = _mm_sub_epi16(step3_3, step2_2);
          step1_3 = _mm_add_epi16(step3_3, step2_2);
          step1_4 = _mm_add_epi16(step3_4, step2_5);
          step1_5 = _mm_sub_epi16(step3_4, step2_5);
          step1_6 = _mm_sub_epi16(step3_7, step2_6);
          step1_7 = _mm_add_epi16(step3_7, step2_6);
        }
        // step 6
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_0, step1_7);
          const __m128i t1 = _mm_unpackhi_epi16(step1_0, step1_7);
          const __m128i t2 = _mm_unpacklo_epi16(step1_1, step1_6);
          const __m128i t3 = _mm_unpackhi_epi16(step1_1, step1_6);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p30_p02);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p30_p02);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_p14_p18);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_p14_p18);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          res01 = _mm_packs_epi32(w0, w1);
          res09 = _mm_packs_epi32(w2, w3);
        }
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_2, step1_5);
          const __m128i t1 = _mm_unpackhi_epi16(step1_2, step1_5);
          const __m128i t2 = _mm_unpacklo_epi16(step1_3, step1_4);
          const __m128i t3 = _mm_unpackhi_epi16(step1_3, step1_4);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p22_p10);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p22_p10);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_p06_p26);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_p06_p26);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          res05 = _mm_packs_epi32(w0, w1);
          res13 = _mm_packs_epi32(w2, w3);
        }
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_2, step1_5);
          const __m128i t1 = _mm_unpackhi_epi16(step1_2, step1_5);
          const __m128i t2 = _mm_unpacklo_epi16(step1_3, step1_4);
          const __m128i t3 = _mm_unpackhi_epi16(step1_3, step1_4);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_m10_p22);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_m10_p22);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_m26_p06);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_m26_p06);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          res11 = _mm_packs_epi32(w0, w1);
          res03 = _mm_packs_epi32(w2, w3);
        }
        {
          const __m128i t0 = _mm_unpacklo_epi16(step1_0, step1_7);
          const __m128i t1 = _mm_unpackhi_epi16(step1_0, step1_7);
          const __m128i t2 = _mm_unpacklo_epi16(step1_1, step1_6);
          const __m128i t3 = _mm_unpackhi_epi16(step1_1, step1_6);
          const __m128i u0 = _mm_madd_epi16(t0, k__cospi_m02_p30);
          const __m128i u1 = _mm_madd_epi16(t1, k__cospi_m02_p30);
          const __m128i u2 = _mm_madd_epi16(t2, k__cospi_m18_p14);
          const __m128i u3 = _mm_madd_epi16(t3, k__cospi_m18_p14);
          // dct_const_round_shift
          const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
          const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
          const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
          const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
          const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
          const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
          const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
          const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
          // Combine
          res15 = _mm_packs_epi32(w0, w1);
          res07 = _mm_packs_epi32(w2, w3);
        }
      }
      // Transpose the results, do it as two 8x8 transposes.
      {
        // 00 01 02 03 04 05 06 07
        // 10 11 12 13 14 15 16 17
        // 20 21 22 23 24 25 26 27
        // 30 31 32 33 34 35 36 37
        // 40 41 42 43 44 45 46 47
        // 50 51 52 53 54 55 56 57
        // 60 61 62 63 64 65 66 67
        // 70 71 72 73 74 75 76 77
        const __m128i tr0_0 = _mm_unpacklo_epi16(res00, res01);
        const __m128i tr0_1 = _mm_unpacklo_epi16(res02, res03);
        const __m128i tr0_2 = _mm_unpackhi_epi16(res00, res01);
        const __m128i tr0_3 = _mm_unpackhi_epi16(res02, res03);
        const __m128i tr0_4 = _mm_unpacklo_epi16(res04, res05);
        const __m128i tr0_5 = _mm_unpacklo_epi16(res06, res07);
        const __m128i tr0_6 = _mm_unpackhi_epi16(res04, res05);
        const __m128i tr0_7 = _mm_unpackhi_epi16(res06, res07);
        // 00 10 01 11 02 12 03 13
        // 20 30 21 31 22 32 23 33
        // 04 14 05 15 06 16 07 17
        // 24 34 25 35 26 36 27 37
        // 40 50 41 51 42 52 43 53
        // 60 70 61 71 62 72 63 73
        // 54 54 55 55 56 56 57 57
        // 64 74 65 75 66 76 67 77
        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
        // 00 10 20 30 01 11 21 31
        // 40 50 60 70 41 51 61 71
        // 02 12 22 32 03 13 23 33
        // 42 52 62 72 43 53 63 73
        // 04 14 24 34 05 15 21 36
        // 44 54 64 74 45 55 61 76
        // 06 16 26 36 07 17 27 37
        // 46 56 66 76 47 57 67 77
        const __m128i tr2_0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
        const __m128i tr2_1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
        const __m128i tr2_2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
        const __m128i tr2_3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
        const __m128i tr2_4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
        const __m128i tr2_5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
        const __m128i tr2_6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
        const __m128i tr2_7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
        // 00 10 20 30 40 50 60 70
        // 01 11 21 31 41 51 61 71
        // 02 12 22 32 42 52 62 72
        // 03 13 23 33 43 53 63 73
        // 04 14 24 34 44 54 64 74
        // 05 15 25 35 45 55 65 75
        // 06 16 26 36 46 56 66 76
        // 07 17 27 37 47 57 67 77
        _mm_storeu_si128((__m128i *)(out + 0 * 16), tr2_0);
        _mm_storeu_si128((__m128i *)(out + 1 * 16), tr2_1);
        _mm_storeu_si128((__m128i *)(out + 2 * 16), tr2_2);
        _mm_storeu_si128((__m128i *)(out + 3 * 16), tr2_3);
        _mm_storeu_si128((__m128i *)(out + 4 * 16), tr2_4);
        _mm_storeu_si128((__m128i *)(out + 5 * 16), tr2_5);
        _mm_storeu_si128((__m128i *)(out + 6 * 16), tr2_6);
        _mm_storeu_si128((__m128i *)(out + 7 * 16), tr2_7);
      }
      {
        // 00 01 02 03 04 05 06 07
        // 10 11 12 13 14 15 16 17
        // 20 21 22 23 24 25 26 27
        // 30 31 32 33 34 35 36 37
        // 40 41 42 43 44 45 46 47
        // 50 51 52 53 54 55 56 57
        // 60 61 62 63 64 65 66 67
        // 70 71 72 73 74 75 76 77
        const __m128i tr0_0 = _mm_unpacklo_epi16(res08, res09);
        const __m128i tr0_1 = _mm_unpacklo_epi16(res10, res11);
        const __m128i tr0_2 = _mm_unpackhi_epi16(res08, res09);
        const __m128i tr0_3 = _mm_unpackhi_epi16(res10, res11);
        const __m128i tr0_4 = _mm_unpacklo_epi16(res12, res13);
        const __m128i tr0_5 = _mm_unpacklo_epi16(res14, res15);
        const __m128i tr0_6 = _mm_unpackhi_epi16(res12, res13);
        const __m128i tr0_7 = _mm_unpackhi_epi16(res14, res15);
        // 00 10 01 11 02 12 03 13
        // 20 30 21 31 22 32 23 33
        // 04 14 05 15 06 16 07 17
        // 24 34 25 35 26 36 27 37
        // 40 50 41 51 42 52 43 53
        // 60 70 61 71 62 72 63 73
        // 54 54 55 55 56 56 57 57
        // 64 74 65 75 66 76 67 77
        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
        // 00 10 20 30 01 11 21 31
        // 40 50 60 70 41 51 61 71
        // 02 12 22 32 03 13 23 33
        // 42 52 62 72 43 53 63 73
        // 04 14 24 34 05 15 21 36
        // 44 54 64 74 45 55 61 76
        // 06 16 26 36 07 17 27 37
        // 46 56 66 76 47 57 67 77
        const __m128i tr2_0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
        const __m128i tr2_1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
        const __m128i tr2_2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
        const __m128i tr2_3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
        const __m128i tr2_4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
        const __m128i tr2_5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
        const __m128i tr2_6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
        const __m128i tr2_7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
        // 00 10 20 30 40 50 60 70
        // 01 11 21 31 41 51 61 71
        // 02 12 22 32 42 52 62 72
        // 03 13 23 33 43 53 63 73
        // 04 14 24 34 44 54 64 74
        // 05 15 25 35 45 55 65 75
        // 06 16 26 36 46 56 66 76
        // 07 17 27 37 47 57 67 77
        // Store results
        _mm_store_si128((__m128i *)(out + 8 + 0 * 16), tr2_0);
        _mm_store_si128((__m128i *)(out + 8 + 1 * 16), tr2_1);
        _mm_store_si128((__m128i *)(out + 8 + 2 * 16), tr2_2);
        _mm_store_si128((__m128i *)(out + 8 + 3 * 16), tr2_3);
        _mm_store_si128((__m128i *)(out + 8 + 4 * 16), tr2_4);
        _mm_store_si128((__m128i *)(out + 8 + 5 * 16), tr2_5);
        _mm_store_si128((__m128i *)(out + 8 + 6 * 16), tr2_6);
        _mm_store_si128((__m128i *)(out + 8 + 7 * 16), tr2_7);
      }
      out += 8*16;
    }
    // Setup in/out for next pass.
    in = intermediate;
    out = output;
  }
}

static INLINE void load_buffer_16x16_avx2(const int16_t* input, __m128i *in0,
                                     __m128i *in1, int stride) {
  // load first 8 columns
  load_buffer_8x8_avx2(input, in0, stride);
  load_buffer_8x8_avx2(input + 8 * stride, in0 + 8, stride);

  input += 8;
  // load second 8 columns
  load_buffer_8x8_avx2(input, in1, stride);
  load_buffer_8x8_avx2(input + 8 * stride, in1 + 8, stride);
}

static INLINE void write_buffer_16x16_avx2(int16_t *output, __m128i *in0,
                                      __m128i *in1, int stride) {
  // write first 8 columns
  write_buffer_8x8_avx2(output, in0, stride);
  write_buffer_8x8_avx2(output + 8 * stride, in0 + 8, stride);
  // write second 8 columns
  output += 8;
  write_buffer_8x8_avx2(output, in1, stride);
  write_buffer_8x8_avx2(output + 8 * stride, in1 + 8, stride);
}

static INLINE void array_transpose_16x16_avx2(__m128i *res0, __m128i *res1) {
  __m128i tbuf[8];
  array_transpose_8x8_avx2(res0, res0);
  array_transpose_8x8_avx2(res1, tbuf);
  array_transpose_8x8_avx2(res0 + 8, res1);
  array_transpose_8x8_avx2(res1 + 8, res1 + 8);

  res0[8] = tbuf[0];
  res0[9] = tbuf[1];
  res0[10] = tbuf[2];
  res0[11] = tbuf[3];
  res0[12] = tbuf[4];
  res0[13] = tbuf[5];
  res0[14] = tbuf[6];
  res0[15] = tbuf[7];
}

static INLINE void right_shift_16x16_avx2(__m128i *res0, __m128i *res1) {
  // perform rounding operations
  right_shift_8x8_avx2(res0, 2);
  right_shift_8x8_avx2(res0 + 8, 2);
  right_shift_8x8_avx2(res1, 2);
  right_shift_8x8_avx2(res1 + 8, 2);
}

void fdct16_8col_avx2(__m128i *in) {
  // perform 16x16 1-D DCT for 8 columns
  __m128i i[8], s[8], p[8], t[8], u[16], v[16];
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
  const __m128i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
  const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
  const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
  const __m128i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
  const __m128i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
  const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
  const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

  // stage 1
  i[0] = _mm_add_epi16(in[0], in[15]);
  i[1] = _mm_add_epi16(in[1], in[14]);
  i[2] = _mm_add_epi16(in[2], in[13]);
  i[3] = _mm_add_epi16(in[3], in[12]);
  i[4] = _mm_add_epi16(in[4], in[11]);
  i[5] = _mm_add_epi16(in[5], in[10]);
  i[6] = _mm_add_epi16(in[6], in[9]);
  i[7] = _mm_add_epi16(in[7], in[8]);

  s[0] = _mm_sub_epi16(in[7], in[8]);
  s[1] = _mm_sub_epi16(in[6], in[9]);
  s[2] = _mm_sub_epi16(in[5], in[10]);
  s[3] = _mm_sub_epi16(in[4], in[11]);
  s[4] = _mm_sub_epi16(in[3], in[12]);
  s[5] = _mm_sub_epi16(in[2], in[13]);
  s[6] = _mm_sub_epi16(in[1], in[14]);
  s[7] = _mm_sub_epi16(in[0], in[15]);

  p[0] = _mm_add_epi16(i[0], i[7]);
  p[1] = _mm_add_epi16(i[1], i[6]);
  p[2] = _mm_add_epi16(i[2], i[5]);
  p[3] = _mm_add_epi16(i[3], i[4]);
  p[4] = _mm_sub_epi16(i[3], i[4]);
  p[5] = _mm_sub_epi16(i[2], i[5]);
  p[6] = _mm_sub_epi16(i[1], i[6]);
  p[7] = _mm_sub_epi16(i[0], i[7]);

  u[0] = _mm_add_epi16(p[0], p[3]);
  u[1] = _mm_add_epi16(p[1], p[2]);
  u[2] = _mm_sub_epi16(p[1], p[2]);
  u[3] = _mm_sub_epi16(p[0], p[3]);

  v[0] = _mm_unpacklo_epi16(u[0], u[1]);
  v[1] = _mm_unpackhi_epi16(u[0], u[1]);
  v[2] = _mm_unpacklo_epi16(u[2], u[3]);
  v[3] = _mm_unpackhi_epi16(u[2], u[3]);

  u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);
  u[1] = _mm_madd_epi16(v[1], k__cospi_p16_p16);
  u[2] = _mm_madd_epi16(v[0], k__cospi_p16_m16);
  u[3] = _mm_madd_epi16(v[1], k__cospi_p16_m16);
  u[4] = _mm_madd_epi16(v[2], k__cospi_p24_p08);
  u[5] = _mm_madd_epi16(v[3], k__cospi_p24_p08);
  u[6] = _mm_madd_epi16(v[2], k__cospi_m08_p24);
  u[7] = _mm_madd_epi16(v[3], k__cospi_m08_p24);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);

  in[0] = _mm_packs_epi32(u[0], u[1]);
  in[4] = _mm_packs_epi32(u[4], u[5]);
  in[8] = _mm_packs_epi32(u[2], u[3]);
  in[12] = _mm_packs_epi32(u[6], u[7]);

  u[0] = _mm_unpacklo_epi16(p[5], p[6]);
  u[1] = _mm_unpackhi_epi16(p[5], p[6]);
  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);

  u[0] = _mm_packs_epi32(v[0], v[1]);
  u[1] = _mm_packs_epi32(v[2], v[3]);

  t[0] = _mm_add_epi16(p[4], u[0]);
  t[1] = _mm_sub_epi16(p[4], u[0]);
  t[2] = _mm_sub_epi16(p[7], u[1]);
  t[3] = _mm_add_epi16(p[7], u[1]);

  u[0] = _mm_unpacklo_epi16(t[0], t[3]);
  u[1] = _mm_unpackhi_epi16(t[0], t[3]);
  u[2] = _mm_unpacklo_epi16(t[1], t[2]);
  u[3] = _mm_unpackhi_epi16(t[1], t[2]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p28_p04);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p28_p04);
  v[2] = _mm_madd_epi16(u[2], k__cospi_p12_p20);
  v[3] = _mm_madd_epi16(u[3], k__cospi_p12_p20);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m20_p12);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m20_p12);
  v[6] = _mm_madd_epi16(u[0], k__cospi_m04_p28);
  v[7] = _mm_madd_epi16(u[1], k__cospi_m04_p28);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  in[2] = _mm_packs_epi32(v[0], v[1]);
  in[6] = _mm_packs_epi32(v[4], v[5]);
  in[10] = _mm_packs_epi32(v[2], v[3]);
  in[14] = _mm_packs_epi32(v[6], v[7]);

  // stage 2
  u[0] = _mm_unpacklo_epi16(s[2], s[5]);
  u[1] = _mm_unpackhi_epi16(s[2], s[5]);
  u[2] = _mm_unpacklo_epi16(s[3], s[4]);
  u[3] = _mm_unpackhi_epi16(s[3], s[4]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
  v[2] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
  v[3] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
  v[6] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
  v[7] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  t[2] = _mm_packs_epi32(v[0], v[1]);
  t[3] = _mm_packs_epi32(v[2], v[3]);
  t[4] = _mm_packs_epi32(v[4], v[5]);
  t[5] = _mm_packs_epi32(v[6], v[7]);

  // stage 3
  p[0] = _mm_add_epi16(s[0], t[3]);
  p[1] = _mm_add_epi16(s[1], t[2]);
  p[2] = _mm_sub_epi16(s[1], t[2]);
  p[3] = _mm_sub_epi16(s[0], t[3]);
  p[4] = _mm_sub_epi16(s[7], t[4]);
  p[5] = _mm_sub_epi16(s[6], t[5]);
  p[6] = _mm_add_epi16(s[6], t[5]);
  p[7] = _mm_add_epi16(s[7], t[4]);

  // stage 4
  u[0] = _mm_unpacklo_epi16(p[1], p[6]);
  u[1] = _mm_unpackhi_epi16(p[1], p[6]);
  u[2] = _mm_unpacklo_epi16(p[2], p[5]);
  u[3] = _mm_unpackhi_epi16(p[2], p[5]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m08_p24);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m08_p24);
  v[2] = _mm_madd_epi16(u[2], k__cospi_m24_m08);
  v[3] = _mm_madd_epi16(u[3], k__cospi_m24_m08);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m08_p24);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m08_p24);
  v[6] = _mm_madd_epi16(u[0], k__cospi_p24_p08);
  v[7] = _mm_madd_epi16(u[1], k__cospi_p24_p08);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

  t[1] = _mm_packs_epi32(v[0], v[1]);
  t[2] = _mm_packs_epi32(v[2], v[3]);
  t[5] = _mm_packs_epi32(v[4], v[5]);
  t[6] = _mm_packs_epi32(v[6], v[7]);

  // stage 5
  s[0] = _mm_add_epi16(p[0], t[1]);
  s[1] = _mm_sub_epi16(p[0], t[1]);
  s[2] = _mm_sub_epi16(p[3], t[2]);
  s[3] = _mm_add_epi16(p[3], t[2]);
  s[4] = _mm_add_epi16(p[4], t[5]);
  s[5] = _mm_sub_epi16(p[4], t[5]);
  s[6] = _mm_sub_epi16(p[7], t[6]);
  s[7] = _mm_add_epi16(p[7], t[6]);

  // stage 6
  u[0] = _mm_unpacklo_epi16(s[0], s[7]);
  u[1] = _mm_unpackhi_epi16(s[0], s[7]);
  u[2] = _mm_unpacklo_epi16(s[1], s[6]);
  u[3] = _mm_unpackhi_epi16(s[1], s[6]);
  u[4] = _mm_unpacklo_epi16(s[2], s[5]);
  u[5] = _mm_unpackhi_epi16(s[2], s[5]);
  u[6] = _mm_unpacklo_epi16(s[3], s[4]);
  u[7] = _mm_unpackhi_epi16(s[3], s[4]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p30_p02);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p30_p02);
  v[2] = _mm_madd_epi16(u[2], k__cospi_p14_p18);
  v[3] = _mm_madd_epi16(u[3], k__cospi_p14_p18);
  v[4] = _mm_madd_epi16(u[4], k__cospi_p22_p10);
  v[5] = _mm_madd_epi16(u[5], k__cospi_p22_p10);
  v[6] = _mm_madd_epi16(u[6], k__cospi_p06_p26);
  v[7] = _mm_madd_epi16(u[7], k__cospi_p06_p26);
  v[8] = _mm_madd_epi16(u[6], k__cospi_m26_p06);
  v[9] = _mm_madd_epi16(u[7], k__cospi_m26_p06);
  v[10] = _mm_madd_epi16(u[4], k__cospi_m10_p22);
  v[11] = _mm_madd_epi16(u[5], k__cospi_m10_p22);
  v[12] = _mm_madd_epi16(u[2], k__cospi_m18_p14);
  v[13] = _mm_madd_epi16(u[3], k__cospi_m18_p14);
  v[14] = _mm_madd_epi16(u[0], k__cospi_m02_p30);
  v[15] = _mm_madd_epi16(u[1], k__cospi_m02_p30);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  in[1]  = _mm_packs_epi32(v[0], v[1]);
  in[9]  = _mm_packs_epi32(v[2], v[3]);
  in[5]  = _mm_packs_epi32(v[4], v[5]);
  in[13] = _mm_packs_epi32(v[6], v[7]);
  in[3]  = _mm_packs_epi32(v[8], v[9]);
  in[11] = _mm_packs_epi32(v[10], v[11]);
  in[7]  = _mm_packs_epi32(v[12], v[13]);
  in[15] = _mm_packs_epi32(v[14], v[15]);
}

void fadst16_8col_avx2(__m128i *in) {
  // perform 16x16 1-D ADST for 8 columns
  __m128i s[16], x[16], u[32], v[32];
  const __m128i k__cospi_p01_p31 = pair_set_epi16(cospi_1_64, cospi_31_64);
  const __m128i k__cospi_p31_m01 = pair_set_epi16(cospi_31_64, -cospi_1_64);
  const __m128i k__cospi_p05_p27 = pair_set_epi16(cospi_5_64, cospi_27_64);
  const __m128i k__cospi_p27_m05 = pair_set_epi16(cospi_27_64, -cospi_5_64);
  const __m128i k__cospi_p09_p23 = pair_set_epi16(cospi_9_64, cospi_23_64);
  const __m128i k__cospi_p23_m09 = pair_set_epi16(cospi_23_64, -cospi_9_64);
  const __m128i k__cospi_p13_p19 = pair_set_epi16(cospi_13_64, cospi_19_64);
  const __m128i k__cospi_p19_m13 = pair_set_epi16(cospi_19_64, -cospi_13_64);
  const __m128i k__cospi_p17_p15 = pair_set_epi16(cospi_17_64, cospi_15_64);
  const __m128i k__cospi_p15_m17 = pair_set_epi16(cospi_15_64, -cospi_17_64);
  const __m128i k__cospi_p21_p11 = pair_set_epi16(cospi_21_64, cospi_11_64);
  const __m128i k__cospi_p11_m21 = pair_set_epi16(cospi_11_64, -cospi_21_64);
  const __m128i k__cospi_p25_p07 = pair_set_epi16(cospi_25_64, cospi_7_64);
  const __m128i k__cospi_p07_m25 = pair_set_epi16(cospi_7_64, -cospi_25_64);
  const __m128i k__cospi_p29_p03 = pair_set_epi16(cospi_29_64, cospi_3_64);
  const __m128i k__cospi_p03_m29 = pair_set_epi16(cospi_3_64, -cospi_29_64);
  const __m128i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
  const __m128i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
  const __m128i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
  const __m128i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
  const __m128i k__cospi_m28_p04 = pair_set_epi16(-cospi_28_64, cospi_4_64);
  const __m128i k__cospi_m12_p20 = pair_set_epi16(-cospi_12_64, cospi_20_64);
  const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
  const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
  const __m128i k__cospi_m16_m16 = _mm_set1_epi16(-cospi_16_64);
  const __m128i k__cospi_p16_p16 = _mm_set1_epi16(cospi_16_64);
  const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i kZero = _mm_set1_epi16(0);

  u[0] = _mm_unpacklo_epi16(in[15], in[0]);
  u[1] = _mm_unpackhi_epi16(in[15], in[0]);
  u[2] = _mm_unpacklo_epi16(in[13], in[2]);
  u[3] = _mm_unpackhi_epi16(in[13], in[2]);
  u[4] = _mm_unpacklo_epi16(in[11], in[4]);
  u[5] = _mm_unpackhi_epi16(in[11], in[4]);
  u[6] = _mm_unpacklo_epi16(in[9], in[6]);
  u[7] = _mm_unpackhi_epi16(in[9], in[6]);
  u[8] = _mm_unpacklo_epi16(in[7], in[8]);
  u[9] = _mm_unpackhi_epi16(in[7], in[8]);
  u[10] = _mm_unpacklo_epi16(in[5], in[10]);
  u[11] = _mm_unpackhi_epi16(in[5], in[10]);
  u[12] = _mm_unpacklo_epi16(in[3], in[12]);
  u[13] = _mm_unpackhi_epi16(in[3], in[12]);
  u[14] = _mm_unpacklo_epi16(in[1], in[14]);
  u[15] = _mm_unpackhi_epi16(in[1], in[14]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p01_p31);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p01_p31);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p31_m01);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p31_m01);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p05_p27);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p05_p27);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p27_m05);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p27_m05);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p09_p23);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p09_p23);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p23_m09);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p23_m09);
  v[12] = _mm_madd_epi16(u[6], k__cospi_p13_p19);
  v[13] = _mm_madd_epi16(u[7], k__cospi_p13_p19);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p19_m13);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p19_m13);
  v[16] = _mm_madd_epi16(u[8], k__cospi_p17_p15);
  v[17] = _mm_madd_epi16(u[9], k__cospi_p17_p15);
  v[18] = _mm_madd_epi16(u[8], k__cospi_p15_m17);
  v[19] = _mm_madd_epi16(u[9], k__cospi_p15_m17);
  v[20] = _mm_madd_epi16(u[10], k__cospi_p21_p11);
  v[21] = _mm_madd_epi16(u[11], k__cospi_p21_p11);
  v[22] = _mm_madd_epi16(u[10], k__cospi_p11_m21);
  v[23] = _mm_madd_epi16(u[11], k__cospi_p11_m21);
  v[24] = _mm_madd_epi16(u[12], k__cospi_p25_p07);
  v[25] = _mm_madd_epi16(u[13], k__cospi_p25_p07);
  v[26] = _mm_madd_epi16(u[12], k__cospi_p07_m25);
  v[27] = _mm_madd_epi16(u[13], k__cospi_p07_m25);
  v[28] = _mm_madd_epi16(u[14], k__cospi_p29_p03);
  v[29] = _mm_madd_epi16(u[15], k__cospi_p29_p03);
  v[30] = _mm_madd_epi16(u[14], k__cospi_p03_m29);
  v[31] = _mm_madd_epi16(u[15], k__cospi_p03_m29);

  u[0] = _mm_add_epi32(v[0], v[16]);
  u[1] = _mm_add_epi32(v[1], v[17]);
  u[2] = _mm_add_epi32(v[2], v[18]);
  u[3] = _mm_add_epi32(v[3], v[19]);
  u[4] = _mm_add_epi32(v[4], v[20]);
  u[5] = _mm_add_epi32(v[5], v[21]);
  u[6] = _mm_add_epi32(v[6], v[22]);
  u[7] = _mm_add_epi32(v[7], v[23]);
  u[8] = _mm_add_epi32(v[8], v[24]);
  u[9] = _mm_add_epi32(v[9], v[25]);
  u[10] = _mm_add_epi32(v[10], v[26]);
  u[11] = _mm_add_epi32(v[11], v[27]);
  u[12] = _mm_add_epi32(v[12], v[28]);
  u[13] = _mm_add_epi32(v[13], v[29]);
  u[14] = _mm_add_epi32(v[14], v[30]);
  u[15] = _mm_add_epi32(v[15], v[31]);
  u[16] = _mm_sub_epi32(v[0], v[16]);
  u[17] = _mm_sub_epi32(v[1], v[17]);
  u[18] = _mm_sub_epi32(v[2], v[18]);
  u[19] = _mm_sub_epi32(v[3], v[19]);
  u[20] = _mm_sub_epi32(v[4], v[20]);
  u[21] = _mm_sub_epi32(v[5], v[21]);
  u[22] = _mm_sub_epi32(v[6], v[22]);
  u[23] = _mm_sub_epi32(v[7], v[23]);
  u[24] = _mm_sub_epi32(v[8], v[24]);
  u[25] = _mm_sub_epi32(v[9], v[25]);
  u[26] = _mm_sub_epi32(v[10], v[26]);
  u[27] = _mm_sub_epi32(v[11], v[27]);
  u[28] = _mm_sub_epi32(v[12], v[28]);
  u[29] = _mm_sub_epi32(v[13], v[29]);
  u[30] = _mm_sub_epi32(v[14], v[30]);
  u[31] = _mm_sub_epi32(v[15], v[31]);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
  v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
  v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
  v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
  v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
  v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
  v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
  v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
  v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
  v[16] = _mm_add_epi32(u[16], k__DCT_CONST_ROUNDING);
  v[17] = _mm_add_epi32(u[17], k__DCT_CONST_ROUNDING);
  v[18] = _mm_add_epi32(u[18], k__DCT_CONST_ROUNDING);
  v[19] = _mm_add_epi32(u[19], k__DCT_CONST_ROUNDING);
  v[20] = _mm_add_epi32(u[20], k__DCT_CONST_ROUNDING);
  v[21] = _mm_add_epi32(u[21], k__DCT_CONST_ROUNDING);
  v[22] = _mm_add_epi32(u[22], k__DCT_CONST_ROUNDING);
  v[23] = _mm_add_epi32(u[23], k__DCT_CONST_ROUNDING);
  v[24] = _mm_add_epi32(u[24], k__DCT_CONST_ROUNDING);
  v[25] = _mm_add_epi32(u[25], k__DCT_CONST_ROUNDING);
  v[26] = _mm_add_epi32(u[26], k__DCT_CONST_ROUNDING);
  v[27] = _mm_add_epi32(u[27], k__DCT_CONST_ROUNDING);
  v[28] = _mm_add_epi32(u[28], k__DCT_CONST_ROUNDING);
  v[29] = _mm_add_epi32(u[29], k__DCT_CONST_ROUNDING);
  v[30] = _mm_add_epi32(u[30], k__DCT_CONST_ROUNDING);
  v[31] = _mm_add_epi32(u[31], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
  u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
  u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
  u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
  u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
  u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
  u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
  u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
  u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
  u[16] = _mm_srai_epi32(v[16], DCT_CONST_BITS);
  u[17] = _mm_srai_epi32(v[17], DCT_CONST_BITS);
  u[18] = _mm_srai_epi32(v[18], DCT_CONST_BITS);
  u[19] = _mm_srai_epi32(v[19], DCT_CONST_BITS);
  u[20] = _mm_srai_epi32(v[20], DCT_CONST_BITS);
  u[21] = _mm_srai_epi32(v[21], DCT_CONST_BITS);
  u[22] = _mm_srai_epi32(v[22], DCT_CONST_BITS);
  u[23] = _mm_srai_epi32(v[23], DCT_CONST_BITS);
  u[24] = _mm_srai_epi32(v[24], DCT_CONST_BITS);
  u[25] = _mm_srai_epi32(v[25], DCT_CONST_BITS);
  u[26] = _mm_srai_epi32(v[26], DCT_CONST_BITS);
  u[27] = _mm_srai_epi32(v[27], DCT_CONST_BITS);
  u[28] = _mm_srai_epi32(v[28], DCT_CONST_BITS);
  u[29] = _mm_srai_epi32(v[29], DCT_CONST_BITS);
  u[30] = _mm_srai_epi32(v[30], DCT_CONST_BITS);
  u[31] = _mm_srai_epi32(v[31], DCT_CONST_BITS);

  s[0] = _mm_packs_epi32(u[0], u[1]);
  s[1] = _mm_packs_epi32(u[2], u[3]);
  s[2] = _mm_packs_epi32(u[4], u[5]);
  s[3] = _mm_packs_epi32(u[6], u[7]);
  s[4] = _mm_packs_epi32(u[8], u[9]);
  s[5] = _mm_packs_epi32(u[10], u[11]);
  s[6] = _mm_packs_epi32(u[12], u[13]);
  s[7] = _mm_packs_epi32(u[14], u[15]);
  s[8] = _mm_packs_epi32(u[16], u[17]);
  s[9] = _mm_packs_epi32(u[18], u[19]);
  s[10] = _mm_packs_epi32(u[20], u[21]);
  s[11] = _mm_packs_epi32(u[22], u[23]);
  s[12] = _mm_packs_epi32(u[24], u[25]);
  s[13] = _mm_packs_epi32(u[26], u[27]);
  s[14] = _mm_packs_epi32(u[28], u[29]);
  s[15] = _mm_packs_epi32(u[30], u[31]);

  // stage 2
  u[0] = _mm_unpacklo_epi16(s[8], s[9]);
  u[1] = _mm_unpackhi_epi16(s[8], s[9]);
  u[2] = _mm_unpacklo_epi16(s[10], s[11]);
  u[3] = _mm_unpackhi_epi16(s[10], s[11]);
  u[4] = _mm_unpacklo_epi16(s[12], s[13]);
  u[5] = _mm_unpackhi_epi16(s[12], s[13]);
  u[6] = _mm_unpacklo_epi16(s[14], s[15]);
  u[7] = _mm_unpackhi_epi16(s[14], s[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p04_p28);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p04_p28);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p28_m04);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p28_m04);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p20_p12);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p20_p12);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p12_m20);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p12_m20);
  v[8] = _mm_madd_epi16(u[4], k__cospi_m28_p04);
  v[9] = _mm_madd_epi16(u[5], k__cospi_m28_p04);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p04_p28);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p04_p28);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m12_p20);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m12_p20);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p20_p12);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p20_p12);

  u[0] = _mm_add_epi32(v[0], v[8]);
  u[1] = _mm_add_epi32(v[1], v[9]);
  u[2] = _mm_add_epi32(v[2], v[10]);
  u[3] = _mm_add_epi32(v[3], v[11]);
  u[4] = _mm_add_epi32(v[4], v[12]);
  u[5] = _mm_add_epi32(v[5], v[13]);
  u[6] = _mm_add_epi32(v[6], v[14]);
  u[7] = _mm_add_epi32(v[7], v[15]);
  u[8] = _mm_sub_epi32(v[0], v[8]);
  u[9] = _mm_sub_epi32(v[1], v[9]);
  u[10] = _mm_sub_epi32(v[2], v[10]);
  u[11] = _mm_sub_epi32(v[3], v[11]);
  u[12] = _mm_sub_epi32(v[4], v[12]);
  u[13] = _mm_sub_epi32(v[5], v[13]);
  u[14] = _mm_sub_epi32(v[6], v[14]);
  u[15] = _mm_sub_epi32(v[7], v[15]);

  v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
  v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
  v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
  v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
  v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
  v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
  v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
  v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
  v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

  u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
  u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
  u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
  u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
  u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
  u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
  u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
  u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
  u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
  u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
  u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
  u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
  u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
  u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
  u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
  u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

  x[0] = _mm_add_epi16(s[0], s[4]);
  x[1] = _mm_add_epi16(s[1], s[5]);
  x[2] = _mm_add_epi16(s[2], s[6]);
  x[3] = _mm_add_epi16(s[3], s[7]);
  x[4] = _mm_sub_epi16(s[0], s[4]);
  x[5] = _mm_sub_epi16(s[1], s[5]);
  x[6] = _mm_sub_epi16(s[2], s[6]);
  x[7] = _mm_sub_epi16(s[3], s[7]);
  x[8] = _mm_packs_epi32(u[0], u[1]);
  x[9] = _mm_packs_epi32(u[2], u[3]);
  x[10] = _mm_packs_epi32(u[4], u[5]);
  x[11] = _mm_packs_epi32(u[6], u[7]);
  x[12] = _mm_packs_epi32(u[8], u[9]);
  x[13] = _mm_packs_epi32(u[10], u[11]);
  x[14] = _mm_packs_epi32(u[12], u[13]);
  x[15] = _mm_packs_epi32(u[14], u[15]);

  // stage 3
  u[0] = _mm_unpacklo_epi16(x[4], x[5]);
  u[1] = _mm_unpackhi_epi16(x[4], x[5]);
  u[2] = _mm_unpacklo_epi16(x[6], x[7]);
  u[3] = _mm_unpackhi_epi16(x[6], x[7]);
  u[4] = _mm_unpacklo_epi16(x[12], x[13]);
  u[5] = _mm_unpackhi_epi16(x[12], x[13]);
  u[6] = _mm_unpacklo_epi16(x[14], x[15]);
  u[7] = _mm_unpackhi_epi16(x[14], x[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_p08_p24);
  v[1] = _mm_madd_epi16(u[1], k__cospi_p08_p24);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p24_m08);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p24_m08);
  v[4] = _mm_madd_epi16(u[2], k__cospi_m24_p08);
  v[5] = _mm_madd_epi16(u[3], k__cospi_m24_p08);
  v[6] = _mm_madd_epi16(u[2], k__cospi_p08_p24);
  v[7] = _mm_madd_epi16(u[3], k__cospi_p08_p24);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p08_p24);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p08_p24);
  v[10] = _mm_madd_epi16(u[4], k__cospi_p24_m08);
  v[11] = _mm_madd_epi16(u[5], k__cospi_p24_m08);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m24_p08);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m24_p08);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p08_p24);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p08_p24);

  u[0] = _mm_add_epi32(v[0], v[4]);
  u[1] = _mm_add_epi32(v[1], v[5]);
  u[2] = _mm_add_epi32(v[2], v[6]);
  u[3] = _mm_add_epi32(v[3], v[7]);
  u[4] = _mm_sub_epi32(v[0], v[4]);
  u[5] = _mm_sub_epi32(v[1], v[5]);
  u[6] = _mm_sub_epi32(v[2], v[6]);
  u[7] = _mm_sub_epi32(v[3], v[7]);
  u[8] = _mm_add_epi32(v[8], v[12]);
  u[9] = _mm_add_epi32(v[9], v[13]);
  u[10] = _mm_add_epi32(v[10], v[14]);
  u[11] = _mm_add_epi32(v[11], v[15]);
  u[12] = _mm_sub_epi32(v[8], v[12]);
  u[13] = _mm_sub_epi32(v[9], v[13]);
  u[14] = _mm_sub_epi32(v[10], v[14]);
  u[15] = _mm_sub_epi32(v[11], v[15]);

  u[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  s[0] = _mm_add_epi16(x[0], x[2]);
  s[1] = _mm_add_epi16(x[1], x[3]);
  s[2] = _mm_sub_epi16(x[0], x[2]);
  s[3] = _mm_sub_epi16(x[1], x[3]);
  s[4] = _mm_packs_epi32(v[0], v[1]);
  s[5] = _mm_packs_epi32(v[2], v[3]);
  s[6] = _mm_packs_epi32(v[4], v[5]);
  s[7] = _mm_packs_epi32(v[6], v[7]);
  s[8] = _mm_add_epi16(x[8], x[10]);
  s[9] = _mm_add_epi16(x[9], x[11]);
  s[10] = _mm_sub_epi16(x[8], x[10]);
  s[11] = _mm_sub_epi16(x[9], x[11]);
  s[12] = _mm_packs_epi32(v[8], v[9]);
  s[13] = _mm_packs_epi32(v[10], v[11]);
  s[14] = _mm_packs_epi32(v[12], v[13]);
  s[15] = _mm_packs_epi32(v[14], v[15]);

  // stage 4
  u[0] = _mm_unpacklo_epi16(s[2], s[3]);
  u[1] = _mm_unpackhi_epi16(s[2], s[3]);
  u[2] = _mm_unpacklo_epi16(s[6], s[7]);
  u[3] = _mm_unpackhi_epi16(s[6], s[7]);
  u[4] = _mm_unpacklo_epi16(s[10], s[11]);
  u[5] = _mm_unpackhi_epi16(s[10], s[11]);
  u[6] = _mm_unpacklo_epi16(s[14], s[15]);
  u[7] = _mm_unpackhi_epi16(s[14], s[15]);

  v[0] = _mm_madd_epi16(u[0], k__cospi_m16_m16);
  v[1] = _mm_madd_epi16(u[1], k__cospi_m16_m16);
  v[2] = _mm_madd_epi16(u[0], k__cospi_p16_m16);
  v[3] = _mm_madd_epi16(u[1], k__cospi_p16_m16);
  v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
  v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
  v[6] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
  v[7] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
  v[8] = _mm_madd_epi16(u[4], k__cospi_p16_p16);
  v[9] = _mm_madd_epi16(u[5], k__cospi_p16_p16);
  v[10] = _mm_madd_epi16(u[4], k__cospi_m16_p16);
  v[11] = _mm_madd_epi16(u[5], k__cospi_m16_p16);
  v[12] = _mm_madd_epi16(u[6], k__cospi_m16_m16);
  v[13] = _mm_madd_epi16(u[7], k__cospi_m16_m16);
  v[14] = _mm_madd_epi16(u[6], k__cospi_p16_m16);
  v[15] = _mm_madd_epi16(u[7], k__cospi_p16_m16);

  u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
  u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
  u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
  u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
  u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
  u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
  u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
  u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
  u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
  u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
  u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
  u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
  u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
  u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
  u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
  u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

  v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
  v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
  v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
  v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
  v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
  v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
  v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
  v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
  v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
  v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
  v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
  v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
  v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
  v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
  v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
  v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

  in[0] = s[0];
  in[1] = _mm_sub_epi16(kZero, s[8]);
  in[2] = s[12];
  in[3] = _mm_sub_epi16(kZero, s[4]);
  in[4] = _mm_packs_epi32(v[4], v[5]);
  in[5] = _mm_packs_epi32(v[12], v[13]);
  in[6] = _mm_packs_epi32(v[8], v[9]);
  in[7] = _mm_packs_epi32(v[0], v[1]);
  in[8] = _mm_packs_epi32(v[2], v[3]);
  in[9] = _mm_packs_epi32(v[10], v[11]);
  in[10] = _mm_packs_epi32(v[14], v[15]);
  in[11] = _mm_packs_epi32(v[6], v[7]);
  in[12] = s[5];
  in[13] = _mm_sub_epi16(kZero, s[13]);
  in[14] = s[9];
  in[15] = _mm_sub_epi16(kZero, s[1]);
}

void fdct16_avx2(__m128i *in0, __m128i *in1) {
  fdct16_8col_avx2(in0);
  fdct16_8col_avx2(in1);
  array_transpose_16x16_avx2(in0, in1);
}

void fadst16_avx2(__m128i *in0, __m128i *in1) {
  fadst16_8col_avx2(in0);
  fadst16_8col_avx2(in1);
  array_transpose_16x16_avx2(in0, in1);
}

void vp9_fht16x16_avx2(const int16_t *input, int16_t *output,
                      int stride, int tx_type) {
  __m128i in0[16], in1[16];

  switch (tx_type) {
    case DCT_DCT:
      vp9_fdct16x16_avx2(input, output, stride);
      break;
    case ADST_DCT:
      load_buffer_16x16_avx2(input, in0, in1, stride);
      fadst16_avx2(in0, in1);
      right_shift_16x16_avx2(in0, in1);
      fdct16_avx2(in0, in1);
      write_buffer_16x16_avx2(output, in0, in1, 16);
      break;
    case DCT_ADST:
      load_buffer_16x16_avx2(input, in0, in1, stride);
      fdct16_avx2(in0, in1);
      right_shift_16x16_avx2(in0, in1);
      fadst16_avx2(in0, in1);
      write_buffer_16x16_avx2(output, in0, in1, 16);
      break;
    case ADST_ADST:
      load_buffer_16x16_avx2(input, in0, in1, stride);
      fadst16_avx2(in0, in1);
      right_shift_16x16_avx2(in0, in1);
      fadst16_avx2(in0, in1);
      write_buffer_16x16_avx2(output, in0, in1, 16);
      break;
    default:
      assert(0);
      break;
  }
}

#define FDCT32x32_2D_AVX2 vp9_fdct32x32_rd_avx2
#define FDCT32x32_HIGH_PRECISION 0
#include "vp9/encoder/x86/vp9_dct32x32_avx2.c"
#undef  FDCT32x32_2D_AVX2
#undef  FDCT32x32_HIGH_PRECISION

#define FDCT32x32_2D_AVX2 vp9_fdct32x32_avx2
#define FDCT32x32_HIGH_PRECISION 1
#include "vp9/encoder/x86/vp9_dct32x32_avx2.c" // NOLINT
#undef  FDCT32x32_2D_AVX2
#undef  FDCT32x32_HIGH_PRECISION
