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

#ifndef _V128_INTRINSICS_H
#define _V128_INTRINSICS_H

#include <arm_neon.h>
#include "./v64_intrinsics_arm.h"

typedef int64x2_t v128;

SIMD_INLINE uint32_t v128_low_u32(v128 a) {
  return v64_low_u32(vget_low_s64(a));
}

SIMD_INLINE v64 v128_low_v64(v128 a) { return vget_low_s64(a); }

SIMD_INLINE v64 v128_high_v64(v128 a) { return vget_high_s64(a); }

SIMD_INLINE v128 v128_from_v64(v64 a, v64 b) { return vcombine_s64(b, a); }

SIMD_INLINE v128 v128_from_64(uint64_t a, uint64_t b) {
  return vcombine_s64((uint64x1_t)b, (uint64x1_t)a);
}

SIMD_INLINE v128 v128_from_32(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  return vcombine_s64(v64_from_32(c, d), v64_from_32(a, b));
}

SIMD_INLINE v128 v128_load_aligned(const void *p) {
  return vreinterpretq_s64_u8(vld1q_u8((const uint8_t *)p));
}

SIMD_INLINE v128 v128_load_unaligned(const void *p) {
  return v128_load_aligned(p);
}

SIMD_INLINE void v128_store_aligned(void *p, v128 r) {
  vst1q_u8((uint8_t *)p, vreinterpretq_u8_s64(r));
}

SIMD_INLINE void v128_store_unaligned(void *p, v128 r) {
  vst1q_u8((uint8_t *)p, vreinterpretq_u8_s64(r));
}

SIMD_INLINE v128 v128_align(v128 a, v128 b, unsigned int c) {
// The following functions require an immediate.
// Some compilers will check this during optimisation, others wont.
#if defined(__OPTIMIZE__) && __OPTIMIZE__ && !defined(__clang__)
  return c ? vreinterpretq_s64_s8(
                 vextq_s8(vreinterpretq_s8_s64(b), vreinterpretq_s8_s64(a), c))
           : b;
#else
  return c < 8 ? v128_from_v64(v64_align(v128_low_v64(a), v128_high_v64(b), c),
                               v64_align(v128_high_v64(b), v128_low_v64(b), c))
               : v128_from_v64(
                     v64_align(v128_high_v64(a), v128_low_v64(a), c - 8),
                     v64_align(v128_low_v64(a), v128_high_v64(b), c - 8));
#endif
}

SIMD_INLINE v128 v128_zero() { return vreinterpretq_s64_u8(vdupq_n_u8(0)); }

SIMD_INLINE v128 v128_ones() { return vreinterpretq_s64_u8(vdupq_n_u8(-1)); }

SIMD_INLINE v128 v128_dup_8(uint8_t x) {
  return vreinterpretq_s64_u8(vdupq_n_u8(x));
}

SIMD_INLINE v128 v128_dup_16(uint16_t x) {
  return vreinterpretq_s64_u16(vdupq_n_u16(x));
}

SIMD_INLINE v128 v128_dup_32(uint32_t x) {
  return vreinterpretq_s64_u32(vdupq_n_u32(x));
}

SIMD_INLINE int64_t v128_dotp_s16(v128 a, v128 b) {
  return v64_dotp_s16(vget_high_s64(a), vget_high_s64(b)) +
         v64_dotp_s16(vget_low_s64(a), vget_low_s64(b));
}

SIMD_INLINE uint64_t v128_hadd_u8(v128 x) {
  uint64x2_t t = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vreinterpretq_u8_s64(x))));
  return vget_lane_s32(
      vreinterpret_s32_u64(vadd_u64(vget_high_u64(t), vget_low_u64(t))), 0);
}

SIMD_INLINE v128 v128_padd_s16(v128 a) {
  return vreinterpretq_s64_s32(vpaddlq_s16(vreinterpretq_s16_s64(a)));
}

typedef struct { sad64_internal hi, lo; } sad128_internal;

SIMD_INLINE sad128_internal v128_sad_u8_init() {
  sad128_internal s;
  s.hi = s.lo = vdupq_n_u16(0);
  return s;
}

