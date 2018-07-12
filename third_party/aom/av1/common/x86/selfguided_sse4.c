#include <smmintrin.h>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/restoration.h"
#include "aom_dsp/x86/synonyms.h"

/* Calculate four consecutive entries of the intermediate A and B arrays
   (corresponding to the first loop in the C version of
   av1_selfguided_restoration)
*/
static void calc_block(__m128i sum, __m128i sum_sq, __m128i n,
                       __m128i *one_over_n_, __m128i *s_, int bit_depth,
                       int idx, int32_t *A, int32_t *B) {
  __m128i a, b, p;
  __m128i one_over_n = *one_over_n_;
  __m128i s = *s_;
#if CONFIG_HIGHBITDEPTH
  if (bit_depth > 8) {
    __m128i rounding_a = _mm_set1_epi32((1 << (2 * (bit_depth - 8))) >> 1);
    __m128i rounding_b = _mm_set1_epi32((1 << (bit_depth - 8)) >> 1);
    __m128i shift_a = _mm_cvtsi32_si128(2 * (bit_depth - 8));
    __m128i shift_b = _mm_cvtsi32_si128(bit_depth - 8);
    a = _mm_srl_epi32(_mm_add_epi32(sum_sq, rounding_a), shift_a);
    b = _mm_srl_epi32(_mm_add_epi32(sum, rounding_b), shift_b);
    a = _mm_mullo_epi32(a, n);
    b = _mm_mullo_epi32(b, b);
    p = _mm_sub_epi32(_mm_max_epi32(a, b), b);
  } else {
#endif
    (void)bit_depth;
    a = _mm_mullo_epi32(sum_sq, n);
    b = _mm_mullo_epi32(sum, sum);
    p = _mm_sub_epi32(a, b);
#if CONFIG_HIGHBITDEPTH
  }
#endif

  __m128i rounding_z = _mm_set1_epi32((1 << SGRPROJ_MTABLE_BITS) >> 1);
  __m128i z = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p, s), rounding_z),
                             SGRPROJ_MTABLE_BITS);
  z = _mm_min_epi32(z, _mm_set1_epi32(255));

  // 'Gather' type instructions are not available pre-AVX2, so synthesize a
  // gather using scalar loads.
  __m128i a_res = _mm_set_epi32(x_by_xplus1[_mm_extract_epi32(z, 3)],
                                x_by_xplus1[_mm_extract_epi32(z, 2)],
                                x_by_xplus1[_mm_extract_epi32(z, 1)],
                                x_by_xplus1[_mm_extract_epi32(z, 0)]);

  _mm_storeu_si128((__m128i *)&A[idx], a_res);

  __m128i rounding_res = _mm_set1_epi32((1 << SGRPROJ_RECIP_BITS) >> 1);
  __m128i a_complement = _mm_sub_epi32(_mm_set1_epi32(SGRPROJ_SGR), a_res);
  __m128i b_int =
      _mm_mullo_epi32(a_complement, _mm_mullo_epi32(sum, one_over_n));
  __m128i b_res =
      _mm_srli_epi32(_mm_add_epi32(b_int, rounding_res), SGRPROJ_RECIP_BITS);

  _mm_storeu_si128((__m128i *)&B[idx], b_res);
}

static void selfguided_restoration_1_v(uint8_t *src, int width, int height,
                                       int src_stride, int32_t *A, int32_t *B,
                                       int buf_stride) {
  int i, j;

  // Vertical sum
  // When the width is not a multiple of 4, we know that 'stride' is rounded up
  // to a multiple of 4. So it is safe for this loop to calculate extra columns
  // at the right-hand edge of the frame.
  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, x, y, x2, y2;
    __m128i sum, sum_sq, tmp;

    a = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[j]));
    b = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[src_stride + j]));

    sum = _mm_cvtepi16_epi32(_mm_add_epi16(a, b));
    tmp = _mm_unpacklo_epi16(a, b);
    sum_sq = _mm_madd_epi16(tmp, tmp);

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[2 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 1; i < height - 2; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu8_epi32(
          xx_loadl_32((__m128i *)&src[(i - 1) * src_stride + j]));
      y = _mm_cvtepu8_epi32(
          xx_loadl_32((__m128i *)&src[(i + 2) * src_stride + j]));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);
  }
}

static void selfguided_restoration_1_h(int32_t *A, int32_t *B, int width,
                                       int height, int buf_stride, int eps,
                                       int bit_depth) {
  int i, j;

  // Horizontal sum
  int width_extend = (width + 3) & ~3;
  for (i = 0; i < height; ++i) {
    int h = AOMMIN(2, height - i) + AOMMIN(1, i);

    __m128i a1 = _mm_loadu_si128((__m128i *)&A[i * buf_stride]);
    __m128i b1 = _mm_loadu_si128((__m128i *)&B[i * buf_stride]);
    __m128i a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + 4]);
    __m128i b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + 4]);

    // Note: The _mm_slli_si128 call sets up a register containing
    // {0, A[i * buf_stride], ..., A[i * buf_stride + 2]},
    // so that the first element of 'sum' (which should only add two values
    // together) ends up calculated correctly.
    __m128i sum_ = _mm_add_epi32(_mm_slli_si128(b1, 4),
                                 _mm_add_epi32(b1, _mm_alignr_epi8(b2, b1, 4)));
    __m128i sum_sq_ = _mm_add_epi32(
        _mm_slli_si128(a1, 4), _mm_add_epi32(a1, _mm_alignr_epi8(a2, a1, 4)));
    __m128i n = _mm_set_epi32(3 * h, 3 * h, 3 * h, 2 * h);
    __m128i one_over_n =
        _mm_set_epi32(one_by_x[3 * h - 1], one_by_x[3 * h - 1],
                      one_by_x[3 * h - 1], one_by_x[2 * h - 1]);
    __m128i s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][3 * h - 1], sgrproj_mtable[eps - 1][3 * h - 1],
        sgrproj_mtable[eps - 1][3 * h - 1], sgrproj_mtable[eps - 1][2 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride, A,
               B);

    n = _mm_set1_epi32(3 * h);
    one_over_n = _mm_set1_epi32(one_by_x[3 * h - 1]);
    s = _mm_set1_epi32(sgrproj_mtable[eps - 1][3 * h - 1]);

    // Re-align a1 and b1 so that they start at index i * buf_stride + 3
    a2 = _mm_alignr_epi8(a2, a1, 12);
    b2 = _mm_alignr_epi8(b2, b1, 12);

    // Note: When the width is not a multiple of 4, this loop may end up
    // writing to the last 4 columns of the frame, potentially with incorrect
    // values (especially for r=2 and r=3).
    // This is fine, since we fix up those values in the block after this
    // loop, and in exchange we never have more than four values to
    // write / fix up after this loop finishes.
    for (j = 4; j < width_extend - 4; j += 4) {
      a1 = a2;
      b1 = b2;
      a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 3]);
      b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 3]);
      /* Loop invariant: At this point,
         a1 = original A[i * buf_stride + j - 1 : i * buf_stride + j + 3]
         a2 = original A[i * buf_stride + j + 3 : i * buf_stride + j + 7]
         and similar for b1,b2 and B
      */
      sum_ = _mm_add_epi32(b1, _mm_add_epi32(_mm_alignr_epi8(b2, b1, 4),
                                             _mm_alignr_epi8(b2, b1, 8)));
      sum_sq_ = _mm_add_epi32(a1, _mm_add_epi32(_mm_alignr_epi8(a2, a1, 4),
                                                _mm_alignr_epi8(a2, a1, 8)));
      calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth,
                 i * buf_stride + j, A, B);
    }
    __m128i a3 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 3]);
    __m128i b3 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 3]);

    j = width - 4;
    switch (width % 4) {
      case 0:
        a1 = a2;
        b1 = b2;
        a2 = a3;
        b2 = b3;
        break;
      case 1:
        a1 = _mm_alignr_epi8(a2, a1, 4);
        b1 = _mm_alignr_epi8(b2, b1, 4);
        a2 = _mm_alignr_epi8(a3, a2, 4);
        b2 = _mm_alignr_epi8(b3, b2, 4);
        break;
      case 2:
        a1 = _mm_alignr_epi8(a2, a1, 8);
        b1 = _mm_alignr_epi8(b2, b1, 8);
        a2 = _mm_alignr_epi8(a3, a2, 8);
        b2 = _mm_alignr_epi8(b3, b2, 8);
        break;
      case 3:
        a1 = _mm_alignr_epi8(a2, a1, 12);
        b1 = _mm_alignr_epi8(b2, b1, 12);
        a2 = _mm_alignr_epi8(a3, a2, 12);
        b2 = _mm_alignr_epi8(b3, b2, 12);
        break;
    }

    // Zero out the data loaded from "off the edge" of the array
    __m128i zero = _mm_setzero_si128();
    a2 = _mm_blend_epi16(a2, zero, 0xfc);
    b2 = _mm_blend_epi16(b2, zero, 0xfc);

    sum_ = _mm_add_epi32(b1, _mm_add_epi32(_mm_alignr_epi8(b2, b1, 4),
                                           _mm_alignr_epi8(b2, b1, 8)));
    sum_sq_ = _mm_add_epi32(a1, _mm_add_epi32(_mm_alignr_epi8(a2, a1, 4),
                                              _mm_alignr_epi8(a2, a1, 8)));
    n = _mm_set_epi32(2 * h, 3 * h, 3 * h, 3 * h);
    one_over_n = _mm_set_epi32(one_by_x[2 * h - 1], one_by_x[3 * h - 1],
                               one_by_x[3 * h - 1], one_by_x[3 * h - 1]);
    s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][2 * h - 1], sgrproj_mtable[eps - 1][3 * h - 1],
        sgrproj_mtable[eps - 1][3 * h - 1], sgrproj_mtable[eps - 1][3 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride + j,
               A, B);
  }
}

