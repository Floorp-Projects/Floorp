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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"

static const SEG_LVL_FEATURES seg_lvl_lf_lut[MAX_MB_PLANE][2] = {
  { SEG_LVL_ALT_LF_Y_V, SEG_LVL_ALT_LF_Y_H },
  { SEG_LVL_ALT_LF_U, SEG_LVL_ALT_LF_U },
  { SEG_LVL_ALT_LF_V, SEG_LVL_ALT_LF_V }
};

static const int delta_lf_id_lut[MAX_MB_PLANE][2] = {
  { 0, 1 }, { 2, 2 }, { 3, 3 }
};

typedef enum EDGE_DIR { VERT_EDGE = 0, HORZ_EDGE = 1, NUM_EDGE_DIRS } EDGE_DIR;

static const int mode_lf_lut[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // INTRA_MODES
  1, 1, 0, 1,                             // INTER_MODES (GLOBALMV == 0)
  1, 1, 1, 1, 1, 1, 0, 1  // INTER_COMPOUND_MODES (GLOBAL_GLOBALMV == 0)
};

#if LOOP_FILTER_BITMASK
// 256 bit masks (64x64 / 4x4) for left transform size for Y plane.
// We use 4 uint64_t to represent the 256 bit.
// Each 1 represents a position where we should apply a loop filter
// across the left border of an 4x4 block boundary.
//
// In the case of TX_8x8->  ( in low order byte first we end up with
// a mask that looks like this (-- and | are used for better view)
//
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    -----------------
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//    10101010|10101010
//
// A loopfilter should be applied to every other 4x4 horizontally.
// TODO(chengchen): make these tables static
const FilterMask left_txform_mask[TX_SIZES] = {
  { { 0xffffffffffffffffULL,  // TX_4X4,
      0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL } },

  { { 0x5555555555555555ULL,  // TX_8X8,
      0x5555555555555555ULL, 0x5555555555555555ULL, 0x5555555555555555ULL } },

  { { 0x1111111111111111ULL,  // TX_16X16,
      0x1111111111111111ULL, 0x1111111111111111ULL, 0x1111111111111111ULL } },

  { { 0x0101010101010101ULL,  // TX_32X32,
      0x0101010101010101ULL, 0x0101010101010101ULL, 0x0101010101010101ULL } },

  { { 0x0001000100010001ULL,  // TX_64X64,
      0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL } },
};

// 256 bit masks (64x64 / 4x4) for above transform size for Y plane.
// We use 4 uint64_t to represent the 256 bit.
// Each 1 represents a position where we should apply a loop filter
// across the top border of an 4x4 block boundary.
//
// In the case of TX_8x8->  ( in low order byte first we end up with
// a mask that looks like this
//
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//    -----------------
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//    11111111|11111111
//    00000000|00000000
//
// A loopfilter should be applied to every other 4x4 horizontally.
const FilterMask above_txform_mask[TX_SIZES] = {
  { { 0xffffffffffffffffULL,  // TX_4X4
      0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL } },

  { { 0x0000ffff0000ffffULL,  // TX_8X8
      0x0000ffff0000ffffULL, 0x0000ffff0000ffffULL, 0x0000ffff0000ffffULL } },

  { { 0x000000000000ffffULL,  // TX_16X16
      0x000000000000ffffULL, 0x000000000000ffffULL, 0x000000000000ffffULL } },

  { { 0x000000000000ffffULL,  // TX_32X32
      0x0000000000000000ULL, 0x000000000000ffffULL, 0x0000000000000000ULL } },

  { { 0x000000000000ffffULL,  // TX_64X64
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },
};

// 64 bit mask to shift and set for each prediction size. A bit is set for
// each 4x4 block that would be in the top left most block of the given block
// size in the 64x64 block.
const FilterMask size_mask_y[BLOCK_SIZES_ALL] = {
  { { 0x0000000000000001ULL,  // BLOCK_4X4
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0000000000010001ULL,  // BLOCK_4X8
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0000000000000003ULL,  // BLOCK_8X4
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0000000000030003ULL,  // BLOCK_8X8
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0003000300030003ULL,  // BLOCK_8X16
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x00000000000f000fULL,  // BLOCK_16X8
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x000f000f000f000fULL,  // BLOCK_16X16
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x000f000f000f000fULL,  // BLOCK_16X32
      0x000f000f000f000fULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x00ff00ff00ff00ffULL,  // BLOCK_32X16
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x00ff00ff00ff00ffULL,  // BLOCK_32X32
      0x00ff00ff00ff00ffULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x00ff00ff00ff00ffULL,  // BLOCK_32X64
      0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL, 0x00ff00ff00ff00ffULL } },

  { { 0xffffffffffffffffULL,  // BLOCK_64X32
      0xffffffffffffffffULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0xffffffffffffffffULL,  // BLOCK_64X64
      0xffffffffffffffffULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL } },
  // Y plane max coding block size is 128x128, but the codec divides it
  // into 4 64x64 blocks.
  // BLOCK_64X128
  { { 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL } },
  // BLOCK_128X64
  { { 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL } },
  // BLOCK_128X128
  { { 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL } },

  { { 0x0001000100010001ULL,  // BLOCK_4X16
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x000000000000000fULL,  // BLOCK_16X4
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0003000300030003ULL,  // BLOCK_8X32
      0x0003000300030003ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0000000000ff00ffULL,  // BLOCK_32X8
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x000f000f000f000fULL,  // BLOCK_16X64
      0x000f000f000f000fULL, 0x000f000f000f000fULL, 0x000f000f000f000fULL } },

  { { 0xffffffffffffffffULL,  // BLOCK_64X16
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } }
};

LoopFilterMask *get_loop_filter_mask(const AV1_COMMON *const cm, int mi_row,
                                     int mi_col) {
  if ((mi_row << MI_SIZE_LOG2) >= cm->height ||
      (mi_col << MI_SIZE_LOG2) >= cm->width)
    return NULL;
  assert(cm->lf.lfm != NULL);
  const int row = mi_row >> MIN_MIB_SIZE_LOG2;  // 64x64
  const int col = mi_col >> MIN_MIB_SIZE_LOG2;
  return &cm->lf.lfm[row * cm->lf.lfm_stride + col];
}

typedef void (*LpfFunc)(uint8_t *s, int p, const uint8_t *blimit,
                        const uint8_t *limit, const uint8_t *thresh);

typedef void (*LpfDualFunc)(uint8_t *s, int p, const uint8_t *blimit0,
                            const uint8_t *limit0, const uint8_t *thresh0,
                            const uint8_t *blimit1, const uint8_t *limit1,
                            const uint8_t *thresh1);

typedef void (*HbdLpfFunc)(uint16_t *s, int p, const uint8_t *blimit,
                           const uint8_t *limit, const uint8_t *thresh, int bd);

typedef void (*HbdLpfDualFunc)(uint16_t *s, int p, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1, int bd);
#endif  // LOOP_FILTER_BITMASK

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
static uint8_t get_filter_level(const AV1_COMMON *cm,
                                const loop_filter_info_n *lfi_n,
                                const int dir_idx, int plane,
                                const MB_MODE_INFO *mbmi) {
  const int segment_id = mbmi->segment_id;
  if (cm->delta_lf_present_flag) {
    int delta_lf;
    if (cm->delta_lf_multi) {
      const int delta_lf_idx = delta_lf_id_lut[plane][dir_idx];
      delta_lf = mbmi->delta_lf[delta_lf_idx];
    } else {
      delta_lf = mbmi->delta_lf_from_base;
    }
    int base_level;
    if (plane == 0)
      base_level = cm->lf.filter_level[dir_idx];
    else if (plane == 1)
      base_level = cm->lf.filter_level_u;
    else
      base_level = cm->lf.filter_level_v;
    int lvl_seg = clamp(delta_lf + base_level, 0, MAX_LOOP_FILTER);
    assert(plane >= 0 && plane <= 2);
    const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir_idx];
    if (segfeature_active(&cm->seg, segment_id, seg_lf_feature_id)) {
      const int data = get_segdata(&cm->seg, segment_id, seg_lf_feature_id);
      lvl_seg = clamp(lvl_seg + data, 0, MAX_LOOP_FILTER);
    }

    if (cm->lf.mode_ref_delta_enabled) {
      const int scale = 1 << (lvl_seg >> 5);
      lvl_seg += cm->lf.ref_deltas[mbmi->ref_frame[0]] * scale;
      if (mbmi->ref_frame[0] > INTRA_FRAME)
        lvl_seg += cm->lf.mode_deltas[mode_lf_lut[mbmi->mode]] * scale;
      lvl_seg = clamp(lvl_seg, 0, MAX_LOOP_FILTER);
    }
    return lvl_seg;
  } else {
    return lfi_n->lvl[plane][segment_id][dir_idx][mbmi->ref_frame[0]]
                     [mode_lf_lut[mbmi->mode]];
  }
}

void av1_loop_filter_init(AV1_COMMON *cm) {
  assert(MB_MODE_COUNT == NELEMENTS(mode_lf_lut));
  loop_filter_info_n *lfi = &cm->lf_info;
  struct loopfilter *lf = &cm->lf;
  int lvl;

  lf->combine_vert_horz_lf = 1;

  // init limits for given sharpness
  update_sharpness(lfi, lf->sharpness_level);

  // init hev threshold const vectors
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++)
    memset(lfi->lfthr[lvl].hev_thr, (lvl >> 4), SIMD_WIDTH);
}

