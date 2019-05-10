/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
 * Changes made compared to libaom version:
 * - we disable TMV and enable MV_COMPRESS so that the
 *   input array for prev_frames can be at 4x4 instead of
 *   8x8 resolution, and therefore shared between cur_frame
 *   and prev_frame. To make enc/dec behave consistent, we
 *   also make this change around line 2580:
#if 0
                AOMMIN(((mi_row >> 1) << 1) + 1 + (((xd->n8_h - 1) >> 1) << 1),
                       mi_row_end - 1) *
                    prev_frame_mvs_stride +
                AOMMIN(((mi_col >> 1) << 1) + 1 + (((xd->n8_w - 1) >> 1) << 1),
                       mi_col_end - 1)
#else
                (((mi_row >> 1) << 1) + 1) * prev_frame_mvs_stride +
                (((mi_col >> 1) << 1) + 1)
#endif
 *   and the same change (swap mi_cols from prev_frame.mv_stride) on line 2407
 * - we disable rect-block overhanging edge inclusion (see
 *   line 2642):
  if (num_8x8_blocks_wide == num_8x8_blocks_high || 1) {
    mv_ref_search[5].row = -1;
    mv_ref_search[5].col = 0;
    mv_ref_search[6].row = 0;
    mv_ref_search[6].col = -1;
  } else {
    mv_ref_search[5].row = -1;
    mv_ref_search[5].col = num_8x8_blocks_wide;
    mv_ref_search[6].row = num_8x8_blocks_high;
    mv_ref_search[6].col = -1;
  }
 *   Note that this is a bitstream change and needs the same
 *   change on the decoder side also.
 * - we change xd->mi to be a pointer instead of a double ptr.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dav1d/common.h"

#include "common/intops.h"

#define av1_zero(a) memset(a, 0, sizeof(a))

#define ATTRIBUTE_PACKED
#define INLINE inline
#define IMPLIES(a, b) (!(a) || (b))  //  Logical 'a implies b' (or 'a -> b')

#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))
#define ROUND_POWER_OF_TWO_SIGNED(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO(-(value), (n)) \
                 : ROUND_POWER_OF_TWO((value), (n)))
#define NELEMENTS(x) (int)(sizeof(x) / sizeof(x[0]))

#define MAX_MV_REF_CANDIDATES 2

#define MAX_REF_MV_STACK_SIZE 8
#define REF_CAT_LEVEL 640

#define FRAME_OFFSET_BITS 5
#define MAX_FRAME_DISTANCE ((1 << FRAME_OFFSET_BITS) - 1)
#define INVALID_MV 0x80008000

#define COMP_NEWMV_CTXS 5
#define REFMV_OFFSET 4
#define REFMV_CTX_MASK ((1 << (8 - REFMV_OFFSET)) - 1)

#define MV_IN_USE_BITS 14
#define MV_UPP (1 << MV_IN_USE_BITS)
#define MV_LOW (-(1 << MV_IN_USE_BITS))

typedef struct MV {
    int16_t row;
    int16_t col;
} MV;
typedef union int_mv {
    uint32_t as_int;
    MV as_mv;
} int_mv;
typedef int8_t MV_REFERENCE_FRAME;
#define MFMV_STACK_SIZE 3
typedef struct {
  int_mv mfmv0;
  uint8_t ref_frame_offset;
} TPL_MV_REF;
typedef struct {
    int_mv mv[2];
    MV_REFERENCE_FRAME ref_frame[2];
    int8_t mode, sb_type;
} MV_REF;
#define MB_MODE_INFO MV_REF

#define AOMMAX(a,b) ((a)>(b)?(a):(b))
#define AOMMIN(a,b) ((a)<(b)?(a):(b))

typedef struct candidate_mv {
    int_mv this_mv;
    int_mv comp_mv;
    int weight;
} CANDIDATE_MV;
#define NONE_FRAME -1
#define INTRA_FRAME 0
#define LAST_FRAME 1

#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF2_FRAME 6
#define ALTREF_FRAME 7
#define LAST_REF_FRAMES (LAST3_FRAME - LAST_FRAME + 1)

#define INTER_REFS_PER_FRAME (ALTREF_FRAME - LAST_FRAME + 1)
#define TOTAL_REFS_PER_FRAME (ALTREF_FRAME - INTRA_FRAME + 1)

#define FWD_REFS (GOLDEN_FRAME - LAST_FRAME + 1)
#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)
#define BWD_REFS (ALTREF_FRAME - BWDREF_FRAME + 1)
#define BWD_RF_OFFSET(ref) (ref - BWDREF_FRAME)
#define FWD_REFS (GOLDEN_FRAME - LAST_FRAME + 1)
#define SINGLE_REFS (FWD_REFS + BWD_REFS)
typedef enum ATTRIBUTE_PACKED {
  LAST_LAST2_FRAMES,      // { LAST_FRAME, LAST2_FRAME }
  LAST_LAST3_FRAMES,      // { LAST_FRAME, LAST3_FRAME }
  LAST_GOLDEN_FRAMES,     // { LAST_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF_FRAMES,   // { BWDREF_FRAME, ALTREF_FRAME }
  LAST2_LAST3_FRAMES,     // { LAST2_FRAME, LAST3_FRAME }
  LAST2_GOLDEN_FRAMES,    // { LAST2_FRAME, GOLDEN_FRAME }
  LAST3_GOLDEN_FRAMES,    // { LAST3_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF2_FRAMES,  // { BWDREF_FRAME, ALTREF2_FRAME }
  ALTREF2_ALTREF_FRAMES,  // { ALTREF2_FRAME, ALTREF_FRAME }
  TOTAL_UNIDIR_COMP_REFS,
  // NOTE: UNIDIR_COMP_REFS is the number of uni-directional reference pairs
  //       that are explicitly signaled.
  UNIDIR_COMP_REFS = BWDREF_ALTREF_FRAMES + 1,
} UNIDIR_COMP_REF;
#define TOTAL_COMP_REFS (FWD_REFS * BWD_REFS + TOTAL_UNIDIR_COMP_REFS)
#define MODE_CTX_REF_FRAMES (TOTAL_REFS_PER_FRAME + TOTAL_COMP_REFS)

#define GLOBALMV_OFFSET 3
#define NEWMV_CTX_MASK ((1 << GLOBALMV_OFFSET) - 1)
#define GLOBALMV_CTX_MASK ((1 << (REFMV_OFFSET - GLOBALMV_OFFSET)) - 1)
#define MI_SIZE_LOG2 2
#define MI_SIZE (1 << MI_SIZE_LOG2)
#define MAX_SB_SIZE_LOG2 7
#define MAX_MIB_SIZE_LOG2 (MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2)
#define MIN_MIB_SIZE_LOG2 (MIN_SB_SIZE_LOG2 - MI_SIZE_LOG2)
#define MAX_MIB_SIZE (1 << MAX_MIB_SIZE_LOG2)
#define MI_SIZE_64X64 (64 >> MI_SIZE_LOG2)
#define MI_SIZE_128X128 (128 >> MI_SIZE_LOG2)
#define REFMV_OFFSET 4

typedef enum ATTRIBUTE_PACKED {
  BLOCK_4X4,
  BLOCK_4X8,
  BLOCK_8X4,
  BLOCK_8X8,
  BLOCK_8X16,
  BLOCK_16X8,
  BLOCK_16X16,
  BLOCK_16X32,
  BLOCK_32X16,
  BLOCK_32X32,
  BLOCK_32X64,
  BLOCK_64X32,
  BLOCK_64X64,
  BLOCK_64X128,
  BLOCK_128X64,
  BLOCK_128X128,
  BLOCK_4X16,
  BLOCK_16X4,
  BLOCK_8X32,
  BLOCK_32X8,
  BLOCK_16X64,
  BLOCK_64X16,
  BLOCK_32X128,
  BLOCK_128X32,
  BLOCK_SIZES_ALL,
  BLOCK_SIZES = BLOCK_4X16,
  BLOCK_INVALID = 255,
  BLOCK_LARGEST = (BLOCK_SIZES - 1)
} BLOCK_SIZE;

typedef enum ATTRIBUTE_PACKED {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_HORZ_A,  // HORZ split and the top partition is split again
  PARTITION_HORZ_B,  // HORZ split and the bottom partition is split again
  PARTITION_VERT_A,  // VERT split and the left partition is split again
  PARTITION_VERT_B,  // VERT split and the right partition is split again
  PARTITION_HORZ_4,  // 4:1 horizontal partition
  PARTITION_VERT_4,  // 4:1 vertical partition
  EXT_PARTITION_TYPES,
  PARTITION_TYPES = PARTITION_SPLIT + 1,
  PARTITION_INVALID = 255
} PARTITION_TYPE;
typedef struct CUR_MODE_INFO {
  PARTITION_TYPE partition;
} CUR_MODE_INFO ;

typedef enum ATTRIBUTE_PACKED {
  DC_PRED,        // Average of above and left pixels
  V_PRED,         // Vertical
  H_PRED,         // Horizontal
  D45_PRED,       // Directional 45  deg = round(arctan(1/1) * 180/pi)
  D135_PRED,      // Directional 135 deg = 180 - 45
  D117_PRED,      // Directional 117 deg = 180 - 63
  D153_PRED,      // Directional 153 deg = 180 - 27
  D207_PRED,      // Directional 207 deg = 180 + 27
  D63_PRED,       // Directional 63  deg = round(arctan(2/1) * 180/pi)
  SMOOTH_PRED,    // Combination of horizontal and vertical interpolation
  SMOOTH_V_PRED,  // Vertical interpolation
  SMOOTH_H_PRED,  // Horizontal interpolation
  PAETH_PRED,     // Predict from the direction of smallest gradient
  NEARESTMV,
  NEARMV,
  GLOBALMV,
  NEWMV,
  // Compound ref compound modes
  NEAREST_NEARESTMV,
  NEAR_NEARMV,
  NEAREST_NEWMV,
  NEW_NEARESTMV,
  NEAR_NEWMV,
  NEW_NEARMV,
  GLOBAL_GLOBALMV,
  NEW_NEWMV,
  MB_MODE_COUNT,
  INTRA_MODES = PAETH_PRED + 1,  // PAETH_PRED has to be the last intra mode.
  INTRA_INVALID = MB_MODE_COUNT  // For uv_mode in inter blocks
} PREDICTION_MODE;
typedef enum {
  IDENTITY = 0,      // identity transformation, 0-parameter
  TRANSLATION = 1,   // translational motion 2-parameter
  ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
  AFFINE = 3,        // affine, 6-parameter
  TRANS_TYPES,
} TransformationType;

#define LEAST_SQUARES_SAMPLES_MAX_BITS 3
#define LEAST_SQUARES_SAMPLES_MAX (1 << LEAST_SQUARES_SAMPLES_MAX_BITS)
#define SAMPLES_ARRAY_SIZE (LEAST_SQUARES_SAMPLES_MAX * 2)

static const uint8_t mi_size_wide[BLOCK_SIZES_ALL] = {
  1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16,
  16, 32, 32,  1, 4, 2, 8, 4, 16, 8, 32
};
static const uint8_t mi_size_high[BLOCK_SIZES_ALL] = {
  1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16,
  32, 16, 32,  4, 1, 8, 2, 16, 4, 32, 8
};

static const uint8_t block_size_wide[BLOCK_SIZES_ALL] = {
  4,  4,
  8,  8,
  8,  16,
  16, 16,
  32, 32,
  32, 64,
  64, 64, 128, 128, 4,
  16, 8,
  32, 16,
  64, 32, 128
};

