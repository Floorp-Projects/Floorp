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

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "./cdef.h"

/* Generated from gen_filter_tables.c. */
#if !CONFIG_CDEF_SINGLEPASS || CDEF_FULL
const int cdef_directions[8][3] = {
  { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2, -3 * CDEF_BSTRIDE + 3 },
  { 0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2, -1 * CDEF_BSTRIDE + 3 },
  { 0 * CDEF_BSTRIDE + 1, 0 * CDEF_BSTRIDE + 2, 0 * CDEF_BSTRIDE + 3 },
  { 0 * CDEF_BSTRIDE + 1, 1 * CDEF_BSTRIDE + 2, 1 * CDEF_BSTRIDE + 3 },
  { 1 * CDEF_BSTRIDE + 1, 2 * CDEF_BSTRIDE + 2, 3 * CDEF_BSTRIDE + 3 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 1, 3 * CDEF_BSTRIDE + 1 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 0, 3 * CDEF_BSTRIDE + 0 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE - 1, 3 * CDEF_BSTRIDE - 1 }
};
#else
const int cdef_directions[8][2] = {
  { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, 0 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, 1 * CDEF_BSTRIDE + 2 },
  { 1 * CDEF_BSTRIDE + 1, 2 * CDEF_BSTRIDE + 2 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 1 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 0 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE - 1 }
};
#endif

/* Detect direction. 0 means 45-degree up-right, 2 is horizontal, and so on.
   The search minimizes the weighted variance along all the lines in a
   particular direction, i.e. the squared error between the input and a
   "predicted" block where each pixel is replaced by the average along a line
   in a particular direction. Since each direction have the same sum(x^2) term,
   that term is never computed. See Section 2, step 2, of:
   http://jmvalin.ca/notes/intra_paint.pdf */
int cdef_find_dir_c(const uint16_t *img, int stride, int32_t *var,
                    int coeff_shift) {
  int i;
  int32_t cost[8] = { 0 };
  int partial[8][15] = { { 0 } };
  int32_t best_cost = 0;
  int best_dir = 0;
  /* Instead of dividing by n between 2 and 8, we multiply by 3*5*7*8/n.
     The output is then 840 times larger, but we don't care for finding
     the max. */
  static const int div_table[] = { 0, 840, 420, 280, 210, 168, 140, 120, 105 };
  for (i = 0; i < 8; i++) {
    int j;
    for (j = 0; j < 8; j++) {
      int x;
      /* We subtract 128 here to reduce the maximum range of the squared
         partial sums. */
      x = (img[i * stride + j] >> coeff_shift) - 128;
      partial[0][i + j] += x;
      partial[1][i + j / 2] += x;
      partial[2][i] += x;
      partial[3][3 + i - j / 2] += x;
      partial[4][7 + i - j] += x;
      partial[5][3 - i / 2 + j] += x;
      partial[6][j] += x;
      partial[7][i / 2 + j] += x;
    }
  }
  for (i = 0; i < 8; i++) {
    cost[2] += partial[2][i] * partial[2][i];
    cost[6] += partial[6][i] * partial[6][i];
  }
  cost[2] *= div_table[8];
  cost[6] *= div_table[8];
  for (i = 0; i < 7; i++) {
    cost[0] += (partial[0][i] * partial[0][i] +
                partial[0][14 - i] * partial[0][14 - i]) *
               div_table[i + 1];
    cost[4] += (partial[4][i] * partial[4][i] +
                partial[4][14 - i] * partial[4][14 - i]) *
               div_table[i + 1];
  }
  cost[0] += partial[0][7] * partial[0][7] * div_table[8];
  cost[4] += partial[4][7] * partial[4][7] * div_table[8];
  for (i = 1; i < 8; i += 2) {
    int j;
    for (j = 0; j < 4 + 1; j++) {
      cost[i] += partial[i][3 + j] * partial[i][3 + j];
    }
    cost[i] *= div_table[8];
    for (j = 0; j < 4 - 1; j++) {
      cost[i] += (partial[i][j] * partial[i][j] +
                  partial[i][10 - j] * partial[i][10 - j]) *
                 div_table[2 * j + 2];
    }
  }
  for (i = 0; i < 8; i++) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      best_dir = i;
    }
  }
  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var = best_cost - cost[(best_dir + 4) & 7];
  /* We'd normally divide by 840, but dividing by 1024 is close enough
     for what we're going to do with this. */
  *var >>= 10;
  return best_dir;
}