// Update the loop filter for the current frame.
// This should be called before loop_filter_rows(),
// av1_loop_filter_frame() calls this function directly.
void av1_loop_filter_frame_init(AV1_COMMON *cm, int plane_start,
                                int plane_end) {
  int filt_lvl[MAX_MB_PLANE], filt_lvl_r[MAX_MB_PLANE];
  int plane;
  int seg_id;
  // n_shift is the multiplier for lf_deltas
  // the multiplier is 1 for when filter_lvl is between 0 and 31;
  // 2 when filter_lvl is between 32 and 63
  loop_filter_info_n *const lfi = &cm->lf_info;
  struct loopfilter *const lf = &cm->lf;
  const struct segmentation *const seg = &cm->seg;

  // update sharpness limits
  update_sharpness(lfi, lf->sharpness_level);

  filt_lvl[0] = cm->lf.filter_level[0];
  filt_lvl[1] = cm->lf.filter_level_u;
  filt_lvl[2] = cm->lf.filter_level_v;

  filt_lvl_r[0] = cm->lf.filter_level[1];
  filt_lvl_r[1] = cm->lf.filter_level_u;
  filt_lvl_r[2] = cm->lf.filter_level_v;

  for (plane = plane_start; plane < plane_end; plane++) {
    if (plane == 0 && !filt_lvl[0] && !filt_lvl_r[0])
      break;
    else if (plane == 1 && !filt_lvl[1])
      continue;
    else if (plane == 2 && !filt_lvl[2])
      continue;

    for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {
      for (int dir = 0; dir < 2; ++dir) {
        int lvl_seg = (dir == 0) ? filt_lvl[plane] : filt_lvl_r[plane];
        assert(plane >= 0 && plane <= 2);
        const int seg_lf_feature_id = seg_lvl_lf_lut[plane][dir];
        if (segfeature_active(seg, seg_id, seg_lf_feature_id)) {
          const int data = get_segdata(&cm->seg, seg_id, seg_lf_feature_id);
          lvl_seg = clamp(lvl_seg + data, 0, MAX_LOOP_FILTER);
        }

        if (!lf->mode_ref_delta_enabled) {
          // we could get rid of this if we assume that deltas are set to
          // zero when not in use; encoder always uses deltas
          memset(lfi->lvl[plane][seg_id][dir], lvl_seg,
                 sizeof(lfi->lvl[plane][seg_id][dir]));
        } else {
          int ref, mode;
          const int scale = 1 << (lvl_seg >> 5);
          const int intra_lvl = lvl_seg + lf->ref_deltas[INTRA_FRAME] * scale;
          lfi->lvl[plane][seg_id][dir][INTRA_FRAME][0] =
              clamp(intra_lvl, 0, MAX_LOOP_FILTER);

          for (ref = LAST_FRAME; ref < REF_FRAMES; ++ref) {
            for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
              const int inter_lvl = lvl_seg + lf->ref_deltas[ref] * scale +
                                    lf->mode_deltas[mode] * scale;
              lfi->lvl[plane][seg_id][dir][ref][mode] =
                  clamp(inter_lvl, 0, MAX_LOOP_FILTER);
            }
          }
        }
      }
    }
  }

#if LOOP_FILTER_BITMASK
  memset(lf->neighbor_sb_lpf_info.tx_size_y_above, TX_64X64,
         sizeof(TX_SIZE) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.tx_size_y_left, TX_64X64,
         sizeof(TX_SIZE) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.tx_size_uv_above, TX_64X64,
         sizeof(TX_SIZE) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.tx_size_uv_left, TX_64X64,
         sizeof(TX_SIZE) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.y_level_above, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.y_level_left, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.u_level_above, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.u_level_left, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.v_level_above, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.v_level_left, 0,
         sizeof(uint8_t) * MI_SIZE_64X64);
  memset(lf->neighbor_sb_lpf_info.skip, 0, sizeof(uint8_t) * MI_SIZE_64X64);
#endif  // LOOP_FILTER_BITMASK
}

#if LOOP_FILTER_BITMASK
// A 64x64 tx block requires 256 bits to represent each 4x4 tx block.
// Every 4 rows is represented by one uint64_t mask. Hence,
// there are 4 uint64_t bitmask[4] to represent the 64x64 block.
//
// Given a location by (mi_col, mi_row), This function returns the index
// 0, 1, 2, 3 to select which bitmask[] to use, and the shift value.
//
// For example, mi_row is the offset of pixels in mi size (4),
// (mi_row / 4) returns which uint64_t.
// After locating which uint64_t, mi_row % 4 is the
// row offset, and each row has 16 = 1 << stride_log2 4x4 units.
// Therefore, shift = (row << stride_log2) + mi_col;
static int get_index_shift(int mi_col, int mi_row, int *index) {
  // *index = mi_row >> 2;
  // rows = mi_row % 4;
  // stride_log2 = 4;
  // shift = (rows << stride_log2) + mi_col;
  *index = mi_row >> 2;
  return ((mi_row & 3) << 4) | mi_col;
}

static void check_mask(const FilterMask *lfm) {
#ifndef NDEBUG
  for (int i = 0; i < 4; ++i) {
    assert(!(lfm[TX_4X4].bits[i] & lfm[TX_8X8].bits[i]));
    assert(!(lfm[TX_4X4].bits[i] & lfm[TX_16X16].bits[i]));
    assert(!(lfm[TX_4X4].bits[i] & lfm[TX_32X32].bits[i]));
    assert(!(lfm[TX_4X4].bits[i] & lfm[TX_64X64].bits[i]));
    assert(!(lfm[TX_8X8].bits[i] & lfm[TX_16X16].bits[i]));
    assert(!(lfm[TX_8X8].bits[i] & lfm[TX_32X32].bits[i]));
    assert(!(lfm[TX_8X8].bits[i] & lfm[TX_64X64].bits[i]));
    assert(!(lfm[TX_16X16].bits[i] & lfm[TX_32X32].bits[i]));
    assert(!(lfm[TX_16X16].bits[i] & lfm[TX_64X64].bits[i]));
    assert(!(lfm[TX_32X32].bits[i] & lfm[TX_64X64].bits[i]));
  }
#else
  (void)lfm;
#endif
}

static void check_loop_filter_masks(const LoopFilterMask *lfm, int plane) {
  if (plane == 0) {
    // Assert if we try to apply 2 different loop filters at the same
    // position.
    check_mask(lfm->left_y);
    check_mask(lfm->above_y);
  } else if (plane == 1) {
    check_mask(lfm->left_u);
    check_mask(lfm->above_u);
  } else {
    check_mask(lfm->left_v);
    check_mask(lfm->above_v);
  }
}

static void update_masks(EDGE_DIR dir, int plane, uint64_t *mask,
                         TX_SIZE sqr_tx_size, LoopFilterMask *lfm) {
  if (dir == VERT_EDGE) {
    switch (plane) {
      case 0:
        for (int i = 0; i < 4; ++i) lfm->left_y[sqr_tx_size].bits[i] |= mask[i];
        break;
      case 1:
        for (int i = 0; i < 4; ++i) lfm->left_u[sqr_tx_size].bits[i] |= mask[i];
        break;
      case 2:
        for (int i = 0; i < 4; ++i) lfm->left_v[sqr_tx_size].bits[i] |= mask[i];
        break;
      default: assert(plane <= 2);
    }
  } else {
    switch (plane) {
      case 0:
        for (int i = 0; i < 4; ++i)
          lfm->above_y[sqr_tx_size].bits[i] |= mask[i];
        break;
      case 1:
        for (int i = 0; i < 4; ++i)
          lfm->above_u[sqr_tx_size].bits[i] |= mask[i];
        break;
      case 2:
        for (int i = 0; i < 4; ++i)
          lfm->above_v[sqr_tx_size].bits[i] |= mask[i];
        break;
      default: assert(plane <= 2);
    }
  }
}

static int is_frame_boundary(AV1_COMMON *const cm, int plane, int mi_row,
                             int mi_col, int ssx, int ssy, EDGE_DIR dir) {
  if (plane && (ssx || ssy)) {
    if (ssx && ssy) {  // format 420
      if ((mi_row << MI_SIZE_LOG2) > cm->height ||
          (mi_col << MI_SIZE_LOG2) > cm->width)
        return 1;
    } else if (ssx) {  // format 422
      if ((mi_row << MI_SIZE_LOG2) >= cm->height ||
          (mi_col << MI_SIZE_LOG2) > cm->width)
        return 1;
    }
  } else {
    if ((mi_row << MI_SIZE_LOG2) >= cm->height ||
        (mi_col << MI_SIZE_LOG2) >= cm->width)
      return 1;
  }

  int row_or_col;
  if (plane == 0) {
    row_or_col = dir == VERT_EDGE ? mi_col : mi_row;
  } else {
    // chroma sub8x8 block uses bottom/right mi of co-located 8x8 luma block.
    // So if mi_col == 1, it is actually the frame boundary.
    if (dir == VERT_EDGE) {
      row_or_col = ssx ? (mi_col & 0x0FFFFFFE) : mi_col;
    } else {
      row_or_col = ssy ? (mi_row & 0x0FFFFFFE) : mi_row;
    }
  }
  return row_or_col == 0;
}

