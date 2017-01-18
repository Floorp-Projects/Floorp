/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tmmintrin.h>  // SSSE3

#include "./vp9_rtcd.h"
#include "./vpx_dsp_rtcd.h"
#include "./vpx_scale_rtcd.h"
#include "vpx_scale/yv12config.h"

extern void vp9_scale_and_extend_frame_c(const YV12_BUFFER_CONFIG *src,
                                         YV12_BUFFER_CONFIG *dst);

static void downsample_2_to_1_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride, int w,
                                    int h) {
  const __m128i mask = _mm_set1_epi16(0x00FF);
  const int max_width = w & ~15;
  int y;
  for (y = 0; y < h; ++y) {
    int x;
    for (x = 0; x < max_width; x += 16) {
      const __m128i a = _mm_loadu_si128((const __m128i *)(src + x * 2 + 0));
      const __m128i b = _mm_loadu_si128((const __m128i *)(src + x * 2 + 16));
      const __m128i a_and = _mm_and_si128(a, mask);
      const __m128i b_and = _mm_and_si128(b, mask);
      const __m128i c = _mm_packus_epi16(a_and, b_and);
      _mm_storeu_si128((__m128i *)(dst + x), c);
    }
    for (; x < w; ++x) dst[x] = src[x * 2];
    src += src_stride * 2;
    dst += dst_stride;
  }
}

static INLINE __m128i filter(const __m128i *const a, const __m128i *const b,
                             const __m128i *const c, const __m128i *const d,
                             const __m128i *const e, const __m128i *const f,
                             const __m128i *const g, const __m128i *const h) {
  const __m128i coeffs_ab =
      _mm_set_epi8(6, -1, 6, -1, 6, -1, 6, -1, 6, -1, 6, -1, 6, -1, 6, -1);
  const __m128i coeffs_cd = _mm_set_epi8(78, -19, 78, -19, 78, -19, 78, -19, 78,
                                         -19, 78, -19, 78, -19, 78, -19);
  const __m128i const64_x16 = _mm_set1_epi16(64);
  const __m128i ab = _mm_unpacklo_epi8(*a, *b);
  const __m128i cd = _mm_unpacklo_epi8(*c, *d);
  const __m128i fe = _mm_unpacklo_epi8(*f, *e);
  const __m128i hg = _mm_unpacklo_epi8(*h, *g);
  const __m128i ab_terms = _mm_maddubs_epi16(ab, coeffs_ab);
  const __m128i cd_terms = _mm_maddubs_epi16(cd, coeffs_cd);
  const __m128i fe_terms = _mm_maddubs_epi16(fe, coeffs_cd);
  const __m128i hg_terms = _mm_maddubs_epi16(hg, coeffs_ab);
  // can not overflow
  const __m128i abcd_terms = _mm_add_epi16(ab_terms, cd_terms);
  // can not overflow
  const __m128i fehg_terms = _mm_add_epi16(fe_terms, hg_terms);
  // can overflow, use saturating add
  const __m128i terms = _mm_adds_epi16(abcd_terms, fehg_terms);
  const __m128i round = _mm_adds_epi16(terms, const64_x16);
  const __m128i shift = _mm_srai_epi16(round, 7);
  return _mm_packus_epi16(shift, shift);
}

static void eight_tap_row_ssse3(const uint8_t *src, uint8_t *dst, int w) {
  const int max_width = w & ~7;
  int x = 0;
  for (; x < max_width; x += 8) {
    const __m128i a = _mm_loadl_epi64((const __m128i *)(src + x + 0));
    const __m128i b = _mm_loadl_epi64((const __m128i *)(src + x + 1));
    const __m128i c = _mm_loadl_epi64((const __m128i *)(src + x + 2));
    const __m128i d = _mm_loadl_epi64((const __m128i *)(src + x + 3));
    const __m128i e = _mm_loadl_epi64((const __m128i *)(src + x + 4));
    const __m128i f = _mm_loadl_epi64((const __m128i *)(src + x + 5));
    const __m128i g = _mm_loadl_epi64((const __m128i *)(src + x + 6));
    const __m128i h = _mm_loadl_epi64((const __m128i *)(src + x + 7));
    const __m128i pack = filter(&a, &b, &c, &d, &e, &f, &g, &h);
    _mm_storel_epi64((__m128i *)(dst + x), pack);
  }
}