static const uint8_t block_size_high[BLOCK_SIZES_ALL] = {
  4,  8,
  4,  8,
  16, 8,
  16, 32,
  16, 32,
  64, 32,
  64, 128, 64, 128, 16,
  4,  32,
  8,  64,
  16, 128, 32
};

static INLINE int is_global_mv_block(const MB_MODE_INFO *const mbmi,
                                     TransformationType type) {
  const PREDICTION_MODE mode = mbmi->mode;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int block_size_allowed =
      AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
  return block_size_allowed && type > TRANSLATION &&
         (mode == GLOBALMV || mode == GLOBAL_GLOBALMV);
}

typedef struct {
  TransformationType wmtype;
  int32_t wmmat[6];
  int16_t alpha, beta, gamma, delta;
} Dav1dWarpedMotionParams;

#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)
#define FRAME_BUFFERS (REF_FRAMES + 7)
typedef struct {

  unsigned int cur_frame_offset;
  unsigned int ref_frame_offset[INTER_REFS_PER_FRAME];

  MV_REF *mvs;
  ptrdiff_t mv_stride;
  int mi_rows;
  int mi_cols;
  uint8_t intra_only;
} RefCntBuffer;

#define INVALID_IDX -1  // Invalid buffer index.
typedef struct TileInfo {
  int mi_row_start, mi_row_end;
  int mi_col_start, mi_col_end;
  int tg_horz_boundary;
} TileInfo;
typedef struct macroblockd {
  TileInfo tile;
  int mi_stride;

  CUR_MODE_INFO cur_mi;
  MB_MODE_INFO *mi;
  int up_available;
  int left_available;
  /* Distance of MB away from frame edges in subpixels (1/8th pixel)  */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;
  // block dimension in the unit of mode_info.
  uint8_t n8_w, n8_h;
  uint8_t is_sec_rect;

} MACROBLOCKD;
typedef struct RefBuffer {
  int idx;  // frame buf idx
} RefBuffer;
typedef struct BufferPool {
  RefCntBuffer frame_bufs[FRAME_BUFFERS];
} BufferPool;
typedef struct AV1Common {

  // TODO(hkuang): Combine this with cur_buf in macroblockd.
  RefCntBuffer cur_frame;

  // Each Inter frame can reference INTER_REFS_PER_FRAME buffers
  RefBuffer frame_refs[INTER_REFS_PER_FRAME];

  int allow_high_precision_mv;
  int cur_frame_force_integer_mv;  // 0 the default in AOM, 1 only integer
  int mi_rows;
  int mi_cols;
  int mi_stride;

  // Whether to use previous frame's motion vectors for prediction.
  int allow_ref_frame_mvs;

  int ref_frame_sign_bias[TOTAL_REFS_PER_FRAME]; /* Two state 0, 1 */
  int frame_parallel_decode;  // frame-based threading.

  unsigned int frame_offset;

  // External BufferPool passed from outside.
  BufferPool buffer_pool;

  Dav1dWarpedMotionParams global_motion[TOTAL_REFS_PER_FRAME];
  struct {
    BLOCK_SIZE sb_size;
    int enable_order_hint;
    int order_hint_bits_minus1;
  } seq_params;
  TPL_MV_REF *tpl_mvs;
  // TODO(jingning): This can be combined with sign_bias later.
  int8_t ref_frame_side[TOTAL_REFS_PER_FRAME];

    int ref_buf_idx[INTER_REFS_PER_FRAME];
    int ref_order_hint[INTER_REFS_PER_FRAME];
} AV1_COMMON;

static INLINE void integer_mv_precision(MV *mv) {
  int mod = (mv->row % 8);
  if (mod != 0) {
    mv->row -= mod;
    if (abs(mod) > 4) {
      if (mod > 0) {
        mv->row += 8;
      } else {
        mv->row -= 8;
      }
    }
  }

  mod = (mv->col % 8);
  if (mod != 0) {
    mv->col -= mod;
    if (abs(mod) > 4) {
      if (mod > 0) {
        mv->col += 8;
      } else {
        mv->col -= 8;
      }
    }
  }
}

static INLINE int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static INLINE void clamp_mv(MV *mv, int min_col, int max_col, int min_row,
                            int max_row) {
  mv->col = clamp(mv->col, min_col, max_col);
  mv->row = clamp(mv->row, min_row, max_row);
}

static INLINE int is_intrabc_block(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[0] == INTRA_FRAME && mbmi->mv[0].as_mv.row != -0x8000;
  //return mbmi->use_intrabc;
}

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
  if (is_intrabc_block(mbmi)) return 1;
  return mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE MV_REFERENCE_FRAME comp_ref0(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST_FRAME,     // LAST_LAST2_FRAMES,
    LAST_FRAME,     // LAST_LAST3_FRAMES,
    LAST_FRAME,     // LAST_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST2_FRAME,    // LAST2_LAST3_FRAMES
    LAST2_FRAME,    // LAST2_GOLDEN_FRAMES,
    LAST3_FRAME,    // LAST3_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF2_FRAMES,
    ALTREF2_FRAME,  // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

static INLINE MV_REFERENCE_FRAME comp_ref1(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST2_FRAME,    // LAST_LAST2_FRAMES,
    LAST3_FRAME,    // LAST_LAST3_FRAMES,
    GOLDEN_FRAME,   // LAST_GOLDEN_FRAMES,
    ALTREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST3_FRAME,    // LAST2_LAST3_FRAMES
    GOLDEN_FRAME,   // LAST2_GOLDEN_FRAMES,
    GOLDEN_FRAME,   // LAST3_GOLDEN_FRAMES,
    ALTREF2_FRAME,  // BWDREF_ALTREF2_FRAMES,
    ALTREF_FRAME,   // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

#define WARPEDMODEL_PREC_BITS 16
#define GM_TRANS_ONLY_PREC_DIFF (WARPEDMODEL_PREC_BITS - 3)
#define WARPEDMODEL_ROW3HOMO_PREC_BITS 16

static INLINE int convert_to_trans_prec(int allow_hp, int coor) {
  if (allow_hp)
    return ROUND_POWER_OF_TWO_SIGNED(coor, WARPEDMODEL_PREC_BITS - 3);
  else
    return ROUND_POWER_OF_TWO_SIGNED(coor, WARPEDMODEL_PREC_BITS - 2) * 2;
}

static INLINE int block_center_x(int mi_col, BLOCK_SIZE bs) {
  const int bw = block_size_wide[bs];
  return mi_col * MI_SIZE + bw / 2 - 1;
}

static INLINE int block_center_y(int mi_row, BLOCK_SIZE bs) {
  const int bh = block_size_high[bs];
  return mi_row * MI_SIZE + bh / 2 - 1;
}

// Convert a global motion vector into a motion vector at the centre of the
// given block.
//
// The resulting motion vector will have three fractional bits of precision. If
// allow_hp is zero, the bottom bit will always be zero. If CONFIG_AMVR and
// is_integer is true, the bottom three bits will be zero (so the motion vector
// represents an integer)
static INLINE int_mv gm_get_motion_vector(const Dav1dWarpedMotionParams *gm,
                                          int allow_hp, BLOCK_SIZE bsize,
                                          int mi_col, int mi_row,
                                          int is_integer) {
  int_mv res;
  const int32_t *mat = gm->wmmat;
  int x, y, tx, ty;

  if (gm->wmtype == TRANSLATION) {
    // All global motion vectors are stored with WARPEDMODEL_PREC_BITS (16)
    // bits of fractional precision. The offset for a translation is stored in
    // entries 0 and 1. For translations, all but the top three (two if
    // cm->allow_high_precision_mv is false) fractional bits are always zero.
    //
    // After the right shifts, there are 3 fractional bits of precision. If
    // allow_hp is false, the bottom bit is always zero (so we don't need a
    // call to convert_to_trans_prec here)
    res.as_mv.row = gm->wmmat[0] >> GM_TRANS_ONLY_PREC_DIFF;
    res.as_mv.col = gm->wmmat[1] >> GM_TRANS_ONLY_PREC_DIFF;
    assert(IMPLIES(1 & (res.as_mv.row | res.as_mv.col), allow_hp));
    if (is_integer) {
      integer_mv_precision(&res.as_mv);
    }
    return res;
  }

  x = block_center_x(mi_col, bsize);
  y = block_center_y(mi_row, bsize);

  if (gm->wmtype == ROTZOOM) {
    assert(gm->wmmat[5] == gm->wmmat[2]);
    assert(gm->wmmat[4] == -gm->wmmat[3]);
  }
  if (gm->wmtype > AFFINE) {
    int xc = (int)((int64_t)mat[2] * x + (int64_t)mat[3] * y + mat[0]);
    int yc = (int)((int64_t)mat[4] * x + (int64_t)mat[5] * y + mat[1]);
    const int Z = (int)((int64_t)mat[6] * x + (int64_t)mat[7] * y +
                        (1 << WARPEDMODEL_ROW3HOMO_PREC_BITS));
    xc *= 1 << (WARPEDMODEL_ROW3HOMO_PREC_BITS - WARPEDMODEL_PREC_BITS);
    yc *= 1 << (WARPEDMODEL_ROW3HOMO_PREC_BITS - WARPEDMODEL_PREC_BITS);
    xc = (int)(xc > 0 ? ((int64_t)xc + Z / 2) / Z : ((int64_t)xc - Z / 2) / Z);
    yc = (int)(yc > 0 ? ((int64_t)yc + Z / 2) / Z : ((int64_t)yc - Z / 2) / Z);
    tx = convert_to_trans_prec(allow_hp, xc) - (x << 3);
    ty = convert_to_trans_prec(allow_hp, yc) - (y << 3);
  } else {
    const int xc =
        (mat[2] - (1 << WARPEDMODEL_PREC_BITS)) * x + mat[3] * y + mat[0];
    const int yc =
        mat[4] * x + (mat[5] - (1 << WARPEDMODEL_PREC_BITS)) * y + mat[1];
    tx = convert_to_trans_prec(allow_hp, xc);
    ty = convert_to_trans_prec(allow_hp, yc);
  }

  res.as_mv.row = ty;
  res.as_mv.col = tx;

  if (is_integer) {
    integer_mv_precision(&res.as_mv);
  }
  return res;
}

static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAREST_NEWMV ||
          mode == NEW_NEARESTMV || mode == NEAR_NEWMV || mode == NEW_NEARMV);
}

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
#ifndef AV1_COMMON_MVREF_COMMON_H_
#define AV1_COMMON_MVREF_COMMON_H_

//#include "av1/common/onyxc_int.h"
//#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MVREF_ROW_COLS 3

// Set the upper limit of the motion vector component magnitude.
// This would make a motion vector fit in 26 bits. Plus 3 bits for the
// reference frame index. A tuple of motion vector can hence be stored within
// 32 bit range for efficient load/store operations.
#define REFMVS_LIMIT ((1 << 12) - 1)

typedef struct position {
  int row;
  int col;
} POSITION;

// clamp_mv_ref
#define MV_BORDER (16 << 3)  // Allow 16 pels in 1/8th pel units