static void setup_masks(AV1_COMMON *const cm, int mi_row, int mi_col, int plane,
                        int ssx, int ssy, TX_SIZE tx_size) {
  LoopFilterMask *lfm = get_loop_filter_mask(cm, mi_row, mi_col);
  const int x = (mi_col << (MI_SIZE_LOG2 - ssx));
  const int y = (mi_row << (MI_SIZE_LOG2 - ssy));
  // decide whether current vertical/horizontal edge needs loop filtering
  for (EDGE_DIR dir = VERT_EDGE; dir <= HORZ_EDGE; ++dir) {
    // chroma sub8x8 block uses bottom/right mi of co-located 8x8 luma block.
    mi_row |= ssy;
    mi_col |= ssx;

    MB_MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride + mi_col;
    const MB_MODE_INFO *const mbmi = mi[0];
    const int curr_skip = mbmi->skip && is_inter_block(mbmi);
    const BLOCK_SIZE bsize = mbmi->sb_type;
    const BLOCK_SIZE bsizec = scale_chroma_bsize(bsize, ssx, ssy);
    const BLOCK_SIZE plane_bsize = ss_size_lookup[bsizec][ssx][ssy];
    const uint8_t level = get_filter_level(cm, &cm->lf_info, dir, plane, mbmi);
    const int prediction_masks = dir == VERT_EDGE
                                     ? block_size_wide[plane_bsize] - 1
                                     : block_size_high[plane_bsize] - 1;
    const int is_coding_block_border =
        dir == VERT_EDGE ? !(x & prediction_masks) : !(y & prediction_masks);

    // TODO(chengchen): step can be optimized.
    const int row_step = mi_size_high[TX_4X4] << ssy;
    const int col_step = mi_size_wide[TX_4X4] << ssx;
    const int mi_height =
        dir == VERT_EDGE ? tx_size_high_unit[tx_size] << ssy : row_step;
    const int mi_width =
        dir == VERT_EDGE ? col_step : tx_size_wide_unit[tx_size] << ssx;

    // assign filter levels
    for (int r = mi_row; r < mi_row + mi_height; r += row_step) {
      for (int c = mi_col; c < mi_col + mi_width; c += col_step) {
        // do not filter frame boundary
        // Note: when chroma planes' size are half of luma plane,
        // chroma plane mi corresponds to even position.
        // If frame size is not even, we still need to filter this chroma
        // position. Therefore the boundary condition check needs to be
        // separated to two cases.
        if (plane && (ssx || ssy)) {
          if (ssx && ssy) {  // format 420
            if ((r << MI_SIZE_LOG2) > cm->height ||
                (c << MI_SIZE_LOG2) > cm->width)
              continue;
          } else if (ssx) {  // format 422
            if ((r << MI_SIZE_LOG2) >= cm->height ||
                (c << MI_SIZE_LOG2) > cm->width)
              continue;
          }
        } else {
          if ((r << MI_SIZE_LOG2) >= cm->height ||
              (c << MI_SIZE_LOG2) >= cm->width)
            continue;
        }

        const int row = r % MI_SIZE_64X64;
        const int col = c % MI_SIZE_64X64;
        if (plane == 0) {
          if (dir == VERT_EDGE)
            lfm->lfl_y_ver[row][col] = level;
          else
            lfm->lfl_y_hor[row][col] = level;
        } else if (plane == 1) {
          if (dir == VERT_EDGE)
            lfm->lfl_u_ver[row][col] = level;
          else
            lfm->lfl_u_hor[row][col] = level;
        } else {
          if (dir == VERT_EDGE)
            lfm->lfl_v_ver[row][col] = level;
          else
            lfm->lfl_v_hor[row][col] = level;
        }
      }
    }

    for (int r = mi_row; r < mi_row + mi_height; r += row_step) {
      for (int c = mi_col; c < mi_col + mi_width; c += col_step) {
        // do not filter frame boundary
        if (is_frame_boundary(cm, plane, r, c, ssx, ssy, dir)) continue;

        uint64_t mask[4] = { 0 };
        const int prev_row = dir == VERT_EDGE ? r : r - (1 << ssy);
        const int prev_col = dir == VERT_EDGE ? c - (1 << ssx) : c;
        MB_MODE_INFO **mi_prev =
            cm->mi_grid_visible + prev_row * cm->mi_stride + prev_col;
        const MB_MODE_INFO *const mbmi_prev = mi_prev[0];
        const int prev_skip = mbmi_prev->skip && is_inter_block(mbmi_prev);
        const uint8_t level_prev =
            get_filter_level(cm, &cm->lf_info, dir, plane, mbmi_prev);
        const int is_edge =
            (level || level_prev) &&
            (!curr_skip || !prev_skip || is_coding_block_border);

        if (is_edge) {
          const TX_SIZE prev_tx_size =
              plane ? av1_get_max_uv_txsize(mbmi_prev->sb_type, ssx, ssy)
                    : mbmi_prev->tx_size;
          const TX_SIZE min_tx_size =
              (dir == VERT_EDGE) ? AOMMIN(txsize_horz_map[tx_size],
                                          txsize_horz_map[prev_tx_size])
                                 : AOMMIN(txsize_vert_map[tx_size],
                                          txsize_vert_map[prev_tx_size]);
          assert(min_tx_size < TX_SIZES);
          const int row = r % MI_SIZE_64X64;
          const int col = c % MI_SIZE_64X64;
          int index = 0;
          const int shift = get_index_shift(col, row, &index);
          assert(index < 4 && index >= 0);
          mask[index] |= ((uint64_t)1 << shift);
          // set mask on corresponding bit
          update_masks(dir, plane, mask, min_tx_size, lfm);
        }
      }
    }
  }
}

static void setup_tx_block_mask(AV1_COMMON *const cm, int mi_row, int mi_col,
                                int blk_row, int blk_col,
                                BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                                int plane, int ssx, int ssy) {
  blk_row <<= ssy;
  blk_col <<= ssx;
  if (((mi_row + blk_row) << MI_SIZE_LOG2) >= cm->height ||
      ((mi_col + blk_col) << MI_SIZE_LOG2) >= cm->width)
    return;

  // U/V plane, tx_size is always the largest size
  if (plane) {
    assert(tx_size_wide[tx_size] <= 32 && tx_size_high[tx_size] <= 32);
    setup_masks(cm, mi_row + blk_row, mi_col + blk_col, plane, ssx, ssy,
                tx_size);
    return;
  }

  MB_MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride + mi_col;
  const MB_MODE_INFO *const mbmi = mi[0];
  // For Y plane:
  // If intra block, tx size is univariant.
  // If inter block, tx size follows inter_tx_size.
  TX_SIZE plane_tx_size = tx_size;
  const int is_inter = is_inter_block(mbmi);

  if (plane == 0) {
    if (is_inter) {
      if (mbmi->skip) {
        // TODO(chengchen): change av1_get_transform_size() to be consistant.
        // plane_tx_size = get_max_rect_tx_size(plane_bsize);
        plane_tx_size = mbmi->tx_size;
      } else {
        plane_tx_size = mbmi->inter_tx_size[av1_get_txb_size_index(
            plane_bsize, blk_row, blk_col)];
      }
    } else {
      MB_MODE_INFO **mi_this = cm->mi_grid_visible +
                               (mi_row + blk_row) * cm->mi_stride + mi_col +
                               blk_col;
      const MB_MODE_INFO *const mbmi_this = mi_this[0];
      plane_tx_size = mbmi_this->tx_size;
    }
  }

  assert(txsize_to_bsize[plane_tx_size] <= plane_bsize);

  if (plane || plane_tx_size == tx_size) {
    setup_masks(cm, mi_row + blk_row, mi_col + blk_col, plane, ssx, ssy,
                tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;
        setup_tx_block_mask(cm, mi_row, mi_col, offsetr, offsetc, plane_bsize,
                            sub_txs, plane, ssx, ssy);
      }
    }
  }
}

static void setup_fix_block_mask(AV1_COMMON *const cm, int mi_row, int mi_col,
                                 int plane, int ssx, int ssy) {
  MB_MODE_INFO **mi =
      cm->mi_grid_visible + (mi_row | ssy) * cm->mi_stride + (mi_col | ssx);
  const MB_MODE_INFO *const mbmi = mi[0];

  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE bsizec = scale_chroma_bsize(bsize, ssx, ssy);
  const BLOCK_SIZE plane_bsize = ss_size_lookup[bsizec][ssx][ssy];

  const int block_width = mi_size_wide[plane_bsize];
  const int block_height = mi_size_high[plane_bsize];

  TX_SIZE max_txsize = max_txsize_rect_lookup[plane_bsize];
  // The decoder is designed so that it can process 64x64 luma pixels at a
  // time. If this is a chroma plane with subsampling and bsize corresponds to
  // a subsampled BLOCK_128X128 then the lookup above will give TX_64X64. That
  // mustn't be used for the subsampled plane (because it would be bigger than
  // a 64x64 luma block) so we round down to TX_32X32.
  if (plane && txsize_sqr_up_map[max_txsize] == TX_64X64) {
    if (max_txsize == TX_16X64)
      max_txsize = TX_16X32;
    else if (max_txsize == TX_64X16)
      max_txsize = TX_32X16;
    else
      max_txsize = TX_32X32;
  }

  const BLOCK_SIZE txb_size = txsize_to_bsize[max_txsize];
  const int bw = block_size_wide[txb_size] >> tx_size_wide_log2[0];
  const int bh = block_size_high[txb_size] >> tx_size_wide_log2[0];
  const BLOCK_SIZE max_unit_bsize = ss_size_lookup[BLOCK_64X64][ssx][ssy];
  int mu_blocks_wide = block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
  int mu_blocks_high = block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

  mu_blocks_wide = AOMMIN(block_width, mu_blocks_wide);
  mu_blocks_high = AOMMIN(block_height, mu_blocks_high);

  // Y: Largest tx_size is 64x64, while superblock size can be 128x128.
  // Here we ensure that setup_tx_block_mask process at most a 64x64 block.
  // U/V: largest tx size is 32x32.
  for (int idy = 0; idy < block_height; idy += mu_blocks_high) {
    for (int idx = 0; idx < block_width; idx += mu_blocks_wide) {
      const int unit_height = AOMMIN(mu_blocks_high + idy, block_height);
      const int unit_width = AOMMIN(mu_blocks_wide + idx, block_width);
      for (int blk_row = idy; blk_row < unit_height; blk_row += bh) {
        for (int blk_col = idx; blk_col < unit_width; blk_col += bw) {
          setup_tx_block_mask(cm, mi_row, mi_col, blk_row, blk_col, plane_bsize,
                              max_txsize, plane, ssx, ssy);
        }
      }
    }
  }
}

