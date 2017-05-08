/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tmmintrin.h>

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/x86/inv_txfm_sse2.h"
#include "aom_dsp/x86/txfm_common_sse2.h"

void aom_idct8x8_64_add_ssse3(const tran_low_t *input, uint8_t *dest,
                              int stride) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i final_rounding = _mm_set1_epi16(1 << 4);
  const __m128i stg1_0 = pair_set_epi16(cospi_28_64, -cospi_4_64);
  const __m128i stg1_1 = pair_set_epi16(cospi_4_64, cospi_28_64);
  const __m128i stg1_2 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i stg1_3 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i stk2_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
  const __m128i stk2_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i stg2_2 = pair_set_epi16(cospi_24_64, -cospi_8_64);
  const __m128i stg2_3 = pair_set_epi16(cospi_8_64, cospi_24_64);

  __m128i in0, in1, in2, in3, in4, in5, in6, in7;
  __m128i stp1_0, stp1_1, stp1_2, stp1_3, stp1_4, stp1_5, stp1_6, stp1_7;
  __m128i stp2_0, stp2_1, stp2_2, stp2_3, stp2_4, stp2_5, stp2_6, stp2_7;
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int i;

  // Load input data.
  in0 = load_input_data(input);
  in1 = load_input_data(input + 8 * 1);
  in2 = load_input_data(input + 8 * 2);
  in3 = load_input_data(input + 8 * 3);
  in4 = load_input_data(input + 8 * 4);
  in5 = load_input_data(input + 8 * 5);
  in6 = load_input_data(input + 8 * 6);
  in7 = load_input_data(input + 8 * 7);

  // 2-D
  for (i = 0; i < 2; i++) {
    // 8x8 Transpose is copied from vpx_fdct8x8_sse2()
    TRANSPOSE_8X8(in0, in1, in2, in3, in4, in5, in6, in7, in0, in1, in2, in3,
                  in4, in5, in6, in7);

    // 4-stage 1D idct8x8
    {
      /* Stage1 */
      {
        const __m128i lo_17 = _mm_unpacklo_epi16(in1, in7);
        const __m128i hi_17 = _mm_unpackhi_epi16(in1, in7);
        const __m128i lo_35 = _mm_unpacklo_epi16(in3, in5);
        const __m128i hi_35 = _mm_unpackhi_epi16(in3, in5);

        {
          tmp0 = _mm_madd_epi16(lo_17, stg1_0);
          tmp1 = _mm_madd_epi16(hi_17, stg1_0);
          tmp2 = _mm_madd_epi16(lo_17, stg1_1);
          tmp3 = _mm_madd_epi16(hi_17, stg1_1);
          tmp4 = _mm_madd_epi16(lo_35, stg1_2);
          tmp5 = _mm_madd_epi16(hi_35, stg1_2);
          tmp6 = _mm_madd_epi16(lo_35, stg1_3);
          tmp7 = _mm_madd_epi16(hi_35, stg1_3);

          tmp0 = _mm_add_epi32(tmp0, rounding);
          tmp1 = _mm_add_epi32(tmp1, rounding);
          tmp2 = _mm_add_epi32(tmp2, rounding);
          tmp3 = _mm_add_epi32(tmp3, rounding);
          tmp4 = _mm_add_epi32(tmp4, rounding);
          tmp5 = _mm_add_epi32(tmp5, rounding);
          tmp6 = _mm_add_epi32(tmp6, rounding);
          tmp7 = _mm_add_epi32(tmp7, rounding);

          tmp0 = _mm_srai_epi32(tmp0, 14);
          tmp1 = _mm_srai_epi32(tmp1, 14);
          tmp2 = _mm_srai_epi32(tmp2, 14);
          tmp3 = _mm_srai_epi32(tmp3, 14);
          tmp4 = _mm_srai_epi32(tmp4, 14);
          tmp5 = _mm_srai_epi32(tmp5, 14);
          tmp6 = _mm_srai_epi32(tmp6, 14);
          tmp7 = _mm_srai_epi32(tmp7, 14);

          stp1_4 = _mm_packs_epi32(tmp0, tmp1);
          stp1_7 = _mm_packs_epi32(tmp2, tmp3);
          stp1_5 = _mm_packs_epi32(tmp4, tmp5);
          stp1_6 = _mm_packs_epi32(tmp6, tmp7);
        }
      }

      /* Stage2 */
      {
        const __m128i lo_26 = _mm_unpacklo_epi16(in2, in6);
        const __m128i hi_26 = _mm_unpackhi_epi16(in2, in6);

        {
          tmp0 = _mm_unpacklo_epi16(in0, in4);
          tmp1 = _mm_unpackhi_epi16(in0, in4);

          tmp2 = _mm_madd_epi16(tmp0, stk2_0);
          tmp3 = _mm_madd_epi16(tmp1, stk2_0);
          tmp4 = _mm_madd_epi16(tmp0, stk2_1);
          tmp5 = _mm_madd_epi16(tmp1, stk2_1);

          tmp2 = _mm_add_epi32(tmp2, rounding);
          tmp3 = _mm_add_epi32(tmp3, rounding);
          tmp4 = _mm_add_epi32(tmp4, rounding);
          tmp5 = _mm_add_epi32(tmp5, rounding);

          tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);
          tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS);
          tmp4 = _mm_srai_epi32(tmp4, DCT_CONST_BITS);
          tmp5 = _mm_srai_epi32(tmp5, DCT_CONST_BITS);

          stp2_0 = _mm_packs_epi32(tmp2, tmp3);
          stp2_1 = _mm_packs_epi32(tmp4, tmp5);

          tmp0 = _mm_madd_epi16(lo_26, stg2_2);
          tmp1 = _mm_madd_epi16(hi_26, stg2_2);
          tmp2 = _mm_madd_epi16(lo_26, stg2_3);
          tmp3 = _mm_madd_epi16(hi_26, stg2_3);

          tmp0 = _mm_add_epi32(tmp0, rounding);
          tmp1 = _mm_add_epi32(tmp1, rounding);
          tmp2 = _mm_add_epi32(tmp2, rounding);
          tmp3 = _mm_add_epi32(tmp3, rounding);

          tmp0 = _mm_srai_epi32(tmp0, 14);
          tmp1 = _mm_srai_epi32(tmp1, 14);
          tmp2 = _mm_srai_epi32(tmp2, 14);
          tmp3 = _mm_srai_epi32(tmp3, 14);

          stp2_2 = _mm_packs_epi32(tmp0, tmp1);
          stp2_3 = _mm_packs_epi32(tmp2, tmp3);
        }

        stp2_4 = _mm_add_epi16(stp1_4, stp1_5);
        stp2_5 = _mm_sub_epi16(stp1_4, stp1_5);
        stp2_6 = _mm_sub_epi16(stp1_7, stp1_6);
        stp2_7 = _mm_add_epi16(stp1_7, stp1_6);
      }

      /* Stage3 */
      {
        stp1_0 = _mm_add_epi16(stp2_0, stp2_3);
        stp1_1 = _mm_add_epi16(stp2_1, stp2_2);
        stp1_2 = _mm_sub_epi16(stp2_1, stp2_2);
        stp1_3 = _mm_sub_epi16(stp2_0, stp2_3);

        tmp0 = _mm_unpacklo_epi16(stp2_6, stp2_5);
        tmp1 = _mm_unpackhi_epi16(stp2_6, stp2_5);

        tmp2 = _mm_madd_epi16(tmp0, stk2_1);
        tmp3 = _mm_madd_epi16(tmp1, stk2_1);
        tmp4 = _mm_madd_epi16(tmp0, stk2_0);
        tmp5 = _mm_madd_epi16(tmp1, stk2_0);

        tmp2 = _mm_add_epi32(tmp2, rounding);
        tmp3 = _mm_add_epi32(tmp3, rounding);
        tmp4 = _mm_add_epi32(tmp4, rounding);
        tmp5 = _mm_add_epi32(tmp5, rounding);

        tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);
        tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS);
        tmp4 = _mm_srai_epi32(tmp4, DCT_CONST_BITS);
        tmp5 = _mm_srai_epi32(tmp5, DCT_CONST_BITS);

        stp1_5 = _mm_packs_epi32(tmp2, tmp3);
        stp1_6 = _mm_packs_epi32(tmp4, tmp5);
      }

      /* Stage4  */
      in0 = _mm_add_epi16(stp1_0, stp2_7);
      in1 = _mm_add_epi16(stp1_1, stp1_6);
      in2 = _mm_add_epi16(stp1_2, stp1_5);
      in3 = _mm_add_epi16(stp1_3, stp2_4);
      in4 = _mm_sub_epi16(stp1_3, stp2_4);
      in5 = _mm_sub_epi16(stp1_2, stp1_5);
      in6 = _mm_sub_epi16(stp1_1, stp1_6);
      in7 = _mm_sub_epi16(stp1_0, stp2_7);
    }
  }

  // Final rounding and shift
  in0 = _mm_adds_epi16(in0, final_rounding);
  in1 = _mm_adds_epi16(in1, final_rounding);
  in2 = _mm_adds_epi16(in2, final_rounding);
  in3 = _mm_adds_epi16(in3, final_rounding);
  in4 = _mm_adds_epi16(in4, final_rounding);
  in5 = _mm_adds_epi16(in5, final_rounding);
  in6 = _mm_adds_epi16(in6, final_rounding);
  in7 = _mm_adds_epi16(in7, final_rounding);

  in0 = _mm_srai_epi16(in0, 5);
  in1 = _mm_srai_epi16(in1, 5);
  in2 = _mm_srai_epi16(in2, 5);
  in3 = _mm_srai_epi16(in3, 5);
  in4 = _mm_srai_epi16(in4, 5);
  in5 = _mm_srai_epi16(in5, 5);
  in6 = _mm_srai_epi16(in6, 5);
  in7 = _mm_srai_epi16(in7, 5);

  RECON_AND_STORE(dest + 0 * stride, in0);
  RECON_AND_STORE(dest + 1 * stride, in1);
  RECON_AND_STORE(dest + 2 * stride, in2);
  RECON_AND_STORE(dest + 3 * stride, in3);
  RECON_AND_STORE(dest + 4 * stride, in4);
  RECON_AND_STORE(dest + 5 * stride, in5);
  RECON_AND_STORE(dest + 6 * stride, in6);
  RECON_AND_STORE(dest + 7 * stride, in7);
}