static void selfguided_restoration_2_v(uint8_t *src, int width, int height,
                                       int src_stride, int32_t *A, int32_t *B,
                                       int buf_stride) {
  int i, j;

  // Vertical sum
  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, c, c2, x, y, x2, y2;
    __m128i sum, sum_sq, tmp;

    a = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[j]));
    b = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[src_stride + j]));
    c = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[2 * src_stride + j]));

    sum = _mm_cvtepi16_epi32(_mm_add_epi16(_mm_add_epi16(a, b), c));
    // Important: Since c may be up to 2^8, the result on squaring may
    // be up to 2^16. So we need to zero-extend, not sign-extend.
    c2 = _mm_cvtepu16_epi32(_mm_mullo_epi16(c, c));
    tmp = _mm_unpacklo_epi16(a, b);
    sum_sq = _mm_add_epi32(_mm_madd_epi16(tmp, tmp), c2);

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[3 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[4 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 2; i < height - 3; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu8_epi32(
          _mm_cvtsi32_si128(*((int *)&src[(i - 2) * src_stride + j])));
      y = _mm_cvtepu8_epi32(
          _mm_cvtsi32_si128(*((int *)&src[(i + 3) * src_stride + j])));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 2) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 2) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 2) * buf_stride + j], sum_sq);
  }
}

static void selfguided_restoration_2_h(int32_t *A, int32_t *B, int width,
                                       int height, int buf_stride, int eps,
                                       int bit_depth) {
  int i, j;

  // Horizontal sum
  int width_extend = (width + 3) & ~3;
  for (i = 0; i < height; ++i) {
    int h = AOMMIN(3, height - i) + AOMMIN(2, i);

    __m128i a1 = _mm_loadu_si128((__m128i *)&A[i * buf_stride]);
    __m128i b1 = _mm_loadu_si128((__m128i *)&B[i * buf_stride]);
    __m128i a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + 4]);
    __m128i b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + 4]);

    __m128i sum_ = _mm_add_epi32(
        _mm_add_epi32(
            _mm_add_epi32(_mm_slli_si128(b1, 8), _mm_slli_si128(b1, 4)),
            _mm_add_epi32(b1, _mm_alignr_epi8(b2, b1, 4))),
        _mm_alignr_epi8(b2, b1, 8));
    __m128i sum_sq_ = _mm_add_epi32(
        _mm_add_epi32(
            _mm_add_epi32(_mm_slli_si128(a1, 8), _mm_slli_si128(a1, 4)),
            _mm_add_epi32(a1, _mm_alignr_epi8(a2, a1, 4))),
        _mm_alignr_epi8(a2, a1, 8));

    __m128i n = _mm_set_epi32(5 * h, 5 * h, 4 * h, 3 * h);
    __m128i one_over_n =
        _mm_set_epi32(one_by_x[5 * h - 1], one_by_x[5 * h - 1],
                      one_by_x[4 * h - 1], one_by_x[3 * h - 1]);
    __m128i s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][5 * h - 1], sgrproj_mtable[eps - 1][5 * h - 1],
        sgrproj_mtable[eps - 1][4 * h - 1], sgrproj_mtable[eps - 1][3 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride, A,
               B);

    // Re-align a1 and b1 so that they start at index i * buf_stride + 2
    a2 = _mm_alignr_epi8(a2, a1, 8);
    b2 = _mm_alignr_epi8(b2, b1, 8);

    n = _mm_set1_epi32(5 * h);
    one_over_n = _mm_set1_epi32(one_by_x[5 * h - 1]);
    s = _mm_set1_epi32(sgrproj_mtable[eps - 1][5 * h - 1]);

    for (j = 4; j < width_extend - 4; j += 4) {
      a1 = a2;
      a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 2]);
      b1 = b2;
      b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 2]);
      /* Loop invariant: At this point,
         a1 = original A[i * buf_stride + j - 2 : i * buf_stride + j + 2]
         a2 = original A[i * buf_stride + j + 2 : i * buf_stride + j + 6]
         and similar for b1,b2 and B
      */
      sum_ = _mm_add_epi32(
          _mm_add_epi32(b1, _mm_add_epi32(_mm_alignr_epi8(b2, b1, 4),
                                          _mm_alignr_epi8(b2, b1, 8))),
          _mm_add_epi32(_mm_alignr_epi8(b2, b1, 12), b2));
      sum_sq_ = _mm_add_epi32(
          _mm_add_epi32(a1, _mm_add_epi32(_mm_alignr_epi8(a2, a1, 4),
                                          _mm_alignr_epi8(a2, a1, 8))),
          _mm_add_epi32(_mm_alignr_epi8(a2, a1, 12), a2));

      calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth,
                 i * buf_stride + j, A, B);
    }
    // If the width is not a multiple of 4, we need to reset j to width - 4
    // and adjust a1, a2, b1, b2 so that the loop invariant above is maintained
    __m128i a3 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 2]);
    __m128i b3 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 2]);

    j = width - 4;
    switch (width % 4) {
      case 0:
        a1 = a2;
        b1 = b2;
        a2 = a3;
        b2 = b3;
        break;
      case 1:
        a1 = _mm_alignr_epi8(a2, a1, 4);
        b1 = _mm_alignr_epi8(b2, b1, 4);
        a2 = _mm_alignr_epi8(a3, a2, 4);
        b2 = _mm_alignr_epi8(b3, b2, 4);
        break;
      case 2:
        a1 = _mm_alignr_epi8(a2, a1, 8);
        b1 = _mm_alignr_epi8(b2, b1, 8);
        a2 = _mm_alignr_epi8(a3, a2, 8);
        b2 = _mm_alignr_epi8(b3, b2, 8);
        break;
      case 3:
        a1 = _mm_alignr_epi8(a2, a1, 12);
        b1 = _mm_alignr_epi8(b2, b1, 12);
        a2 = _mm_alignr_epi8(a3, a2, 12);
        b2 = _mm_alignr_epi8(b3, b2, 12);
        break;
    }

    // Zero out the data loaded from "off the edge" of the array
    __m128i zero = _mm_setzero_si128();
    a2 = _mm_blend_epi16(a2, zero, 0xf0);
    b2 = _mm_blend_epi16(b2, zero, 0xf0);

    sum_ = _mm_add_epi32(
        _mm_add_epi32(b1, _mm_add_epi32(_mm_alignr_epi8(b2, b1, 4),
                                        _mm_alignr_epi8(b2, b1, 8))),
        _mm_add_epi32(_mm_alignr_epi8(b2, b1, 12), b2));
    sum_sq_ = _mm_add_epi32(
        _mm_add_epi32(a1, _mm_add_epi32(_mm_alignr_epi8(a2, a1, 4),
                                        _mm_alignr_epi8(a2, a1, 8))),
        _mm_add_epi32(_mm_alignr_epi8(a2, a1, 12), a2));

    n = _mm_set_epi32(3 * h, 4 * h, 5 * h, 5 * h);
    one_over_n = _mm_set_epi32(one_by_x[3 * h - 1], one_by_x[4 * h - 1],
                               one_by_x[5 * h - 1], one_by_x[5 * h - 1]);
    s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][3 * h - 1], sgrproj_mtable[eps - 1][4 * h - 1],
        sgrproj_mtable[eps - 1][5 * h - 1], sgrproj_mtable[eps - 1][5 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride + j,
               A, B);
  }
}

