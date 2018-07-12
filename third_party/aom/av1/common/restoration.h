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

#ifndef AV1_COMMON_RESTORATION_H_
#define AV1_COMMON_RESTORATION_H_

#include "aom_ports/mem.h"
#include "./aom_config.h"

#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLIP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define RINT(x) ((x) < 0 ? (int)((x)-0.5) : (int)((x) + 0.5))

#define RESTORATION_PROC_UNIT_SIZE 64

#if CONFIG_STRIPED_LOOP_RESTORATION
// Filter tile grid offset upwards compared to the superblock grid
#define RESTORATION_TILE_OFFSET 8
#endif

#if CONFIG_STRIPED_LOOP_RESTORATION
#define SGRPROJ_BORDER_VERT 2  // Vertical border used for Sgr
#else
#define SGRPROJ_BORDER_VERT 1  // Vertical border used for Sgr
#endif
#define SGRPROJ_BORDER_HORZ 2  // Horizontal border used for Sgr

#if CONFIG_STRIPED_LOOP_RESTORATION
#define WIENER_BORDER_VERT 2  // Vertical border used for Wiener
#else
#define WIENER_BORDER_VERT 1  // Vertical border used for Wiener
#endif
#define WIENER_HALFWIN 3
#define WIENER_BORDER_HORZ (WIENER_HALFWIN)  // Horizontal border for Wiener

// RESTORATION_BORDER_VERT determines line buffer requirement for LR.
// Should be set at the max of SGRPROJ_BORDER_VERT and WIENER_BORDER_VERT.
// Note the line buffer needed is twice the value of this macro.
#if SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT
#define RESTORATION_BORDER_VERT (SGRPROJ_BORDER_VERT)
#else
#define RESTORATION_BORDER_VERT (WIENER_BORDER_VERT)
#endif  // SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT

#if SGRPROJ_BORDER_HORZ >= WIENER_BORDER_HORZ
#define RESTORATION_BORDER_HORZ (SGRPROJ_BORDER_HORZ)
#else
#define RESTORATION_BORDER_HORZ (WIENER_BORDER_HORZ)
#endif  // SGRPROJ_BORDER_VERT >= WIENER_BORDER_VERT

#if CONFIG_STRIPED_LOOP_RESTORATION
// Additional pixels to the left and right in above/below buffers
// It is RESTORATION_BORDER_HORZ rounded up to get nicer buffer alignment
#define RESTORATION_EXTRA_HORZ 4
#endif

// Pad up to 20 more (may be much less is needed)
#define RESTORATION_PADDING 20
#define RESTORATION_PROC_UNIT_PELS                             \
  ((RESTORATION_PROC_UNIT_SIZE + RESTORATION_BORDER_HORZ * 2 + \
    RESTORATION_PADDING) *                                     \
   (RESTORATION_PROC_UNIT_SIZE + RESTORATION_BORDER_VERT * 2 + \
    RESTORATION_PADDING))

#define RESTORATION_TILESIZE_MAX 256
#if CONFIG_STRIPED_LOOP_RESTORATION
#define RESTORATION_TILEPELS_HORZ_MAX \
  (RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_HORZ + 16)
#define RESTORATION_TILEPELS_VERT_MAX                                \
  ((RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_VERT + \
    RESTORATION_TILE_OFFSET))
#define RESTORATION_TILEPELS_MAX \
  (RESTORATION_TILEPELS_HORZ_MAX * RESTORATION_TILEPELS_VERT_MAX)
#else
#define RESTORATION_TILEPELS_MAX                                           \
  ((RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_HORZ + 16) * \
   (RESTORATION_TILESIZE_MAX * 3 / 2 + 2 * RESTORATION_BORDER_VERT))
#endif

// Two 32-bit buffers needed for the restored versions from two filters
// TODO(debargha, rupert): Refactor to not need the large tilesize to be stored
// on the decoder side.
#define SGRPROJ_TMPBUF_SIZE (RESTORATION_TILEPELS_MAX * 2 * sizeof(int32_t))

#define SGRPROJ_EXTBUF_SIZE (0)
#define SGRPROJ_PARAMS_BITS 4
#define SGRPROJ_PARAMS (1 << SGRPROJ_PARAMS_BITS)
#define USE_HIGHPASS_IN_SGRPROJ 0