static INLINE int get_relative_dist(const AV1_COMMON *cm, int a, int b) {
  if (!cm->seq_params.enable_order_hint) return 0;

  const int bits = cm->seq_params.order_hint_bits_minus1 + 1;

  assert(bits >= 1);
  assert(a >= 0 && a < (1 << bits));
  assert(b >= 0 && b < (1 << bits));

  int diff = a - b;
  int m = 1 << (bits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

static INLINE void clamp_mv_ref(MV *mv, int bw, int bh, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - bw * 8 - MV_BORDER,
           xd->mb_to_right_edge + bw * 8 + MV_BORDER,
           xd->mb_to_top_edge - bh * 8 - MV_BORDER,
           xd->mb_to_bottom_edge + bh * 8 + MV_BORDER);
}

// This function returns either the appropriate sub block or block's mv
// on whether the block_size < 8x8 and we have check_sub_blocks set.
static INLINE int_mv get_sub_block_mv(const MB_MODE_INFO *candidate,
                                      int which_mv, int search_col) {
  (void)search_col;
  return candidate->mv[which_mv];
}

// Checks that the given mi_row, mi_col and search point
// are inside the borders of the tile.
static INLINE int is_inside(const TileInfo *const tile, int mi_col, int mi_row,
                            int mi_rows, const POSITION *mi_pos) {
  const int dependent_horz_tile_flag = 0;
  if (dependent_horz_tile_flag && !tile->tg_horz_boundary) {
    return !(mi_row + mi_pos->row < 0 ||
             mi_col + mi_pos->col < tile->mi_col_start ||
             mi_row + mi_pos->row >= mi_rows ||
             mi_col + mi_pos->col >= tile->mi_col_end);
  } else {
    return !(mi_row + mi_pos->row < tile->mi_row_start ||
             mi_col + mi_pos->col < tile->mi_col_start ||
             mi_row + mi_pos->row >= tile->mi_row_end ||
             mi_col + mi_pos->col >= tile->mi_col_end);
  }
}

static INLINE int find_valid_row_offset(const TileInfo *const tile, int mi_row,
                                        int mi_rows, int row_offset) {
  const int dependent_horz_tile_flag = 0;
  if (dependent_horz_tile_flag && !tile->tg_horz_boundary)
    return clamp(row_offset, -mi_row, mi_rows - mi_row - 1);
  else
    return clamp(row_offset, tile->mi_row_start - mi_row,
                 tile->mi_row_end - mi_row - 1);
}

static INLINE int find_valid_col_offset(const TileInfo *const tile, int mi_col,
                                        int col_offset) {
  return clamp(col_offset, tile->mi_col_start - mi_col,
               tile->mi_col_end - mi_col - 1);
}

static INLINE void lower_mv_precision(MV *mv, int allow_hp,
                                      int is_integer) {
  if (is_integer) {
    integer_mv_precision(mv);
  } else {
    if (!allow_hp) {
      if (mv->row & 1) mv->row += (mv->row > 0 ? -1 : 1);
      if (mv->col & 1) mv->col += (mv->col > 0 ? -1 : 1);
    }
  }
}

static INLINE int8_t get_uni_comp_ref_idx(const MV_REFERENCE_FRAME *const rf) {
  // Single ref pred
  if (rf[1] <= INTRA_FRAME) return -1;

  // Bi-directional comp ref pred
  if ((rf[0] < BWDREF_FRAME) && (rf[1] >= BWDREF_FRAME)) return -1;

  for (int8_t ref_idx = 0; ref_idx < TOTAL_UNIDIR_COMP_REFS; ++ref_idx) {
    if (rf[0] == comp_ref0(ref_idx) && rf[1] == comp_ref1(ref_idx))
      return ref_idx;
  }
  return -1;
}

static INLINE int8_t av1_ref_frame_type(const MV_REFERENCE_FRAME *const rf) {
  if (rf[1] > INTRA_FRAME) {
    const int8_t uni_comp_ref_idx = get_uni_comp_ref_idx(rf);
    if (uni_comp_ref_idx >= 0) {
      assert((REF_FRAMES + FWD_REFS * BWD_REFS + uni_comp_ref_idx) <
             MODE_CTX_REF_FRAMES);
      return REF_FRAMES + FWD_REFS * BWD_REFS + uni_comp_ref_idx;
    } else {
      return REF_FRAMES + FWD_RF_OFFSET(rf[0]) +
             BWD_RF_OFFSET(rf[1]) * FWD_REFS;
    }
  }

  return rf[0];
}

// clang-format off
static MV_REFERENCE_FRAME ref_frame_map[TOTAL_COMP_REFS][2] = {
  { LAST_FRAME, BWDREF_FRAME },  { LAST2_FRAME, BWDREF_FRAME },
  { LAST3_FRAME, BWDREF_FRAME }, { GOLDEN_FRAME, BWDREF_FRAME },

  { LAST_FRAME, ALTREF2_FRAME },  { LAST2_FRAME, ALTREF2_FRAME },
  { LAST3_FRAME, ALTREF2_FRAME }, { GOLDEN_FRAME, ALTREF2_FRAME },

  { LAST_FRAME, ALTREF_FRAME },  { LAST2_FRAME, ALTREF_FRAME },
  { LAST3_FRAME, ALTREF_FRAME }, { GOLDEN_FRAME, ALTREF_FRAME },

  { LAST_FRAME, LAST2_FRAME }, { LAST_FRAME, LAST3_FRAME },
  { LAST_FRAME, GOLDEN_FRAME }, { BWDREF_FRAME, ALTREF_FRAME },

  // NOTE: Following reference frame pairs are not supported to be explicitly
  //       signalled, but they are possibly chosen by the use of skip_mode,
  //       which may use the most recent one-sided reference frame pair.
  { LAST2_FRAME, LAST3_FRAME }, { LAST2_FRAME, GOLDEN_FRAME },
  { LAST3_FRAME, GOLDEN_FRAME }, {BWDREF_FRAME, ALTREF2_FRAME},
  { ALTREF2_FRAME, ALTREF_FRAME }
};
// clang-format on

static INLINE void av1_set_ref_frame(MV_REFERENCE_FRAME *rf,
                                     int8_t ref_frame_type) {
  if (ref_frame_type >= REF_FRAMES) {
    rf[0] = ref_frame_map[ref_frame_type - REF_FRAMES][0];
    rf[1] = ref_frame_map[ref_frame_type - REF_FRAMES][1];
  } else {
    rf[0] = ref_frame_type;
    rf[1] = NONE_FRAME;
    assert(ref_frame_type > NONE_FRAME);
  }
}

static uint16_t compound_mode_ctx_map[3][COMP_NEWMV_CTXS] = {
  { 0, 1, 1, 1, 1 },
  { 1, 2, 3, 4, 4 },
  { 4, 4, 5, 6, 7 },
};

static INLINE int16_t av1_mode_context_analyzer(
    const int16_t *const mode_context, const MV_REFERENCE_FRAME *const rf) {
  const int8_t ref_frame = av1_ref_frame_type(rf);

  if (rf[1] <= INTRA_FRAME) return mode_context[ref_frame];

  const int16_t newmv_ctx = mode_context[ref_frame] & NEWMV_CTX_MASK;
  const int16_t refmv_ctx =
      (mode_context[ref_frame] >> REFMV_OFFSET) & REFMV_CTX_MASK;

  const int16_t comp_ctx = compound_mode_ctx_map[refmv_ctx >> 1][AOMMIN(
      newmv_ctx, COMP_NEWMV_CTXS - 1)];
  return comp_ctx;
}

#define INTRABC_DELAY_PIXELS 256  //  Delay of 256 pixels
#define INTRABC_DELAY_SB64 (INTRABC_DELAY_PIXELS / 64)
#define USE_WAVE_FRONT 1  // Use only top left area of frame for reference.

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_MVREF_COMMON_H_

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

#include <stdlib.h>

//#include "av1/common/mvref_common.h"
//#include "av1/common/warped_motion.h"

// Although we assign 32 bit integers, all the values are strictly under 14
// bits.
static int div_mult[32] = { 0,    16384, 8192, 5461, 4096, 3276, 2730, 2340,
                            2048, 1820,  1638, 1489, 1365, 1260, 1170, 1092,
                            1024, 963,   910,  862,  819,  780,  744,  712,
                            682,  655,   630,  606,  585,  564,  546,  528 };

// TODO(jingning): Consider the use of lookup table for (num / den)
// altogether.
static void get_mv_projection(MV *output, MV ref, int num, int den) {
  den = AOMMIN(den, MAX_FRAME_DISTANCE);
  num = num > 0 ? AOMMIN(num, MAX_FRAME_DISTANCE)
                : AOMMAX(num, -MAX_FRAME_DISTANCE);
  int mv_row = ROUND_POWER_OF_TWO_SIGNED(ref.row * num * div_mult[den], 14);
  int mv_col = ROUND_POWER_OF_TWO_SIGNED(ref.col * num * div_mult[den], 14);
  const int clamp_max = MV_UPP - 1;
  const int clamp_min = MV_LOW + 1;
  output->row = (int16_t)clamp(mv_row, clamp_min, clamp_max);
  output->col = (int16_t)clamp(mv_col, clamp_min, clamp_max);
}

static void add_ref_mv_candidate(
    const MB_MODE_INFO *const candidate, const MV_REFERENCE_FRAME rf[2],
    uint8_t *refmv_count, uint8_t *ref_match_count, uint8_t *newmv_count,
    CANDIDATE_MV *ref_mv_stack, int_mv *gm_mv_candidates,
    const Dav1dWarpedMotionParams *gm_params, int col, int weight) {
  if (!is_inter_block(candidate)) return;  // for intrabc
  int index = 0, ref;
  assert(weight % 2 == 0);

  if (rf[1] == NONE_FRAME) {
    // single reference frame
    for (ref = 0; ref < 2; ++ref) {
      if (candidate->ref_frame[ref] == rf[0]) {
        int_mv this_refmv;
        if (is_global_mv_block(candidate, gm_params[rf[0]].wmtype))
          this_refmv = gm_mv_candidates[0];
        else
          this_refmv = get_sub_block_mv(candidate, ref, col);

        for (index = 0; index < *refmv_count; ++index)
          if (ref_mv_stack[index].this_mv.as_int == this_refmv.as_int) break;

        if (index < *refmv_count) ref_mv_stack[index].weight += weight;

        // Add a new item to the list.
        if (index == *refmv_count && *refmv_count < MAX_REF_MV_STACK_SIZE) {
          ref_mv_stack[index].this_mv = this_refmv;
          ref_mv_stack[index].weight = weight;
          ++(*refmv_count);
        }
        if (have_newmv_in_inter_mode(candidate->mode)) ++*newmv_count;
        ++*ref_match_count;
      }
    }
  } else {
    // compound reference frame
    if (candidate->ref_frame[0] == rf[0] && candidate->ref_frame[1] == rf[1]) {
      int_mv this_refmv[2];

      for (ref = 0; ref < 2; ++ref) {
        if (is_global_mv_block(candidate, gm_params[rf[ref]].wmtype))
          this_refmv[ref] = gm_mv_candidates[ref];
        else
          this_refmv[ref] = get_sub_block_mv(candidate, ref, col);
      }

      for (index = 0; index < *refmv_count; ++index)
        if ((ref_mv_stack[index].this_mv.as_int == this_refmv[0].as_int) &&
            (ref_mv_stack[index].comp_mv.as_int == this_refmv[1].as_int))
          break;

      if (index < *refmv_count) ref_mv_stack[index].weight += weight;

      // Add a new item to the list.
      if (index == *refmv_count && *refmv_count < MAX_REF_MV_STACK_SIZE) {
        ref_mv_stack[index].this_mv = this_refmv[0];
        ref_mv_stack[index].comp_mv = this_refmv[1];
        ref_mv_stack[index].weight = weight;
        ++(*refmv_count);
      }
      if (have_newmv_in_inter_mode(candidate->mode)) ++*newmv_count;
      ++*ref_match_count;
    }
  }
}

static void scan_row_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                          int mi_row, int mi_col,
                          const MV_REFERENCE_FRAME rf[2], int row_offset,
                          CANDIDATE_MV *ref_mv_stack, uint8_t *refmv_count,
                          uint8_t *ref_match_count, uint8_t *newmv_count,
                          int_mv *gm_mv_candidates, int max_row_offset,
                          int *processed_rows) {
  int end_mi = AOMMIN(xd->n8_w, cm->mi_cols - mi_col);
  end_mi = AOMMIN(end_mi, mi_size_wide[BLOCK_64X64]);
  const int n8_w_8 = mi_size_wide[BLOCK_8X8];
  const int n8_w_16 = mi_size_wide[BLOCK_16X16];
  int i;
  int col_offset = 0;
  const int shift = 0;
  // TODO(jingning): Revisit this part after cb4x4 is stable.
  if (abs(row_offset) > 1) {
    col_offset = 1;
    if ((mi_col & 0x01) && xd->n8_w < n8_w_8) --col_offset;
  }
  const int use_step_16 = (xd->n8_w >= 16);
  MB_MODE_INFO *const candidate_mi0 = xd->mi + row_offset * xd->mi_stride;
  (void)mi_row;

  for (i = 0; i < end_mi;) {
    const MB_MODE_INFO *const candidate = &candidate_mi0[col_offset + i];
    const int candidate_bsize = candidate->sb_type;
    const int n8_w = mi_size_wide[candidate_bsize];
    int len = AOMMIN(xd->n8_w, n8_w);
    if (use_step_16)
      len = AOMMAX(n8_w_16, len);
    else if (abs(row_offset) > 1)
      len = AOMMAX(len, n8_w_8);

    int weight = 2;
    if (xd->n8_w >= n8_w_8 && xd->n8_w <= n8_w) {
      int inc = AOMMIN(-max_row_offset + row_offset + 1,
                       mi_size_high[candidate_bsize]);
      // Obtain range used in weight calculation.
      weight = AOMMAX(weight, (inc << shift));
      // Update processed rows.
      *processed_rows = inc - row_offset - 1;
    }

    add_ref_mv_candidate(candidate, rf, refmv_count, ref_match_count,
                         newmv_count, ref_mv_stack, gm_mv_candidates,
                         cm->global_motion, col_offset + i, len * weight);

    i += len;
  }
}

