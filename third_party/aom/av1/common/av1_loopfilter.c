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

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"

#if CONFIG_LOOPFILTER_LEVEL
static const SEG_LVL_FEATURES seg_lvl_lf_lut[MAX_MB_PLANE][2] = {
  { SEG_LVL_ALT_LF_Y_V, SEG_LVL_ALT_LF_Y_H },
  { SEG_LVL_ALT_LF_U, SEG_LVL_ALT_LF_U },
  { SEG_LVL_ALT_LF_V, SEG_LVL_ALT_LF_V }
};

#if CONFIG_EXT_DELTA_Q
static const int delta_lf_id_lut[MAX_MB_PLANE][2] = {
  { 0, 1 }, { 2, 2 }, { 3, 3 }
};
#endif  // CONFIG_EXT_DELTA_Q
#endif  // CONFIG_LOOPFILTER_LEVEL

#if CONFIG_LPF_DIRECT
static void pick_filter_pixel_left(uint8_t *const src, uint8_t *const line,
                                   int *const orig_pos, int length, int row,
                                   int col, int width, int height, int pitch,
                                   int pivot, int direct) {
  int i;
  int pos = row * pitch + col;

  for (i = 0; i < length; ++i) {
    int dy = 0;
    switch (direct) {
      case VERT_HORZ: dy = 0; break;
      case DEGREE_45: dy = 1; break;
      case DEGREE_135: dy = -1; break;
    }
    col -= 1;
    row += dy;
    if (col >= 0 && col < width && row >= 0 && row < height) {
      pos = row * pitch + col;
      line[pivot - 1 - i] = src[pos];
      orig_pos[pivot - 1 - i] = pos;
    }
  }
}

static void pick_filter_pixel_right(uint8_t *const src, uint8_t *const line,
                                    int *const orig_pos, int length, int row,
                                    int col, int width, int height, int pitch,
                                    int pivot, int direct) {
  int i;
  int pos = row * pitch + col;

  line[pivot] = src[pos];
  orig_pos[pivot] = pos;

  for (i = 1; i < length; ++i) {
    int dy = 0;
    switch (direct) {
      case VERT_HORZ: dy = 0; break;
      case DEGREE_45: dy = -1; break;
      case DEGREE_135: dy = 1; break;
    }
    col += 1;
    row += dy;
    if (col >= 0 && col < width && row >= 0 && row < height) {
      pos = row * pitch + col;
      line[pivot + i] = src[pos];
      orig_pos[pivot + i] = pos;
    }
  }
}

static void pick_filter_pixel_above(uint8_t *const src, uint8_t *const line,
                                    int *const orig_pos, int length, int row,
                                    int col, int width, int height, int pitch,
                                    int pivot, int direct) {
  int i;
  int pos = row * pitch + col;

  for (i = 0; i < length; ++i) {
    int dx = 0;
    switch (direct) {
      case VERT_HORZ: dx = 0; break;
      case DEGREE_45: dx = 1; break;
      case DEGREE_135: dx = -1; break;
    }
    col += dx;
    row -= 1;
    if (col >= 0 && col < width && row >= 0 && row < height) {
      pos = row * pitch + col;
      line[pivot - 1 - i] = src[pos];
      orig_pos[pivot - 1 - i] = pos;
    }
  }
}

static void pick_filter_pixel_bot(uint8_t *const src, uint8_t *const line,
                                  int *const orig_pos, int length, int row,
                                  int col, int width, int height, int pitch,
                                  int pivot, int direct) {
  int i;
  int pos = row * pitch + col;

  line[pivot] = src[pos];
  orig_pos[pivot] = pos;

  for (i = 1; i < length; ++i) {
    int dx = 0;
    switch (direct) {
      case VERT_HORZ: dx = 0; break;
      case DEGREE_45: dx = -1; break;
      case DEGREE_135: dx = 1; break;
    }
    col += dx;
    row += 1;
    if (col >= 0 && col < width && row >= 0 && row < height) {
      pos = row * pitch + col;
      line[pivot + i] = src[pos];
      orig_pos[pivot + i] = pos;
    }
  }
}

static void pick_filter_block_vert(uint8_t *const src, uint8_t *const block,
                                   int *const orig_pos, int length, int row,
                                   int col, int width, int height, int pitch,
                                   int pivot, int line_length, int unit,
                                   int direct) {
  int i;
  for (i = 0; i < 8 * unit; ++i) {
    pick_filter_pixel_left(src, block + i * line_length,
                           orig_pos + i * line_length, length, row + i, col,
                           width, height, pitch, pivot, direct);
    pick_filter_pixel_right(src, block + i * line_length,
                            orig_pos + i * line_length, length, row + i, col,
                            width, height, pitch, pivot, direct);
  }
}

static void pick_filter_block_horz(uint8_t *const src, uint8_t *const block,
                                   int *const orig_pos, int length, int row,
                                   int col, int width, int height, int pitch,
                                   int pivot, int line_length, int unit,
                                   int direct) {
  int i, j;
  int num = 8 * unit;
  for (i = 0; i < num; ++i) {
    pick_filter_pixel_above(src, block + i * line_length,
                            orig_pos + i * line_length, length, row, col + i,
                            width, height, pitch, pivot, direct);
    pick_filter_pixel_bot(src, block + i * line_length,
                          orig_pos + i * line_length, length, row, col + i,
                          width, height, pitch, pivot, direct);
  }

  // rearrange block
  // TODO(chengchen): make it in-place or a stand alone function
  uint8_t tmp_block[256];
  int tmp_pos[256];
  for (i = 0; i < 256; ++i) {
    tmp_block[i] = 0;
    tmp_pos[i] = -1;
  }
  for (i = 0; i < num; ++i) {
    for (j = 0; j < line_length; ++j) {
      tmp_block[j * line_length + i] = block[i * line_length + j];
      tmp_pos[j * line_length + i] = orig_pos[i * line_length + j];
    }
  }
  for (i = 0; i < 256; ++i) {
    block[i] = tmp_block[i];
    orig_pos[i] = tmp_pos[i];
  }
}

static int compute_block_grad(uint8_t *const src, int length, int row, int col,
                              int width, int height, int pitch, int unit,
                              int vert_or_horz, int direct) {
  int i, j;
  int r0, c0, pos0, r1 = 0, c1 = 0, pos1;
  int sum_grad = 0;
  for (i = 0; i < 8 * unit; ++i) {
    // vert_or_horz: 0 vertical edge, 1 horizontal edge
    r0 = vert_or_horz ? row : row + i;
    c0 = vert_or_horz ? col + i : col;
    pos0 = r0 * pitch + c0;

    for (j = 0; j < length; ++j) {
      if (vert_or_horz == 0) {
        switch (direct) {
          case VERT_HORZ: r1 = r0; break;
          case DEGREE_45: r1 = r0 + 1; break;
          case DEGREE_135: r1 = r0 - 1; break;
        }
        c1 = c0 - 1;
      } else {
        r1 = r0 - 1;
        switch (direct) {
          case VERT_HORZ: c1 = c0; break;
          case DEGREE_45: c1 = c0 + 1; break;
          case DEGREE_135: c1 = c0 - 1; break;
        }
      }
      pos1 = r1 * pitch + c1;

      if (r0 >= 0 && r0 < height && c0 >= 0 && c0 < width && r1 >= 0 &&
          r1 < height && c1 >= 0 && c1 < width) {
        sum_grad += abs(src[pos1] - src[pos0]);
      } else {
        sum_grad += 255;  // penalize unreachable boundary
      }
      r0 = r1;
      c0 = c1;
      pos0 = pos1;
    }

    r0 = vert_or_horz ? row : row + i;
    c0 = vert_or_horz ? col + i : col;
    pos0 = r0 * pitch + c0;

    for (j = 0; j < length - 1; ++j) {
      if (vert_or_horz == 0) {
        switch (direct) {
          case VERT_HORZ: r1 = r0; break;
          case DEGREE_45: r1 = r0 - 1; break;
          case DEGREE_135: r1 = r0 + 1; break;
        }
        c1 = c0 + 1;
      } else {
        r1 = r0 + 1;
        switch (direct) {
          case VERT_HORZ: c1 = c0; break;
          case DEGREE_45: c1 = c0 - 1; break;
          case DEGREE_135: c1 = c0 + 1; break;
        }
      }
      pos1 = r1 * pitch + c1;

      if (r0 >= 0 && r0 < height && c0 >= 0 && c0 < width && r1 >= 0 &&
          r1 < height && c1 >= 0 && c1 < width) {
        sum_grad += abs(src[pos1] - src[pos0]);
      } else {
        sum_grad += 255;  // penalize unreachable boundary
      }
      r0 = r1;
      c0 = c1;
      pos0 = pos1;
    }
  }

  return sum_grad;
}

static int pick_min_grad_direct(uint8_t *const src, int length, int row,
                                int col, int width, int height, int pitch,
                                int unit, int vert_or_horz) {
  int direct = VERT_HORZ;
  int min_grad = INT_MAX, sum_grad = 0;

  int degree;
  for (degree = 0; degree < FILTER_DEGREES; ++degree) {
    // compute abs gradient along each line for the filter block
    sum_grad = compute_block_grad(src, length, row, col, width, height, pitch,
                                  unit, vert_or_horz, degree);
    if (sum_grad < min_grad) {
      min_grad = sum_grad;
      direct = degree;
    }
  }

  return direct;
}
#endif  // CONFIG_LPF_DIRECT

#define PARALLEL_DEBLOCKING_15TAPLUMAONLY 1
#define PARALLEL_DEBLOCKING_DISABLE_15TAP 0
#if CONFIG_DEBLOCK_13TAP
#define PARALLEL_DEBLOCKING_5_TAP_CHROMA 1
#else
#define PARALLEL_DEBLOCKING_5_TAP_CHROMA 0
#endif

#if PARALLEL_DEBLOCKING_5_TAP_CHROMA
extern void aom_lpf_vertical_6_c(uint8_t *s, int pitch, const uint8_t *blimit,
                                 const uint8_t *limit, const uint8_t *thresh);

extern void aom_lpf_horizontal_6_c(uint8_t *s, int p, const uint8_t *blimit,
                                   const uint8_t *limit, const uint8_t *thresh);

extern void aom_highbd_lpf_horizontal_6_c(uint16_t *s, int p,
                                          const uint8_t *blimit,
                                          const uint8_t *limit,
                                          const uint8_t *thresh, int bd);

extern void aom_highbd_lpf_vertical_6_c(uint16_t *s, int pitch,
                                        const uint8_t *blimit,
                                        const uint8_t *limit,
                                        const uint8_t *thresh, int bd);
#endif

// 64 bit masks for left transform size. Each 1 represents a position where
// we should apply a loop filter across the left border of an 8x8 block
// boundary.
//
// In the case of TX_16X16->  ( in low order byte first we end up with
// a mask that looks like this
//
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//
// A loopfilter should be applied to every other 8x8 horizontally.
static const uint64_t left_64x64_txform_mask[TX_SIZES] = {
#if CONFIG_CHROMA_2X2
  0xffffffffffffffffULL,  // TX_2X2
#endif
  0xffffffffffffffffULL,  // TX_4X4
  0xffffffffffffffffULL,  // TX_8x8
  0x5555555555555555ULL,  // TX_16x16
  0x1111111111111111ULL,  // TX_32x32
#if CONFIG_TX64X64
  0x0101010101010101ULL,  // TX_64x64
#endif                    // CONFIG_TX64X64
};

// 64 bit masks for above transform size. Each 1 represents a position where
// we should apply a loop filter across the top border of an 8x8 block
// boundary.
//
// In the case of TX_32x32 ->  ( in low order byte first we end up with
// a mask that looks like this
//
//    11111111
//    00000000
//    00000000
//    00000000
//    11111111
//    00000000
//    00000000
//    00000000
//
// A loopfilter should be applied to every other 4 the row vertically.
static const uint64_t above_64x64_txform_mask[TX_SIZES] = {
#if CONFIG_CHROMA_2X2
  0xffffffffffffffffULL,  // TX_4X4
#endif
  0xffffffffffffffffULL,  // TX_4X4
  0xffffffffffffffffULL,  // TX_8x8
  0x00ff00ff00ff00ffULL,  // TX_16x16
  0x000000ff000000ffULL,  // TX_32x32
#if CONFIG_TX64X64
  0x00000000000000ffULL,  // TX_64x64
#endif                    // CONFIG_TX64X64
};

// 64 bit masks for prediction sizes (left). Each 1 represents a position
// where left border of an 8x8 block. These are aligned to the right most
// appropriate bit, and then shifted into place.
//
// In the case of TX_16x32 ->  ( low order byte first ) we end up with
// a mask that looks like this :
//
//  10000000
//  10000000
//  10000000
//  10000000
//  00000000
//  00000000
//  00000000
//  00000000
static const uint64_t left_prediction_mask[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0000000000000001ULL,  // BLOCK_2X2,
  0x0000000000000001ULL,  // BLOCK_2X4,
  0x0000000000000001ULL,  // BLOCK_4X2,
#endif
  0x0000000000000001ULL,  // BLOCK_4X4,
  0x0000000000000001ULL,  // BLOCK_4X8,
  0x0000000000000001ULL,  // BLOCK_8X4,
  0x0000000000000001ULL,  // BLOCK_8X8,
  0x0000000000000101ULL,  // BLOCK_8X16,
  0x0000000000000001ULL,  // BLOCK_16X8,
  0x0000000000000101ULL,  // BLOCK_16X16,
  0x0000000001010101ULL,  // BLOCK_16X32,
  0x0000000000000101ULL,  // BLOCK_32X16,
  0x0000000001010101ULL,  // BLOCK_32X32,
  0x0101010101010101ULL,  // BLOCK_32X64,
  0x0000000001010101ULL,  // BLOCK_64X32,
  0x0101010101010101ULL,  // BLOCK_64X64,
  0x0000000000000101ULL,  // BLOCK_4X16,
  0x0000000000000001ULL,  // BLOCK_16X4,
  0x0000000001010101ULL,  // BLOCK_8X32,
  0x0000000000000001ULL,  // BLOCK_32X8,
  0x0101010101010101ULL,  // BLOCK_16X64,
  0x0000000000000101ULL,  // BLOCK_64X16
};

// 64 bit mask to shift and set for each prediction size.
static const uint64_t above_prediction_mask[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0000000000000001ULL,  // BLOCK_2X2
  0x0000000000000001ULL,  // BLOCK_2X4
  0x0000000000000001ULL,  // BLOCK_4X2
#endif
  0x0000000000000001ULL,  // BLOCK_4X4
  0x0000000000000001ULL,  // BLOCK_4X8
  0x0000000000000001ULL,  // BLOCK_8X4
  0x0000000000000001ULL,  // BLOCK_8X8
  0x0000000000000001ULL,  // BLOCK_8X16,
  0x0000000000000003ULL,  // BLOCK_16X8
  0x0000000000000003ULL,  // BLOCK_16X16
  0x0000000000000003ULL,  // BLOCK_16X32,
  0x000000000000000fULL,  // BLOCK_32X16,
  0x000000000000000fULL,  // BLOCK_32X32,
  0x000000000000000fULL,  // BLOCK_32X64,
  0x00000000000000ffULL,  // BLOCK_64X32,
  0x00000000000000ffULL,  // BLOCK_64X64,
  0x0000000000000001ULL,  // BLOCK_4X16,
  0x0000000000000003ULL,  // BLOCK_16X4,
  0x0000000000000001ULL,  // BLOCK_8X32,
  0x000000000000000fULL,  // BLOCK_32X8,
  0x0000000000000003ULL,  // BLOCK_16X64,
  0x00000000000000ffULL,  // BLOCK_64X16
};
// 64 bit mask to shift and set for each prediction size. A bit is set for
// each 8x8 block that would be in the top left most block of the given block
// size in the 64x64 block.
static const uint64_t size_mask[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0000000000000001ULL,  // BLOCK_2X2
  0x0000000000000001ULL,  // BLOCK_2X4
  0x0000000000000001ULL,  // BLOCK_4X2
#endif
  0x0000000000000001ULL,  // BLOCK_4X4
  0x0000000000000001ULL,  // BLOCK_4X8
  0x0000000000000001ULL,  // BLOCK_8X4
  0x0000000000000001ULL,  // BLOCK_8X8
  0x0000000000000101ULL,  // BLOCK_8X16,
  0x0000000000000003ULL,  // BLOCK_16X8
  0x0000000000000303ULL,  // BLOCK_16X16
  0x0000000003030303ULL,  // BLOCK_16X32,
  0x0000000000000f0fULL,  // BLOCK_32X16,
  0x000000000f0f0f0fULL,  // BLOCK_32X32,
  0x0f0f0f0f0f0f0f0fULL,  // BLOCK_32X64,
  0x00000000ffffffffULL,  // BLOCK_64X32,
  0xffffffffffffffffULL,  // BLOCK_64X64,
  0x0000000000000101ULL,  // BLOCK_4X16,
  0x0000000000000003ULL,  // BLOCK_16X4,
  0x0000000001010101ULL,  // BLOCK_8X32,
  0x000000000000000fULL,  // BLOCK_32X8,
  0x0303030303030303ULL,  // BLOCK_16X64,
  0x000000000000ffffULL,  // BLOCK_64X16
};

// These are used for masking the left and above 32x32 borders.
static const uint64_t left_border = 0x1111111111111111ULL;
static const uint64_t above_border = 0x000000ff000000ffULL;

// 16 bit masks for uv transform sizes.
static const uint16_t left_64x64_txform_mask_uv[TX_SIZES] = {
#if CONFIG_CHROMA_2X2
  0xffff,  // TX_2X2
#endif
  0xffff,  // TX_4X4
  0xffff,  // TX_8x8
  0x5555,  // TX_16x16
  0x1111,  // TX_32x32
#if CONFIG_TX64X64
  0x0101,  // TX_64x64, never used
#endif     // CONFIG_TX64X64
};

static const uint16_t above_64x64_txform_mask_uv[TX_SIZES] = {
#if CONFIG_CHROMA_2X2
  0xffff,  // TX_2X2
#endif
  0xffff,  // TX_4X4
  0xffff,  // TX_8x8
  0x0f0f,  // TX_16x16
  0x000f,  // TX_32x32
#if CONFIG_TX64X64
  0x0003,  // TX_64x64, never used
#endif     // CONFIG_TX64X64
};

