/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"

static INLINE int16x4_t compound_convolve8_4_v(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16x8_t filter,
    const int32x4_t offset_const) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = offset_const;
  sum = vmlal_lane_s16(sum, s0, filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, filter_4_7, 3);

  return vshrn_n_s32(sum, COMPOUND_ROUND1_BITS);
}

static INLINE int16x8_t compound_convolve8_8_v(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16x8_t filter,
    const int32x4_t offset_const) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = offset_const;
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s0), filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filter_4_7, 3);

  int32x4_t sum1 = offset_const;
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s0), filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filter_4_7, 3);

  int16x4_t res0 = vshrn_n_s32(sum0, COMPOUND_ROUND1_BITS);
  int16x4_t res1 = vshrn_n_s32(sum1, COMPOUND_ROUND1_BITS);

  return vcombine_s16(res0, res1);
}

static INLINE void compound_convolve_vert_scale_neon(
    const int16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const int16_t *y_filter, int subpel_y_qn, int y_step_qn) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  // A shim of 1 << (COMPOUND_ROUND1_BITS - 1) enables us to use
  // non-rounding shifts - which are generally faster than rounding shifts on
  // modern CPUs.
  const int32x4_t vert_offset =
      vdupq_n_s32((1 << offset_bits) + (1 << (COMPOUND_ROUND1_BITS - 1)));

  int y_qn = subpel_y_qn;

  if (w == 4) {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      int16x4_t d0 = compound_convolve8_4_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                            filter, vert_offset);

      vst1_u16(dst, vreinterpret_u16_s16(d0));

      dst += dst_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  } else {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int width = w;
      uint16_t *d = dst;

      do {
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        int16x8_t d0 = compound_convolve8_8_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                              filter, vert_offset);

        vst1q_u16(d, vreinterpretq_u16_s16(d0));

        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);

      dst += dst_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  }
}

static INLINE void compound_avg_convolve_vert_scale_neon(
    const int16_t *src, int src_stride, uint8_t *dst8, int dst8_stride,
    uint16_t *dst16, int dst16_stride, int w, int h, const int16_t *y_filter,
    int subpel_y_qn, int y_step_qn) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  // A shim of 1 << (COMPOUND_ROUND1_BITS - 1) enables us to use
  // non-rounding shifts - which are generally faster than rounding shifts
  // on modern CPUs.
  const int32_t vert_offset_bits =
      (1 << offset_bits) + (1 << (COMPOUND_ROUND1_BITS - 1));
  // For the averaging code path substract round offset and convolve round.
  const int32_t avg_offset_bits = (1 << (offset_bits + 1)) + (1 << offset_bits);
  const int32x4_t vert_offset = vdupq_n_s32(vert_offset_bits - avg_offset_bits);

  int y_qn = subpel_y_qn;

  if (w == 4) {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      int16x4_t d0 = compound_convolve8_4_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                            filter, vert_offset);

      int16x4_t dd0 = vreinterpret_s16_u16(vld1_u16(dst16));

      int16x4_t avg = vhadd_s16(dd0, d0);
      int16x8_t d0_s16 = vcombine_s16(avg, vdup_n_s16(0));

      uint8x8_t d0_u8 = vqrshrun_n_s16(
          d0_s16, (2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS));

      store_u8_4x1(dst8, d0_u8);

      dst16 += dst16_stride;
      dst8 += dst8_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  } else {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int width = w;
      uint8_t *dst8_ptr = dst8;
      uint16_t *dst16_ptr = dst16;

      do {
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        int16x8_t d0 = compound_convolve8_8_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                              filter, vert_offset);

        int16x8_t dd0 = vreinterpretq_s16_u16(vld1q_u16(dst16_ptr));

        int16x8_t avg = vhaddq_s16(dd0, d0);

        uint8x8_t d0_u8 = vqrshrun_n_s16(
            avg, (2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS));

        vst1_u8(dst8_ptr, d0_u8);

        s += 8;
        dst8_ptr += 8;
        dst16_ptr += 8;
        width -= 8;
      } while (width != 0);

      dst16 += dst16_stride;
      dst8 += dst8_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  }
}