static void scan_col_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                          int mi_row, int mi_col,
                          const MV_REFERENCE_FRAME rf[2], int col_offset,
                          CANDIDATE_MV *ref_mv_stack, uint8_t *refmv_count,
                          uint8_t *ref_match_count, uint8_t *newmv_count,
                          int_mv *gm_mv_candidates, int max_col_offset,
                          int *processed_cols) {
  int end_mi = AOMMIN(xd->n8_h, cm->mi_rows - mi_row);
  end_mi = AOMMIN(end_mi, mi_size_high[BLOCK_64X64]);
  const int n8_h_8 = mi_size_high[BLOCK_8X8];
  const int n8_h_16 = mi_size_high[BLOCK_16X16];
  int i;
  int row_offset = 0;
  const int shift = 0;
  if (abs(col_offset) > 1) {
    row_offset = 1;
    if ((mi_row & 0x01) && xd->n8_h < n8_h_8) --row_offset;
  }
  const int use_step_16 = (xd->n8_h >= 16);
  (void)mi_col;

  for (i = 0; i < end_mi;) {
    const MB_MODE_INFO *const candidate =
        &xd->mi[(row_offset + i) * xd->mi_stride + col_offset];
    const int candidate_bsize = candidate->sb_type;
    const int n8_h = mi_size_high[candidate_bsize];
    int len = AOMMIN(xd->n8_h, n8_h);
    if (use_step_16)
      len = AOMMAX(n8_h_16, len);
    else if (abs(col_offset) > 1)
      len = AOMMAX(len, n8_h_8);

    int weight = 2;
    if (xd->n8_h >= n8_h_8 && xd->n8_h <= n8_h) {
      int inc = AOMMIN(-max_col_offset + col_offset + 1,
                       mi_size_wide[candidate_bsize]);
      // Obtain range used in weight calculation.
      weight = AOMMAX(weight, (inc << shift));
      // Update processed cols.
      *processed_cols = inc - col_offset - 1;
    }

    add_ref_mv_candidate(candidate, rf, refmv_count, ref_match_count,
                         newmv_count, ref_mv_stack, gm_mv_candidates,
                         cm->global_motion, col_offset, len * weight);

    i += len;
  }
}

static void scan_blk_mbmi(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                          const int mi_row, const int mi_col,
                          const MV_REFERENCE_FRAME rf[2], int row_offset,
                          int col_offset, CANDIDATE_MV *ref_mv_stack,
                          uint8_t *ref_match_count, uint8_t *newmv_count,
                          int_mv *gm_mv_candidates,
                          uint8_t refmv_count[MODE_CTX_REF_FRAMES]) {
  const TileInfo *const tile = &xd->tile;
  POSITION mi_pos;

  mi_pos.row = row_offset;
  mi_pos.col = col_offset;

  if (is_inside(tile, mi_col, mi_row, cm->mi_rows, &mi_pos)) {
    const MB_MODE_INFO *const candidate =
        &xd->mi[mi_pos.row * xd->mi_stride + mi_pos.col];
    const int len = mi_size_wide[BLOCK_8X8];

    add_ref_mv_candidate(candidate, rf, refmv_count, ref_match_count,
                         newmv_count, ref_mv_stack, gm_mv_candidates,
                         cm->global_motion, mi_pos.col, 2 * len);
  }  // Analyze a single 8x8 block motion information.
}

static int has_top_right(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                         int mi_row, int mi_col, int bs) {
  const int sb_mi_size = mi_size_wide[cm->seq_params.sb_size];
  const int mask_row = mi_row & (sb_mi_size - 1);
  const int mask_col = mi_col & (sb_mi_size - 1);

  if (bs > mi_size_wide[BLOCK_64X64]) return 0;

  // In a split partition all apart from the bottom right has a top right
  int has_tr = !((mask_row & bs) && (mask_col & bs));

  // bs > 0 and bs is a power of 2
  assert(bs > 0 && !(bs & (bs - 1)));

  // For each 4x4 group of blocks, when the bottom right is decoded the blocks
  // to the right have not been decoded therefore the bottom right does
  // not have a top right
  while (bs < sb_mi_size) {
    if (mask_col & bs) {
      if ((mask_col & (2 * bs)) && (mask_row & (2 * bs))) {
        has_tr = 0;
        break;
      }
    } else {
      break;
    }
    bs <<= 1;
  }

  // The left hand of two vertical rectangles always has a top right (as the
  // block above will have been decoded)
  if (xd->n8_w < xd->n8_h)
    if (!xd->is_sec_rect) has_tr = 1;

  // The bottom of two horizontal rectangles never has a top right (as the block
  // to the right won't have been decoded)
  if (xd->n8_w > xd->n8_h)
    if (xd->is_sec_rect) has_tr = 0;

  // The bottom left square of a Vertical A (in the old format) does
  // not have a top right as it is decoded before the right hand
  // rectangle of the partition
  if (xd->cur_mi.partition == PARTITION_VERT_A) {
    if (xd->n8_w == xd->n8_h)
      if (mask_row & bs) has_tr = 0;
  }

  return has_tr;
}

static int check_sb_border(const int mi_row, const int mi_col,
                           const int row_offset, const int col_offset) {
  const int sb_mi_size = mi_size_wide[BLOCK_64X64];
  const int row = mi_row & (sb_mi_size - 1);
  const int col = mi_col & (sb_mi_size - 1);

  if (row + row_offset < 0 || row + row_offset >= sb_mi_size ||
      col + col_offset < 0 || col + col_offset >= sb_mi_size)
    return 0;

  return 1;
}

static int add_tpl_ref_mv(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                          int mi_row, int mi_col, MV_REFERENCE_FRAME ref_frame,
                          int blk_row, int blk_col, int_mv *gm_mv_candidates,
                          uint8_t refmv_count[MODE_CTX_REF_FRAMES],
                          CANDIDATE_MV ref_mv_stacks[][MAX_REF_MV_STACK_SIZE],
                          int16_t *mode_context) {
  POSITION mi_pos;
  int idx;
  const int weight_unit = 1;  // mi_size_wide[BLOCK_8X8];

  mi_pos.row = (mi_row & 0x01) ? blk_row : blk_row + 1;
  mi_pos.col = (mi_col & 0x01) ? blk_col : blk_col + 1;

  if (!is_inside(&xd->tile, mi_col, mi_row, cm->mi_rows, &mi_pos)) return 0;

  const TPL_MV_REF *prev_frame_mvs =
      cm->tpl_mvs + ((mi_row + mi_pos.row) >> 1) * (cm->mi_stride >> 1) +
      ((mi_col + mi_pos.col) >> 1);

  MV_REFERENCE_FRAME rf[2];
  av1_set_ref_frame(rf, ref_frame);

  if (rf[1] == NONE_FRAME) {
    int cur_frame_index = cm->cur_frame.cur_frame_offset;
    int buf_idx_0 = cm->frame_refs[FWD_RF_OFFSET(rf[0])].idx;
    int frame0_index = cm->buffer_pool.frame_bufs[buf_idx_0].cur_frame_offset;
    int cur_offset_0 = get_relative_dist(cm, cur_frame_index, frame0_index);
    CANDIDATE_MV *ref_mv_stack = ref_mv_stacks[rf[0]];

    if (prev_frame_mvs->mfmv0.as_int != INVALID_MV) {
      int_mv this_refmv;

      get_mv_projection(&this_refmv.as_mv, prev_frame_mvs->mfmv0.as_mv,
                        cur_offset_0, prev_frame_mvs->ref_frame_offset);
      lower_mv_precision(&this_refmv.as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);

      if (blk_row == 0 && blk_col == 0)
        if (abs(this_refmv.as_mv.row - gm_mv_candidates[0].as_mv.row) >= 16 ||
            abs(this_refmv.as_mv.col - gm_mv_candidates[0].as_mv.col) >= 16)
          mode_context[ref_frame] |= (1 << GLOBALMV_OFFSET);

      for (idx = 0; idx < refmv_count[rf[0]]; ++idx)
        if (this_refmv.as_int == ref_mv_stack[idx].this_mv.as_int) break;

      if (idx < refmv_count[rf[0]]) ref_mv_stack[idx].weight += 2 * weight_unit;

      if (idx == refmv_count[rf[0]] &&
          refmv_count[rf[0]] < MAX_REF_MV_STACK_SIZE) {
        ref_mv_stack[idx].this_mv.as_int = this_refmv.as_int;
        ref_mv_stack[idx].weight = 2 * weight_unit;
        ++(refmv_count[rf[0]]);
      }

      return 1;
    }
  } else {
    // Process compound inter mode
    int cur_frame_index = cm->cur_frame.cur_frame_offset;
    int buf_idx_0 = cm->frame_refs[FWD_RF_OFFSET(rf[0])].idx;
    int frame0_index = cm->buffer_pool.frame_bufs[buf_idx_0].cur_frame_offset;

    int cur_offset_0 = get_relative_dist(cm, cur_frame_index, frame0_index);
    int buf_idx_1 = cm->frame_refs[FWD_RF_OFFSET(rf[1])].idx;
    int frame1_index = cm->buffer_pool.frame_bufs[buf_idx_1].cur_frame_offset;
    int cur_offset_1 = get_relative_dist(cm, cur_frame_index, frame1_index);
    CANDIDATE_MV *ref_mv_stack = ref_mv_stacks[ref_frame];

    if (prev_frame_mvs->mfmv0.as_int != INVALID_MV) {
      int_mv this_refmv;
      int_mv comp_refmv;
      get_mv_projection(&this_refmv.as_mv, prev_frame_mvs->mfmv0.as_mv,
                        cur_offset_0, prev_frame_mvs->ref_frame_offset);
      get_mv_projection(&comp_refmv.as_mv, prev_frame_mvs->mfmv0.as_mv,
                        cur_offset_1, prev_frame_mvs->ref_frame_offset);

      lower_mv_precision(&this_refmv.as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);
      lower_mv_precision(&comp_refmv.as_mv, cm->allow_high_precision_mv,
                         cm->cur_frame_force_integer_mv);

      if (blk_row == 0 && blk_col == 0)
        if (abs(this_refmv.as_mv.row - gm_mv_candidates[0].as_mv.row) >= 16 ||
            abs(this_refmv.as_mv.col - gm_mv_candidates[0].as_mv.col) >= 16 ||
            abs(comp_refmv.as_mv.row - gm_mv_candidates[1].as_mv.row) >= 16 ||
            abs(comp_refmv.as_mv.col - gm_mv_candidates[1].as_mv.col) >= 16)
          mode_context[ref_frame] |= (1 << GLOBALMV_OFFSET);

      for (idx = 0; idx < refmv_count[ref_frame]; ++idx)
        if (this_refmv.as_int == ref_mv_stack[idx].this_mv.as_int &&
            comp_refmv.as_int == ref_mv_stack[idx].comp_mv.as_int)
          break;

      if (idx < refmv_count[ref_frame])
        ref_mv_stack[idx].weight += 2 * weight_unit;

      if (idx == refmv_count[ref_frame] &&
          refmv_count[ref_frame] < MAX_REF_MV_STACK_SIZE) {
        ref_mv_stack[idx].this_mv.as_int = this_refmv.as_int;
        ref_mv_stack[idx].comp_mv.as_int = comp_refmv.as_int;
        ref_mv_stack[idx].weight = 2 * weight_unit;
        ++(refmv_count[ref_frame]);
      }
      return 1;
    }
  }
  return 0;
}