static void setup_block_mask(AV1_COMMON *const cm, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int plane, int ssx, int ssy) {
  if ((mi_row << MI_SIZE_LOG2) >= cm->height ||
      (mi_col << MI_SIZE_LOG2) >= cm->width)
    return;

  const PARTITION_TYPE partition = get_partition(cm, mi_row, mi_col, bsize);
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  const int hbs = mi_size_wide[bsize] / 2;
  const int quarter_step = mi_size_wide[bsize] / 4;
  const int allow_sub8x8 = (ssx || ssy) ? bsize > BLOCK_8X8 : 1;
  const int has_next_row =
      (((mi_row + hbs) << MI_SIZE_LOG2) < cm->height) & allow_sub8x8;
  const int has_next_col =
      (((mi_col + hbs) << MI_SIZE_LOG2) < cm->width) & allow_sub8x8;
  int i;

  switch (partition) {
    case PARTITION_NONE:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      break;
    case PARTITION_HORZ:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col, plane, ssx, ssy);
      break;
    case PARTITION_VERT:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_col)
        setup_fix_block_mask(cm, mi_row, mi_col + hbs, plane, ssx, ssy);
      break;
    case PARTITION_SPLIT:
      setup_block_mask(cm, mi_row, mi_col, subsize, plane, ssx, ssy);
      if (has_next_col)
        setup_block_mask(cm, mi_row, mi_col + hbs, subsize, plane, ssx, ssy);
      if (has_next_row)
        setup_block_mask(cm, mi_row + hbs, mi_col, subsize, plane, ssx, ssy);
      if (has_next_col & has_next_row)
        setup_block_mask(cm, mi_row + hbs, mi_col + hbs, subsize, plane, ssx,
                         ssy);
      break;
    case PARTITION_HORZ_A:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_col)
        setup_fix_block_mask(cm, mi_row, mi_col + hbs, plane, ssx, ssy);
      if (has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col, plane, ssx, ssy);
      break;
    case PARTITION_HORZ_B:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col, plane, ssx, ssy);
      if (has_next_col & has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col + hbs, plane, ssx, ssy);
      break;
    case PARTITION_VERT_A:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col, plane, ssx, ssy);
      if (has_next_col)
        setup_fix_block_mask(cm, mi_row, mi_col + hbs, plane, ssx, ssy);
      break;
    case PARTITION_VERT_B:
      setup_fix_block_mask(cm, mi_row, mi_col, plane, ssx, ssy);
      if (has_next_col)
        setup_fix_block_mask(cm, mi_row, mi_col + hbs, plane, ssx, ssy);
      if (has_next_row)
        setup_fix_block_mask(cm, mi_row + hbs, mi_col + hbs, plane, ssx, ssy);
      break;
    case PARTITION_HORZ_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * quarter_step;
        if (i > 0 && (this_mi_row << MI_SIZE_LOG2) >= cm->height) break;
        // chroma plane filter the odd location
        if (plane && bsize == BLOCK_16X16 && (i & 0x01)) continue;

        setup_fix_block_mask(cm, this_mi_row, mi_col, plane, ssx, ssy);
      }
      break;
    case PARTITION_VERT_4:
      for (i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * quarter_step;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;
        // chroma plane filter the odd location
        if (plane && bsize == BLOCK_16X16 && (i & 0x01)) continue;

        setup_fix_block_mask(cm, mi_row, this_mi_col, plane, ssx, ssy);
      }
      break;
    default: assert(0);
  }
}

// TODO(chengchen): if lossless, do not need to setup mask. But when
// segments enabled, each segment has different lossless settings.
void av1_setup_bitmask(AV1_COMMON *const cm, int mi_row, int mi_col, int plane,
                       int subsampling_x, int subsampling_y, int row_end,
                       int col_end) {
  const int num_64x64 = cm->seq_params.mib_size >> MIN_MIB_SIZE_LOG2;
  for (int y = 0; y < num_64x64; ++y) {
    for (int x = 0; x < num_64x64; ++x) {
      const int row = mi_row + y * MI_SIZE_64X64;
      const int col = mi_col + x * MI_SIZE_64X64;
      if (row >= row_end || col >= col_end) continue;
      if ((row << MI_SIZE_LOG2) >= cm->height ||
          (col << MI_SIZE_LOG2) >= cm->width)
        continue;

      LoopFilterMask *lfm = get_loop_filter_mask(cm, row, col);
      if (lfm == NULL) return;

      // init mask to zero
      if (plane == 0) {
        av1_zero(lfm->left_y);
        av1_zero(lfm->above_y);
        av1_zero(lfm->lfl_y_ver);
        av1_zero(lfm->lfl_y_hor);
      } else if (plane == 1) {
        av1_zero(lfm->left_u);
        av1_zero(lfm->above_u);
        av1_zero(lfm->lfl_u_ver);
        av1_zero(lfm->lfl_u_hor);
      } else {
        av1_zero(lfm->left_v);
        av1_zero(lfm->above_v);
        av1_zero(lfm->lfl_v_ver);
        av1_zero(lfm->lfl_v_hor);
      }
    }
  }

  // set up bitmask for each superblock
  setup_block_mask(cm, mi_row, mi_col, cm->seq_params.sb_size, plane,
                   subsampling_x, subsampling_y);

  for (int y = 0; y < num_64x64; ++y) {
    for (int x = 0; x < num_64x64; ++x) {
      const int row = mi_row + y * MI_SIZE_64X64;
      const int col = mi_col + x * MI_SIZE_64X64;
      if (row >= row_end || col >= col_end) continue;
      if ((row << MI_SIZE_LOG2) >= cm->height ||
          (col << MI_SIZE_LOG2) >= cm->width)
        continue;

      LoopFilterMask *lfm = get_loop_filter_mask(cm, row, col);
      if (lfm == NULL) return;

      // check if the mask is valid
      check_loop_filter_masks(lfm, plane);

      {
        // Let 16x16 hold 32x32 (Y/U/V) and 64x64(Y only).
        // Even tx size is greater, we only apply max length filter, which
        // is 16.
        if (plane == 0) {
          for (int j = 0; j < 4; ++j) {
            lfm->left_y[TX_16X16].bits[j] |= lfm->left_y[TX_32X32].bits[j];
            lfm->left_y[TX_16X16].bits[j] |= lfm->left_y[TX_64X64].bits[j];
            lfm->above_y[TX_16X16].bits[j] |= lfm->above_y[TX_32X32].bits[j];
            lfm->above_y[TX_16X16].bits[j] |= lfm->above_y[TX_64X64].bits[j];

            // set 32x32 and 64x64 to 0
            lfm->left_y[TX_32X32].bits[j] = 0;
            lfm->left_y[TX_64X64].bits[j] = 0;
            lfm->above_y[TX_32X32].bits[j] = 0;
            lfm->above_y[TX_64X64].bits[j] = 0;
          }
        } else if (plane == 1) {
          for (int j = 0; j < 4; ++j) {
            lfm->left_u[TX_16X16].bits[j] |= lfm->left_u[TX_32X32].bits[j];
            lfm->above_u[TX_16X16].bits[j] |= lfm->above_u[TX_32X32].bits[j];

            // set 32x32 to 0
            lfm->left_u[TX_32X32].bits[j] = 0;
            lfm->above_u[TX_32X32].bits[j] = 0;
          }
        } else {
          for (int j = 0; j < 4; ++j) {
            lfm->left_v[TX_16X16].bits[j] |= lfm->left_v[TX_32X32].bits[j];
            lfm->above_v[TX_16X16].bits[j] |= lfm->above_v[TX_32X32].bits[j];

            // set 32x32 to 0
            lfm->left_v[TX_32X32].bits[j] = 0;
            lfm->above_v[TX_32X32].bits[j] = 0;
          }
        }
      }

      // check if the mask is valid
      check_loop_filter_masks(lfm, plane);
    }
  }
}