/* Implementation dependent return value.  Result must be finalised with
   v128_sad_u8_sum().
   The result for more than 32 v128_sad_u8() calls is undefined. */
SIMD_INLINE sad128_internal v128_sad_u8(sad128_internal s, v128 a, v128 b) {
  sad128_internal r;
  r.hi = v64_sad_u8(s.hi, vget_high_s64(a), vget_high_s64(b));
  r.lo = v64_sad_u8(s.lo, vget_low_s64(a), vget_low_s64(b));
  return r;
}

SIMD_INLINE uint32_t v128_sad_u8_sum(sad128_internal s) {
  return (uint32_t)(v64_sad_u8_sum(s.hi) + v64_sad_u8_sum(s.lo));
}

typedef struct { ssd64_internal hi, lo; } ssd128_internal;

SIMD_INLINE ssd128_internal v128_ssd_u8_init() {
  ssd128_internal s;
  s.hi = s.lo = (ssd64_internal)(uint64_t)0;
  return s;
}

/* Implementation dependent return value.  Result must be finalised with
 * v128_ssd_u8_sum(). */
SIMD_INLINE ssd128_internal v128_ssd_u8(ssd128_internal s, v128 a, v128 b) {
  ssd128_internal r;
  r.hi = v64_ssd_u8(s.hi, vget_high_s64(a), vget_high_s64(b));
  r.lo = v64_ssd_u8(s.lo, vget_low_s64(a), vget_low_s64(b));
  return r;
}

SIMD_INLINE uint32_t v128_ssd_u8_sum(ssd128_internal s) {
  return (uint32_t)(v64_ssd_u8_sum(s.hi) + v64_ssd_u8_sum(s.lo));
}

SIMD_INLINE v128 v128_or(v128 x, v128 y) { return vorrq_s64(x, y); }

SIMD_INLINE v128 v128_xor(v128 x, v128 y) { return veorq_s64(x, y); }

SIMD_INLINE v128 v128_and(v128 x, v128 y) { return vandq_s64(x, y); }

SIMD_INLINE v128 v128_andn(v128 x, v128 y) { return vbicq_s64(x, y); }

SIMD_INLINE v128 v128_add_8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vaddq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_add_16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vaddq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_sadd_s16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vqaddq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_add_32(v128 x, v128 y) {
  return vreinterpretq_s64_u32(
      vaddq_u32(vreinterpretq_u32_s64(x), vreinterpretq_u32_s64(y)));
}

SIMD_INLINE v128 v128_sub_8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vsubq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_sub_16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vsubq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_ssub_s16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vqsubq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_ssub_u16(v128 x, v128 y) {
  return vreinterpretq_s64_u16(
      vqsubq_u16(vreinterpretq_u16_s64(x), vreinterpretq_u16_s64(y)));
}

SIMD_INLINE v128 v128_ssub_u8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vqsubq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_ssub_s8(v128 x, v128 y) {
  return vreinterpretq_s64_s8(
      vqsubq_s8(vreinterpretq_s8_s64(x), vreinterpretq_s8_s64(y)));
}

SIMD_INLINE v128 v128_sub_32(v128 x, v128 y) {
  return vreinterpretq_s64_s32(
      vsubq_s32(vreinterpretq_s32_s64(x), vreinterpretq_s32_s64(y)));
}

SIMD_INLINE v128 v128_abs_s16(v128 x) {
  return vreinterpretq_s64_s16(vabsq_s16(vreinterpretq_s16_s64(x)));
}

SIMD_INLINE v128 v128_abs_s8(v128 x) {
  return vreinterpretq_s64_s8(vabsq_s8(vreinterpretq_s8_s64(x)));
}

SIMD_INLINE v128 v128_mul_s16(v64 a, v64 b) {
  return vreinterpretq_s64_s32(
      vmull_s16(vreinterpret_s16_s64(a), vreinterpret_s16_s64(b)));
}