static void selfguided_restoration_3_v(uint8_t *src, int width, int height,
                                       int src_stride, int32_t *A, int32_t *B,
                                       int buf_stride) {
  int i, j;

  // Vertical sum over 7-pixel regions, 4 columns at a time
  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, c, d, x, y, x2, y2;
    __m128i sum, sum_sq, tmp, tmp2;

    a = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[j]));
    b = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[src_stride + j]));
    c = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[2 * src_stride + j]));
    d = _mm_cvtepu8_epi16(xx_loadl_32((__m128i *)&src[3 * src_stride + j]));

    sum = _mm_cvtepi16_epi32(
        _mm_add_epi16(_mm_add_epi16(a, b), _mm_add_epi16(c, d)));
    tmp = _mm_unpacklo_epi16(a, b);
    tmp2 = _mm_unpacklo_epi16(c, d);
    sum_sq =
        _mm_add_epi32(_mm_madd_epi16(tmp, tmp), _mm_madd_epi16(tmp2, tmp2));

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[4 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[5 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[2 * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[2 * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(xx_loadl_32((__m128i *)&src[6 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 3; i < height - 4; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu8_epi32(xx_loadl_32(&src[(i - 3) * src_stride + j]));
      y = _mm_cvtepu8_epi32(xx_loadl_32(&src[(i + 4) * src_stride + j]));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 3) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 2) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 2) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 2) * buf_stride + j], sum_sq);

    x = _mm_cvtepu8_epi32(
        xx_loadl_32((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 3) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 3) * buf_stride + j], sum_sq);
  }
}