static void filter_selectively_vert_row2(
    int subsampling_factor, uint8_t *s, int pitch, int plane,
    uint64_t mask_16x16_0, uint64_t mask_8x8_0, uint64_t mask_4x4_0,
    uint64_t mask_16x16_1, uint64_t mask_8x8_1, uint64_t mask_4x4_1,
    const loop_filter_info_n *lfi_n, uint8_t *lfl, uint8_t *lfl2) {
  uint64_t mask;
  const int step = 1 << subsampling_factor;

  for (mask = mask_16x16_0 | mask_8x8_0 | mask_4x4_0 | mask_16x16_1 |
              mask_8x8_1 | mask_4x4_1;
       mask; mask >>= step) {
    const loop_filter_thresh *lfi0 = lfi_n->lfthr + *lfl;
    const loop_filter_thresh *lfi1 = lfi_n->lfthr + *lfl2;

    if (mask & 1) {
      if ((mask_16x16_0 | mask_16x16_1) & 1) {
        // chroma plane filters less pixels introduced in deblock_13tap
        // experiment
        LpfFunc lpf_vertical = plane ? aom_lpf_vertical_6 : aom_lpf_vertical_14;

        if ((mask_16x16_0 & mask_16x16_1) & 1) {
          if (plane) {
            // TODO(any): add aom_lpf_vertical_6_dual for chroma plane.
            aom_lpf_vertical_6(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
            aom_lpf_vertical_6(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                               lfi1->hev_thr);
          } else {
            // TODO(any): add dual function simd function. Current sse2 code
            // just called aom_lpf_vertical_14_sse2 twice.
            aom_lpf_vertical_14_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                     lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                     lfi1->hev_thr);
          }
        } else if (mask_16x16_0 & 1) {
          lpf_vertical(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
        } else {
          lpf_vertical(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                       lfi1->hev_thr);
        }
      }

      if ((mask_8x8_0 | mask_8x8_1) & 1) {
        // chroma plane filters less pixels introduced in deblock_13tap
        // experiment
        LpfFunc lpf_vertical = plane ? aom_lpf_vertical_6 : aom_lpf_vertical_8;

        if ((mask_8x8_0 & mask_8x8_1) & 1) {
          if (plane) {
            aom_lpf_vertical_6(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
            aom_lpf_vertical_6(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                               lfi1->hev_thr);
          } else {
            aom_lpf_vertical_8_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                    lfi0->hev_thr, lfi1->mblim, lfi1->lim,
                                    lfi1->hev_thr);
          }
        } else if (mask_8x8_0 & 1) {
          lpf_vertical(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr);
        } else {
          lpf_vertical(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
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
          aom_lpf_vertical_4(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                             lfi1->hev_thr);
        }
      }
    }

    s += 4;
    lfl += step;
    lfl2 += step;
    mask_16x16_0 >>= step;
    mask_8x8_0 >>= step;
    mask_4x4_0 >>= step;
    mask_16x16_1 >>= step;
    mask_8x8_1 >>= step;
    mask_4x4_1 >>= step;
  }
}

static void highbd_filter_selectively_vert_row2(
    int subsampling_factor, uint16_t *s, int pitch, int plane,
    uint64_t mask_16x16_0, uint64_t mask_8x8_0, uint64_t mask_4x4_0,
    uint64_t mask_16x16_1, uint64_t mask_8x8_1, uint64_t mask_4x4_1,
    const loop_filter_info_n *lfi_n, uint8_t *lfl, uint8_t *lfl2, int bd) {
  uint64_t mask;
  const int step = 1 << subsampling_factor;

  for (mask = mask_16x16_0 | mask_8x8_0 | mask_4x4_0 | mask_16x16_1 |
              mask_8x8_1 | mask_4x4_1;
       mask; mask >>= step) {
    const loop_filter_thresh *lfi0 = lfi_n->lfthr + *lfl;
    const loop_filter_thresh *lfi1 = lfi_n->lfthr + *lfl2;

    if (mask & 1) {
      if ((mask_16x16_0 | mask_16x16_1) & 1) {
        // chroma plane filters less pixels introduced in deblock_13tap
        // experiment
        HbdLpfFunc highbd_lpf_vertical =
            plane ? aom_highbd_lpf_vertical_6 : aom_highbd_lpf_vertical_14;

        if ((mask_16x16_0 & mask_16x16_1) & 1) {
          if (plane) {
            aom_highbd_lpf_vertical_6(s, pitch, lfi0->mblim, lfi0->lim,
                                      lfi0->hev_thr, bd);
            aom_highbd_lpf_vertical_6(s + 4 * pitch, pitch, lfi1->mblim,
                                      lfi1->lim, lfi1->hev_thr, bd);
          } else {
            aom_highbd_lpf_vertical_14_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                            lfi0->hev_thr, lfi1->mblim,
                                            lfi1->lim, lfi1->hev_thr, bd);
          }
        } else if (mask_16x16_0 & 1) {
          highbd_lpf_vertical(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr,
                              bd);
        } else {
          highbd_lpf_vertical(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                              lfi1->hev_thr, bd);
        }
      }

      if ((mask_8x8_0 | mask_8x8_1) & 1) {
        HbdLpfFunc highbd_lpf_vertical =
            plane ? aom_highbd_lpf_vertical_6 : aom_highbd_lpf_vertical_8;

        if ((mask_8x8_0 & mask_8x8_1) & 1) {
          if (plane) {
            aom_highbd_lpf_vertical_6(s, pitch, lfi0->mblim, lfi0->lim,
                                      lfi0->hev_thr, bd);
            aom_highbd_lpf_vertical_6(s + 4 * pitch, pitch, lfi1->mblim,
                                      lfi1->lim, lfi1->hev_thr, bd);
          } else {
            aom_highbd_lpf_vertical_8_dual(s, pitch, lfi0->mblim, lfi0->lim,
                                           lfi0->hev_thr, lfi1->mblim,
                                           lfi1->lim, lfi1->hev_thr, bd);
          }
        } else if (mask_8x8_0 & 1) {
          highbd_lpf_vertical(s, pitch, lfi0->mblim, lfi0->lim, lfi0->hev_thr,
                              bd);
        } else {
          highbd_lpf_vertical(s + 4 * pitch, pitch, lfi1->mblim, lfi1->lim,
                              lfi1->hev_thr, bd);
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
          aom_highbd_lpf_vertical_4(s + 4 * pitch, pitch, lfi1->mblim,
                                    lfi1->lim, lfi1->hev_thr, bd);
        }
      }
    }

    s += 4;
    lfl += step;
    lfl2 += step;
    mask_16x16_0 >>= step;
    mask_8x8_0 >>= step;
    mask_4x4_0 >>= step;
    mask_16x16_1 >>= step;
    mask_8x8_1 >>= step;
    mask_4x4_1 >>= step;
  }
}

static void filter_selectively_horiz(uint8_t *s, int pitch, int plane,
                                     int subsampling, uint64_t mask_16x16,
                                     uint64_t mask_8x8, uint64_t mask_4x4,
                                     const loop_filter_info_n *lfi_n,
                                     const uint8_t *lfl) {
  uint64_t mask;
  int count;
  const int step = 1 << subsampling;
  const unsigned int two_block_mask = subsampling ? 5 : 3;

  for (mask = mask_16x16 | mask_8x8 | mask_4x4; mask; mask >>= step * count) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;
    // Next block's thresholds.
    const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + step);

    count = 1;
    if (mask & 1) {
      if (mask_16x16 & 1) {
        // chroma plane filters less pixels introduced in deblock_13tap
        // experiment
        LpfFunc lpf_horizontal =
            plane ? aom_lpf_horizontal_6 : aom_lpf_horizontal_14;

        if ((mask_16x16 & two_block_mask) == two_block_mask) {
          /*
          aom_lpf_horizontal_14_dual(s, pitch, lfi->mblim, lfi->lim,
                                     lfi->hev_thr);
          */

          lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
          lpf_horizontal(s + 4, pitch, lfin->mblim, lfin->lim, lfin->hev_thr);
          count = 2;
        } else {
          lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
        }
      } else if (mask_8x8 & 1) {
        // chroma plane filters less pixels introduced in deblock_13tap
        // experiment
        LpfFunc lpf_horizontal =
            plane ? aom_lpf_horizontal_6 : aom_lpf_horizontal_8;

        if ((mask_8x8 & two_block_mask) == two_block_mask) {
          /*
          aom_lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr, lfin->mblim, lfin->lim,
                                    lfin->hev_thr);
          */

          lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
          lpf_horizontal(s + 4, pitch, lfin->mblim, lfin->lim, lfin->hev_thr);
          count = 2;
        } else {
          lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
        }
      } else if (mask_4x4 & 1) {
        if ((mask_4x4 & two_block_mask) == two_block_mask) {
          /*
          aom_lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                                    lfi->hev_thr, lfin->mblim, lfin->lim,
                                    lfin->hev_thr);
          */
          aom_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
          aom_lpf_horizontal_4(s + 4, pitch, lfin->mblim, lfin->lim,
                               lfin->hev_thr);
          count = 2;
        } else {
          aom_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);
        }
      }
    }

    s += 4 * count;
    lfl += step * count;
    mask_16x16 >>= step * count;
    mask_8x8 >>= step * count;
    mask_4x4 >>= step * count;
  }
}

