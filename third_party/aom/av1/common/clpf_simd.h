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
#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"

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

// delta = 1/16 * constrain(a, x, s, d) + 3/16 * constrain(b, x, s, d) +
//         1/16 * constrain(c, x, s, d) + 3/16 * constrain(d, x, s, d) +
//         3/16 * constrain(e, x, s, d) + 1/16 * constrain(f, x, s, d) +
//         3/16 * constrain(g, x, s, d) + 1/16 * constrain(h, x, s, d)
SIMD_INLINE v128 calc_delta(v256 x, v256 a, v256 b, v256 c, v256 d, v256 e,
                            v256 f, v256 g, v256 h, unsigned int s,
                            unsigned int dmp) {
  const v128 bdeg =
      v128_add_8(v128_add_8(constrain(b, x, s, dmp), constrain(d, x, s, dmp)),
                 v128_add_8(constrain(e, x, s, dmp), constrain(g, x, s, dmp)));
  const v128 delta = v128_add_8(
      v128_add_8(v128_add_8(constrain(a, x, s, dmp), constrain(c, x, s, dmp)),
                 v128_add_8(constrain(f, x, s, dmp), constrain(h, x, s, dmp))),
      v128_add_8(v128_add_8(bdeg, bdeg), bdeg));
  return v128_add_8(
      v128_pack_s16_u8(v256_high_v128(x), v256_low_v128(x)),
      v128_shr_s8(
          v128_add_8(v128_dup_8(8),
                     v128_add_8(delta, v128_cmplt_s8(delta, v128_zero()))),
          4));
}

// delta = 1/8 * constrain(a, x, s, d) + 3/8 * constrain(b, x, s, d) +
//         3/8 * constrain(c, x, s, d) + 1/8 * constrain(d, x, s, d) +
SIMD_INLINE v128 calc_hdelta(v256 x, v256 a, v256 b, v256 c, v256 d,
                             unsigned int s, unsigned int dmp) {
  const v128 bc = v128_add_8(constrain(b, x, s, dmp), constrain(c, x, s, dmp));
  const v128 delta =
      v128_add_8(v128_add_8(constrain(a, x, s, dmp), constrain(d, x, s, dmp)),
                 v128_add_8(v128_add_8(bc, bc), bc));
  return v128_add_8(
      v128_pack_s16_u8(v256_high_v128(x), v256_low_v128(x)),
      v128_shr_s8(
          v128_add_8(v128_dup_8(4),
                     v128_add_8(delta, v128_cmplt_s8(delta, v128_zero()))),
          3));
}