void aom_idct8x8_12_add_ssse3(const tran_low_t *input, uint8_t *dest,
                              int stride) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);
  const __m128i final_rounding = _mm_set1_epi16(1 << 4);
  const __m128i stg1_0 = pair_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m128i stg1_1 = pair_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
  const __m128i stg1_2 = pair_set_epi16(-2 * cospi_20_64, -2 * cospi_20_64);
  const __m128i stg1_3 = pair_set_epi16(2 * cospi_12_64, 2 * cospi_12_64);
  const __m128i stg2_0 = pair_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m128i stk2_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
  const __m128i stk2_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i stg2_2 = pair_set_epi16(2 * cospi_24_64, 2 * cospi_24_64);
  const __m128i stg2_3 = pair_set_epi16(2 * cospi_8_64, 2 * cospi_8_64);
  const __m128i stg3_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);

  __m128i in0, in1, in2, in3, in4, in5, in6, in7;
  __m128i stp1_0, stp1_1, stp1_2, stp1_3, stp1_4, stp1_5, stp1_6, stp1_7;
  __m128i stp2_0, stp2_1, stp2_2, stp2_3, stp2_4, stp2_5, stp2_6, stp2_7;
  __m128i tmp0, tmp1, tmp2, tmp3;

  // Rows. Load 4-row input data.
  in0 = load_input_data(input);
  in1 = load_input_data(input + 8 * 1);
  in2 = load_input_data(input + 8 * 2);
  in3 = load_input_data(input + 8 * 3);

  // 8x4 Transpose
  TRANSPOSE_8X8_10(in0, in1, in2, in3, in0, in1);

  // Stage1
  tmp0 = _mm_mulhrs_epi16(in0, stg1_0);
  tmp1 = _mm_mulhrs_epi16(in0, stg1_1);
  tmp2 = _mm_mulhrs_epi16(in1, stg1_2);
  tmp3 = _mm_mulhrs_epi16(in1, stg1_3);

  stp1_4 = _mm_unpackhi_epi64(tmp0, tmp1);
  stp1_5 = _mm_unpackhi_epi64(tmp2, tmp3);

  // Stage2
  tmp0 = _mm_mulhrs_epi16(in0, stg2_0);
  stp2_0 = _mm_unpacklo_epi64(tmp0, tmp0);

  tmp1 = _mm_mulhrs_epi16(in1, stg2_2);
  tmp2 = _mm_mulhrs_epi16(in1, stg2_3);
  stp2_2 = _mm_unpacklo_epi64(tmp2, tmp1);

  tmp0 = _mm_add_epi16(stp1_4, stp1_5);
  tmp1 = _mm_sub_epi16(stp1_4, stp1_5);

  stp2_4 = tmp0;
  stp2_5 = _mm_unpacklo_epi64(tmp1, zero);
  stp2_6 = _mm_unpackhi_epi64(tmp1, zero);

  tmp0 = _mm_unpacklo_epi16(stp2_5, stp2_6);
  tmp1 = _mm_madd_epi16(tmp0, stg3_0);
  tmp2 = _mm_madd_epi16(tmp0, stk2_0);  // stg3_1 = stk2_0

  tmp1 = _mm_add_epi32(tmp1, rounding);
  tmp2 = _mm_add_epi32(tmp2, rounding);
  tmp1 = _mm_srai_epi32(tmp1, DCT_CONST_BITS);
  tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);

  stp1_5 = _mm_packs_epi32(tmp1, tmp2);

  // Stage3
  tmp2 = _mm_add_epi16(stp2_0, stp2_2);
  tmp3 = _mm_sub_epi16(stp2_0, stp2_2);

  stp1_2 = _mm_unpackhi_epi64(tmp3, tmp2);
  stp1_3 = _mm_unpacklo_epi64(tmp3, tmp2);

  // Stage4
  tmp0 = _mm_add_epi16(stp1_3, stp2_4);
  tmp1 = _mm_add_epi16(stp1_2, stp1_5);
  tmp2 = _mm_sub_epi16(stp1_3, stp2_4);
  tmp3 = _mm_sub_epi16(stp1_2, stp1_5);

  TRANSPOSE_4X8_10(tmp0, tmp1, tmp2, tmp3, in0, in1, in2, in3)

  /* Stage1 */
  stp1_4 = _mm_mulhrs_epi16(in1, stg1_0);
  stp1_7 = _mm_mulhrs_epi16(in1, stg1_1);
  stp1_5 = _mm_mulhrs_epi16(in3, stg1_2);
  stp1_6 = _mm_mulhrs_epi16(in3, stg1_3);

  /* Stage2 */
  stp2_0 = _mm_mulhrs_epi16(in0, stg2_0);
  stp2_1 = _mm_mulhrs_epi16(in0, stg2_0);

  stp2_2 = _mm_mulhrs_epi16(in2, stg2_2);
  stp2_3 = _mm_mulhrs_epi16(in2, stg2_3);

  stp2_4 = _mm_add_epi16(stp1_4, stp1_5);
  stp2_5 = _mm_sub_epi16(stp1_4, stp1_5);
  stp2_6 = _mm_sub_epi16(stp1_7, stp1_6);
  stp2_7 = _mm_add_epi16(stp1_7, stp1_6);

  /* Stage3 */
  stp1_0 = _mm_add_epi16(stp2_0, stp2_3);
  stp1_1 = _mm_add_epi16(stp2_1, stp2_2);
  stp1_2 = _mm_sub_epi16(stp2_1, stp2_2);
  stp1_3 = _mm_sub_epi16(stp2_0, stp2_3);

  tmp0 = _mm_unpacklo_epi16(stp2_6, stp2_5);
  tmp1 = _mm_unpackhi_epi16(stp2_6, stp2_5);

  tmp2 = _mm_madd_epi16(tmp0, stk2_0);
  tmp3 = _mm_madd_epi16(tmp1, stk2_0);
  tmp2 = _mm_add_epi32(tmp2, rounding);
  tmp3 = _mm_add_epi32(tmp3, rounding);
  tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);
  tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS);
  stp1_6 = _mm_packs_epi32(tmp2, tmp3);

  tmp2 = _mm_madd_epi16(tmp0, stk2_1);
  tmp3 = _mm_madd_epi16(tmp1, stk2_1);
  tmp2 = _mm_add_epi32(tmp2, rounding);
  tmp3 = _mm_add_epi32(tmp3, rounding);
  tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS);
  tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS);
  stp1_5 = _mm_packs_epi32(tmp2, tmp3);

  /* Stage4  */
  in0 = _mm_add_epi16(stp1_0, stp2_7);
  in1 = _mm_add_epi16(stp1_1, stp1_6);
  in2 = _mm_add_epi16(stp1_2, stp1_5);
  in3 = _mm_add_epi16(stp1_3, stp2_4);
  in4 = _mm_sub_epi16(stp1_3, stp2_4);
  in5 = _mm_sub_epi16(stp1_2, stp1_5);
  in6 = _mm_sub_epi16(stp1_1, stp1_6);
  in7 = _mm_sub_epi16(stp1_0, stp2_7);

  // Final rounding and shift
  in0 = _mm_adds_epi16(in0, final_rounding);
  in1 = _mm_adds_epi16(in1, final_rounding);
  in2 = _mm_adds_epi16(in2, final_rounding);
  in3 = _mm_adds_epi16(in3, final_rounding);
  in4 = _mm_adds_epi16(in4, final_rounding);
  in5 = _mm_adds_epi16(in5, final_rounding);
  in6 = _mm_adds_epi16(in6, final_rounding);
  in7 = _mm_adds_epi16(in7, final_rounding);

  in0 = _mm_srai_epi16(in0, 5);
  in1 = _mm_srai_epi16(in1, 5);
  in2 = _mm_srai_epi16(in2, 5);
  in3 = _mm_srai_epi16(in3, 5);
  in4 = _mm_srai_epi16(in4, 5);
  in5 = _mm_srai_epi16(in5, 5);
  in6 = _mm_srai_epi16(in6, 5);
  in7 = _mm_srai_epi16(in7, 5);

  RECON_AND_STORE(dest + 0 * stride, in0);
  RECON_AND_STORE(dest + 1 * stride, in1);
  RECON_AND_STORE(dest + 2 * stride, in2);
  RECON_AND_STORE(dest + 3 * stride, in3);
  RECON_AND_STORE(dest + 4 * stride, in4);
  RECON_AND_STORE(dest + 5 * stride, in5);
  RECON_AND_STORE(dest + 6 * stride, in6);
  RECON_AND_STORE(dest + 7 * stride, in7);
}