static void setup_ref_mv_list(
    const AV1_COMMON *cm, const MACROBLOCKD *xd, MV_REFERENCE_FRAME ref_frame,
    uint8_t refmv_count[MODE_CTX_REF_FRAMES],
    CANDIDATE_MV ref_mv_stack[][MAX_REF_MV_STACK_SIZE],
    int_mv mv_ref_list[][MAX_MV_REF_CANDIDATES], int_mv *gm_mv_candidates,
    int mi_row, int mi_col, int16_t *mode_context) {
  const int bs = AOMMAX(xd->n8_w, xd->n8_h);
  const int has_tr = has_top_right(cm, xd, mi_row, mi_col, bs);
  MV_REFERENCE_FRAME rf[2];

  const TileInfo *const tile = &xd->tile;
  int max_row_offset = 0, max_col_offset = 0;
  const int row_adj = (xd->n8_h < mi_size_high[BLOCK_8X8]) && (mi_row & 0x01);
  const int col_adj = (xd->n8_w < mi_size_wide[BLOCK_8X8]) && (mi_col & 0x01);
  int processed_rows = 0;
  int processed_cols = 0;

  av1_set_ref_frame(rf, ref_frame);
  mode_context[ref_frame] = 0;
  refmv_count[ref_frame] = 0;

  // Find valid maximum row/col offset.
  if (xd->up_available) {
    max_row_offset = -(MVREF_ROW_COLS << 1) + row_adj;

    if (xd->n8_h < mi_size_high[BLOCK_8X8])
      max_row_offset = -(2 << 1) + row_adj;

    max_row_offset =
        find_valid_row_offset(tile, mi_row, cm->mi_rows, max_row_offset);
  }

  if (xd->left_available) {
    max_col_offset = -(MVREF_ROW_COLS << 1) + col_adj;

    if (xd->n8_w < mi_size_wide[BLOCK_8X8])
      max_col_offset = -(2 << 1) + col_adj;

    max_col_offset = find_valid_col_offset(tile, mi_col, max_col_offset);
  }

  uint8_t col_match_count = 0;
  uint8_t row_match_count = 0;
  uint8_t newmv_count = 0;

  // Scan the first above row mode info. row_offset = -1;
  if (abs(max_row_offset) >= 1)
    scan_row_mbmi(cm, xd, mi_row, mi_col, rf, -1, ref_mv_stack[ref_frame],
                  &refmv_count[ref_frame], &row_match_count, &newmv_count,
                  gm_mv_candidates, max_row_offset, &processed_rows);
  // Scan the first left column mode info. col_offset = -1;
  if (abs(max_col_offset) >= 1)
    scan_col_mbmi(cm, xd, mi_row, mi_col, rf, -1, ref_mv_stack[ref_frame],
                  &refmv_count[ref_frame], &col_match_count, &newmv_count,
                  gm_mv_candidates, max_col_offset, &processed_cols);
  // Check top-right boundary
  if (has_tr)
    scan_blk_mbmi(cm, xd, mi_row, mi_col, rf, -1, xd->n8_w,
                  ref_mv_stack[ref_frame], &row_match_count, &newmv_count,
                  gm_mv_candidates, &refmv_count[ref_frame]);

  uint8_t nearest_match = (row_match_count > 0) + (col_match_count > 0);
  uint8_t nearest_refmv_count = refmv_count[ref_frame];

  // TODO(yunqing): for comp_search, do it for all 3 cases.
  for (int idx = 0; idx < nearest_refmv_count; ++idx)
    ref_mv_stack[ref_frame][idx].weight += REF_CAT_LEVEL;

  if (cm->allow_ref_frame_mvs) {
    int is_available = 0;
    const int voffset = AOMMAX(mi_size_high[BLOCK_8X8], xd->n8_h);
    const int hoffset = AOMMAX(mi_size_wide[BLOCK_8X8], xd->n8_w);
    const int blk_row_end = AOMMIN(xd->n8_h, mi_size_high[BLOCK_64X64]);
    const int blk_col_end = AOMMIN(xd->n8_w, mi_size_wide[BLOCK_64X64]);

    const int tpl_sample_pos[3][2] = {
      { voffset, -2 },
      { voffset, hoffset },
      { voffset - 2, hoffset },
    };
    const int allow_extension = (xd->n8_h >= mi_size_high[BLOCK_8X8]) &&
                                (xd->n8_h < mi_size_high[BLOCK_64X64]) &&
                                (xd->n8_w >= mi_size_wide[BLOCK_8X8]) &&
                                (xd->n8_w < mi_size_wide[BLOCK_64X64]);

    int step_h = (xd->n8_h >= mi_size_high[BLOCK_64X64])
                     ? mi_size_high[BLOCK_16X16]
                     : mi_size_high[BLOCK_8X8];
    int step_w = (xd->n8_w >= mi_size_wide[BLOCK_64X64])
                     ? mi_size_wide[BLOCK_16X16]
                     : mi_size_wide[BLOCK_8X8];

    for (int blk_row = 0; blk_row < blk_row_end; blk_row += step_h) {
      for (int blk_col = 0; blk_col < blk_col_end; blk_col += step_w) {
        int ret = add_tpl_ref_mv(cm, xd, mi_row, mi_col, ref_frame, blk_row,
                                 blk_col, gm_mv_candidates, refmv_count,
                                 ref_mv_stack, mode_context);
        if (blk_row == 0 && blk_col == 0) is_available = ret;
      }
    }

    if (is_available == 0) mode_context[ref_frame] |= (1 << GLOBALMV_OFFSET);

    for (int i = 0; i < 3 && allow_extension; ++i) {
      const int blk_row = tpl_sample_pos[i][0];
      const int blk_col = tpl_sample_pos[i][1];

      if (!check_sb_border(mi_row, mi_col, blk_row, blk_col)) continue;
      add_tpl_ref_mv(cm, xd, mi_row, mi_col, ref_frame, blk_row, blk_col,
                     gm_mv_candidates, refmv_count, ref_mv_stack, mode_context);
    }
  }

  uint8_t dummy_newmv_count = 0;

  // Scan the second outer area.
  scan_blk_mbmi(cm, xd, mi_row, mi_col, rf, -1, -1, ref_mv_stack[ref_frame],
                &row_match_count, &dummy_newmv_count, gm_mv_candidates,
                &refmv_count[ref_frame]);

  for (int idx = 2; idx <= MVREF_ROW_COLS; ++idx) {
    const int row_offset = -(idx << 1) + 1 + row_adj;
    const int col_offset = -(idx << 1) + 1 + col_adj;

    if (abs(row_offset) <= abs(max_row_offset) &&
        abs(row_offset) > processed_rows)
      scan_row_mbmi(cm, xd, mi_row, mi_col, rf, row_offset,
                    ref_mv_stack[ref_frame], &refmv_count[ref_frame],
                    &row_match_count, &dummy_newmv_count, gm_mv_candidates,
                    max_row_offset, &processed_rows);

    if (abs(col_offset) <= abs(max_col_offset) &&
        abs(col_offset) > processed_cols)
      scan_col_mbmi(cm, xd, mi_row, mi_col, rf, col_offset,
                    ref_mv_stack[ref_frame], &refmv_count[ref_frame],
                    &col_match_count, &dummy_newmv_count, gm_mv_candidates,
                    max_col_offset, &processed_cols);
  }

  uint8_t ref_match_count = (row_match_count > 0) + (col_match_count > 0);

  switch (nearest_match) {
    case 0:
      mode_context[ref_frame] |= 0;
      if (ref_match_count >= 1) mode_context[ref_frame] |= 1;
      if (ref_match_count == 1)
        mode_context[ref_frame] |= (1 << REFMV_OFFSET);
      else if (ref_match_count >= 2)
        mode_context[ref_frame] |= (2 << REFMV_OFFSET);
      break;
    case 1:
      mode_context[ref_frame] |= (newmv_count > 0) ? 2 : 3;
      if (ref_match_count == 1)
        mode_context[ref_frame] |= (3 << REFMV_OFFSET);
      else if (ref_match_count >= 2)
        mode_context[ref_frame] |= (4 << REFMV_OFFSET);
      break;
    case 2:
    default:
      if (newmv_count >= 1)
        mode_context[ref_frame] |= 4;
      else
        mode_context[ref_frame] |= 5;

      mode_context[ref_frame] |= (5 << REFMV_OFFSET);
      break;
  }

  // Rank the likelihood and assign nearest and near mvs.
  int len = nearest_refmv_count;
  while (len > 0) {
    int nr_len = 0;
    for (int idx = 1; idx < len; ++idx) {
      if (ref_mv_stack[ref_frame][idx - 1].weight <
          ref_mv_stack[ref_frame][idx].weight) {
        CANDIDATE_MV tmp_mv = ref_mv_stack[ref_frame][idx - 1];
        ref_mv_stack[ref_frame][idx - 1] = ref_mv_stack[ref_frame][idx];
        ref_mv_stack[ref_frame][idx] = tmp_mv;
        nr_len = idx;
      }
    }
    len = nr_len;
  }

  len = refmv_count[ref_frame];
  while (len > nearest_refmv_count) {
    int nr_len = nearest_refmv_count;
    for (int idx = nearest_refmv_count + 1; idx < len; ++idx) {
      if (ref_mv_stack[ref_frame][idx - 1].weight <
          ref_mv_stack[ref_frame][idx].weight) {
        CANDIDATE_MV tmp_mv = ref_mv_stack[ref_frame][idx - 1];
        ref_mv_stack[ref_frame][idx - 1] = ref_mv_stack[ref_frame][idx];
        ref_mv_stack[ref_frame][idx] = tmp_mv;
        nr_len = idx;
      }
    }
    len = nr_len;
  }

  if (rf[1] > NONE_FRAME) {
    // TODO(jingning, yunqing): Refactor and consolidate the compound and
    // single reference frame modes. Reduce unnecessary redundancy.
    if (refmv_count[ref_frame] < MAX_MV_REF_CANDIDATES) {
      int_mv ref_id[2][2], ref_diff[2][2];
      int ref_id_count[2] = { 0 }, ref_diff_count[2] = { 0 };

      int mi_width = AOMMIN(mi_size_wide[BLOCK_64X64], xd->n8_w);
      mi_width = AOMMIN(mi_width, cm->mi_cols - mi_col);
      int mi_height = AOMMIN(mi_size_high[BLOCK_64X64], xd->n8_h);
      mi_height = AOMMIN(mi_height, cm->mi_rows - mi_row);
      int mi_size = AOMMIN(mi_width, mi_height);

      for (int idx = 0; abs(max_row_offset) >= 1 && idx < mi_size;) {
        const MB_MODE_INFO *const candidate = &xd->mi[-xd->mi_stride + idx];
        const int candidate_bsize = candidate->sb_type;

        for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
          MV_REFERENCE_FRAME can_rf = candidate->ref_frame[rf_idx];

          for (int cmp_idx = 0; cmp_idx < 2; ++cmp_idx) {
            if (can_rf == rf[cmp_idx] && ref_id_count[cmp_idx] < 2) {
              ref_id[cmp_idx][ref_id_count[cmp_idx]] = candidate->mv[rf_idx];
              ++ref_id_count[cmp_idx];
            } else if (can_rf > INTRA_FRAME && ref_diff_count[cmp_idx] < 2) {
              int_mv this_mv = candidate->mv[rf_idx];
              if (cm->ref_frame_sign_bias[can_rf] !=
                  cm->ref_frame_sign_bias[rf[cmp_idx]]) {
                this_mv.as_mv.row = -this_mv.as_mv.row;
                this_mv.as_mv.col = -this_mv.as_mv.col;
              }
              ref_diff[cmp_idx][ref_diff_count[cmp_idx]] = this_mv;
              ++ref_diff_count[cmp_idx];
            }
          }
        }
        idx += mi_size_wide[candidate_bsize];
      }

      for (int idx = 0; abs(max_col_offset) >= 1 && idx < mi_size;) {
        const MB_MODE_INFO *const candidate = &xd->mi[idx * xd->mi_stride - 1];
        const int candidate_bsize = candidate->sb_type;

        for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
          MV_REFERENCE_FRAME can_rf = candidate->ref_frame[rf_idx];

          for (int cmp_idx = 0; cmp_idx < 2; ++cmp_idx) {
            if (can_rf == rf[cmp_idx] && ref_id_count[cmp_idx] < 2) {
              ref_id[cmp_idx][ref_id_count[cmp_idx]] = candidate->mv[rf_idx];
              ++ref_id_count[cmp_idx];
            } else if (can_rf > INTRA_FRAME && ref_diff_count[cmp_idx] < 2) {
              int_mv this_mv = candidate->mv[rf_idx];
              if (cm->ref_frame_sign_bias[can_rf] !=
                  cm->ref_frame_sign_bias[rf[cmp_idx]]) {
                this_mv.as_mv.row = -this_mv.as_mv.row;
                this_mv.as_mv.col = -this_mv.as_mv.col;
              }
              ref_diff[cmp_idx][ref_diff_count[cmp_idx]] = this_mv;
              ++ref_diff_count[cmp_idx];
            }
          }
        }
        idx += mi_size_high[candidate_bsize];
      }

      // Build up the compound mv predictor
      int_mv comp_list[3][2];

      for (int idx = 0; idx < 2; ++idx) {
        int comp_idx = 0;
        for (int list_idx = 0; list_idx < ref_id_count[idx] && comp_idx < 2;
             ++list_idx, ++comp_idx)
          comp_list[comp_idx][idx] = ref_id[idx][list_idx];
        for (int list_idx = 0; list_idx < ref_diff_count[idx] && comp_idx < 2;
             ++list_idx, ++comp_idx)
          comp_list[comp_idx][idx] = ref_diff[idx][list_idx];
        for (; comp_idx < 3; ++comp_idx)
          comp_list[comp_idx][idx] = gm_mv_candidates[idx];
      }

      if (refmv_count[ref_frame]) {
        assert(refmv_count[ref_frame] == 1);
        if (comp_list[0][0].as_int ==
                ref_mv_stack[ref_frame][0].this_mv.as_int &&
            comp_list[0][1].as_int ==
                ref_mv_stack[ref_frame][0].comp_mv.as_int) {
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].this_mv =
              comp_list[1][0];
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].comp_mv =
              comp_list[1][1];
        } else {
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].this_mv =
              comp_list[0][0];
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].comp_mv =
              comp_list[0][1];
        }
        ref_mv_stack[ref_frame][refmv_count[ref_frame]].weight = 2;
        ++refmv_count[ref_frame];
      } else {
        for (int idx = 0; idx < MAX_MV_REF_CANDIDATES; ++idx) {
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].this_mv =
              comp_list[idx][0];
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].comp_mv =
              comp_list[idx][1];
          ref_mv_stack[ref_frame][refmv_count[ref_frame]].weight = 2;
          ++refmv_count[ref_frame];
        }
      }
    }

    assert(refmv_count[ref_frame] >= 2);

    for (int idx = 0; idx < refmv_count[ref_frame]; ++idx) {
      clamp_mv_ref(&ref_mv_stack[ref_frame][idx].this_mv.as_mv,
                   xd->n8_w << MI_SIZE_LOG2, xd->n8_h << MI_SIZE_LOG2, xd);
      clamp_mv_ref(&ref_mv_stack[ref_frame][idx].comp_mv.as_mv,
                   xd->n8_w << MI_SIZE_LOG2, xd->n8_h << MI_SIZE_LOG2, xd);
    }
  } else {
    // Handle single reference frame extension
    int mi_width = AOMMIN(mi_size_wide[BLOCK_64X64], xd->n8_w);
    mi_width = AOMMIN(mi_width, cm->mi_cols - mi_col);
    int mi_height = AOMMIN(mi_size_high[BLOCK_64X64], xd->n8_h);
    mi_height = AOMMIN(mi_height, cm->mi_rows - mi_row);
    int mi_size = AOMMIN(mi_width, mi_height);

    for (int idx = 0; abs(max_row_offset) >= 1 && idx < mi_size &&
                      refmv_count[ref_frame] < MAX_MV_REF_CANDIDATES;) {
      const MB_MODE_INFO *const candidate = &xd->mi[-xd->mi_stride + idx];
      const int candidate_bsize = candidate->sb_type;

      // TODO(jingning): Refactor the following code.
      for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
        if (candidate->ref_frame[rf_idx] > INTRA_FRAME) {
          int_mv this_mv = candidate->mv[rf_idx];
          if (cm->ref_frame_sign_bias[candidate->ref_frame[rf_idx]] !=
              cm->ref_frame_sign_bias[ref_frame]) {
            this_mv.as_mv.row = -this_mv.as_mv.row;
            this_mv.as_mv.col = -this_mv.as_mv.col;
          }
          int stack_idx;
          for (stack_idx = 0; stack_idx < refmv_count[ref_frame]; ++stack_idx) {
            int_mv stack_mv = ref_mv_stack[ref_frame][stack_idx].this_mv;
            if (this_mv.as_int == stack_mv.as_int) break;
          }

          if (stack_idx == refmv_count[ref_frame]) {
            ref_mv_stack[ref_frame][stack_idx].this_mv = this_mv;

            // TODO(jingning): Set an arbitrary small number here. The weight
            // doesn't matter as long as it is properly initialized.
            ref_mv_stack[ref_frame][stack_idx].weight = 2;
            ++refmv_count[ref_frame];
          }
        }
      }
      idx += mi_size_wide[candidate_bsize];
    }

    for (int idx = 0; abs(max_col_offset) >= 1 && idx < mi_size &&
                      refmv_count[ref_frame] < MAX_MV_REF_CANDIDATES;) {
      const MB_MODE_INFO *const candidate = &xd->mi[idx * xd->mi_stride - 1];
      const int candidate_bsize = candidate->sb_type;

      // TODO(jingning): Refactor the following code.
      for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
        if (candidate->ref_frame[rf_idx] > INTRA_FRAME) {
          int_mv this_mv = candidate->mv[rf_idx];
          if (cm->ref_frame_sign_bias[candidate->ref_frame[rf_idx]] !=
              cm->ref_frame_sign_bias[ref_frame]) {
            this_mv.as_mv.row = -this_mv.as_mv.row;
            this_mv.as_mv.col = -this_mv.as_mv.col;
          }
          int stack_idx;
          for (stack_idx = 0; stack_idx < refmv_count[ref_frame]; ++stack_idx) {
            int_mv stack_mv = ref_mv_stack[ref_frame][stack_idx].this_mv;
            if (this_mv.as_int == stack_mv.as_int) break;
          }

          if (stack_idx == refmv_count[ref_frame]) {
            ref_mv_stack[ref_frame][stack_idx].this_mv = this_mv;

            // TODO(jingning): Set an arbitrary small number here. The weight
            // doesn't matter as long as it is properly initialized.
            ref_mv_stack[ref_frame][stack_idx].weight = 2;
            ++refmv_count[ref_frame];
          }
        }
      }
      idx += mi_size_high[candidate_bsize];
    }

    for (int idx = 0; idx < refmv_count[ref_frame]; ++idx) {
      clamp_mv_ref(&ref_mv_stack[ref_frame][idx].this_mv.as_mv,
                   xd->n8_w << MI_SIZE_LOG2, xd->n8_h << MI_SIZE_LOG2, xd);
    }

    if (mv_ref_list != NULL) {
      for (int idx = refmv_count[ref_frame]; idx < MAX_MV_REF_CANDIDATES; ++idx)
        mv_ref_list[rf[0]][idx].as_int = gm_mv_candidates[0].as_int;

      for (int idx = 0;
           idx < AOMMIN(MAX_MV_REF_CANDIDATES, refmv_count[ref_frame]); ++idx) {
        mv_ref_list[rf[0]][idx].as_int =
            ref_mv_stack[ref_frame][idx].this_mv.as_int;
      }
    }
  }
}