// 16 bit left mask to shift and set for each uv prediction size.
static const uint16_t left_prediction_mask_uv[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0001,  // BLOCK_2X2,
  0x0001,  // BLOCK_2X4,
  0x0001,  // BLOCK_4X2,
#endif
  0x0001,  // BLOCK_4X4,
  0x0001,  // BLOCK_4X8,
  0x0001,  // BLOCK_8X4,
  0x0001,  // BLOCK_8X8,
  0x0001,  // BLOCK_8X16,
  0x0001,  // BLOCK_16X8,
  0x0001,  // BLOCK_16X16,
  0x0011,  // BLOCK_16X32,
  0x0001,  // BLOCK_32X16,
  0x0011,  // BLOCK_32X32,
  0x1111,  // BLOCK_32X64
  0x0011,  // BLOCK_64X32,
  0x1111,  // BLOCK_64X64,
  0x0001,  // BLOCK_4X16,
  0x0001,  // BLOCK_16X4,
  0x0011,  // BLOCK_8X32,
  0x0001,  // BLOCK_32X8,
  0x1111,  // BLOCK_16X64,
  0x0001,  // BLOCK_64X16,
};

// 16 bit above mask to shift and set for uv each prediction size.
static const uint16_t above_prediction_mask_uv[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0001,  // BLOCK_2X2
  0x0001,  // BLOCK_2X4
  0x0001,  // BLOCK_4X2
#endif
  0x0001,  // BLOCK_4X4
  0x0001,  // BLOCK_4X8
  0x0001,  // BLOCK_8X4
  0x0001,  // BLOCK_8X8
  0x0001,  // BLOCK_8X16,
  0x0001,  // BLOCK_16X8
  0x0001,  // BLOCK_16X16
  0x0001,  // BLOCK_16X32,
  0x0003,  // BLOCK_32X16,
  0x0003,  // BLOCK_32X32,
  0x0003,  // BLOCK_32X64,
  0x000f,  // BLOCK_64X32,
  0x000f,  // BLOCK_64X64,
  0x0001,  // BLOCK_4X16,
  0x0001,  // BLOCK_16X4,
  0x0001,  // BLOCK_8X32,
  0x0003,  // BLOCK_32X8,
  0x0001,  // BLOCK_16X64,
  0x000f,  // BLOCK_64X16
};

// 64 bit mask to shift and set for each uv prediction size
static const uint16_t size_mask_uv[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  0x0001,  // BLOCK_2X2
  0x0001,  // BLOCK_2X4
  0x0001,  // BLOCK_4X2
#endif
  0x0001,  // BLOCK_4X4
  0x0001,  // BLOCK_4X8
  0x0001,  // BLOCK_8X4
  0x0001,  // BLOCK_8X8
  0x0001,  // BLOCK_8X16,
  0x0001,  // BLOCK_16X8
  0x0001,  // BLOCK_16X16
  0x0011,  // BLOCK_16X32,
  0x0003,  // BLOCK_32X16,
  0x0033,  // BLOCK_32X32,
  0x3333,  // BLOCK_32X64,
  0x00ff,  // BLOCK_64X32,
  0xffff,  // BLOCK_64X64,
  0x0001,  // BLOCK_4X16,
  0x0001,  // BLOCK_16X4,
  0x0011,  // BLOCK_8X32,
  0x0003,  // BLOCK_32X8,
  0x1111,  // BLOCK_16X64,
  0x000f,  // BLOCK_64X16
};
static const uint16_t left_border_uv = 0x1111;
static const uint16_t above_border_uv = 0x000f;

static const int mode_lf_lut[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // INTRA_MODES
  0,
#if CONFIG_SMOOTH_HV
  0, 0,
#endif         // CONFIG_SMOOTH_HV
  1, 1, 0, 1,  // INTER_MODES (ZEROMV == 0)
#if CONFIG_COMPOUND_SINGLEREF
  // 1, 1, 1, 1, 1,       // INTER_SINGLEREF_COMP_MODES
  // NOTE(zoeliu): Remove SR_NEAREST_NEWMV
  1, 1, 1, 1,             // INTER_SINGLEREF_COMP_MODES
#endif                    // CONFIG_COMPOUND_SINGLEREF
  1, 1, 1, 1, 1, 1, 0, 1  // INTER_COMPOUND_MODES (ZERO_ZEROMV == 0)
};

static void update_sharpness(loop_filter_info_n *lfi, int sharpness_lvl) {
  int lvl;

  // For each possible value for the loop filter fill out limits
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++) {
    // Set loop filter parameters that control sharpness.
    int block_inside_limit = lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));

    if (sharpness_lvl > 0) {
      if (block_inside_limit > (9 - sharpness_lvl))
        block_inside_limit = (9 - sharpness_lvl);
    }

    if (block_inside_limit < 1) block_inside_limit = 1;

    memset(lfi->lfthr[lvl].lim, block_inside_limit, SIMD_WIDTH);
    memset(lfi->lfthr[lvl].mblim, (2 * (lvl + 2) + block_inside_limit),
           SIMD_WIDTH);
  }
}
#if CONFIG_EXT_DELTA_Q
static uint8_t get_filter_level(const AV1_COMMON *cm,
                                const loop_filter_info_n *lfi_n,
#if CONFIG_LOOPFILTER_LEVEL
                                const int dir_idx, int plane,
#endif
#if CONFIG_LPF_SB
                                int mi_row, int mi_col,
#endif
                                const MB_MODE_INFO *mbmi) {
#if CONFIG_LPF_SB
  return cm->mi[mi_row * cm->mi_stride + mi_col].mbmi.filt_lvl;
#endif

#if CONFIG_SUPERTX
  const int segment_id = AOMMIN(mbmi->segment_id, mbmi->segment_id_supertx);
  assert(
      IMPLIES(supertx_enabled(mbmi), mbmi->segment_id_supertx != MAX_SEGMENTS));
  assert(IMPLIES(supertx_enabled(mbmi),
                 mbmi->segment_id_supertx <= mbmi->segment_id));
#else
  const int segment_id = mbmi->segment_id;
#endif  // CONFIG_SUPERTX
  if (cm->delta_lf_present_flag) {
#if CONFIG_LOOPFILTER_LEVEL
    int delta_lf;
    if (cm->delta_lf_multi) {
      const int delta_lf_idx = delta_lf_id_lut[plane][dir_idx];
      delta_lf = mbmi->curr_delta_lf[delta_lf_idx];
    } else {
      delta_lf = mbmi->current_delta_lf_from_base;
    }
    int lvl_seg =
        clamp(delta_lf + cm->lf.filter_level[dir_idx], 0, MAX_LOOP_FILTER);
#else
    int lvl_seg = clamp(mbmi->current_delta_lf_from_base + cm->lf.filter_level,
                        0, MAX_LOOP_FILTER);
#endif
    const int scale = 1 << (lvl_seg >> 5);
#if CONFIG_LOOPFILTER_LEVEL
    assert(plane >= 0 && plane <= 2);
    const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir_idx];
    if (segfeature_active(&cm->seg, segment_id, seg_lf_feature_id)) {
      const int data = get_segdata(&cm->seg, segment_id, seg_lf_feature_id);
      lvl_seg =
          clamp(cm->seg.abs_delta == SEGMENT_ABSDATA ? data : lvl_seg + data, 0,
                MAX_LOOP_FILTER);
    }
#else
    if (segfeature_active(&cm->seg, segment_id, SEG_LVL_ALT_LF)) {
      const int data = get_segdata(&cm->seg, segment_id, SEG_LVL_ALT_LF);
      lvl_seg =
          clamp(cm->seg.abs_delta == SEGMENT_ABSDATA ? data : lvl_seg + data, 0,
                MAX_LOOP_FILTER);
    }
#endif  // CONFIG_LOOPFILTER_LEVEL

    if (cm->lf.mode_ref_delta_enabled) {
      lvl_seg += cm->lf.ref_deltas[mbmi->ref_frame[0]] * scale;
      if (mbmi->ref_frame[0] > INTRA_FRAME)
        lvl_seg += cm->lf.mode_deltas[mode_lf_lut[mbmi->mode]] * scale;
      lvl_seg = clamp(lvl_seg, 0, MAX_LOOP_FILTER);
    }
    return lvl_seg;
  } else {
#if CONFIG_LOOPFILTER_LEVEL
    return lfi_n
        ->lvl[segment_id][dir_idx][mbmi->ref_frame[0]][mode_lf_lut[mbmi->mode]];
#else
    return lfi_n->lvl[segment_id][mbmi->ref_frame[0]][mode_lf_lut[mbmi->mode]];
#endif
  }
}
#else
static uint8_t get_filter_level(const loop_filter_info_n *lfi_n,
                                const MB_MODE_INFO *mbmi) {
#if CONFIG_SUPERTX
  const int segment_id = AOMMIN(mbmi->segment_id, mbmi->segment_id_supertx);
  assert(
      IMPLIES(supertx_enabled(mbmi), mbmi->segment_id_supertx != MAX_SEGMENTS));
  assert(IMPLIES(supertx_enabled(mbmi),
                 mbmi->segment_id_supertx <= mbmi->segment_id));
#else
  const int segment_id = mbmi->segment_id;
#endif  // CONFIG_SUPERTX
  return lfi_n->lvl[segment_id][mbmi->ref_frame[0]][mode_lf_lut[mbmi->mode]];
}
#endif

void av1_loop_filter_init(AV1_COMMON *cm) {
  assert(MB_MODE_COUNT == NELEMENTS(mode_lf_lut));
  loop_filter_info_n *lfi = &cm->lf_info;
  struct loopfilter *lf = &cm->lf;
  int lvl;

  // init limits for given sharpness
  update_sharpness(lfi, lf->sharpness_level);
  lf->last_sharpness_level = lf->sharpness_level;

  // init hev threshold const vectors
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++)
    memset(lfi->lfthr[lvl].hev_thr, (lvl >> 4), SIMD_WIDTH);
}

#if CONFIG_LPF_SB
void av1_loop_filter_sb_level_init(AV1_COMMON *cm, int mi_row, int mi_col,
                                   int lvl) {
  const int mi_row_start = AOMMAX(0, mi_row - FILT_BOUNDARY_MI_OFFSET);
  const int mi_col_start = AOMMAX(0, mi_col - FILT_BOUNDARY_MI_OFFSET);
  const int mi_row_range = mi_row - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_col_range = mi_col - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
  const int mi_row_end = AOMMIN(mi_row_range, cm->mi_rows);
  const int mi_col_end = AOMMIN(mi_col_range, cm->mi_cols);

  int row, col;
  for (row = mi_row_start; row < mi_row_end; ++row) {
    for (col = mi_col_start; col < mi_col_end; ++col) {
      // Note: can't use cm->mi_grid_visible. Because for each partition,
      // all visible pointers will point to the first of the partition.
      cm->mi[row * cm->mi_stride + col].mbmi.filt_lvl = lvl;
    }
  }
}
#endif  // CONFIG_LPF_SB

void av1_loop_filter_frame_init(AV1_COMMON *cm, int default_filt_lvl,
                                int default_filt_lvl_r
#if CONFIG_LOOPFILTER_LEVEL
                                ,
                                int plane
#endif
                                ) {
  int seg_id;
  // n_shift is the multiplier for lf_deltas
  // the multiplier is 1 for when filter_lvl is between 0 and 31;
  // 2 when filter_lvl is between 32 and 63
  int scale = 1 << (default_filt_lvl >> 5);
  loop_filter_info_n *const lfi = &cm->lf_info;
  struct loopfilter *const lf = &cm->lf;
  const struct segmentation *const seg = &cm->seg;

  // update limits if sharpness has changed
  if (lf->last_sharpness_level != lf->sharpness_level) {
    update_sharpness(lfi, lf->sharpness_level);
    lf->last_sharpness_level = lf->sharpness_level;
  }

  for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {
    for (int dir = 0; dir < 2; ++dir) {
      int lvl_seg = (dir == 0) ? default_filt_lvl : default_filt_lvl_r;
#if CONFIG_LOOPFILTER_LEVEL
      assert(plane >= 0 && plane <= 2);
      const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir];
      if (segfeature_active(seg, seg_id, seg_lf_feature_id)) {
        const int data = get_segdata(&cm->seg, seg_id, seg_lf_feature_id);
        lvl_seg = clamp(
            seg->abs_delta == SEGMENT_ABSDATA ? data : default_filt_lvl + data,
            0, MAX_LOOP_FILTER);
      }
#else
      if (segfeature_active(seg, seg_id, SEG_LVL_ALT_LF)) {
        const int data = get_segdata(seg, seg_id, SEG_LVL_ALT_LF);
        lvl_seg = clamp(
            seg->abs_delta == SEGMENT_ABSDATA ? data : default_filt_lvl + data,
            0, MAX_LOOP_FILTER);
      }
#endif  // CONFIG_LOOPFILTER_LEVEL

      if (!lf->mode_ref_delta_enabled) {
// we could get rid of this if we assume that deltas are set to
// zero when not in use; encoder always uses deltas
#if CONFIG_LOOPFILTER_LEVEL
        memset(lfi->lvl[seg_id][dir], lvl_seg, sizeof(lfi->lvl[seg_id][dir]));
#else
        memset(lfi->lvl[seg_id], lvl_seg, sizeof(lfi->lvl[seg_id]));
#endif  // CONFIG_LOOPFILTER_LEVEL
      } else {
        int ref, mode;
#if CONFIG_LOOPFILTER_LEVEL
        scale = 1 << (lvl_seg >> 5);

        const int intra_lvl = lvl_seg + lf->ref_deltas[INTRA_FRAME] * scale;
        lfi->lvl[seg_id][dir][INTRA_FRAME][0] =
            clamp(intra_lvl, 0, MAX_LOOP_FILTER);

        for (ref = LAST_FRAME; ref < TOTAL_REFS_PER_FRAME; ++ref) {
          for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
            const int inter_lvl = lvl_seg + lf->ref_deltas[ref] * scale +
                                  lf->mode_deltas[mode] * scale;
            lfi->lvl[seg_id][dir][ref][mode] =
                clamp(inter_lvl, 0, MAX_LOOP_FILTER);
          }
        }
#else
        (void)default_filt_lvl_r;
        const int intra_lvl = lvl_seg + lf->ref_deltas[INTRA_FRAME] * scale;
        lfi->lvl[seg_id][INTRA_FRAME][0] = clamp(intra_lvl, 0, MAX_LOOP_FILTER);

        for (ref = LAST_FRAME; ref < TOTAL_REFS_PER_FRAME; ++ref) {
          for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
            const int inter_lvl = lvl_seg + lf->ref_deltas[ref] * scale +
                                  lf->mode_deltas[mode] * scale;
            lfi->lvl[seg_id][ref][mode] = clamp(inter_lvl, 0, MAX_LOOP_FILTER);
          }
        }
#endif
      }
    }
  }
}

static void filter_selectively_vert_row2(int subsampling_factor, uint8_t *s,
                                         int pitch, unsigned int mask_16x16_l,
                                         unsigned int mask_8x8_l,
                                         unsigned int mask_4x4_l,
                                         unsigned int mask_4x4_int_l,
                                         const loop_filter_info_n *lfi_n,
                                         const uint8_t *lfl) {
  const int mask_shift = subsampling_factor ? 4 : 8;
  const int mask_cutoff = subsampling_factor ? 0xf : 0xff;
  const int lfl_forward = subsampling_factor ? 4 : 8;

  unsigned int mask_16x16_0 = mask_16x16_l & mask_cutoff;
  unsigned int mask_8x8_0 = mask_8x8_l & mask_cutoff;
  unsigned int mask_4x4_0 = mask_4x4_l & mask_cutoff;
  unsigned int mask_4x4_int_0 = mask_4x4_int_l & mask_cutoff;
  unsigned int mask_16x16_1 = (mask_16x16_l >> mask_shift) & mask_cutoff;
  unsigned int mask_8x8_1 = (mask_8x8_l >> mask_shift) & mask_cutoff;
  unsigned int mask_4x4_1 = (mask_4x4_l >> mask_shift) & mask_cutoff;
  unsigned int mask_4x4_int_1 = (mask_4x4_int_l >> mask_shift) & mask_cutoff;
  unsigned int mask;

  for (mask = mask_16x16_0 | mask_8x8_0 | mask_4x4_0 | mask_4x4_int_0 |
              mask_16x16_1 | mask_8x8_1 | mask_4x4_1 | mask_4x4_int_1;
       mask; mask >>= 1) {
    const loop_filter_thresh *lfi0 = lfi_n->lfthr + *lfl;
    const loop_filter_thresh *lfi1 = lfi_n->lfthr + *(lfl + lfl_forward);

    if (mask & 1) {
      if ((mask_16x16_0 | mask_16x16_1) & 1) {
        if ((mask_16x16_0 & mask_16x16_1) & 1) {
          aom_lpf_vertical_16_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                   lfi0->hev_thr);
        } else if (mask_16x16_0 & 1) {
          aom_lpf_vertical_16(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
        } else {
          aom_lpf_vertical_16(s + 8 * pitch, pitch, lfi1->mblim, lfi1->lim,
                              lfi1->hev_thr);
        }
      }

      if ((mask_8x8_0 | mask_8x8_1) & 1) {
        if ((mask_8x8_0 & mask_8x8_1) & 1) {
          aom_lpf_vertical_8_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                  lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                  lfi1->hev_thr);
        } else if (mask_8x8_0 & 1) {
          aom_lpf_vertical_8(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
        } else {
          aom_lpf_vertical_8(s + 8 * pitch, pitch, lfi1->mblim, lfi1->lim,
                             lfi1->hev_thr);
        }
      }

      if ((mask_4x4_0 | mask_4x4_1) & 1) {
        if ((mask_4x4_0 & mask_4x4_1) & 1) {
          aom_lpf_vertical_4_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                  lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                  lfi1->hev_thr);
        } else if (mask_4x4_0 & 1) {
          aom_lpf_vertical_4(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
        } else {
          aom_lpf_vertical_4(s + 8 * pitch, pitch, lfi1->mblim, lfi1->lim,
                             lfi1->hev_thr);
        }
      }

      if ((mask_4x4_int_0 | mask_4x4_int_1) & 1) {
        if ((mask_4x4_int_0 & mask_4x4_int_1) & 1) {
          aom_lpf_vertical_4_dual(s + 4, pitch, lfi0->mblim, lfi0->lim,
                                  lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                  lfi1->hev_thr);
        } else if (mask_4x4_int_0 & 1) {
          aom_lpf_vertical_4(s + 4, pitch, lfi0->mblim, lfi0->lim,
                             lfi0->hev_thr);
        } else {
          aom_lpf_vertical_4(s + 8 * pitch + 4, pitch, lfi1->mblim, lfi1->lim,
                             lfi1->hev_thr);
        }
      }
    }

    s += 8;
    lfl += 1;
    mask_16x16_0 >>= 1;
    mask_8x8_0 >>= 1;
    mask_4x4_0 >>= 1;
    mask_4x4_int_0 >>= 1;
    mask_16x16_1 >>= 1;
    mask_8x8_1 >>= 1;
    mask_4x4_1 >>= 1;
    mask_4x4_int_1 >>= 1;
  }
}