static void highbd_filter_selectively_horiz(
    uint16_t *s, int pitch, int plane, int subsampling, uint64_t mask_16x16,
    uint64_t mask_8x8, uint64_t mask_4x4, const loop_filter_info_n *lfi_n,
    uint8_t *lfl, int bd) {
  uint64_t mask;
  int count;
  const int step = 1 << subsampling;
  const unsigned int two_block_mask = subsampling ? 5 : 3;

  for (mask = mask_16x16 | mask_8x8 | mask_4x4; mask; mask >>= step * count) {
    const loop_filter_thresh *lfi = lfi_n->lfthr + *lfl;
    // Next block's thresholds.
    const loop_filter_thresh *lfin = lfi_n->lfthr + *(lfl + step);

    count = 1;
    if (mask & 1) {
      if (mask_16x16 & 1) {
        HbdLpfFunc highbd_lpf_horizontal =
            plane ? aom_highbd_lpf_horizontal_6 : aom_highbd_lpf_horizontal_14;

        if ((mask_16x16 & two_block_mask) == two_block_mask) {
          /*
          aom_highbd_lpf_horizontal_14_dual(s, pitch, lfi->mblim, lfi->lim,
                                            lfi->hev_thr, bd);
          */

          highbd_lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                bd);
          highbd_lpf_horizontal(s + 4, pitch, lfin->mblim, lfin->lim,
                                lfin->hev_thr, bd);
          count = 2;
        } else {
          highbd_lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                bd);
        }
      } else if (mask_8x8 & 1) {
        HbdLpfFunc highbd_lpf_horizontal =
            plane ? aom_highbd_lpf_horizontal_6 : aom_highbd_lpf_horizontal_8;

        if ((mask_8x8 & two_block_mask) == two_block_mask) {
          /*
          aom_highbd_lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                                           lfi->hev_thr, lfin->mblim, lfin->lim,
                                           lfin->hev_thr, bd);
          */
          highbd_lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                bd);
          highbd_lpf_horizontal(s + 4, pitch, lfin->mblim, lfin->lim,
                                lfin->hev_thr, bd);
          count = 2;
        } else {
          highbd_lpf_horizontal(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                bd);
        }
      } else if (mask_4x4 & 1) {
        if ((mask_4x4 & two_block_mask) == two_block_mask) {
          /*
          aom_highbd_lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                                           lfi->hev_thr, lfin->mblim, lfin->lim,
                                           lfin->hev_thr, bd);
          */
          aom_highbd_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim,
                                      lfi->hev_thr, bd);
          aom_highbd_lpf_horizontal_4(s + 4, pitch, lfin->mblim, lfin->lim,
                                      lfin->hev_thr, bd);
          count = 2;
        } else {
          aom_highbd_lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim,
                                      lfi->hev_thr, bd);
        }
      }
    }

    s += 4 * count;
    lfl += step * count;
    mask_16x16 >>= step * count;
    mask_8x8 >>= step * count;
    mask_4x4 >>= step * count;
  }
}

static int compare_ref_dst(AV1_COMMON *const cm, uint8_t *ref_buf,
                           uint8_t *dst_buf, int ref_stride, int dst_stride,
                           int start, int end) {
  return 0;

  start <<= MI_SIZE_LOG2;
  end <<= MI_SIZE_LOG2;
  uint8_t *ref0 = ref_buf;
  uint8_t *dst0 = dst_buf;
  if (cm->seq_params.use_highbitdepth) {
    const uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref_buf);
    const uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst_buf);
    for (int j = 0; j < 4; ++j) {
      for (int i = start; i < end; ++i)
        if (ref16[i] != dst16[i]) {
          ref_buf = ref0;
          dst_buf = dst0;
          return i + 1;
        }
      ref16 += ref_stride;
      dst16 += dst_stride;
    }
  } else {
    for (int j = 0; j < 4; ++j) {
      for (int i = start; i < end; ++i)
        if (ref_buf[i] != dst_buf[i]) {
          ref_buf = ref0;
          dst_buf = dst0;
          return i + 1;
        }
      ref_buf += ref_stride;
      dst_buf += dst_stride;
    }
  }
  ref_buf = ref0;
  dst_buf = dst0;
  return 0;
}

void av1_filter_block_plane_ver(AV1_COMMON *const cm,
                                struct macroblockd_plane *const plane_ptr,
                                int pl, int mi_row, int mi_col) {
  struct buf_2d *const dst = &plane_ptr->dst;
  int r, c;
  const int ssx = plane_ptr->subsampling_x;
  const int ssy = plane_ptr->subsampling_y;
  const int mask_cutoff = 0xffff;
  const int single_step = 1 << ssy;
  const int r_step = 2 << ssy;
  uint64_t mask_16x16 = 0;
  uint64_t mask_8x8 = 0;
  uint64_t mask_4x4 = 0;
  uint8_t *lfl;
  uint8_t *lfl2;

  // filter two rows at a time
  for (r = 0; r < cm->seq_params.mib_size &&
              ((mi_row + r) << MI_SIZE_LOG2 < cm->height);
       r += r_step) {
    for (c = 0; c < cm->seq_params.mib_size &&
                ((mi_col + c) << MI_SIZE_LOG2 < cm->width);
         c += MI_SIZE_64X64) {
      dst->buf += ((c << MI_SIZE_LOG2) >> ssx);
      LoopFilterMask *lfm = get_loop_filter_mask(cm, mi_row + r, mi_col + c);
      assert(lfm);
      const int row = ((mi_row + r) | ssy) % MI_SIZE_64X64;
      const int col = ((mi_col + c) | ssx) % MI_SIZE_64X64;
      int index = 0;
      const int shift = get_index_shift(col, row, &index);
      // current and next row should belong to the same mask_idx and index
      // next row's shift
      const int row_next = row + single_step;
      int index_next = 0;
      const int shift_next = get_index_shift(col, row_next, &index_next);
      switch (pl) {
        case 0:
          mask_16x16 = lfm->left_y[TX_16X16].bits[index];
          mask_8x8 = lfm->left_y[TX_8X8].bits[index];
          mask_4x4 = lfm->left_y[TX_4X4].bits[index];
          lfl = &lfm->lfl_y_ver[row][col];
          lfl2 = &lfm->lfl_y_ver[row_next][col];
          break;
        case 1:
          mask_16x16 = lfm->left_u[TX_16X16].bits[index];
          mask_8x8 = lfm->left_u[TX_8X8].bits[index];
          mask_4x4 = lfm->left_u[TX_4X4].bits[index];
          lfl = &lfm->lfl_u_ver[row][col];
          lfl2 = &lfm->lfl_u_ver[row_next][col];
          break;
        case 2:
          mask_16x16 = lfm->left_v[TX_16X16].bits[index];
          mask_8x8 = lfm->left_v[TX_8X8].bits[index];
          mask_4x4 = lfm->left_v[TX_4X4].bits[index];
          lfl = &lfm->lfl_v_ver[row][col];
          lfl2 = &lfm->lfl_v_ver[row_next][col];
          break;
        default: assert(pl >= 0 && pl <= 2); return;
      }
      uint64_t mask_16x16_0 = (mask_16x16 >> shift) & mask_cutoff;
      uint64_t mask_8x8_0 = (mask_8x8 >> shift) & mask_cutoff;
      uint64_t mask_4x4_0 = (mask_4x4 >> shift) & mask_cutoff;
      uint64_t mask_16x16_1 = (mask_16x16 >> shift_next) & mask_cutoff;
      uint64_t mask_8x8_1 = (mask_8x8 >> shift_next) & mask_cutoff;
      uint64_t mask_4x4_1 = (mask_4x4 >> shift_next) & mask_cutoff;

      if (cm->seq_params.use_highbitdepth)
        highbd_filter_selectively_vert_row2(
            ssx, CONVERT_TO_SHORTPTR(dst->buf), dst->stride, pl, mask_16x16_0,
            mask_8x8_0, mask_4x4_0, mask_16x16_1, mask_8x8_1, mask_4x4_1,
            &cm->lf_info, lfl, lfl2, (int)cm->seq_params.bit_depth);
      else
        filter_selectively_vert_row2(ssx, dst->buf, dst->stride, pl,
                                     mask_16x16_0, mask_8x8_0, mask_4x4_0,
                                     mask_16x16_1, mask_8x8_1, mask_4x4_1,
                                     &cm->lf_info, lfl, lfl2);
      dst->buf -= ((c << MI_SIZE_LOG2) >> ssx);
    }
    dst->buf += 2 * MI_SIZE * dst->stride;
  }
}