// Precision bits for projection
#define SGRPROJ_PRJ_BITS 7
// Restoration precision bits generated higher than source before projection
#define SGRPROJ_RST_BITS 4
// Internal precision bits for core selfguided_restoration
#define SGRPROJ_SGR_BITS 8
#define SGRPROJ_SGR (1 << SGRPROJ_SGR_BITS)

#if USE_HIGHPASS_IN_SGRPROJ
#define SGRPROJ_PRJ_MIN0 (-(1 << SGRPROJ_PRJ_BITS) / 8)
#define SGRPROJ_PRJ_MAX0 (SGRPROJ_PRJ_MIN0 + (1 << SGRPROJ_PRJ_BITS) - 1)
#define SGRPROJ_PRJ_MIN1 (-(1 << SGRPROJ_PRJ_BITS) / 2)
#define SGRPROJ_PRJ_MAX1 (SGRPROJ_PRJ_MIN1 + (1 << SGRPROJ_PRJ_BITS) - 1)
#else
#define SGRPROJ_PRJ_MIN0 (-(1 << SGRPROJ_PRJ_BITS) * 3 / 4)
#define SGRPROJ_PRJ_MAX0 (SGRPROJ_PRJ_MIN0 + (1 << SGRPROJ_PRJ_BITS) - 1)
#define SGRPROJ_PRJ_MIN1 (-(1 << SGRPROJ_PRJ_BITS) / 4)
#define SGRPROJ_PRJ_MAX1 (SGRPROJ_PRJ_MIN1 + (1 << SGRPROJ_PRJ_BITS) - 1)
#endif  // USE_HIGHPASS_IN_SGRPROJ

#define SGRPROJ_PRJ_SUBEXP_K 4

#define SGRPROJ_BITS (SGRPROJ_PRJ_BITS * 2 + SGRPROJ_PARAMS_BITS)

#define MAX_RADIUS 2  // Only 1, 2, 3 allowed
#define MAX_EPS 80    // Max value of eps
#define MAX_NELEM ((2 * MAX_RADIUS + 1) * (2 * MAX_RADIUS + 1))
#define SGRPROJ_MTABLE_BITS 20
#define SGRPROJ_RECIP_BITS 12

#define WIENER_HALFWIN1 (WIENER_HALFWIN + 1)
#define WIENER_WIN (2 * WIENER_HALFWIN + 1)
#define WIENER_WIN2 ((WIENER_WIN) * (WIENER_WIN))
#define WIENER_TMPBUF_SIZE (0)
#define WIENER_EXTBUF_SIZE (0)

// If WIENER_WIN_CHROMA == WIENER_WIN - 2, that implies 5x5 filters are used for
// chroma. To use 7x7 for chroma set WIENER_WIN_CHROMA to WIENER_WIN.
#define WIENER_WIN_CHROMA (WIENER_WIN - 2)

#define WIENER_FILT_PREC_BITS 7
#define WIENER_FILT_STEP (1 << WIENER_FILT_PREC_BITS)

// Whether to use high intermediate precision filtering
#define USE_WIENER_HIGH_INTERMEDIATE_PRECISION 1

// Central values for the taps
#define WIENER_FILT_TAP0_MIDV (3)
#define WIENER_FILT_TAP1_MIDV (-7)
#define WIENER_FILT_TAP2_MIDV (15)
#define WIENER_FILT_TAP3_MIDV                           \
  (WIENER_FILT_STEP -                                   \
   2 * (WIENER_FILT_TAP0_MIDV + WIENER_FILT_TAP1_MIDV + \
        WIENER_FILT_TAP2_MIDV))

#define WIENER_FILT_TAP0_BITS 4
#define WIENER_FILT_TAP1_BITS 5
#define WIENER_FILT_TAP2_BITS 6

#define WIENER_FILT_BITS \
  ((WIENER_FILT_TAP0_BITS + WIENER_FILT_TAP1_BITS + WIENER_FILT_TAP2_BITS) * 2)

#define WIENER_FILT_TAP0_MINV \
  (WIENER_FILT_TAP0_MIDV - (1 << WIENER_FILT_TAP0_BITS) / 2)
#define WIENER_FILT_TAP1_MINV \
  (WIENER_FILT_TAP1_MIDV - (1 << WIENER_FILT_TAP1_BITS) / 2)
#define WIENER_FILT_TAP2_MINV \
  (WIENER_FILT_TAP2_MIDV - (1 << WIENER_FILT_TAP2_BITS) / 2)