// Only do addition and subtraction butterfly, size = 16, 32
static INLINE void add_sub_butterfly(const __m128i *in, __m128i *out,
                                     int size) {
  int i = 0;
  const int num = size >> 1;
  const int bound = size - 1;
  while (i < num) {
    out[i] = _mm_add_epi16(in[i], in[bound - i]);
    out[bound - i] = _mm_sub_epi16(in[i], in[bound - i]);
    i++;
  }
}

#define BUTTERFLY_PAIR(x0, x1, co0, co1)         \
  do {                                           \
    tmp0 = _mm_madd_epi16(x0, co0);              \
    tmp1 = _mm_madd_epi16(x1, co0);              \
    tmp2 = _mm_madd_epi16(x0, co1);              \
    tmp3 = _mm_madd_epi16(x1, co1);              \
    tmp0 = _mm_add_epi32(tmp0, rounding);        \
    tmp1 = _mm_add_epi32(tmp1, rounding);        \
    tmp2 = _mm_add_epi32(tmp2, rounding);        \
    tmp3 = _mm_add_epi32(tmp3, rounding);        \
    tmp0 = _mm_srai_epi32(tmp0, DCT_CONST_BITS); \
    tmp1 = _mm_srai_epi32(tmp1, DCT_CONST_BITS); \
    tmp2 = _mm_srai_epi32(tmp2, DCT_CONST_BITS); \
    tmp3 = _mm_srai_epi32(tmp3, DCT_CONST_BITS); \
  } while (0)

static INLINE void butterfly(const __m128i *x0, const __m128i *x1,
                             const __m128i *c0, const __m128i *c1, __m128i *y0,
                             __m128i *y1) {
  __m128i tmp0, tmp1, tmp2, tmp3, u0, u1;
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);

  u0 = _mm_unpacklo_epi16(*x0, *x1);
  u1 = _mm_unpackhi_epi16(*x0, *x1);
  BUTTERFLY_PAIR(u0, u1, *c0, *c1);
  *y0 = _mm_packs_epi32(tmp0, tmp1);
  *y1 = _mm_packs_epi32(tmp2, tmp3);
}

static INLINE void butterfly_self(__m128i *x0, __m128i *x1, const __m128i *c0,
                                  const __m128i *c1) {
  __m128i tmp0, tmp1, tmp2, tmp3, u0, u1;
  const __m128i rounding = _mm_set1_epi32(DCT_CONST_ROUNDING);

  u0 = _mm_unpacklo_epi16(*x0, *x1);
  u1 = _mm_unpackhi_epi16(*x0, *x1);
  BUTTERFLY_PAIR(u0, u1, *c0, *c1);
  *x0 = _mm_packs_epi32(tmp0, tmp1);
  *x1 = _mm_packs_epi32(tmp2, tmp3);
}

static void idct32_34_first_half(const __m128i *in, __m128i *stp1) {
  const __m128i stk2_0 = pair_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
  const __m128i stk2_1 = pair_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
  const __m128i stk2_6 = pair_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
  const __m128i stk2_7 = pair_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);

  const __m128i stk3_0 = pair_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
  const __m128i stk3_1 = pair_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);

  const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
  const __m128i stk4_0 = pair_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
  const __m128i stg4_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
  const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);

  const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i x0, x1, x4, x5, x6, x7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;

  // phase 1

  // 0, 15
  u2 = _mm_mulhrs_epi16(in[2], stk2_1);  // stp2_15
  u3 = _mm_mulhrs_epi16(in[6], stk2_7);  // stp2_12
  v15 = _mm_add_epi16(u2, u3);
  // in[0], in[4]
  x0 = _mm_mulhrs_epi16(in[0], stk4_0);  // stp1[0]
  x7 = _mm_mulhrs_epi16(in[4], stk3_1);  // stp1[7]
  v0 = _mm_add_epi16(x0, x7);            // stp2_0
  stp1[0] = _mm_add_epi16(v0, v15);
  stp1[15] = _mm_sub_epi16(v0, v15);

  // in[2], in[6]
  u0 = _mm_mulhrs_epi16(in[2], stk2_0);             // stp2_8
  u1 = _mm_mulhrs_epi16(in[6], stk2_6);             // stp2_11
  butterfly(&u0, &u2, &stg4_4, &stg4_5, &u4, &u5);  // stp2_9, stp2_14
  butterfly(&u1, &u3, &stg4_6, &stg4_4, &u6, &u7);  // stp2_10, stp2_13

  v8 = _mm_add_epi16(u0, u1);
  v9 = _mm_add_epi16(u4, u6);
  v10 = _mm_sub_epi16(u4, u6);
  v11 = _mm_sub_epi16(u0, u1);
  v12 = _mm_sub_epi16(u2, u3);
  v13 = _mm_sub_epi16(u5, u7);
  v14 = _mm_add_epi16(u5, u7);

  butterfly_self(&v10, &v13, &stg6_0, &stg4_0);
  butterfly_self(&v11, &v12, &stg6_0, &stg4_0);

  // 1, 14
  x1 = _mm_mulhrs_epi16(in[0], stk4_0);  // stp1[1], stk4_1 = stk4_0
  // stp1[2] = stp1[0], stp1[3] = stp1[1]
  x4 = _mm_mulhrs_epi16(in[4], stk3_0);  // stp1[4]
  butterfly(&x7, &x4, &stg4_1, &stg4_0, &x5, &x6);
  v1 = _mm_add_epi16(x1, x6);  // stp2_1
  v2 = _mm_add_epi16(x0, x5);  // stp2_2
  stp1[1] = _mm_add_epi16(v1, v14);
  stp1[14] = _mm_sub_epi16(v1, v14);

  stp1[2] = _mm_add_epi16(v2, v13);
  stp1[13] = _mm_sub_epi16(v2, v13);

  v3 = _mm_add_epi16(x1, x4);  // stp2_3
  v4 = _mm_sub_epi16(x1, x4);  // stp2_4

  v5 = _mm_sub_epi16(x0, x5);  // stp2_5

  v6 = _mm_sub_epi16(x1, x6);  // stp2_6
  v7 = _mm_sub_epi16(x0, x7);  // stp2_7
  stp1[3] = _mm_add_epi16(v3, v12);
  stp1[12] = _mm_sub_epi16(v3, v12);

  stp1[6] = _mm_add_epi16(v6, v9);
  stp1[9] = _mm_sub_epi16(v6, v9);

  stp1[7] = _mm_add_epi16(v7, v8);
  stp1[8] = _mm_sub_epi16(v7, v8);

  stp1[4] = _mm_add_epi16(v4, v11);
  stp1[11] = _mm_sub_epi16(v4, v11);

  stp1[5] = _mm_add_epi16(v5, v10);
  stp1[10] = _mm_sub_epi16(v5, v10);
}

