/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <assert.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_ports/mem.h"

static INLINE void load_4x4(const int16_t *s, ptrdiff_t p, int16x4_t *s0,
                            int16x4_t *s1, int16x4_t *s2, int16x4_t *s3) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
}

static INLINE void load_8x4(const uint16_t *s, ptrdiff_t p, uint16x8_t *s0,
                            uint16x8_t *s1, uint16x8_t *s2, uint16x8_t *s3) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
}

static INLINE void load_8x8(const int16_t *s, ptrdiff_t p, int16x8_t *s0,
                            int16x8_t *s1, int16x8_t *s2, int16x8_t *s3,
                            int16x8_t *s4, int16x8_t *s5, int16x8_t *s6,
                            int16x8_t *s7) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
}

static INLINE void store_8x8(uint16_t *s, ptrdiff_t p, const uint16x8_t s0,
                             const uint16x8_t s1, const uint16x8_t s2,
                             const uint16x8_t s3, const uint16x8_t s4,
                             const uint16x8_t s5, const uint16x8_t s6,
                             const uint16x8_t s7) {
  vst1q_u16(s, s0);
  s += p;
  vst1q_u16(s, s1);
  s += p;
  vst1q_u16(s, s2);
  s += p;
  vst1q_u16(s, s3);
  s += p;
  vst1q_u16(s, s4);
  s += p;
  vst1q_u16(s, s5);
  s += p;
  vst1q_u16(s, s6);
  s += p;
  vst1q_u16(s, s7);
}

static INLINE int32x4_t convolve8_4(const int16x4_t s0, const int16x4_t s1,
                                    const int16x4_t s2, const int16x4_t s3,
                                    const int16x4_t s4, const int16x4_t s5,
                                    const int16x4_t s6, const int16x4_t s7,
                                    const int16x8_t filters) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int32x4_t sum = vdupq_n_s32(0);

  sum = vmlal_lane_s16(sum, s0, filters_lo, 0);
  sum = vmlal_lane_s16(sum, s1, filters_lo, 1);
  sum = vmlal_lane_s16(sum, s2, filters_lo, 2);
  sum = vmlal_lane_s16(sum, s3, filters_lo, 3);
  sum = vmlal_lane_s16(sum, s4, filters_hi, 0);
  sum = vmlal_lane_s16(sum, s5, filters_hi, 1);
  sum = vmlal_lane_s16(sum, s6, filters_hi, 2);
  sum = vmlal_lane_s16(sum, s7, filters_hi, 3);
  return sum;
}

static INLINE uint16x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                     const int16x8_t s2, const int16x8_t s3,
                                     const int16x8_t s4, const int16x8_t s5,
                                     const int16x8_t s6, const int16x8_t s7,
                                     const int16x8_t filters,
                                     const uint16x8_t max) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int32x4_t sum0 = vdupq_n_s32(0);
  int32x4_t sum1 = vdupq_n_s32(0);
  uint16x8_t d;

  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s0), filters_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filters_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filters_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filters_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filters_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filters_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filters_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filters_hi, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s0), filters_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filters_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filters_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filters_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filters_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filters_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filters_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filters_hi, 3);
  d = vcombine_u16(vqrshrun_n_s32(sum0, 7), vqrshrun_n_s32(sum1, 7));
  d = vminq_u16(d, max);
  return d;
}

