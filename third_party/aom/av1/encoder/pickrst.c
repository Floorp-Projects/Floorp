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

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "./aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_dsp/psnr.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"

#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"
#include "av1/common/restoration.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/mathutils.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"

// When set to RESTORE_WIENER or RESTORE_SGRPROJ only those are allowed.
// When set to RESTORE_TYPES we allow switchable.
static const RestorationType force_restore_type = RESTORE_TYPES;

// Number of Wiener iterations
#define NUM_WIENER_ITERS 5

typedef double (*search_restore_type)(const YV12_BUFFER_CONFIG *src,
                                      AV1_COMP *cpi, int partial_frame,
                                      int plane, RestorationInfo *info,
                                      RestorationType *rest_level,
                                      int64_t *best_tile_cost,
                                      YV12_BUFFER_CONFIG *dst_frame);

const int frame_level_restore_bits[RESTORE_TYPES] = { 2, 2, 2, 2 };

static int64_t sse_restoration_tile(const YV12_BUFFER_CONFIG *src,
                                    const YV12_BUFFER_CONFIG *dst,
                                    const AV1_COMMON *cm, int h_start,
                                    int width, int v_start, int height,
                                    int components_pattern) {
  int64_t filt_err = 0;
  (void)cm;
  // Y and UV components cannot be mixed
  assert(components_pattern == 1 || components_pattern == 2 ||
         components_pattern == 4 || components_pattern == 6);
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    if ((components_pattern >> AOM_PLANE_Y) & 1) {
      filt_err +=
          aom_highbd_get_y_sse_part(src, dst, h_start, width, v_start, height);
    }
    if ((components_pattern >> AOM_PLANE_U) & 1) {
      filt_err +=
          aom_highbd_get_u_sse_part(src, dst, h_start, width, v_start, height);
    }
    if ((components_pattern >> AOM_PLANE_V) & 1) {
      filt_err +=
          aom_highbd_get_v_sse_part(src, dst, h_start, width, v_start, height);
    }
    return filt_err;
  }
#endif  // CONFIG_HIGHBITDEPTH
  if ((components_pattern >> AOM_PLANE_Y) & 1) {
    filt_err += aom_get_y_sse_part(src, dst, h_start, width, v_start, height);
  }
  if ((components_pattern >> AOM_PLANE_U) & 1) {
    filt_err += aom_get_u_sse_part(src, dst, h_start, width, v_start, height);
  }
  if ((components_pattern >> AOM_PLANE_V) & 1) {
    filt_err += aom_get_v_sse_part(src, dst, h_start, width, v_start, height);
  }
  return filt_err;
}

static int64_t sse_restoration_frame(AV1_COMMON *const cm,
                                     const YV12_BUFFER_CONFIG *src,
                                     const YV12_BUFFER_CONFIG *dst,
                                     int components_pattern) {
  int64_t filt_err = 0;
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    if ((components_pattern >> AOM_PLANE_Y) & 1) {
      filt_err += aom_highbd_get_y_sse(src, dst);
    }
    if ((components_pattern >> AOM_PLANE_U) & 1) {
      filt_err += aom_highbd_get_u_sse(src, dst);
    }
    if ((components_pattern >> AOM_PLANE_V) & 1) {
      filt_err += aom_highbd_get_v_sse(src, dst);
    }
    return filt_err;
  }
#else
  (void)cm;
#endif  // CONFIG_HIGHBITDEPTH
  if ((components_pattern >> AOM_PLANE_Y) & 1) {
    filt_err = aom_get_y_sse(src, dst);
  }
  if ((components_pattern >> AOM_PLANE_U) & 1) {
    filt_err += aom_get_u_sse(src, dst);
  }
  if ((components_pattern >> AOM_PLANE_V) & 1) {
    filt_err += aom_get_v_sse(src, dst);
  }
  return filt_err;
}

static int64_t try_restoration_tile(const YV12_BUFFER_CONFIG *src,
                                    AV1_COMP *const cpi, RestorationInfo *rsi,
                                    int components_pattern, int partial_frame,
                                    int tile_idx,
                                    YV12_BUFFER_CONFIG *dst_frame) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;
  int tile_width, tile_height, nhtiles, nvtiles;
  int ntiles, width, height;

  // Y and UV components cannot be mixed
  assert(components_pattern == 1 || components_pattern == 2 ||
         components_pattern == 4 || components_pattern == 6);

  if (components_pattern == 1) {  // Y only
    width = src->y_crop_width;
    height = src->y_crop_height;
  } else {  // Color
    width = src->uv_crop_width;
    height = src->uv_crop_height;
  }
  ntiles = av1_get_rest_ntiles(
      width, height, cm->rst_info[components_pattern > 1].restoration_tilesize,
      &tile_width, &tile_height, &nhtiles, &nvtiles);
  (void)ntiles;

  av1_loop_restoration_frame(cm->frame_to_show, cm, rsi, components_pattern,
                             partial_frame, dst_frame);
  RestorationTileLimits limits = av1_get_rest_tile_limits(
      tile_idx, nhtiles, nvtiles, tile_width, tile_height, width,
#if CONFIG_STRIPED_LOOP_RESTORATION
      height, components_pattern > 1 ? cm->subsampling_y : 0);
#else
      height);
#endif
  filt_err = sse_restoration_tile(
      src, dst_frame, cm, limits.h_start, limits.h_end - limits.h_start,
      limits.v_start, limits.v_end - limits.v_start, components_pattern);

  return filt_err;
}

static int64_t try_restoration_frame(const YV12_BUFFER_CONFIG *src,
                                     AV1_COMP *const cpi, RestorationInfo *rsi,
                                     int components_pattern, int partial_frame,
                                     YV12_BUFFER_CONFIG *dst_frame) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;
  av1_loop_restoration_frame(cm->frame_to_show, cm, rsi, components_pattern,
                             partial_frame, dst_frame);
  filt_err = sse_restoration_frame(cm, src, dst_frame, components_pattern);
  return filt_err;
}

static int64_t get_pixel_proj_error(const uint8_t *src8, int width, int height,
                                    int src_stride, const uint8_t *dat8,
                                    int dat_stride, int use_highbitdepth,
                                    int32_t *flt1, int flt1_stride,
                                    int32_t *flt2, int flt2_stride, int *xqd) {
  int i, j;
  int64_t err = 0;
  int xq[2];
  decode_xq(xqd, xq);
  if (!use_highbitdepth) {
    const uint8_t *src = src8;
    const uint8_t *dat = dat8;
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t u =
            (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
        const int32_t f1 = (int32_t)flt1[i * flt1_stride + j] - u;
        const int32_t f2 = (int32_t)flt2[i * flt2_stride + j] - u;
        const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) -
            src[i * src_stride + j];
        err += e * e;
      }
    }
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t u =
            (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
        const int32_t f1 = (int32_t)flt1[i * flt1_stride + j] - u;
        const int32_t f2 = (int32_t)flt2[i * flt2_stride + j] - u;
        const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) -
            src[i * src_stride + j];
        err += e * e;
      }
    }
  }
  return err;
}

#define USE_SGRPROJ_REFINEMENT_SEARCH 1
static int64_t finer_search_pixel_proj_error(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int use_highbitdepth, int32_t *flt1,
    int flt1_stride, int32_t *flt2, int flt2_stride, int start_step, int *xqd) {
  int64_t err = get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                     dat_stride, use_highbitdepth, flt1,
                                     flt1_stride, flt2, flt2_stride, xqd);
  (void)start_step;