static void selfguided_restoration_3_h(int32_t *A, int32_t *B, int width,
                                       int height, int buf_stride, int eps,
                                       int bit_depth) {
  int i, j;
  // Horizontal sum over 7-pixel regions of dst
  int width_extend = (width + 3) & ~3;
  for (i = 0; i < height; ++i) {
    int h = AOMMIN(4, height - i) + AOMMIN(3, i);

    __m128i a1 = _mm_loadu_si128((__m128i *)&A[i * buf_stride]);
    __m128i b1 = _mm_loadu_si128((__m128i *)&B[i * buf_stride]);
    __m128i a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + 4]);
    __m128i b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + 4]);

    __m128i sum_ = _mm_add_epi32(
        _mm_add_epi32(
            _mm_add_epi32(_mm_slli_si128(b1, 12), _mm_slli_si128(b1, 8)),
            _mm_add_epi32(_mm_slli_si128(b1, 4), b1)),
        _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(b2, b1, 4),
                                    _mm_alignr_epi8(b2, b1, 8)),
                      _mm_alignr_epi8(b2, b1, 12)));
    __m128i sum_sq_ = _mm_add_epi32(
        _mm_add_epi32(
            _mm_add_epi32(_mm_slli_si128(a1, 12), _mm_slli_si128(a1, 8)),
            _mm_add_epi32(_mm_slli_si128(a1, 4), a1)),
        _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(a2, a1, 4),
                                    _mm_alignr_epi8(a2, a1, 8)),
                      _mm_alignr_epi8(a2, a1, 12)));

    __m128i n = _mm_set_epi32(7 * h, 6 * h, 5 * h, 4 * h);
    __m128i one_over_n =
        _mm_set_epi32(one_by_x[7 * h - 1], one_by_x[6 * h - 1],
                      one_by_x[5 * h - 1], one_by_x[4 * h - 1]);
    __m128i s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][7 * h - 1], sgrproj_mtable[eps - 1][6 * h - 1],
        sgrproj_mtable[eps - 1][5 * h - 1], sgrproj_mtable[eps - 1][4 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride, A,
               B);

    // Re-align a1 and b1 so that they start at index i * buf_stride + 1
    a2 = _mm_alignr_epi8(a2, a1, 4);
    b2 = _mm_alignr_epi8(b2, b1, 4);

    n = _mm_set1_epi32(7 * h);
    one_over_n = _mm_set1_epi32(one_by_x[7 * h - 1]);
    s = _mm_set1_epi32(sgrproj_mtable[eps - 1][7 * h - 1]);

    for (j = 4; j < width_extend - 4; j += 4) {
      a1 = a2;
      a2 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 1]);
      b1 = b2;
      b2 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 1]);
      __m128i a3 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 5]);
      __m128i b3 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 5]);
      /* Loop invariant: At this point,
         a1 = original A[i * buf_stride + j - 3 : i * buf_stride + j + 1]
         a2 = original A[i * buf_stride + j + 1 : i * buf_stride + j + 5]
         a3 = original A[i * buf_stride + j + 5 : i * buf_stride + j + 9]
         and similar for b1,b2,b3 and B
      */
      sum_ = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(b1, _mm_alignr_epi8(b2, b1, 4)),
                        _mm_add_epi32(_mm_alignr_epi8(b2, b1, 8),
                                      _mm_alignr_epi8(b2, b1, 12))),
          _mm_add_epi32(_mm_add_epi32(b2, _mm_alignr_epi8(b3, b2, 4)),
                        _mm_alignr_epi8(b3, b2, 8)));
      sum_sq_ = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(a1, _mm_alignr_epi8(a2, a1, 4)),
                        _mm_add_epi32(_mm_alignr_epi8(a2, a1, 8),
                                      _mm_alignr_epi8(a2, a1, 12))),
          _mm_add_epi32(_mm_add_epi32(a2, _mm_alignr_epi8(a3, a2, 4)),
                        _mm_alignr_epi8(a3, a2, 8)));

      calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth,
                 i * buf_stride + j, A, B);
    }
    __m128i a3 = _mm_loadu_si128((__m128i *)&A[i * buf_stride + j + 1]);
    __m128i b3 = _mm_loadu_si128((__m128i *)&B[i * buf_stride + j + 1]);

    j = width - 4;
    switch (width % 4) {
      case 0:
        a1 = a2;
        b1 = b2;
        a2 = a3;
        b2 = b3;
        break;
      case 1:
        a1 = _mm_alignr_epi8(a2, a1, 4);
        b1 = _mm_alignr_epi8(b2, b1, 4);
        a2 = _mm_alignr_epi8(a3, a2, 4);
        b2 = _mm_alignr_epi8(b3, b2, 4);
        break;
      case 2:
        a1 = _mm_alignr_epi8(a2, a1, 8);
        b1 = _mm_alignr_epi8(b2, b1, 8);
        a2 = _mm_alignr_epi8(a3, a2, 8);
        b2 = _mm_alignr_epi8(b3, b2, 8);
        break;
      case 3:
        a1 = _mm_alignr_epi8(a2, a1, 12);
        b1 = _mm_alignr_epi8(b2, b1, 12);
        a2 = _mm_alignr_epi8(a3, a2, 12);
        b2 = _mm_alignr_epi8(b3, b2, 12);
        break;
    }

    // Zero out the data loaded from "off the edge" of the array
    __m128i zero = _mm_setzero_si128();
    a2 = _mm_blend_epi16(a2, zero, 0xc0);
    b2 = _mm_blend_epi16(b2, zero, 0xc0);

    sum_ = _mm_add_epi32(
        _mm_add_epi32(_mm_add_epi32(b1, _mm_alignr_epi8(b2, b1, 4)),
                      _mm_add_epi32(_mm_alignr_epi8(b2, b1, 8),
                                    _mm_alignr_epi8(b2, b1, 12))),
        _mm_add_epi32(_mm_add_epi32(b2, _mm_alignr_epi8(zero, b2, 4)),
                      _mm_alignr_epi8(zero, b2, 8)));
    sum_sq_ = _mm_add_epi32(
        _mm_add_epi32(_mm_add_epi32(a1, _mm_alignr_epi8(a2, a1, 4)),
                      _mm_add_epi32(_mm_alignr_epi8(a2, a1, 8),
                                    _mm_alignr_epi8(a2, a1, 12))),
        _mm_add_epi32(_mm_add_epi32(a2, _mm_alignr_epi8(zero, a2, 4)),
                      _mm_alignr_epi8(zero, a2, 8)));

    n = _mm_set_epi32(4 * h, 5 * h, 6 * h, 7 * h);
    one_over_n = _mm_set_epi32(one_by_x[4 * h - 1], one_by_x[5 * h - 1],
                               one_by_x[6 * h - 1], one_by_x[7 * h - 1]);
    s = _mm_set_epi32(
        sgrproj_mtable[eps - 1][4 * h - 1], sgrproj_mtable[eps - 1][5 * h - 1],
        sgrproj_mtable[eps - 1][6 * h - 1], sgrproj_mtable[eps - 1][7 * h - 1]);
    calc_block(sum_, sum_sq_, n, &one_over_n, &s, bit_depth, i * buf_stride + j,
               A, B);
  }
}