void av1_filter_block_plane_hor(AV1_COMMON *const cm,
                                struct macroblockd_plane *const plane_ptr,
                                int pl, int mi_row, int mi_col) {
  struct buf_2d *const dst = &plane_ptr->dst;
  int r, c;
  const int ssx = plane_ptr->subsampling_x;
  const int ssy = plane_ptr->subsampling_y;
  const int mask_cutoff = 0xffff;
  const int r_step = 1 << ssy;
  uint64_t mask_16x16 = 0;
  uint64_t mask_8x8 = 0;
  uint64_t mask_4x4 = 0;
  uint8_t *lfl;

  for (r = 0; r < cm->seq_params.mib_size &&
              ((mi_row + r) << MI_SIZE_LOG2 < cm->height);
       r += r_step) {
    for (c = 0; c < cm->seq_params.mib_size &&
                ((mi_col + c) << MI_SIZE_LOG2 < cm->width);
         c += MI_SIZE_64X64) {
      if (mi_row + r == 0) continue;

      dst->buf += ((c << MI_SIZE_LOG2) >> ssx);
      LoopFilterMask *lfm = get_loop_filter_mask(cm, mi_row + r, mi_col + c);
      assert(lfm);
      const int row = ((mi_row + r) | ssy) % MI_SIZE_64X64;
      const int col = ((mi_col + c) | ssx) % MI_SIZE_64X64;
      int index = 0;
      const int shift = get_index_shift(col, row, &index);
      switch (pl) {
        case 0:
          mask_16x16 = lfm->above_y[TX_16X16].bits[index];
          mask_8x8 = lfm->above_y[TX_8X8].bits[index];
          mask_4x4 = lfm->above_y[TX_4X4].bits[index];
          lfl = &lfm->lfl_y_hor[row][col];
          break;
        case 1:
          mask_16x16 = lfm->above_u[TX_16X16].bits[index];
          mask_8x8 = lfm->above_u[TX_8X8].bits[index];
          mask_4x4 = lfm->above_u[TX_4X4].bits[index];
          lfl = &lfm->lfl_u_hor[row][col];
          break;
        case 2:
          mask_16x16 = lfm->above_v[TX_16X16].bits[index];
          mask_8x8 = lfm->above_v[TX_8X8].bits[index];
          mask_4x4 = lfm->above_v[TX_4X4].bits[index];
          lfl = &lfm->lfl_v_hor[row][col];
          break;
        default: assert(pl >= 0 && pl <= 2); return;
      }
      mask_16x16 = (mask_16x16 >> shift) & mask_cutoff;
      mask_8x8 = (mask_8x8 >> shift) & mask_cutoff;
      mask_4x4 = (mask_4x4 >> shift) & mask_cutoff;

      if (cm->seq_params.use_highbitdepth)
        highbd_filter_selectively_horiz(CONVERT_TO_SHORTPTR(dst->buf),
                                        dst->stride, pl, ssx, mask_16x16,
                                        mask_8x8, mask_4x4, &cm->lf_info, lfl,
                                        (int)cm->seq_params.bit_depth);
      else
        filter_selectively_horiz(dst->buf, dst->stride, pl, ssx, mask_16x16,
                                 mask_8x8, mask_4x4, &cm->lf_info, lfl);
      dst->buf -= ((c << MI_SIZE_LOG2) >> ssx);
    }
    dst->buf += MI_SIZE * dst->stride;
  }
}
#endif  // LOOP_FILTER_BITMASK

static TX_SIZE get_transform_size(const MACROBLOCKD *const xd,
                                  const MB_MODE_INFO *const mbmi,
                                  const EDGE_DIR edge_dir, const int mi_row,
                                  const int mi_col, const int plane,
                                  const struct macroblockd_plane *plane_ptr) {
  assert(mbmi != NULL);
  if (xd && xd->lossless[mbmi->segment_id]) return TX_4X4;

  TX_SIZE tx_size =
      (plane == AOM_PLANE_Y)
          ? mbmi->tx_size
          : av1_get_max_uv_txsize(mbmi->sb_type, plane_ptr->subsampling_x,
                                  plane_ptr->subsampling_y);
  assert(tx_size < TX_SIZES_ALL);
  if ((plane == AOM_PLANE_Y) && is_inter_block(mbmi) && !mbmi->skip) {
    const BLOCK_SIZE sb_type = mbmi->sb_type;
    const int blk_row = mi_row & (mi_size_high[sb_type] - 1);
    const int blk_col = mi_col & (mi_size_wide[sb_type] - 1);
    const TX_SIZE mb_tx_size =
        mbmi->inter_tx_size[av1_get_txb_size_index(sb_type, blk_row, blk_col)];
    assert(mb_tx_size < TX_SIZES_ALL);
    tx_size = mb_tx_size;
  }

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
  // deblocking limits
  const uint8_t *lim;
  const uint8_t *mblim;
  const uint8_t *hev_thr;
} AV1_DEBLOCKING_PARAMETERS;

// Return TX_SIZE from get_transform_size(), so it is plane and direction
// awared
static TX_SIZE set_lpf_parameters(
    AV1_DEBLOCKING_PARAMETERS *const params, const ptrdiff_t mode_step,
    const AV1_COMMON *const cm, const MACROBLOCKD *const xd,
    const EDGE_DIR edge_dir, const uint32_t x, const uint32_t y,
    const int plane, const struct macroblockd_plane *const plane_ptr) {
  // reset to initial values
  params->filter_length = 0;

  // no deblocking is required
  const uint32_t width = plane_ptr->dst.width;
  const uint32_t height = plane_ptr->dst.height;
  if ((width <= x) || (height <= y)) {
    // just return the smallest transform unit size
    return TX_4X4;
  }

  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  // for sub8x8 block, chroma prediction mode is obtained from the bottom/right
  // mi structure of the co-located 8x8 luma block. so for chroma plane, mi_row
  // and mi_col should map to the bottom/right mi structure, i.e, both mi_row
  // and mi_col should be odd number for chroma plane.
  const int mi_row = scale_vert | ((y << scale_vert) >> MI_SIZE_LOG2);
  const int mi_col = scale_horz | ((x << scale_horz) >> MI_SIZE_LOG2);
  MB_MODE_INFO **mi = cm->mi_grid_visible + mi_row * cm->mi_stride + mi_col;
  const MB_MODE_INFO *mbmi = mi[0];
  // If current mbmi is not correctly setup, return an invalid value to stop
  // filtering. One example is that if this tile is not coded, then its mbmi
  // it not set up.
  if (mbmi == NULL) return TX_INVALID;

  const TX_SIZE ts =
      get_transform_size(xd, mi[0], edge_dir, mi_row, mi_col, plane, plane_ptr);

  {
    const uint32_t coord = (VERT_EDGE == edge_dir) ? (x) : (y);
    const uint32_t transform_masks =
        edge_dir == VERT_EDGE ? tx_size_wide[ts] - 1 : tx_size_high[ts] - 1;
    const int32_t tu_edge = (coord & transform_masks) ? (0) : (1);

    if (!tu_edge) return ts;

    // prepare outer edge parameters. deblock the edge if it's an edge of a TU
    {
      const uint32_t curr_level =
          get_filter_level(cm, &cm->lf_info, edge_dir, plane, mbmi);
      const int curr_skipped = mbmi->skip && is_inter_block(mbmi);
      uint32_t level = curr_level;
      if (coord) {
        {
          const MB_MODE_INFO *const mi_prev = *(mi - mode_step);
          if (mi_prev == NULL) return TX_INVALID;
          const int pv_row =
              (VERT_EDGE == edge_dir) ? (mi_row) : (mi_row - (1 << scale_vert));
          const int pv_col =
              (VERT_EDGE == edge_dir) ? (mi_col - (1 << scale_horz)) : (mi_col);
          const TX_SIZE pv_ts = get_transform_size(
              xd, mi_prev, edge_dir, pv_row, pv_col, plane, plane_ptr);

          const uint32_t pv_lvl =
              get_filter_level(cm, &cm->lf_info, edge_dir, plane, mi_prev);

          const int pv_skip = mi_prev->skip && is_inter_block(mi_prev);
          const BLOCK_SIZE bsize =
              get_plane_block_size(mbmi->sb_type, plane_ptr->subsampling_x,
                                   plane_ptr->subsampling_y);
          const int prediction_masks = edge_dir == VERT_EDGE
                                           ? block_size_wide[bsize] - 1
                                           : block_size_high[bsize] - 1;
          const int32_t pu_edge = !(coord & prediction_masks);
          // if the current and the previous blocks are skipped,
          // deblock the edge if the edge belongs to a PU's edge only.
          if ((curr_level || pv_lvl) &&
              (!pv_skip || !curr_skipped || pu_edge)) {
            const TX_SIZE min_ts = AOMMIN(ts, pv_ts);
            if (TX_4X4 >= min_ts) {
              params->filter_length = 4;
            } else if (TX_8X8 == min_ts) {
              if (plane != 0)
                params->filter_length = 6;
              else
                params->filter_length = 8;
            } else {
              params->filter_length = 14;
              // No wide filtering for chroma plane
              if (plane != 0) {
                params->filter_length = 6;
              }
            }

            // update the level if the current block is skipped,
            // but the previous one is not
            level = (curr_level) ? (curr_level) : (pv_lvl);
          }
        }
      }
      // prepare common parameters
      if (params->filter_length) {
        const loop_filter_thresh *const limits = cm->lf_info.lfthr + level;
        params->lim = limits->lim;
        params->mblim = limits->mblim;
        params->hev_thr = limits->hev_thr;
      }
    }
  }

  return ts;
}