void vpx_highbd_convolve8_horiz_neon(const uint8_t *src8, ptrdiff_t src_stride,
                                     uint8_t *dst8, ptrdiff_t dst_stride,
                                     const int16_t *filter_x, int x_step_q4,
                                     const int16_t *filter_y,  // unused
                                     int y_step_q4,            // unused
                                     int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_horiz_c(src8, src_stride, dst8, dst_stride, filter_x,
                                 x_step_q4, filter_y, y_step_q4, w, h, bd);
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    const int16x8_t filters = vld1q_s16(filter_x);
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    uint16x8_t t0, t1, t2, t3;

    assert(!((intptr_t)dst & 3));
    assert(!(dst_stride & 3));

    src -= 3;

    if (h == 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      int32x4_t d0, d1, d2, d3;
      uint16x8_t d01, d23;

      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      load_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u16_8x4(&t0, &t1, &t2, &t3);
      s0 = vreinterpret_s16_u16(vget_low_u16(t0));
      s1 = vreinterpret_s16_u16(vget_low_u16(t1));
      s2 = vreinterpret_s16_u16(vget_low_u16(t2));
      s3 = vreinterpret_s16_u16(vget_low_u16(t3));
      s4 = vreinterpret_s16_u16(vget_high_u16(t0));
      s5 = vreinterpret_s16_u16(vget_high_u16(t1));
      s6 = vreinterpret_s16_u16(vget_high_u16(t2));
      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      src += 7;

      do {
        load_4x4((const int16_t *)src, src_stride, &s7, &s8, &s9, &s10);
        transpose_s16_4x4d(&s7, &s8, &s9, &s10);

        d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        d01 = vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
        d23 = vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));
        d01 = vminq_u16(d01, max);
        d23 = vminq_u16(d23, max);
        transpose_u16_4x4q(&d01, &d23);

        vst1_u16(dst + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(dst + 1 * dst_stride, vget_low_u16(d23));
        vst1_u16(dst + 2 * dst_stride, vget_high_u16(d01));
        vst1_u16(dst + 3 * dst_stride, vget_high_u16(d23));

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        src += 4;
        dst += 4;
        w -= 4;
      } while (w > 0);
    } else {
      int16x8_t t4, t5, t6, t7;
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      uint16x8_t d0, d1, d2, d3;

      if (w == 4) {
        do {
          load_8x8((const int16_t *)src, src_stride, &s0, &s1, &s2, &s3, &s4,
                   &s5, &s6, &s7);
          transpose_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

          load_8x8((const int16_t *)(src + 7), src_stride, &s7, &s8, &s9, &s10,
                   &t4, &t5, &t6, &t7);
          src += 8 * src_stride;
          __builtin_prefetch(dst + 0 * dst_stride);
          __builtin_prefetch(dst + 1 * dst_stride);
          __builtin_prefetch(dst + 2 * dst_stride);
          __builtin_prefetch(dst + 3 * dst_stride);
          __builtin_prefetch(dst + 4 * dst_stride);
          __builtin_prefetch(dst + 5 * dst_stride);
          __builtin_prefetch(dst + 6 * dst_stride);
          __builtin_prefetch(dst + 7 * dst_stride);
          transpose_s16_8x8(&s7, &s8, &s9, &s10, &t4, &t5, &t6, &t7);

          __builtin_prefetch(src + 0 * src_stride);
          __builtin_prefetch(src + 1 * src_stride);
          __builtin_prefetch(src + 2 * src_stride);
          __builtin_prefetch(src + 3 * src_stride);
          __builtin_prefetch(src + 4 * src_stride);
          __builtin_prefetch(src + 5 * src_stride);
          __builtin_prefetch(src + 6 * src_stride);
          __builtin_prefetch(src + 7 * src_stride);
          d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
          d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
          d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
          d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);

          transpose_u16_8x4(&d0, &d1, &d2, &d3);
          vst1_u16(dst, vget_low_u16(d0));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d1));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d2));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d3));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d0));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d1));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d2));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d3));
          dst += dst_stride;
          h -= 8;
        } while (h > 0);
      } else {
        int width;
        const uint16_t *s;
        uint16_t *d;
        int16x8_t s11, s12, s13, s14;
        uint16x8_t d4, d5, d6, d7;

        do {
          __builtin_prefetch(src + 0 * src_stride);
          __builtin_prefetch(src + 1 * src_stride);
          __builtin_prefetch(src + 2 * src_stride);
          __builtin_prefetch(src + 3 * src_stride);
          __builtin_prefetch(src + 4 * src_stride);
          __builtin_prefetch(src + 5 * src_stride);
          __builtin_prefetch(src + 6 * src_stride);
          __builtin_prefetch(src + 7 * src_stride);
          load_8x8((const int16_t *)src, src_stride, &s0, &s1, &s2, &s3, &s4,
                   &s5, &s6, &s7);
          transpose_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

          width = w;
          s = src + 7;
          d = dst;
          __builtin_prefetch(dst + 0 * dst_stride);
          __builtin_prefetch(dst + 1 * dst_stride);
          __builtin_prefetch(dst + 2 * dst_stride);
          __builtin_prefetch(dst + 3 * dst_stride);
          __builtin_prefetch(dst + 4 * dst_stride);
          __builtin_prefetch(dst + 5 * dst_stride);
          __builtin_prefetch(dst + 6 * dst_stride);
          __builtin_prefetch(dst + 7 * dst_stride);

          do {
            load_8x8((const int16_t *)s, src_stride, &s7, &s8, &s9, &s10, &s11,
                     &s12, &s13, &s14);
            transpose_s16_8x8(&s7, &s8, &s9, &s10, &s11, &s12, &s13, &s14);

            d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
            d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
            d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
            d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);
            d4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters, max);
            d5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters, max);
            d6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters, max);
            d7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters, max);

            transpose_u16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);
            store_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

            s0 = s8;
            s1 = s9;
            s2 = s10;
            s3 = s11;
            s4 = s12;
            s5 = s13;
            s6 = s14;
            s += 8;
            d += 8;
            width -= 8;
          } while (width > 0);
          src += 8 * src_stride;
          dst += 8 * dst_stride;
          h -= 8;
        } while (h > 0);
      }
    }
  }
}