#if CONFIG_CDEF_SINGLEPASS
#if CDEF_FULL
const int cdef_pri_taps[2][3] = { { 3, 2, 1 }, { 2, 2, 2 } };
const int cdef_sec_taps[2][2] = { { 3, 1 }, { 3, 1 } };
#else
const int cdef_pri_taps[2][2] = { { 4, 2 }, { 3, 3 } };
const int cdef_sec_taps[2][2] = { { 2, 1 }, { 2, 1 } };
#endif

/* Smooth in the direction detected. */
#if CDEF_CAP
void cdef_filter_block_c(uint8_t *dst8, uint16_t *dst16, int dstride,
                         const uint16_t *in, int pri_strength, int sec_strength,
                         int dir, int pri_damping, int sec_damping, int bsize,
                         UNUSED int max_unused)
#else
void cdef_filter_block_c(uint8_t *dst8, uint16_t *dst16, int dstride,
                         const uint16_t *in, int pri_strength, int sec_strength,
                         int dir, int pri_damping, int sec_damping, int bsize,
                         int max)
#endif
{
  int i, j, k;
  const int s = CDEF_BSTRIDE;
  const int *pri_taps = cdef_pri_taps[pri_strength & 1];
  const int *sec_taps = cdef_sec_taps[pri_strength & 1];
  for (i = 0; i < 4 << (bsize == BLOCK_8X8); i++) {
    for (j = 0; j < 4 << (bsize == BLOCK_8X8); j++) {
      int16_t sum = 0;
      int16_t y;
      int16_t x = in[i * s + j];
#if CDEF_CAP
      int max = x;
      int min = x;
#endif
#if CDEF_FULL
      for (k = 0; k < 3; k++)
#else
      for (k = 0; k < 2; k++)
#endif
      {
        int16_t p0 = in[i * s + j + cdef_directions[dir][k]];
        int16_t p1 = in[i * s + j - cdef_directions[dir][k]];
        sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
        sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
#if CDEF_CAP
        if (p0 != CDEF_VERY_LARGE) max = AOMMAX(p0, max);
        if (p1 != CDEF_VERY_LARGE) max = AOMMAX(p1, max);
        min = AOMMIN(p0, min);
        min = AOMMIN(p1, min);
#endif
#if CDEF_FULL
        if (k == 2) continue;
#endif
        int16_t s0 = in[i * s + j + cdef_directions[(dir + 2) & 7][k]];
        int16_t s1 = in[i * s + j - cdef_directions[(dir + 2) & 7][k]];
        int16_t s2 = in[i * s + j + cdef_directions[(dir + 6) & 7][k]];
        int16_t s3 = in[i * s + j - cdef_directions[(dir + 6) & 7][k]];
#if CDEF_CAP
        if (s0 != CDEF_VERY_LARGE) max = AOMMAX(s0, max);
        if (s1 != CDEF_VERY_LARGE) max = AOMMAX(s1, max);
        if (s2 != CDEF_VERY_LARGE) max = AOMMAX(s2, max);
        if (s3 != CDEF_VERY_LARGE) max = AOMMAX(s3, max);
        min = AOMMIN(s0, min);
        min = AOMMIN(s1, min);
        min = AOMMIN(s2, min);
        min = AOMMIN(s3, min);
#endif
        sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
        sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
        sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
        sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
      }
#if CDEF_CAP
      y = clamp((int16_t)x + ((8 + sum - (sum < 0)) >> 4), min, max);
#else
      y = clamp((int16_t)x + ((8 + sum - (sum < 0)) >> 4), 0, max);
#endif
      if (dst8)
        dst8[i * dstride + j] = (uint8_t)y;
      else
        dst16[i * dstride + j] = (uint16_t)y;
    }
  }
}

#else

/* Smooth in the direction detected. */
void cdef_direction_8x8_c(uint16_t *y, int ystride, const uint16_t *in,
                          int threshold, int dir, int damping) {
  int i;
  int j;
  int k;
  static const int taps[3] = { 3, 2, 1 };
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      int16_t sum;
      int16_t xx;
      int16_t yy;
      xx = in[i * CDEF_BSTRIDE + j];
      sum = 0;
      for (k = 0; k < 3; k++) {
        int16_t p0;
        int16_t p1;
        p0 = in[i * CDEF_BSTRIDE + j + cdef_directions[dir][k]] - xx;
        p1 = in[i * CDEF_BSTRIDE + j - cdef_directions[dir][k]] - xx;
        sum += taps[k] * constrain(p0, threshold, damping);
        sum += taps[k] * constrain(p1, threshold, damping);
      }
      sum = (sum + 8) >> 4;
      yy = xx + sum;
      y[i * ystride + j] = yy;
    }
  }
}