void av1_filter_block_plane_vert(const AV1_COMMON *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col) {
  const int row_step = MI_SIZE >> MI_SIZE_LOG2;
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  const int y_range = (MAX_MIB_SIZE >> scale_vert);
  const int x_range = (MAX_MIB_SIZE >> scale_horz);
  const int use_highbitdepth = cm->seq_params.use_highbitdepth;
  const aom_bit_depth_t bit_depth = cm->seq_params.bit_depth;
  for (int y = 0; y < y_range; y += row_step) {
    uint8_t *p = dst_ptr + y * MI_SIZE * dst_stride;
    for (int x = 0; x < x_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will filter the vertical edge aligned with a 8x8 block.
      // If 4x4 trasnform is used, it will then filter the internal edge
      //  aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      uint32_t advance_units;
      TX_SIZE tx_size;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      tx_size =
          set_lpf_parameters(&params, ((ptrdiff_t)1 << scale_horz), cm, xd,
                             VERT_EDGE, curr_x, curr_y, plane, plane_ptr);
      if (tx_size == TX_INVALID) {
        params.filter_length = 0;
        tx_size = TX_4X4;
      }

      switch (params.filter_length) {
        // apply 4-tap filtering
        case 4:
          if (use_highbitdepth)
            aom_highbd_lpf_vertical_4(CONVERT_TO_SHORTPTR(p), dst_stride,
                                      params.mblim, params.lim, params.hev_thr,
                                      bit_depth);
          else
            aom_lpf_vertical_4(p, dst_stride, params.mblim, params.lim,
                               params.hev_thr);
          break;
        case 6:  // apply 6-tap filter for chroma plane only
          assert(plane != 0);
          if (use_highbitdepth)
            aom_highbd_lpf_vertical_6(CONVERT_TO_SHORTPTR(p), dst_stride,
                                      params.mblim, params.lim, params.hev_thr,
                                      bit_depth);
          else
            aom_lpf_vertical_6(p, dst_stride, params.mblim, params.lim,
                               params.hev_thr);
          break;
        // apply 8-tap filtering
        case 8:
          if (use_highbitdepth)
            aom_highbd_lpf_vertical_8(CONVERT_TO_SHORTPTR(p), dst_stride,
                                      params.mblim, params.lim, params.hev_thr,
                                      bit_depth);
          else
            aom_lpf_vertical_8(p, dst_stride, params.mblim, params.lim,
                               params.hev_thr);
          break;
        // apply 14-tap filtering
        case 14:
          if (use_highbitdepth)
            aom_highbd_lpf_vertical_14(CONVERT_TO_SHORTPTR(p), dst_stride,
                                       params.mblim, params.lim, params.hev_thr,
                                       bit_depth);
          else
            aom_lpf_vertical_14(p, dst_stride, params.mblim, params.lim,
                                params.hev_thr);
          break;
        // no filtering
        default: break;
      }
      // advance the destination pointer
      advance_units = tx_size_wide_unit[tx_size];
      x += advance_units;
      p += advance_units * MI_SIZE;
    }
  }
}

void av1_filter_block_plane_horz(const AV1_COMMON *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col) {
  const int col_step = MI_SIZE >> MI_SIZE_LOG2;
  const uint32_t scale_horz = plane_ptr->subsampling_x;
  const uint32_t scale_vert = plane_ptr->subsampling_y;
  uint8_t *const dst_ptr = plane_ptr->dst.buf;
  const int dst_stride = plane_ptr->dst.stride;
  const int y_range = (MAX_MIB_SIZE >> scale_vert);
  const int x_range = (MAX_MIB_SIZE >> scale_horz);
  const int use_highbitdepth = cm->seq_params.use_highbitdepth;
  const aom_bit_depth_t bit_depth = cm->seq_params.bit_depth;
  for (int x = 0; x < x_range; x += col_step) {
    uint8_t *p = dst_ptr + x * MI_SIZE;
    for (int y = 0; y < y_range;) {
      // inner loop always filter vertical edges in a MI block. If MI size
      // is 8x8, it will first filter the vertical edge aligned with a 8x8
      // block. If 4x4 trasnform is used, it will then filter the internal
      // edge aligned with a 4x4 block
      const uint32_t curr_x = ((mi_col * MI_SIZE) >> scale_horz) + x * MI_SIZE;
      const uint32_t curr_y = ((mi_row * MI_SIZE) >> scale_vert) + y * MI_SIZE;
      uint32_t advance_units;
      TX_SIZE tx_size;
      AV1_DEBLOCKING_PARAMETERS params;
      memset(&params, 0, sizeof(params));

      tx_size =
          set_lpf_parameters(&params, (cm->mi_stride << scale_vert), cm, xd,
                             HORZ_EDGE, curr_x, curr_y, plane, plane_ptr);
      if (tx_size == TX_INVALID) {
        params.filter_length = 0;
        tx_size = TX_4X4;
      }

      switch (params.filter_length) {
        // apply 4-tap filtering
        case 4:
          if (use_highbitdepth)
            aom_highbd_lpf_horizontal_4(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, bit_depth);
          else
            aom_lpf_horizontal_4(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
        // apply 6-tap filtering
        case 6:
          assert(plane != 0);
          if (use_highbitdepth)
            aom_highbd_lpf_horizontal_6(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, bit_depth);
          else
            aom_lpf_horizontal_6(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
        // apply 8-tap filtering
        case 8:
          if (use_highbitdepth)
            aom_highbd_lpf_horizontal_8(CONVERT_TO_SHORTPTR(p), dst_stride,
                                        params.mblim, params.lim,
                                        params.hev_thr, bit_depth);
          else
            aom_lpf_horizontal_8(p, dst_stride, params.mblim, params.lim,
                                 params.hev_thr);
          break;
        // apply 14-tap filtering
        case 14:
          if (use_highbitdepth)
            aom_highbd_lpf_horizontal_14(CONVERT_TO_SHORTPTR(p), dst_stride,
                                         params.mblim, params.lim,
                                         params.hev_thr, bit_depth);
          else
            aom_lpf_horizontal_14(p, dst_stride, params.mblim, params.lim,
                                  params.hev_thr);
          break;
        // no filtering
        default: break;
      }

      // advance the destination pointer
      advance_units = tx_size_high_unit[tx_size];
      y += advance_units;
      p += advance_units * dst_stride * MI_SIZE;
    }
  }
}

static void loop_filter_rows(YV12_BUFFER_CONFIG *frame_buffer, AV1_COMMON *cm,
                             MACROBLOCKD *xd, int start, int stop,
                             int plane_start, int plane_end) {
  struct macroblockd_plane *pd = xd->plane;
  const int col_start = 0;
  const int col_end = cm->mi_cols;
  int mi_row, mi_col;
  int plane;

  for (plane = plane_start; plane < plane_end; plane++) {
    if (plane == 0 && !(cm->lf.filter_level[0]) && !(cm->lf.filter_level[1]))
      break;
    else if (plane == 1 && !(cm->lf.filter_level_u))
      continue;
    else if (plane == 2 && !(cm->lf.filter_level_v))
      continue;

#if LOOP_FILTER_BITMASK
    // filter all vertical edges every superblock (could be 128x128 or 64x64)
    for (mi_row = start; mi_row < stop; mi_row += cm->seq_params.mib_size) {
      for (mi_col = col_start; mi_col < col_end;
           mi_col += cm->seq_params.mib_size) {
        av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                             mi_col, plane, plane + 1);

        av1_setup_bitmask(cm, mi_row, mi_col, plane, pd[plane].subsampling_x,
                          pd[plane].subsampling_y, stop, col_end);
        av1_filter_block_plane_ver(cm, &pd[plane], plane, mi_row, mi_col);
      }
    }

    // filter all horizontal edges every superblock
    for (mi_row = start; mi_row < stop; mi_row += cm->seq_params.mib_size) {
      for (mi_col = col_start; mi_col < col_end;
           mi_col += cm->seq_params.mib_size) {
        av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                             mi_col, plane, plane + 1);

        av1_filter_block_plane_hor(cm, &pd[plane], plane, mi_row, mi_col);
      }
    }
#else
    if (cm->lf.combine_vert_horz_lf) {
      // filter all vertical and horizontal edges in every 128x128 super block
      for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
        for (mi_col = col_start; mi_col < col_end; mi_col += MAX_MIB_SIZE) {
          // filter vertical edges
          av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                               mi_col, plane, plane + 1);
          av1_filter_block_plane_vert(cm, xd, plane, &pd[plane], mi_row,
                                      mi_col);
          // filter horizontal edges
          if (mi_col - MAX_MIB_SIZE >= 0) {
            av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer,
                                 mi_row, mi_col - MAX_MIB_SIZE, plane,
                                 plane + 1);
            av1_filter_block_plane_horz(cm, xd, plane, &pd[plane], mi_row,
                                        mi_col - MAX_MIB_SIZE);
          }
        }
        // filter horizontal edges
        av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                             mi_col - MAX_MIB_SIZE, plane, plane + 1);
        av1_filter_block_plane_horz(cm, xd, plane, &pd[plane], mi_row,
                                    mi_col - MAX_MIB_SIZE);
      }
    } else {
      // filter all vertical edges in every 128x128 super block
      for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
        for (mi_col = col_start; mi_col < col_end; mi_col += MAX_MIB_SIZE) {
          av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                               mi_col, plane, plane + 1);
          av1_filter_block_plane_vert(cm, xd, plane, &pd[plane], mi_row,
                                      mi_col);
        }
      }

      // filter all horizontal edges in every 128x128 super block
      for (mi_row = start; mi_row < stop; mi_row += MAX_MIB_SIZE) {
        for (mi_col = col_start; mi_col < col_end; mi_col += MAX_MIB_SIZE) {
          av1_setup_dst_planes(pd, cm->seq_params.sb_size, frame_buffer, mi_row,
                               mi_col, plane, plane + 1);
          av1_filter_block_plane_horz(cm, xd, plane, &pd[plane], mi_row,
                                      mi_col);
        }
      }
    }
#endif  // LOOP_FILTER_BITMASK
  }
}

void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                           MACROBLOCKD *xd, int plane_start, int plane_end,
                           int partial_frame) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;

  start_mi_row = 0;
  mi_rows_to_filter = cm->mi_rows;
  if (partial_frame && cm->mi_rows > 8) {
    start_mi_row = cm->mi_rows >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(cm->mi_rows / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  av1_loop_filter_frame_init(cm, plane_start, plane_end);
  loop_filter_rows(frame, cm, xd, start_mi_row, end_mi_row, plane_start,
                   plane_end);
}