void vpx_highbd_convolve8_avg_horiz_neon(const uint8_t *src8,
                                         ptrdiff_t src_stride, uint8_t *dst8,
                                         ptrdiff_t dst_stride,
                                         const int16_t *filter_x, int x_step_q4,
                                         const int16_t *filter_y,  // unused
                                         int y_step_q4,            // unused
                                         int w, int h, int bd) {
  if (x_step_q4 != 16) {
    vpx_highbd_convolve8_avg_horiz_c(src8, src_stride, dst8, dst_stride,
                                     filter_x, x_step_q4, filter_y, y_step_q4,
                                     w, h, bd);
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    const int16x8_t filters = vld1q_s16(filter_x);
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);
    uint16x8_t t0, t1, t2, t3;

    assert(!((intptr_t)dst & 3));
    assert(!(dst_stride & 3));

    src -= 3;

    if (h == 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      int32x4_t d0, d1, d2, d3;
      uint16x8_t d01, d23, t01, t23;

      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      load_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u16_8x4(&t0, &t1, &t2, &t3);
      s0 = vreinterpret_s16_u16(vget_low_u16(t0));
      s1 = vreinterpret_s16_u16(vget_low_u16(t1));
      s2 = vreinterpret_s16_u16(vget_low_u16(t2));
      s3 = vreinterpret_s16_u16(vget_low_u16(t3));
      s4 = vreinterpret_s16_u16(vget_high_u16(t0));
      s5 = vreinterpret_s16_u16(vget_high_u16(t1));
      s6 = vreinterpret_s16_u16(vget_high_u16(t2));
      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      src += 7;

      do {
        load_4x4((const int16_t *)src, src_stride, &s7, &s8, &s9, &s10);
        transpose_s16_4x4d(&s7, &s8, &s9, &s10);

        d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        t01 = vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
        t23 = vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));
        t01 = vminq_u16(t01, max);
        t23 = vminq_u16(t23, max);
        transpose_u16_4x4q(&t01, &t23);

        d01 = vcombine_u16(vld1_u16(dst + 0 * dst_stride),
                           vld1_u16(dst + 2 * dst_stride));
        d23 = vcombine_u16(vld1_u16(dst + 1 * dst_stride),
                           vld1_u16(dst + 3 * dst_stride));
        d01 = vrhaddq_u16(d01, t01);
        d23 = vrhaddq_u16(d23, t23);

        vst1_u16(dst + 0 * dst_stride, vget_low_u16(d01));
        vst1_u16(dst + 1 * dst_stride, vget_low_u16(d23));
        vst1_u16(dst + 2 * dst_stride, vget_high_u16(d01));
        vst1_u16(dst + 3 * dst_stride, vget_high_u16(d23));

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        src += 4;
        dst += 4;
        w -= 4;
      } while (w > 0);
    } else {
      int16x8_t t4, t5, t6, t7;
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      uint16x8_t d0, d1, d2, d3, t0, t1, t2, t3;

      if (w == 4) {
        do {
          load_8x8((const int16_t *)src, src_stride, &s0, &s1, &s2, &s3, &s4,
                   &s5, &s6, &s7);
          transpose_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

          load_8x8((const int16_t *)(src + 7), src_stride, &s7, &s8, &s9, &s10,
                   &t4, &t5, &t6, &t7);
          src += 8 * src_stride;
          __builtin_prefetch(dst + 0 * dst_stride);
          __builtin_prefetch(dst + 1 * dst_stride);
          __builtin_prefetch(dst + 2 * dst_stride);
          __builtin_prefetch(dst + 3 * dst_stride);
          __builtin_prefetch(dst + 4 * dst_stride);
          __builtin_prefetch(dst + 5 * dst_stride);
          __builtin_prefetch(dst + 6 * dst_stride);
          __builtin_prefetch(dst + 7 * dst_stride);
          transpose_s16_8x8(&s7, &s8, &s9, &s10, &t4, &t5, &t6, &t7);

          __builtin_prefetch(src + 0 * src_stride);
          __builtin_prefetch(src + 1 * src_stride);
          __builtin_prefetch(src + 2 * src_stride);
          __builtin_prefetch(src + 3 * src_stride);
          __builtin_prefetch(src + 4 * src_stride);
          __builtin_prefetch(src + 5 * src_stride);
          __builtin_prefetch(src + 6 * src_stride);
          __builtin_prefetch(src + 7 * src_stride);
          t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
          t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
          t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
          t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);
          transpose_u16_8x4(&t0, &t1, &t2, &t3);

          d0 = vcombine_u16(vld1_u16(dst + 0 * dst_stride),
                            vld1_u16(dst + 4 * dst_stride));
          d1 = vcombine_u16(vld1_u16(dst + 1 * dst_stride),
                            vld1_u16(dst + 5 * dst_stride));
          d2 = vcombine_u16(vld1_u16(dst + 2 * dst_stride),
                            vld1_u16(dst + 6 * dst_stride));
          d3 = vcombine_u16(vld1_u16(dst + 3 * dst_stride),
                            vld1_u16(dst + 7 * dst_stride));
          d0 = vrhaddq_u16(d0, t0);
          d1 = vrhaddq_u16(d1, t1);
          d2 = vrhaddq_u16(d2, t2);
          d3 = vrhaddq_u16(d3, t3);

          vst1_u16(dst, vget_low_u16(d0));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d1));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d2));
          dst += dst_stride;
          vst1_u16(dst, vget_low_u16(d3));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d0));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d1));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d2));
          dst += dst_stride;
          vst1_u16(dst, vget_high_u16(d3));
          dst += dst_stride;
          h -= 8;
        } while (h > 0);
      } else {
        int width;
        const uint16_t *s;
        uint16_t *d;
        int16x8_t s11, s12, s13, s14;
        uint16x8_t d4, d5, d6, d7;

        do {
          __builtin_prefetch(src + 0 * src_stride);
          __builtin_prefetch(src + 1 * src_stride);
          __builtin_prefetch(src + 2 * src_stride);
          __builtin_prefetch(src + 3 * src_stride);
          __builtin_prefetch(src + 4 * src_stride);
          __builtin_prefetch(src + 5 * src_stride);
          __builtin_prefetch(src + 6 * src_stride);
          __builtin_prefetch(src + 7 * src_stride);
          load_8x8((const int16_t *)src, src_stride, &s0, &s1, &s2, &s3, &s4,
                   &s5, &s6, &s7);
          transpose_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

          width = w;
          s = src + 7;
          d = dst;
          __builtin_prefetch(dst + 0 * dst_stride);
          __builtin_prefetch(dst + 1 * dst_stride);
          __builtin_prefetch(dst + 2 * dst_stride);
          __builtin_prefetch(dst + 3 * dst_stride);
          __builtin_prefetch(dst + 4 * dst_stride);
          __builtin_prefetch(dst + 5 * dst_stride);
          __builtin_prefetch(dst + 6 * dst_stride);
          __builtin_prefetch(dst + 7 * dst_stride);

          do {
            load_8x8((const int16_t *)s, src_stride, &s7, &s8, &s9, &s10, &s11,
                     &s12, &s13, &s14);
            transpose_s16_8x8(&s7, &s8, &s9, &s10, &s11, &s12, &s13, &s14);

            d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
            d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
            d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
            d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);
            d4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, filters, max);
            d5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, filters, max);
            d6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, filters, max);
            d7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, filters, max);

            transpose_u16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

            d0 = vrhaddq_u16(d0, vld1q_u16(d + 0 * dst_stride));
            d1 = vrhaddq_u16(d1, vld1q_u16(d + 1 * dst_stride));
            d2 = vrhaddq_u16(d2, vld1q_u16(d + 2 * dst_stride));
            d3 = vrhaddq_u16(d3, vld1q_u16(d + 3 * dst_stride));
            d4 = vrhaddq_u16(d4, vld1q_u16(d + 4 * dst_stride));
            d5 = vrhaddq_u16(d5, vld1q_u16(d + 5 * dst_stride));
            d6 = vrhaddq_u16(d6, vld1q_u16(d + 6 * dst_stride));
            d7 = vrhaddq_u16(d7, vld1q_u16(d + 7 * dst_stride));

            store_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

            s0 = s8;
            s1 = s9;
            s2 = s10;
            s3 = s11;
            s4 = s12;
            s5 = s13;
            s6 = s14;
            s += 8;
            d += 8;
            width -= 8;
          } while (width > 0);
          src += 8 * src_stride;
          dst += 8 * dst_stride;
          h -= 8;
        } while (h > 0);
      }
    }
  }
}