static void upsample_1_to_2_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                                  uint8_t *dst, ptrdiff_t dst_stride, int dst_w,
                                  int dst_h) {
  dst_w /= 2;
  dst_h /= 2;
  {
    DECLARE_ALIGNED(16, uint8_t, tmp[1920 * 8]);
    uint8_t *tmp0 = tmp + dst_w * 0;
    uint8_t *tmp1 = tmp + dst_w * 1;
    uint8_t *tmp2 = tmp + dst_w * 2;
    uint8_t *tmp3 = tmp + dst_w * 3;
    uint8_t *tmp4 = tmp + dst_w * 4;
    uint8_t *tmp5 = tmp + dst_w * 5;
    uint8_t *tmp6 = tmp + dst_w * 6;
    uint8_t *tmp7 = tmp + dst_w * 7;
    uint8_t *tmp8 = NULL;
    const int max_width = dst_w & ~7;
    int y;
    eight_tap_row_ssse3(src - src_stride * 3 - 3, tmp0, dst_w);
    eight_tap_row_ssse3(src - src_stride * 2 - 3, tmp1, dst_w);
    eight_tap_row_ssse3(src - src_stride * 1 - 3, tmp2, dst_w);
    eight_tap_row_ssse3(src + src_stride * 0 - 3, tmp3, dst_w);
    eight_tap_row_ssse3(src + src_stride * 1 - 3, tmp4, dst_w);
    eight_tap_row_ssse3(src + src_stride * 2 - 3, tmp5, dst_w);
    eight_tap_row_ssse3(src + src_stride * 3 - 3, tmp6, dst_w);
    for (y = 0; y < dst_h; y++) {
      int x;
      eight_tap_row_ssse3(src + src_stride * 4 - 3, tmp7, dst_w);
      for (x = 0; x < max_width; x += 8) {
        const __m128i A = _mm_loadl_epi64((const __m128i *)(src + x));
        const __m128i B = _mm_loadl_epi64((const __m128i *)(tmp3 + x));
        const __m128i AB = _mm_unpacklo_epi8(A, B);
        __m128i C, D, CD;
        _mm_storeu_si128((__m128i *)(dst + x * 2), AB);
        {
          const __m128i a =
              _mm_loadl_epi64((const __m128i *)(src + x - src_stride * 3));
          const __m128i b =
              _mm_loadl_epi64((const __m128i *)(src + x - src_stride * 2));
          const __m128i c =
              _mm_loadl_epi64((const __m128i *)(src + x - src_stride * 1));
          const __m128i d =
              _mm_loadl_epi64((const __m128i *)(src + x + src_stride * 0));
          const __m128i e =
              _mm_loadl_epi64((const __m128i *)(src + x + src_stride * 1));
          const __m128i f =
              _mm_loadl_epi64((const __m128i *)(src + x + src_stride * 2));
          const __m128i g =
              _mm_loadl_epi64((const __m128i *)(src + x + src_stride * 3));
          const __m128i h =
              _mm_loadl_epi64((const __m128i *)(src + x + src_stride * 4));
          C = filter(&a, &b, &c, &d, &e, &f, &g, &h);
        }
        {
          const __m128i a = _mm_loadl_epi64((const __m128i *)(tmp0 + x));
          const __m128i b = _mm_loadl_epi64((const __m128i *)(tmp1 + x));
          const __m128i c = _mm_loadl_epi64((const __m128i *)(tmp2 + x));
          const __m128i d = _mm_loadl_epi64((const __m128i *)(tmp3 + x));
          const __m128i e = _mm_loadl_epi64((const __m128i *)(tmp4 + x));
          const __m128i f = _mm_loadl_epi64((const __m128i *)(tmp5 + x));
          const __m128i g = _mm_loadl_epi64((const __m128i *)(tmp6 + x));
          const __m128i h = _mm_loadl_epi64((const __m128i *)(tmp7 + x));
          D = filter(&a, &b, &c, &d, &e, &f, &g, &h);
        }
        CD = _mm_unpacklo_epi8(C, D);
        _mm_storeu_si128((__m128i *)(dst + x * 2 + dst_stride), CD);
      }
      src += src_stride;
      dst += dst_stride * 2;
      tmp8 = tmp0;
      tmp0 = tmp1;
      tmp1 = tmp2;
      tmp2 = tmp3;
      tmp3 = tmp4;
      tmp4 = tmp5;
      tmp5 = tmp6;
      tmp6 = tmp7;
      tmp7 = tmp8;
    }
  }
}

void vp9_scale_and_extend_frame_ssse3(const YV12_BUFFER_CONFIG *src,
                                      YV12_BUFFER_CONFIG *dst) {
  const int src_w = src->y_crop_width;
  const int src_h = src->y_crop_height;
  const int dst_w = dst->y_crop_width;
  const int dst_h = dst->y_crop_height;
  const int dst_uv_w = dst_w / 2;
  const int dst_uv_h = dst_h / 2;

  if (dst_w * 2 == src_w && dst_h * 2 == src_h) {
    downsample_2_to_1_ssse3(src->y_buffer, src->y_stride, dst->y_buffer,
                            dst->y_stride, dst_w, dst_h);
    downsample_2_to_1_ssse3(src->u_buffer, src->uv_stride, dst->u_buffer,
                            dst->uv_stride, dst_uv_w, dst_uv_h);
    downsample_2_to_1_ssse3(src->v_buffer, src->uv_stride, dst->v_buffer,
                            dst->uv_stride, dst_uv_w, dst_uv_h);
    vpx_extend_frame_borders(dst);
  } else if (dst_w == src_w * 2 && dst_h == src_h * 2) {
    // The upsample() supports widths up to 1920 * 2.  If greater, fall back
    // to vp9_scale_and_extend_frame_c().
    if (dst_w / 2 <= 1920) {
      upsample_1_to_2_ssse3(src->y_buffer, src->y_stride, dst->y_buffer,
                            dst->y_stride, dst_w, dst_h);
      upsample_1_to_2_ssse3(src->u_buffer, src->uv_stride, dst->u_buffer,
                            dst->uv_stride, dst_uv_w, dst_uv_h);
      upsample_1_to_2_ssse3(src->v_buffer, src->uv_stride, dst->v_buffer,
                            dst->uv_stride, dst_uv_w, dst_uv_h);
      vpx_extend_frame_borders(dst);
    } else {
      vp9_scale_and_extend_frame_c(src, dst);
    }
  } else {
    vp9_scale_and_extend_frame_c(src, dst);
  }
}