void av1_selfguided_restoration_sse4_1(uint8_t *dgd, int width, int height,
                                       int dgd_stride, int32_t *dst,
                                       int dst_stride, int r, int eps) {
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *A = A_;
  int32_t *B = B_;
  int i, j;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes for efficiency.
  int buf_stride = ((width_ext + 3) & ~3) + 16;

  // Don't filter tiles with dimensions < 5 on any axis
  if ((width < 5) || (height < 5)) return;

  uint8_t *dgd0 = dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ;
  if (r == 1) {
    selfguided_restoration_1_v(dgd0, width_ext, height_ext, dgd_stride, A, B,
                               buf_stride);
    selfguided_restoration_1_h(A, B, width_ext, height_ext, buf_stride, eps, 8);
  } else if (r == 2) {
    selfguided_restoration_2_v(dgd0, width_ext, height_ext, dgd_stride, A, B,
                               buf_stride);
    selfguided_restoration_2_h(A, B, width_ext, height_ext, buf_stride, eps, 8);
  } else if (r == 3) {
    selfguided_restoration_3_v(dgd0, width_ext, height_ext, dgd_stride, A, B,
                               buf_stride);
    selfguided_restoration_3_h(A, B, width_ext, height_ext, buf_stride, eps, 8);
  } else {
    assert(0);
  }
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

  {
    i = 0;
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k + 1] + 2 * A[k + buf_stride] +
                        A[k + buf_stride + 1];
      const int32_t b = 3 * B[k] + 2 * B[k + 1] + 2 * B[k + buf_stride] +
                        B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + buf_stride] +
                        A[k + buf_stride - 1] + A[k + buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + buf_stride] +
                        B[k + buf_stride - 1] + B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k - 1] + 2 * A[k + buf_stride] +
                        A[k + buf_stride - 1];
      const int32_t b = 3 * B[k] + 2 * B[k - 1] + 2 * B[k + buf_stride] +
                        B[k + buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
  for (i = 1; i < height - 1; ++i) {
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                        A[k + 1] + A[k - buf_stride + 1] +
                        A[k + buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                        B[k + 1] + B[k - buf_stride + 1] +
                        B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }

    // Vectorize the innermost loop
    for (j = 1; j < width - 1; j += 4) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;

      __m128i tmp0 = _mm_loadu_si128((__m128i *)&A[k - 1 - buf_stride]);
      __m128i tmp1 = _mm_loadu_si128((__m128i *)&A[k + 3 - buf_stride]);
      __m128i tmp2 = _mm_loadu_si128((__m128i *)&A[k - 1]);
      __m128i tmp3 = _mm_loadu_si128((__m128i *)&A[k + 3]);
      __m128i tmp4 = _mm_loadu_si128((__m128i *)&A[k - 1 + buf_stride]);
      __m128i tmp5 = _mm_loadu_si128((__m128i *)&A[k + 3 + buf_stride]);

      __m128i a0 = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(tmp3, tmp2, 4), tmp2),
                        _mm_add_epi32(_mm_alignr_epi8(tmp3, tmp2, 8),
                                      _mm_alignr_epi8(tmp5, tmp4, 4))),
          _mm_alignr_epi8(tmp1, tmp0, 4));
      __m128i a1 = _mm_add_epi32(_mm_add_epi32(tmp0, tmp4),
                                 _mm_add_epi32(_mm_alignr_epi8(tmp1, tmp0, 8),
                                               _mm_alignr_epi8(tmp5, tmp4, 8)));
      __m128i a = _mm_sub_epi32(_mm_slli_epi32(_mm_add_epi32(a0, a1), 2), a1);

      __m128i tmp6 = _mm_loadu_si128((__m128i *)&B[k - 1 - buf_stride]);
      __m128i tmp7 = _mm_loadu_si128((__m128i *)&B[k + 3 - buf_stride]);
      __m128i tmp8 = _mm_loadu_si128((__m128i *)&B[k - 1]);
      __m128i tmp9 = _mm_loadu_si128((__m128i *)&B[k + 3]);
      __m128i tmp10 = _mm_loadu_si128((__m128i *)&B[k - 1 + buf_stride]);
      __m128i tmp11 = _mm_loadu_si128((__m128i *)&B[k + 3 + buf_stride]);

      __m128i b0 = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(tmp9, tmp8, 4), tmp8),
                        _mm_add_epi32(_mm_alignr_epi8(tmp9, tmp8, 8),
                                      _mm_alignr_epi8(tmp11, tmp10, 4))),
          _mm_alignr_epi8(tmp7, tmp6, 4));
      __m128i b1 =
          _mm_add_epi32(_mm_add_epi32(tmp6, tmp10),
                        _mm_add_epi32(_mm_alignr_epi8(tmp7, tmp6, 8),
                                      _mm_alignr_epi8(tmp11, tmp10, 8)));
      __m128i b = _mm_sub_epi32(_mm_slli_epi32(_mm_add_epi32(b0, b1), 2), b1);

      __m128i src = _mm_cvtepu8_epi32(_mm_loadu_si128((__m128i *)&dgd[l]));

      __m128i rounding = _mm_set1_epi32(
          (1 << (SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS)) >> 1);
      __m128i v = _mm_add_epi32(_mm_mullo_epi32(a, src), b);
      __m128i w = _mm_srai_epi32(_mm_add_epi32(v, rounding),
                                 SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
      _mm_storeu_si128((__m128i *)&dst[m], w);
    }

    // Deal with any extra pixels at the right-hand edge of the frame
    // (typically have 2 such pixels, but may have anywhere between 0 and 3)
    for (; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }

    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                        A[k - 1] + A[k - buf_stride - 1] +
                        A[k + buf_stride - 1];
      const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                        B[k - 1] + B[k - buf_stride - 1] +
                        B[k + buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }

  {
    i = height - 1;
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k + 1] + 2 * A[k - buf_stride] +
                        A[k - buf_stride + 1];
      const int32_t b = 3 * B[k] + 2 * B[k + 1] + 2 * B[k - buf_stride] +
                        B[k - buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - buf_stride] +
                        A[k - buf_stride - 1] + A[k - buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - buf_stride] +
                        B[k - buf_stride - 1] + B[k - buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k - 1] + 2 * A[k - buf_stride] +
                        A[k - buf_stride - 1];
      const int32_t b = 3 * B[k] + 2 * B[k - 1] + 2 * B[k - buf_stride] +
                        B[k - buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

void av1_highpass_filter_sse4_1(uint8_t *dgd, int width, int height, int stride,
                                int32_t *dst, int dst_stride, int corner,
                                int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  {
    i = 0;
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
          corner *
              (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] = center * dgd[k] +
               edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
               corner * (dgd[k + stride - 1] + dgd[k + stride + 1] +
                         dgd[k - 1] + dgd[k + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
          corner *
              (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
    }
  }
  {
    i = height - 1;
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
          corner *
              (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] = center * dgd[k] +
               edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
               corner * (dgd[k - stride - 1] + dgd[k - stride + 1] +
                         dgd[k - 1] + dgd[k + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
          corner *
              (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
    }
  }
  __m128i center_ = _mm_set1_epi16(center);
  __m128i edge_ = _mm_set1_epi16(edge);
  __m128i corner_ = _mm_set1_epi16(corner);
  for (i = 1; i < height - 1; ++i) {
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
          corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                    dgd[k - stride] + dgd[k + stride]);
    }
    // Process in units of 8 pixels at a time.
    for (j = 1; j < width - 8; j += 8) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;

      __m128i a = _mm_loadu_si128((__m128i *)&dgd[k - stride - 1]);
      __m128i b = _mm_loadu_si128((__m128i *)&dgd[k - 1]);
      __m128i c = _mm_loadu_si128((__m128i *)&dgd[k + stride - 1]);

      __m128i tl = _mm_cvtepu8_epi16(a);
      __m128i tr = _mm_cvtepu8_epi16(_mm_srli_si128(a, 8));
      __m128i cl = _mm_cvtepu8_epi16(b);
      __m128i cr = _mm_cvtepu8_epi16(_mm_srli_si128(b, 8));
      __m128i bl = _mm_cvtepu8_epi16(c);
      __m128i br = _mm_cvtepu8_epi16(_mm_srli_si128(c, 8));

      __m128i x = _mm_alignr_epi8(cr, cl, 2);
      __m128i y = _mm_add_epi16(_mm_add_epi16(_mm_alignr_epi8(tr, tl, 2), cl),
                                _mm_add_epi16(_mm_alignr_epi8(br, bl, 2),
                                              _mm_alignr_epi8(cr, cl, 4)));
      __m128i z = _mm_add_epi16(_mm_add_epi16(tl, bl),
                                _mm_add_epi16(_mm_alignr_epi8(tr, tl, 4),
                                              _mm_alignr_epi8(br, bl, 4)));

      __m128i res = _mm_add_epi16(_mm_mullo_epi16(x, center_),
                                  _mm_add_epi16(_mm_mullo_epi16(y, edge_),
                                                _mm_mullo_epi16(z, corner_)));

      _mm_storeu_si128((__m128i *)&dst[l], _mm_cvtepi16_epi32(res));
      _mm_storeu_si128((__m128i *)&dst[l + 4],
                       _mm_cvtepi16_epi32(_mm_srli_si128(res, 8)));
    }
    // If there are enough pixels left in this row, do another batch of 4
    // pixels.
    for (; j < width - 4; j += 4) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;

      __m128i a = _mm_loadl_epi64((__m128i *)&dgd[k - stride - 1]);
      __m128i b = _mm_loadl_epi64((__m128i *)&dgd[k - 1]);
      __m128i c = _mm_loadl_epi64((__m128i *)&dgd[k + stride - 1]);

      __m128i tl = _mm_cvtepu8_epi16(a);
      __m128i cl = _mm_cvtepu8_epi16(b);
      __m128i bl = _mm_cvtepu8_epi16(c);

      __m128i x = _mm_srli_si128(cl, 2);
      __m128i y = _mm_add_epi16(
          _mm_add_epi16(_mm_srli_si128(tl, 2), cl),
          _mm_add_epi16(_mm_srli_si128(bl, 2), _mm_srli_si128(cl, 4)));
      __m128i z = _mm_add_epi16(
          _mm_add_epi16(tl, bl),
          _mm_add_epi16(_mm_srli_si128(tl, 4), _mm_srli_si128(bl, 4)));

      __m128i res = _mm_add_epi16(_mm_mullo_epi16(x, center_),
                                  _mm_add_epi16(_mm_mullo_epi16(y, edge_),
                                                _mm_mullo_epi16(z, corner_)));

      _mm_storeu_si128((__m128i *)&dst[l], _mm_cvtepi16_epi32(res));
    }
    // Handle any leftover pixels
    for (; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride] + dgd[k + stride]);
    }
  }
}