static INLINE void compound_dist_wtd_convolve_vert_scale_neon(
    const int16_t *src, int src_stride, uint8_t *dst8, int dst8_stride,
    uint16_t *dst16, int dst16_stride, int w, int h, const int16_t *y_filter,
    ConvolveParams *conv_params, int subpel_y_qn, int y_step_qn) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  int y_qn = subpel_y_qn;
  // A shim of 1 << (COMPOUND_ROUND1_BITS - 1) enables us to use
  // non-rounding shifts - which are generally faster than rounding shifts on
  // modern CPUs.
  const int32x4_t vert_offset =
      vdupq_n_s32((1 << offset_bits) + (1 << (COMPOUND_ROUND1_BITS - 1)));
  // For the weighted averaging code path we have to substract round offset and
  // convolve round. The shim of 1 << (2 * FILTER_BITS - ROUND0_BITS -
  // COMPOUND_ROUND1_BITS - 1) enables us to use non-rounding shifts. The
  // additional shift by DIST_PRECISION_BITS is needed in order to merge two
  // shift calculations into one.
  const int32x4_t dist_wtd_offset = vdupq_n_s32(
      (1 << (2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS - 1 +
             DIST_PRECISION_BITS)) -
      (1 << (offset_bits - COMPOUND_ROUND1_BITS + DIST_PRECISION_BITS)) -
      (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1 + DIST_PRECISION_BITS)));
  const int16x4_t bck_offset = vdup_n_s16(conv_params->bck_offset);
  const int16x4_t fwd_offset = vdup_n_s16(conv_params->fwd_offset);

  if (w == 4) {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      int16x4_t d0 = compound_convolve8_4_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                            filter, vert_offset);

      int16x4_t dd0 = vreinterpret_s16_u16(vld1_u16(dst16));

      int32x4_t dst_wtd_avg = vmlal_s16(dist_wtd_offset, bck_offset, d0);
      dst_wtd_avg = vmlal_s16(dst_wtd_avg, fwd_offset, dd0);

      int16x4_t d0_s16 = vshrn_n_s32(
          dst_wtd_avg, 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS +
                           DIST_PRECISION_BITS);

      uint8x8_t d0_u8 = vqmovun_s16(vcombine_s16(d0_s16, vdup_n_s16(0)));

      store_u8_4x1(dst8, d0_u8);

      dst16 += dst16_stride;
      dst8 += dst8_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  } else {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int width = w;
      uint8_t *dst8_ptr = dst8;
      uint16_t *dst16_ptr = dst16;

      do {
        int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
        load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

        int16x8_t d0 = compound_convolve8_8_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                              filter, vert_offset);

        int16x8_t dd0 = vreinterpretq_s16_u16(vld1q_u16(dst16_ptr));

        int32x4_t dst_wtd_avg0 =
            vmlal_s16(dist_wtd_offset, bck_offset, vget_low_s16(d0));
        int32x4_t dst_wtd_avg1 =
            vmlal_s16(dist_wtd_offset, bck_offset, vget_high_s16(d0));

        dst_wtd_avg0 = vmlal_s16(dst_wtd_avg0, fwd_offset, vget_low_s16(dd0));
        dst_wtd_avg1 = vmlal_s16(dst_wtd_avg1, fwd_offset, vget_high_s16(dd0));

        int16x4_t d0_s16_0 = vshrn_n_s32(
            dst_wtd_avg0, 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS +
                              DIST_PRECISION_BITS);
        int16x4_t d0_s16_1 = vshrn_n_s32(
            dst_wtd_avg1, 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS +
                              DIST_PRECISION_BITS);

        uint8x8_t d0_u8 = vqmovun_s16(vcombine_s16(d0_s16_0, d0_s16_1));

        vst1_u8(dst8_ptr, d0_u8);

        s += 8;
        dst8_ptr += 8;
        dst16_ptr += 8;
        width -= 8;
      } while (width != 0);

      dst16 += dst16_stride;
      dst8 += dst8_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  }
}

static INLINE uint8x8_t convolve8_4_v(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16x8_t filter,
                                      const int32x4_t offset_const) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum = offset_const;
  sum = vmlal_lane_s16(sum, s0, filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, filter_4_7, 3);

  int16x4_t res = vshrn_n_s32(sum, 2 * FILTER_BITS - ROUND0_BITS);

  return vqmovun_s16(vcombine_s16(res, vdup_n_s16(0)));
}

static INLINE uint8x8_t convolve8_8_v(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter,
                                      const int32x4_t offset_const) {
  const int16x4_t filter_0_3 = vget_low_s16(filter);
  const int16x4_t filter_4_7 = vget_high_s16(filter);

  int32x4_t sum0 = offset_const;
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s0), filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), filter_4_7, 3);

  int32x4_t sum1 = offset_const;
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s0), filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), filter_4_7, 3);

  int16x4_t res0 = vshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS);
  int16x4_t res1 = vshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS);

  return vqmovun_s16(vcombine_s16(res0, res1));
}