#if CONFIG_HIGHBITDEPTH
static void highbd_filter_selectively_vert_row2(
    int subsampling_factor, uint16_t *s, int pitch, unsigned int mask_16x16_l,
    unsigned int mask_8x8_l, unsigned int mask_4x4_l,
    unsigned int mask_4x4_int_l, const loop_filter_info_n *lfi_n,
    const uint8_t *lfl, int bd) {
  const int mask_shift = subsampling_factor ? 4 : 8;
  const int mask_cutoff = subsampling_factor ? 0xf : 0xff;
  const int lfl_forward = subsampling_factor ? 4 : 8;

  unsigned int mask_16x16_0 = mask_16x16_l & mask_cutoff;
  unsigned int mask_8x8_0 = mask_8x8_l & mask_cutoff;
  unsigned int mask_4x4_0 = mask_4x4_l & mask_cutoff;
  unsigned int mask_4x4_int_0 = mask_4x4_int_l & mask_cutoff;
  unsigned int mask_16x16_1 = (mask_16x16_l >> mask_shift) & mask_cutoff;
  unsigned int mask_8x8_1 = (mask_8x8_l >> mask_shift) & mask_cutoff;
  unsigned int mask_4x4_1 = (mask_4x4_l >> mask_shift) & mask_cutoff;
  unsigned int mask_4x4_int_1 = (mask_4x4_int_l >> mask_shift) & mask_cutoff;
  unsigned int mask;

  for (mask = mask_16x16_0 | mask_8x8_0 | mask_4x4_0 | mask_4x4_int_0 |
              mask_16x16_1 | mask_8x8_1 | mask_4x4_1 | mask_4x4_int_1;
       mask; mask >>= 1) {
    const loop_filter_thresh *lfi0 = lfi_n->lfthr + *lfl;
    const loop_filter_thresh *lfi1 = lfi_n->lfthr + *(lfl + lfl_forward);

    if (mask & 1) {
      if ((mask_16x16_0 | mask_16x16_1) & 1) {
        if ((mask_16x16_0 & mask_16x16_1) & 1) {
          aom_highbd_lpf_vertical_16_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                          lfi0->hev_thr, bd);
        } else if (mask_16x16_0 & 1) {
          aom_highbd_lpf_vertical_16(s, pitch, lfi0->mblim, lfi0->lim,
                                     lfi0->hev_thr, bd);
        } else {
          aom_highbd_lpf_vertical_16(s + 8 * pitch, pitch, lfi1->mblim,
                                     lfi1->lim, lfi1->hev_thr, bd);
        }
      }

      if ((mask_8x8_0 | mask_8x8_1) & 1) {
        if ((mask_8x8_0 & mask_8x8_1) & 1) {
          aom_highbd_lpf_vertical_8_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                         lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                         lfi1->hev_thr, bd);
        } else if (mask_8x8_0 & 1) {
          aom_highbd_lpf_vertical_8(s, pitch, lfi0->mblim, lfi0->lim,
                                    lfi0->hev_thr, bd);
        } else {
          aom_highbd_lpf_vertical_8(s + 8 * pitch, pitch, lfi1->mblim,
                                    lfi1->lim, lfi1->hev_thr, bd);
        }
      }

      if ((mask_4x4_0 | mask_4x4_1) & 1) {
        if ((mask_4x4_0 & mask_4x4_1) & 1) {
          aom_highbd_lpf_vertical_4_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                         lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                         lfi1->hev_thr, bd);
        } else if (mask_4x4_0 & 1) {
          aom_highbd_lpf_vertical_4(s, pitch, lfi0->mblim, lfi0->lim,
                                    lfi0->hev_thr, bd);
        } else {
          aom_highbd_lpf_vertical_4(s + 8 * pitch, pitch, lfi1->mblim,
                                    lfi1->lim, lfi1->hev_thr, bd);
        }
      }

      if ((mask_4x4_int_0 | mask_4x4_int_1) & 1) {
        if ((mask_4x4_int_0 & mask_4x4_int_1) & 1) {
          aom_highbd_lpf_vertical_4_dual(s + 4, pitch, lfi0->mblim, lfi0->lim,
                                         lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                         lfi1->hev_thr, bd);
        } else if (mask_4x4_int_0 & 1) {
          aom_highbd_lpf_vertical_4(s + 4, pitch, lfi0->mblim, lfi0->lim,
                                    lfi0->hev_thr, bd);
        } else {
          aom_highbd_lpf_vertical_4(s + 8 * pitch + 4, pitch, lfi1->mblim,
                                    lfi1->lim, lfi1->hev_thr, bd);
        }
      }
    }

    s += 8;
    lfl += 1;
    mask_16x16_0 >>= 1;
    mask_8x8_0 >>= 1;
    mask_4x4_0 >>= 1;
    mask_4x4_int_0 >>= 1;
    mask_16x16_1 >>= 1;
    mask_8x8_1 >>= 1;
    mask_4x4_1 >>= 1;
    mask_4x4_int_1 >>= 1;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void filter_selectively_horiz(
    uint8_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const loop_filter_info_n *lfi_n, const uint8_t *lfl
#if CONFIG_LPF_DIRECT
    ,
    uint8_t *const src, int mi_row, int mi_col, int idx_r, int col_step,
    int width, int height, int ss_x, int ss_y
#endif
    ) {
  unsigned int mask;
  int count;
#if CONFIG_LPF_DIRECT
  // scale for u, v plane
  width >>= ss_x;
  height >>= ss_y;
  int idx_c = 0;
#endif

  for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
       mask >>= count) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;

    count = 1;
    if (mask & 1) {
#if CONFIG_LPF_DIRECT
      int i;
      const int line_length = 16;
      const int pivot = 8;
      const int above_filt_len = mask_16x16 & 1 ? 8 : 4;
      const int bot_filt_len = mask_16x16 & 1 ? 8 : 4;
      uint8_t block[256];  // line_length * size_of(BLOCK_8X8) * two_blocks
      int orig_pos[256];
      int direct;

      assert(above_filt_len == bot_filt_len);
      (void)bot_filt_len;
      for (i = 0; i < 256; ++i) {
        block[i] = 0;
        orig_pos[i] = -1;
      }

      // actual position for current pixel
      const int row = (mi_row + idx_r) * MI_SIZE >> ss_y;
      const int col = (mi_col + idx_c) * MI_SIZE >> ss_x;

      // Next block's thresholds.
      const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + 1);

      if (mask_16x16 & 1) {
        if ((mask_16x16 & 3) == 3) {
          // Could use asymmetric length in the future
          direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                        height, pitch, 2, 1);

          pick_filter_block_horz(src, block, orig_pos, above_filt_len, row, col,
                                 width, height, pitch, pivot, line_length, 2,
                                 direct);

          aom_lpf_horizontal_edge_16(block + pivot * line_length, line_length,
                                     lfi->mblim, lfi->lim, lfi->hev_thr);
          count = 2;
        } else {
          direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                        height, pitch, 1, 1);

          pick_filter_block_horz(src, block, orig_pos, above_filt_len, row, col,
                                 width, height, pitch, pivot, line_length, 1,
                                 direct);

          aom_lpf_horizontal_edge_8(block + pivot * line_length, line_length,
                                    lfi->mblim, lfi->lim, lfi->hev_thr);
        }

        for (i = 0; i < 256; ++i)
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
      } else if (mask_8x8 & 1) {
        if ((mask_8x8 & 3) == 3) {
          count = 2;
          direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                        height, pitch, 2, 1);

          pick_filter_block_horz(src, block, orig_pos, above_filt_len, row, col,
                                 width, height, pitch, pivot, line_length, 2,
                                 direct);

          aom_lpf_horizontal_8_dual(block + pivot * line_length, line_length,
                                    lfi->mblim, lfi->lim, lfi->hev_thr,
                                    lfin->mblim, lfin->lim, lfin->hev_thr);

          for (i = 0; i < 256; ++i)
            if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];

          if ((mask_4x4_int & 3) == 3) {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }

            direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                          pitch, 2, 1);

            pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col, width,
                                   height, pitch, pivot, line_length, 2,
                                   direct);

            aom_lpf_horizontal_4_dual(block + pivot * line_length, line_length,
                                      lfi->mblim, lfi->lim, lfi->hev_thr,
                                      lfin->mblim, lfin->lim, lfin->hev_thr);

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          } else {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }

            if (mask_4x4_int & 1) {
              direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                            pitch, 1, 1);

              pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col,
                                     width, height, pitch, pivot, line_length,
                                     1, direct);

              aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                   lfi->mblim, lfi->lim, lfi->hev_thr);
            } else if (mask_4x4_int & 2) {
              direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                            pitch, 1, 1);

              pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col + 8,
                                     width, height, pitch, pivot, line_length,
                                     1, direct);

              aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                   lfin->mblim, lfin->lim, lfin->hev_thr);
            }

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          }
        } else {
          direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                        height, pitch, 1, 1);

          pick_filter_block_horz(src, block, orig_pos, above_filt_len, row, col,
                                 width, height, pitch, pivot, line_length, 1,
                                 direct);

          aom_lpf_horizontal_8(block + pivot * line_length, line_length,
                               lfi->mblim, lfi->lim, lfi->hev_thr);

          for (i = 0; i < 256; ++i)
            if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];

          if (mask_4x4_int & 1) {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }
            direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                          pitch, 1, 1);

            pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col, width,
                                   height, pitch, pivot, line_length, 1,
                                   direct);

            aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                 lfi->mblim, lfi->lim, lfi->hev_thr);

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          }
        }
      } else if (mask_4x4 & 1) {
        if ((mask_4x4 & 3) == 3) {
          count = 2;
          direct = pick_min_grad_direct(src, 4, row, col, width, height, pitch,
                                        2, 1);

          pick_filter_block_horz(src, block, orig_pos, 4, row, col, width,
                                 height, pitch, pivot, line_length, 2, direct);

          aom_lpf_horizontal_4_dual(block + pivot * line_length, line_length,
                                    lfi->mblim, lfi->lim, lfi->hev_thr,
                                    lfin->mblim, lfin->lim, lfin->hev_thr);

          for (i = 0; i < 256; ++i)
            if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];

          if ((mask_4x4_int & 3) == 3) {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }

            direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                          pitch, 2, 1);

            pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col, width,
                                   height, pitch, pivot, line_length, 2,
                                   direct);

            aom_lpf_horizontal_4_dual(block + pivot * line_length, line_length,
                                      lfi->mblim, lfi->lim, lfi->hev_thr,
                                      lfin->mblim, lfin->lim, lfin->hev_thr);

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          } else {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }

            if (mask_4x4_int & 1) {
              direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                            pitch, 1, 1);

              pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col,
                                     width, height, pitch, pivot, line_length,
                                     1, direct);

              aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                   lfi->mblim, lfi->lim, lfi->hev_thr);
            } else if (mask_4x4_int & 2) {
              direct = pick_min_grad_direct(src, 4, row, col, width, height,
                                            pitch, 1, 1);

              pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col + 8,
                                     width, height, pitch, pivot, line_length,
                                     1, direct);

              aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                   lfin->mblim, lfin->lim, lfin->hev_thr);
            }

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          }
        } else {
          direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                        height, pitch, 1, 1);

          pick_filter_block_horz(src, block, orig_pos, above_filt_len, row, col,
                                 width, height, pitch, pivot, line_length, 1,
                                 direct);

          aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                               lfi->mblim, lfi->lim, lfi->hev_thr);

          for (i = 0; i < 256; ++i)
            if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];

          if (mask_4x4_int & 1) {
            for (i = 0; i < 256; ++i) {
              block[i] = 0;
              orig_pos[i] = -1;
            }
            direct = pick_min_grad_direct(src, above_filt_len, row, col, width,
                                          height, pitch, 1, 1);

            pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col, width,
                                   height, pitch, pivot, line_length, 1,
                                   direct);

            aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                                 lfi->mblim, lfi->lim, lfi->hev_thr);

            for (i = 0; i < 256; ++i)
              if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
          }
        }
      } else if (mask_4x4_int & 1) {
        direct =
            pick_min_grad_direct(src, 4, row, col, width, height, pitch, 1, 1);

        pick_filter_block_horz(src, block, orig_pos, 4, row + 4, col, width,
                               height, pitch, pivot, line_length, 1, direct);

        aom_lpf_horizontal_4(block + pivot * line_length, line_length,
                             lfi->mblim, lfi->lim, lfi->hev_thr);

        for (i = 0; i < 256; ++i)
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
      }
#else   // CONFIG_LPF_DIRECT
      if (mask_16x16 & 1) {
        if ((mask_16x16 & 3) == 3) {
          aom_lpf_horizontal_edge_16(s, pitch, lfi->mblim, lfi->lim,
                                     lfi->hev_thr);
          count = 2;
        } else {
          aom_lpf_horizontal_edge_8(s, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr);
        }
      } else if (mask_8x8 & 1) {
        if ((mask_8x8 & 3) == 3) {
          // Next block's thresholds.
          const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + 1);

          aom_lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr, lfin->mblim, lfin->lim,
                                    lfin->hev_thr);

          if ((mask_4x4_int & 3) == 3) {
            aom_lpf_horizontal_4_dual(s + 4 * pitch, pitch, lfi->mblim,
                                      lfi->lim, lfi->hev_thr, lfin->mblim,
                                      lfin->lim, lfin->hev_thr);
          } else {
            if (mask_4x4_int & 1)
              aom_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                   lfi->hev_thr);
            else if (mask_4x4_int & 2)
              aom_lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                   lfin->lim, lfin->hev_thr);
          }
          count = 2;
        } else {
          aom_lpf_horizontal_8(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);

          if (mask_4x4_int & 1)
            aom_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                 lfi->hev_thr);
        }
      } else if (mask_4x4 & 1) {
        if ((mask_4x4 & 3) == 3) {
          // Next block's thresholds.
          const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + 1);

          aom_lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr, lfin->mblim, lfin->lim,
                                    lfin->hev_thr);

          if ((mask_4x4_int & 3) == 3) {
            aom_lpf_horizontal_4_dual(s + 4 * pitch, pitch, lfi->mblim,
                                      lfi->lim, lfi->hev_thr, lfin->mblim,
                                      lfin->lim, lfin->hev_thr);
          } else {
            if (mask_4x4_int & 1)
              aom_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                   lfi->hev_thr);
            else if (mask_4x4_int & 2)
              aom_lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                   lfin->lim, lfin->hev_thr);
          }
          count = 2;
        } else {
          aom_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);

          if (mask_4x4_int & 1)
            aom_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                 lfi->hev_thr);
        }
      } else if (mask_4x4_int & 1) {
        aom_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                             lfi->hev_thr);
      }
#endif  // CONFIG_LPF_DIRECT
    }
#if CONFIG_LPF_DIRECT
    idx_c += col_step * count;
#endif
    s += 8 * count;
    lfl += count;
    mask_16x16 >>= count;
    mask_8x8 >>= count;
    mask_4x4 >>= count;
    mask_4x4_int >>= count;
  }
}

#if CONFIG_HIGHBITDEPTH
static void highbd_filter_selectively_horiz(
    uint16_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const loop_filter_info_n *lfi_n, const uint8_t *lfl, int bd) {
  unsigned int mask;
  int count;

  for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
       mask >>= count) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;

    count = 1;
    if (mask & 1) {
      if (mask_16x16 & 1) {
        if ((mask_16x16 & 3) == 3) {
          aom_highbd_lpf_horizontal_edge_16(s, pitch, lfi->mblim, lfi->lim,
                                            lfi->hev_thr, bd);
          count = 2;
        } else {
          aom_highbd_lpf_horizontal_edge_8(s, pitch, lfi->mblim, lfi->lim,
                                           lfi->hev_thr, bd);
        }
      } else if (mask_8x8 & 1) {
        if ((mask_8x8 & 3) == 3) {
          // Next block's thresholds.
          const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + 1);

          aom_highbd_lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                                           lfi->hev_thr, lfin->mblim, lfin->lim,
                                           lfin->hev_thr, bd);

          if ((mask_4x4_int & 3) == 3) {
            aom_highbd_lpf_horizontal_4_dual(
                s + 4 * pitch, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                lfin->mblim, lfin->lim, lfin->hev_thr, bd);
          } else {
            if (mask_4x4_int & 1) {
              aom_highbd_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                          lfi->lim, lfi->hev_thr, bd);
            } else if (mask_4x4_int & 2) {
              aom_highbd_lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                          lfin->lim, lfin->hev_thr, bd);
            }
          }
          count = 2;
        } else {
          aom_highbd_lpf_horizontal_8(s, pitch, lfi->mblim, lfi->lim,
                                      lfi->hev_thr, bd);

          if (mask_4x4_int & 1) {
            aom_highbd_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                        lfi->lim, lfi->hev_thr, bd);
          }
        }
      } else if (mask_4x4 & 1) {
        if ((mask_4x4 & 3) == 3) {
          // Next block's thresholds.
          const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + 1);

          aom_highbd_lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                                           lfi->hev_thr, lfin->mblim, lfin->lim,
                                           lfin->hev_thr, bd);
          if ((mask_4x4_int & 3) == 3) {
            aom_highbd_lpf_horizontal_4_dual(
                s + 4 * pitch, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                lfin->mblim, lfin->lim, lfin->hev_thr, bd);
          } else {
            if (mask_4x4_int & 1) {
              aom_highbd_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                          lfi->lim, lfi->hev_thr, bd);
            } else if (mask_4x4_int & 2) {
              aom_highbd_lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                          lfin->lim, lfin->hev_thr, bd);
            }
          }
          count = 2;
        } else {
          aom_highbd_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim,
                                      lfi->hev_thr, bd);

          if (mask_4x4_int & 1) {
            aom_highbd_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                        lfi->lim, lfi->hev_thr, bd);
          }
        }
      } else if (mask_4x4_int & 1) {
        aom_highbd_lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr, bd);
      }
    }
    s += 8 * count;
    lfl += count;
    mask_16x16 >>= count;
    mask_8x8 >>= count;
    mask_4x4 >>= count;
    mask_4x4_int >>= count;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

