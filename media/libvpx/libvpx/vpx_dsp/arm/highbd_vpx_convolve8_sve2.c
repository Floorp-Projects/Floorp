/*
 *  Copyright (c) 2024 The WebM project authors. All Rights Reserved.
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
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/transpose_neon.h"
#include "vpx_dsp/arm/vpx_neon_sve_bridge.h"
#include "vpx_dsp/arm/vpx_neon_sve2_bridge.h"

// clang-format off
DECLARE_ALIGNED(16, static const uint16_t, kDotProdMergeBlockTbl[24]) = {
  // Shift left and insert new last column in transposed 4x4 block.
  1, 2, 3, 0, 5, 6, 7, 4,
  // Shift left and insert two new columns in transposed 4x4 block.
  2, 3, 0, 1, 6, 7, 4, 5,
  // Shift left and insert three new columns in transposed 4x4 block.
  3, 0, 1, 2, 7, 4, 5, 6,
};
// clang-format on

static INLINE void transpose_concat_4x4(const int16x4_t s0, const int16x4_t s1,
                                        const int16x4_t s2, const int16x4_t s3,
                                        int16x8_t res[2]) {
  // Transpose 16-bit elements:
  // s0: 00, 01, 02, 03
  // s1: 10, 11, 12, 13
  // s2: 20, 21, 22, 23
  // s3: 30, 31, 32, 33
  //
  // res[0]: 00 10 20 30 01 11 21 31
  // res[1]: 02 12 22 32 03 13 23 33

  int16x8_t s0q = vcombine_s16(s0, vdup_n_s16(0));
  int16x8_t s1q = vcombine_s16(s1, vdup_n_s16(0));
  int16x8_t s2q = vcombine_s16(s2, vdup_n_s16(0));
  int16x8_t s3q = vcombine_s16(s3, vdup_n_s16(0));

  int32x4_t s01 = vreinterpretq_s32_s16(vzip1q_s16(s0q, s1q));
  int32x4_t s23 = vreinterpretq_s32_s16(vzip1q_s16(s2q, s3q));

  int32x4x2_t t0123 = vzipq_s32(s01, s23);

  res[0] = vreinterpretq_s16_s32(t0123.val[0]);
  res[1] = vreinterpretq_s16_s32(t0123.val[1]);
}

static INLINE void transpose_concat_8x4(const int16x8_t s0, const int16x8_t s1,
                                        const int16x8_t s2, const int16x8_t s3,
                                        int16x8_t res[4]) {
  // Transpose 16-bit elements:
  // s0: 00, 01, 02, 03, 04, 05, 06, 07
  // s1: 10, 11, 12, 13, 14, 15, 16, 17
  // s2: 20, 21, 22, 23, 24, 25, 26, 27
  // s3: 30, 31, 32, 33, 34, 35, 36, 37
  //
  // res[0]: 00 10 20 30 01 11 21 31
  // res[1]: 02 12 22 32 03 13 23 33
  // res[2]: 04 14 24 34 05 15 25 35
  // res[3]: 06 16 26 36 07 17 27 37

  int16x8x2_t s01 = vzipq_s16(s0, s1);
  int16x8x2_t s23 = vzipq_s16(s2, s3);

  int32x4x2_t t0123_lo = vzipq_s32(vreinterpretq_s32_s16(s01.val[0]),
                                   vreinterpretq_s32_s16(s23.val[0]));
  int32x4x2_t t0123_hi = vzipq_s32(vreinterpretq_s32_s16(s01.val[1]),
                                   vreinterpretq_s32_s16(s23.val[1]));

  res[0] = vreinterpretq_s16_s32(t0123_lo.val[0]);
  res[1] = vreinterpretq_s16_s32(t0123_lo.val[1]);
  res[2] = vreinterpretq_s16_s32(t0123_hi.val[0]);
  res[3] = vreinterpretq_s16_s32(t0123_hi.val[1]);
}

static INLINE void vpx_tbl2x4_s16(int16x8_t s0[4], int16x8_t s1[4],
                                  int16x8_t res[4], uint16x8_t idx) {
  res[0] = vpx_tbl2_s16(s0[0], s1[0], idx);
  res[1] = vpx_tbl2_s16(s0[1], s1[1], idx);
  res[2] = vpx_tbl2_s16(s0[2], s1[2], idx);
  res[3] = vpx_tbl2_s16(s0[3], s1[3], idx);
}

static INLINE void vpx_tbl2x2_s16(int16x8_t s0[2], int16x8_t s1[2],
                                  int16x8_t res[2], uint16x8_t idx) {
  res[0] = vpx_tbl2_s16(s0[0], s1[0], idx);
  res[1] = vpx_tbl2_s16(s0[1], s1[1], idx);
}

static INLINE uint16x4_t highbd_convolve8_4_v(int16x8_t s_lo[2],
                                              int16x8_t s_hi[2],
                                              int16x8_t filter,
                                              uint16x4_t max) {
  int64x2_t sum01 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[0], filter, 0);
  sum01 = vpx_dotq_lane_s16(sum01, s_hi[0], filter, 1);

  int64x2_t sum23 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[1], filter, 0);
  sum23 = vpx_dotq_lane_s16(sum23, s_hi[1], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));

  uint16x4_t res = vqrshrun_n_s32(sum0123, FILTER_BITS);
  return vmin_u16(res, max);
}

static INLINE uint16x8_t highbd_convolve8_8_v(const int16x8_t s_lo[4],
                                              const int16x8_t s_hi[4],
                                              const int16x8_t filter,
                                              const uint16x8_t max) {
  int64x2_t sum01 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[0], filter, 0);
  sum01 = vpx_dotq_lane_s16(sum01, s_hi[0], filter, 1);

  int64x2_t sum23 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[1], filter, 0);
  sum23 = vpx_dotq_lane_s16(sum23, s_hi[1], filter, 1);

  int64x2_t sum45 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[2], filter, 0);
  sum45 = vpx_dotq_lane_s16(sum45, s_hi[2], filter, 1);

  int64x2_t sum67 = vpx_dotq_lane_s16(vdupq_n_s64(0), s_lo[3], filter, 0);
  sum67 = vpx_dotq_lane_s16(sum67, s_hi[3], filter, 1);

  int32x4_t sum0123 = vcombine_s32(vmovn_s64(sum01), vmovn_s64(sum23));
  int32x4_t sum4567 = vcombine_s32(vmovn_s64(sum45), vmovn_s64(sum67));

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(sum0123, FILTER_BITS),
                                vqrshrun_n_s32(sum4567, FILTER_BITS));
  return vminq_u16(res, max);
}

static INLINE void highbd_convolve8_8tap_vert_sve2(
    const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst,
    ptrdiff_t dst_stride, int w, int h, const int16x8_t filter, int bd) {
  assert(w >= 4 && h >= 4);
  uint16x8x3_t merge_tbl_idx = vld1q_u16_x3(kDotProdMergeBlockTbl);

  // Correct indices by the size of vector length.
  merge_tbl_idx.val[0] = vaddq_u16(
      merge_tbl_idx.val[0],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL)));
  merge_tbl_idx.val[1] = vaddq_u16(
      merge_tbl_idx.val[1],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL)));
  merge_tbl_idx.val[2] = vaddq_u16(
      merge_tbl_idx.val[2],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL)));

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);

    do {
      int16x4_t s7, s8, s9, sA;

      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &sA);

      int16x8_t s4567[2], s5678[2], s6789[2], s789A[2];
      transpose_concat_4x4(s7, s8, s9, sA, s789A);

      vpx_tbl2x2_s16(s3456, s789A, s4567, merge_tbl_idx.val[0]);
      vpx_tbl2x2_s16(s3456, s789A, s5678, merge_tbl_idx.val[1]);
      vpx_tbl2x2_s16(s3456, s789A, s6789, merge_tbl_idx.val[2]);

      uint16x4_t d0 = highbd_convolve8_4_v(s0123, s4567, filter, max);
      uint16x4_t d1 = highbd_convolve8_4_v(s1234, s5678, filter, max);
      uint16x4_t d2 = highbd_convolve8_4_v(s2345, s6789, filter, max);
      uint16x4_t d3 = highbd_convolve8_4_v(s3456, s789A, filter, max);

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int height = h;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
      transpose_concat_8x4(s0, s1, s2, s3, s0123);
      transpose_concat_8x4(s1, s2, s3, s4, s1234);
      transpose_concat_8x4(s2, s3, s4, s5, s2345);
      transpose_concat_8x4(s3, s4, s5, s6, s3456);

      do {
        int16x8_t s7, s8, s9, sA;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &sA);

        int16x8_t s4567[4], s5678[5], s6789[4], s789A[4];
        transpose_concat_8x4(s7, s8, s9, sA, s789A);

        vpx_tbl2x4_s16(s3456, s789A, s4567, merge_tbl_idx.val[0]);
        vpx_tbl2x4_s16(s3456, s789A, s5678, merge_tbl_idx.val[1]);
        vpx_tbl2x4_s16(s3456, s789A, s6789, merge_tbl_idx.val[2]);

        uint16x8_t d0 = highbd_convolve8_8_v(s0123, s4567, filter, max);
        uint16x8_t d1 = highbd_convolve8_8_v(s1234, s5678, filter, max);
        uint16x8_t d2 = highbd_convolve8_8_v(s2345, s6789, filter, max);
        uint16x8_t d3 = highbd_convolve8_8_v(s3456, s789A, filter, max);

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0123[0] = s4567[0];
        s0123[1] = s4567[1];
        s0123[2] = s4567[2];
        s0123[3] = s4567[3];
        s1234[0] = s5678[0];
        s1234[1] = s5678[1];
        s1234[2] = s5678[2];
        s1234[3] = s5678[3];
        s2345[0] = s6789[0];
        s2345[1] = s6789[1];
        s2345[2] = s6789[2];
        s2345[3] = s6789[3];
        s3456[0] = s789A[0];
        s3456[1] = s789A[1];
        s3456[2] = s789A[2];
        s3456[3] = s789A[3];

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}

void vpx_highbd_convolve8_vert_sve2(const uint16_t *src, ptrdiff_t src_stride,
                                    uint16_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *filter, int x0_q4,
                                    int x_step_q4, int y0_q4, int y_step_q4,
                                    int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_vert_c(src, src_stride, dst, dst_stride, filter, x0_q4,
                                x_step_q4, y0_q4, y_step_q4, w, h, bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);
  assert(y_step_q4 == 16);

  (void)x_step_q4;
  (void)y0_q4;
  (void)y_step_q4;

  if (vpx_get_filter_taps(filter[y0_q4]) <= 4) {
    vpx_highbd_convolve8_vert_neon(src, src_stride, dst, dst_stride, filter,
                                   x0_q4, x_step_q4, y0_q4, y_step_q4, w, h,
                                   bd);
  } else {
    const int16x8_t y_filter_8tap = vld1q_s16(filter[y0_q4]);
    highbd_convolve8_8tap_vert_sve2(src - 3 * src_stride, src_stride, dst,
                                    dst_stride, w, h, y_filter_8tap, bd);
  }
}

void vpx_highbd_convolve8_avg_vert_sve2(const uint16_t *src,
                                        ptrdiff_t src_stride, uint16_t *dst,
                                        ptrdiff_t dst_stride,
                                        const InterpKernel *filter, int x0_q4,
                                        int x_step_q4, int y0_q4, int y_step_q4,
                                        int w, int h, int bd) {
  if (y_step_q4 != 16) {
    vpx_highbd_convolve8_avg_vert_c(src, src_stride, dst, dst_stride, filter,
                                    x0_q4, x_step_q4, y0_q4, y_step_q4, w, h,
                                    bd);
    return;
  }

  assert((intptr_t)dst % 4 == 0);
  assert(dst_stride % 4 == 0);

  const int16x8_t filters = vld1q_s16(filter[y0_q4]);

  src -= 3 * src_stride;

  uint16x8x3_t merge_tbl_idx = vld1q_u16_x3(kDotProdMergeBlockTbl);

  // Correct indices by the size of vector length.
  merge_tbl_idx.val[0] = vaddq_u16(
      merge_tbl_idx.val[0],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000000000000ULL)));
  merge_tbl_idx.val[1] = vaddq_u16(
      merge_tbl_idx.val[1],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100000000ULL)));
  merge_tbl_idx.val[2] = vaddq_u16(
      merge_tbl_idx.val[2],
      vreinterpretq_u16_u64(vdupq_n_u64(svcnth() * 0x0001000100010000ULL)));

  if (w == 4) {
    const uint16x4_t max = vdup_n_u16((1 << bd) - 1);
    const int16_t *s = (const int16_t *)src;
    uint16_t *d = dst;

    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    s += 7 * src_stride;

    int16x8_t s0123[2], s1234[2], s2345[2], s3456[2];
    transpose_concat_4x4(s0, s1, s2, s3, s0123);
    transpose_concat_4x4(s1, s2, s3, s4, s1234);
    transpose_concat_4x4(s2, s3, s4, s5, s2345);
    transpose_concat_4x4(s3, s4, s5, s6, s3456);

    do {
      int16x4_t s7, s8, s9, sA;

      load_s16_4x4(s, src_stride, &s7, &s8, &s9, &sA);

      int16x8_t s4567[2], s5678[2], s6789[2], s789A[2];
      transpose_concat_4x4(s7, s8, s9, sA, s789A);

      vpx_tbl2x2_s16(s3456, s789A, s4567, merge_tbl_idx.val[0]);
      vpx_tbl2x2_s16(s3456, s789A, s5678, merge_tbl_idx.val[1]);
      vpx_tbl2x2_s16(s3456, s789A, s6789, merge_tbl_idx.val[2]);

      uint16x4_t d0 = highbd_convolve8_4_v(s0123, s4567, filters, max);
      uint16x4_t d1 = highbd_convolve8_4_v(s1234, s5678, filters, max);
      uint16x4_t d2 = highbd_convolve8_4_v(s2345, s6789, filters, max);
      uint16x4_t d3 = highbd_convolve8_4_v(s3456, s789A, filters, max);

      d0 = vrhadd_u16(d0, vld1_u16(d + 0 * dst_stride));
      d1 = vrhadd_u16(d1, vld1_u16(d + 1 * dst_stride));
      d2 = vrhadd_u16(d2, vld1_u16(d + 2 * dst_stride));
      d3 = vrhadd_u16(d3, vld1_u16(d + 3 * dst_stride));

      store_u16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0123[0] = s4567[0];
      s0123[1] = s4567[1];
      s1234[0] = s5678[0];
      s1234[1] = s5678[1];
      s2345[0] = s6789[0];
      s2345[1] = s6789[1];
      s3456[0] = s789A[0];
      s3456[1] = s789A[1];

      s += 4 * src_stride;
      d += 4 * dst_stride;
      h -= 4;
    } while (h != 0);
  } else {
    const uint16x8_t max = vdupq_n_u16((1 << bd) - 1);

    do {
      const int16_t *s = (const int16_t *)src;
      uint16_t *d = dst;
      int height = h;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      int16x8_t s0123[4], s1234[4], s2345[4], s3456[4];
      transpose_concat_8x4(s0, s1, s2, s3, s0123);
      transpose_concat_8x4(s1, s2, s3, s4, s1234);
      transpose_concat_8x4(s2, s3, s4, s5, s2345);
      transpose_concat_8x4(s3, s4, s5, s6, s3456);

      do {
        int16x8_t s7, s8, s9, sA;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &sA);

        int16x8_t s4567[4], s5678[5], s6789[4], s789A[4];
        transpose_concat_8x4(s7, s8, s9, sA, s789A);

        vpx_tbl2x4_s16(s3456, s789A, s4567, merge_tbl_idx.val[0]);
        vpx_tbl2x4_s16(s3456, s789A, s5678, merge_tbl_idx.val[1]);
        vpx_tbl2x4_s16(s3456, s789A, s6789, merge_tbl_idx.val[2]);

        uint16x8_t d0 = highbd_convolve8_8_v(s0123, s4567, filters, max);
        uint16x8_t d1 = highbd_convolve8_8_v(s1234, s5678, filters, max);
        uint16x8_t d2 = highbd_convolve8_8_v(s2345, s6789, filters, max);
        uint16x8_t d3 = highbd_convolve8_8_v(s3456, s789A, filters, max);

        d0 = vrhaddq_u16(d0, vld1q_u16(d + 0 * dst_stride));
        d1 = vrhaddq_u16(d1, vld1q_u16(d + 1 * dst_stride));
        d2 = vrhaddq_u16(d2, vld1q_u16(d + 2 * dst_stride));
        d3 = vrhaddq_u16(d3, vld1q_u16(d + 3 * dst_stride));

        store_u16_8x4(d, dst_stride, d0, d1, d2, d3);

        s0123[0] = s4567[0];
        s0123[1] = s4567[1];
        s0123[2] = s4567[2];
        s0123[3] = s4567[3];
        s1234[0] = s5678[0];
        s1234[1] = s5678[1];
        s1234[2] = s5678[2];
        s1234[3] = s5678[3];
        s2345[0] = s6789[0];
        s2345[1] = s6789[1];
        s2345[2] = s6789[2];
        s2345[3] = s6789[3];
        s3456[0] = s789A[0];
        s3456[1] = s789A[1];
        s3456[2] = s789A[2];
        s3456[3] = s789A[3];

        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height != 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w != 0);
  }
}