#if USE_SGRPROJ_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MIN1 };
  int tap_max[] = { SGRPROJ_PRJ_MAX0, SGRPROJ_PRJ_MAX1 };
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = 0; p < 2; ++p) {
      int skip = 0;
      do {
        if (xqd[p] - s >= tap_min[p]) {
          xqd[p] -= s;
          err2 = get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                      dat_stride, use_highbitdepth, flt1,
                                      flt1_stride, flt2, flt2_stride, xqd);
          if (err2 > err) {
            xqd[p] += s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (xqd[p] + s <= tap_max[p]) {
          xqd[p] += s;
          err2 = get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                      dat_stride, use_highbitdepth, flt1,
                                      flt1_stride, flt2, flt2_stride, xqd);
          if (err2 > err) {
            xqd[p] -= s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
  }
#endif  // USE_SGRPROJ_REFINEMENT_SEARCH
  return err;
}

static void get_proj_subspace(const uint8_t *src8, int width, int height,
                              int src_stride, uint8_t *dat8, int dat_stride,
                              int use_highbitdepth, int32_t *flt1,
                              int flt1_stride, int32_t *flt2, int flt2_stride,
                              int *xq) {
  int i, j;
  double H[2][2] = { { 0, 0 }, { 0, 0 } };
  double C[2] = { 0, 0 };
  double Det;
  double x[2];
  const int size = width * height;

  aom_clear_system_state();

  // Default
  xq[0] = 0;
  xq[1] = 0;
  if (!use_highbitdepth) {
    const uint8_t *src = src8;
    const uint8_t *dat = dat8;
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const double u = (double)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
        const double s =
            (double)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
        const double f1 = (double)flt1[i * flt1_stride + j] - u;
        const double f2 = (double)flt2[i * flt2_stride + j] - u;
        H[0][0] += f1 * f1;
        H[1][1] += f2 * f2;
        H[0][1] += f1 * f2;
        C[0] += f1 * s;
        C[1] += f2 * s;
      }
    }
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const double u = (double)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
        const double s =
            (double)(src[i * src_stride + j] << SGRPROJ_RST_BITS) - u;
        const double f1 = (double)flt1[i * flt1_stride + j] - u;
        const double f2 = (double)flt2[i * flt2_stride + j] - u;
        H[0][0] += f1 * f1;
        H[1][1] += f2 * f2;
        H[0][1] += f1 * f2;
        C[0] += f1 * s;
        C[1] += f2 * s;
      }
    }
  }
  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
  Det = (H[0][0] * H[1][1] - H[0][1] * H[1][0]);
  if (Det < 1e-8) return;  // ill-posed, return default values
  x[0] = (H[1][1] * C[0] - H[0][1] * C[1]) / Det;
  x[1] = (H[0][0] * C[1] - H[1][0] * C[0]) / Det;
  xq[0] = (int)rint(x[0] * (1 << SGRPROJ_PRJ_BITS));
  xq[1] = (int)rint(x[1] * (1 << SGRPROJ_PRJ_BITS));
}