// This function ors into the current lfm structure, where to do loop
// filters for the specific mi we are looking at. It uses information
// including the block_size_type (32x16, 32x32, etc.), the transform size,
// whether there were any coefficients encoded, and the loop filter strength
// block we are currently looking at. Shift is used to position the
// 1's we produce.
// TODO(JBB) Need another function for different resolution color..
static void build_masks(AV1_COMMON *const cm,
                        const loop_filter_info_n *const lfi_n,
                        const MODE_INFO *mi, const int shift_y,
                        const int shift_uv, LOOP_FILTER_MASK *lfm) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
  const BLOCK_SIZE block_size = mbmi->sb_type;
  // TODO(debargha): Check if masks can be setup correctly when
  // rectangular transfroms are used with the EXT_TX expt.
  const TX_SIZE tx_size_y = txsize_sqr_map[mbmi->tx_size];
  const TX_SIZE tx_size_y_left = txsize_horz_map[mbmi->tx_size];
  const TX_SIZE tx_size_y_above = txsize_vert_map[mbmi->tx_size];
  const TX_SIZE tx_size_uv =
      txsize_sqr_map[uv_txsize_lookup[block_size][mbmi->tx_size][1][1]];
  const TX_SIZE tx_size_uv_left =
      txsize_horz_map[uv_txsize_lookup[block_size][mbmi->tx_size][1][1]];
  const TX_SIZE tx_size_uv_above =
      txsize_vert_map[uv_txsize_lookup[block_size][mbmi->tx_size][1][1]];
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  const int filter_level = get_filter_level(cm, lfi_n, 0, 0, mbmi);
#else
#if CONFIG_LPF_SB
  const int filter_level = get_filter_level(cm, lfi_n, 0, 0, mbmi);
#else
  const int filter_level = get_filter_level(cm, lfi_n, mbmi);
#endif  // CONFIG_LPF_SB
#endif
#else
  const int filter_level = get_filter_level(lfi_n, mbmi);
  (void)cm;
#endif
  uint64_t *const left_y = &lfm->left_y[tx_size_y_left];
  uint64_t *const above_y = &lfm->above_y[tx_size_y_above];
  uint64_t *const int_4x4_y = &lfm->int_4x4_y;
  uint16_t *const left_uv = &lfm->left_uv[tx_size_uv_left];
  uint16_t *const above_uv = &lfm->above_uv[tx_size_uv_above];
  uint16_t *const int_4x4_uv = &lfm->left_int_4x4_uv;
  int i;

  // If filter level is 0 we don't loop filter.
  if (!filter_level) {
    return;
  } else {
    const int w = num_8x8_blocks_wide_lookup[block_size];
    const int h = num_8x8_blocks_high_lookup[block_size];
    const int row = (shift_y >> MAX_MIB_SIZE_LOG2);
    const int col = shift_y - (row << MAX_MIB_SIZE_LOG2);

    for (i = 0; i < h; i++) memset(&lfm->lfl_y[row + i][col], filter_level, w);
  }

  // These set 1 in the current block size for the block size edges.
  // For instance if the block size is 32x16, we'll set:
  //    above =   1111
  //              0000
  //    and
  //    left  =   1000
  //          =   1000
  // NOTE : In this example the low bit is left most ( 1000 ) is stored as
  //        1,  not 8...
  //
  // U and V set things on a 16 bit scale.
  //
  *above_y |= above_prediction_mask[block_size] << shift_y;
  *above_uv |= above_prediction_mask_uv[block_size] << shift_uv;
  *left_y |= left_prediction_mask[block_size] << shift_y;
  *left_uv |= left_prediction_mask_uv[block_size] << shift_uv;

  // If the block has no coefficients and is not intra we skip applying
  // the loop filter on block edges.
  if (mbmi->skip && is_inter_block(mbmi)) return;

  // Here we are adding a mask for the transform size. The transform
  // size mask is set to be correct for a 64x64 prediction block size. We
  // mask to match the size of the block we are working on and then shift it
  // into place..
  *above_y |= (size_mask[block_size] & above_64x64_txform_mask[tx_size_y_above])
              << shift_y;
  *above_uv |=
      (size_mask_uv[block_size] & above_64x64_txform_mask_uv[tx_size_uv_above])
      << shift_uv;

  *left_y |= (size_mask[block_size] & left_64x64_txform_mask[tx_size_y_left])
             << shift_y;
  *left_uv |=
      (size_mask_uv[block_size] & left_64x64_txform_mask_uv[tx_size_uv_left])
      << shift_uv;

  // Here we are trying to determine what to do with the internal 4x4 block
  // boundaries.  These differ from the 4x4 boundaries on the outside edge of
  // an 8x8 in that the internal ones can be skipped and don't depend on
  // the prediction block size.
  if (tx_size_y == TX_4X4)
    *int_4x4_y |= (size_mask[block_size] & 0xffffffffffffffffULL) << shift_y;

  if (tx_size_uv == TX_4X4)
    *int_4x4_uv |= (size_mask_uv[block_size] & 0xffff) << shift_uv;
}

// This function does the same thing as the one above with the exception that
// it only affects the y masks. It exists because for blocks < 16x16 in size,
// we only update u and v masks on the first block.
static void build_y_mask(AV1_COMMON *const cm,
                         const loop_filter_info_n *const lfi_n,
                         const MODE_INFO *mi, const int shift_y,
#if CONFIG_SUPERTX
                         int supertx_enabled,
#endif  // CONFIG_SUPERTX
                         LOOP_FILTER_MASK *lfm) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
  const TX_SIZE tx_size_y = txsize_sqr_map[mbmi->tx_size];
  const TX_SIZE tx_size_y_left = txsize_horz_map[mbmi->tx_size];
  const TX_SIZE tx_size_y_above = txsize_vert_map[mbmi->tx_size];
#if CONFIG_SUPERTX
  const BLOCK_SIZE block_size =
      supertx_enabled ? (BLOCK_SIZE)(3 * tx_size_y) : mbmi->sb_type;
#else
  const BLOCK_SIZE block_size = mbmi->sb_type;
#endif
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  const int filter_level = get_filter_level(cm, lfi_n, 0, 0, mbmi);
#else
#if CONFIG_LPF_SB
  const int filter_level = get_filter_level(cm, lfi_n, 0, 0, mbmi);
#else
  const int filter_level = get_filter_level(cm, lfi_n, mbmi);
#endif  // CONFIG_LPF_SB
#endif
#else
  const int filter_level = get_filter_level(lfi_n, mbmi);
  (void)cm;
#endif
  uint64_t *const left_y = &lfm->left_y[tx_size_y_left];
  uint64_t *const above_y = &lfm->above_y[tx_size_y_above];
  uint64_t *const int_4x4_y = &lfm->int_4x4_y;
  int i;

  if (!filter_level) {
    return;
  } else {
    const int w = num_8x8_blocks_wide_lookup[block_size];
    const int h = num_8x8_blocks_high_lookup[block_size];
    const int row = (shift_y >> MAX_MIB_SIZE_LOG2);
    const int col = shift_y - (row << MAX_MIB_SIZE_LOG2);

    for (i = 0; i < h; i++) memset(&lfm->lfl_y[row + i][col], filter_level, w);
  }

  *above_y |= above_prediction_mask[block_size] << shift_y;
  *left_y |= left_prediction_mask[block_size] << shift_y;

  if (mbmi->skip && is_inter_block(mbmi)) return;

  *above_y |= (size_mask[block_size] & above_64x64_txform_mask[tx_size_y_above])
              << shift_y;

  *left_y |= (size_mask[block_size] & left_64x64_txform_mask[tx_size_y_left])
             << shift_y;

  if (tx_size_y == TX_4X4)
    *int_4x4_y |= (size_mask[block_size] & 0xffffffffffffffffULL) << shift_y;
}

#if CONFIG_LOOPFILTERING_ACROSS_TILES
// This function update the bit masks for the entire 64x64 region represented
// by mi_row, mi_col. In case one of the edge is a tile boundary, loop filtering
// for that edge is disabled. This function only check the tile boundary info
// for the top left corner mi to determine the boundary information for the
// top and left edge of the whole super block
static void update_tile_boundary_filter_mask(AV1_COMMON *const cm,
                                             const int mi_row, const int mi_col,
                                             LOOP_FILTER_MASK *lfm) {
  int i;
  MODE_INFO *const mi = cm->mi + mi_row * cm->mi_stride + mi_col;

  if (mi->mbmi.boundary_info & TILE_LEFT_BOUNDARY) {
    for (i = 0; i <= TX_32X32; i++) {
      lfm->left_y[i] &= 0xfefefefefefefefeULL;
      lfm->left_uv[i] &= 0xeeee;
    }
  }

  if (mi->mbmi.boundary_info & TILE_ABOVE_BOUNDARY) {
    for (i = 0; i <= TX_32X32; i++) {
      lfm->above_y[i] &= 0xffffffffffffff00ULL;
      lfm->above_uv[i] &= 0xfff0;
    }
  }
}
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

// This function sets up the bit masks for the entire 64x64 region represented
// by mi_row, mi_col.
// TODO(JBB): This function only works for yv12.
void av1_setup_mask(AV1_COMMON *const cm, const int mi_row, const int mi_col,
                    MODE_INFO **mi, const int mode_info_stride,
                    LOOP_FILTER_MASK *lfm) {
#if CONFIG_EXT_PARTITION
  assert(0 && "Not yet updated");
#endif  // CONFIG_EXT_PARTITION
  int idx_32, idx_16, idx_8;
  const loop_filter_info_n *const lfi_n = &cm->lf_info;
  MODE_INFO **mip = mi;
  MODE_INFO **mip2 = mi;

  // These are offsets to the next mi in the 64x64 block. It is what gets
  // added to the mi ptr as we go through each loop. It helps us to avoid
  // setting up special row and column counters for each index. The last step
  // brings us out back to the starting position.
  const int offset_32[] = { 4, (mode_info_stride << 2) - 4, 4,
                            -(mode_info_stride << 2) - 4 };
  const int offset_16[] = { 2, (mode_info_stride << 1) - 2, 2,
                            -(mode_info_stride << 1) - 2 };
  const int offset[] = { 1, mode_info_stride - 1, 1, -mode_info_stride - 1 };

  // Following variables represent shifts to position the current block
  // mask over the appropriate block. A shift of 36 to the left will move
  // the bits for the final 32 by 32 block in the 64x64 up 4 rows and left
  // 4 rows to the appropriate spot.
  const int shift_32_y[] = { 0, 4, 32, 36 };
  const int shift_16_y[] = { 0, 2, 16, 18 };
  const int shift_8_y[] = { 0, 1, 8, 9 };
  const int shift_32_uv[] = { 0, 2, 8, 10 };
  const int shift_16_uv[] = { 0, 1, 4, 5 };
  int i;
  const int max_rows = AOMMIN(cm->mi_rows - mi_row, MAX_MIB_SIZE);
  const int max_cols = AOMMIN(cm->mi_cols - mi_col, MAX_MIB_SIZE);

  av1_zero(*lfm);
  assert(mip[0] != NULL);

  // TODO(jimbankoski): Try moving most of the following code into decode
  // loop and storing lfm in the mbmi structure so that we don't have to go
  // through the recursive loop structure multiple times.
  switch (mip[0]->mbmi.sb_type) {
    case BLOCK_64X64: build_masks(cm, lfi_n, mip[0], 0, 0, lfm); break;
    case BLOCK_64X32: build_masks(cm, lfi_n, mip[0], 0, 0, lfm);
#if CONFIG_SUPERTX && CONFIG_TX64X64
      if (supertx_enabled(&mip[0]->mbmi)) break;
#endif  // CONFIG_SUPERTX && CONFIG_TX64X64
      mip2 = mip + mode_info_stride * 4;
      if (4 >= max_rows) break;
      build_masks(cm, lfi_n, mip2[0], 32, 8, lfm);
      break;
    case BLOCK_32X64: build_masks(cm, lfi_n, mip[0], 0, 0, lfm);
#if CONFIG_SUPERTX && CONFIG_TX64X64
      if (supertx_enabled(&mip[0]->mbmi)) break;
#endif  // CONFIG_SUPERTX && CONFIG_TX64X64
      mip2 = mip + 4;
      if (4 >= max_cols) break;
      build_masks(cm, lfi_n, mip2[0], 4, 2, lfm);
      break;
    default:
#if CONFIG_SUPERTX && CONFIG_TX64X64
      if (mip[0]->mbmi.tx_size == TX_64X64) {
        build_masks(cm, lfi_n, mip[0], 0, 0, lfm);
      } else {
#endif  // CONFIG_SUPERTX && CONFIG_TX64X64
        for (idx_32 = 0; idx_32 < 4; mip += offset_32[idx_32], ++idx_32) {
          const int shift_y_32 = shift_32_y[idx_32];
          const int shift_uv_32 = shift_32_uv[idx_32];
          const int mi_32_col_offset = ((idx_32 & 1) << 2);
          const int mi_32_row_offset = ((idx_32 >> 1) << 2);
          if (mi_32_col_offset >= max_cols || mi_32_row_offset >= max_rows)
            continue;
          switch (mip[0]->mbmi.sb_type) {
            case BLOCK_32X32:
              build_masks(cm, lfi_n, mip[0], shift_y_32, shift_uv_32, lfm);
              break;
            case BLOCK_32X16:
              build_masks(cm, lfi_n, mip[0], shift_y_32, shift_uv_32, lfm);
#if CONFIG_SUPERTX
              if (supertx_enabled(&mip[0]->mbmi)) break;
#endif
              if (mi_32_row_offset + 2 >= max_rows) continue;
              mip2 = mip + mode_info_stride * 2;
              build_masks(cm, lfi_n, mip2[0], shift_y_32 + 16, shift_uv_32 + 4,
                          lfm);
              break;
            case BLOCK_16X32:
              build_masks(cm, lfi_n, mip[0], shift_y_32, shift_uv_32, lfm);
#if CONFIG_SUPERTX
              if (supertx_enabled(&mip[0]->mbmi)) break;
#endif
              if (mi_32_col_offset + 2 >= max_cols) continue;
              mip2 = mip + 2;
              build_masks(cm, lfi_n, mip2[0], shift_y_32 + 2, shift_uv_32 + 1,
                          lfm);
              break;
            default:
#if CONFIG_SUPERTX
              if (mip[0]->mbmi.tx_size == TX_32X32) {
                build_masks(cm, lfi_n, mip[0], shift_y_32, shift_uv_32, lfm);
                break;
              }
#endif
              for (idx_16 = 0; idx_16 < 4; mip += offset_16[idx_16], ++idx_16) {
                const int shift_y_32_16 = shift_y_32 + shift_16_y[idx_16];
                const int shift_uv_32_16 = shift_uv_32 + shift_16_uv[idx_16];
                const int mi_16_col_offset =
                    mi_32_col_offset + ((idx_16 & 1) << 1);
                const int mi_16_row_offset =
                    mi_32_row_offset + ((idx_16 >> 1) << 1);

                if (mi_16_col_offset >= max_cols ||
                    mi_16_row_offset >= max_rows)
                  continue;

                switch (mip[0]->mbmi.sb_type) {
                  case BLOCK_16X16:
                    build_masks(cm, lfi_n, mip[0], shift_y_32_16,
                                shift_uv_32_16, lfm);
                    break;
                  case BLOCK_16X8:
#if CONFIG_SUPERTX
                    if (supertx_enabled(&mip[0]->mbmi)) break;
#endif
                    build_masks(cm, lfi_n, mip[0], shift_y_32_16,
                                shift_uv_32_16, lfm);
                    if (mi_16_row_offset + 1 >= max_rows) continue;
                    mip2 = mip + mode_info_stride;
                    build_y_mask(cm, lfi_n, mip2[0], shift_y_32_16 + 8,
#if CONFIG_SUPERTX
                                 0,
#endif
                                 lfm);
                    break;
                  case BLOCK_8X16:
#if CONFIG_SUPERTX
                    if (supertx_enabled(&mip[0]->mbmi)) break;
#endif
                    build_masks(cm, lfi_n, mip[0], shift_y_32_16,
                                shift_uv_32_16, lfm);
                    if (mi_16_col_offset + 1 >= max_cols) continue;
                    mip2 = mip + 1;
                    build_y_mask(cm, lfi_n, mip2[0], shift_y_32_16 + 1,
#if CONFIG_SUPERTX
                                 0,
#endif
                                 lfm);
                    break;
                  default: {
                    const int shift_y_32_16_8_zero =
                        shift_y_32_16 + shift_8_y[0];
#if CONFIG_SUPERTX
                    if (mip[0]->mbmi.tx_size == TX_16X16) {
                      build_masks(cm, lfi_n, mip[0], shift_y_32_16_8_zero,
                                  shift_uv_32_16, lfm);
                      break;
                    }
#endif
                    build_masks(cm, lfi_n, mip[0], shift_y_32_16_8_zero,
                                shift_uv_32_16, lfm);
                    mip += offset[0];
                    for (idx_8 = 1; idx_8 < 4; mip += offset[idx_8], ++idx_8) {
                      const int shift_y_32_16_8 =
                          shift_y_32_16 + shift_8_y[idx_8];
                      const int mi_8_col_offset =
                          mi_16_col_offset + ((idx_8 & 1));
                      const int mi_8_row_offset =
                          mi_16_row_offset + ((idx_8 >> 1));

                      if (mi_8_col_offset >= max_cols ||
                          mi_8_row_offset >= max_rows)
                        continue;
                      build_y_mask(cm, lfi_n, mip[0], shift_y_32_16_8,
#if CONFIG_SUPERTX
                                   supertx_enabled(&mip[0]->mbmi),
#endif
                                   lfm);
                    }
                    break;
                  }
                }
              }
              break;
          }
        }
#if CONFIG_SUPERTX && CONFIG_TX64X64
      }
#endif  // CONFIG_SUPERTX && CONFIG_TX64X64
      break;
  }
  // The largest loopfilter we have is 16x16 so we use the 16x16 mask
  // for 32x32 transforms also.
  lfm->left_y[TX_16X16] |= lfm->left_y[TX_32X32];
  lfm->above_y[TX_16X16] |= lfm->above_y[TX_32X32];
  lfm->left_uv[TX_16X16] |= lfm->left_uv[TX_32X32];
  lfm->above_uv[TX_16X16] |= lfm->above_uv[TX_32X32];

  // We do at least 8 tap filter on every 32x32 even if the transform size
  // is 4x4. So if the 4x4 is set on a border pixel add it to the 8x8 and
  // remove it from the 4x4.
  lfm->left_y[TX_8X8] |= lfm->left_y[TX_4X4] & left_border;
  lfm->left_y[TX_4X4] &= ~left_border;
  lfm->above_y[TX_8X8] |= lfm->above_y[TX_4X4] & above_border;
  lfm->above_y[TX_4X4] &= ~above_border;
  lfm->left_uv[TX_8X8] |= lfm->left_uv[TX_4X4] & left_border_uv;
  lfm->left_uv[TX_4X4] &= ~left_border_uv;
  lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_4X4] & above_border_uv;
  lfm->above_uv[TX_4X4] &= ~above_border_uv;

  // We do some special edge handling.
  if (mi_row + MAX_MIB_SIZE > cm->mi_rows) {
    const uint64_t rows = cm->mi_rows - mi_row;

    // Each pixel inside the border gets a 1,
    const uint64_t mask_y = (((uint64_t)1 << (rows << MAX_MIB_SIZE_LOG2)) - 1);
    const uint16_t mask_uv =
        (((uint16_t)1 << (((rows + 1) >> 1) << (MAX_MIB_SIZE_LOG2 - 1))) - 1);

    // Remove values completely outside our border.
    for (i = 0; i < TX_32X32; i++) {
      lfm->left_y[i] &= mask_y;
      lfm->above_y[i] &= mask_y;
      lfm->left_uv[i] &= mask_uv;
      lfm->above_uv[i] &= mask_uv;
    }
    lfm->int_4x4_y &= mask_y;
    lfm->above_int_4x4_uv = lfm->left_int_4x4_uv & mask_uv;

    // We don't apply a wide loop filter on the last uv block row. If set
    // apply the shorter one instead.
    if (rows == 1) {
      lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_16X16];
      lfm->above_uv[TX_16X16] = 0;
    }
    if (rows == 5) {
      lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_16X16] & 0xff00;
      lfm->above_uv[TX_16X16] &= ~(lfm->above_uv[TX_16X16] & 0xff00);
    }
  } else {
    lfm->above_int_4x4_uv = lfm->left_int_4x4_uv;
  }

  if (mi_col + MAX_MIB_SIZE > cm->mi_cols) {
    const uint64_t columns = cm->mi_cols - mi_col;

    // Each pixel inside the border gets a 1, the multiply copies the border
    // to where we need it.
    const uint64_t mask_y = (((1 << columns) - 1)) * 0x0101010101010101ULL;
    const uint16_t mask_uv = ((1 << ((columns + 1) >> 1)) - 1) * 0x1111;

    // Internal edges are not applied on the last column of the image so
    // we mask 1 more for the internal edges
    const uint16_t mask_uv_int = ((1 << (columns >> 1)) - 1) * 0x1111;

    // Remove the bits outside the image edge.
    for (i = 0; i < TX_32X32; i++) {
      lfm->left_y[i] &= mask_y;
      lfm->above_y[i] &= mask_y;
      lfm->left_uv[i] &= mask_uv;
      lfm->above_uv[i] &= mask_uv;
    }
    lfm->int_4x4_y &= mask_y;
    lfm->left_int_4x4_uv &= mask_uv_int;

    // We don't apply a wide loop filter on the last uv column. If set
    // apply the shorter one instead.
    if (columns == 1) {
      lfm->left_uv[TX_8X8] |= lfm->left_uv[TX_16X16];
      lfm->left_uv[TX_16X16] = 0;
    }
    if (columns == 5) {
      lfm->left_uv[TX_8X8] |= (lfm->left_uv[TX_16X16] & 0xcccc);
      lfm->left_uv[TX_16X16] &= ~(lfm->left_uv[TX_16X16] & 0xcccc);
    }
  }
  // We don't apply a loop filter on the first column in the image, mask that
  // out.
  if (mi_col == 0) {
    for (i = 0; i < TX_32X32; i++) {
      lfm->left_y[i] &= 0xfefefefefefefefeULL;
      lfm->left_uv[i] &= 0xeeee;
    }
  }

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  if (av1_disable_loopfilter_on_tile_boundary(cm)) {
    update_tile_boundary_filter_mask(cm, mi_row, mi_col, lfm);
  }
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

  // Assert if we try to apply 2 different loop filters at the same position.
  assert(!(lfm->left_y[TX_16X16] & lfm->left_y[TX_8X8]));
  assert(!(lfm->left_y[TX_16X16] & lfm->left_y[TX_4X4]));
  assert(!(lfm->left_y[TX_8X8] & lfm->left_y[TX_4X4]));
  assert(!(lfm->int_4x4_y & lfm->left_y[TX_16X16]));
  assert(!(lfm->left_uv[TX_16X16] & lfm->left_uv[TX_8X8]));
  assert(!(lfm->left_uv[TX_16X16] & lfm->left_uv[TX_4X4]));
  assert(!(lfm->left_uv[TX_8X8] & lfm->left_uv[TX_4X4]));
  assert(!(lfm->left_int_4x4_uv & lfm->left_uv[TX_16X16]));
  assert(!(lfm->above_y[TX_16X16] & lfm->above_y[TX_8X8]));
  assert(!(lfm->above_y[TX_16X16] & lfm->above_y[TX_4X4]));
  assert(!(lfm->above_y[TX_8X8] & lfm->above_y[TX_4X4]));
  assert(!(lfm->int_4x4_y & lfm->above_y[TX_16X16]));
  assert(!(lfm->above_uv[TX_16X16] & lfm->above_uv[TX_8X8]));
  assert(!(lfm->above_uv[TX_16X16] & lfm->above_uv[TX_4X4]));
  assert(!(lfm->above_uv[TX_8X8] & lfm->above_uv[TX_4X4]));
  assert(!(lfm->above_int_4x4_uv & lfm->above_uv[TX_16X16]));
}