SIMD_INLINE v128 v128_mullo_s16(v128 a, v128 b) {
  return vreinterpretq_s64_s16(
      vmulq_s16(vreinterpretq_s16_s64(a), vreinterpretq_s16_s64(b)));
}

SIMD_INLINE v128 v128_mulhi_s16(v128 a, v128 b) {
  return v128_from_v64(v64_mulhi_s16(vget_high_s64(a), vget_high_s64(b)),
                       v64_mulhi_s16(vget_low_s64(a), vget_low_s64(b)));
}

SIMD_INLINE v128 v128_mullo_s32(v128 a, v128 b) {
  return vreinterpretq_s64_s32(
      vmulq_s32(vreinterpretq_s32_s64(a), vreinterpretq_s32_s64(b)));
}

SIMD_INLINE v128 v128_madd_s16(v128 a, v128 b) {
  return v128_from_v64(v64_madd_s16(vget_high_s64(a), vget_high_s64(b)),
                       v64_madd_s16(vget_low_s64(a), vget_low_s64(b)));
}

SIMD_INLINE v128 v128_madd_us8(v128 a, v128 b) {
  return v128_from_v64(v64_madd_us8(vget_high_s64(a), vget_high_s64(b)),
                       v64_madd_us8(vget_low_s64(a), vget_low_s64(b)));
}

SIMD_INLINE v128 v128_avg_u8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vrhaddq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_rdavg_u8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vhaddq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_avg_u16(v128 x, v128 y) {
  return vreinterpretq_s64_u16(
      vrhaddq_u16(vreinterpretq_u16_s64(x), vreinterpretq_u16_s64(y)));
}

SIMD_INLINE v128 v128_min_u8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vminq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_max_u8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vmaxq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_min_s8(v128 x, v128 y) {
  return vreinterpretq_s64_s8(
      vminq_s8(vreinterpretq_s8_s64(x), vreinterpretq_s8_s64(y)));
}

SIMD_INLINE v128 v128_max_s8(v128 x, v128 y) {
  return vreinterpretq_s64_s8(
      vmaxq_s8(vreinterpretq_s8_s64(x), vreinterpretq_s8_s64(y)));
}

SIMD_INLINE v128 v128_min_s16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vminq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_max_s16(v128 x, v128 y) {
  return vreinterpretq_s64_s16(
      vmaxq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_ziplo_8(v128 x, v128 y) {
  uint8x16x2_t r = vzipq_u8(vreinterpretq_u8_s64(y), vreinterpretq_u8_s64(x));
  return vreinterpretq_s64_u8(r.val[0]);
}

SIMD_INLINE v128 v128_ziphi_8(v128 x, v128 y) {
  uint8x16x2_t r = vzipq_u8(vreinterpretq_u8_s64(y), vreinterpretq_u8_s64(x));
  return vreinterpretq_s64_u8(r.val[1]);
}

SIMD_INLINE v128 v128_zip_8(v64 x, v64 y) {
  uint8x8x2_t r = vzip_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x));
  return vreinterpretq_s64_u8(vcombine_u8(r.val[0], r.val[1]));
}

SIMD_INLINE v128 v128_ziplo_16(v128 x, v128 y) {
  int16x8x2_t r = vzipq_s16(vreinterpretq_s16_s64(y), vreinterpretq_s16_s64(x));
  return vreinterpretq_s64_s16(r.val[0]);
}

SIMD_INLINE v128 v128_ziphi_16(v128 x, v128 y) {
  int16x8x2_t r = vzipq_s16(vreinterpretq_s16_s64(y), vreinterpretq_s16_s64(x));
  return vreinterpretq_s64_s16(r.val[1]);
}

SIMD_INLINE v128 v128_zip_16(v64 x, v64 y) {
  uint16x4x2_t r = vzip_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x));
  return vreinterpretq_s64_u16(vcombine_u16(r.val[0], r.val[1]));
}

SIMD_INLINE v128 v128_ziplo_32(v128 x, v128 y) {
  int32x4x2_t r = vzipq_s32(vreinterpretq_s32_s64(y), vreinterpretq_s32_s64(x));
  return vreinterpretq_s64_s32(r.val[0]);
}

