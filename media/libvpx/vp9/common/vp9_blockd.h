/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_COMMON_VP9_BLOCKD_H_
#define VP9_COMMON_VP9_BLOCKD_H_

#include "./vpx_config.h"

#include "vpx_ports/mem.h"
#include "vpx_scale/yv12config.h"

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_common_data.h"
#include "vp9/common/vp9_enums.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_mv.h"
#include "vp9/common/vp9_scale.h"
#include "vp9/common/vp9_seg_common.h"
#include "vp9/common/vp9_treecoder.h"

#define BLOCK_SIZE_GROUPS   4
#define MBSKIP_CONTEXTS 3

/* Segment Feature Masks */
#define MAX_MV_REF_CANDIDATES 2

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 5

typedef enum {
  PLANE_TYPE_Y_WITH_DC,
  PLANE_TYPE_UV,
} PLANE_TYPE;

typedef char ENTROPY_CONTEXT;

typedef char PARTITION_CONTEXT;

static INLINE int combine_entropy_contexts(ENTROPY_CONTEXT a,
                                           ENTROPY_CONTEXT b) {
  return (a != 0) + (b != 0);
}

typedef enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  FRAME_TYPES,
} FRAME_TYPE;

typedef enum {
  DC_PRED,         // Average of above and left pixels
  V_PRED,          // Vertical
  H_PRED,          // Horizontal
  D45_PRED,        // Directional 45  deg = round(arctan(1/1) * 180/pi)
  D135_PRED,       // Directional 135 deg = 180 - 45
  D117_PRED,       // Directional 117 deg = 180 - 63
  D153_PRED,       // Directional 153 deg = 180 - 27
  D207_PRED,       // Directional 207 deg = 180 + 27
  D63_PRED,        // Directional 63  deg = round(arctan(2/1) * 180/pi)
  TM_PRED,         // True-motion
  NEARESTMV,
  NEARMV,
  ZEROMV,
  NEWMV,
  MB_MODE_COUNT
} MB_PREDICTION_MODE;

static INLINE int is_inter_mode(MB_PREDICTION_MODE mode) {
  return mode >= NEARESTMV && mode <= NEWMV;
}

#define INTRA_MODES (TM_PRED + 1)

#define INTER_MODES (1 + NEWMV - NEARESTMV)

#define INTER_OFFSET(mode) ((mode) - NEARESTMV)


/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

typedef struct {
  MB_PREDICTION_MODE as_mode;
  int_mv as_mv[2];  // first, second inter predictor motion vectors
} b_mode_info;

typedef enum {
  NONE = -1,
  INTRA_FRAME = 0,
  LAST_FRAME = 1,
  GOLDEN_FRAME = 2,
  ALTREF_FRAME = 3,
  MAX_REF_FRAMES = 4
} MV_REFERENCE_FRAME;

static INLINE int b_width_log2(BLOCK_SIZE sb_type) {
  return b_width_log2_lookup[sb_type];
}
static INLINE int b_height_log2(BLOCK_SIZE sb_type) {
  return b_height_log2_lookup[sb_type];
}

static INLINE int mi_width_log2(BLOCK_SIZE sb_type) {
  return mi_width_log2_lookup[sb_type];
}

static INLINE int mi_height_log2(BLOCK_SIZE sb_type) {
  return mi_height_log2_lookup[sb_type];
}

// This structure now relates to 8x8 block regions.
typedef struct {
  MB_PREDICTION_MODE mode, uv_mode;
  MV_REFERENCE_FRAME ref_frame[2];
  TX_SIZE tx_size;
  int_mv mv[2];                // for each reference frame used
  int_mv ref_mvs[MAX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int_mv best_mv[2];

  uint8_t mode_context[MAX_REF_FRAMES];

  unsigned char skip_coeff;    // 0=need to decode coeffs, 1=no coefficients
  unsigned char segment_id;    // Segment id for this block.

  // Flags used for prediction status of various bit-stream signals
  unsigned char seg_id_predicted;

  INTERPOLATION_TYPE interp_filter;

  BLOCK_SIZE sb_type;
} MB_MODE_INFO;

typedef struct {
  MB_MODE_INFO mbmi;
  b_mode_info bmi[4];
} MODE_INFO;

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
}

enum mv_precision {
  MV_PRECISION_Q3,
  MV_PRECISION_Q4
};