static void filter_selectively_vert(
    uint8_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const loop_filter_info_n *lfi_n, const uint8_t *lfl
#if CONFIG_LPF_DIRECT
    ,
    uint8_t *const src, int mi_row, int mi_col, int idx_r, int col_step,
    int width, int height, int ss_x, int ss_y
#endif
    ) {
  unsigned int mask;
#if CONFIG_LPF_DIRECT
  // scale for u, v plane
  width >>= ss_x;
  height >>= ss_y;
  int idx_c = 0;
#endif

  for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
       mask >>= 1) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;

#if CONFIG_LPF_DIRECT
    int i;
    const int pivot = 8;
    const int left_filt_len = mask_16x16 & 1 ? 8 : 4;
    const int right_filt_len = mask_16x16 & 1 ? 8 : 4;
    const int line_length = 16;
    uint8_t block[128];
    int orig_pos[128];

    // actual position for current pixel
    const int row = (mi_row + idx_r) * MI_SIZE >> ss_y;
    const int col = (mi_col + idx_c) * MI_SIZE >> ss_x;

    // Could use asymmetric length in the future
    assert(left_filt_len == right_filt_len);
    (void)right_filt_len;

    if ((mask_16x16 & 1) || (mask_8x8 & 1) || (mask_4x4 & 1)) {
      for (i = 0; i < 128; ++i) {
        block[i] = 0;
        orig_pos[i] = -1;
      }

      const int direct = pick_min_grad_direct(src, left_filt_len, row, col,
                                              width, height, pitch, 1, 0);

      pick_filter_block_vert(src, block, orig_pos, left_filt_len, row, col,
                             width, height, pitch, pivot, line_length, 1,
                             direct);

      // apply filtering
      if (mask_16x16 & 1) {
        aom_lpf_vertical_16(block + pivot, line_length, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
      } else if (mask_8x8 & 1) {
        aom_lpf_vertical_8(block + pivot, line_length, lfi->mblim, lfi->lim,
                           lfi->hev_thr);
      } else if (mask_4x4 & 1) {
        aom_lpf_vertical_4(block + pivot, line_length, lfi->mblim, lfi->lim,
                           lfi->hev_thr);
      }

      for (i = 0; i < 128; ++i)
        if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
    }

    // filter inner 4x4
    if (mask_4x4_int & 1) {
      for (i = 0; i < 128; ++i) {
        block[i] = 0;
        orig_pos[i] = -1;
      }

      const int direct = pick_min_grad_direct(src, 4, row, col + 4, width,
                                              height, pitch, 1, 0);

      pick_filter_block_vert(src, block, orig_pos, 4, row, col + 4, width,
                             height, pitch, pivot, line_length, 1, direct);

      aom_lpf_vertical_4(block + pivot, line_length, lfi->mblim, lfi->lim,
                         lfi->hev_thr);

      for (i = 0; i < 128; ++i)
        if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
    }
#else
    if (mask & 1) {
      if (mask_16x16 & 1) {
        aom_lpf_vertical_16(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
      } else if (mask_8x8 & 1) {
        aom_lpf_vertical_8(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
      } else if (mask_4x4 & 1) {
        aom_lpf_vertical_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
      }
    }
    if (mask_4x4_int & 1)
      aom_lpf_vertical_4(s + 4, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
#endif  // CONFIG_LPF_DIRECT
#if CONFIG_LPF_DIRECT
    idx_c += col_step;
#endif
    s += 8;
    lfl += 1;
    mask_16x16 >>= 1;
    mask_8x8 >>= 1;
    mask_4x4 >>= 1;
    mask_4x4_int >>= 1;
  }
}

#if CONFIG_HIGHBITDEPTH
static void highbd_filter_selectively_vert(
    uint16_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const loop_filter_info_n *lfi_n, const uint8_t *lfl, int bd) {
  unsigned int mask;

  for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
       mask >>= 1) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;

    if (mask & 1) {
      if (mask_16x16 & 1) {
        aom_highbd_lpf_vertical_16(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                   bd);
      } else if (mask_8x8 & 1) {
        aom_highbd_lpf_vertical_8(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                  bd);
      } else if (mask_4x4 & 1) {
        aom_highbd_lpf_vertical_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                  bd);
      }
    }
    if (mask_4x4_int & 1)
      aom_highbd_lpf_vertical_4(s + 4, pitch, lfi->mblim, lfi->lim,
                                lfi->hev_thr, bd);
    s += 8;
    lfl += 1;
    mask_16x16 >>= 1;
    mask_8x8 >>= 1;
    mask_4x4 >>= 1;
    mask_4x4_int >>= 1;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

typedef struct {
  unsigned int m16x16;
  unsigned int m8x8;
  unsigned int m4x4;
} FilterMasks;

// Get filter level and masks for the given row index 'idx_r'. (Only used for
// the non420 case).
// Note: 'row_masks_ptr' and/or 'col_masks_ptr' can be passed NULL.
static void get_filter_level_and_masks_non420(
    AV1_COMMON *const cm, const struct macroblockd_plane *const plane, int pl,
    MODE_INFO **mib, int mi_row, int mi_col, int idx_r, uint8_t *const lfl_r,
    unsigned int *const mask_4x4_int_r_ptr,
    unsigned int *const mask_4x4_int_c_ptr, FilterMasks *const row_masks_ptr,
    FilterMasks *const col_masks_ptr) {
  const int ss_x = plane->subsampling_x;
  const int ss_y = plane->subsampling_y;
  const int col_step = mi_size_wide[BLOCK_8X8] << ss_x;
  FilterMasks row_masks, col_masks;
  memset(&row_masks, 0, sizeof(row_masks));
  memset(&col_masks, 0, sizeof(col_masks));
  unsigned int mask_4x4_int_r = 0, mask_4x4_int_c = 0;
  const int r = idx_r >> mi_height_log2_lookup[BLOCK_8X8];

  // Determine the vertical edges that need filtering
  int idx_c;
  for (idx_c = 0; idx_c < cm->mib_size && mi_col + idx_c < cm->mi_cols;
       idx_c += col_step) {
    const MODE_INFO *mi = mib[idx_r * cm->mi_stride + idx_c];
    const MB_MODE_INFO *mbmi = &mi[0].mbmi;
    const BLOCK_SIZE sb_type = mbmi->sb_type;
    const int skip_this = mbmi->skip && is_inter_block(mbmi);
    // Map index to 8x8 unit
    const int c = idx_c >> mi_width_log2_lookup[BLOCK_8X8];

    const int blk_row = r & (num_8x8_blocks_high_lookup[sb_type] - 1);
    const int blk_col = c & (num_8x8_blocks_wide_lookup[sb_type] - 1);

    // left edge of current unit is block/partition edge -> no skip
    const int block_edge_left =
        (num_4x4_blocks_wide_lookup[sb_type] > 1) ? !blk_col : 1;
    const int skip_this_c = skip_this && !block_edge_left;
    // top edge of current unit is block/partition edge -> no skip
    const int block_edge_above =
        (num_4x4_blocks_high_lookup[sb_type] > 1) ? !blk_row : 1;
    const int skip_this_r = skip_this && !block_edge_above;

    TX_SIZE tx_size = (plane->plane_type == PLANE_TYPE_UV)
                          ? av1_get_uv_tx_size(mbmi, plane)
                          : mbmi->tx_size;

    const int skip_border_4x4_c =
        ss_x && mi_col + idx_c >= cm->mi_cols - mi_size_wide[BLOCK_8X8];
    const int skip_border_4x4_r =
        ss_y && mi_row + idx_r >= cm->mi_rows - mi_size_high[BLOCK_8X8];

    int tx_size_mask = 0;
    const int c_step = (c >> ss_x);
    const int r_step = (r >> ss_y);
    const int col_mask = 1 << c_step;

#if CONFIG_VAR_TX
    if (is_inter_block(mbmi) && !mbmi->skip) {
      const int tx_row_idx =
          (blk_row * mi_size_high[BLOCK_8X8] << TX_UNIT_HIGH_LOG2) >> 1;
      const int tx_col_idx =
          (blk_col * mi_size_wide[BLOCK_8X8] << TX_UNIT_WIDE_LOG2) >> 1;
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      const BLOCK_SIZE bsize =
          AOMMAX(BLOCK_4X4, get_plane_block_size(mbmi->sb_type, plane));
#else
      const BLOCK_SIZE bsize = get_plane_block_size(mbmi->sb_type, plane);
#endif
      const TX_SIZE mb_tx_size = mbmi->inter_tx_size[tx_row_idx][tx_col_idx];
      tx_size = (plane->plane_type == PLANE_TYPE_UV)
                    ? uv_txsize_lookup[bsize][mb_tx_size][0][0]
                    : mb_tx_size;
    }
#endif

// Filter level can vary per MI
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
    if (!(lfl_r[c_step] = get_filter_level(cm, &cm->lf_info, 0, 0, mbmi)))
      continue;
#else
#if CONFIG_LPF_SB
    if (!(lfl_r[c_step] =
              get_filter_level(cm, &cm->lf_info, mi_row, mi_col, mbmi)))
      continue;
#else
    if (!(lfl_r[c_step] = get_filter_level(cm, &cm->lf_info, mbmi))) continue;
#endif  // CONFIG_LPF_SB
#endif
#else
    if (!(lfl_r[c_step] = get_filter_level(&cm->lf_info, mbmi))) continue;
#endif

#if CONFIG_VAR_TX
    TX_SIZE tx_size_horz_edge, tx_size_vert_edge;

    // filt_len_vert_edge is the length of deblocking filter for a vertical edge
    // The filter direction of a vertical edge is horizontal.
    // Thus, filt_len_vert_edge is determined as the minimum width of the two
    // transform block sizes on the left and right (current block) side of edge
    const int filt_len_vert_edge = AOMMIN(
        tx_size_wide[tx_size],
        tx_size_wide[cm->left_txfm_context[pl][((mi_row + idx_r) & MAX_MIB_MASK)
                                               << TX_UNIT_HIGH_LOG2]]);

    // filt_len_horz_edge is the len of deblocking filter for a horizontal edge
    // The filter direction of a horizontal edge is vertical.
    // Thus, filt_len_horz_edge is determined as the minimum height of the two
    // transform block sizes on the top and bottom (current block) side of edge
    const int filt_len_horz_edge =
        AOMMIN(tx_size_high[tx_size],
               tx_size_high[cm->top_txfm_context[pl][(mi_col + idx_c)
                                                     << TX_UNIT_WIDE_LOG2]]);

    // transform width/height of current block
    const int tx_wide_cur = tx_size_wide[tx_size];
    const int tx_high_cur = tx_size_high[tx_size];

    // tx_size_vert_edge is square transform size for a vertical deblocking edge
    // It determines the type of filter applied to the vertical edge
    // Similarly, tx_size_horz_edge is for a horizontal deblocking edge
    tx_size_vert_edge = get_sqr_tx_size(filt_len_vert_edge);
    tx_size_horz_edge = get_sqr_tx_size(filt_len_horz_edge);

    memset(cm->top_txfm_context[pl] + ((mi_col + idx_c) << TX_UNIT_WIDE_LOG2),
           tx_size, mi_size_wide[BLOCK_8X8] << TX_UNIT_WIDE_LOG2);
    memset(cm->left_txfm_context[pl] +
               (((mi_row + idx_r) & MAX_MIB_MASK) << TX_UNIT_HIGH_LOG2),
           tx_size, mi_size_high[BLOCK_8X8] << TX_UNIT_HIGH_LOG2);
#else
    // The length (or equally the square tx size) of deblocking filter is only
    // determined by
    // a) current block's width for a vertical deblocking edge
    // b) current block's height for a horizontal deblocking edge
    TX_SIZE tx_size_vert_edge = txsize_horz_map[tx_size];
    TX_SIZE tx_size_horz_edge = txsize_vert_map[tx_size];
    (void)pl;
#endif  // CONFIG_VAR_TX

    if (tx_size_vert_edge == TX_32X32)
      tx_size_mask = 3;
    else if (tx_size_vert_edge == TX_16X16)
      tx_size_mask = 1;
    else
      tx_size_mask = 0;

    // Build masks based on the transform size of each block
    // handle vertical mask
    if (tx_size_vert_edge == TX_32X32) {
      if (!skip_this_c && (c_step & tx_size_mask) == 0) {
        if (!skip_border_4x4_c)
          col_masks.m16x16 |= col_mask;
        else
          col_masks.m8x8 |= col_mask;
      }
    } else if (tx_size_vert_edge == TX_16X16) {
      if (!skip_this_c && (c_step & tx_size_mask) == 0) {
        if (!skip_border_4x4_c)
          col_masks.m16x16 |= col_mask;
        else
          col_masks.m8x8 |= col_mask;
      }
    } else {
      // force 8x8 filtering on 32x32 boundaries
      if (!skip_this_c && (c_step & tx_size_mask) == 0) {
        if (tx_size_vert_edge == TX_8X8 || (c_step & 3) == 0)
          col_masks.m8x8 |= col_mask;
        else
          col_masks.m4x4 |= col_mask;
      }

#if CONFIG_VAR_TX
      if (!skip_this && tx_wide_cur < 8 && !skip_border_4x4_c &&
          (c_step & tx_size_mask) == 0)
#else
      if (!skip_this && tx_size_vert_edge < TX_8X8 && !skip_border_4x4_c &&
          (c_step & tx_size_mask) == 0)
#endif  // CONFIG_VAR_TX
        mask_4x4_int_c |= col_mask;
    }

    if (tx_size_horz_edge == TX_32X32)
      tx_size_mask = 3;
    else if (tx_size_horz_edge == TX_16X16)
      tx_size_mask = 1;
    else
      tx_size_mask = 0;

    // set horizontal mask
    if (tx_size_horz_edge == TX_32X32) {
      if (!skip_this_r && (r_step & tx_size_mask) == 0) {
        if (!skip_border_4x4_r)
          row_masks.m16x16 |= col_mask;
        else
          row_masks.m8x8 |= col_mask;
      }
    } else if (tx_size_horz_edge == TX_16X16) {
      if (!skip_this_r && (r_step & tx_size_mask) == 0) {
        if (!skip_border_4x4_r)
          row_masks.m16x16 |= col_mask;
        else
          row_masks.m8x8 |= col_mask;
      }
    } else {
      // force 8x8 filtering on 32x32 boundaries
      if (!skip_this_r && (r_step & tx_size_mask) == 0) {
        if (tx_size_horz_edge == TX_8X8 || (r_step & 3) == 0)
          row_masks.m8x8 |= col_mask;
        else
          row_masks.m4x4 |= col_mask;
      }

#if CONFIG_VAR_TX
      if (!skip_this && tx_high_cur < 8 && !skip_border_4x4_r &&
          (r_step & tx_size_mask) == 0)
#else
      if (!skip_this && tx_size_horz_edge < TX_8X8 && !skip_border_4x4_r &&
          (r_step & tx_size_mask) == 0)
#endif  // CONFIG_VAR_TX
        mask_4x4_int_r |= col_mask;
    }
  }

  if (row_masks_ptr) *row_masks_ptr = row_masks;
  if (col_masks_ptr) *col_masks_ptr = col_masks;
  if (mask_4x4_int_c_ptr) *mask_4x4_int_c_ptr = mask_4x4_int_c;
  if (mask_4x4_int_r_ptr) *mask_4x4_int_r_ptr = mask_4x4_int_r;
}

void av1_filter_block_plane_non420_ver(AV1_COMMON *const cm,
                                       struct macroblockd_plane *plane,
                                       MODE_INFO **mib, int mi_row, int mi_col,
                                       int pl) {
  const int ss_y = plane->subsampling_y;
  const int row_step = mi_size_high[BLOCK_8X8] << ss_y;
#if CONFIG_LPF_DIRECT
  const int ss_x = plane->subsampling_x;
  const int col_step = mi_size_wide[BLOCK_8X8] << ss_x;
#endif
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  uint8_t lfl[MAX_MIB_SIZE][MAX_MIB_SIZE] = { { 0 } };

  int idx_r;
  for (idx_r = 0; idx_r < cm->mib_size && mi_row + idx_r < cm->mi_rows;
       idx_r += row_step) {
    unsigned int mask_4x4_int;
    FilterMasks col_masks;
    const int r = idx_r >> mi_height_log2_lookup[BLOCK_8X8];
    get_filter_level_and_masks_non420(cm, plane, pl, mib, mi_row, mi_col, idx_r,
                                      &lfl[r][0], NULL, &mask_4x4_int, NULL,
                                      &col_masks);

    // Disable filtering on the leftmost column or tile boundary
    unsigned int border_mask = ~(mi_col == 0 ? 1 : 0);
#if CONFIG_LOOPFILTERING_ACROSS_TILES
    MODE_INFO *const mi = cm->mi + (mi_row + idx_r) * cm->mi_stride + mi_col;
    if (av1_disable_loopfilter_on_tile_boundary(cm) &&
        ((mi->mbmi.boundary_info & TILE_LEFT_BOUNDARY) != 0)) {
      border_mask = 0xfffffffe;
    }
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      highbd_filter_selectively_vert(
          CONVERT_TO_SHORTPTR(dst->buf), dst->stride,
          col_masks.m16x16 & border_mask, col_masks.m8x8 & border_mask,
          col_masks.m4x4 & border_mask, mask_4x4_int, &cm->lf_info, &lfl[r][0],
          (int)cm->bit_depth);
    else
#endif  // CONFIG_HIGHBITDEPTH
      filter_selectively_vert(
          dst->buf, dst->stride, col_masks.m16x16 & border_mask,
          col_masks.m8x8 & border_mask, col_masks.m4x4 & border_mask,
          mask_4x4_int, &cm->lf_info, &lfl[r][0]
#if CONFIG_LPF_DIRECT
          ,
          dst->buf0, mi_row, mi_col, idx_r, col_step, cm->width, cm->height,
          ss_x, ss_y
#endif  // CONFIG_LPF_DIRECT
          );
    dst->buf += 8 * dst->stride;
  }

  // Now do horizontal pass
  dst->buf = dst0;
}

void av1_filter_block_plane_non420_hor(AV1_COMMON *const cm,
                                       struct macroblockd_plane *plane,
                                       MODE_INFO **mib, int mi_row, int mi_col,
                                       int pl) {
  const int ss_y = plane->subsampling_y;
  const int row_step = mi_size_high[BLOCK_8X8] << ss_y;
#if CONFIG_LPF_DIRECT
  const int ss_x = plane->subsampling_x;
  const int col_step = mi_size_wide[BLOCK_8X8] << ss_x;
#endif
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  uint8_t lfl[MAX_MIB_SIZE][MAX_MIB_SIZE] = { { 0 } };

  int idx_r;
  for (idx_r = 0; idx_r < cm->mib_size && mi_row + idx_r < cm->mi_rows;
       idx_r += row_step) {
    unsigned int mask_4x4_int;
    FilterMasks row_masks;
    const int r = idx_r >> mi_height_log2_lookup[BLOCK_8X8];
    get_filter_level_and_masks_non420(cm, plane, pl, mib, mi_row, mi_col, idx_r,
                                      &lfl[r][0], &mask_4x4_int, NULL,
                                      &row_masks, NULL);

#if CONFIG_LOOPFILTERING_ACROSS_TILES
    // Disable filtering on the abovemost row or tile boundary
    const MODE_INFO *mi = cm->mi + (mi_row + idx_r) * cm->mi_stride + mi_col;
    if ((av1_disable_loopfilter_on_tile_boundary(cm) &&
         (mi->mbmi.boundary_info & TILE_ABOVE_BOUNDARY)) ||
        (mi_row + idx_r == 0))
      memset(&row_masks, 0, sizeof(row_masks));
#else
    if (mi_row + idx_r == 0) memset(&row_masks, 0, sizeof(row_masks));
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES

#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      highbd_filter_selectively_horiz(
          CONVERT_TO_SHORTPTR(dst->buf), dst->stride, row_masks.m16x16,
          row_masks.m8x8, row_masks.m4x4, mask_4x4_int, &cm->lf_info,
          &lfl[r][0], (int)cm->bit_depth);
    else
#endif  // CONFIG_HIGHBITDEPTH
      filter_selectively_horiz(dst->buf, dst->stride, row_masks.m16x16,
                               row_masks.m8x8, row_masks.m4x4, mask_4x4_int,
                               &cm->lf_info, &lfl[r][0]
#if CONFIG_LPF_DIRECT
                               ,
                               dst->buf0, mi_row, mi_col, idx_r, col_step,
                               cm->width, cm->height, ss_x, ss_y
#endif  // CONFIG_LPF_DIRECT
                               );
    dst->buf += 8 * dst->stride;
  }
  dst->buf = dst0;
}