// Process blocks of width 8, two lines at a time, 8 bit.
static void SIMD_FUNC(clpf_block8)(uint8_t *dst, const uint16_t *src,
                                   int dstride, int sstride, int sizey,
                                   unsigned int strength,
                                   unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v128 l1 = v128_load_aligned(src);
    const v128 l2 = v128_load_aligned(src + sstride);
    const v128 l3 = v128_load_aligned(src - sstride);
    const v128 l4 = v128_load_aligned(src + 2 * sstride);
    const v256 a = v256_from_v128(v128_load_aligned(src - 2 * sstride), l3);
    const v256 b = v256_from_v128(l3, l1);
    const v256 g = v256_from_v128(l2, l4);
    const v256 h = v256_from_v128(l4, v128_load_aligned(src + 3 * sstride));
    const v256 c = v256_from_v128(v128_load_unaligned(src - 2),
                                  v128_load_unaligned(src - 2 + sstride));
    const v256 d = v256_from_v128(v128_load_unaligned(src - 1),
                                  v128_load_unaligned(src - 1 + sstride));
    const v256 e = v256_from_v128(v128_load_unaligned(src + 1),
                                  v128_load_unaligned(src + 1 + sstride));
    const v256 f = v256_from_v128(v128_load_unaligned(src + 2),
                                  v128_load_unaligned(src + 2 + sstride));
    const v128 o = calc_delta(v256_from_v128(l1, l2), a, b, c, d, e, f, g, h,
                              strength, adjdamp);

    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 4, four lines at a time, 8 bit.
static void SIMD_FUNC(clpf_block4)(uint8_t *dst, const uint16_t *src,
                                   int dstride, int sstride, int sizey,
                                   unsigned int strength,
                                   unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 4) {
    const v64 l0 = v64_load_aligned(src - 2 * sstride);
    const v64 l1 = v64_load_aligned(src - sstride);
    const v64 l2 = v64_load_aligned(src);
    const v64 l3 = v64_load_aligned(src + sstride);
    const v64 l4 = v64_load_aligned(src + 2 * sstride);
    const v64 l5 = v64_load_aligned(src + 3 * sstride);
    const v64 l6 = v64_load_aligned(src + 4 * sstride);
    const v64 l7 = v64_load_aligned(src + 5 * sstride);
    const v128 o =
        calc_delta(v256_from_v64(l2, l3, l4, l5), v256_from_v64(l0, l1, l2, l3),
                   v256_from_v64(l1, l2, l3, l4),
                   v256_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src + sstride - 2),
                                 v64_load_unaligned(src + 2 * sstride - 2),
                                 v64_load_unaligned(src + 3 * sstride - 2)),
                   v256_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src + sstride - 1),
                                 v64_load_unaligned(src + 2 * sstride - 1),
                                 v64_load_unaligned(src + 3 * sstride - 1)),
                   v256_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + sstride + 1),
                                 v64_load_unaligned(src + 2 * sstride + 1),
                                 v64_load_unaligned(src + 3 * sstride + 1)),
                   v256_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + sstride + 2),
                                 v64_load_unaligned(src + 2 * sstride + 2),
                                 v64_load_unaligned(src + 3 * sstride + 2)),
                   v256_from_v64(l3, l4, l5, l6), v256_from_v64(l4, l5, l6, l7),
                   strength, adjdamp);

    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

static void SIMD_FUNC(clpf_hblock8)(uint8_t *dst, const uint16_t *src,
                                    int dstride, int sstride, int sizey,
                                    unsigned int strength,
                                    unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v256 x = v256_from_v128(v128_load_aligned(src),
                                  v128_load_aligned(src + sstride));
    const v256 a = v256_from_v128(v128_load_unaligned(src - 2),
                                  v128_load_unaligned(src - 2 + sstride));
    const v256 b = v256_from_v128(v128_load_unaligned(src - 1),
                                  v128_load_unaligned(src - 1 + sstride));
    const v256 c = v256_from_v128(v128_load_unaligned(src + 1),
                                  v128_load_unaligned(src + 1 + sstride));
    const v256 d = v256_from_v128(v128_load_unaligned(src + 2),
                                  v128_load_unaligned(src + 2 + sstride));
    const v128 o = calc_hdelta(x, a, b, c, d, strength, adjdamp);

    v64_store_aligned(dst, v128_high_v64(o));
    v64_store_aligned(dst + dstride, v128_low_v64(o));
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 4, four lines at a time, 8 bit.
static void SIMD_FUNC(clpf_hblock4)(uint8_t *dst, const uint16_t *src,
                                    int dstride, int sstride, int sizey,
                                    unsigned int strength,
                                    unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 4) {
    const v256 a = v256_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src + sstride - 2),
                                 v64_load_unaligned(src + 2 * sstride - 2),
                                 v64_load_unaligned(src + 3 * sstride - 2));
    const v256 b = v256_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src + sstride - 1),
                                 v64_load_unaligned(src + 2 * sstride - 1),
                                 v64_load_unaligned(src + 3 * sstride - 1));
    const v256 c = v256_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + sstride + 1),
                                 v64_load_unaligned(src + 2 * sstride + 1),
                                 v64_load_unaligned(src + 3 * sstride + 1));
    const v256 d = v256_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + sstride + 2),
                                 v64_load_unaligned(src + 2 * sstride + 2),
                                 v64_load_unaligned(src + 3 * sstride + 2));

    const v128 o = calc_hdelta(
        v256_from_v64(v64_load_aligned(src), v64_load_aligned(src + sstride),
                      v64_load_aligned(src + 2 * sstride),
                      v64_load_aligned(src + 3 * sstride)),
        a, b, c, d, strength, adjdamp);

    u32_store_aligned(dst, v128_low_u32(v128_shr_n_byte(o, 12)));
    u32_store_aligned(dst + dstride, v128_low_u32(v128_shr_n_byte(o, 8)));
    u32_store_aligned(dst + 2 * dstride, v128_low_u32(v128_shr_n_byte(o, 4)));
    u32_store_aligned(dst + 3 * dstride, v128_low_u32(o));

    dst += 4 * dstride;
    src += 4 * sstride;
  }
}