#if CONFIG_ALPHA
enum { MAX_MB_PLANE = 4 };
#else
enum { MAX_MB_PLANE = 3 };
#endif

struct buf_2d {
  uint8_t *buf;
  int stride;
};

struct macroblockd_plane {
  int16_t *qcoeff;
  int16_t *dqcoeff;
  uint16_t *eobs;
  PLANE_TYPE plane_type;
  int subsampling_x;
  int subsampling_y;
  struct buf_2d dst;
  struct buf_2d pre[2];
  int16_t *dequant;
  ENTROPY_CONTEXT *above_context;
  ENTROPY_CONTEXT *left_context;
};

#define BLOCK_OFFSET(x, i) ((x) + (i) * 16)

typedef struct macroblockd {
  struct macroblockd_plane plane[MAX_MB_PLANE];

  struct scale_factors scale_factor[2];

  MODE_INFO *last_mi;
  int mode_info_stride;

  // A NULL indicates that the 8x8 is not part of the image
  MODE_INFO **mi_8x8;
  MODE_INFO **prev_mi_8x8;
  MODE_INFO *mi_stream;

  int up_available;
  int left_available;

  /* Distance of MB away from frame edges */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;

  int lossless;
  /* Inverse transform function pointers. */
  void (*itxm_add)(const int16_t *input, uint8_t *dest, int stride, int eob);

  struct subpix_fn_table  subpix;

  int corrupted;

  /* Y,U,V,(A) */
  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][16];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[8];
} MACROBLOCKD;



static BLOCK_SIZE get_subsize(BLOCK_SIZE bsize, PARTITION_TYPE partition) {
  const BLOCK_SIZE subsize = subsize_lookup[partition][bsize];
  assert(subsize < BLOCK_SIZES);
  return subsize;
}

extern const TX_TYPE mode2txfm_map[MB_MODE_COUNT];