#define WIENER_FILT_TAP0_MAXV \
  (WIENER_FILT_TAP0_MIDV - 1 + (1 << WIENER_FILT_TAP0_BITS) / 2)
#define WIENER_FILT_TAP1_MAXV \
  (WIENER_FILT_TAP1_MIDV - 1 + (1 << WIENER_FILT_TAP1_BITS) / 2)
#define WIENER_FILT_TAP2_MAXV \
  (WIENER_FILT_TAP2_MIDV - 1 + (1 << WIENER_FILT_TAP2_BITS) / 2)

#define WIENER_FILT_TAP0_SUBEXP_K 1
#define WIENER_FILT_TAP1_SUBEXP_K 2
#define WIENER_FILT_TAP2_SUBEXP_K 3

// Max of SGRPROJ_TMPBUF_SIZE, DOMAINTXFMRF_TMPBUF_SIZE, WIENER_TMPBUF_SIZE
#define RESTORATION_TMPBUF_SIZE (SGRPROJ_TMPBUF_SIZE)

// Max of SGRPROJ_EXTBUF_SIZE, WIENER_EXTBUF_SIZE
#define RESTORATION_EXTBUF_SIZE (WIENER_EXTBUF_SIZE)

// Check the assumptions of the existing code
#if SUBPEL_TAPS != WIENER_WIN + 1
#error "Wiener filter currently only works if SUBPEL_TAPS == WIENER_WIN + 1"
#endif
#if WIENER_FILT_PREC_BITS != 7
#error "Wiener filter currently only works if WIENER_FILT_PREC_BITS == 7"
#endif

typedef struct {
#if USE_HIGHPASS_IN_SGRPROJ
  int corner;
  int edge;
#else
  int r1;
  int e1;
#endif  // USE_HIGHPASS_IN_SGRPROJ
  int r2;
  int e2;
} sgr_params_type;

typedef struct {
  int restoration_tilesize;
  int procunit_width, procunit_height;
  RestorationType frame_restoration_type;
  RestorationType *restoration_type;
  // Wiener filter
  WienerInfo *wiener_info;
  // Selfguided proj filter
  SgrprojInfo *sgrproj_info;
} RestorationInfo;

typedef struct {
  RestorationInfo *rsi;
  int keyframe;
  int ntiles;
  int tile_width, tile_height;
  int nhtiles, nvtiles;
  int32_t *tmpbuf;
#if CONFIG_STRIPED_LOOP_RESTORATION
  int component;
  int subsampling_y;
  uint8_t *stripe_boundary_above[MAX_MB_PLANE];
  uint8_t *stripe_boundary_below[MAX_MB_PLANE];
  int stripe_boundary_stride[MAX_MB_PLANE];
  // Temporary buffers to save/restore 2 lines above/below the restoration
  // stripe
  // Allow for filter margin to left and right
  uint16_t
      tmp_save_above[2][RESTORATION_TILESIZE_MAX + 2 * RESTORATION_EXTRA_HORZ];
  uint16_t
      tmp_save_below[2][RESTORATION_TILESIZE_MAX + 2 * RESTORATION_EXTRA_HORZ];
#endif
} RestorationInternal;

static INLINE void set_default_sgrproj(SgrprojInfo *sgrproj_info) {
  sgrproj_info->xqd[0] = (SGRPROJ_PRJ_MIN0 + SGRPROJ_PRJ_MAX0) / 2;
  sgrproj_info->xqd[1] = (SGRPROJ_PRJ_MIN1 + SGRPROJ_PRJ_MAX1) / 2;
}

static INLINE void set_default_wiener(WienerInfo *wiener_info) {
  wiener_info->vfilter[0] = wiener_info->hfilter[0] = WIENER_FILT_TAP0_MIDV;
  wiener_info->vfilter[1] = wiener_info->hfilter[1] = WIENER_FILT_TAP1_MIDV;
  wiener_info->vfilter[2] = wiener_info->hfilter[2] = WIENER_FILT_TAP2_MIDV;
  wiener_info->vfilter[WIENER_HALFWIN] = wiener_info->hfilter[WIENER_HALFWIN] =
      -2 *
      (WIENER_FILT_TAP2_MIDV + WIENER_FILT_TAP1_MIDV + WIENER_FILT_TAP0_MIDV);
  wiener_info->vfilter[4] = wiener_info->hfilter[4] = WIENER_FILT_TAP2_MIDV;
  wiener_info->vfilter[5] = wiener_info->hfilter[5] = WIENER_FILT_TAP1_MIDV;
  wiener_info->vfilter[6] = wiener_info->hfilter[6] = WIENER_FILT_TAP0_MIDV;
}