void encode_xq(int *xq, int *xqd) {
  xqd[0] = xq[0];
  xqd[0] = clamp(xqd[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
  xqd[1] = (1 << SGRPROJ_PRJ_BITS) - xqd[0] - xq[1];
  xqd[1] = clamp(xqd[1], SGRPROJ_PRJ_MIN1, SGRPROJ_PRJ_MAX1);
}

static void search_selfguided_restoration(uint8_t *dat8, int width, int height,
                                          int dat_stride, const uint8_t *src8,
                                          int src_stride, int use_highbitdepth,
                                          int bit_depth, int pu_width,
                                          int pu_height, int *eps, int *xqd,
                                          int32_t *rstbuf) {
  int32_t *flt1 = rstbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int ep, bestep = 0;
  int64_t err, besterr = -1;
  int exqd[2], bestxqd[2] = { 0, 0 };
  int flt1_stride = ((width + 7) & ~7) + 8;
  int flt2_stride = ((width + 7) & ~7) + 8;
  assert(pu_width == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_width == RESTORATION_PROC_UNIT_SIZE);
  assert(pu_height == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_height == RESTORATION_PROC_UNIT_SIZE);
#if !CONFIG_HIGHBITDEPTH
  (void)bit_depth;
#endif

  for (ep = 0; ep < SGRPROJ_PARAMS; ep++) {
    int exq[2];
#if CONFIG_HIGHBITDEPTH
    if (use_highbitdepth) {
      uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
      for (int i = 0; i < height; i += pu_height)
        for (int j = 0; j < width; j += pu_width) {
          const int w = AOMMIN(pu_width, width - j);
          const int h = AOMMIN(pu_height, height - i);
          uint16_t *dat_p = dat + i * dat_stride + j;
          int32_t *flt1_p = flt1 + i * flt1_stride + j;
          int32_t *flt2_p = flt2 + i * flt2_stride + j;
#if USE_HIGHPASS_IN_SGRPROJ
          av1_highpass_filter_highbd(dat_p, w, h, dat_stride, flt1_p,
                                     flt1_stride, sgr_params[ep].corner,
                                     sgr_params[ep].edge);
#else
          av1_selfguided_restoration_highbd(
              dat_p, w, h, dat_stride, flt1_p, flt1_stride, bit_depth,
              sgr_params[ep].r1, sgr_params[ep].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
          av1_selfguided_restoration_highbd(
              dat_p, w, h, dat_stride, flt2_p, flt2_stride, bit_depth,
              sgr_params[ep].r2, sgr_params[ep].e2);
        }
    } else {
#endif
      for (int i = 0; i < height; i += pu_height)
        for (int j = 0; j < width; j += pu_width) {
          const int w = AOMMIN(pu_width, width - j);
          const int h = AOMMIN(pu_height, height - i);
          uint8_t *dat_p = dat8 + i * dat_stride + j;
          int32_t *flt1_p = flt1 + i * flt1_stride + j;
          int32_t *flt2_p = flt2 + i * flt2_stride + j;
#if USE_HIGHPASS_IN_SGRPROJ
          av1_highpass_filter(dat_p, w, h, dat_stride, flt1_p, flt1_stride,
                              sgr_params[ep].corner, sgr_params[ep].edge);
#else
        av1_selfguided_restoration(dat_p, w, h, dat_stride, flt1_p, flt1_stride,
                                   sgr_params[ep].r1, sgr_params[ep].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
          av1_selfguided_restoration(dat_p, w, h, dat_stride, flt2_p,
                                     flt2_stride, sgr_params[ep].r2,
                                     sgr_params[ep].e2);
        }
#if CONFIG_HIGHBITDEPTH
    }
#endif
    aom_clear_system_state();
    get_proj_subspace(src8, width, height, src_stride, dat8, dat_stride,
                      use_highbitdepth, flt1, flt1_stride, flt2, flt2_stride,
                      exq);
    aom_clear_system_state();
    encode_xq(exq, exqd);
    err = finer_search_pixel_proj_error(
        src8, width, height, src_stride, dat8, dat_stride, use_highbitdepth,
        flt1, flt1_stride, flt2, flt2_stride, 2, exqd);
    if (besterr == -1 || err < besterr) {
      bestep = ep;
      besterr = err;
      bestxqd[0] = exqd[0];
      bestxqd[1] = exqd[1];
    }
  }
  *eps = bestep;
  xqd[0] = bestxqd[0];
  xqd[1] = bestxqd[1];
}

static int count_sgrproj_bits(SgrprojInfo *sgrproj_info,
                              SgrprojInfo *ref_sgrproj_info) {
  int bits = SGRPROJ_PARAMS_BITS;
  bits += aom_count_primitive_refsubexpfin(
      SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
      ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
      sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
  bits += aom_count_primitive_refsubexpfin(
      SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
      ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
      sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  return bits;
}

struct rest_search_ctxt {
  const YV12_BUFFER_CONFIG *src;
  AV1_COMP *cpi;
  uint8_t *dgd_buffer;
  const uint8_t *src_buffer;
  int dgd_stride;
  int src_stride;
  int partial_frame;
  RestorationInfo *info;
  RestorationType *type;
  int64_t *best_tile_cost;
  int plane;
  int plane_width;
  int plane_height;
  int nrtiles_x;
  int nrtiles_y;
  YV12_BUFFER_CONFIG *dst_frame;
};

// Fill in ctxt. Returns the number of restoration tiles for this plane
static INLINE int init_rest_search_ctxt(
    const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi, int partial_frame, int plane,
    RestorationInfo *info, RestorationType *type, int64_t *best_tile_cost,
    YV12_BUFFER_CONFIG *dst_frame, struct rest_search_ctxt *ctxt) {
  AV1_COMMON *const cm = &cpi->common;
  ctxt->src = src;
  ctxt->cpi = cpi;
  ctxt->partial_frame = partial_frame;
  ctxt->info = info;
  ctxt->type = type;
  ctxt->best_tile_cost = best_tile_cost;
  ctxt->plane = plane;
  ctxt->dst_frame = dst_frame;

  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  if (plane == AOM_PLANE_Y) {
    ctxt->plane_width = src->y_crop_width;
    ctxt->plane_height = src->y_crop_height;
    ctxt->src_buffer = src->y_buffer;
    ctxt->src_stride = src->y_stride;
    ctxt->dgd_buffer = dgd->y_buffer;
    ctxt->dgd_stride = dgd->y_stride;
    assert(ctxt->plane_width == dgd->y_crop_width);
    assert(ctxt->plane_height == dgd->y_crop_height);
    assert(ctxt->plane_width == src->y_crop_width);
    assert(ctxt->plane_height == src->y_crop_height);
  } else {
    ctxt->plane_width = src->uv_crop_width;
    ctxt->plane_height = src->uv_crop_height;
    ctxt->src_stride = src->uv_stride;
    ctxt->dgd_stride = dgd->uv_stride;
    ctxt->src_buffer = plane == AOM_PLANE_U ? src->u_buffer : src->v_buffer;
    ctxt->dgd_buffer = plane == AOM_PLANE_U ? dgd->u_buffer : dgd->v_buffer;
    assert(ctxt->plane_width == dgd->uv_crop_width);
    assert(ctxt->plane_height == dgd->uv_crop_height);
  }

  return av1_get_rest_ntiles(ctxt->plane_width, ctxt->plane_height,
                             cm->rst_info[plane].restoration_tilesize, NULL,
                             NULL, &ctxt->nrtiles_x, &ctxt->nrtiles_y);
}

typedef void (*rtile_visitor_t)(const struct rest_search_ctxt *search_ctxt,
                                int rtile_idx,
                                const RestorationTileLimits *limits, void *arg);

static void foreach_rtile_in_tile(const struct rest_search_ctxt *ctxt,
                                  int tile_row, int tile_col,
                                  rtile_visitor_t fun, void *arg) {
  const AV1_COMMON *const cm = &ctxt->cpi->common;
  const RestorationInfo *rsi = ctxt->cpi->rst_search;
  TileInfo tile_info;

  av1_tile_set_row(&tile_info, cm, tile_row);
  av1_tile_set_col(&tile_info, cm, tile_col);

  int tile_col_start = tile_info.mi_col_start * MI_SIZE;
  int tile_col_end = tile_info.mi_col_end * MI_SIZE;
  int tile_row_start = tile_info.mi_row_start * MI_SIZE;
  int tile_row_end = tile_info.mi_row_end * MI_SIZE;
  if (ctxt->plane > 0) {
    tile_col_start = ROUND_POWER_OF_TWO(tile_col_start, cm->subsampling_x);
    tile_col_end = ROUND_POWER_OF_TWO(tile_col_end, cm->subsampling_x);
    tile_row_start = ROUND_POWER_OF_TWO(tile_row_start, cm->subsampling_y);
    tile_row_end = ROUND_POWER_OF_TWO(tile_row_end, cm->subsampling_y);
  }

#if CONFIG_FRAME_SUPERRES
  // If upscaling is enabled, the tile limits need scaling to match the
  // upscaled frame where the restoration tiles live. To do this, scale up the
  // top-left and bottom-right of the tile.
  if (!av1_superres_unscaled(cm)) {
    av1_calculate_unscaled_superres_size(&tile_col_start, &tile_row_start,
                                         cm->superres_scale_denominator);
    av1_calculate_unscaled_superres_size(&tile_col_end, &tile_row_end,
                                         cm->superres_scale_denominator);
    // Make sure we don't fall off the bottom-right of the frame.
    tile_col_end = AOMMIN(tile_col_end, ctxt->plane_width);
    tile_row_end = AOMMIN(tile_row_end, ctxt->plane_height);
  }
#endif  // CONFIG_FRAME_SUPERRES

  const int rtile_size = rsi->restoration_tilesize;
  const int rtile_col0 = (tile_col_start + rtile_size - 1) / rtile_size;
  const int rtile_col1 =
      AOMMIN((tile_col_end + rtile_size - 1) / rtile_size, ctxt->nrtiles_x);
  const int rtile_row0 = (tile_row_start + rtile_size - 1) / rtile_size;
  const int rtile_row1 =
      AOMMIN((tile_row_end + rtile_size - 1) / rtile_size, ctxt->nrtiles_y);

  const int rtile_width = AOMMIN(tile_col_end - tile_col_start, rtile_size);
  const int rtile_height = AOMMIN(tile_row_end - tile_row_start, rtile_size);

  for (int rtile_row = rtile_row0; rtile_row < rtile_row1; ++rtile_row) {
    for (int rtile_col = rtile_col0; rtile_col < rtile_col1; ++rtile_col) {
      const int rtile_idx = rtile_row * ctxt->nrtiles_x + rtile_col;
      RestorationTileLimits limits = av1_get_rest_tile_limits(
          rtile_idx, ctxt->nrtiles_x, ctxt->nrtiles_y, rtile_width,
          rtile_height, ctxt->plane_width,
#if CONFIG_STRIPED_LOOP_RESTORATION
          ctxt->plane_height, ctxt->plane > 0 ? cm->subsampling_y : 0);
#else
          ctxt->plane_height);
#endif
      fun(ctxt, rtile_idx, &limits, arg);
    }
  }
}

static void search_sgrproj_for_rtile(const struct rest_search_ctxt *ctxt,
                                     int rtile_idx,
                                     const RestorationTileLimits *limits,
                                     void *arg) {
  const MACROBLOCK *const x = &ctxt->cpi->td.mb;
  const AV1_COMMON *const cm = &ctxt->cpi->common;
  RestorationInfo *rsi = ctxt->cpi->rst_search;
  SgrprojInfo *sgrproj_info = ctxt->info->sgrproj_info;

  SgrprojInfo *ref_sgrproj_info = (SgrprojInfo *)arg;

  int64_t err =
      sse_restoration_tile(ctxt->src, cm->frame_to_show, cm, limits->h_start,
                           limits->h_end - limits->h_start, limits->v_start,
                           limits->v_end - limits->v_start, (1 << ctxt->plane));
  // #bits when a tile is not restored
  int bits = av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 0);
  double cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  ctxt->best_tile_cost[rtile_idx] = INT64_MAX;

  RestorationInfo *plane_rsi = &rsi[ctxt->plane];
  SgrprojInfo *rtile_sgrproj_info = &plane_rsi->sgrproj_info[rtile_idx];
  uint8_t *dgd_start =
      ctxt->dgd_buffer + limits->v_start * ctxt->dgd_stride + limits->h_start;
  const uint8_t *src_start =
      ctxt->src_buffer + limits->v_start * ctxt->src_stride + limits->h_start;

  search_selfguided_restoration(
      dgd_start, limits->h_end - limits->h_start,
      limits->v_end - limits->v_start, ctxt->dgd_stride, src_start,
      ctxt->src_stride,
#if CONFIG_HIGHBITDEPTH
      cm->use_highbitdepth, cm->bit_depth,
#else
      0, 8,
#endif  // CONFIG_HIGHBITDEPTH
      rsi[ctxt->plane].procunit_width, rsi[ctxt->plane].procunit_height,
      &rtile_sgrproj_info->ep, rtile_sgrproj_info->xqd,
      cm->rst_internal.tmpbuf);
  plane_rsi->restoration_type[rtile_idx] = RESTORE_SGRPROJ;
  err = try_restoration_tile(ctxt->src, ctxt->cpi, rsi, (1 << ctxt->plane),
                             ctxt->partial_frame, rtile_idx, ctxt->dst_frame);
  bits =
      count_sgrproj_bits(&plane_rsi->sgrproj_info[rtile_idx], ref_sgrproj_info)
      << AV1_PROB_COST_SHIFT;
  bits += av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 1);
  double cost_sgrproj = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  if (cost_sgrproj >= cost_norestore) {
    ctxt->type[rtile_idx] = RESTORE_NONE;
  } else {
    ctxt->type[rtile_idx] = RESTORE_SGRPROJ;
    *ref_sgrproj_info = sgrproj_info[rtile_idx] =
        plane_rsi->sgrproj_info[rtile_idx];
    ctxt->best_tile_cost[rtile_idx] = err;
  }
  plane_rsi->restoration_type[rtile_idx] = RESTORE_NONE;
}

static double search_sgrproj(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                             int partial_frame, int plane,
                             RestorationInfo *info, RestorationType *type,
                             int64_t *best_tile_cost,
                             YV12_BUFFER_CONFIG *dst_frame) {
  struct rest_search_ctxt ctxt;
  const int nrtiles =
      init_rest_search_ctxt(src, cpi, partial_frame, plane, info, type,
                            best_tile_cost, dst_frame, &ctxt);

  RestorationInfo *plane_rsi = &cpi->rst_search[plane];
  plane_rsi->frame_restoration_type = RESTORE_SGRPROJ;
  for (int rtile_idx = 0; rtile_idx < nrtiles; ++rtile_idx) {
    plane_rsi->restoration_type[rtile_idx] = RESTORE_NONE;
  }

  // Compute best Sgrproj filters for each rtile, one (encoder/decoder)
  // tile at a time.
  const AV1_COMMON *const cm = &cpi->common;
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth)
    extend_frame_highbd(CONVERT_TO_SHORTPTR(ctxt.dgd_buffer), ctxt.plane_width,
                        ctxt.plane_height, ctxt.dgd_stride, SGRPROJ_BORDER_HORZ,
                        SGRPROJ_BORDER_VERT);
  else
#endif
    extend_frame(ctxt.dgd_buffer, ctxt.plane_width, ctxt.plane_height,
                 ctxt.dgd_stride, SGRPROJ_BORDER_HORZ, SGRPROJ_BORDER_VERT);

  for (int tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
    for (int tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
      SgrprojInfo ref_sgrproj_info;
      set_default_sgrproj(&ref_sgrproj_info);
      foreach_rtile_in_tile(&ctxt, tile_row, tile_col, search_sgrproj_for_rtile,
                            &ref_sgrproj_info);
    }
  }

  // Cost for Sgrproj filtering
  SgrprojInfo ref_sgrproj_info;
  set_default_sgrproj(&ref_sgrproj_info);
  SgrprojInfo *sgrproj_info = info->sgrproj_info;

  int bits = frame_level_restore_bits[plane_rsi->frame_restoration_type]
             << AV1_PROB_COST_SHIFT;
  for (int rtile_idx = 0; rtile_idx < nrtiles; ++rtile_idx) {
    bits += av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB,
                         type[rtile_idx] != RESTORE_NONE);
    plane_rsi->sgrproj_info[rtile_idx] = sgrproj_info[rtile_idx];
    if (type[rtile_idx] == RESTORE_SGRPROJ) {
      bits += count_sgrproj_bits(&plane_rsi->sgrproj_info[rtile_idx],
                                 &ref_sgrproj_info)
              << AV1_PROB_COST_SHIFT;
      ref_sgrproj_info = plane_rsi->sgrproj_info[rtile_idx];
    }
    plane_rsi->restoration_type[rtile_idx] = type[rtile_idx];
  }
  int64_t err = try_restoration_frame(src, cpi, cpi->rst_search, (1 << plane),
                                      partial_frame, dst_frame);
  double cost_sgrproj = RDCOST_DBL(cpi->td.mb.rdmult, (bits >> 4), err);
  return cost_sgrproj;
}

static double find_average(const uint8_t *src, int h_start, int h_end,
                           int v_start, int v_end, int stride) {
  uint64_t sum = 0;
  double avg = 0;
  int i, j;
  aom_clear_system_state();
  for (i = v_start; i < v_end; i++)
    for (j = h_start; j < h_end; j++) sum += src[i * stride + j];
  avg = (double)sum / ((v_end - v_start) * (h_end - h_start));
  return avg;
}

static void compute_stats(int wiener_win, const uint8_t *dgd,
                          const uint8_t *src, int h_start, int h_end,
                          int v_start, int v_end, int dgd_stride,
                          int src_stride, double *M, double *H) {
  int i, j, k, l;
  double Y[WIENER_WIN2];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -wiener_halfwin; k <= wiener_halfwin; k++) {
        for (l = -wiener_halfwin; l <= wiener_halfwin; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      assert(idx == wiener_win2);
      for (k = 0; k < wiener_win2; ++k) {
        M[k] += Y[k] * X;
        H[k * wiener_win2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < wiener_win2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H[k * wiener_win2 + l] += Y[k] * Y[l];
        }
      }
    }
  }
  for (k = 0; k < wiener_win2; ++k) {
    for (l = k + 1; l < wiener_win2; ++l) {
      H[l * wiener_win2 + k] = H[k * wiener_win2 + l];
    }
  }
}