static INLINE TX_TYPE get_tx_type_4x4(PLANE_TYPE plane_type,
                                      const MACROBLOCKD *xd, int ib) {
  const MODE_INFO *const mi = xd->mi_8x8[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;

  if (plane_type != PLANE_TYPE_Y_WITH_DC ||
      xd->lossless ||
      is_inter_block(mbmi))
    return DCT_DCT;

  return mode2txfm_map[mbmi->sb_type < BLOCK_8X8 ?
                       mi->bmi[ib].as_mode : mbmi->mode];
}

static INLINE TX_TYPE get_tx_type_8x8(PLANE_TYPE plane_type,
                                      const MACROBLOCKD *xd) {
  return plane_type == PLANE_TYPE_Y_WITH_DC ?
             mode2txfm_map[xd->mi_8x8[0]->mbmi.mode] : DCT_DCT;
}

static INLINE TX_TYPE get_tx_type_16x16(PLANE_TYPE plane_type,
                                        const MACROBLOCKD *xd) {
  return plane_type == PLANE_TYPE_Y_WITH_DC ?
             mode2txfm_map[xd->mi_8x8[0]->mbmi.mode] : DCT_DCT;
}

static void setup_block_dptrs(MACROBLOCKD *xd, int ss_x, int ss_y) {
  int i;

  for (i = 0; i < MAX_MB_PLANE; i++) {
    xd->plane[i].plane_type = i ? PLANE_TYPE_UV : PLANE_TYPE_Y_WITH_DC;
    xd->plane[i].subsampling_x = i ? ss_x : 0;
    xd->plane[i].subsampling_y = i ? ss_y : 0;
  }
#if CONFIG_ALPHA
  // TODO(jkoleszar): Using the Y w/h for now
  xd->plane[3].subsampling_x = 0;
  xd->plane[3].subsampling_y = 0;
#endif
}


static INLINE TX_SIZE get_uv_tx_size(const MB_MODE_INFO *mbmi) {
  return MIN(mbmi->tx_size, max_uv_txsize_lookup[mbmi->sb_type]);
}

static BLOCK_SIZE get_plane_block_size(BLOCK_SIZE bsize,
                                       const struct macroblockd_plane *pd) {
  BLOCK_SIZE bs = ss_size_lookup[bsize][pd->subsampling_x][pd->subsampling_y];
  assert(bs < BLOCK_SIZES);
  return bs;
}

static INLINE int plane_block_width(BLOCK_SIZE bsize,
                                    const struct macroblockd_plane* plane) {
  return 4 << (b_width_log2(bsize) - plane->subsampling_x);
}

static INLINE int plane_block_height(BLOCK_SIZE bsize,
                                     const struct macroblockd_plane* plane) {
  return 4 << (b_height_log2(bsize) - plane->subsampling_y);
}

typedef void (*foreach_transformed_block_visitor)(int plane, int block,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size,
                                                  void *arg);

static INLINE void foreach_transformed_block_in_plane(
    const MACROBLOCKD *const xd, BLOCK_SIZE bsize, int plane,
    foreach_transformed_block_visitor visit, void *arg) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const MB_MODE_INFO* mbmi = &xd->mi_8x8[0]->mbmi;
  // block and transform sizes, in number of 4x4 blocks log 2 ("*_b")
  // 4x4=0, 8x8=2, 16x16=4, 32x32=6, 64x64=8
  // transform size varies per plane, look it up in a common way.
  const TX_SIZE tx_size = plane ? get_uv_tx_size(mbmi)
                                : mbmi->tx_size;
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
  const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
  const int step = 1 << (tx_size << 1);
  int i;

  // If mb_to_right_edge is < 0 we are in a situation in which
  // the current block size extends into the UMV and we won't
  // visit the sub blocks that are wholly within the UMV.
  if (xd->mb_to_right_edge < 0 || xd->mb_to_bottom_edge < 0) {
    int r, c;

    int max_blocks_wide = num_4x4_w;
    int max_blocks_high = num_4x4_h;

    // xd->mb_to_right_edge is in units of pixels * 8.  This converts
    // it to 4x4 block sizes.
    if (xd->mb_to_right_edge < 0)
      max_blocks_wide += (xd->mb_to_right_edge >> (5 + pd->subsampling_x));

    if (xd->mb_to_bottom_edge < 0)
      max_blocks_high += (xd->mb_to_bottom_edge >> (5 + pd->subsampling_y));

    i = 0;
    // Unlike the normal case - in here we have to keep track of the
    // row and column of the blocks we use so that we know if we are in
    // the unrestricted motion border.
    for (r = 0; r < num_4x4_h; r += (1 << tx_size)) {
      for (c = 0; c < num_4x4_w; c += (1 << tx_size)) {
        if (r < max_blocks_high && c < max_blocks_wide)
          visit(plane, i, plane_bsize, tx_size, arg);
        i += step;
      }
    }
  } else {
    for (i = 0; i < num_4x4_w * num_4x4_h; i += step)
      visit(plane, i, plane_bsize, tx_size, arg);
  }
}

static INLINE void foreach_transformed_block(
    const MACROBLOCKD* const xd, BLOCK_SIZE bsize,
    foreach_transformed_block_visitor visit, void *arg) {
  int plane;

  for (plane = 0; plane < MAX_MB_PLANE; plane++)
    foreach_transformed_block_in_plane(xd, bsize, plane, visit, arg);
}

static INLINE void foreach_transformed_block_uv(
    const MACROBLOCKD* const xd, BLOCK_SIZE bsize,
    foreach_transformed_block_visitor visit, void *arg) {
  int plane;

  for (plane = 1; plane < MAX_MB_PLANE; plane++)
    foreach_transformed_block_in_plane(xd, bsize, plane, visit, arg);
}

static int raster_block_offset(BLOCK_SIZE plane_bsize,
                               int raster_block, int stride) {
  const int bw = b_width_log2(plane_bsize);
  const int y = 4 * (raster_block >> bw);
  const int x = 4 * (raster_block & ((1 << bw) - 1));
  return y * stride + x;
}
static int16_t* raster_block_offset_int16(BLOCK_SIZE plane_bsize,
                                          int raster_block, int16_t *base) {
  const int stride = 4 << b_width_log2(plane_bsize);
  return base + raster_block_offset(plane_bsize, raster_block, stride);
}
static uint8_t* raster_block_offset_uint8(BLOCK_SIZE plane_bsize,
                                          int raster_block, uint8_t *base,
                                          int stride) {
  return base + raster_block_offset(plane_bsize, raster_block, stride);
}