static INLINE int av1_get_rest_ntiles(int width, int height, int tilesize,
                                      int *tile_width, int *tile_height,
                                      int *nhtiles, int *nvtiles) {
  int nhtiles_, nvtiles_;
  int tile_width_, tile_height_;
  tile_width_ = (tilesize < 0) ? width : AOMMIN(tilesize, width);
  tile_height_ = (tilesize < 0) ? height : AOMMIN(tilesize, height);
  assert(tile_width_ > 0 && tile_height_ > 0);

  nhtiles_ = (width + (tile_width_ >> 1)) / tile_width_;
  nvtiles_ = (height + (tile_height_ >> 1)) / tile_height_;
  if (tile_width) *tile_width = tile_width_;
  if (tile_height) *tile_height = tile_height_;
  if (nhtiles) *nhtiles = nhtiles_;
  if (nvtiles) *nvtiles = nvtiles_;
  return (nhtiles_ * nvtiles_);
}

typedef struct { int h_start, h_end, v_start, v_end; } RestorationTileLimits;

static INLINE RestorationTileLimits
av1_get_rest_tile_limits(int tile_idx, int nhtiles, int nvtiles, int tile_width,
                         int tile_height, int im_width,
#if CONFIG_STRIPED_LOOP_RESTORATION
                         int im_height, int subsampling_y) {
#else
                         int im_height) {
#endif
  const int htile_idx = tile_idx % nhtiles;
  const int vtile_idx = tile_idx / nhtiles;
  RestorationTileLimits limits;
  limits.h_start = htile_idx * tile_width;
  limits.v_start = vtile_idx * tile_height;
  limits.h_end =
      (htile_idx < nhtiles - 1) ? limits.h_start + tile_width : im_width;
  limits.v_end =
      (vtile_idx < nvtiles - 1) ? limits.v_start + tile_height : im_height;
#if CONFIG_STRIPED_LOOP_RESTORATION
  // Offset the tile upwards to align with the restoration processing stripe
  limits.v_start -= RESTORATION_TILE_OFFSET >> subsampling_y;
  if (limits.v_start < 0) limits.v_start = 0;
  if (limits.v_end < im_height)
    limits.v_end -= RESTORATION_TILE_OFFSET >> subsampling_y;
#endif
  return limits;
}

extern const sgr_params_type sgr_params[SGRPROJ_PARAMS];
extern int sgrproj_mtable[MAX_EPS][MAX_NELEM];
extern const int32_t x_by_xplus1[256];
extern const int32_t one_by_x[MAX_NELEM];

int av1_alloc_restoration_struct(struct AV1Common *cm,
                                 RestorationInfo *rst_info, int width,
                                 int height);
void av1_free_restoration_struct(RestorationInfo *rst_info);

void extend_frame(uint8_t *data, int width, int height, int stride,
                  int border_horz, int border_vert);
#if CONFIG_HIGHBITDEPTH
void extend_frame_highbd(uint16_t *data, int width, int height, int stride,
                         int border_horz, int border_vert);
#endif  // CONFIG_HIGHBITDEPTH
void decode_xq(int *xqd, int *xq);
void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                                RestorationInfo *rsi, int components_pattern,
                                int partial_frame, YV12_BUFFER_CONFIG *dst);
void av1_loop_restoration_precal();

// Return 1 iff the block at mi_row, mi_col with size bsize is a
// top-level superblock containing the top-left corner of at least one
// loop restoration tile.
//
// If the block is a top-level superblock, the function writes to
// *rcol0, *rcol1, *rrow0, *rrow1. The rectangle of indices given by
// [*rcol0, *rcol1) x [*rrow0, *rrow1) will point at the set of rtiles
// whose top left corners lie in the superblock. Note that the set is
// only nonempty if *rcol0 < *rcol1 and *rrow0 < *rrow1.
int av1_loop_restoration_corners_in_sb(const struct AV1Common *cm, int plane,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int *rcol0, int *rcol1, int *rrow0,
                                       int *rrow1, int *nhtiles);

void av1_loop_restoration_save_boundary_lines(YV12_BUFFER_CONFIG *frame,
                                              struct AV1Common *cm);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RESTORATION_H_