void SIMD_FUNC(aom_clpf_block)(uint8_t *dst, const uint16_t *src, int dstride,
                               int sstride, int sizex, int sizey,
                               unsigned int strength, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 3) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block widths not 4 or 8
    // * block heights not a multiple of 4 if the block width is 4
    aom_clpf_block_c(dst, src, dstride, sstride, sizex, sizey, strength, dmp);
  } else {
    (sizex == 4 ? SIMD_FUNC(clpf_block4) : SIMD_FUNC(clpf_block8))(
        dst, src, dstride, sstride, sizey, strength, dmp - get_msb(strength));
  }
}

void SIMD_FUNC(aom_clpf_hblock)(uint8_t *dst, const uint16_t *src, int dstride,
                                int sstride, int sizex, int sizey,
                                unsigned int strength, unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 3) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block widths not 4 or 8
    // * block heights not a multiple of 4 if the block width is 4
    aom_clpf_hblock_c(dst, src, dstride, sstride, sizex, sizey, strength, dmp);
  } else {
    (sizex == 4 ? SIMD_FUNC(clpf_hblock4) : SIMD_FUNC(clpf_hblock8))(
        dst, src, dstride, sstride, sizey, strength, dmp - get_msb(strength));
  }
}

// delta = 1/16 * constrain(a, x, s, d) + 3/16 * constrain(b, x, s, d) +
//         1/16 * constrain(c, x, s, d) + 3/16 * constrain(d, x, s, d) +
//         3/16 * constrain(e, x, s, d) + 1/16 * constrain(f, x, s, d) +
//         3/16 * constrain(g, x, s, d) + 1/16 * constrain(h, x, s, d)
SIMD_INLINE v128 calc_delta_hbd(v128 x, v128 a, v128 b, v128 c, v128 d, v128 e,
                                v128 f, v128 g, v128 h, unsigned int s,
                                unsigned int dmp) {
  const v128 bdeg = v128_add_16(
      v128_add_16(constrain16(b, x, s, dmp), constrain16(d, x, s, dmp)),
      v128_add_16(constrain16(e, x, s, dmp), constrain16(g, x, s, dmp)));
  const v128 delta = v128_add_16(
      v128_add_16(
          v128_add_16(constrain16(a, x, s, dmp), constrain16(c, x, s, dmp)),
          v128_add_16(constrain16(f, x, s, dmp), constrain16(h, x, s, dmp))),
      v128_add_16(v128_add_16(bdeg, bdeg), bdeg));
  return v128_add_16(
      x,
      v128_shr_s16(
          v128_add_16(v128_dup_16(8),
                      v128_add_16(delta, v128_cmplt_s16(delta, v128_zero()))),
          4));
}

static void calc_delta_hbd4(v128 o, v128 a, v128 b, v128 c, v128 d, v128 e,
                            v128 f, v128 g, v128 h, uint16_t *dst,
                            unsigned int s, unsigned int dmp, int dstride) {
  o = calc_delta_hbd(o, a, b, c, d, e, f, g, h, s, dmp);
  v64_store_aligned(dst, v128_high_v64(o));
  v64_store_aligned(dst + dstride, v128_low_v64(o));
}