static void idct32_34_second_half(const __m128i *in, __m128i *stp1) {
  const __m128i stk1_0 = pair_set_epi16(2 * cospi_31_64, 2 * cospi_31_64);
  const __m128i stk1_1 = pair_set_epi16(2 * cospi_1_64, 2 * cospi_1_64);
  const __m128i stk1_6 = pair_set_epi16(-2 * cospi_25_64, -2 * cospi_25_64);
  const __m128i stk1_7 = pair_set_epi16(2 * cospi_7_64, 2 * cospi_7_64);
  const __m128i stk1_8 = pair_set_epi16(2 * cospi_27_64, 2 * cospi_27_64);
  const __m128i stk1_9 = pair_set_epi16(2 * cospi_5_64, 2 * cospi_5_64);
  const __m128i stk1_14 = pair_set_epi16(-2 * cospi_29_64, -2 * cospi_29_64);
  const __m128i stk1_15 = pair_set_epi16(2 * cospi_3_64, 2 * cospi_3_64);
  const __m128i stg3_4 = pair_set_epi16(-cospi_4_64, cospi_28_64);
  const __m128i stg3_5 = pair_set_epi16(cospi_28_64, cospi_4_64);
  const __m128i stg3_6 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
  const __m128i stg3_8 = pair_set_epi16(-cospi_20_64, cospi_12_64);
  const __m128i stg3_9 = pair_set_epi16(cospi_12_64, cospi_20_64);
  const __m128i stg3_10 = pair_set_epi16(-cospi_12_64, -cospi_20_64);

  const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
  const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
  const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
  const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);

  const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
  __m128i v16, v17, v18, v19, v20, v21, v22, v23;
  __m128i v24, v25, v26, v27, v28, v29, v30, v31;
  __m128i u16, u17, u18, u19, u20, u21, u22, u23;
  __m128i u24, u25, u26, u27, u28, u29, u30, u31;

  v16 = _mm_mulhrs_epi16(in[1], stk1_0);
  v31 = _mm_mulhrs_epi16(in[1], stk1_1);

  v19 = _mm_mulhrs_epi16(in[7], stk1_6);
  v28 = _mm_mulhrs_epi16(in[7], stk1_7);

  v20 = _mm_mulhrs_epi16(in[5], stk1_8);
  v27 = _mm_mulhrs_epi16(in[5], stk1_9);

  v23 = _mm_mulhrs_epi16(in[3], stk1_14);
  v24 = _mm_mulhrs_epi16(in[3], stk1_15);

  butterfly(&v16, &v31, &stg3_4, &stg3_5, &v17, &v30);
  butterfly(&v19, &v28, &stg3_6, &stg3_4, &v18, &v29);
  butterfly(&v20, &v27, &stg3_8, &stg3_9, &v21, &v26);
  butterfly(&v23, &v24, &stg3_10, &stg3_8, &v22, &v25);

  u16 = _mm_add_epi16(v16, v19);
  u17 = _mm_add_epi16(v17, v18);
  u18 = _mm_sub_epi16(v17, v18);
  u19 = _mm_sub_epi16(v16, v19);
  u20 = _mm_sub_epi16(v23, v20);
  u21 = _mm_sub_epi16(v22, v21);
  u22 = _mm_add_epi16(v22, v21);
  u23 = _mm_add_epi16(v23, v20);
  u24 = _mm_add_epi16(v24, v27);
  u27 = _mm_sub_epi16(v24, v27);
  u25 = _mm_add_epi16(v25, v26);
  u26 = _mm_sub_epi16(v25, v26);
  u28 = _mm_sub_epi16(v31, v28);
  u31 = _mm_add_epi16(v28, v31);
  u29 = _mm_sub_epi16(v30, v29);
  u30 = _mm_add_epi16(v29, v30);

  butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
  butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
  butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
  butterfly_self(&u21, &u26, &stg4_6, &stg4_4);

  stp1[16] = _mm_add_epi16(u16, u23);
  stp1[23] = _mm_sub_epi16(u16, u23);

  stp1[17] = _mm_add_epi16(u17, u22);
  stp1[22] = _mm_sub_epi16(u17, u22);

  stp1[18] = _mm_add_epi16(u18, u21);
  stp1[21] = _mm_sub_epi16(u18, u21);

  stp1[19] = _mm_add_epi16(u19, u20);
  stp1[20] = _mm_sub_epi16(u19, u20);

  stp1[24] = _mm_sub_epi16(u31, u24);
  stp1[31] = _mm_add_epi16(u24, u31);

  stp1[25] = _mm_sub_epi16(u30, u25);
  stp1[30] = _mm_add_epi16(u25, u30);

  stp1[26] = _mm_sub_epi16(u29, u26);
  stp1[29] = _mm_add_epi16(u26, u29);

  stp1[27] = _mm_sub_epi16(u28, u27);
  stp1[28] = _mm_add_epi16(u27, u28);

  butterfly_self(&stp1[20], &stp1[27], &stg6_0, &stg4_0);
  butterfly_self(&stp1[21], &stp1[26], &stg6_0, &stg4_0);
  butterfly_self(&stp1[22], &stp1[25], &stg6_0, &stg4_0);
  butterfly_self(&stp1[23], &stp1[24], &stg6_0, &stg4_0);
}

// Only upper-left 8x8 has non-zero coeff
void aom_idct32x32_34_add_ssse3(const tran_low_t *input, uint8_t *dest,
                                int stride) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i final_rounding = _mm_set1_epi16(1 << 5);
  __m128i in[32], col[32];
  __m128i stp1[32];
  int i;

  // Load input data. Only need to load the top left 8x8 block.
  in[0] = load_input_data(input);
  in[1] = load_input_data(input + 32);
  in[2] = load_input_data(input + 64);
  in[3] = load_input_data(input + 96);
  in[4] = load_input_data(input + 128);
  in[5] = load_input_data(input + 160);
  in[6] = load_input_data(input + 192);
  in[7] = load_input_data(input + 224);

  array_transpose_8x8(in, in);
  idct32_34_first_half(in, stp1);
  idct32_34_second_half(in, stp1);

  // 1_D: Store 32 intermediate results for each 8x32 block.
  add_sub_butterfly(stp1, col, 32);
  for (i = 0; i < 4; i++) {
    int j;
    // Transpose 32x8 block to 8x32 block
    array_transpose_8x8(col + i * 8, in);
    idct32_34_first_half(in, stp1);
    idct32_34_second_half(in, stp1);

    // 2_D: Calculate the results and store them to destination.
    add_sub_butterfly(stp1, in, 32);
    for (j = 0; j < 32; ++j) {
      // Final rounding and shift
      in[j] = _mm_adds_epi16(in[j], final_rounding);
      in[j] = _mm_srai_epi16(in[j], 6);
      RECON_AND_STORE(dest + j * stride, in[j]);
    }

    dest += 8;
  }
}