/* Smooth in the direction detected. */
void cdef_direction_4x4_c(uint16_t *y, int ystride, const uint16_t *in,
                          int threshold, int dir, int damping) {
  int i;
  int j;
  int k;
  static const int taps[2] = { 4, 1 };
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      int16_t sum;
      int16_t xx;
      int16_t yy;
      xx = in[i * CDEF_BSTRIDE + j];
      sum = 0;
      for (k = 0; k < 2; k++) {
        int16_t p0;
        int16_t p1;
        p0 = in[i * CDEF_BSTRIDE + j + cdef_directions[dir][k]] - xx;
        p1 = in[i * CDEF_BSTRIDE + j - cdef_directions[dir][k]] - xx;
        sum += taps[k] * constrain(p0, threshold, damping);
        sum += taps[k] * constrain(p1, threshold, damping);
      }
      sum = (sum + 8) >> 4;
      yy = xx + sum;
      y[i * ystride + j] = yy;
    }
  }
}
#endif

/* Compute the primary filter strength for an 8x8 block based on the
   directional variance difference. A high variance difference means
   that we have a highly directional pattern (e.g. a high contrast
   edge), so we can apply more deringing. A low variance means that we
   either have a low contrast edge, or a non-directional texture, so
   we want to be careful not to blur. */
static INLINE int adjust_strength(int strength, int32_t var) {
  const int i = var >> 6 ? AOMMIN(get_msb(var >> 6), 12) : 0;
  /* We use the variance of 8x8 blocks to adjust the strength. */
  return var ? (strength * (4 + i) + 8) >> 4 : 0;
}

#if !CONFIG_CDEF_SINGLEPASS
void copy_8x8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src,
                               int sstride) {
  int i, j;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++) dst[i * dstride + j] = src[i * sstride + j];
}

void copy_4x4_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src,
                               int sstride) {
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++) dst[i * dstride + j] = src[i * sstride + j];
}

static void copy_block_16bit_to_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                      cdef_list *dlist, int cdef_count,
                                      int bsize) {
  int bi, bx, by;

  if (bsize == BLOCK_8X8) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_8x8_16bit_to_16bit(&dst[(by << 3) * dstride + (bx << 3)], dstride,
                              &src[bi << (3 + 3)], 8);
    }
  } else if (bsize == BLOCK_4X8) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_16bit(&dst[(by << 3) * dstride + (bx << 2)], dstride,
                              &src[bi << (3 + 2)], 4);
      copy_4x4_16bit_to_16bit(&dst[((by << 3) + 4) * dstride + (bx << 2)],
                              dstride, &src[(bi << (3 + 2)) + 4 * 4], 4);
    }
  } else if (bsize == BLOCK_8X4) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_16bit(&dst[(by << 2) * dstride + (bx << 3)], dstride,
                              &src[bi << (2 + 3)], 8);
      copy_4x4_16bit_to_16bit(&dst[(by << 2) * dstride + (bx << 3) + 4],
                              dstride, &src[(bi << (2 + 3)) + 4], 8);
    }
  } else {
    assert(bsize == BLOCK_4X4);
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_16bit(&dst[(by << 2) * dstride + (bx << 2)], dstride,
                              &src[bi << (2 + 2)], 4);
    }
  }
}

void copy_8x8_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src,
                              int sstride) {
  int i, j;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      dst[i * dstride + j] = (uint8_t)src[i * sstride + j];
}

void copy_4x4_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src,
                              int sstride) {
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      dst[i * dstride + j] = (uint8_t)src[i * sstride + j];
}