static void av1_find_mv_refs(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      MB_MODE_INFO *mi, MV_REFERENCE_FRAME ref_frame,
                      uint8_t ref_mv_count[MODE_CTX_REF_FRAMES],
                      CANDIDATE_MV ref_mv_stack[][MAX_REF_MV_STACK_SIZE],
                      int_mv mv_ref_list[][MAX_MV_REF_CANDIDATES],
                      int_mv *global_mvs, int mi_row, int mi_col,
                      int16_t *mode_context) {
  int_mv zeromv[2];
  BLOCK_SIZE bsize = mi->sb_type;
  MV_REFERENCE_FRAME rf[2];
  av1_set_ref_frame(rf, ref_frame);

  if (ref_frame < REF_FRAMES) {
    if (ref_frame != INTRA_FRAME) {
      global_mvs[ref_frame] = gm_get_motion_vector(
          &cm->global_motion[ref_frame], cm->allow_high_precision_mv, bsize,
          mi_col, mi_row, cm->cur_frame_force_integer_mv);
    } else {
      global_mvs[ref_frame].as_int = INVALID_MV;
    }
  }

  if (ref_frame != INTRA_FRAME) {
    zeromv[0].as_int =
        gm_get_motion_vector(&cm->global_motion[rf[0]],
                             cm->allow_high_precision_mv, bsize, mi_col, mi_row,
                             cm->cur_frame_force_integer_mv)
            .as_int;
    zeromv[1].as_int =
        (rf[1] != NONE_FRAME)
            ? gm_get_motion_vector(&cm->global_motion[rf[1]],
                                   cm->allow_high_precision_mv, bsize, mi_col,
                                   mi_row, cm->cur_frame_force_integer_mv)
                  .as_int
            : 0;
  } else {
    zeromv[0].as_int = zeromv[1].as_int = 0;
  }

  setup_ref_mv_list(cm, xd, ref_frame, ref_mv_count, ref_mv_stack, mv_ref_list,
                    zeromv, mi_row, mi_col, mode_context);
}