// in0[16] represents the left 8x16 block
// in1[16] represents the right 8x16 block
static void load_buffer_16x16(const tran_low_t *input, __m128i *in0,
                              __m128i *in1) {
  int i;
  for (i = 0; i < 16; i++) {
    in0[i] = load_input_data(input);
    in1[i] = load_input_data(input + 8);
    input += 32;
  }
}

static void array_transpose_16x16_2(__m128i *in0, __m128i *in1, __m128i *out0,
                                    __m128i *out1) {
  array_transpose_8x8(in0, out0);
  array_transpose_8x8(&in0[8], out1);
  array_transpose_8x8(in1, &out0[8]);
  array_transpose_8x8(&in1[8], &out1[8]);
}

// Group the coefficient calculation into smaller functions
// to prevent stack spillover:
// quarter_1: 0-7
// quarter_2: 8-15
// quarter_3_4: 16-23, 24-31
static void idct32_8x32_135_quarter_1(const __m128i *in /*in[16]*/,
                                      __m128i *out /*out[8]*/) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;

  {
    const __m128i stk4_0 = pair_set_epi16(2 * cospi_16_64, 2 * cospi_16_64);
    const __m128i stk4_2 = pair_set_epi16(2 * cospi_24_64, 2 * cospi_24_64);
    const __m128i stk4_3 = pair_set_epi16(2 * cospi_8_64, 2 * cospi_8_64);
    u0 = _mm_mulhrs_epi16(in[0], stk4_0);
    u2 = _mm_mulhrs_epi16(in[8], stk4_2);
    u3 = _mm_mulhrs_epi16(in[8], stk4_3);
    u1 = u0;
  }

  v0 = _mm_add_epi16(u0, u3);
  v1 = _mm_add_epi16(u1, u2);
  v2 = _mm_sub_epi16(u1, u2);
  v3 = _mm_sub_epi16(u0, u3);

  {
    const __m128i stk3_0 = pair_set_epi16(2 * cospi_28_64, 2 * cospi_28_64);
    const __m128i stk3_1 = pair_set_epi16(2 * cospi_4_64, 2 * cospi_4_64);
    const __m128i stk3_2 = pair_set_epi16(-2 * cospi_20_64, -2 * cospi_20_64);
    const __m128i stk3_3 = pair_set_epi16(2 * cospi_12_64, 2 * cospi_12_64);
    u4 = _mm_mulhrs_epi16(in[4], stk3_0);
    u7 = _mm_mulhrs_epi16(in[4], stk3_1);
    u5 = _mm_mulhrs_epi16(in[12], stk3_2);
    u6 = _mm_mulhrs_epi16(in[12], stk3_3);
  }

  v4 = _mm_add_epi16(u4, u5);
  v5 = _mm_sub_epi16(u4, u5);
  v6 = _mm_sub_epi16(u7, u6);
  v7 = _mm_add_epi16(u7, u6);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg4_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
    butterfly(&v6, &v5, &stg4_1, &stg4_0, &v5, &v6);
  }

  out[0] = _mm_add_epi16(v0, v7);
  out[1] = _mm_add_epi16(v1, v6);
  out[2] = _mm_add_epi16(v2, v5);
  out[3] = _mm_add_epi16(v3, v4);
  out[4] = _mm_sub_epi16(v3, v4);
  out[5] = _mm_sub_epi16(v2, v5);
  out[6] = _mm_sub_epi16(v1, v6);
  out[7] = _mm_sub_epi16(v0, v7);
}

static void idct32_8x32_135_quarter_2(const __m128i *in /*in[16]*/,
                                      __m128i *out /*out[8]*/) {
  __m128i u8, u9, u10, u11, u12, u13, u14, u15;
  __m128i v8, v9, v10, v11, v12, v13, v14, v15;

  {
    const __m128i stk2_0 = pair_set_epi16(2 * cospi_30_64, 2 * cospi_30_64);
    const __m128i stk2_1 = pair_set_epi16(2 * cospi_2_64, 2 * cospi_2_64);
    const __m128i stk2_2 = pair_set_epi16(-2 * cospi_18_64, -2 * cospi_18_64);
    const __m128i stk2_3 = pair_set_epi16(2 * cospi_14_64, 2 * cospi_14_64);
    const __m128i stk2_4 = pair_set_epi16(2 * cospi_22_64, 2 * cospi_22_64);
    const __m128i stk2_5 = pair_set_epi16(2 * cospi_10_64, 2 * cospi_10_64);
    const __m128i stk2_6 = pair_set_epi16(-2 * cospi_26_64, -2 * cospi_26_64);
    const __m128i stk2_7 = pair_set_epi16(2 * cospi_6_64, 2 * cospi_6_64);
    u8 = _mm_mulhrs_epi16(in[2], stk2_0);
    u15 = _mm_mulhrs_epi16(in[2], stk2_1);
    u9 = _mm_mulhrs_epi16(in[14], stk2_2);
    u14 = _mm_mulhrs_epi16(in[14], stk2_3);
    u10 = _mm_mulhrs_epi16(in[10], stk2_4);
    u13 = _mm_mulhrs_epi16(in[10], stk2_5);
    u11 = _mm_mulhrs_epi16(in[6], stk2_6);
    u12 = _mm_mulhrs_epi16(in[6], stk2_7);
  }

  v8 = _mm_add_epi16(u8, u9);
  v9 = _mm_sub_epi16(u8, u9);
  v10 = _mm_sub_epi16(u11, u10);
  v11 = _mm_add_epi16(u11, u10);
  v12 = _mm_add_epi16(u12, u13);
  v13 = _mm_sub_epi16(u12, u13);
  v14 = _mm_sub_epi16(u15, u14);
  v15 = _mm_add_epi16(u15, u14);

  {
    const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
    const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
    const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&v9, &v14, &stg4_4, &stg4_5);
    butterfly_self(&v10, &v13, &stg4_6, &stg4_4);
  }

  out[0] = _mm_add_epi16(v8, v11);
  out[1] = _mm_add_epi16(v9, v10);
  out[2] = _mm_sub_epi16(v9, v10);
  out[3] = _mm_sub_epi16(v8, v11);
  out[4] = _mm_sub_epi16(v15, v12);
  out[5] = _mm_sub_epi16(v14, v13);
  out[6] = _mm_add_epi16(v14, v13);
  out[7] = _mm_add_epi16(v15, v12);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[2], &out[5], &stg6_0, &stg4_0);
    butterfly_self(&out[3], &out[4], &stg6_0, &stg4_0);
  }
}

// 8x32 block even indexed 8 inputs of in[16],
// output first half 16 to out[32]
static void idct32_8x32_quarter_1_2(const __m128i *in /*in[16]*/,
                                    __m128i *out /*out[32]*/) {
  __m128i temp[16];
  idct32_8x32_135_quarter_1(in, temp);
  idct32_8x32_135_quarter_2(in, &temp[8]);
  add_sub_butterfly(temp, out, 16);
}