static void calc_delta_hbd8(v128 o, v128 a, v128 b, v128 c, v128 d, v128 e,
                            v128 f, v128 g, v128 h, uint16_t *dst,
                            unsigned int s, unsigned int adjdamp) {
  v128_store_aligned(dst,
                     calc_delta_hbd(o, a, b, c, d, e, f, g, h, s, adjdamp));
}

// delta = 1/16 * constrain(a, x, s, dmp) + 3/16 * constrain(b, x, s, dmp) +
//         3/16 * constrain(c, x, s, dmp) + 1/16 * constrain(d, x, s, dmp)
SIMD_INLINE v128 calc_hdelta_hbd(v128 x, v128 a, v128 b, v128 c, v128 d,
                                 unsigned int s, unsigned int dmp) {
  const v128 bc =
      v128_add_16(constrain16(b, x, s, dmp), constrain16(c, x, s, dmp));
  const v128 delta = v128_add_16(
      v128_add_16(constrain16(a, x, s, dmp), constrain16(d, x, s, dmp)),
      v128_add_16(v128_add_16(bc, bc), bc));
  return v128_add_16(
      x,
      v128_shr_s16(
          v128_add_16(v128_dup_16(4),
                      v128_add_16(delta, v128_cmplt_s16(delta, v128_zero()))),
          3));
}

static void calc_hdelta_hbd4(v128 o, v128 a, v128 b, v128 c, v128 d,
                             uint16_t *dst, unsigned int s,
                             unsigned int adjdamp, int dstride) {
  o = calc_hdelta_hbd(o, a, b, c, d, s, adjdamp);
  v64_store_aligned(dst, v128_high_v64(o));
  v64_store_aligned(dst + dstride, v128_low_v64(o));
}

static void calc_hdelta_hbd8(v128 o, v128 a, v128 b, v128 c, v128 d,
                             uint16_t *dst, unsigned int s,
                             unsigned int adjdamp) {
  v128_store_aligned(dst, calc_hdelta_hbd(o, a, b, c, d, s, adjdamp));
}

// Process blocks of width 4, two lines at time.
static void SIMD_FUNC(clpf_block_hbd4)(uint16_t *dst, const uint16_t *src,
                                       int dstride, int sstride, int sizey,
                                       unsigned int strength,
                                       unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v64 l1 = v64_load_aligned(src);
    const v64 l2 = v64_load_aligned(src + sstride);
    const v64 l3 = v64_load_aligned(src - sstride);
    const v64 l4 = v64_load_aligned(src + 2 * sstride);
    const v128 a = v128_from_v64(v64_load_aligned(src - 2 * sstride), l3);
    const v128 b = v128_from_v64(l3, l1);
    const v128 g = v128_from_v64(l2, l4);
    const v128 h = v128_from_v64(l4, v64_load_aligned(src + 3 * sstride));
    const v128 c = v128_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src - 2 + sstride));
    const v128 d = v128_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src - 1 + sstride));
    const v128 e = v128_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + 1 + sstride));
    const v128 f = v128_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + 2 + sstride));

    calc_delta_hbd4(v128_from_v64(l1, l2), a, b, c, d, e, f, g, h, dst,
                    strength, adjdamp, dstride);
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// The most simple case.  Start here if you need to understand the functions.
static void SIMD_FUNC(clpf_block_hbd)(uint16_t *dst, const uint16_t *src,
                                      int dstride, int sstride, int sizey,
                                      unsigned int strength,
                                      unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y++) {
    const v128 o = v128_load_aligned(src);
    const v128 a = v128_load_aligned(src - 2 * sstride);
    const v128 b = v128_load_aligned(src - 1 * sstride);
    const v128 g = v128_load_aligned(src + sstride);
    const v128 h = v128_load_aligned(src + 2 * sstride);
    const v128 c = v128_load_unaligned(src - 2);
    const v128 d = v128_load_unaligned(src - 1);
    const v128 e = v128_load_unaligned(src + 1);
    const v128 f = v128_load_unaligned(src + 2);

    calc_delta_hbd8(o, a, b, c, d, e, f, g, h, dst, strength, adjdamp);
    src += sstride;
    dst += dstride;
  }
}