void apply_selfguided_restoration_sse4_1(uint8_t *dat, int width, int height,
                                         int stride, int eps, int *xqd,
                                         uint8_t *dst, int dst_stride,
                                         int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_sse4_1(dat, width, height, stride, flt1, width,
                             sgr_params[eps].corner, sgr_params[eps].edge);
#else
    av1_selfguided_restoration_sse4_1(dat, width, height, stride, flt1, width,
                                      sgr_params[eps].r1, sgr_params[eps].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_sse4_1(dat, width, height, stride, flt2, width,
                                    sgr_params[eps].r2, sgr_params[eps].e2);
  decode_xq(xqd, xq);

  __m128i xq0 = _mm_set1_epi32(xq[0]);
  __m128i xq1 = _mm_set1_epi32(xq[1]);
  for (i = 0; i < height; ++i) {
    // Calculate output in batches of 8 pixels
    for (j = 0; j < width; j += 8) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      __m128i src =
          _mm_slli_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&dat[l])),
                         SGRPROJ_RST_BITS);

      const __m128i u_0 = _mm_cvtepu16_epi32(src);
      const __m128i u_1 = _mm_cvtepu16_epi32(_mm_srli_si128(src, 8));

      const __m128i f1_0 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt1[k]), u_0);
      const __m128i f2_0 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt2[k]), u_0);
      const __m128i f1_1 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt1[k + 4]), u_1);
      const __m128i f2_1 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt2[k + 4]), u_1);

      const __m128i v_0 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_0), _mm_mullo_epi32(xq1, f2_0)),
          _mm_slli_epi32(u_0, SGRPROJ_PRJ_BITS));
      const __m128i v_1 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_1), _mm_mullo_epi32(xq1, f2_1)),
          _mm_slli_epi32(u_1, SGRPROJ_PRJ_BITS));

      const __m128i rounding =
          _mm_set1_epi32((1 << (SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS)) >> 1);
      const __m128i w_0 = _mm_srai_epi32(_mm_add_epi32(v_0, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      const __m128i w_1 = _mm_srai_epi32(_mm_add_epi32(v_1, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

      const __m128i tmp = _mm_packs_epi32(w_0, w_1);
      const __m128i res = _mm_packus_epi16(tmp, tmp /* "don't care" value */);
      _mm_storel_epi64((__m128i *)&dst[m], res);
    }
    // Process leftover pixels
    for (; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = (uint16_t)clip_pixel(w);
    }
  }
}

#if CONFIG_HIGHBITDEPTH
// Only the vertical sums need to be adjusted for highbitdepth

static void highbd_selfguided_restoration_1_v(uint16_t *src, int width,
                                              int height, int src_stride,
                                              int32_t *A, int32_t *B,
                                              int buf_stride) {
  int i, j;

  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, x, y, x2, y2;
    __m128i sum, sum_sq, tmp;

    a = _mm_loadl_epi64((__m128i *)&src[j]);
    b = _mm_loadl_epi64((__m128i *)&src[src_stride + j]);

    sum = _mm_cvtepi16_epi32(_mm_add_epi16(a, b));
    tmp = _mm_unpacklo_epi16(a, b);
    sum_sq = _mm_madd_epi16(tmp, tmp);

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[2 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 1; i < height - 2; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i - 1) * src_stride + j]));
      y = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i + 2) * src_stride + j]));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);
  }
}

static void highbd_selfguided_restoration_2_v(uint16_t *src, int width,
                                              int height, int src_stride,
                                              int32_t *A, int32_t *B,
                                              int buf_stride) {
  int i, j;

  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, c, c2, x, y, x2, y2;
    __m128i sum, sum_sq, tmp;

    a = _mm_loadl_epi64((__m128i *)&src[j]);
    b = _mm_loadl_epi64((__m128i *)&src[src_stride + j]);
    c = _mm_loadl_epi64((__m128i *)&src[2 * src_stride + j]);

    sum = _mm_cvtepi16_epi32(_mm_add_epi16(_mm_add_epi16(a, b), c));
    // Important: We need to widen *before* squaring here, since
    // c^2 may be up to 2^24.
    c = _mm_cvtepu16_epi32(c);
    c2 = _mm_mullo_epi32(c, c);
    tmp = _mm_unpacklo_epi16(a, b);
    sum_sq = _mm_add_epi32(_mm_madd_epi16(tmp, tmp), c2);

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[3 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[4 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 2; i < height - 3; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i - 2) * src_stride + j]));
      y = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i + 3) * src_stride + j]));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 2) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 2) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 2) * buf_stride + j], sum_sq);
  }
}