SIMD_INLINE v128 v128_ziphi_32(v128 x, v128 y) {
  int32x4x2_t r = vzipq_s32(vreinterpretq_s32_s64(y), vreinterpretq_s32_s64(x));
  return vreinterpretq_s64_s32(r.val[1]);
}

SIMD_INLINE v128 v128_zip_32(v64 x, v64 y) {
  uint32x2x2_t r = vzip_u32(vreinterpret_u32_s64(y), vreinterpret_u32_s64(x));
  return vreinterpretq_s64_u32(vcombine_u32(r.val[0], r.val[1]));
}

SIMD_INLINE v128 v128_ziplo_64(v128 a, v128 b) {
  return v128_from_v64(vget_low_u64((uint64x2_t)a),
                       vget_low_u64((uint64x2_t)b));
}

SIMD_INLINE v128 v128_ziphi_64(v128 a, v128 b) {
  return v128_from_v64(vget_high_u64((uint64x2_t)a),
                       vget_high_u64((uint64x2_t)b));
}

SIMD_INLINE v128 v128_unziplo_8(v128 x, v128 y) {
  uint8x16x2_t r = vuzpq_u8(vreinterpretq_u8_s64(y), vreinterpretq_u8_s64(x));
  return vreinterpretq_s64_u8(r.val[0]);
}

SIMD_INLINE v128 v128_unziphi_8(v128 x, v128 y) {
  uint8x16x2_t r = vuzpq_u8(vreinterpretq_u8_s64(y), vreinterpretq_u8_s64(x));
  return vreinterpretq_s64_u8(r.val[1]);
}

SIMD_INLINE v128 v128_unziplo_16(v128 x, v128 y) {
  uint16x8x2_t r =
      vuzpq_u16(vreinterpretq_u16_s64(y), vreinterpretq_u16_s64(x));
  return vreinterpretq_s64_u16(r.val[0]);
}

SIMD_INLINE v128 v128_unziphi_16(v128 x, v128 y) {
  uint16x8x2_t r =
      vuzpq_u16(vreinterpretq_u16_s64(y), vreinterpretq_u16_s64(x));
  return vreinterpretq_s64_u16(r.val[1]);
}

SIMD_INLINE v128 v128_unziplo_32(v128 x, v128 y) {
  uint32x4x2_t r =
      vuzpq_u32(vreinterpretq_u32_s64(y), vreinterpretq_u32_s64(x));
  return vreinterpretq_s64_u32(r.val[0]);
}

SIMD_INLINE v128 v128_unziphi_32(v128 x, v128 y) {
  uint32x4x2_t r =
      vuzpq_u32(vreinterpretq_u32_s64(y), vreinterpretq_u32_s64(x));
  return vreinterpretq_s64_u32(r.val[1]);
}

SIMD_INLINE v128 v128_unpack_u8_s16(v64 a) {
  return vreinterpretq_s64_u16(vmovl_u8(vreinterpret_u8_s64(a)));
}

SIMD_INLINE v128 v128_unpacklo_u8_s16(v128 a) {
  return vreinterpretq_s64_u16(vmovl_u8(vreinterpret_u8_s64(vget_low_s64(a))));
}

SIMD_INLINE v128 v128_unpackhi_u8_s16(v128 a) {
  return vreinterpretq_s64_u16(vmovl_u8(vreinterpret_u8_s64(vget_high_s64(a))));
}

SIMD_INLINE v128 v128_unpack_s8_s16(v64 a) {
  return vreinterpretq_s64_s16(vmovl_s8(vreinterpret_s8_s64(a)));
}

SIMD_INLINE v128 v128_unpacklo_s8_s16(v128 a) {
  return vreinterpretq_s64_s16(vmovl_s8(vreinterpret_s8_s64(vget_low_s64(a))));
}

SIMD_INLINE v128 v128_unpackhi_s8_s16(v128 a) {
  return vreinterpretq_s64_s16(vmovl_s8(vreinterpret_s8_s64(vget_high_s64(a))));
}