void vpx_highbd_convolve8_vert_neon(const uint8_t *src8, ptrdiff_t src_stride,
                                    uint8_t *dst8, ptrdiff_t dst_stride,
                                    const int16_t *filter_x,  // unused
                                    int x_step_q4,            // unused
                                    const int16_t *filter_y, int y_step_q4,
                                    int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_vert_c(src8, src_stride, dst8, dst_stride, filter_x,
                                x_step_q4, filter_y, y_step_q4, w, h, bd);
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    const int16x8_t filters = vld1q_s16(filter_y);
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    assert(!((intptr_t)dst & 3));
    assert(!(dst_stride & 3));

    src -= 3 * src_stride;

    if (w == 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      int32x4_t d0, d1, d2, d3;
      uint16x8_t d01, d23;

      s0 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s1 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s2 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s3 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s4 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s5 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s6 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;

      do {
        s7 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s8 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s9 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s10 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;

        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        d01 = vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
        d23 = vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));
        d01 = vminq_u16(d01, max);
        d23 = vminq_u16(d23, max);
        vst1_u16(dst, vget_low_u16(d01));
        dst += dst_stride;
        vst1_u16(dst, vget_high_u16(d01));
        dst += dst_stride;
        vst1_u16(dst, vget_low_u16(d23));
        dst += dst_stride;
        vst1_u16(dst, vget_high_u16(d23));
        dst += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        h -= 4;
      } while (h > 0);
    } else {
      int height;
      const uint16_t *s;
      uint16_t *d;
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      uint16x8_t d0, d1, d2, d3;

      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        s = src;
        s0 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s1 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s2 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s3 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s4 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s5 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s6 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        d = dst;
        height = h;

        do {
          s7 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s8 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s9 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s10 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;

          __builtin_prefetch(d + 0 * dst_stride);
          __builtin_prefetch(d + 1 * dst_stride);
          __builtin_prefetch(d + 2 * dst_stride);
          __builtin_prefetch(d + 3 * dst_stride);
          __builtin_prefetch(s + 0 * src_stride);
          __builtin_prefetch(s + 1 * src_stride);
          __builtin_prefetch(s + 2 * src_stride);
          __builtin_prefetch(s + 3 * src_stride);
          d0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
          d1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
          d2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
          d3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);

          vst1q_u16(d, d0);
          d += dst_stride;
          vst1q_u16(d, d1);
          d += dst_stride;
          vst1q_u16(d, d2);
          d += dst_stride;
          vst1q_u16(d, d3);
          d += dst_stride;

          s0 = s4;
          s1 = s5;
          s2 = s6;
          s3 = s7;
          s4 = s8;
          s5 = s9;
          s6 = s10;
          height -= 4;
        } while (height > 0);
        src += 8;
        dst += 8;
        w -= 8;
      } while (w > 0);
    }
  }
}