void av1_filter_block_plane_ss00_ver(AV1_COMMON *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm) {
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  int r;
  uint64_t mask_16x16 = lfm->left_y[TX_16X16];
  uint64_t mask_8x8 = lfm->left_y[TX_8X8];
  uint64_t mask_4x4 = lfm->left_y[TX_4X4];
  uint64_t mask_4x4_int = lfm->int_4x4_y;

  assert(plane->subsampling_x == 0 && plane->subsampling_y == 0);

  // Vertical pass: do 2 rows at one time
  for (r = 0; r < cm->mib_size && mi_row + r < cm->mi_rows; r += 2) {
    unsigned int mask_16x16_l = mask_16x16 & 0xffff;
    unsigned int mask_8x8_l = mask_8x8 & 0xffff;
    unsigned int mask_4x4_l = mask_4x4 & 0xffff;
    unsigned int mask_4x4_int_l = mask_4x4_int & 0xffff;

// Disable filtering on the leftmost column.
#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      highbd_filter_selectively_vert_row2(
          plane->subsampling_x, CONVERT_TO_SHORTPTR(dst->buf), dst->stride,
          mask_16x16_l, mask_8x8_l, mask_4x4_l, mask_4x4_int_l, &cm->lf_info,
          &lfm->lfl_y[r][0], (int)cm->bit_depth);
    else
#endif  // CONFIG_HIGHBITDEPTH
      filter_selectively_vert_row2(
          plane->subsampling_x, dst->buf, dst->stride, mask_16x16_l, mask_8x8_l,
          mask_4x4_l, mask_4x4_int_l, &cm->lf_info, &lfm->lfl_y[r][0]);

    dst->buf += 2 * MI_SIZE * dst->stride;
    mask_16x16 >>= 2 * MI_SIZE;
    mask_8x8 >>= 2 * MI_SIZE;
    mask_4x4 >>= 2 * MI_SIZE;
    mask_4x4_int >>= 2 * MI_SIZE;
  }

  // Horizontal pass
  dst->buf = dst0;
}

void av1_filter_block_plane_ss00_hor(AV1_COMMON *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm) {
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  int r;
  uint64_t mask_16x16 = lfm->above_y[TX_16X16];
  uint64_t mask_8x8 = lfm->above_y[TX_8X8];
  uint64_t mask_4x4 = lfm->above_y[TX_4X4];
  uint64_t mask_4x4_int = lfm->int_4x4_y;

  assert(plane->subsampling_x == 0 && plane->subsampling_y == 0);

  for (r = 0; r < cm->mib_size && mi_row + r < cm->mi_rows; r++) {
    unsigned int mask_16x16_r;
    unsigned int mask_8x8_r;
    unsigned int mask_4x4_r;

    if (mi_row + r == 0) {
      mask_16x16_r = 0;
      mask_8x8_r = 0;
      mask_4x4_r = 0;
    } else {
      mask_16x16_r = mask_16x16 & 0xff;
      mask_8x8_r = mask_8x8 & 0xff;
      mask_4x4_r = mask_4x4 & 0xff;
    }

#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      highbd_filter_selectively_horiz(
          CONVERT_TO_SHORTPTR(dst->buf), dst->stride, mask_16x16_r, mask_8x8_r,
          mask_4x4_r, mask_4x4_int & 0xff, &cm->lf_info, &lfm->lfl_y[r][0],
          (int)cm->bit_depth);
    else
#endif  // CONFIG_HIGHBITDEPTH
#if !CONFIG_LPF_DIRECT
      filter_selectively_horiz(dst->buf, dst->stride, mask_16x16_r, mask_8x8_r,
                               mask_4x4_r, mask_4x4_int & 0xff, &cm->lf_info,
                               &lfm->lfl_y[r][0]);
#endif  // CONFIG_LPF_DIRECT

    dst->buf += MI_SIZE * dst->stride;
    mask_16x16 >>= MI_SIZE;
    mask_8x8 >>= MI_SIZE;
    mask_4x4 >>= MI_SIZE;
    mask_4x4_int >>= MI_SIZE;
  }
  // restore the buf pointer in case there is additional filter pass.
  dst->buf = dst0;
}

void av1_filter_block_plane_ss11_ver(AV1_COMMON *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm) {
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  int r, c;

  uint16_t mask_16x16 = lfm->left_uv[TX_16X16];
  uint16_t mask_8x8 = lfm->left_uv[TX_8X8];
  uint16_t mask_4x4 = lfm->left_uv[TX_4X4];
  uint16_t mask_4x4_int = lfm->left_int_4x4_uv;

  assert(plane->subsampling_x == 1 && plane->subsampling_y == 1);
  assert(plane->plane_type == PLANE_TYPE_UV);
  memset(lfm->lfl_uv, 0, sizeof(lfm->lfl_uv));

  // Vertical pass: do 2 rows at one time
  for (r = 0; r < cm->mib_size && mi_row + r < cm->mi_rows; r += 4) {
    for (c = 0; c < (cm->mib_size >> 1); c++) {
      lfm->lfl_uv[r >> 1][c] = lfm->lfl_y[r][c << 1];
      lfm->lfl_uv[(r + 2) >> 1][c] = lfm->lfl_y[r + 2][c << 1];
    }

    {
      unsigned int mask_16x16_l = mask_16x16 & 0xff;
      unsigned int mask_8x8_l = mask_8x8 & 0xff;
      unsigned int mask_4x4_l = mask_4x4 & 0xff;
      unsigned int mask_4x4_int_l = mask_4x4_int & 0xff;

// Disable filtering on the leftmost column.
#if CONFIG_HIGHBITDEPTH
      if (cm->use_highbitdepth)
        highbd_filter_selectively_vert_row2(
            plane->subsampling_x, CONVERT_TO_SHORTPTR(dst->buf), dst->stride,
            mask_16x16_l, mask_8x8_l, mask_4x4_l, mask_4x4_int_l, &cm->lf_info,
            &lfm->lfl_uv[r >> 1][0], (int)cm->bit_depth);
      else
#endif  // CONFIG_HIGHBITDEPTH
        filter_selectively_vert_row2(plane->subsampling_x, dst->buf,
                                     dst->stride, mask_16x16_l, mask_8x8_l,
                                     mask_4x4_l, mask_4x4_int_l, &cm->lf_info,
                                     &lfm->lfl_uv[r >> 1][0]);

      dst->buf += 2 * MI_SIZE * dst->stride;
      mask_16x16 >>= MI_SIZE;
      mask_8x8 >>= MI_SIZE;
      mask_4x4 >>= MI_SIZE;
      mask_4x4_int >>= MI_SIZE;
    }
  }

  // Horizontal pass
  dst->buf = dst0;
}

void av1_filter_block_plane_ss11_hor(AV1_COMMON *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm) {
  struct buf_2d *const dst = &plane->dst;
  uint8_t *const dst0 = dst->buf;
  int r, c;
  uint64_t mask_16x16 = lfm->above_uv[TX_16X16];
  uint64_t mask_8x8 = lfm->above_uv[TX_8X8];
  uint64_t mask_4x4 = lfm->above_uv[TX_4X4];
  uint64_t mask_4x4_int = lfm->above_int_4x4_uv;

  assert(plane->subsampling_x == 1 && plane->subsampling_y == 1);
  memset(lfm->lfl_uv, 0, sizeof(lfm->lfl_uv));

  // re-porpulate the filter level for uv, same as the code for vertical
  // filter in av1_filter_block_plane_ss11_ver
  for (r = 0; r < cm->mib_size && mi_row + r < cm->mi_rows; r += 4) {
    for (c = 0; c < (cm->mib_size >> 1); c++) {
      lfm->lfl_uv[r >> 1][c] = lfm->lfl_y[r][c << 1];
      lfm->lfl_uv[(r + 2) >> 1][c] = lfm->lfl_y[r + 2][c << 1];
    }
  }

  for (r = 0; r < cm->mib_size && mi_row + r < cm->mi_rows; r += 2) {
    const int skip_border_4x4_r = mi_row + r == cm->mi_rows - 1;
    const unsigned int mask_4x4_int_r =
        skip_border_4x4_r ? 0 : (mask_4x4_int & 0xf);
    unsigned int mask_16x16_r;
    unsigned int mask_8x8_r;
    unsigned int mask_4x4_r;

    if (mi_row + r == 0) {
      mask_16x16_r = 0;
      mask_8x8_r = 0;
      mask_4x4_r = 0;
    } else {
      mask_16x16_r = mask_16x16 & 0xf;
      mask_8x8_r = mask_8x8 & 0xf;
      mask_4x4_r = mask_4x4 & 0xf;
    }

#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      highbd_filter_selectively_horiz(
          CONVERT_TO_SHORTPTR(dst->buf), dst->stride, mask_16x16_r, mask_8x8_r,
          mask_4x4_r, mask_4x4_int_r, &cm->lf_info, &lfm->lfl_uv[r >> 1][0],
          (int)cm->bit_depth);
    else
#endif  // CONFIG_HIGHBITDEPTH
#if !CONFIG_LPF_DIRECT
      filter_selectively_horiz(dst->buf, dst->stride, mask_16x16_r, mask_8x8_r,
                               mask_4x4_r, mask_4x4_int_r, &cm->lf_info,
                               &lfm->lfl_uv[r >> 1][0]);
#endif  // CONFIG_LPF_DIRECT

    dst->buf += MI_SIZE * dst->stride;
    mask_16x16 >>= MI_SIZE / 2;
    mask_8x8 >>= MI_SIZE / 2;
    mask_4x4 >>= MI_SIZE / 2;
    mask_4x4_int >>= MI_SIZE / 2;
  }
  // restore the buf pointer in case there is additional filter pass.
  dst->buf = dst0;
}