// 8x32 block odd indexed 8 inputs of in[16],
// output second half 16 to out[32]
static void idct32_8x32_quarter_3_4(const __m128i *in /*in[16]*/,
                                    __m128i *out /*out[32]*/) {
  __m128i v16, v17, v18, v19, v20, v21, v22, v23;
  __m128i v24, v25, v26, v27, v28, v29, v30, v31;
  __m128i u16, u17, u18, u19, u20, u21, u22, u23;
  __m128i u24, u25, u26, u27, u28, u29, u30, u31;

  {
    const __m128i stk1_0 = pair_set_epi16(2 * cospi_31_64, 2 * cospi_31_64);
    const __m128i stk1_1 = pair_set_epi16(2 * cospi_1_64, 2 * cospi_1_64);
    const __m128i stk1_2 = pair_set_epi16(-2 * cospi_17_64, -2 * cospi_17_64);
    const __m128i stk1_3 = pair_set_epi16(2 * cospi_15_64, 2 * cospi_15_64);

    const __m128i stk1_4 = pair_set_epi16(2 * cospi_23_64, 2 * cospi_23_64);
    const __m128i stk1_5 = pair_set_epi16(2 * cospi_9_64, 2 * cospi_9_64);
    const __m128i stk1_6 = pair_set_epi16(-2 * cospi_25_64, -2 * cospi_25_64);
    const __m128i stk1_7 = pair_set_epi16(2 * cospi_7_64, 2 * cospi_7_64);
    const __m128i stk1_8 = pair_set_epi16(2 * cospi_27_64, 2 * cospi_27_64);
    const __m128i stk1_9 = pair_set_epi16(2 * cospi_5_64, 2 * cospi_5_64);
    const __m128i stk1_10 = pair_set_epi16(-2 * cospi_21_64, -2 * cospi_21_64);
    const __m128i stk1_11 = pair_set_epi16(2 * cospi_11_64, 2 * cospi_11_64);

    const __m128i stk1_12 = pair_set_epi16(2 * cospi_19_64, 2 * cospi_19_64);
    const __m128i stk1_13 = pair_set_epi16(2 * cospi_13_64, 2 * cospi_13_64);
    const __m128i stk1_14 = pair_set_epi16(-2 * cospi_29_64, -2 * cospi_29_64);
    const __m128i stk1_15 = pair_set_epi16(2 * cospi_3_64, 2 * cospi_3_64);
    u16 = _mm_mulhrs_epi16(in[1], stk1_0);
    u31 = _mm_mulhrs_epi16(in[1], stk1_1);
    u17 = _mm_mulhrs_epi16(in[15], stk1_2);
    u30 = _mm_mulhrs_epi16(in[15], stk1_3);

    u18 = _mm_mulhrs_epi16(in[9], stk1_4);
    u29 = _mm_mulhrs_epi16(in[9], stk1_5);
    u19 = _mm_mulhrs_epi16(in[7], stk1_6);
    u28 = _mm_mulhrs_epi16(in[7], stk1_7);

    u20 = _mm_mulhrs_epi16(in[5], stk1_8);
    u27 = _mm_mulhrs_epi16(in[5], stk1_9);
    u21 = _mm_mulhrs_epi16(in[11], stk1_10);
    u26 = _mm_mulhrs_epi16(in[11], stk1_11);

    u22 = _mm_mulhrs_epi16(in[13], stk1_12);
    u25 = _mm_mulhrs_epi16(in[13], stk1_13);
    u23 = _mm_mulhrs_epi16(in[3], stk1_14);
    u24 = _mm_mulhrs_epi16(in[3], stk1_15);
  }

  v16 = _mm_add_epi16(u16, u17);
  v17 = _mm_sub_epi16(u16, u17);
  v18 = _mm_sub_epi16(u19, u18);
  v19 = _mm_add_epi16(u19, u18);

  v20 = _mm_add_epi16(u20, u21);
  v21 = _mm_sub_epi16(u20, u21);
  v22 = _mm_sub_epi16(u23, u22);
  v23 = _mm_add_epi16(u23, u22);

  v24 = _mm_add_epi16(u24, u25);
  v25 = _mm_sub_epi16(u24, u25);
  v26 = _mm_sub_epi16(u27, u26);
  v27 = _mm_add_epi16(u27, u26);

  v28 = _mm_add_epi16(u28, u29);
  v29 = _mm_sub_epi16(u28, u29);
  v30 = _mm_sub_epi16(u31, u30);
  v31 = _mm_add_epi16(u31, u30);

  {
    const __m128i stg3_4 = pair_set_epi16(-cospi_4_64, cospi_28_64);
    const __m128i stg3_5 = pair_set_epi16(cospi_28_64, cospi_4_64);
    const __m128i stg3_6 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
    const __m128i stg3_8 = pair_set_epi16(-cospi_20_64, cospi_12_64);
    const __m128i stg3_9 = pair_set_epi16(cospi_12_64, cospi_20_64);
    const __m128i stg3_10 = pair_set_epi16(-cospi_12_64, -cospi_20_64);

    butterfly_self(&v17, &v30, &stg3_4, &stg3_5);
    butterfly_self(&v18, &v29, &stg3_6, &stg3_4);
    butterfly_self(&v21, &v26, &stg3_8, &stg3_9);
    butterfly_self(&v22, &v25, &stg3_10, &stg3_8);
  }

  u16 = _mm_add_epi16(v16, v19);
  u17 = _mm_add_epi16(v17, v18);
  u18 = _mm_sub_epi16(v17, v18);
  u19 = _mm_sub_epi16(v16, v19);
  u20 = _mm_sub_epi16(v23, v20);
  u21 = _mm_sub_epi16(v22, v21);
  u22 = _mm_add_epi16(v22, v21);
  u23 = _mm_add_epi16(v23, v20);

  u24 = _mm_add_epi16(v24, v27);
  u25 = _mm_add_epi16(v25, v26);
  u26 = _mm_sub_epi16(v25, v26);
  u27 = _mm_sub_epi16(v24, v27);
  u28 = _mm_sub_epi16(v31, v28);
  u29 = _mm_sub_epi16(v30, v29);
  u30 = _mm_add_epi16(v29, v30);
  u31 = _mm_add_epi16(v28, v31);

  {
    const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
    const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
    const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
    butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
    butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
    butterfly_self(&u21, &u26, &stg4_6, &stg4_4);
  }

  out[0] = _mm_add_epi16(u16, u23);
  out[1] = _mm_add_epi16(u17, u22);
  out[2] = _mm_add_epi16(u18, u21);
  out[3] = _mm_add_epi16(u19, u20);
  v20 = _mm_sub_epi16(u19, u20);
  v21 = _mm_sub_epi16(u18, u21);
  v22 = _mm_sub_epi16(u17, u22);
  v23 = _mm_sub_epi16(u16, u23);

  v24 = _mm_sub_epi16(u31, u24);
  v25 = _mm_sub_epi16(u30, u25);
  v26 = _mm_sub_epi16(u29, u26);
  v27 = _mm_sub_epi16(u28, u27);
  out[12] = _mm_add_epi16(u27, u28);
  out[13] = _mm_add_epi16(u26, u29);
  out[14] = _mm_add_epi16(u25, u30);
  out[15] = _mm_add_epi16(u24, u31);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly(&v20, &v27, &stg6_0, &stg4_0, &out[4], &out[11]);
    butterfly(&v21, &v26, &stg6_0, &stg4_0, &out[5], &out[10]);
    butterfly(&v22, &v25, &stg6_0, &stg4_0, &out[6], &out[9]);
    butterfly(&v23, &v24, &stg6_0, &stg4_0, &out[7], &out[8]);
  }
}

// 8x16 block, input __m128i in[16], output __m128i in[32]
static void idct32_8x32_135(__m128i *in /*in[32]*/) {
  __m128i out[32];
  idct32_8x32_quarter_1_2(in, out);
  idct32_8x32_quarter_3_4(in, &out[16]);
  add_sub_butterfly(out, in, 32);
}

static INLINE void store_buffer_8x32(__m128i *in, uint8_t *dst, int stride) {
  const __m128i final_rounding = _mm_set1_epi16(1 << 5);
  const __m128i zero = _mm_setzero_si128();
  int j = 0;
  while (j < 32) {
    in[j] = _mm_adds_epi16(in[j], final_rounding);
    in[j + 1] = _mm_adds_epi16(in[j + 1], final_rounding);

    in[j] = _mm_srai_epi16(in[j], 6);
    in[j + 1] = _mm_srai_epi16(in[j + 1], 6);

    RECON_AND_STORE(dst, in[j]);
    dst += stride;
    RECON_AND_STORE(dst, in[j + 1]);
    dst += stride;
    j += 2;
  }
}

static INLINE void recon_and_store(__m128i *in0, __m128i *in1, uint8_t *dest,
                                   int stride) {
  store_buffer_8x32(in0, dest, stride);
  store_buffer_8x32(in1, dest + 8, stride);
}

static INLINE void idct32_135(__m128i *col0, __m128i *col1) {
  idct32_8x32_135(col0);
  idct32_8x32_135(col1);
}

typedef enum { left_16, right_16 } ColsIndicator;