static int txfrm_block_to_raster_block(BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, int block) {
  const int bwl = b_width_log2(plane_bsize);
  const int tx_cols_log2 = bwl - tx_size;
  const int tx_cols = 1 << tx_cols_log2;
  const int raster_mb = block >> (tx_size << 1);
  const int x = (raster_mb & (tx_cols - 1)) << tx_size;
  const int y = (raster_mb >> tx_cols_log2) << tx_size;
  return x + (y << bwl);
}

static void txfrm_block_to_raster_xy(BLOCK_SIZE plane_bsize,
                                     TX_SIZE tx_size, int block,
                                     int *x, int *y) {
  const int bwl = b_width_log2(plane_bsize);
  const int tx_cols_log2 = bwl - tx_size;
  const int tx_cols = 1 << tx_cols_log2;
  const int raster_mb = block >> (tx_size << 1);
  *x = (raster_mb & (tx_cols - 1)) << tx_size;
  *y = (raster_mb >> tx_cols_log2) << tx_size;
}

static void extend_for_intra(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize,
                             int plane, int block, TX_SIZE tx_size) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  uint8_t *const buf = pd->dst.buf;
  const int stride = pd->dst.stride;

  int x, y;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &x, &y);
  x = x * 4 - 1;
  y = y * 4 - 1;
  // Copy a pixel into the umv if we are in a situation where the block size
  // extends into the UMV.
  // TODO(JBB): Should be able to do the full extend in place so we don't have
  // to do this multiple times.
  if (xd->mb_to_right_edge < 0) {
    const int bw = 4 << b_width_log2(plane_bsize);
    const int umv_border_start = bw + (xd->mb_to_right_edge >>
                                       (3 + pd->subsampling_x));

    if (x + bw > umv_border_start)
      vpx_memset(&buf[y * stride + umv_border_start],
                 buf[y * stride + umv_border_start - 1], bw);
  }

  if (xd->mb_to_bottom_edge < 0) {
    if (xd->left_available || x >= 0) {
      const int bh = 4 << b_height_log2(plane_bsize);
      const int umv_border_start =
          bh + (xd->mb_to_bottom_edge >> (3 + pd->subsampling_y));

      if (y + bh > umv_border_start) {
        const uint8_t c = buf[(umv_border_start - 1) * stride + x];
        uint8_t *d = &buf[umv_border_start * stride + x];
        int i;
        for (i = 0; i < bh; ++i, d += stride)
          *d = c;
      }
    }
  }
}

static void set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                         int has_eob, int aoff, int loff) {
  ENTROPY_CONTEXT *const a = pd->above_context + aoff;
  ENTROPY_CONTEXT *const l = pd->left_context + loff;
  const int tx_size_in_blocks = 1 << tx_size;

  // above
  if (has_eob && xd->mb_to_right_edge < 0) {
    int i;
    const int blocks_wide = num_4x4_blocks_wide_lookup[plane_bsize] +
                            (xd->mb_to_right_edge >> (5 + pd->subsampling_x));
    int above_contexts = tx_size_in_blocks;
    if (above_contexts + aoff > blocks_wide)
      above_contexts = blocks_wide - aoff;

    for (i = 0; i < above_contexts; ++i)
      a[i] = has_eob;
    for (i = above_contexts; i < tx_size_in_blocks; ++i)
      a[i] = 0;
  } else {
    vpx_memset(a, has_eob, sizeof(ENTROPY_CONTEXT) * tx_size_in_blocks);
  }

  // left
  if (has_eob && xd->mb_to_bottom_edge < 0) {
    int i;
    const int blocks_high = num_4x4_blocks_high_lookup[plane_bsize] +
                            (xd->mb_to_bottom_edge >> (5 + pd->subsampling_y));
    int left_contexts = tx_size_in_blocks;
    if (left_contexts + loff > blocks_high)
      left_contexts = blocks_high - loff;

    for (i = 0; i < left_contexts; ++i)
      l[i] = has_eob;
    for (i = left_contexts; i < tx_size_in_blocks; ++i)
      l[i] = 0;
  } else {
    vpx_memset(l, has_eob, sizeof(ENTROPY_CONTEXT) * tx_size_in_blocks);
  }
}

static int get_tx_eob(const struct segmentation *seg, int segment_id,
                      TX_SIZE tx_size) {
  const int eob_max = 16 << (tx_size << 1);
  return vp9_segfeature_active(seg, segment_id, SEG_LVL_SKIP) ? 0 : eob_max;
}

#endif  // VP9_COMMON_VP9_BLOCKD_H_