static void copy_block_16bit_to_8bit(uint8_t *dst, int dstride,
                                     const uint16_t *src, cdef_list *dlist,
                                     int cdef_count, int bsize) {
  int bi, bx, by;
  if (bsize == BLOCK_8X8) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_8x8_16bit_to_8bit(&dst[(by << 3) * dstride + (bx << 3)], dstride,
                             &src[bi << (3 + 3)], 8);
    }
  } else if (bsize == BLOCK_4X8) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_8bit(&dst[(by << 3) * dstride + (bx << 2)], dstride,
                             &src[bi << (3 + 2)], 4);
      copy_4x4_16bit_to_8bit(&dst[((by << 3) + 4) * dstride + (bx << 2)],
                             dstride, &src[(bi << (3 + 2)) + 4 * 4], 4);
    }
  } else if (bsize == BLOCK_8X4) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_8bit(&dst[(by << 2) * dstride + (bx << 3)], dstride,
                             &src[bi << (2 + 3)], 8);
      copy_4x4_16bit_to_8bit(&dst[(by << 2) * dstride + (bx << 3) + 4], dstride,
                             &src[(bi << (2 + 3)) + 4], 8);
    }
  } else {
    assert(bsize == BLOCK_4X4);
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_8bit(&dst[(by << 2) * dstride + (bx << 2)], dstride,
                             &src[bi << (2 * 2)], 4);
    }
  }
}

int get_filter_skip(int level) {
  int filter_skip = level & 1;
  if (level == 1) filter_skip = 0;
  return filter_skip;
}

void cdef_filter_fb(uint8_t *dst, int dstride, uint16_t *y, uint16_t *in,
                    int xdec, int ydec, int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
                    int *dirinit, int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int pli,
                    cdef_list *dlist, int cdef_count, int level,
                    int sec_strength, int sec_damping, int pri_damping,
                    int coeff_shift, int skip_dering, int hbd) {
#else

void cdef_filter_fb(uint8_t *dst8, uint16_t *dst16, int dstride, uint16_t *in,
                    int xdec, int ydec, int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
                    int *dirinit, int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int pli,
                    cdef_list *dlist, int cdef_count, int level,
                    int sec_strength, int pri_damping, int sec_damping,
                    int coeff_shift) {
#endif
  int bi;
  int bx;
  int by;
  int bsize, bsizex, bsizey;

#if CONFIG_CDEF_SINGLEPASS
  int pri_strength = (level >> 1) << coeff_shift;
  int filter_skip = level & 1;
  if (!pri_strength && !sec_strength && filter_skip) {
    pri_strength = 19 << coeff_shift;
    sec_strength = 7 << coeff_shift;
  }
#else
  int threshold = (level >> 1) << coeff_shift;
  int filter_skip = get_filter_skip(level);
  if (level == 1) threshold = 31 << coeff_shift;

  cdef_direction_func cdef_direction[] = { cdef_direction_4x4,
                                           cdef_direction_8x8 };
#endif
  sec_damping += coeff_shift - (pli != AOM_PLANE_Y);
  pri_damping += coeff_shift - (pli != AOM_PLANE_Y);
  bsize =
      ydec ? (xdec ? BLOCK_4X4 : BLOCK_8X4) : (xdec ? BLOCK_4X8 : BLOCK_8X8);
  bsizex = 3 - xdec;
  bsizey = 3 - ydec;
#if CONFIG_CDEF_SINGLEPASS
  if (dirinit && pri_strength == 0 && sec_strength == 0)
#else
  if (!skip_dering)
#endif
  {
#if CONFIG_CDEF_SINGLEPASS
    // If we're here, both primary and secondary strengths are 0, and
    // we still haven't written anything to y[] yet, so we just copy
    // the input to y[]. This is necessary only for av1_cdef_search()
    // and only av1_cdef_search() sets dirinit.
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
#else
    if (pli == 0) {
      if (!dirinit || !*dirinit) {
        for (bi = 0; bi < cdef_count; bi++) {
          by = dlist[bi].by;
          bx = dlist[bi].bx;
          dir[by][bx] = cdef_find_dir(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                                      CDEF_BSTRIDE, &var[by][bx], coeff_shift);
        }
        if (dirinit) *dirinit = 1;
      }
    }
    // Only run dering for non-zero threshold (which is always the case for
    // 4:2:2 or 4:4:0). If we don't dering, we still need to eventually write
    // something out in y[] later.
    if (threshold != 0) {
      assert(bsize == BLOCK_8X8 || bsize == BLOCK_4X4);
      for (bi = 0; bi < cdef_count; bi++) {
        int t = !filter_skip && dlist[bi].skip ? 0 : threshold;
        by = dlist[bi].by;
        bx = dlist[bi].bx;
        (cdef_direction[bsize == BLOCK_8X8])(
            &y[bi << (bsizex + bsizey)], 1 << bsizex,
            &in[(by * CDEF_BSTRIDE << bsizey) + (bx << bsizex)],
            pli ? t : adjust_strength(t, var[by][bx]), dir[by][bx],
            pri_damping);
      }
    }
  }

  if (sec_strength) {
    if (threshold && !skip_dering)
      copy_block_16bit_to_16bit(in, CDEF_BSTRIDE, y, dlist, cdef_count, bsize);
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      int py = by << bsizey;
      int px = bx << bsizex;

      if (!filter_skip && dlist[bi].skip) continue;
      if (!dst || hbd) {
        // 16 bit destination if high bitdepth or 8 bit destination not given
        (!threshold || (dir[by][bx] < 4 && dir[by][bx]) ? aom_clpf_block_hbd
                                                        : aom_clpf_hblock_hbd)(
            dst ? (uint16_t *)dst + py * dstride + px
                : &y[bi << (bsizex + bsizey)],
            in + py * CDEF_BSTRIDE + px, dst && hbd ? dstride : 1 << bsizex,
            CDEF_BSTRIDE, 1 << bsizex, 1 << bsizey, sec_strength << coeff_shift,
            sec_damping);
      } else {
        // Do clpf and write the result to an 8 bit destination
        (!threshold || (dir[by][bx] < 4 && dir[by][bx]) ? aom_clpf_block
                                                        : aom_clpf_hblock)(
            dst + py * dstride + px, in + py * CDEF_BSTRIDE + px, dstride,
            CDEF_BSTRIDE, 1 << bsizex, 1 << bsizey, sec_strength << coeff_shift,
            sec_damping);
      }
    }
  } else if (threshold != 0) {
    // No clpf, so copy instead
    if (hbd) {
      copy_block_16bit_to_16bit((uint16_t *)dst, dstride, y, dlist, cdef_count,
                                bsize);
    } else {
      copy_block_16bit_to_8bit(dst, dstride, y, dlist, cdef_count, bsize);
    }
  } else if (dirinit) {
    // If we're here, both dering and clpf are off, and we still haven't written
    // anything to y[] yet, so we just copy the input to y[]. This is necessary
    // only for av1_cdef_search() and only av1_cdef_search() sets dirinit.
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
#endif
      int iy, ix;
      // TODO(stemidts/jmvalin): SIMD optimisations
      for (iy = 0; iy < 1 << bsizey; iy++)
        for (ix = 0; ix < 1 << bsizex; ix++)
#if CONFIG_CDEF_SINGLEPASS
          dst16[(bi << (bsizex + bsizey)) + (iy << bsizex) + ix] =
#else
          y[(bi << (bsizex + bsizey)) + (iy << bsizex) + ix] =
#endif
              in[((by << bsizey) + iy) * CDEF_BSTRIDE + (bx << bsizex) + ix];
    }
#if CONFIG_CDEF_SINGLEPASS
    return;
#endif
  }