static void av1_setup_frame_buf_refs(AV1_COMMON *cm) {
  cm->cur_frame.cur_frame_offset = cm->frame_offset;

  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int buf_idx = cm->frame_refs[ref_frame - LAST_FRAME].idx;
    if (buf_idx >= 0)
      cm->cur_frame.ref_frame_offset[ref_frame - LAST_FRAME] =
          cm->buffer_pool.frame_bufs[buf_idx].cur_frame_offset;
  }
}

#define MAX_OFFSET_WIDTH 64
#define MAX_OFFSET_HEIGHT 0

static int get_block_position(AV1_COMMON *cm, int *mi_r, int *mi_c, int blk_row,
                              int blk_col, MV mv, int sign_bias) {
  const int base_blk_row = (blk_row >> 3) << 3;
  const int base_blk_col = (blk_col >> 3) << 3;

  const int row_offset = (mv.row >= 0) ? (mv.row >> (4 + MI_SIZE_LOG2))
                                       : -((-mv.row) >> (4 + MI_SIZE_LOG2));

  const int col_offset = (mv.col >= 0) ? (mv.col >> (4 + MI_SIZE_LOG2))
                                       : -((-mv.col) >> (4 + MI_SIZE_LOG2));

  int row = (sign_bias == 1) ? blk_row - row_offset : blk_row + row_offset;
  int col = (sign_bias == 1) ? blk_col - col_offset : blk_col + col_offset;

  if (row < 0 || row >= (cm->mi_rows >> 1) || col < 0 ||
      col >= (cm->mi_cols >> 1))
    return 0;

  if (row < base_blk_row - (MAX_OFFSET_HEIGHT >> 3) ||
      row >= base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3) ||
      col < base_blk_col - (MAX_OFFSET_WIDTH >> 3) ||
      col >= base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3))
    return 0;

  *mi_r = row;
  *mi_c = col;

  return 1;
}

static int motion_field_projection(AV1_COMMON *cm, MV_REFERENCE_FRAME ref_frame,
                                   int dir,
                                   const int from_x4, const int to_x4,
                                   const int from_y4, const int to_y4) {
  TPL_MV_REF *tpl_mvs_base = cm->tpl_mvs;
  int ref_offset[TOTAL_REFS_PER_FRAME] = { 0 };
  int ref_sign[TOTAL_REFS_PER_FRAME] = { 0 };

  (void)dir;

  int ref_frame_idx = cm->frame_refs[FWD_RF_OFFSET(ref_frame)].idx;
  if (ref_frame_idx < 0) return 0;

  if (cm->buffer_pool.frame_bufs[ref_frame_idx].intra_only) return 0;

  if (cm->buffer_pool.frame_bufs[ref_frame_idx].mi_rows != cm->mi_rows ||
      cm->buffer_pool.frame_bufs[ref_frame_idx].mi_cols != cm->mi_cols)
    return 0;

  int ref_frame_index =
      cm->buffer_pool.frame_bufs[ref_frame_idx].cur_frame_offset;
  unsigned int *ref_rf_idx =
      &cm->buffer_pool.frame_bufs[ref_frame_idx].ref_frame_offset[0];
   int cur_frame_index = cm->cur_frame.cur_frame_offset;
  int ref_to_cur = get_relative_dist(cm, ref_frame_index, cur_frame_index);

  for (MV_REFERENCE_FRAME rf = LAST_FRAME; rf <= INTER_REFS_PER_FRAME; ++rf) {
    ref_offset[rf] =
        get_relative_dist(cm, ref_frame_index, ref_rf_idx[rf - LAST_FRAME]);
    // note the inverted sign
    ref_sign[rf] =
        get_relative_dist(cm, ref_rf_idx[rf - LAST_FRAME], ref_frame_index) < 0;
  }

  if (dir == 2) ref_to_cur = -ref_to_cur;

  MV_REF *mv_ref_base = cm->buffer_pool.frame_bufs[ref_frame_idx].mvs;
  const ptrdiff_t mv_stride =
    cm->buffer_pool.frame_bufs[ref_frame_idx].mv_stride;
  const int mvs_rows = (cm->mi_rows + 1) >> 1;
  const int mvs_cols = (cm->mi_cols + 1) >> 1;

  assert(from_y4 >= 0);
  const int row_start8 = from_y4 >> 1;
  const int row_end8 = imin(to_y4 >> 1, mvs_rows);
  const int col_start8 = imax((from_x4 - (MAX_OFFSET_WIDTH >> 2)) >> 1, 0);
  const int col_end8 = imin((to_x4 + (MAX_OFFSET_WIDTH >> 2)) >> 1, mvs_cols);
  for (int blk_row = row_start8; blk_row < row_end8; ++blk_row) {
    for (int blk_col = col_start8; blk_col < col_end8; ++blk_col) {
      MV_REF *mv_ref = &mv_ref_base[((blk_row << 1) + 1) * mv_stride +
                                     (blk_col << 1) + 1];
      int diridx;
      const int ref0 = mv_ref->ref_frame[0], ref1 = mv_ref->ref_frame[1];
      if (ref1 > 0 && ref_sign[ref1] &&
          abs(mv_ref->mv[1].as_mv.row) < (1 << 12) &&
          abs(mv_ref->mv[1].as_mv.col) < (1 << 12))
      {
        diridx = 1;
      } else if (ref0 > 0 && ref_sign[ref0] &&
                 abs(mv_ref->mv[0].as_mv.row) < (1 << 12) &&
                 abs(mv_ref->mv[0].as_mv.col) < (1 << 12))
      {
        diridx = 0;
      } else {
        continue;
      }
      MV fwd_mv = mv_ref->mv[diridx].as_mv;

      if (mv_ref->ref_frame[diridx] > INTRA_FRAME) {
        int_mv this_mv;
        int mi_r, mi_c;
        const int ref_frame_offset = ref_offset[mv_ref->ref_frame[diridx]];

        int pos_valid = abs(ref_frame_offset) <= MAX_FRAME_DISTANCE &&
                        ref_frame_offset > 0 &&
                        abs(ref_to_cur) <= MAX_FRAME_DISTANCE;

        if (pos_valid) {
          get_mv_projection(&this_mv.as_mv, fwd_mv, ref_to_cur,
                            ref_frame_offset);
          pos_valid = get_block_position(cm, &mi_r, &mi_c, blk_row, blk_col,
                                         this_mv.as_mv, dir >> 1);
        }

        if (pos_valid && mi_c >= (from_x4 >> 1) && mi_c < (to_x4 >> 1)) {
          int mi_offset = mi_r * (cm->mi_stride >> 1) + mi_c;

          tpl_mvs_base[mi_offset].mfmv0.as_mv.row = fwd_mv.row;
          tpl_mvs_base[mi_offset].mfmv0.as_mv.col = fwd_mv.col;
          tpl_mvs_base[mi_offset].ref_frame_offset = ref_frame_offset;
        }
      }
    }
  }

  return 1;
}

static void av1_setup_motion_field(AV1_COMMON *cm) {
  if (!cm->seq_params.enable_order_hint) return;

  TPL_MV_REF *tpl_mvs_base = cm->tpl_mvs;
  int size = (((cm->mi_rows + 31) & ~31) >> 1) * (cm->mi_stride >> 1);
  for (int idx = 0; idx < size; ++idx) {
    tpl_mvs_base[idx].mfmv0.as_int = INVALID_MV;
    tpl_mvs_base[idx].ref_frame_offset = 0;
  }

  memset(cm->ref_frame_side, 0, sizeof(cm->ref_frame_side));
  RefCntBuffer *const frame_bufs = cm->buffer_pool.frame_bufs;

  const int cur_order_hint = cm->cur_frame.cur_frame_offset;
  int *const ref_buf_idx = cm->ref_buf_idx;
  int *const ref_order_hint = cm->ref_order_hint;

  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    const int ref_idx = ref_frame - LAST_FRAME;
    const int buf_idx = cm->frame_refs[ref_idx].idx;
    int order_hint = 0;

    if (buf_idx >= 0) order_hint = frame_bufs[buf_idx].cur_frame_offset;

    ref_buf_idx[ref_idx] = buf_idx;
    ref_order_hint[ref_idx] = order_hint;

    if (get_relative_dist(cm, order_hint, cur_order_hint) > 0)
      cm->ref_frame_side[ref_frame] = 1;
    else if (order_hint == cur_order_hint)
      cm->ref_frame_side[ref_frame] = -1;
  }
}

enum BlockSize {
    BS_128x128,
    BS_128x64,
    BS_64x128,
    BS_64x64,
    BS_64x32,
    BS_64x16,
    BS_32x64,
    BS_32x32,
    BS_32x16,
    BS_32x8,
    BS_16x64,
    BS_16x32,
    BS_16x16,
    BS_16x8,
    BS_16x4,
    BS_8x32,
    BS_8x16,
    BS_8x8,
    BS_8x4,
    BS_4x16,
    BS_4x8,
    BS_4x4,
    N_BS_SIZES,
};
extern const uint8_t dav1d_block_dimensions[N_BS_SIZES][4];
const uint8_t dav1d_bs_to_sbtype[N_BS_SIZES] = {
    [BS_128x128] = BLOCK_128X128,
    [BS_128x64] = BLOCK_128X64,
    [BS_64x128] = BLOCK_64X128,
    [BS_64x64] = BLOCK_64X64,
    [BS_64x32] = BLOCK_64X32,
    [BS_64x16] = BLOCK_64X16,
    [BS_32x64] = BLOCK_32X64,
    [BS_32x32] = BLOCK_32X32,
    [BS_32x16] = BLOCK_32X16,
    [BS_32x8] = BLOCK_32X8,
    [BS_16x64] = BLOCK_16X64,
    [BS_16x32] = BLOCK_16X32,
    [BS_16x16] = BLOCK_16X16,
    [BS_16x8] = BLOCK_16X8,
    [BS_16x4] = BLOCK_16X4,
    [BS_8x32] = BLOCK_8X32,
    [BS_8x16] = BLOCK_8X16,
    [BS_8x8] = BLOCK_8X8,
    [BS_8x4] = BLOCK_8X4,
    [BS_4x16] = BLOCK_4X16,
    [BS_4x8] = BLOCK_4X8,
    [BS_4x4] = BLOCK_4X4,
};
const uint8_t dav1d_sbtype_to_bs[BLOCK_SIZES_ALL] = {
    [BLOCK_128X128] = BS_128x128,
    [BLOCK_128X64] = BS_128x64,
    [BLOCK_64X128] = BS_64x128,
    [BLOCK_64X64] = BS_64x64,
    [BLOCK_64X32] = BS_64x32,
    [BLOCK_64X16] = BS_64x16,
    [BLOCK_32X64] = BS_32x64,
    [BLOCK_32X32] = BS_32x32,
    [BLOCK_32X16] = BS_32x16,
    [BLOCK_32X8] = BS_32x8,
    [BLOCK_16X64] = BS_16x64,
    [BLOCK_16X32] = BS_16x32,
    [BLOCK_16X16] = BS_16x16,
    [BLOCK_16X8] = BS_16x8,
    [BLOCK_16X4] = BS_16x4,
    [BLOCK_8X32] = BS_8x32,
    [BLOCK_8X16] = BS_8x16,
    [BLOCK_8X8] = BS_8x8,
    [BLOCK_8X4] = BS_8x4,
    [BLOCK_4X16] = BS_4x16,
    [BLOCK_4X8] = BS_4x8,
    [BLOCK_4X4] = BS_4x4,
};