static void transpose_and_copy_16x16(__m128i *in0, __m128i *in1, __m128i *store,
                                     ColsIndicator cols) {
  switch (cols) {
    case left_16: {
      int i;
      array_transpose_16x16(in0, in1);
      for (i = 0; i < 16; ++i) {
        store[i] = in0[16 + i];
        store[16 + i] = in1[16 + i];
      }
      break;
    }
    case right_16: {
      array_transpose_16x16_2(store, &store[16], in0, in1);
      break;
    }
    default: { assert(0); }
  }
}

// Only upper-left 16x16 has non-zero coeff
void aom_idct32x32_135_add_ssse3(const tran_low_t *input, uint8_t *dest,
                                 int stride) {
  // Each array represents an 8x32 block
  __m128i col0[32], col1[32];
  // This array represents a 16x16 block
  __m128i temp[32];

  // Load input data. Only need to load the top left 16x16 block.
  load_buffer_16x16(input, col0, col1);

  // columns
  array_transpose_16x16(col0, col1);
  idct32_135(col0, col1);

  // rows
  transpose_and_copy_16x16(col0, col1, temp, left_16);
  idct32_135(col0, col1);
  recon_and_store(col0, col1, dest, stride);

  transpose_and_copy_16x16(col0, col1, temp, right_16);
  idct32_135(col0, col1);
  recon_and_store(col0, col1, dest + 16, stride);
}

// For each 8x32 block __m128i in[32],
// Input with index, 2, 6, 10, 14, 18, 22, 26, 30
// output pixels: 8-15 in __m128i in[32]
static void idct32_full_8x32_quarter_2(const __m128i *in /*in[32]*/,
                                       __m128i *out /*out[16]*/) {
  __m128i u8, u9, u10, u11, u12, u13, u14, u15;  // stp2_
  __m128i v8, v9, v10, v11, v12, v13, v14, v15;  // stp1_

  {
    const __m128i stg2_0 = pair_set_epi16(cospi_30_64, -cospi_2_64);
    const __m128i stg2_1 = pair_set_epi16(cospi_2_64, cospi_30_64);
    const __m128i stg2_2 = pair_set_epi16(cospi_14_64, -cospi_18_64);
    const __m128i stg2_3 = pair_set_epi16(cospi_18_64, cospi_14_64);
    butterfly(&in[2], &in[30], &stg2_0, &stg2_1, &u8, &u15);
    butterfly(&in[18], &in[14], &stg2_2, &stg2_3, &u9, &u14);
  }

  v8 = _mm_add_epi16(u8, u9);
  v9 = _mm_sub_epi16(u8, u9);
  v14 = _mm_sub_epi16(u15, u14);
  v15 = _mm_add_epi16(u15, u14);

  {
    const __m128i stg2_4 = pair_set_epi16(cospi_22_64, -cospi_10_64);
    const __m128i stg2_5 = pair_set_epi16(cospi_10_64, cospi_22_64);
    const __m128i stg2_6 = pair_set_epi16(cospi_6_64, -cospi_26_64);
    const __m128i stg2_7 = pair_set_epi16(cospi_26_64, cospi_6_64);
    butterfly(&in[10], &in[22], &stg2_4, &stg2_5, &u10, &u13);
    butterfly(&in[26], &in[6], &stg2_6, &stg2_7, &u11, &u12);
  }

  v10 = _mm_sub_epi16(u11, u10);
  v11 = _mm_add_epi16(u11, u10);
  v12 = _mm_add_epi16(u12, u13);
  v13 = _mm_sub_epi16(u12, u13);

  {
    const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
    const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
    const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&v9, &v14, &stg4_4, &stg4_5);
    butterfly_self(&v10, &v13, &stg4_6, &stg4_4);
  }

  out[0] = _mm_add_epi16(v8, v11);
  out[1] = _mm_add_epi16(v9, v10);
  out[6] = _mm_add_epi16(v14, v13);
  out[7] = _mm_add_epi16(v15, v12);

  out[2] = _mm_sub_epi16(v9, v10);
  out[3] = _mm_sub_epi16(v8, v11);
  out[4] = _mm_sub_epi16(v15, v12);
  out[5] = _mm_sub_epi16(v14, v13);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[2], &out[5], &stg6_0, &stg4_0);
    butterfly_self(&out[3], &out[4], &stg6_0, &stg4_0);
  }
}

// For each 8x32 block __m128i in[32],
// Input with index, 0, 4, 8, 12, 16, 20, 24, 28
// output pixels: 0-7 in __m128i in[32]
static void idct32_full_8x32_quarter_1(const __m128i *in /*in[32]*/,
                                       __m128i *out /*out[8]*/) {
  __m128i u0, u1, u2, u3, u4, u5, u6, u7;  // stp1_
  __m128i v0, v1, v2, v3, v4, v5, v6, v7;  // stp2_

  {
    const __m128i stg3_0 = pair_set_epi16(cospi_28_64, -cospi_4_64);
    const __m128i stg3_1 = pair_set_epi16(cospi_4_64, cospi_28_64);
    const __m128i stg3_2 = pair_set_epi16(cospi_12_64, -cospi_20_64);
    const __m128i stg3_3 = pair_set_epi16(cospi_20_64, cospi_12_64);
    butterfly(&in[4], &in[28], &stg3_0, &stg3_1, &u4, &u7);
    butterfly(&in[20], &in[12], &stg3_2, &stg3_3, &u5, &u6);
  }

  v4 = _mm_add_epi16(u4, u5);
  v5 = _mm_sub_epi16(u4, u5);
  v6 = _mm_sub_epi16(u7, u6);
  v7 = _mm_add_epi16(u7, u6);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg4_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
    const __m128i stg4_2 = pair_set_epi16(cospi_24_64, -cospi_8_64);
    const __m128i stg4_3 = pair_set_epi16(cospi_8_64, cospi_24_64);
    butterfly(&v6, &v5, &stg4_1, &stg4_0, &v5, &v6);

    butterfly(&in[0], &in[16], &stg4_0, &stg4_1, &u0, &u1);
    butterfly(&in[8], &in[24], &stg4_2, &stg4_3, &u2, &u3);
  }

  v0 = _mm_add_epi16(u0, u3);
  v1 = _mm_add_epi16(u1, u2);
  v2 = _mm_sub_epi16(u1, u2);
  v3 = _mm_sub_epi16(u0, u3);

  out[0] = _mm_add_epi16(v0, v7);
  out[1] = _mm_add_epi16(v1, v6);
  out[2] = _mm_add_epi16(v2, v5);
  out[3] = _mm_add_epi16(v3, v4);
  out[4] = _mm_sub_epi16(v3, v4);
  out[5] = _mm_sub_epi16(v2, v5);
  out[6] = _mm_sub_epi16(v1, v6);
  out[7] = _mm_sub_epi16(v0, v7);
}