#if CONFIG_CDEF_SINGLEPASS
  if (pli == 0) {
    if (!dirinit || !*dirinit) {
      for (bi = 0; bi < cdef_count; bi++) {
        by = dlist[bi].by;
        bx = dlist[bi].bx;
        dir[by][bx] = cdef_find_dir(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                                    CDEF_BSTRIDE, &var[by][bx], coeff_shift);
      }
      if (dirinit) *dirinit = 1;
    }
  }

  assert(bsize == BLOCK_8X8 || bsize == BLOCK_4X4);
  for (bi = 0; bi < cdef_count; bi++) {
    int t = !filter_skip && dlist[bi].skip ? 0 : pri_strength;
    int s = !filter_skip && dlist[bi].skip ? 0 : sec_strength;
    by = dlist[bi].by;
    bx = dlist[bi].bx;
    if (dst8)
      cdef_filter_block(
          &dst8[(by << bsizey) * dstride + (bx << bsizex)], NULL, dstride,
          &in[(by * CDEF_BSTRIDE << bsizey) + (bx << bsizex)],
          (pli ? t : adjust_strength(t, var[by][bx])), s, t ? dir[by][bx] : 0,
          pri_damping, sec_damping, bsize, (256 << coeff_shift) - 1);
    else
      cdef_filter_block(
          NULL,
          &dst16[dirinit ? bi << (bsizex + bsizey)
                         : (by << bsizey) * dstride + (bx << bsizex)],
          dirinit ? 1 << bsizex : dstride,
          &in[(by * CDEF_BSTRIDE << bsizey) + (bx << bsizex)],
          (pli ? t : adjust_strength(t, var[by][bx])), s, t ? dir[by][bx] : 0,
          pri_damping, sec_damping, bsize, (256 << coeff_shift) - 1);
  }
#endif
}