#include <stdio.h>

void dav1d_find_ref_mvs(CANDIDATE_MV *mvstack, int *cnt, int_mv (*mvlist)[2],
                        int *ctx, int refidx_dav1d[2],
                        int w4, int h4, int bs, int bp, int by4, int bx4,
                        int tile_col_start4, int tile_col_end4,
                        int tile_row_start4, int tile_row_end4,
                        AV1_COMMON *cm);
void dav1d_find_ref_mvs(CANDIDATE_MV *mvstack, int *cnt, int_mv (*mvlist)[2],
                        int *ctx, int refidx_dav1d[2],
                        int w4, int h4, int bs, int bp, int by4, int bx4,
                        int tile_col_start4, int tile_col_end4,
                        int tile_row_start4, int tile_row_end4,
                        AV1_COMMON *cm)
{
    const int bw4 = dav1d_block_dimensions[bs][0];
    const int bh4 = dav1d_block_dimensions[bs][1];
    int stride = (int) cm->cur_frame.mv_stride;
    MACROBLOCKD xd = (MACROBLOCKD) {
        .n8_w = bw4,
        .n8_h = bh4,
        .mi_stride = stride,
        .up_available = by4 > tile_row_start4,
        .left_available = bx4 > tile_col_start4,
        .tile = {
            .mi_col_end = AOMMIN(w4, tile_col_end4),
            .mi_row_end = AOMMIN(h4, tile_row_end4),
            .tg_horz_boundary = 0,
            .mi_row_start = tile_row_start4,
            .mi_col_start = tile_col_start4,
        },
        .mi = (MB_MODE_INFO *) &cm->cur_frame.mvs[by4 * stride + bx4],
        .mb_to_bottom_edge = (h4 - bh4 - by4) * 32,
        .mb_to_left_edge = -bx4 * 32,
        .mb_to_right_edge = (w4 - bw4 - bx4) * 32,
        .mb_to_top_edge = -by4 * 32,
        .is_sec_rect = 0,
        .cur_mi = {
            .partition = bp,
        },
    };
    xd.mi->sb_type = dav1d_bs_to_sbtype[bs];
    if (xd.n8_w < xd.n8_h) {
        // Only mark is_sec_rect as 1 for the last block.
        // For PARTITION_VERT_4, it would be (0, 0, 0, 1);
        // For other partitions, it would be (0, 1).
        if (!((bx4 + xd.n8_w) & (xd.n8_h - 1))) xd.is_sec_rect = 1;
    }

    if (xd.n8_w > xd.n8_h)
        if (by4 & (xd.n8_w - 1)) xd.is_sec_rect = 1;

    MV_REFERENCE_FRAME rf[2] = { refidx_dav1d[0] + 1, refidx_dav1d[1] + 1 };
    const int refidx = av1_ref_frame_type(rf);
    int16_t single_context[MODE_CTX_REF_FRAMES];
    uint8_t mv_cnt[MODE_CTX_REF_FRAMES];
    CANDIDATE_MV mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
    int_mv mv_list[MODE_CTX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
    int_mv gmvs[MODE_CTX_REF_FRAMES];
    av1_find_mv_refs(cm, &xd, xd.mi, refidx, mv_cnt,
                     mv_stack, mv_list, gmvs, by4, bx4,
                     single_context);
    for (int i = 0; i < mv_cnt[refidx]; i++)
        mvstack[i] = mv_stack[refidx][i];
    *cnt = mv_cnt[refidx];

    mvlist[0][0] = mv_list[refidx_dav1d[0] + 1][0];
    mvlist[0][1] = mv_list[refidx_dav1d[0] + 1][1];
    if (refidx_dav1d[1] != -1) {
        mvlist[1][0] = mv_list[refidx_dav1d[1] + 1][0];
        mvlist[1][1] = mv_list[refidx_dav1d[1] + 1][1];
    }

    if (ctx) {
        if (refidx_dav1d[1] == -1)
            *ctx = single_context[refidx_dav1d[0] + 1];
        else
            *ctx = av1_mode_context_analyzer(single_context, rf);
    }
}

int dav1d_init_ref_mv_common(AV1_COMMON *cm, const int w8, const int h8,
                             const ptrdiff_t stride, const int allow_sb128,
                             MV_REF *cur, MV_REF *ref_mvs[7],
                             const unsigned cur_poc,
                             const unsigned ref_poc[7],
                             const unsigned ref_ref_poc[7][7],
                             const Dav1dWarpedMotionParams gmv[7],
                             const int allow_hp, const int force_int_mv,
                             const int allow_ref_frame_mvs,
                             const int order_hint);
int dav1d_init_ref_mv_common(AV1_COMMON *cm, const int w8, const int h8,
                             const ptrdiff_t stride, const int allow_sb128,
                             MV_REF *cur, MV_REF *ref_mvs[7],
                             const unsigned cur_poc,
                             const unsigned ref_poc[7],
                             const unsigned ref_ref_poc[7][7],
                             const Dav1dWarpedMotionParams gmv[7],
                             const int allow_hp, const int force_int_mv,
                             const int allow_ref_frame_mvs,
                             const int order_hint)
{
    if (cm->mi_cols != (w8 << 1) || cm->mi_rows != (h8 << 1)) {
        const int align_h = (h8 + 15) & ~15;
        if (cm->tpl_mvs) free(cm->tpl_mvs);
        cm->tpl_mvs = malloc(sizeof(*cm->tpl_mvs) * (stride >> 1) * align_h);
        if (!cm->tpl_mvs) return DAV1D_ERR(ENOMEM);
        for (int i = 0; i < 7; i++)
            cm->frame_refs[i].idx = i;
        cm->mi_cols = w8 << 1;
        cm->mi_rows = h8 << 1;
        cm->mi_stride = (int) stride;
        for (int i = 0; i < 7; i++) {
            cm->buffer_pool.frame_bufs[i].mi_rows = cm->mi_rows;
            cm->buffer_pool.frame_bufs[i].mi_cols = cm->mi_cols;
            cm->buffer_pool.frame_bufs[i].mv_stride = stride;
        }
        cm->cur_frame.mv_stride = stride;
    }

    cm->allow_high_precision_mv = allow_hp;
    cm->seq_params.sb_size = allow_sb128 ? BLOCK_128X128 : BLOCK_64X64;

    cm->seq_params.enable_order_hint = !!order_hint;
    cm->seq_params.order_hint_bits_minus1 = order_hint - 1;
    // FIXME get these from the sequence/frame headers instead of hardcoding
    cm->frame_parallel_decode = 0;
    cm->cur_frame_force_integer_mv = force_int_mv;

    memcpy(&cm->global_motion[1], gmv, sizeof(*gmv) * 7);

    cm->frame_offset = cur_poc;
    cm->allow_ref_frame_mvs = allow_ref_frame_mvs;
    cm->cur_frame.mvs = cur;
    for (int i = 0; i < 7; i++) {
        cm->buffer_pool.frame_bufs[i].mvs = ref_mvs[i];
        cm->buffer_pool.frame_bufs[i].intra_only = ref_mvs[i] == NULL;
        cm->buffer_pool.frame_bufs[i].cur_frame_offset = ref_poc[i];
        for (int j = 0; j < 7; j++)
            cm->buffer_pool.frame_bufs[i].ref_frame_offset[j] =
                ref_ref_poc[i][j];
    }
    av1_setup_frame_buf_refs(cm);
    for (int i = 0; i < 7; i++) {
        const int ref_poc = cm->buffer_pool.frame_bufs[i].cur_frame_offset;
        cm->ref_frame_sign_bias[1 + i] = get_relative_dist(cm, ref_poc, cur_poc) > 0;
    }
    if (allow_ref_frame_mvs) {
        av1_setup_motion_field(cm);
    }

    return 0;
}

void dav1d_init_ref_mv_tile_row(AV1_COMMON *cm,
                                int tile_col_start4, int tile_col_end4,
                                int row_start4, int row_end4);
void dav1d_init_ref_mv_tile_row(AV1_COMMON *cm,
                                int tile_col_start4, int tile_col_end4,
                                int row_start4, int row_end4)
{
  RefCntBuffer *const frame_bufs = cm->buffer_pool.frame_bufs;
  const int cur_order_hint = cm->cur_frame.cur_frame_offset;
  int *const ref_buf_idx = cm->ref_buf_idx;
  int *const ref_order_hint = cm->ref_order_hint;

  int ref_stamp = MFMV_STACK_SIZE - 1;

  if (ref_buf_idx[LAST_FRAME - LAST_FRAME] >= 0) {
    const int alt_of_lst_order_hint =
        frame_bufs[ref_buf_idx[LAST_FRAME - LAST_FRAME]]
            .ref_frame_offset[ALTREF_FRAME - LAST_FRAME];

    const int is_lst_overlay =
        (alt_of_lst_order_hint == ref_order_hint[GOLDEN_FRAME - LAST_FRAME]);
      if (!is_lst_overlay) motion_field_projection(cm, LAST_FRAME, 2,
                                                   tile_col_start4, tile_col_end4,
                                                   row_start4, row_end4);
    --ref_stamp;
  }

  if (get_relative_dist(cm, ref_order_hint[BWDREF_FRAME - LAST_FRAME],
                        cur_order_hint) > 0) {
      if (motion_field_projection(cm, BWDREF_FRAME, 0,
                                  tile_col_start4, tile_col_end4,
                                  row_start4, row_end4)) --ref_stamp;
  }

  if (get_relative_dist(cm, ref_order_hint[ALTREF2_FRAME - LAST_FRAME],
                        cur_order_hint) > 0) {
      if (motion_field_projection(cm, ALTREF2_FRAME, 0,
                                  tile_col_start4, tile_col_end4,
                                  row_start4, row_end4)) --ref_stamp;
  }

  if (get_relative_dist(cm, ref_order_hint[ALTREF_FRAME - LAST_FRAME],
                        cur_order_hint) > 0 &&
      ref_stamp >= 0)
      if (motion_field_projection(cm, ALTREF_FRAME, 0,
                                  tile_col_start4, tile_col_end4,
                                  row_start4, row_end4)) --ref_stamp;

  if (ref_stamp >= 0 && ref_buf_idx[LAST2_FRAME - LAST_FRAME] >= 0)
      if (motion_field_projection(cm, LAST2_FRAME, 2,
                                  tile_col_start4, tile_col_end4,
                                  row_start4, row_end4)) --ref_stamp;
}

AV1_COMMON *dav1d_alloc_ref_mv_common(void);
AV1_COMMON *dav1d_alloc_ref_mv_common(void) {
    AV1_COMMON *cm = malloc(sizeof(*cm));
    if (!cm) return NULL;
    memset(cm, 0, sizeof(*cm));
    return cm;
}

void dav1d_free_ref_mv_common(AV1_COMMON *cm);
void dav1d_free_ref_mv_common(AV1_COMMON *cm) {
    if (cm->tpl_mvs) free(cm->tpl_mvs);
    free(cm);
}