void vpx_highbd_convolve8_avg_vert_neon(const uint8_t *src8,
                                        ptrdiff_t src_stride, uint8_t *dst8,
                                        ptrdiff_t dst_stride,
                                        const int16_t *filter_x,  // unused
                                        int x_step_q4,            // unused
                                        const int16_t *filter_y, int y_step_q4,
                                        int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_avg_vert_c(src8, src_stride, dst8, dst_stride,
                                    filter_x, x_step_q4, filter_y, y_step_q4, w,
                                    h, bd);
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
    const int16x8_t filters = vld1q_s16(filter_y);
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    assert(!((intptr_t)dst & 3));
    assert(!(dst_stride & 3));

    src -= 3 * src_stride;

    if (w == 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      int32x4_t d0, d1, d2, d3;
      uint16x8_t d01, d23, t01, t23;

      s0 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s1 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s2 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s3 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s4 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s5 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;
      s6 = vreinterpret_s16_u16(vld1_u16(src));
      src += src_stride;

      do {
        s7 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s8 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s9 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;
        s10 = vreinterpret_s16_u16(vld1_u16(src));
        src += src_stride;

        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        d0 = convolve8_4(s0, s1, s2, s3, s4, s5, s6, s7, filters);
        d1 = convolve8_4(s1, s2, s3, s4, s5, s6, s7, s8, filters);
        d2 = convolve8_4(s2, s3, s4, s5, s6, s7, s8, s9, filters);
        d3 = convolve8_4(s3, s4, s5, s6, s7, s8, s9, s10, filters);

        t01 = vcombine_u16(vqrshrun_n_s32(d0, 7), vqrshrun_n_s32(d1, 7));
        t23 = vcombine_u16(vqrshrun_n_s32(d2, 7), vqrshrun_n_s32(d3, 7));
        t01 = vminq_u16(t01, max);
        t23 = vminq_u16(t23, max);

        d01 = vcombine_u16(vld1_u16(dst + 0 * dst_stride),
                           vld1_u16(dst + 1 * dst_stride));
        d23 = vcombine_u16(vld1_u16(dst + 2 * dst_stride),
                           vld1_u16(dst + 3 * dst_stride));
        d01 = vrhaddq_u16(d01, t01);
        d23 = vrhaddq_u16(d23, t23);

        vst1_u16(dst, vget_low_u16(d01));
        dst += dst_stride;
        vst1_u16(dst, vget_high_u16(d01));
        dst += dst_stride;
        vst1_u16(dst, vget_low_u16(d23));
        dst += dst_stride;
        vst1_u16(dst, vget_high_u16(d23));
        dst += dst_stride;

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        h -= 4;
      } while (h > 0);
    } else {
      int height;
      const uint16_t *s;
      uint16_t *d;
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      uint16x8_t d0, d1, d2, d3, t0, t1, t2, t3;

      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        s = src;
        s0 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s1 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s2 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s3 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s4 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s5 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        s6 = vreinterpretq_s16_u16(vld1q_u16(s));
        s += src_stride;
        d = dst;
        height = h;

        do {
          s7 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s8 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s9 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;
          s10 = vreinterpretq_s16_u16(vld1q_u16(s));
          s += src_stride;

          __builtin_prefetch(d + 0 * dst_stride);
          __builtin_prefetch(d + 1 * dst_stride);
          __builtin_prefetch(d + 2 * dst_stride);
          __builtin_prefetch(d + 3 * dst_stride);
          __builtin_prefetch(s + 0 * src_stride);
          __builtin_prefetch(s + 1 * src_stride);
          __builtin_prefetch(s + 2 * src_stride);
          __builtin_prefetch(s + 3 * src_stride);
          t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, filters, max);
          t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, filters, max);
          t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, filters, max);
          t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, filters, max);

          d0 = vld1q_u16(d + 0 * dst_stride);
          d1 = vld1q_u16(d + 1 * dst_stride);
          d2 = vld1q_u16(d + 2 * dst_stride);
          d3 = vld1q_u16(d + 3 * dst_stride);
          d0 = vrhaddq_u16(d0, t0);
          d1 = vrhaddq_u16(d1, t1);
          d2 = vrhaddq_u16(d2, t2);
          d3 = vrhaddq_u16(d3, t3);

          vst1q_u16(d, d0);
          d += dst_stride;
          vst1q_u16(d, d1);
          d += dst_stride;
          vst1q_u16(d, d2);
          d += dst_stride;
          vst1q_u16(d, d3);
          d += dst_stride;

          s0 = s4;
          s1 = s5;
          s2 = s6;
          s3 = s7;
          s4 = s8;
          s5 = s9;
          s6 = s10;
          height -= 4;
        } while (height > 0);
        src += 8;
        dst += 8;
        w -= 8;
      } while (w > 0);
    }
  }
}