#if CONFIG_PARALLEL_DEBLOCKING
typedef enum EDGE_DIR { VERT_EDGE = 0, HORZ_EDGE = 1, NUM_EDGE_DIRS } EDGE_DIR;
static const uint32_t av1_prediction_masks[NUM_EDGE_DIRS][BLOCK_SIZES_ALL] = {
  // mask for vertical edges filtering
  {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      2 - 1,   // BLOCK_2X2
      2 - 1,   // BLOCK_2X4
      4 - 1,   // BLOCK_4X2
#endif         // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      4 - 1,   // BLOCK_4X4
      4 - 1,   // BLOCK_4X8
      8 - 1,   // BLOCK_8X4
      8 - 1,   // BLOCK_8X8
      8 - 1,   // BLOCK_8X16
      16 - 1,  // BLOCK_16X8
      16 - 1,  // BLOCK_16X16
      16 - 1,  // BLOCK_16X32
      32 - 1,  // BLOCK_32X16
      32 - 1,  // BLOCK_32X32
      32 - 1,  // BLOCK_32X64
      64 - 1,  // BLOCK_64X32
      64 - 1,  // BLOCK_64X64
#if CONFIG_EXT_PARTITION
      64 - 1,   // BLOCK_64X128
      128 - 1,  // BLOCK_128X64
      128 - 1,  // BLOCK_128X128
#endif          // CONFIG_EXT_PARTITION
      4 - 1,    // BLOCK_4X16,
      16 - 1,   // BLOCK_16X4,
      8 - 1,    // BLOCK_8X32,
      32 - 1,   // BLOCK_32X8,
      16 - 1,   // BLOCK_16X64,
      64 - 1,   // BLOCK_64X16
#if CONFIG_EXT_PARTITION
      32 - 1,   // BLOCK_32X128
      128 - 1,  // BLOCK_128X32
#endif          // CONFIG_EXT_PARTITION
  },
  // mask for horizontal edges filtering
  {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      2 - 1,   // BLOCK_2X2
      4 - 1,   // BLOCK_2X4
      2 - 1,   // BLOCK_4X2
#endif         // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      4 - 1,   // BLOCK_4X4
      8 - 1,   // BLOCK_4X8
      4 - 1,   // BLOCK_8X4
      8 - 1,   // BLOCK_8X8
      16 - 1,  // BLOCK_8X16
      8 - 1,   // BLOCK_16X8
      16 - 1,  // BLOCK_16X16
      32 - 1,  // BLOCK_16X32
      16 - 1,  // BLOCK_32X16
      32 - 1,  // BLOCK_32X32
      64 - 1,  // BLOCK_32X64
      32 - 1,  // BLOCK_64X32
      64 - 1,  // BLOCK_64X64
#if CONFIG_EXT_PARTITION
      128 - 1,  // BLOCK_64X128
      64 - 1,   // BLOCK_128X64
      128 - 1,  // BLOCK_128X128
#endif          // CONFIG_EXT_PARTITION
      16 - 1,   // BLOCK_4X16,
      4 - 1,    // BLOCK_16X4,
      32 - 1,   // BLOCK_8X32,
      8 - 1,    // BLOCK_32X8,
      64 - 1,   // BLOCK_16X64,
      16 - 1,   // BLOCK_64X16
#if CONFIG_EXT_PARTITION
      128 - 1,  // BLOCK_32X128
      32 - 1,   // BLOCK_128X32
#endif          // CONFIG_EXT_PARTITION
  },
};

static const uint32_t av1_transform_masks[NUM_EDGE_DIRS][TX_SIZES_ALL] = {
  {
#if CONFIG_CHROMA_2X2
      2 - 1,  // TX_2X2
#endif
      4 - 1,   // TX_4X4
      8 - 1,   // TX_8X8
      16 - 1,  // TX_16X16
      32 - 1,  // TX_32X32
#if CONFIG_TX64X64
      64 - 1,  // TX_64X64
#endif         // CONFIG_TX64X64
      4 - 1,   // TX_4X8
      8 - 1,   // TX_8X4
      8 - 1,   // TX_8X16
      16 - 1,  // TX_16X8
      16 - 1,  // TX_16X32
      32 - 1,  // TX_32X16
#if CONFIG_TX64X64
      32 - 1,  // TX_32X64
      64 - 1,  // TX_64X32
#endif         // CONFIG_TX64X64
      4 - 1,   // TX_4X16
      16 - 1,  // TX_16X4
      8 - 1,   // TX_8X32
      32 - 1   // TX_32X8
  },
  {
#if CONFIG_CHROMA_2X2
      2 - 1,  // TX_2X2
#endif
      4 - 1,   // TX_4X4
      8 - 1,   // TX_8X8
      16 - 1,  // TX_16X16
      32 - 1,  // TX_32X32
#if CONFIG_TX64X64
      64 - 1,  // TX_64X64
#endif         // CONFIG_TX64X64
      8 - 1,   // TX_4X8
      4 - 1,   // TX_8X4
      16 - 1,  // TX_8X16
      8 - 1,   // TX_16X8
      32 - 1,  // TX_16X32
      16 - 1,  // TX_32X16
#if CONFIG_TX64X64
      64 - 1,  // TX_32X64
      32 - 1,  // TX_64X32
#endif         // CONFIG_TX64X64
      16 - 1,  // TX_4X16
      4 - 1,   // TX_16X4
      32 - 1,  // TX_8X32
      8 - 1    // TX_32X8
  }
};

static TX_SIZE av1_get_transform_size(const MODE_INFO *const mi,
                                      const EDGE_DIR edge_dir, const int mi_row,
                                      const int mi_col, const int plane,
                                      const struct macroblockd_plane *plane_ptr,
                                      const uint32_t scale_horz,
                                      const uint32_t scale_vert) {
  const MB_MODE_INFO *mbmi = &mi->mbmi;
  TX_SIZE tx_size = (plane == AOM_PLANE_Y)
                        ? mbmi->tx_size
                        : av1_get_uv_tx_size(mbmi, plane_ptr);
  assert(tx_size < TX_SIZES_ALL);

#if CONFIG_VAR_TX
  // mi_row and mi_col is the absolute position of the MI block.
  // idx_c and idx_r is the relative offset of the MI within the super block
  // c and r is the relative offset of the 8x8 block within the supert block
  // blk_row and block_col is the relative offset of the current 8x8 block
  // within the current partition.
  const int idx_c = mi_col & MAX_MIB_MASK;
  const int idx_r = mi_row & MAX_MIB_MASK;
  const int c = idx_c >> mi_width_log2_lookup[BLOCK_8X8];
  const int r = idx_r >> mi_height_log2_lookup[BLOCK_8X8];
  const BLOCK_SIZE sb_type = mi->mbmi.sb_type;
  const int blk_row = r & (num_8x8_blocks_high_lookup[sb_type] - 1);
  const int blk_col = c & (num_8x8_blocks_wide_lookup[sb_type] - 1);

  if (is_inter_block(mbmi) && !mbmi->skip) {
    const int tx_row_idx =
        (blk_row * mi_size_high[BLOCK_8X8] << TX_UNIT_HIGH_LOG2) >> 1;
    const int tx_col_idx =
        (blk_col * mi_size_wide[BLOCK_8X8] << TX_UNIT_WIDE_LOG2) >> 1;

#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
    const BLOCK_SIZE bsize =
        AOMMAX(BLOCK_4X4, ss_size_lookup[sb_type][scale_horz][scale_vert]);
#else
    const BLOCK_SIZE bsize = ss_size_lookup[sb_type][scale_horz][scale_vert];
#endif
    const TX_SIZE mb_tx_size = mbmi->inter_tx_size[tx_row_idx][tx_col_idx];

    assert(mb_tx_size < TX_SIZES_ALL);

    tx_size = (plane == AOM_PLANE_Y)
                  ? mb_tx_size
                  : uv_txsize_lookup[bsize][mb_tx_size][0][0];
    assert(tx_size < TX_SIZES_ALL);
  }
#else
  (void)mi_row;
  (void)mi_col;
  (void)scale_horz;
  (void)scale_vert;
#endif  // CONFIG_VAR_TX

  // since in case of chrominance or non-square transorm need to convert
  // transform size into transform size in particular direction.
  // for vertical edge, filter direction is horizontal, for horizontal
  // edge, filter direction is vertical.
  tx_size = (VERT_EDGE == edge_dir) ? txsize_horz_map[tx_size]
                                    : txsize_vert_map[tx_size];
  return tx_size;
}

typedef struct AV1_DEBLOCKING_PARAMETERS {
  // length of the filter applied to the outer edge
  uint32_t filter_length;
  // length of the filter applied to the inner edge
  uint32_t filter_length_internal;
  // deblocking limits
  const uint8_t *lim;
  const uint8_t *mblim;
  const uint8_t *hev_thr;
} AV1_DEBLOCKING_PARAMETERS;

static void set_lpf_parameters(
    AV1_DEBLOCKING_PARAMETERS *const params, const ptrdiff_t mode_step,
    const AV1_COMMON *const cm, const EDGE_DIR edge_dir, const uint32_t x,
    const uint32_t y, const int plane,
    const struct macroblockd_plane *const plane_ptr) {
  // reset to initial values
  params->filter_length = 0;
  params->filter_length_internal = 0;

  // no deblocking is required
  const uint32_t width = plane_ptr->dst.width;
  const uint32_t height = plane_ptr->dst.height;
  if ((width <= x) || (height <= y)) {
    return;
  }

  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  const int mi_row = (y << scale_vert) >> MI_SIZE_LOG2;
  const int mi_col = (x << scale_horz) >> MI_SIZE_LOG2;
  MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride + mi_col;
  const MB_MODE_INFO *mbmi = &mi[0]->mbmi;

  {
    const TX_SIZE ts =
        av1_get_transform_size(mi[0], edge_dir, mi_row, mi_col, plane,
                               plane_ptr, scale_horz, scale_vert);

#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
    const uint32_t curr_level =
        get_filter_level(cm, &cm->lf_info, edge_dir, plane, mbmi);
#else
#if CONFIG_LPF_SB
    const uint32_t curr_level =
        get_filter_level(cm, &cm->lf_info, mi_row, mi_col, mbmi);
#else
    const uint32_t curr_level = get_filter_level(cm, &cm->lf_info, mbmi);
#endif  // CONFIG_LPF_SB
#endif
#else
    const uint32_t curr_level = get_filter_level(&cm->lf_info, mbmi);
#endif  // CONFIG_EXT_DELTA_Q

    const int curr_skipped = mbmi->skip && is_inter_block(mbmi);
    const uint32_t coord = (VERT_EDGE == edge_dir) ? (x) : (y);
    uint32_t level = curr_level;
    // prepare outer edge parameters. deblock the edge if it's an edge of a TU
    if (coord) {
#if CONFIG_LOOPFILTERING_ACROSS_TILES
      MODE_INFO *const mi_bound = cm->mi + mi_row * cm->mi_stride + mi_col;
      if (!av1_disable_loopfilter_on_tile_boundary(cm) ||
          ((VERT_EDGE == edge_dir) &&
           (0 == (mi_bound->mbmi.boundary_info & TILE_LEFT_BOUNDARY))) ||
          ((HORZ_EDGE == edge_dir) &&
           (0 == (mi_bound->mbmi.boundary_info & TILE_ABOVE_BOUNDARY))))
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
      {
        const int32_t tu_edge =
            (coord & av1_transform_masks[edge_dir][ts]) ? (0) : (1);
        if (tu_edge) {
          const MODE_INFO *const mi_prev = *(mi - mode_step);
          const int pv_row =
              (VERT_EDGE == edge_dir) ? (mi_row) : (mi_row - (1 << scale_vert));
          const int pv_col =
              (VERT_EDGE == edge_dir) ? (mi_col - (1 << scale_horz)) : (mi_col);
          const TX_SIZE pv_ts =
              av1_get_transform_size(mi_prev, edge_dir, pv_row, pv_col, plane,
                                     plane_ptr, scale_horz, scale_vert);

#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
          const uint32_t pv_lvl = get_filter_level(cm, &cm->lf_info, edge_dir,
                                                   plane, &mi_prev->mbmi);
#else
#if CONFIG_LPF_SB
          const uint32_t pv_lvl = get_filter_level(cm, &cm->lf_info, pv_row,
                                                   pv_col, &mi_prev->mbmi);
#else
          const uint32_t pv_lvl =
              get_filter_level(cm, &cm->lf_info, &mi_prev->mbmi);
#endif  // CONFIG_LPF_SB
#endif
#else
          const uint32_t pv_lvl =
              get_filter_level(&cm->lf_info, &mi_prev->mbmi);
#endif  // CONFIG_EXT_DELTA_Q

          const int pv_skip =
              mi_prev->mbmi.skip && is_inter_block(&mi_prev->mbmi);
          const int32_t pu_edge =
              (coord &
               av1_prediction_masks[edge_dir]
                                   [ss_size_lookup[mbmi->sb_type][scale_horz]
                                                  [scale_vert]])
                  ? (0)
                  : (1);
          // if the current and the previous blocks are skipped,
          // deblock the edge if the edge belongs to a PU's edge only.
          if ((curr_level || pv_lvl) &&
              (!pv_skip || !curr_skipped || pu_edge)) {
            const TX_SIZE min_ts = AOMMIN(ts, pv_ts);
            if (TX_4X4 >= min_ts) {
              params->filter_length = 4;
            } else if (TX_8X8 == min_ts) {
              params->filter_length = 8;
            } else {
              params->filter_length = 16;
#if PARALLEL_DEBLOCKING_15TAPLUMAONLY
              // No wide filtering for chroma plane
              if (plane != 0) {
#if PARALLEL_DEBLOCKING_5_TAP_CHROMA
                params->filter_length = 6;
#else
                params->filter_length = 8;
#endif
              }
#endif
            }

#if PARALLEL_DEBLOCKING_DISABLE_15TAP
            params->filter_length = (TX_4X4 >= AOMMIN(ts, pv_ts)) ? (4) : (8);
#endif  // PARALLEL_DEBLOCKING_DISABLE_15TAP

            // update the level if the current block is skipped,
            // but the previous one is not
            level = (curr_level) ? (curr_level) : (pv_lvl);
          }
        }
      }

#if !CONFIG_CB4X4
      // prepare internal edge parameters
      if (curr_level && !curr_skipped) {
        params->filter_length_internal = (TX_4X4 >= ts) ? (4) : (0);
      }
#endif

      // prepare common parameters
      if (params->filter_length || params->filter_length_internal) {
        const loop_filter_thresh *const limits = cm->lf_info.lfthr + level;
        params->lim = limits->lim;
        params->mblim = limits->mblim;
        params->hev_thr = limits->hev_thr;
      }
    }
  }
}