SIMD_INLINE v128 v128_pack_s32_s16(v128 a, v128 b) {
  return v128_from_v64(
      vreinterpret_s64_s16(vqmovn_s32(vreinterpretq_s32_s64(a))),
      vreinterpret_s64_s16(vqmovn_s32(vreinterpretq_s32_s64(b))));
}

SIMD_INLINE v128 v128_pack_s16_u8(v128 a, v128 b) {
  return v128_from_v64(
      vreinterpret_s64_u8(vqmovun_s16(vreinterpretq_s16_s64(a))),
      vreinterpret_s64_u8(vqmovun_s16(vreinterpretq_s16_s64(b))));
}

SIMD_INLINE v128 v128_pack_s16_s8(v128 a, v128 b) {
  return v128_from_v64(
      vreinterpret_s64_s8(vqmovn_s16(vreinterpretq_s16_s64(a))),
      vreinterpret_s64_s8(vqmovn_s16(vreinterpretq_s16_s64(b))));
}

SIMD_INLINE v128 v128_unpack_u16_s32(v64 a) {
  return vreinterpretq_s64_u32(vmovl_u16(vreinterpret_u16_s64(a)));
}

SIMD_INLINE v128 v128_unpack_s16_s32(v64 a) {
  return vreinterpretq_s64_s32(vmovl_s16(vreinterpret_s16_s64(a)));
}

SIMD_INLINE v128 v128_unpacklo_u16_s32(v128 a) {
  return vreinterpretq_s64_u32(
      vmovl_u16(vreinterpret_u16_s64(vget_low_s64(a))));
}

SIMD_INLINE v128 v128_unpacklo_s16_s32(v128 a) {
  return vreinterpretq_s64_s32(
      vmovl_s16(vreinterpret_s16_s64(vget_low_s64(a))));
}

SIMD_INLINE v128 v128_unpackhi_u16_s32(v128 a) {
  return vreinterpretq_s64_u32(
      vmovl_u16(vreinterpret_u16_s64(vget_high_s64(a))));
}

SIMD_INLINE v128 v128_unpackhi_s16_s32(v128 a) {
  return vreinterpretq_s64_s32(
      vmovl_s16(vreinterpret_s16_s64(vget_high_s64(a))));
}

SIMD_INLINE v128 v128_shuffle_8(v128 x, v128 pattern) {
  return v128_from_64(
      (uint64_t)vreinterpret_s64_u8(
          vtbl2_u8((uint8x8x2_t){ { vget_low_u8(vreinterpretq_u8_s64(x)),
                                    vget_high_u8(vreinterpretq_u8_s64(x)) } },
                   vreinterpret_u8_s64(vget_high_s64(pattern)))),
      (uint64_t)vreinterpret_s64_u8(
          vtbl2_u8((uint8x8x2_t){ { vget_low_u8(vreinterpretq_u8_s64(x)),
                                    vget_high_u8(vreinterpretq_u8_s64(x)) } },
                   vreinterpret_u8_s64(vget_low_s64(pattern)))));
}

SIMD_INLINE v128 v128_cmpgt_s8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vcgtq_s8(vreinterpretq_s8_s64(x), vreinterpretq_s8_s64(y)));
}

SIMD_INLINE v128 v128_cmplt_s8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vcltq_s8(vreinterpretq_s8_s64(x), vreinterpretq_s8_s64(y)));
}

SIMD_INLINE v128 v128_cmpeq_8(v128 x, v128 y) {
  return vreinterpretq_s64_u8(
      vceqq_u8(vreinterpretq_u8_s64(x), vreinterpretq_u8_s64(y)));
}