static void highbd_selfguided_restoration_3_v(uint16_t *src, int width,
                                              int height, int src_stride,
                                              int32_t *A, int32_t *B,
                                              int buf_stride) {
  int i, j;

  int width_extend = (width + 3) & ~3;
  for (j = 0; j < width_extend; j += 4) {
    __m128i a, b, c, d, x, y, x2, y2;
    __m128i sum, sum_sq, tmp, tmp2;

    a = _mm_loadl_epi64((__m128i *)&src[j]);
    b = _mm_loadl_epi64((__m128i *)&src[src_stride + j]);
    c = _mm_loadl_epi64((__m128i *)&src[2 * src_stride + j]);
    d = _mm_loadl_epi64((__m128i *)&src[3 * src_stride + j]);

    sum = _mm_cvtepi16_epi32(
        _mm_add_epi16(_mm_add_epi16(a, b), _mm_add_epi16(c, d)));
    tmp = _mm_unpacklo_epi16(a, b);
    tmp2 = _mm_unpacklo_epi16(c, d);
    sum_sq =
        _mm_add_epi32(_mm_madd_epi16(tmp, tmp), _mm_madd_epi16(tmp2, tmp2));

    _mm_store_si128((__m128i *)&B[j], sum);
    _mm_store_si128((__m128i *)&A[j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[4 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[5 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[2 * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[2 * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[6 * src_stride + j]));
    sum = _mm_add_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_add_epi32(sum_sq, x2);

    for (i = 3; i < height - 4; ++i) {
      _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
      _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

      x = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i - 3) * src_stride + j]));
      y = _mm_cvtepu16_epi32(
          _mm_loadl_epi64((__m128i *)&src[(i + 4) * src_stride + j]));

      sum = _mm_add_epi32(sum, _mm_sub_epi32(y, x));

      x2 = _mm_mullo_epi32(x, x);
      y2 = _mm_mullo_epi32(y, y);

      sum_sq = _mm_add_epi32(sum_sq, _mm_sub_epi32(y2, x2));
    }
    _mm_store_si128((__m128i *)&B[i * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[i * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 3) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 1) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 1) * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 2) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 2) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 2) * buf_stride + j], sum_sq);

    x = _mm_cvtepu16_epi32(
        _mm_loadl_epi64((__m128i *)&src[(i - 1) * src_stride + j]));
    sum = _mm_sub_epi32(sum, x);
    x2 = _mm_mullo_epi32(x, x);
    sum_sq = _mm_sub_epi32(sum_sq, x2);

    _mm_store_si128((__m128i *)&B[(i + 3) * buf_stride + j], sum);
    _mm_store_si128((__m128i *)&A[(i + 3) * buf_stride + j], sum_sq);
  }
}

void av1_selfguided_restoration_highbd_sse4_1(uint16_t *dgd, int width,
                                              int height, int dgd_stride,
                                              int32_t *dst, int dst_stride,
                                              int bit_depth, int r, int eps) {
  DECLARE_ALIGNED(16, int32_t, A_[RESTORATION_PROC_UNIT_PELS]);
  DECLARE_ALIGNED(16, int32_t, B_[RESTORATION_PROC_UNIT_PELS]);
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  int32_t *A = A_;
  int32_t *B = B_;
  int i, j;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes for efficiency.
  int buf_stride = ((width_ext + 3) & ~3) + 16;

  // Don't filter tiles with dimensions < 5 on any axis
  if ((width < 5) || (height < 5)) return;

  uint16_t *dgd0 = dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ;
  if (r == 1) {
    highbd_selfguided_restoration_1_v(dgd0, width_ext, height_ext, dgd_stride,
                                      A, B, buf_stride);
    selfguided_restoration_1_h(A, B, width_ext, height_ext, buf_stride, eps,
                               bit_depth);
  } else if (r == 2) {
    highbd_selfguided_restoration_2_v(dgd0, width_ext, height_ext, dgd_stride,
                                      A, B, buf_stride);
    selfguided_restoration_2_h(A, B, width_ext, height_ext, buf_stride, eps,
                               bit_depth);
  } else if (r == 3) {
    highbd_selfguided_restoration_3_v(dgd0, width_ext, height_ext, dgd_stride,
                                      A, B, buf_stride);
    selfguided_restoration_3_h(A, B, width_ext, height_ext, buf_stride, eps,
                               bit_depth);
  } else {
    assert(0);
  }
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

  {
    i = 0;
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k + 1] + 2 * A[k + buf_stride] +
                        A[k + buf_stride + 1];
      const int32_t b = 3 * B[k] + 2 * B[k + 1] + 2 * B[k + buf_stride] +
                        B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + buf_stride] +
                        A[k + buf_stride - 1] + A[k + buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + buf_stride] +
                        B[k + buf_stride - 1] + B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k - 1] + 2 * A[k + buf_stride] +
                        A[k + buf_stride - 1];
      const int32_t b = 3 * B[k] + 2 * B[k - 1] + 2 * B[k + buf_stride] +
                        B[k + buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
  for (i = 1; i < height - 1; ++i) {
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                        A[k + 1] + A[k - buf_stride + 1] +
                        A[k + buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                        B[k + 1] + B[k - buf_stride + 1] +
                        B[k + buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }

    // Vectorize the innermost loop
    for (j = 1; j < width - 1; j += 4) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;

      __m128i tmp0 = _mm_loadu_si128((__m128i *)&A[k - 1 - buf_stride]);
      __m128i tmp1 = _mm_loadu_si128((__m128i *)&A[k + 3 - buf_stride]);
      __m128i tmp2 = _mm_loadu_si128((__m128i *)&A[k - 1]);
      __m128i tmp3 = _mm_loadu_si128((__m128i *)&A[k + 3]);
      __m128i tmp4 = _mm_loadu_si128((__m128i *)&A[k - 1 + buf_stride]);
      __m128i tmp5 = _mm_loadu_si128((__m128i *)&A[k + 3 + buf_stride]);

      __m128i a0 = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(tmp3, tmp2, 4), tmp2),
                        _mm_add_epi32(_mm_alignr_epi8(tmp3, tmp2, 8),
                                      _mm_alignr_epi8(tmp5, tmp4, 4))),
          _mm_alignr_epi8(tmp1, tmp0, 4));
      __m128i a1 = _mm_add_epi32(_mm_add_epi32(tmp0, tmp4),
                                 _mm_add_epi32(_mm_alignr_epi8(tmp1, tmp0, 8),
                                               _mm_alignr_epi8(tmp5, tmp4, 8)));
      __m128i a = _mm_sub_epi32(_mm_slli_epi32(_mm_add_epi32(a0, a1), 2), a1);

      __m128i tmp6 = _mm_loadu_si128((__m128i *)&B[k - 1 - buf_stride]);
      __m128i tmp7 = _mm_loadu_si128((__m128i *)&B[k + 3 - buf_stride]);
      __m128i tmp8 = _mm_loadu_si128((__m128i *)&B[k - 1]);
      __m128i tmp9 = _mm_loadu_si128((__m128i *)&B[k + 3]);
      __m128i tmp10 = _mm_loadu_si128((__m128i *)&B[k - 1 + buf_stride]);
      __m128i tmp11 = _mm_loadu_si128((__m128i *)&B[k + 3 + buf_stride]);

      __m128i b0 = _mm_add_epi32(
          _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(tmp9, tmp8, 4), tmp8),
                        _mm_add_epi32(_mm_alignr_epi8(tmp9, tmp8, 8),
                                      _mm_alignr_epi8(tmp11, tmp10, 4))),
          _mm_alignr_epi8(tmp7, tmp6, 4));
      __m128i b1 =
          _mm_add_epi32(_mm_add_epi32(tmp6, tmp10),
                        _mm_add_epi32(_mm_alignr_epi8(tmp7, tmp6, 8),
                                      _mm_alignr_epi8(tmp11, tmp10, 8)));
      __m128i b = _mm_sub_epi32(_mm_slli_epi32(_mm_add_epi32(b0, b1), 2), b1);

      __m128i src = _mm_cvtepu16_epi32(_mm_loadu_si128((__m128i *)&dgd[l]));

      __m128i rounding = _mm_set1_epi32(
          (1 << (SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS)) >> 1);
      __m128i v = _mm_add_epi32(_mm_mullo_epi32(a, src), b);
      __m128i w = _mm_srai_epi32(_mm_add_epi32(v, rounding),
                                 SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
      _mm_storeu_si128((__m128i *)&dst[m], w);
    }

    // Deal with any extra pixels at the right-hand edge of the frame
    // (typically have 2 such pixels, but may have anywhere between 0 and 3)
    for (; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }

    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                        A[k - 1] + A[k - buf_stride - 1] +
                        A[k + buf_stride - 1];
      const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                        B[k - 1] + B[k - buf_stride - 1] +
                        B[k + buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }

  {
    i = height - 1;
    j = 0;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k + 1] + 2 * A[k - buf_stride] +
                        A[k - buf_stride + 1];
      const int32_t b = 3 * B[k] + 2 * B[k + 1] + 2 * B[k - buf_stride] +
                        B[k - buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - buf_stride] +
                        A[k - buf_stride - 1] + A[k - buf_stride + 1];
      const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - buf_stride] +
                        B[k - buf_stride - 1] + B[k - buf_stride + 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
    j = width - 1;
    {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 3;
      const int32_t a = 3 * A[k] + 2 * A[k - 1] + 2 * A[k - buf_stride] +
                        A[k - buf_stride - 1];
      const int32_t b = 3 * B[k] + 2 * B[k - 1] + 2 * B[k - buf_stride] +
                        B[k - buf_stride - 1];
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

void av1_highpass_filter_highbd_sse4_1(uint16_t *dgd, int width, int height,
                                       int stride, int32_t *dst, int dst_stride,
                                       int corner, int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  {
    i = 0;
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
          corner *
              (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] = center * dgd[k] +
               edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
               corner * (dgd[k + stride - 1] + dgd[k + stride + 1] +
                         dgd[k - 1] + dgd[k + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
          corner *
              (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
    }
  }
  __m128i center_ = _mm_set1_epi32(center);
  __m128i edge_ = _mm_set1_epi32(edge);
  __m128i corner_ = _mm_set1_epi32(corner);
  for (i = 1; i < height - 1; ++i) {
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
          corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                    dgd[k - stride] + dgd[k + stride]);
    }
    // Process 4 pixels at a time
    for (j = 1; j < width - 4; j += 4) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;

      __m128i a = _mm_loadu_si128((__m128i *)&dgd[k - stride - 1]);
      __m128i b = _mm_loadu_si128((__m128i *)&dgd[k - 1]);
      __m128i c = _mm_loadu_si128((__m128i *)&dgd[k + stride - 1]);

      __m128i tl = _mm_cvtepu16_epi32(a);
      __m128i tr = _mm_cvtepu16_epi32(_mm_srli_si128(a, 8));
      __m128i cl = _mm_cvtepu16_epi32(b);
      __m128i cr = _mm_cvtepu16_epi32(_mm_srli_si128(b, 8));
      __m128i bl = _mm_cvtepu16_epi32(c);
      __m128i br = _mm_cvtepu16_epi32(_mm_srli_si128(c, 8));

      __m128i x = _mm_alignr_epi8(cr, cl, 4);
      __m128i y = _mm_add_epi32(_mm_add_epi32(_mm_alignr_epi8(tr, tl, 4), cl),
                                _mm_add_epi32(_mm_alignr_epi8(br, bl, 4),
                                              _mm_alignr_epi8(cr, cl, 8)));
      __m128i z = _mm_add_epi32(_mm_add_epi32(tl, bl),
                                _mm_add_epi32(_mm_alignr_epi8(tr, tl, 8),
                                              _mm_alignr_epi8(br, bl, 8)));

      __m128i res = _mm_add_epi32(_mm_mullo_epi32(x, center_),
                                  _mm_add_epi32(_mm_mullo_epi32(y, edge_),
                                                _mm_mullo_epi32(z, corner_)));

      _mm_storeu_si128((__m128i *)&dst[l], res);
    }
    // Handle any leftover pixels
    for (; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride] + dgd[k + stride]);
    }
  }
  {
    i = height - 1;
    j = 0;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
          corner *
              (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
    }
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] = center * dgd[k] +
               edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
               corner * (dgd[k - stride - 1] + dgd[k - stride + 1] +
                         dgd[k - 1] + dgd[k + 1]);
    }
    j = width - 1;
    {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
          corner *
              (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
    }
  }
}

