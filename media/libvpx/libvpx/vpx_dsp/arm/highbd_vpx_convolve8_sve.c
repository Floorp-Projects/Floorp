/*
 *  Copyright (c) 2024 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"

#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/arm/vpx_neon_sve_bridge.h"

DECLARE_ALIGNED(16, static const uint16_t, kTblConv4_8[8]) = { 0, 2, 4, 6,
                                                               1, 3, 5, 7 };

static INLINE uint16x4_t highbd_convolve4_4(const int16x4_t s[4],
                                            const int16x8_t filter,
                                            const uint16x4_t max) {
  int16x8_t s01 = vcombine_s16(s[0], s[1]);
  int16x8_t s23 = vcombine_s16(s[2], s[3]);

  int64x2_t sum01 = vpx_dotq_lane_s16(vdupq_n_s64(0), s01, filter, 0);
  int64x2_t sum23 = vpx_dotq_lane_s16(vdupq_n_s64(0), s23, filter, 0);

  int32x4_t res_s32 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));

  uint16x4_t res_u16 = vqrshrun_n_s32(res_s32, FILTER_BITS);
  return vmin_u16(res_u16, max);
}

static INLINE uint16x8_t highbd_convolve4_8(const int16x8_t s[4],
                                            const int16x8_t filter,
                                            const uint16x8_t max,
                                            uint16x8_t idx) {
  int64x2_t sum04 = vpx_dotq_lane_s16(vdupq_n_s64(0), s[0], filter, 0);
  int64x2_t sum15 = vpx_dotq_lane_s16(vdupq_n_s64(0), s[1], filter, 0);
  int64x2_t sum26 = vpx_dotq_lane_s16(vdupq_n_s64(0), s[2], filter, 0);
  int64x2_t sum37 = vpx_dotq_lane_s16(vdupq_n_s64(0), s[3], filter, 0);

  int32x4_t res0 = vcombine_s32(vmovn_s64(sum04), vmovn_s64(sum15));
  int32x4_t res1 = vcombine_s32(vmovn_s64(sum26), vmovn_s64(sum37));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(res0, FILTER_BITS),
                                vqrshrun_n_s32(res1, FILTER_BITS));

  res = vpx_tbl_u16(res, idx);

  return vminq_u16(res, max);
}

static INLINE uint16x4_t highbd_convolve8_4(const int16x8_t s[4],
                                            const int16x8_t filter,
                                            const uint16x4_t max) {
  int64x2_t sum[4];

  sum[0] = vpx_dotq_s16(vdupq_n_s64(0), s[0], filter);
  sum[1] = vpx_dotq_s16(vdupq_n_s64(0), s[1], filter);
  sum[2] = vpx_dotq_s16(vdupq_n_s64(0), s[2], filter);
  sum[3] = vpx_dotq_s16(vdupq_n_s64(0), s[3], filter);

  sum[0] = vpaddq_s64(sum[0], sum[1]);
  sum[2] = vpaddq_s64(sum[2], sum[3]);

  int32x4_t res_s32 = vcombine_s32(vmovn_s64(sum[0]), vmovn_s64(sum[2]));

  uint16x4_t res_u16 = vqrshrun_n_s32(res_s32, FILTER_BITS);
  return vmin_u16(res_u16, max);
}

static INLINE uint16x8_t highbd_convolve8_8(const int16x8_t s[8],
                                            const int16x8_t filter,
                                            const uint16x8_t max) {
  int64x2_t sum[8];

  sum[0] = vpx_dotq_s16(vdupq_n_s64(0), s[0], filter);
  sum[1] = vpx_dotq_s16(vdupq_n_s64(0), s[1], filter);
  sum[2] = vpx_dotq_s16(vdupq_n_s64(0), s[2], filter);
  sum[3] = vpx_dotq_s16(vdupq_n_s64(0), s[3], filter);
  sum[4] = vpx_dotq_s16(vdupq_n_s64(0), s[4], filter);
  sum[5] = vpx_dotq_s16(vdupq_n_s64(0), s[5], filter);
  sum[6] = vpx_dotq_s16(vdupq_n_s64(0), s[6], filter);
  sum[7] = vpx_dotq_s16(vdupq_n_s64(0), s[7], filter);

  int64x2_t sum01 = vpaddq_s64(sum[0], sum[1]);
  int64x2_t sum23 = vpaddq_s64(sum[2], sum[3]);
  int64x2_t sum45 = vpaddq_s64(sum[4], sum[5]);
  int64x2_t sum67 = vpaddq_s64(sum[6], sum[7]);

  int32x4_t res0 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t res1 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(res0, FILTER_BITS),
                                vqrshrun_n_s32(res1, FILTER_BITS));
  return vminq_u16(res, max);
}

static INLINE void highbd_convolve_4tap_horiz_sve(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16x4_t filters, int bd) {
  const int16x8_t filter = vcombine_s16(filters, vdup_n_s16(0));

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x4_t s0[4], s1[4], s2[4], s3[4];
      load_s16_4x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_4x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_4x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_4x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve4_4(s0, filter, max);
      uint16x4_t d1 = highbd_convolve4_4(s1, filter, max);
      uint16x4_t d2 = highbd_convolve4_4(s2, filter, max);
      uint16x4_t d3 = highbd_convolve4_4(s3, filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    const uint16x8_t idx = vld1q_u16(kTblConv4_8);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int width = w;

      do {
        int16x8_t s0[4], s1[4], s2[4], s3[4];
        load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
        load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
        load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
        load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

        uint16x8_t d0 = highbd_convolve4_8(s0, filter, max, idx);
        uint16x8_t d1 = highbd_convolve4_8(s1, filter, max, idx);
        uint16x8_t d2 = highbd_convolve4_8(s2, filter, max, idx);
        uint16x8_t d3 = highbd_convolve4_8(s3, filter, max, idx);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  }
}

static INLINE void highbd_convolve_8tap_horiz_sve(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16x8_t filters, int bd) {
  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x8_t s0[4], s1[4], s2[4], s3[4];
      load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve8_4(s0, filters, max);
      uint16x4_t d1 = highbd_convolve8_4(s1, filters, max);
      uint16x4_t d2 = highbd_convolve8_4(s2, filters, max);
      uint16x4_t d3 = highbd_convolve8_4(s3, filters, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int width = w;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8(s0, filters, max);
        uint16x8_t d1 = highbd_convolve8_8(s1, filters, max);
        uint16x8_t d2 = highbd_convolve8_8(s2, filters, max);
        uint16x8_t d3 = highbd_convolve8_8(s3, filters, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  }
}

void vpx_highbd_convolve8_horiz_sve(const uint16_t *src, ptrdiff_t src_stride,
                                    uint16_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *filter, int x0_q4,
                                    int x_step_q4, int y0_q4, int y_step_q4,
                                    int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter,
                                 x0_q4, x_step_q4, y0_q4, y_step_q4, w, h, bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(x_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  if (vpx_get_filter_taps(filter[x0_q4]) <= 4) {
    const int16x4_t x_filter_4tap = vld1_s16(filter[x0_q4] + 2);
    highbd_convolve_4tap_horiz_sve(src - 1, src_stride, dst, dst_stride, w, h,
                                   x_filter_4tap, bd);
  } else {
    const int16x8_t x_filter_8tap = vld1q_s16(filter[x0_q4]);
    highbd_convolve_8tap_horiz_sve(src - 3, src_stride, dst, dst_stride, w, h,
                                   x_filter_8tap, bd);
  }
}

void vpx_highbd_convolve8_avg_horiz_sve(const uint16_t *src,
                                        ptrdiff_t src_stride, uint16_t *dst,
                                        ptrdiff_t dst_stride,
                                        const InterpKernel *filter, int x0_q4,
                                        int x_step_q4, int y0_q4, int y_step_q4,
                                        int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_avg_horiz_c(src, src_stride, dst, dst_stride, filter,
                                     x0_q4, x_step_q4, y0_q4, y_step_q4, w, h,
                                     bd);
    return;
  }
  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[x0_q4]);

  src -= 3;

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    do {
      int16x8_t s0[4], s1[4], s2[4], s3[4];
      load_s16_8x4(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3]);
      load_s16_8x4(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3]);
      load_s16_8x4(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3]);
      load_s16_8x4(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3]);

      uint16x4_t d0 = highbd_convolve8_4(s0, filters, max);
      uint16x4_t d1 = highbd_convolve8_4(s1, filters, max);
      uint16x4_t d2 = highbd_convolve8_4(s2, filters, max);
      uint16x4_t d3 = highbd_convolve8_4(s3, filters, max);

      d0 = vrhadd_u16(d0, vld1_u16(d + 0 * dst_stride));
      d1 = vrhadd_u16(d1, vld1_u16(d + 1 * dst_stride));
      d2 = vrhadd_u16(d2, vld1_u16(d + 2 * dst_stride));
      d3 = vrhadd_u16(d3, vld1_u16(d + 3 * dst_stride));

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int width = w;

      do {
        int16x8_t s0[8], s1[8], s2[8], s3[8];
        load_s16_8x8(s + 0 * src_stride, 1, &s0[0], &s0[1], &s0[2], &s0[3],
                     &s0[4], &s0[5], &s0[6], &s0[7]);
        load_s16_8x8(s + 1 * src_stride, 1, &s1[0], &s1[1], &s1[2], &s1[3],
                     &s1[4], &s1[5], &s1[6], &s1[7]);
        load_s16_8x8(s + 2 * src_stride, 1, &s2[0], &s2[1], &s2[2], &s2[3],
                     &s2[4], &s2[5], &s2[6], &s2[7]);
        load_s16_8x8(s + 3 * src_stride, 1, &s3[0], &s3[1], &s3[2], &s3[3],
                     &s3[4], &s3[5], &s3[6], &s3[7]);

        uint16x8_t d0 = highbd_convolve8_8(s0, filters, max);
        uint16x8_t d1 = highbd_convolve8_8(s1, filters, max);
        uint16x8_t d2 = highbd_convolve8_8(s2, filters, max);
        uint16x8_t d3 = highbd_convolve8_8(s3, filters, max);

        d0 = vrhaddq_u16(d0, vld1q_u16(d + 0 * dst_stride));
        d1 = vrhaddq_u16(d1, vld1q_u16(d + 1 * dst_stride));
        d2 = vrhaddq_u16(d2, vld1q_u16(d + 2 * dst_stride));
        d3 = vrhaddq_u16(d3, vld1q_u16(d + 3 * dst_stride));

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  }
}