static INLINE void convolve_vert_scale_neon(const int16_t *src, int src_stride,
                                            uint8_t *dst, int dst_stride, int w,
                                            int h, const int16_t *y_filter,
                                            int subpel_y_qn, int y_step_qn) {
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int round_1 = 2 * FILTER_BITS - ROUND0_BITS;
  // The shim of 1 << (round_1 - 1) enables us to use non-rounding shifts.
  int32x4_t vert_offset =
      vdupq_n_s32((1 << (round_1 - 1)) - (1 << (offset_bits - 1)));

  int y_qn = subpel_y_qn;
  if (w == 4) {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_4x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      uint8x8_t d =
          convolve8_4_v(s0, s1, s2, s3, s4, s5, s6, s7, filter, vert_offset);

      store_u8_4x1(dst, d);

      dst += dst_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  } else if (w == 8) {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      load_s16_8x8(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

      uint8x8_t d =
          convolve8_8_v(s0, s1, s2, s3, s4, s5, s6, s7, filter, vert_offset);

      vst1_u8(dst, d);

      dst += dst_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  } else {
    do {
      const int16_t *s = &src[(y_qn >> SCALE_SUBPEL_BITS) * src_stride];
      uint8_t *d = dst;
      int width = w;

      const ptrdiff_t filter_offset =
          SUBPEL_TAPS * ((y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
      const int16x8_t filter = vld1q_s16(y_filter + filter_offset);

      do {
        int16x8_t s0[2], s1[2], s2[2], s3[2], s4[2], s5[2], s6[2], s7[2];
        load_s16_8x8(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0], &s4[0],
                     &s5[0], &s6[0], &s7[0]);
        load_s16_8x8(s + 8, src_stride, &s0[1], &s1[1], &s2[1], &s3[1], &s4[1],
                     &s5[1], &s6[1], &s7[1]);

        uint8x8_t d0 = convolve8_8_v(s0[0], s1[0], s2[0], s3[0], s4[0], s5[0],
                                     s6[0], s7[0], filter, vert_offset);
        uint8x8_t d1 = convolve8_8_v(s0[1], s1[1], s2[1], s3[1], s4[1], s5[1],
                                     s6[1], s7[1], filter, vert_offset);

        vst1q_u8(d, vcombine_u8(d0, d1));

        s += 16;
        d += 16;
        width -= 16;
      } while (width != 0);

      dst += dst_stride;
      y_qn += y_step_qn;
    } while (--h != 0);
  }
}

static INLINE int16x4_t convolve8_4_h(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16x8_t filter,
                                      const int32x4_t horiz_const) {
  int16x4_t filter_lo = vget_low_s16(filter);
  int16x4_t filter_hi = vget_high_s16(filter);

  int32x4_t sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlal_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlal_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlal_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlal_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlal_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlal_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlal_lane_s16(sum, s7, filter_hi, 3);

  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve8_8_h(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter,
                                      const int16x8_t horiz_const) {
  int16x4_t filter_lo = vget_low_s16(filter);
  int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  return vshrq_n_s16(sum, ROUND0_BITS - 1);
}

static INLINE void convolve_horiz_scale_neon(const uint8_t *src, int src_stride,
                                             int16_t *dst, int dst_stride,
                                             int w, int h,
                                             const int16_t *x_filter,
                                             const int subpel_x_qn,
                                             const int x_step_qn) {
  DECLARE_ALIGNED(16, int16_t, temp[8 * 8]);
  const int bd = 8;

  if (w == 4) {
    // The shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts.
    const int32x4_t horiz_offset =
        vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));

    do {
      int x_qn = subpel_x_qn;

      // Process a 4x4 tile.
      for (int r = 0; r < 4; ++r) {
        const uint8_t *const s = &src[x_qn >> SCALE_SUBPEL_BITS];

        const ptrdiff_t filter_offset =
            SUBPEL_TAPS * ((x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
        const int16x8_t filter = vld1q_s16(x_filter + filter_offset);

        uint8x8_t t0, t1, t2, t3;
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);

        transpose_elems_inplace_u8_8x4(&t0, &t1, &t2, &t3);

        int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
        int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

        int16x4_t d0 =
            convolve8_4_h(s0, s1, s2, s3, s4, s5, s6, s7, filter, horiz_offset);

        vst1_s16(&temp[r * 4], d0);
        x_qn += x_step_qn;
      }

      // Transpose the 4x4 result tile and store.
      int16x4_t d0, d1, d2, d3;
      load_s16_4x4(temp, 4, &d0, &d1, &d2, &d3);

      transpose_elems_inplace_s16_4x4(&d0, &d1, &d2, &d3);

      store_s16_4x4(dst, dst_stride, d0, d1, d2, d3);

      dst += 4 * dst_stride;
      src += 4 * src_stride;
      h -= 4;
    } while (h > 0);
  } else {
    // The shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts.
    // The additional -1 is needed because we are halving the filter values.
    const int16x8_t horiz_offset =
        vdupq_n_s16((1 << (bd + FILTER_BITS - 2)) + (1 << (ROUND0_BITS - 2)));

    do {
      int x_qn = subpel_x_qn;
      int16_t *d = dst;
      int width = w;

      do {
        // Process an 8x8 tile.
        for (int r = 0; r < 8; ++r) {
          const uint8_t *const s = &src[(x_qn >> SCALE_SUBPEL_BITS)];

          const ptrdiff_t filter_offset =
              SUBPEL_TAPS * ((x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS);
          int16x8_t filter = vld1q_s16(x_filter + filter_offset);
          // Filter values are all even so halve them to allow convolution
          // kernel computations to stay in 16-bit element types.
          filter = vshrq_n_s16(filter, 1);

          uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

          transpose_elems_u8_8x8(t0, t1, t2, t3, t4, t5, t6, t7, &t0, &t1, &t2,
                                 &t3, &t4, &t5, &t6, &t7);

          int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
          int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
          int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
          int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
          int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
          int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
          int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
          int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));

          int16x8_t d0 = convolve8_8_h(s0, s1, s2, s3, s4, s5, s6, s7, filter,
                                       horiz_offset);

          vst1q_s16(&temp[r * 8], d0);

          x_qn += x_step_qn;
        }

        // Transpose the 8x8 result tile and store.
        int16x8_t d0, d1, d2, d3, d4, d5, d6, d7;
        load_s16_8x8(temp, 8, &d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        transpose_elems_inplace_s16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        store_s16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

        d += 8;
        width -= 8;
      } while (width != 0);

      dst += 8 * dst_stride;
      src += 8 * src_stride;
      h -= 8;
    } while (h > 0);
  }
}

void av1_convolve_2d_scale_neon(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_x,
                                const InterpFilterParams *filter_params_y,
                                const int subpel_x_qn, const int x_step_qn,
                                const int subpel_y_qn, const int y_step_qn,
                                ConvolveParams *conv_params) {
  if (w < 4 || h < 4) {
    av1_convolve_2d_scale_c(src, src_stride, dst, dst_stride, w, h,
                            filter_params_x, filter_params_y, subpel_x_qn,
                            x_step_qn, subpel_y_qn, y_step_qn, conv_params);
    return;
  }

  // For the interpolation 8-tap filters are used.
  assert(filter_params_y->taps <= 8 && filter_params_x->taps <= 8);

  DECLARE_ALIGNED(32, int16_t,
                  im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  int im_stride = MAX_SB_SIZE;
  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int dst16_stride = conv_params->dst_stride;

  // Account for needing filter_taps / 2 - 1 lines prior and filter_taps / 2
  // lines post both horizontally and vertically.
  const ptrdiff_t horiz_offset = filter_params_x->taps / 2 - 1;
  const ptrdiff_t vert_offset = (filter_params_y->taps / 2 - 1) * src_stride;

  // Horizontal filter
  convolve_horiz_scale_neon(
      src - horiz_offset - vert_offset, src_stride, im_block, im_stride, w,
      im_h, filter_params_x->filter_ptr, subpel_x_qn, x_step_qn);

  // Vertical filter
  if (UNLIKELY(conv_params->is_compound)) {
    if (conv_params->do_average) {
      if (conv_params->use_dist_wtd_comp_avg) {
        compound_dist_wtd_convolve_vert_scale_neon(
            im_block, im_stride, dst, dst_stride, dst16, dst16_stride, w, h,
            filter_params_y->filter_ptr, conv_params, subpel_y_qn, y_step_qn);
      } else {
        compound_avg_convolve_vert_scale_neon(
            im_block, im_stride, dst, dst_stride, dst16, dst16_stride, w, h,
            filter_params_y->filter_ptr, subpel_y_qn, y_step_qn);
      }
    } else {
      compound_convolve_vert_scale_neon(
          im_block, im_stride, dst16, dst16_stride, w, h,
          filter_params_y->filter_ptr, subpel_y_qn, y_step_qn);
    }
  } else {
    convolve_vert_scale_neon(im_block, im_stride, dst, dst_stride, w, h,
                             filter_params_y->filter_ptr, subpel_y_qn,
                             y_step_qn);
  }
}