// For each 8x32 block __m128i in[32],
// Input with odd index,
// 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
// output pixels: 16-23, 24-31 in __m128i in[32]
// We avoid hide an offset, 16, inside this function. So we output 0-15 into
// array out[16]
static void idct32_full_8x32_quarter_3_4(const __m128i *in /*in[32]*/,
                                         __m128i *out /*out[16]*/) {
  __m128i v16, v17, v18, v19, v20, v21, v22, v23;
  __m128i v24, v25, v26, v27, v28, v29, v30, v31;
  __m128i u16, u17, u18, u19, u20, u21, u22, u23;
  __m128i u24, u25, u26, u27, u28, u29, u30, u31;

  {
    const __m128i stg1_0 = pair_set_epi16(cospi_31_64, -cospi_1_64);
    const __m128i stg1_1 = pair_set_epi16(cospi_1_64, cospi_31_64);
    const __m128i stg1_2 = pair_set_epi16(cospi_15_64, -cospi_17_64);
    const __m128i stg1_3 = pair_set_epi16(cospi_17_64, cospi_15_64);
    const __m128i stg1_4 = pair_set_epi16(cospi_23_64, -cospi_9_64);
    const __m128i stg1_5 = pair_set_epi16(cospi_9_64, cospi_23_64);
    const __m128i stg1_6 = pair_set_epi16(cospi_7_64, -cospi_25_64);
    const __m128i stg1_7 = pair_set_epi16(cospi_25_64, cospi_7_64);
    const __m128i stg1_8 = pair_set_epi16(cospi_27_64, -cospi_5_64);
    const __m128i stg1_9 = pair_set_epi16(cospi_5_64, cospi_27_64);
    const __m128i stg1_10 = pair_set_epi16(cospi_11_64, -cospi_21_64);
    const __m128i stg1_11 = pair_set_epi16(cospi_21_64, cospi_11_64);
    const __m128i stg1_12 = pair_set_epi16(cospi_19_64, -cospi_13_64);
    const __m128i stg1_13 = pair_set_epi16(cospi_13_64, cospi_19_64);
    const __m128i stg1_14 = pair_set_epi16(cospi_3_64, -cospi_29_64);
    const __m128i stg1_15 = pair_set_epi16(cospi_29_64, cospi_3_64);
    butterfly(&in[1], &in[31], &stg1_0, &stg1_1, &u16, &u31);
    butterfly(&in[17], &in[15], &stg1_2, &stg1_3, &u17, &u30);
    butterfly(&in[9], &in[23], &stg1_4, &stg1_5, &u18, &u29);
    butterfly(&in[25], &in[7], &stg1_6, &stg1_7, &u19, &u28);

    butterfly(&in[5], &in[27], &stg1_8, &stg1_9, &u20, &u27);
    butterfly(&in[21], &in[11], &stg1_10, &stg1_11, &u21, &u26);

    butterfly(&in[13], &in[19], &stg1_12, &stg1_13, &u22, &u25);
    butterfly(&in[29], &in[3], &stg1_14, &stg1_15, &u23, &u24);
  }

  v16 = _mm_add_epi16(u16, u17);
  v17 = _mm_sub_epi16(u16, u17);
  v18 = _mm_sub_epi16(u19, u18);
  v19 = _mm_add_epi16(u19, u18);

  v20 = _mm_add_epi16(u20, u21);
  v21 = _mm_sub_epi16(u20, u21);
  v22 = _mm_sub_epi16(u23, u22);
  v23 = _mm_add_epi16(u23, u22);

  v24 = _mm_add_epi16(u24, u25);
  v25 = _mm_sub_epi16(u24, u25);
  v26 = _mm_sub_epi16(u27, u26);
  v27 = _mm_add_epi16(u27, u26);

  v28 = _mm_add_epi16(u28, u29);
  v29 = _mm_sub_epi16(u28, u29);
  v30 = _mm_sub_epi16(u31, u30);
  v31 = _mm_add_epi16(u31, u30);

  {
    const __m128i stg3_4 = pair_set_epi16(-cospi_4_64, cospi_28_64);
    const __m128i stg3_5 = pair_set_epi16(cospi_28_64, cospi_4_64);
    const __m128i stg3_6 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
    const __m128i stg3_8 = pair_set_epi16(-cospi_20_64, cospi_12_64);
    const __m128i stg3_9 = pair_set_epi16(cospi_12_64, cospi_20_64);
    const __m128i stg3_10 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
    butterfly_self(&v17, &v30, &stg3_4, &stg3_5);
    butterfly_self(&v18, &v29, &stg3_6, &stg3_4);
    butterfly_self(&v21, &v26, &stg3_8, &stg3_9);
    butterfly_self(&v22, &v25, &stg3_10, &stg3_8);
  }

  u16 = _mm_add_epi16(v16, v19);
  u17 = _mm_add_epi16(v17, v18);
  u18 = _mm_sub_epi16(v17, v18);
  u19 = _mm_sub_epi16(v16, v19);
  u20 = _mm_sub_epi16(v23, v20);
  u21 = _mm_sub_epi16(v22, v21);
  u22 = _mm_add_epi16(v22, v21);
  u23 = _mm_add_epi16(v23, v20);

  u24 = _mm_add_epi16(v24, v27);
  u25 = _mm_add_epi16(v25, v26);
  u26 = _mm_sub_epi16(v25, v26);
  u27 = _mm_sub_epi16(v24, v27);

  u28 = _mm_sub_epi16(v31, v28);
  u29 = _mm_sub_epi16(v30, v29);
  u30 = _mm_add_epi16(v29, v30);
  u31 = _mm_add_epi16(v28, v31);

  {
    const __m128i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
    const __m128i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
    const __m128i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
    butterfly_self(&u18, &u29, &stg4_4, &stg4_5);
    butterfly_self(&u19, &u28, &stg4_4, &stg4_5);
    butterfly_self(&u20, &u27, &stg4_6, &stg4_4);
    butterfly_self(&u21, &u26, &stg4_6, &stg4_4);
  }

  out[0] = _mm_add_epi16(u16, u23);
  out[1] = _mm_add_epi16(u17, u22);
  out[2] = _mm_add_epi16(u18, u21);
  out[3] = _mm_add_epi16(u19, u20);
  out[4] = _mm_sub_epi16(u19, u20);
  out[5] = _mm_sub_epi16(u18, u21);
  out[6] = _mm_sub_epi16(u17, u22);
  out[7] = _mm_sub_epi16(u16, u23);

  out[8] = _mm_sub_epi16(u31, u24);
  out[9] = _mm_sub_epi16(u30, u25);
  out[10] = _mm_sub_epi16(u29, u26);
  out[11] = _mm_sub_epi16(u28, u27);
  out[12] = _mm_add_epi16(u27, u28);
  out[13] = _mm_add_epi16(u26, u29);
  out[14] = _mm_add_epi16(u25, u30);
  out[15] = _mm_add_epi16(u24, u31);

  {
    const __m128i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
    const __m128i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
    butterfly_self(&out[4], &out[11], &stg6_0, &stg4_0);
    butterfly_self(&out[5], &out[10], &stg6_0, &stg4_0);
    butterfly_self(&out[6], &out[9], &stg6_0, &stg4_0);
    butterfly_self(&out[7], &out[8], &stg6_0, &stg4_0);
  }
}

static void idct32_full_8x32_quarter_1_2(const __m128i *in /*in[32]*/,
                                         __m128i *out /*out[32]*/) {
  __m128i temp[16];
  idct32_full_8x32_quarter_1(in, temp);
  idct32_full_8x32_quarter_2(in, &temp[8]);
  add_sub_butterfly(temp, out, 16);
}

static void idct32_full_8x32(const __m128i *in /*in[32]*/,
                             __m128i *out /*out[32]*/) {
  __m128i temp[32];
  idct32_full_8x32_quarter_1_2(in, temp);
  idct32_full_8x32_quarter_3_4(in, &temp[16]);
  add_sub_butterfly(temp, out, 32);
}

static void load_buffer_8x32(const tran_low_t *input, __m128i *in) {
  int i;
  for (i = 0; i < 8; ++i) {
    in[i] = load_input_data(input);
    in[i + 8] = load_input_data(input + 8);
    in[i + 16] = load_input_data(input + 16);
    in[i + 24] = load_input_data(input + 24);
    input += 32;
  }
}

void aom_idct32x32_1024_add_ssse3(const tran_low_t *input, uint8_t *dest,
                                  int stride) {
  __m128i col[128], in[32];
  int i, j;

  // rows
  for (i = 0; i < 4; ++i) {
    load_buffer_8x32(input, in);
    input += 32 << 3;

    // Transpose 32x8 block to 8x32 block
    array_transpose_8x8(in, in);
    array_transpose_8x8(in + 8, in + 8);
    array_transpose_8x8(in + 16, in + 16);
    array_transpose_8x8(in + 24, in + 24);

    idct32_full_8x32(in, col + (i << 5));
  }

  // columns
  for (i = 0; i < 4; ++i) {
    j = i << 3;
    // Transpose 32x8 block to 8x32 block
    array_transpose_8x8(col + j, in);
    array_transpose_8x8(col + j + 32, in + 8);
    array_transpose_8x8(col + j + 64, in + 16);
    array_transpose_8x8(col + j + 96, in + 24);

    idct32_full_8x32(in, in);
    store_buffer_8x32(in, dest, stride);
    dest += 8;
  }
}