SIMD_INLINE v128 v128_cmpgt_s16(v128 x, v128 y) {
  return vreinterpretq_s64_u16(
      vcgtq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_cmplt_s16(v128 x, v128 y) {
  return vreinterpretq_s64_u16(
      vcltq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_cmpeq_16(v128 x, v128 y) {
  return vreinterpretq_s64_u16(
      vceqq_s16(vreinterpretq_s16_s64(x), vreinterpretq_s16_s64(y)));
}

SIMD_INLINE v128 v128_shl_8(v128 a, unsigned int c) {
  return (c > 7) ? v128_zero() : vreinterpretq_s64_u8(vshlq_u8(
                                     vreinterpretq_u8_s64(a), vdupq_n_s8(c)));
}

SIMD_INLINE v128 v128_shr_u8(v128 a, unsigned int c) {
  return (c > 7) ? v128_zero() : vreinterpretq_s64_u8(vshlq_u8(
                                     vreinterpretq_u8_s64(a), vdupq_n_s8(-c)));
}

SIMD_INLINE v128 v128_shr_s8(v128 a, unsigned int c) {
  return (c > 7) ? v128_ones() : vreinterpretq_s64_s8(vshlq_s8(
                                     vreinterpretq_s8_s64(a), vdupq_n_s8(-c)));
}

SIMD_INLINE v128 v128_shl_16(v128 a, unsigned int c) {
  return (c > 15) ? v128_zero()
                  : vreinterpretq_s64_u16(
                        vshlq_u16(vreinterpretq_u16_s64(a), vdupq_n_s16(c)));
}

SIMD_INLINE v128 v128_shr_u16(v128 a, unsigned int c) {
  return (c > 15) ? v128_zero()
                  : vreinterpretq_s64_u16(
                        vshlq_u16(vreinterpretq_u16_s64(a), vdupq_n_s16(-c)));
}

SIMD_INLINE v128 v128_shr_s16(v128 a, unsigned int c) {
  return (c > 15) ? v128_ones()
                  : vreinterpretq_s64_s16(
                        vshlq_s16(vreinterpretq_s16_s64(a), vdupq_n_s16(-c)));
}

SIMD_INLINE v128 v128_shl_32(v128 a, unsigned int c) {
  return (c > 31) ? v128_zero()
                  : vreinterpretq_s64_u32(
                        vshlq_u32(vreinterpretq_u32_s64(a), vdupq_n_s32(c)));
}

SIMD_INLINE v128 v128_shr_u32(v128 a, unsigned int c) {
  return (c > 31) ? v128_zero()
                  : vreinterpretq_s64_u32(
                        vshlq_u32(vreinterpretq_u32_s64(a), vdupq_n_s32(-c)));
}

SIMD_INLINE v128 v128_shr_s32(v128 a, unsigned int c) {
  return (c > 31) ? v128_ones()
                  : vreinterpretq_s64_s32(
                        vshlq_s32(vreinterpretq_s32_s64(a), vdupq_n_s32(-c)));
}

#if defined(__OPTIMIZE__) && __OPTIMIZE__ && !defined(__clang__)

SIMD_INLINE v128 v128_shl_n_byte(v128 a, unsigned int n) {
  return n < 8
             ? v128_from_64(
                   (uint64_t)vorr_u64(
                       vshl_n_u64(vreinterpret_u64_s64(vget_high_s64(a)),
                                  n * 8),
                       vshr_n_u64(vreinterpret_u64_s64(vget_low_s64(a)),
                                  (8 - n) * 8)),
                   (uint64_t)vshl_n_u64(vreinterpret_u64_s64(vget_low_s64(a)),
                                        n * 8))
             : (n == 8 ? v128_from_64(
                             (uint64_t)vreinterpret_u64_s64(vget_low_s64(a)), 0)
                       : v128_from_64((uint64_t)vshl_n_u64(
                                          vreinterpret_u64_s64(vget_low_s64(a)),
                                          (n - 8) * 8),
                                      0));
}

SIMD_INLINE v128 v128_shr_n_byte(v128 a, unsigned int n) {
  return n < 8
             ? v128_from_64(
                   vshr_n_u64(vreinterpret_u64_s64(vget_high_s64(a)), n * 8),
                   vorr_u64(
                       vshr_n_u64(vreinterpret_u64_s64(vget_low_s64(a)), n * 8),
                       vshl_n_u64(vreinterpret_u64_s64(vget_high_s64(a)),
                                  (8 - n) * 8)))
             : (n == 8
                    ? v128_from_64(0, vreinterpret_u64_s64(vget_high_s64(a)))
                    : v128_from_64(
                          0, vshr_n_u64(vreinterpret_u64_s64(vget_high_s64(a)),
                                        (n - 8) * 8)));
}

SIMD_INLINE v128 v128_shl_n_8(v128 a, unsigned int c) {
  return vreinterpretq_s64_u8(vshlq_n_u8(vreinterpretq_u8_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_u8(v128 a, unsigned int c) {
  return vreinterpretq_s64_u8(vshrq_n_u8(vreinterpretq_u8_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_s8(v128 a, unsigned int c) {
  return vreinterpretq_s64_s8(vshrq_n_s8(vreinterpretq_s8_s64(a), c));
}

SIMD_INLINE v128 v128_shl_n_16(v128 a, unsigned int c) {
  return vreinterpretq_s64_u16(vshlq_n_u16(vreinterpretq_u16_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_u16(v128 a, unsigned int c) {
  return vreinterpretq_s64_u16(vshrq_n_u16(vreinterpretq_u16_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_s16(v128 a, unsigned int c) {
  return vreinterpretq_s64_s16(vshrq_n_s16(vreinterpretq_s16_s64(a), c));
}

SIMD_INLINE v128 v128_shl_n_32(v128 a, unsigned int c) {
  return vreinterpretq_s64_u32(vshlq_n_u32(vreinterpretq_u32_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_u32(v128 a, unsigned int c) {
  return vreinterpretq_s64_u32(vshrq_n_u32(vreinterpretq_u32_s64(a), c));
}

SIMD_INLINE v128 v128_shr_n_s32(v128 a, unsigned int c) {
  return vreinterpretq_s64_s32(vshrq_n_s32(vreinterpretq_s32_s64(a), c));
}

#else

SIMD_INLINE v128 v128_shl_n_byte(v128 a, unsigned int n) {
  if (n < 8)
    return v128_from_v64(v64_or(v64_shl_n_byte(v128_high_v64(a), n),
                                v64_shr_n_byte(v128_low_v64(a), 8 - n)),
                         v64_shl_n_byte(v128_low_v64(a), n));
  else
    return v128_from_v64(v64_shl_n_byte(v128_low_v64(a), n - 8), v64_zero());
}

SIMD_INLINE v128 v128_shr_n_byte(v128 a, unsigned int n) {
  if (n < 8)
    return v128_from_v64(v64_shr_n_byte(v128_high_v64(a), n),
                         v64_or(v64_shr_n_byte(v128_low_v64(a), n),
                                v64_shl_n_byte(v128_high_v64(a), 8 - n)));
  else
    return v128_from_v64(v64_zero(), v64_shr_n_byte(v128_high_v64(a), n - 8));
}

SIMD_INLINE v128 v128_shl_n_8(v128 a, unsigned int c) {
  return v128_shl_8(a, c);
}

SIMD_INLINE v128 v128_shr_n_u8(v128 a, unsigned int c) {
  return v128_shr_u8(a, c);
}

SIMD_INLINE v128 v128_shr_n_s8(v128 a, unsigned int c) {
  return v128_shr_s8(a, c);
}

SIMD_INLINE v128 v128_shl_n_16(v128 a, unsigned int c) {
  return v128_shl_16(a, c);
}

SIMD_INLINE v128 v128_shr_n_u16(v128 a, unsigned int c) {
  return v128_shr_u16(a, c);
}

SIMD_INLINE v128 v128_shr_n_s16(v128 a, unsigned int c) {
  return v128_shr_s16(a, c);
}

SIMD_INLINE v128 v128_shl_n_32(v128 a, unsigned int c) {
  return v128_shl_32(a, c);
}

SIMD_INLINE v128 v128_shr_n_u32(v128 a, unsigned int c) {
  return v128_shr_u32(a, c);
}

SIMD_INLINE v128 v128_shr_n_s32(v128 a, unsigned int c) {
  return v128_shr_s32(a, c);
}

#endif

#endif /* _V128_INTRINSICS_H */