#if CONFIG_HIGHBITDEPTH
static double find_average_highbd(const uint16_t *src, int h_start, int h_end,
                                  int v_start, int v_end, int stride) {
  uint64_t sum = 0;
  double avg = 0;
  int i, j;
  aom_clear_system_state();
  for (i = v_start; i < v_end; i++)
    for (j = h_start; j < h_end; j++) sum += src[i * stride + j];
  avg = (double)sum / ((v_end - v_start) * (h_end - h_start));
  return avg;
}

static void compute_stats_highbd(int wiener_win, const uint8_t *dgd8,
                                 const uint8_t *src8, int h_start, int h_end,
                                 int v_start, int v_end, int dgd_stride,
                                 int src_stride, double *M, double *H) {
  int i, j, k, l;
  double Y[WIENER_WIN2];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const double avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -wiener_halfwin; k <= wiener_halfwin; k++) {
        for (l = -wiener_halfwin; l <= wiener_halfwin; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      assert(idx == wiener_win2);
      for (k = 0; k < wiener_win2; ++k) {
        M[k] += Y[k] * X;
        H[k * wiener_win2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < wiener_win2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H[k * wiener_win2 + l] += Y[k] * Y[l];
        }
      }
    }
  }
  for (k = 0; k < wiener_win2; ++k) {
    for (l = k + 1; l < wiener_win2; ++l) {
      H[l * wiener_win2 + k] = H[k * wiener_win2 + l];
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static INLINE int wrap_index(int i, int wiener_win) {
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  return (i >= wiener_halfwin1 ? wiener_win - 1 - i : i);
}

// Fix vector b, update vector a
static void update_a_sep_sym(int wiener_win, double **Mc, double **Hc,
                             double *a, double *b) {
  int i, j;
  double S[WIENER_WIN];
  double A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < wiener_win; i++) {
    for (j = 0; j < wiener_win; ++j) {
      const int jj = wrap_index(j, wiener_win);
      A[jj] += Mc[i][j] * b[i];
    }
  }
  for (i = 0; i < wiener_win; i++) {
    for (j = 0; j < wiener_win; j++) {
      int k, l;
      for (k = 0; k < wiener_win; ++k)
        for (l = 0; l < wiener_win; ++l) {
          const int kk = wrap_index(k, wiener_win);
          const int ll = wrap_index(l, wiener_win);
          B[ll * wiener_halfwin1 + kk] +=
              Hc[j * wiener_win + i][k * wiener_win2 + l] * b[i] * b[j];
        }
    }
  }
  // Normalization enforcement in the system of equations itself
  for (i = 0; i < wiener_halfwin1 - 1; ++i)
    A[i] -=
        A[wiener_halfwin1 - 1] * 2 +
        B[i * wiener_halfwin1 + wiener_halfwin1 - 1] -
        2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 + (wiener_halfwin1 - 1)];
  for (i = 0; i < wiener_halfwin1 - 1; ++i)
    for (j = 0; j < wiener_halfwin1 - 1; ++j)
      B[i * wiener_halfwin1 + j] -=
          2 * (B[i * wiener_halfwin1 + (wiener_halfwin1 - 1)] +
               B[(wiener_halfwin1 - 1) * wiener_halfwin1 + j] -
               2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 +
                     (wiener_halfwin1 - 1)]);
  if (linsolve(wiener_halfwin1 - 1, B, wiener_halfwin1, A, S)) {
    S[wiener_halfwin1 - 1] = 1.0;
    for (i = wiener_halfwin1; i < wiener_win; ++i) {
      S[i] = S[wiener_win - 1 - i];
      S[wiener_halfwin1 - 1] -= 2 * S[i];
    }
    memcpy(a, S, wiener_win * sizeof(*a));
  }
}