static void av1_filter_block_plane_vert(
    const AV1_COMMON *const cm, const int plane,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col) {
  const int col_step = MI_SIZE >> MI_SIZE_LOG2;
  const int row_step = MI_SIZE >> MI_SIZE_LOG2;
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
#if CONFIG_LPF_SB
  int y_range = mi_row ? MAX_MIB_SIZE : MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET;
  y_range = AOMMIN(y_range, cm->mi_rows);
  y_range >>= scale_vert;

  int x_range = mi_col ? MAX_MIB_SIZE : MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET;
  x_range = AOMMIN(x_range, cm->mi_cols);
  x_range >>= scale_horz;
#else
  const int y_range = (MAX_MIB_SIZE >> scale_vert);
  const int x_range = (MAX_MIB_SIZE >> scale_horz);
#endif  // CONFIG_LPF_SB
  for (int y = 0; y < y_range; y += row_step) {
    uint8_t *p = dst_ptr + y * MI_SIZE * dst_stride;
    for (int x = 0; x < x_range; x += col_step) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will filter the vertical edge aligned with a 8x8 block.
      // If 4x4 trasnform is used, it will then filter the internal edge
      //  aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      set_lpf_parameters(&params, ((ptrdiff_t)1 << scale_horz), cm, VERT_EDGE,
                         curr_x, curr_y, plane, plane_ptr);

#if CONFIG_LPF_DIRECT
      uint8_t *const src = plane_ptr->dst.buf0;
      const int width = cm->width >> scale_horz;
      const int height = cm->height >> scale_vert;
      const int pivot = 8;
      const int line_length = 16;
      uint8_t block[128];
      int orig_pos[128];
      const int vert_or_horz = 0;  // 0: vertical
      const int unit = 1;
      int i;
      for (i = 0; i < 128; ++i) {
        block[i] = 0;
        orig_pos[i] = -1;
      }

      if (params.filter_length) {
        const int filt_len = params.filter_length == 16 ? 8 : 4;
        const int direct =
            pick_min_grad_direct(src, filt_len, curr_y, curr_x, width, height,
                                 dst_stride, unit, vert_or_horz);

        pick_filter_block_vert(src, block, orig_pos, filt_len, curr_y, curr_x,
                               width, height, dst_stride, pivot, line_length,
                               unit, direct);
        uint8_t *const filt_start = block + pivot;
        switch (params.filter_length) {
          // apply 4-tap filtering
          case 4:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_vertical_4(CONVERT_TO_SHORTPTR(filt_start),
                                        line_length, params.mblim, params.lim,
                                        params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_vertical_4(filt_start, line_length, params.mblim,
                                 params.lim, params.hev_thr);
            break;
          // apply 8-tap filtering
          case 8:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_vertical_8(CONVERT_TO_SHORTPTR(filt_start),
                                        line_length, params.mblim, params.lim,
                                        params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_vertical_8(filt_start, line_length, params.mblim,
                                 params.lim, params.hev_thr);
            break;
          // apply 16-tap filtering
          case 16:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_vertical_16(CONVERT_TO_SHORTPTR(filt_start),
                                         line_length, params.mblim, params.lim,
                                         params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_vertical_16(filt_start, line_length, params.mblim,
                                  params.lim, params.hev_thr);
            break;
          // no filtering
          default: break;
        }

        for (i = 0; i < 128; ++i) {
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
        }
      }

      if (params.filter_length_internal) {
        for (i = 0; i < 128; ++i) {
          block[i] = 0;
          orig_pos[i] = -1;
        }

        const int direct =
            pick_min_grad_direct(src, 4, curr_y, curr_x + 4, width, height,
                                 dst_stride, unit, vert_or_horz);

        pick_filter_block_vert(src, block, orig_pos, 4, curr_y, curr_x + 4,
                               width, height, dst_stride, pivot, line_length,
                               unit, direct);

        uint8_t *const filt_start = block + pivot;
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth)
          aom_highbd_lpf_vertical_4(CONVERT_TO_SHORTPTR(filt_start),
                                    line_length, params.mblim, params.lim,
                                    params.hev_thr, cm->bit_depth);
        else
#endif  // CONFIG_HIGHBITDEPTH
          aom_lpf_vertical_4(filt_start, line_length, params.mblim, params.lim,
                             params.hev_thr);

        for (i = 0; i < 128; ++i) {
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
        }
      }
#else  // !CONFIG_LPF_DIRECT
      switch (params.filter_length) {
        // apply 4-tap filtering
        case 4:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_vertical_4(CONVERT_TO_SHORTPTR(p), dst_stride,
                                      params.mblim, params.lim, params.hev_thr,
                                      cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_vertical_4(p, dst_stride, params.mblim, params.lim,
                               params.hev_thr);
          break;
#if PARALLEL_DEBLOCKING_5_TAP_CHROMA
        case 6:  // apply 6-tap filter for chroma plane only
          assert(plane != 0);
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_vertical_6_c(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_vertical_6_c(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
#endif
        // apply 8-tap filtering
        case 8:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_vertical_8(CONVERT_TO_SHORTPTR(p), dst_stride,
                                      params.mblim, params.lim, params.hev_thr,
                                      cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_vertical_8(p, dst_stride, params.mblim, params.lim,
                               params.hev_thr);
          break;
        // apply 16-tap filtering
        case 16:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
#if CONFIG_DEBLOCK_13TAP
            // TODO(olah): Remove _c once SIMD for 13-tap is available
            aom_highbd_lpf_vertical_16_c(CONVERT_TO_SHORTPTR(p), dst_stride,
                                         params.mblim, params.lim,
                                         params.hev_thr, cm->bit_depth);
#else
            aom_highbd_lpf_vertical_16(CONVERT_TO_SHORTPTR(p), dst_stride,
                                       params.mblim, params.lim, params.hev_thr,
                                       cm->bit_depth);
#endif
          else
#endif  // CONFIG_HIGHBITDEPTH
#if CONFIG_DEBLOCK_13TAP
            aom_lpf_vertical_16_c(p, dst_stride, params.mblim, params.lim,
                                  params.hev_thr);
#else
          aom_lpf_vertical_16(p, dst_stride, params.mblim, params.lim,
                              params.hev_thr);
#endif
          break;
        // no filtering
        default: break;
      }
      // process the internal edge
      if (params.filter_length_internal) {
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth)
          aom_highbd_lpf_vertical_4(CONVERT_TO_SHORTPTR(p + 4), dst_stride,
                                    params.mblim, params.lim, params.hev_thr,
                                    cm->bit_depth);
        else
#endif  // CONFIG_HIGHBITDEPTH
          aom_lpf_vertical_4(p + 4, dst_stride, params.mblim, params.lim,
                             params.hev_thr);
      }
#endif  // CONFIG_LPF_DIRECT
      // advance the destination pointer
      p += MI_SIZE;
    }
  }
}

static void av1_filter_block_plane_horz(
    const AV1_COMMON *const cm, const int plane,
    const MACROBLOCKD_PLANE *const plane_ptr, const uint32_t mi_row,
    const uint32_t mi_col) {
  const int col_step = MI_SIZE >> MI_SIZE_LOG2;
  const int row_step = MI_SIZE >> MI_SIZE_LOG2;
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
#if CONFIG_LPF_SB
  int y_range = mi_row ? MAX_MIB_SIZE : MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET;
  y_range = AOMMIN(y_range, cm->mi_rows);
  y_range >>= scale_vert;

  int x_range = mi_col ? MAX_MIB_SIZE : MAX_MIB_SIZE - FILT_BOUNDARY_MI_OFFSET;
  x_range = AOMMIN(x_range, cm->mi_cols);
  x_range >>= scale_horz;
#else
  const int y_range = (MAX_MIB_SIZE >> scale_vert);
  const int x_range = (MAX_MIB_SIZE >> scale_horz);
#endif  // CONFIG_LPF_SB
  for (int y = 0; y < y_range; y += row_step) {
    uint8_t *p = dst_ptr + y * MI_SIZE * dst_stride;
    for (int x = 0; x < x_range; x += col_step) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will first filter the vertical edge aligned with a 8x8
      // block. If 4x4 trasnform is used, it will then filter the internal
      // edge aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      set_lpf_parameters(&params, (cm->mi_stride << scale_vert), cm, HORZ_EDGE,
                         curr_x, curr_y, plane, plane_ptr);

#if CONFIG_LPF_DIRECT
      uint8_t *const src = plane_ptr->dst.buf0;
      const int width = cm->width >> scale_horz;
      const int height = cm->height >> scale_vert;
      const int pivot = 8;
      const int line_length = 16;
      uint8_t block[256];
      int orig_pos[256];
      const int vert_or_horz = 1;  // 1: horizontal
      const int unit = 1;
      int i;
      for (i = 0; i < 256; ++i) {
        block[i] = 0;
        orig_pos[i] = -1;
      }

      if (params.filter_length) {
        const int filt_len = params.filter_length == 16 ? 8 : 4;
        const int direct =
            pick_min_grad_direct(src, filt_len, curr_y, curr_x, width, height,
                                 dst_stride, unit, vert_or_horz);

        pick_filter_block_horz(src, block, orig_pos, filt_len, curr_y, curr_x,
                               width, height, dst_stride, pivot, line_length,
                               unit, direct);
        uint8_t *const filt_start = block + pivot * line_length;
        switch (params.filter_length) {
          // apply 4-tap filtering
          case 4:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_horizontal_4(CONVERT_TO_SHORTPTR(filt_start),
                                          line_length, params.mblim, params.lim,
                                          params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_horizontal_4(filt_start, line_length, params.mblim,
                                   params.lim, params.hev_thr);
            break;
          // apply 8-tap filtering
          case 8:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_horizontal_8(CONVERT_TO_SHORTPTR(filt_start),
                                          line_length, params.mblim, params.lim,
                                          params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_horizontal_8(filt_start, line_length, params.mblim,
                                   params.lim, params.hev_thr);
            break;
          // apply 16-tap filtering
          case 16:
#if CONFIG_HIGHBITDEPTH
            if (cm->use_highbitdepth)
              aom_highbd_lpf_horizontal_edge_16(
                  CONVERT_TO_SHORTPTR(filt_start), line_length, params.mblim,
                  params.lim, params.hev_thr, cm->bit_depth);
            else
#endif  // CONFIG_HIGHBITDEPTH
              aom_lpf_horizontal_edge_16(filt_start, line_length, params.mblim,
                                         params.lim, params.hev_thr);
            break;
          // no filtering
          default: break;
        }

        for (i = 0; i < 256; ++i) {
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
        }
      }
      if (params.filter_length_internal) {
        for (i = 0; i < 256; ++i) {
          block[i] = 0;
          orig_pos[i] = -1;
        }

        const int direct =
            pick_min_grad_direct(src, 4, curr_y + 4, curr_x, width, height,
                                 dst_stride, unit, vert_or_horz);

        pick_filter_block_horz(src, block, orig_pos, 4, curr_y + 4, curr_x,
                               width, height, dst_stride, pivot, line_length,
                               unit, direct);

        uint8_t *const filt_start = block + pivot * line_length;
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth)
          aom_highbd_lpf_horizontal_4(CONVERT_TO_SHORTPTR(filt_start),
                                      line_length, params.mblim, params.lim,
                                      params.hev_thr, cm->bit_depth);
        else
#endif  // CONFIG_HIGHBITDEPTH
          aom_lpf_horizontal_4(filt_start, line_length, params.mblim,
                               params.lim, params.hev_thr);

        for (i = 0; i < 256; ++i) {
          if (orig_pos[i] >= 0) src[orig_pos[i]] = block[i];
        }
      }
#else  // !CONFIG_LPF_DIRECT
      switch (params.filter_length) {
        // apply 4-tap filtering
        case 4:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_horizontal_4(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_horizontal_4(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
#if PARALLEL_DEBLOCKING_5_TAP_CHROMA
        // apply 6-tap filtering
        case 6: assert(plane != 0);
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_horizontal_6_c(CONVERT_TO_SHORTPTR(p), dst_stride,
                                          params.mblim, params.lim,
                                          params.hev_thr, cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_horizontal_6_c(p, dst_stride, params.mblim, params.lim,
                                   params.hev_thr);
          break;
#endif
        // apply 8-tap filtering
        case 8:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
            aom_highbd_lpf_horizontal_8(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, cm->bit_depth);
          else
#endif  // CONFIG_HIGHBITDEPTH
            aom_lpf_horizontal_8(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
        // apply 16-tap filtering
        case 16:
#if CONFIG_HIGHBITDEPTH
          if (cm->use_highbitdepth)
#if CONFIG_DEBLOCK_13TAP
            // TODO(olah): Remove _c once SIMD for 13-tap is available
            aom_highbd_lpf_horizontal_edge_16_c(
                CONVERT_TO_SHORTPTR(p), dst_stride, params.mblim, params.lim,
                params.hev_thr, cm->bit_depth);
#else
            aom_highbd_lpf_horizontal_edge_16(
                CONVERT_TO_SHORTPTR(p), dst_stride, params.mblim, params.lim,
                params.hev_thr, cm->bit_depth);
#endif
          else
#endif  // CONFIG_HIGHBITDEPTH
#if CONFIG_DEBLOCK_13TAP
            aom_lpf_horizontal_edge_16_c(p, dst_stride, params.mblim,
                                         params.lim, params.hev_thr);
#else
          aom_lpf_horizontal_edge_16(p, dst_stride, params.mblim, params.lim,
                                     params.hev_thr);
#endif
          break;
        // no filtering
        default: break;
      }
      // process the internal edge
      if (params.filter_length_internal) {
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth)
          aom_highbd_lpf_horizontal_4(CONVERT_TO_SHORTPTR(p + 4 * dst_stride),
                                      dst_stride, params.mblim, params.lim,
                                      params.hev_thr, cm->bit_depth);
        else
#endif  // CONFIG_HIGHBITDEPTH
          aom_lpf_horizontal_4(p + 4 * dst_stride, dst_stride, params.mblim,
                               params.lim, params.hev_thr);
      }
#endif  // CONFIG_LPF_DIRECT
      // advance the destination pointer
      p += MI_SIZE;
    }
  }
}
#endif  // CONFIG_PARALLEL_DEBLOCKING

void av1_loop_filter_rows(YV12_BUFFER_CONFIG *frame_buffer, AV1_COMMON *cm,
                          struct macroblockd_plane *planes, int start, int stop,
#if CONFIG_LPF_SB
                          int col_start, int col_end,
#endif
                          int y_only) {
#if CONFIG_LOOPFILTER_LEVEL
  // y_only no longer has its original meaning.
  // Here it means which plane to filter
  // when y_only = {0, 1, 2}, it means we are searching for filter level for
  // Y/U/V plane individually.
  const int plane_start = y_only;
  const int plane_end = plane_start + 1;
#else
  const int num_planes = y_only ? 1 : MAX_MB_PLANE;
  const int plane_start = 0;
  const int plane_end = num_planes;
#endif  // CONFIG_LOOPFILTER_LEVEL
#if !CONFIG_LPF_SB
  const int col_start = 0;
  const int col_end = cm->mi_cols;
#endif  // CONFIG_LPF_SB
  int mi_row, mi_col;
  int plane;

#if CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_EXT_PARTITION_TYPES || \
    CONFIG_CB4X4

#if !CONFIG_PARALLEL_DEBLOCKING
#if CONFIG_VAR_TX
  for (int i = 0; i < MAX_MB_PLANE; ++i)
    memset(cm->top_txfm_context[i], TX_32X32, cm->mi_cols << TX_UNIT_WIDE_LOG2);
#endif  // CONFIG_VAR_TX
  for (mi_row = start; mi_row < stop; mi_row += cm->mib_size) {
    MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride;
#if CONFIG_VAR_TX
    for (int i = 0; i < MAX_MB_PLANE; ++i)
      memset(cm->left_txfm_context[i], TX_32X32,
             MAX_MIB_SIZE << TX_UNIT_HIGH_LOG2);
#endif  // CONFIG_VAR_TX
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += cm->mib_size) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);

      for (plane = plane_start; plane < plane_end; ++plane) {
        av1_filter_block_plane_non420_ver(cm, &planes[plane], mi + mi_col,
                                          mi_row, mi_col, plane);
        av1_filter_block_plane_non420_hor(cm, &planes[plane], mi + mi_col,
                                          mi_row, mi_col, plane);
      }
    }
  }
#else

  // filter all vertical edges in every 64x64 super block
  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    for (mi_col = col_start; mi_col < col_end; mi_col += MAX_MIB_SIZE) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);
      for (plane = plane_start; plane < plane_end; ++plane) {
        av1_filter_block_plane_vert(cm, plane, &planes[plane], mi_row, mi_col);
      }
    }
  }

  // filter all horizontal edges in every 64x64 super block
  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    for (mi_col = col_start; mi_col < col_end; mi_col += MAX_MIB_SIZE) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);
      for (plane = plane_start; plane < plane_end; ++plane) {
        av1_filter_block_plane_horz(cm, plane, &planes[plane], mi_row, mi_col);
      }
    }
  }
#endif  // CONFIG_PARALLEL_DEBLOCKING

#else  // CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_EXT_PARTITION_TYPES

#if CONFIG_PARALLEL_DEBLOCKING
  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);
      // filter all vertical edges in every 64x64 super block
      for (plane = plane_start; plane < plane_end; plane += 1) {
        av1_filter_block_plane_vert(cm, plane, &planes[plane], mi_row, mi_col);
      }
    }
  }
  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);
      // filter all horizontal edges in every 64x64 super block
      for (plane = plane_start; plane < plane_end; plane += 1) {
        av1_filter_block_plane_horz(cm, plane, &planes[plane], mi_row, mi_col);
      }
    }
  }
#else   // CONFIG_PARALLEL_DEBLOCKING
  enum lf_path path;
  LOOP_FILTER_MASK lfm;

  if (y_only)
    path = LF_PATH_444;
  else if (planes[1].subsampling_y == 1 && planes[1].subsampling_x == 1)
    path = LF_PATH_420;
  else if (planes[1].subsampling_y == 0 && planes[1].subsampling_x == 0)
    path = LF_PATH_444;
  else
    path = LF_PATH_SLOW;

  for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
    MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride;
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col += MAX_MIB_SIZE) {
      av1_setup_dst_planes(planes, cm->sb_size, frame_buffer, mi_row, mi_col);

      // TODO(JBB): Make setup_mask work for non 420.
      av1_setup_mask(cm, mi_row, mi_col, mi + mi_col, cm->mi_stride, &lfm);

      av1_filter_block_plane_ss00_ver(cm, &planes[0], mi_row, &lfm);
      av1_filter_block_plane_ss00_hor(cm, &planes[0], mi_row, &lfm);
      for (plane = 1; plane < num_planes; ++plane) {
        switch (path) {
          case LF_PATH_420:
            av1_filter_block_plane_ss11_ver(cm, &planes[plane], mi_row, &lfm);
            av1_filter_block_plane_ss11_hor(cm, &planes[plane], mi_row, &lfm);
            break;
          case LF_PATH_444:
            av1_filter_block_plane_ss00_ver(cm, &planes[plane], mi_row, &lfm);
            av1_filter_block_plane_ss00_hor(cm, &planes[plane], mi_row, &lfm);
            break;
          case LF_PATH_SLOW:
            av1_filter_block_plane_non420_ver(cm, &planes[plane], mi + mi_col,
                                              mi_row, mi_col, plane);
            av1_filter_block_plane_non420_hor(cm, &planes[plane], mi + mi_col,
                                              mi_row, mi_col, plane);

            break;
        }
      }
    }
  }
#endif  // CONFIG_PARALLEL_DEBLOCKING
#endif  // CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_EXT_PARTITION_TYPES
}

void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                           MACROBLOCKD *xd, int frame_filter_level,
#if CONFIG_LOOPFILTER_LEVEL
                           int frame_filter_level_r,
#endif
                           int y_only, int partial_frame
#if CONFIG_LPF_SB
                           ,
                           int mi_row, int mi_col
#endif
                           ) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;
#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  int orig_filter_level[2] = { cm->lf.filter_level[0], cm->lf.filter_level[1] };
#else
  int orig_filter_level = cm->lf.filter_level;
#endif
#endif

#if CONFIG_LPF_SB
  if (partial_frame && !frame_filter_level) return;
#else
#if CONFIG_LOOPFILTER_LEVEL
  if (!frame_filter_level && !frame_filter_level_r) return;
#else
  if (!frame_filter_level) return;
#endif
#endif  // CONFIG_LPF_SB
#if CONFIG_LPF_SB
  int start_mi_col;
  int end_mi_col;

  // In the experiment of deblocking filtering per superblock.
  // When partial_frame is 1, it indicates we are searching for the best filter
  // level for current superblock. We reuse frame_filter_level as filter level
  // for superblock, no longer for the whole frame.
  // When partial_frame is 0, it's in the actual filtering stage for the frame
  if (partial_frame) {
    start_mi_row = AOMMAX(0, mi_row - FILT_BOUNDARY_MI_OFFSET);
    start_mi_col = AOMMAX(0, mi_col - FILT_BOUNDARY_MI_OFFSET);
    const int mi_row_range = mi_row - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
    const int mi_col_range = mi_col - FILT_BOUNDARY_MI_OFFSET + MAX_MIB_SIZE;
    end_mi_row = AOMMIN(mi_row_range, cm->mi_rows);
    end_mi_col = AOMMIN(mi_col_range, cm->mi_cols);

    av1_loop_filter_sb_level_init(cm, mi_row, mi_col, frame_filter_level);
  } else {
    start_mi_row = 0;
    mi_rows_to_filter = cm->mi_rows;
    end_mi_row = start_mi_row + mi_rows_to_filter;
    start_mi_col = 0;
    end_mi_col = cm->mi_cols;
  }
#else
  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
#if CONFIG_LOOPFILTER_LEVEL
  // TODO(chengchen): refactor the code such that y_only has its matching
  // meaning. Now it means the plane to be filtered in this experiment.
  av1_loop_filter_frame_init(cm, frame_filter_level, frame_filter_level_r,
                             y_only);
#else
  av1_loop_filter_frame_init(cm, frame_filter_level, frame_filter_level);
#endif
#endif  // CONFIG_LPF_SB

#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  cm->lf.filter_level[0] = frame_filter_level;
  cm->lf.filter_level[1] = frame_filter_level_r;
#else
  cm->lf.filter_level = frame_filter_level;
#endif
#endif

#if CONFIG_LPF_SB
  av1_loop_filter_rows(frame, cm, xd->plane, start_mi_row, end_mi_row,
                       start_mi_col, end_mi_col, y_only);
#else
  av1_loop_filter_rows(frame, cm, xd->plane, start_mi_row, end_mi_row, y_only);
#endif  // CONFIG_LPF_SB

#if CONFIG_EXT_DELTA_Q
#if CONFIG_LOOPFILTER_LEVEL
  cm->lf.filter_level[0] = orig_filter_level[0];
  cm->lf.filter_level[1] = orig_filter_level[1];
#else
  cm->lf.filter_level = orig_filter_level;
#endif
#endif
}

void av1_loop_filter_data_reset(LFWorkerData *lf_data,
                                YV12_BUFFER_CONFIG *frame_buffer,
                                struct AV1Common *cm,
                                const struct macroblockd_plane *planes) {
  lf_data->frame_buffer = frame_buffer;
  lf_data->cm = cm;
  lf_data->start = 0;
  lf_data->stop = 0;
  lf_data->y_only = 0;
  memcpy(lf_data->planes, planes, sizeof(lf_data->planes));
}

int av1_loop_filter_worker(LFWorkerData *const lf_data, void *unused) {
  (void)unused;
#if !CONFIG_LPF_SB
  av1_loop_filter_rows(lf_data->frame_buffer, lf_data->cm, lf_data->planes,
                       lf_data->start, lf_data->stop, lf_data->y_only);
#else
  (void)lf_data;
#endif  // CONFIG_LPF_SB
  return 1;
}