// Process blocks of width 4, horizontal filter, two lines at time.
static void SIMD_FUNC(clpf_hblock_hbd4)(uint16_t *dst, const uint16_t *src,
                                        int dstride, int sstride, int sizey,
                                        unsigned int strength,
                                        unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y += 2) {
    const v128 a = v128_from_v64(v64_load_unaligned(src - 2),
                                 v64_load_unaligned(src - 2 + sstride));
    const v128 b = v128_from_v64(v64_load_unaligned(src - 1),
                                 v64_load_unaligned(src - 1 + sstride));
    const v128 c = v128_from_v64(v64_load_unaligned(src + 1),
                                 v64_load_unaligned(src + 1 + sstride));
    const v128 d = v128_from_v64(v64_load_unaligned(src + 2),
                                 v64_load_unaligned(src + 2 + sstride));

    calc_hdelta_hbd4(v128_from_v64(v64_load_unaligned(src),
                                   v64_load_unaligned(src + sstride)),
                     a, b, c, d, dst, strength, adjdamp, dstride);
    src += sstride * 2;
    dst += dstride * 2;
  }
}

// Process blocks of width 8, horizontal filter, two lines at time.
static void SIMD_FUNC(clpf_hblock_hbd)(uint16_t *dst, const uint16_t *src,
                                       int dstride, int sstride, int sizey,
                                       unsigned int strength,
                                       unsigned int adjdamp) {
  int y;

  for (y = 0; y < sizey; y++) {
    const v128 o = v128_load_aligned(src);
    const v128 a = v128_load_unaligned(src - 2);
    const v128 b = v128_load_unaligned(src - 1);
    const v128 c = v128_load_unaligned(src + 1);
    const v128 d = v128_load_unaligned(src + 2);

    calc_hdelta_hbd8(o, a, b, c, d, dst, strength, adjdamp);
    src += sstride;
    dst += dstride;
  }
}

void SIMD_FUNC(aom_clpf_block_hbd)(uint16_t *dst, const uint16_t *src,
                                   int dstride, int sstride, int sizex,
                                   int sizey, unsigned int strength,
                                   unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 1) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block width not 4 or 8
    // * block heights not a multiple of 2 if the block width is 4
    aom_clpf_block_hbd_c(dst, src, dstride, sstride, sizex, sizey, strength,
                         dmp);
  } else {
    (sizex == 4 ? SIMD_FUNC(clpf_block_hbd4) : SIMD_FUNC(clpf_block_hbd))(
        dst, src, dstride, sstride, sizey, strength, dmp - get_msb(strength));
  }
}

void SIMD_FUNC(aom_clpf_hblock_hbd)(uint16_t *dst, const uint16_t *src,
                                    int dstride, int sstride, int sizex,
                                    int sizey, unsigned int strength,
                                    unsigned int dmp) {
  if ((sizex != 4 && sizex != 8) || ((sizey & 1) && sizex == 4)) {
    // Fallback to C for odd sizes:
    // * block width not 4 or 8
    // * block heights not a multiple of 2 if the block width is 4
    aom_clpf_hblock_hbd_c(dst, src, dstride, sstride, sizex, sizey, strength,
                          dmp);
  } else {
    (sizex == 4 ? SIMD_FUNC(clpf_hblock_hbd4) : SIMD_FUNC(clpf_hblock_hbd))(
        dst, src, dstride, sstride, sizey, strength, dmp - get_msb(strength));
  }
}