// Fix vector a, update vector b
static void update_b_sep_sym(int wiener_win, double **Mc, double **Hc,
                             double *a, double *b) {
  int i, j;
  double S[WIENER_WIN];
  double A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin1 = (wiener_win >> 1) + 1;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < wiener_win; i++) {
    const int ii = wrap_index(i, wiener_win);
    for (j = 0; j < wiener_win; j++) A[ii] += Mc[i][j] * a[j];
  }

  for (i = 0; i < wiener_win; i++) {
    for (j = 0; j < wiener_win; j++) {
      const int ii = wrap_index(i, wiener_win);
      const int jj = wrap_index(j, wiener_win);
      int k, l;
      for (k = 0; k < wiener_win; ++k)
        for (l = 0; l < wiener_win; ++l)
          B[jj * wiener_halfwin1 + ii] +=
              Hc[i * wiener_win + j][k * wiener_win2 + l] * a[k] * a[l];
    }
  }
  // Normalization enforcement in the system of equations itself
  for (i = 0; i < wiener_halfwin1 - 1; ++i)
    A[i] -=
        A[wiener_halfwin1 - 1] * 2 +
        B[i * wiener_halfwin1 + wiener_halfwin1 - 1] -
        2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 + (wiener_halfwin1 - 1)];
  for (i = 0; i < wiener_halfwin1 - 1; ++i)
    for (j = 0; j < wiener_halfwin1 - 1; ++j)
      B[i * wiener_halfwin1 + j] -=
          2 * (B[i * wiener_halfwin1 + (wiener_halfwin1 - 1)] +
               B[(wiener_halfwin1 - 1) * wiener_halfwin1 + j] -
               2 * B[(wiener_halfwin1 - 1) * wiener_halfwin1 +
                     (wiener_halfwin1 - 1)]);
  if (linsolve(wiener_halfwin1 - 1, B, wiener_halfwin1, A, S)) {
    S[wiener_halfwin1 - 1] = 1.0;
    for (i = wiener_halfwin1; i < wiener_win; ++i) {
      S[i] = S[wiener_win - 1 - i];
      S[wiener_halfwin1 - 1] -= 2 * S[i];
    }
    memcpy(b, S, wiener_win * sizeof(*b));
  }
}

static int wiener_decompose_sep_sym(int wiener_win, double *M, double *H,
                                    double *a, double *b) {
  static const int init_filt[WIENER_WIN] = {
    WIENER_FILT_TAP0_MIDV, WIENER_FILT_TAP1_MIDV, WIENER_FILT_TAP2_MIDV,
    WIENER_FILT_TAP3_MIDV, WIENER_FILT_TAP2_MIDV, WIENER_FILT_TAP1_MIDV,
    WIENER_FILT_TAP0_MIDV,
  };
  double *Hc[WIENER_WIN2];
  double *Mc[WIENER_WIN];
  int i, j, iter;
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  const int wiener_win2 = wiener_win * wiener_win;
  for (i = 0; i < wiener_win; i++) {
    a[i] = b[i] = (double)init_filt[i + plane_off] / WIENER_FILT_STEP;
  }
  for (i = 0; i < wiener_win; i++) {
    Mc[i] = M + i * wiener_win;
    for (j = 0; j < wiener_win; j++) {
      Hc[i * wiener_win + j] =
          H + i * wiener_win * wiener_win2 + j * wiener_win;
    }
  }

  iter = 1;
  while (iter < NUM_WIENER_ITERS) {
    update_a_sep_sym(wiener_win, Mc, Hc, a, b);
    update_b_sep_sym(wiener_win, Mc, Hc, a, b);
    iter++;
  }
  return 1;
}

// Computes the function x'*H*x - x'*M for the learned 2D filter x, and compares
// against identity filters; Final score is defined as the difference between
// the function values
static double compute_score(int wiener_win, double *M, double *H,
                            InterpKernel vfilt, InterpKernel hfilt) {
  double ab[WIENER_WIN * WIENER_WIN];
  int i, k, l;
  double P = 0, Q = 0;
  double iP = 0, iQ = 0;
  double Score, iScore;
  double a[WIENER_WIN], b[WIENER_WIN];
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  const int wiener_win2 = wiener_win * wiener_win;

  aom_clear_system_state();

  a[WIENER_HALFWIN] = b[WIENER_HALFWIN] = 1.0;
  for (i = 0; i < WIENER_HALFWIN; ++i) {
    a[i] = a[WIENER_WIN - i - 1] = (double)vfilt[i] / WIENER_FILT_STEP;
    b[i] = b[WIENER_WIN - i - 1] = (double)hfilt[i] / WIENER_FILT_STEP;
    a[WIENER_HALFWIN] -= 2 * a[i];
    b[WIENER_HALFWIN] -= 2 * b[i];
  }
  memset(ab, 0, sizeof(ab));
  for (k = 0; k < wiener_win; ++k) {
    for (l = 0; l < wiener_win; ++l)
      ab[k * wiener_win + l] = a[l + plane_off] * b[k + plane_off];
  }
  for (k = 0; k < wiener_win2; ++k) {
    P += ab[k] * M[k];
    for (l = 0; l < wiener_win2; ++l)
      Q += ab[k] * H[k * wiener_win2 + l] * ab[l];
  }
  Score = Q - 2 * P;

  iP = M[wiener_win2 >> 1];
  iQ = H[(wiener_win2 >> 1) * wiener_win2 + (wiener_win2 >> 1)];
  iScore = iQ - 2 * iP;

  return Score - iScore;
}

