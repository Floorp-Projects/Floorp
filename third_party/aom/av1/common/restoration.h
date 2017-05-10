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

#define RESTORATION_TILESIZE_MAX 256
#define RESTORATION_TILEPELS_MAX \
  (RESTORATION_TILESIZE_MAX * RESTORATION_TILESIZE_MAX * 9 / 4)

// 4 32-bit buffers needed for the filter:
// 2 for the restored versions of the frame and
// 2 for each restoration operation
#define SGRPROJ_OUTBUF_SIZE \
  ((RESTORATION_TILESIZE_MAX * 3 / 2) * (RESTORATION_TILESIZE_MAX * 3 / 2 + 16))
#define SGRPROJ_TMPBUF_SIZE                         \
  (RESTORATION_TILEPELS_MAX * 2 * sizeof(int32_t) + \
   SGRPROJ_OUTBUF_SIZE * 2 * sizeof(int32_t))
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

#define MAX_RADIUS 3  // Only 1, 2, 3 allowed
#define MAX_EPS 80    // Max value of eps
#define MAX_NELEM ((2 * MAX_RADIUS + 1) * (2 * MAX_RADIUS + 1))
#define SGRPROJ_MTABLE_BITS 20
#define SGRPROJ_RECIP_BITS 12

#define WIENER_HALFWIN 3
#define WIENER_HALFWIN1 (WIENER_HALFWIN + 1)
#define WIENER_WIN (2 * WIENER_HALFWIN + 1)
#define WIENER_WIN2 ((WIENER_WIN) * (WIENER_WIN))
#define WIENER_TMPBUF_SIZE (0)
#define WIENER_EXTBUF_SIZE (0)

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
  DECLARE_ALIGNED(16, InterpKernel, vfilter);
  DECLARE_ALIGNED(16, InterpKernel, hfilter);
} WienerInfo;

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
  int ep;
  int xqd[2];
} SgrprojInfo;

typedef struct {
  int restoration_tilesize;
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
  nhtiles_ = (width + (tile_width_ >> 1)) / tile_width_;
  nvtiles_ = (height + (tile_height_ >> 1)) / tile_height_;
  if (tile_width) *tile_width = tile_width_;
  if (tile_height) *tile_height = tile_height_;
  if (nhtiles) *nhtiles = nhtiles_;
  if (nvtiles) *nvtiles = nvtiles_;
  return (nhtiles_ * nvtiles_);
}

static INLINE void av1_get_rest_tile_limits(
    int tile_idx, int subtile_idx, int subtile_bits, int nhtiles, int nvtiles,
    int tile_width, int tile_height, int im_width, int im_height, int clamp_h,
    int clamp_v, int *h_start, int *h_end, int *v_start, int *v_end) {
  const int htile_idx = tile_idx % nhtiles;
  const int vtile_idx = tile_idx / nhtiles;
  *h_start = htile_idx * tile_width;
  *v_start = vtile_idx * tile_height;
  *h_end = (htile_idx < nhtiles - 1) ? *h_start + tile_width : im_width;
  *v_end = (vtile_idx < nvtiles - 1) ? *v_start + tile_height : im_height;
  if (subtile_bits) {
    const int num_subtiles_1d = (1 << subtile_bits);
    const int subtile_width = (*h_end - *h_start) >> subtile_bits;
    const int subtile_height = (*v_end - *v_start) >> subtile_bits;
    const int subtile_idx_h = subtile_idx & (num_subtiles_1d - 1);
    const int subtile_idx_v = subtile_idx >> subtile_bits;
    *h_start += subtile_idx_h * subtile_width;
    *v_start += subtile_idx_v * subtile_height;
    *h_end = subtile_idx_h == num_subtiles_1d - 1 ? *h_end
                                                  : *h_start + subtile_width;
    *v_end = subtile_idx_v == num_subtiles_1d - 1 ? *v_end
                                                  : *v_start + subtile_height;
  }
  if (clamp_h) {
    *h_start = AOMMAX(*h_start, clamp_h);
    *h_end = AOMMIN(*h_end, im_width - clamp_h);
  }
  if (clamp_v) {
    *v_start = AOMMAX(*v_start, clamp_v);
    *v_end = AOMMIN(*v_end, im_height - clamp_v);
  }
}

extern const sgr_params_type sgr_params[SGRPROJ_PARAMS];
extern int sgrproj_mtable[MAX_EPS][MAX_NELEM];
extern const int32_t x_by_xplus1[256];
extern const int32_t one_by_x[MAX_NELEM];

int av1_alloc_restoration_struct(struct AV1Common *cm,
                                 RestorationInfo *rst_info, int width,
                                 int height);
void av1_free_restoration_struct(RestorationInfo *rst_info);

void extend_frame(uint8_t *data, int width, int height, int stride);
#if CONFIG_HIGHBITDEPTH
void extend_frame_highbd(uint16_t *data, int width, int height, int stride);
#endif  // CONFIG_HIGHBITDEPTH
void decode_xq(int *xqd, int *xq);
void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                                RestorationInfo *rsi, int components_pattern,
                                int partial_frame, YV12_BUFFER_CONFIG *dst);
void av1_loop_restoration_precal();
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_RESTORATION_H_