void apply_selfguided_restoration_highbd_sse4_1(
    uint16_t *dat, int width, int height, int stride, int bit_depth, int eps,
    int *xqd, uint16_t *dst, int dst_stride, int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_highbd_sse4_1(dat, width, height, stride, flt1, width,
                                    sgr_params[eps].corner,
                                    sgr_params[eps].edge);
#else
  av1_selfguided_restoration_highbd_sse4_1(dat, width, height, stride, flt1,
                                           width, bit_depth, sgr_params[eps].r1,
                                           sgr_params[eps].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_highbd_sse4_1(dat, width, height, stride, flt2,
                                           width, bit_depth, sgr_params[eps].r2,
                                           sgr_params[eps].e2);
  decode_xq(xqd, xq);

  __m128i xq0 = _mm_set1_epi32(xq[0]);
  __m128i xq1 = _mm_set1_epi32(xq[1]);
  for (i = 0; i < height; ++i) {
    // Calculate output in batches of 8 pixels
    for (j = 0; j < width; j += 8) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      __m128i src =
          _mm_slli_epi16(_mm_load_si128((__m128i *)&dat[l]), SGRPROJ_RST_BITS);

      const __m128i u_0 = _mm_cvtepu16_epi32(src);
      const __m128i u_1 = _mm_cvtepu16_epi32(_mm_srli_si128(src, 8));

      const __m128i f1_0 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt1[k]), u_0);
      const __m128i f2_0 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt2[k]), u_0);
      const __m128i f1_1 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt1[k + 4]), u_1);
      const __m128i f2_1 =
          _mm_sub_epi32(_mm_loadu_si128((__m128i *)&flt2[k + 4]), u_1);

      const __m128i v_0 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_0), _mm_mullo_epi32(xq1, f2_0)),
          _mm_slli_epi32(u_0, SGRPROJ_PRJ_BITS));
      const __m128i v_1 = _mm_add_epi32(
          _mm_add_epi32(_mm_mullo_epi32(xq0, f1_1), _mm_mullo_epi32(xq1, f2_1)),
          _mm_slli_epi32(u_1, SGRPROJ_PRJ_BITS));

      const __m128i rounding =
          _mm_set1_epi32((1 << (SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS)) >> 1);
      const __m128i w_0 = _mm_srai_epi32(_mm_add_epi32(v_0, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      const __m128i w_1 = _mm_srai_epi32(_mm_add_epi32(v_1, rounding),
                                         SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

      // Pack into 16 bits and clamp to [0, 2^bit_depth)
      const __m128i tmp = _mm_packus_epi32(w_0, w_1);
      const __m128i max = _mm_set1_epi16((1 << bit_depth) - 1);
      const __m128i res = _mm_min_epi16(tmp, max);

      _mm_store_si128((__m128i *)&dst[m], res);
    }
    // Process leftover pixels
    for (; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = (uint16_t)clip_pixel_highbd(w, bit_depth);
    }
  }
}

#endif