static void quantize_sym_filter(int wiener_win, double *f, InterpKernel fi) {
  int i;
  const int wiener_halfwin = (wiener_win >> 1);
  for (i = 0; i < wiener_halfwin; ++i) {
    fi[i] = RINT(f[i] * WIENER_FILT_STEP);
  }
  // Specialize for 7-tap filter
  if (wiener_win == WIENER_WIN) {
    fi[0] = CLIP(fi[0], WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP0_MAXV);
    fi[1] = CLIP(fi[1], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
    fi[2] = CLIP(fi[2], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
  } else {
    fi[2] = CLIP(fi[1], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
    fi[1] = CLIP(fi[0], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
    fi[0] = 0;
  }
  // Satisfy filter constraints
  fi[WIENER_WIN - 1] = fi[0];
  fi[WIENER_WIN - 2] = fi[1];
  fi[WIENER_WIN - 3] = fi[2];
  // The central element has an implicit +WIENER_FILT_STEP
  fi[3] = -2 * (fi[0] + fi[1] + fi[2]);
}

static int count_wiener_bits(int wiener_win, WienerInfo *wiener_info,
                             WienerInfo *ref_wiener_info) {
  int bits = 0;
  if (wiener_win == WIENER_WIN)
    bits += aom_count_primitive_refsubexpfin(
        WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->vfilter[0] - WIENER_FILT_TAP0_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->vfilter[1] - WIENER_FILT_TAP1_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->vfilter[2] - WIENER_FILT_TAP2_MINV);
  if (wiener_win == WIENER_WIN)
    bits += aom_count_primitive_refsubexpfin(
        WIENER_FILT_TAP0_MAXV - WIENER_FILT_TAP0_MINV + 1,
        WIENER_FILT_TAP0_SUBEXP_K,
        ref_wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV,
        wiener_info->hfilter[0] - WIENER_FILT_TAP0_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP1_MAXV - WIENER_FILT_TAP1_MINV + 1,
      WIENER_FILT_TAP1_SUBEXP_K,
      ref_wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV,
      wiener_info->hfilter[1] - WIENER_FILT_TAP1_MINV);
  bits += aom_count_primitive_refsubexpfin(
      WIENER_FILT_TAP2_MAXV - WIENER_FILT_TAP2_MINV + 1,
      WIENER_FILT_TAP2_SUBEXP_K,
      ref_wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV,
      wiener_info->hfilter[2] - WIENER_FILT_TAP2_MINV);
  return bits;
}

#define USE_WIENER_REFINEMENT_SEARCH 1
static int64_t finer_tile_search_wiener(const YV12_BUFFER_CONFIG *src,
                                        AV1_COMP *cpi, RestorationInfo *rsi,
                                        int start_step, int plane,
                                        int wiener_win, int tile_idx,
                                        int partial_frame,
                                        YV12_BUFFER_CONFIG *dst_frame) {
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  int64_t err = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                     tile_idx, dst_frame);
  (void)start_step;
#if USE_WIENER_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP1_MINV,
                    WIENER_FILT_TAP2_MINV };
  int tap_max[] = { WIENER_FILT_TAP0_MAXV, WIENER_FILT_TAP1_MAXV,
                    WIENER_FILT_TAP2_MAXV };
  // printf("err  pre = %"PRId64"\n", err);
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = plane_off; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (rsi[plane].wiener_info[tile_idx].hfilter[p] - s >= tap_min[p]) {
          rsi[plane].wiener_info[tile_idx].hfilter[p] -= s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_WIN - p - 1] -= s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, dst_frame);
          if (err2 > err) {
            rsi[plane].wiener_info[tile_idx].hfilter[p] += s;
            rsi[plane].wiener_info[tile_idx].hfilter[WIENER_WIN - p - 1] += s;
            rsi[plane].wiener_info[tile_idx].hfilter[WIENER_HALFWIN] -= 2 * s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (rsi[plane].wiener_info[tile_idx].hfilter[p] + s <= tap_max[p]) {
          rsi[plane].wiener_info[tile_idx].hfilter[p] += s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_WIN - p - 1] += s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, dst_frame);
          if (err2 > err) {
            rsi[plane].wiener_info[tile_idx].hfilter[p] -= s;
            rsi[plane].wiener_info[tile_idx].hfilter[WIENER_WIN - p - 1] -= s;
            rsi[plane].wiener_info[tile_idx].hfilter[WIENER_HALFWIN] += 2 * s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
    for (int p = plane_off; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (rsi[plane].wiener_info[tile_idx].vfilter[p] - s >= tap_min[p]) {
          rsi[plane].wiener_info[tile_idx].vfilter[p] -= s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_WIN - p - 1] -= s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, dst_frame);
          if (err2 > err) {
            rsi[plane].wiener_info[tile_idx].vfilter[p] += s;
            rsi[plane].wiener_info[tile_idx].vfilter[WIENER_WIN - p - 1] += s;
            rsi[plane].wiener_info[tile_idx].vfilter[WIENER_HALFWIN] -= 2 * s;
          } else {
            err = err2;
            skip = 1;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
      if (skip) break;
      do {
        if (rsi[plane].wiener_info[tile_idx].vfilter[p] + s <= tap_max[p]) {
          rsi[plane].wiener_info[tile_idx].vfilter[p] += s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_WIN - p - 1] += s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, dst_frame);
          if (err2 > err) {
            rsi[plane].wiener_info[tile_idx].vfilter[p] -= s;
            rsi[plane].wiener_info[tile_idx].vfilter[WIENER_WIN - p - 1] -= s;
            rsi[plane].wiener_info[tile_idx].vfilter[WIENER_HALFWIN] += 2 * s;
          } else {
            err = err2;
            // At the highest step size continue moving in the same direction
            if (s == start_step) continue;
          }
        }
        break;
      } while (1);
    }
  }
// printf("err post = %"PRId64"\n", err);
#endif  // USE_WIENER_REFINEMENT_SEARCH
  return err;
}

static void search_wiener_for_rtile(const struct rest_search_ctxt *ctxt,
                                    int rtile_idx,
                                    const RestorationTileLimits *limits,
                                    void *arg) {
  const MACROBLOCK *const x = &ctxt->cpi->td.mb;
  const AV1_COMMON *const cm = &ctxt->cpi->common;
  RestorationInfo *rsi = ctxt->cpi->rst_search;

  const int wiener_win =
      (ctxt->plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  double M[WIENER_WIN2];
  double H[WIENER_WIN2 * WIENER_WIN2];
  double vfilterd[WIENER_WIN], hfilterd[WIENER_WIN];

  WienerInfo *ref_wiener_info = (WienerInfo *)arg;

  int64_t err =
      sse_restoration_tile(ctxt->src, cm->frame_to_show, cm, limits->h_start,
                           limits->h_end - limits->h_start, limits->v_start,
                           limits->v_end - limits->v_start, (1 << ctxt->plane));
  // #bits when a tile is not restored
  int bits = av1_cost_bit(RESTORE_NONE_WIENER_PROB, 0);
  double cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  ctxt->best_tile_cost[rtile_idx] = INT64_MAX;

#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth)
    compute_stats_highbd(wiener_win, ctxt->dgd_buffer, ctxt->src_buffer,
                         limits->h_start, limits->h_end, limits->v_start,
                         limits->v_end, ctxt->dgd_stride, ctxt->src_stride, M,
                         H);
  else
#endif  // CONFIG_HIGHBITDEPTH
    compute_stats(wiener_win, ctxt->dgd_buffer, ctxt->src_buffer,
                  limits->h_start, limits->h_end, limits->v_start,
                  limits->v_end, ctxt->dgd_stride, ctxt->src_stride, M, H);

  ctxt->type[rtile_idx] = RESTORE_WIENER;

  if (!wiener_decompose_sep_sym(wiener_win, M, H, vfilterd, hfilterd)) {
    ctxt->type[rtile_idx] = RESTORE_NONE;
    return;
  }

  RestorationInfo *plane_rsi = &rsi[ctxt->plane];
  WienerInfo *rtile_wiener_info = &plane_rsi->wiener_info[rtile_idx];
  quantize_sym_filter(wiener_win, vfilterd, rtile_wiener_info->vfilter);
  quantize_sym_filter(wiener_win, hfilterd, rtile_wiener_info->hfilter);

  // Filter score computes the value of the function x'*A*x - x'*b for the
  // learned filter and compares it against identity filer. If there is no
  // reduction in the function, the filter is reverted back to identity
  double score = compute_score(wiener_win, M, H, rtile_wiener_info->vfilter,
                               rtile_wiener_info->hfilter);
  if (score > 0.0) {
    ctxt->type[rtile_idx] = RESTORE_NONE;
    return;
  }
  aom_clear_system_state();

  plane_rsi->restoration_type[rtile_idx] = RESTORE_WIENER;
  err = finer_tile_search_wiener(ctxt->src, ctxt->cpi, rsi, 4, ctxt->plane,
                                 wiener_win, rtile_idx, ctxt->partial_frame,
                                 ctxt->dst_frame);
  if (wiener_win != WIENER_WIN) {
    assert(rtile_wiener_info->vfilter[0] == 0 &&
           rtile_wiener_info->vfilter[WIENER_WIN - 1] == 0);
    assert(rtile_wiener_info->hfilter[0] == 0 &&
           rtile_wiener_info->hfilter[WIENER_WIN - 1] == 0);
  }
  bits = count_wiener_bits(wiener_win, rtile_wiener_info, ref_wiener_info)
         << AV1_PROB_COST_SHIFT;
  bits += av1_cost_bit(RESTORE_NONE_WIENER_PROB, 1);
  double cost_wiener = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  if (cost_wiener >= cost_norestore) {
    ctxt->type[rtile_idx] = RESTORE_NONE;
  } else {
    ctxt->type[rtile_idx] = RESTORE_WIENER;
    *ref_wiener_info = ctxt->info->wiener_info[rtile_idx] = *rtile_wiener_info;
    ctxt->best_tile_cost[rtile_idx] = err;
  }
  plane_rsi->restoration_type[rtile_idx] = RESTORE_NONE;
}

static double search_wiener(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                            int partial_frame, int plane, RestorationInfo *info,
                            RestorationType *type, int64_t *best_tile_cost,
                            YV12_BUFFER_CONFIG *dst_frame) {
  struct rest_search_ctxt ctxt;
  const int nrtiles =
      init_rest_search_ctxt(src, cpi, partial_frame, plane, info, type,
                            best_tile_cost, dst_frame, &ctxt);

  RestorationInfo *plane_rsi = &cpi->rst_search[plane];
  plane_rsi->frame_restoration_type = RESTORE_WIENER;
  for (int tile_idx = 0; tile_idx < nrtiles; ++tile_idx) {
    plane_rsi->restoration_type[tile_idx] = RESTORE_NONE;
  }

  AV1_COMMON *const cm = &cpi->common;
// Construct a (WIENER_HALFWIN)-pixel border around the frame
// Note use this border to gather stats even though the actual filter
// may use less border on the top/bottom of a processing unit.
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth)
    extend_frame_highbd(CONVERT_TO_SHORTPTR(ctxt.dgd_buffer), ctxt.plane_width,
                        ctxt.plane_height, ctxt.dgd_stride, WIENER_HALFWIN,
                        WIENER_HALFWIN);
  else
#endif
    extend_frame(ctxt.dgd_buffer, ctxt.plane_width, ctxt.plane_height,
                 ctxt.dgd_stride, WIENER_HALFWIN, WIENER_HALFWIN);

  // Compute best Wiener filters for each rtile, one (encoder/decoder)
  // tile at a time.
  for (int tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
    for (int tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
      WienerInfo ref_wiener_info;
      set_default_wiener(&ref_wiener_info);

      foreach_rtile_in_tile(&ctxt, tile_row, tile_col, search_wiener_for_rtile,
                            &ref_wiener_info);
    }
  }

  // cost for Wiener filtering
  WienerInfo ref_wiener_info;
  set_default_wiener(&ref_wiener_info);
  int bits = frame_level_restore_bits[plane_rsi->frame_restoration_type]
             << AV1_PROB_COST_SHIFT;
  WienerInfo *wiener_info = info->wiener_info;
  const int wiener_win =
      (plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  for (int tile_idx = 0; tile_idx < nrtiles; ++tile_idx) {
    bits +=
        av1_cost_bit(RESTORE_NONE_WIENER_PROB, type[tile_idx] != RESTORE_NONE);
    plane_rsi->wiener_info[tile_idx] = wiener_info[tile_idx];

    if (type[tile_idx] == RESTORE_WIENER) {
      bits += count_wiener_bits(wiener_win, &plane_rsi->wiener_info[tile_idx],
                                &ref_wiener_info)
              << AV1_PROB_COST_SHIFT;
      ref_wiener_info = plane_rsi->wiener_info[tile_idx];
    }
    plane_rsi->restoration_type[tile_idx] = type[tile_idx];
  }
  int64_t err = try_restoration_frame(src, cpi, cpi->rst_search, 1 << plane,
                                      partial_frame, dst_frame);
  double cost_wiener = RDCOST_DBL(cpi->td.mb.rdmult, (bits >> 4), err);

  return cost_wiener;
}

static double search_norestore(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                               int partial_frame, int plane,
                               RestorationInfo *info, RestorationType *type,
                               int64_t *best_tile_cost,
                               YV12_BUFFER_CONFIG *dst_frame) {
  int64_t err;
  double cost_norestore;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int width, height;
  if (plane == AOM_PLANE_Y) {
    width = src->y_crop_width;
    height = src->y_crop_height;
  } else {
    width = src->uv_crop_width;
    height = src->uv_crop_height;
  }
  const int ntiles = av1_get_rest_ntiles(
      width, height, cm->rst_info[plane].restoration_tilesize, &tile_width,
      &tile_height, &nhtiles, &nvtiles);
  (void)info;
  (void)dst_frame;
  (void)partial_frame;

  info->frame_restoration_type = RESTORE_NONE;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    RestorationTileLimits limits = av1_get_rest_tile_limits(
        tile_idx, nhtiles, nvtiles, tile_width, tile_height, width,
#if CONFIG_STRIPED_LOOP_RESTORATION
        height, plane != AOM_PLANE_Y ? cm->subsampling_y : 0);
#else
        height);
#endif
    err = sse_restoration_tile(src, cm->frame_to_show, cm, limits.h_start,
                               limits.h_end - limits.h_start, limits.v_start,
                               limits.v_end - limits.v_start, 1 << plane);
    type[tile_idx] = RESTORE_NONE;
    best_tile_cost[tile_idx] = err;
  }
  // RD cost associated with no restoration
  err = sse_restoration_frame(cm, src, cm->frame_to_show, (1 << plane));
  bits = frame_level_restore_bits[RESTORE_NONE] << AV1_PROB_COST_SHIFT;
  cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  return cost_norestore;
}

struct switchable_rest_search_ctxt {
  SgrprojInfo sgrproj_info;
  WienerInfo wiener_info;
  RestorationType *const *restore_types;
  int64_t *const *tile_cost;
  double cost_switchable;
};

static void search_switchable_for_rtile(const struct rest_search_ctxt *ctxt,
                                        int rtile_idx,
                                        const RestorationTileLimits *limits,
                                        void *arg) {
  const MACROBLOCK *x = &ctxt->cpi->td.mb;
  RestorationInfo *rsi = &ctxt->cpi->common.rst_info[ctxt->plane];
  struct switchable_rest_search_ctxt *swctxt =
      (struct switchable_rest_search_ctxt *)arg;

  (void)limits;

  double best_cost =
      RDCOST_DBL(x->rdmult, (x->switchable_restore_cost[RESTORE_NONE] >> 4),
                 swctxt->tile_cost[RESTORE_NONE][rtile_idx]);
  rsi->restoration_type[rtile_idx] = RESTORE_NONE;
  for (RestorationType r = 1; r < RESTORE_SWITCHABLE_TYPES; r++) {
    if (force_restore_type != RESTORE_TYPES)
      if (r != force_restore_type) continue;
    int tilebits = 0;
    if (swctxt->restore_types[r][rtile_idx] != r) continue;
    if (r == RESTORE_WIENER)
      tilebits += count_wiener_bits(
          (ctxt->plane == AOM_PLANE_Y ? WIENER_WIN : WIENER_WIN - 2),
          &rsi->wiener_info[rtile_idx], &swctxt->wiener_info);
    else if (r == RESTORE_SGRPROJ)
      tilebits += count_sgrproj_bits(&rsi->sgrproj_info[rtile_idx],
                                     &swctxt->sgrproj_info);
    tilebits <<= AV1_PROB_COST_SHIFT;
    tilebits += x->switchable_restore_cost[r];
    double cost =
        RDCOST_DBL(x->rdmult, tilebits >> 4, swctxt->tile_cost[r][rtile_idx]);

    if (cost < best_cost) {
      rsi->restoration_type[rtile_idx] = r;
      best_cost = cost;
    }
  }
  if (rsi->restoration_type[rtile_idx] == RESTORE_WIENER)
    swctxt->wiener_info = rsi->wiener_info[rtile_idx];
  else if (rsi->restoration_type[rtile_idx] == RESTORE_SGRPROJ)
    swctxt->sgrproj_info = rsi->sgrproj_info[rtile_idx];
  if (force_restore_type != RESTORE_TYPES)
    assert(rsi->restoration_type[rtile_idx] == force_restore_type ||
           rsi->restoration_type[rtile_idx] == RESTORE_NONE);
  swctxt->cost_switchable += best_cost;
}

static double search_switchable_restoration(
    const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi, int partial_frame, int plane,
    RestorationType *const restore_types[RESTORE_SWITCHABLE_TYPES],
    int64_t *const tile_cost[RESTORE_SWITCHABLE_TYPES], RestorationInfo *rsi) {
  const AV1_COMMON *const cm = &cpi->common;
  struct rest_search_ctxt ctxt;
  init_rest_search_ctxt(src, cpi, partial_frame, plane, NULL, NULL, NULL, NULL,
                        &ctxt);
  struct switchable_rest_search_ctxt swctxt;
  swctxt.restore_types = restore_types;
  swctxt.tile_cost = tile_cost;

  rsi->frame_restoration_type = RESTORE_SWITCHABLE;
  int bits = frame_level_restore_bits[rsi->frame_restoration_type]
             << AV1_PROB_COST_SHIFT;
  swctxt.cost_switchable = RDCOST_DBL(cpi->td.mb.rdmult, bits >> 4, 0);

  for (int tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
    for (int tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
      set_default_sgrproj(&swctxt.sgrproj_info);
      set_default_wiener(&swctxt.wiener_info);
      foreach_rtile_in_tile(&ctxt, tile_row, tile_col,
                            search_switchable_for_rtile, &swctxt);
    }
  }

  return swctxt.cost_switchable;
}

void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                                 LPF_PICK_METHOD method) {
  static search_restore_type search_restore_fun[RESTORE_SWITCHABLE_TYPES] = {
    search_norestore, search_wiener, search_sgrproj,
  };
  AV1_COMMON *const cm = &cpi->common;
  double cost_restore[RESTORE_TYPES];
  int64_t *tile_cost[RESTORE_SWITCHABLE_TYPES];
  RestorationType *restore_types[RESTORE_SWITCHABLE_TYPES];
  double best_cost_restore;
  RestorationType r, best_restore;
  const int ywidth = src->y_crop_width;
  const int yheight = src->y_crop_height;
  const int uvwidth = src->uv_crop_width;
  const int uvheight = src->uv_crop_height;

  const int ntiles_y =
      av1_get_rest_ntiles(ywidth, yheight, cm->rst_info[0].restoration_tilesize,
                          NULL, NULL, NULL, NULL);
  const int ntiles_uv = av1_get_rest_ntiles(
      uvwidth, uvheight, cm->rst_info[1].restoration_tilesize, NULL, NULL, NULL,
      NULL);

  // Assume ntiles_uv is never larger that ntiles_y and so the same arrays work.
  for (r = 0; r < RESTORE_SWITCHABLE_TYPES; r++) {
    tile_cost[r] = (int64_t *)aom_malloc(sizeof(*tile_cost[0]) * ntiles_y);
    restore_types[r] =
        (RestorationType *)aom_malloc(sizeof(*restore_types[0]) * ntiles_y);
  }

  for (int plane = AOM_PLANE_Y; plane <= AOM_PLANE_V; ++plane) {
    for (r = 0; r < RESTORE_SWITCHABLE_TYPES; ++r) {
      cost_restore[r] = DBL_MAX;
      if (force_restore_type != RESTORE_TYPES)
        if (r != RESTORE_NONE && r != force_restore_type) continue;
      cost_restore[r] =
          search_restore_fun[r](src, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                                plane, &cm->rst_info[plane], restore_types[r],
                                tile_cost[r], &cpi->trial_frame_rst);
    }
    if (plane == AOM_PLANE_Y)
      cost_restore[RESTORE_SWITCHABLE] = search_switchable_restoration(
          src, cpi, method == LPF_PICK_FROM_SUBIMAGE, plane, restore_types,
          tile_cost, &cm->rst_info[plane]);
    else
      cost_restore[RESTORE_SWITCHABLE] = DBL_MAX;
    best_cost_restore = DBL_MAX;
    best_restore = 0;
    for (r = 0; r < RESTORE_TYPES; ++r) {
      if (force_restore_type != RESTORE_TYPES)
        if (r != RESTORE_NONE && r != force_restore_type) continue;
      if (cost_restore[r] < best_cost_restore) {
        best_restore = r;
        best_cost_restore = cost_restore[r];
      }
    }
    cm->rst_info[plane].frame_restoration_type = best_restore;
    if (force_restore_type != RESTORE_TYPES)
      assert(best_restore == force_restore_type ||
             best_restore == RESTORE_NONE);
    if (best_restore != RESTORE_SWITCHABLE) {
      const int nt = (plane == AOM_PLANE_Y ? ntiles_y : ntiles_uv);
      memcpy(cm->rst_info[plane].restoration_type, restore_types[best_restore],
             nt * sizeof(restore_types[best_restore][0]));
    }
  }
  /*
  printf("Frame %d/%d restore types: %d %d %d\n", cm->current_video_frame,
         cm->show_frame, cm->rst_info[0].frame_restoration_type,
         cm->rst_info[1].frame_restoration_type,
         cm->rst_info[2].frame_restoration_type);
  printf("Frame %d/%d frame_restore_type %d : %f %f %f %f\n",
         cm->current_video_frame, cm->show_frame,
         cm->rst_info[0].frame_restoration_type, cost_restore[0],
         cost_restore[1], cost_restore[2], cost_restore[3]);
         */

  for (r = 0; r < RESTORE_SWITCHABLE_TYPES; r++) {
    aom_free(tile_cost[r]);
    aom_free(restore_types[r]);
  }
}
