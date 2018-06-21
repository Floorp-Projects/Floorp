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

#include "config/aom_scale_rtcd.h"

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

// Penalty factor for use of dual sgr
#define DUAL_SGR_PENALTY_MULT 0.01

const int frame_level_restore_bits[RESTORE_TYPES] = { 2, 2, 2, 2 };

typedef int64_t (*sse_extractor_type)(const YV12_BUFFER_CONFIG *a,
                                      const YV12_BUFFER_CONFIG *b);
typedef int64_t (*sse_part_extractor_type)(const YV12_BUFFER_CONFIG *a,
                                           const YV12_BUFFER_CONFIG *b,
                                           int hstart, int width, int vstart,
                                           int height);

#define NUM_EXTRACTORS (3 * (1 + 1))

static const sse_part_extractor_type sse_part_extractors[NUM_EXTRACTORS] = {
  aom_get_y_sse_part,        aom_get_u_sse_part,
  aom_get_v_sse_part,        aom_highbd_get_y_sse_part,
  aom_highbd_get_u_sse_part, aom_highbd_get_v_sse_part,
};

static int64_t sse_restoration_unit(const RestorationTileLimits *limits,
                                    const YV12_BUFFER_CONFIG *src,
                                    const YV12_BUFFER_CONFIG *dst, int plane,
                                    int highbd) {
  return sse_part_extractors[3 * highbd + plane](
      src, dst, limits->h_start, limits->h_end - limits->h_start,
      limits->v_start, limits->v_end - limits->v_start);
}

typedef struct {
  // The best coefficients for Wiener or Sgrproj restoration
  WienerInfo wiener;
  SgrprojInfo sgrproj;

  // The sum of squared errors for this rtype.
  int64_t sse[RESTORE_SWITCHABLE_TYPES];

  // The rtype to use for this unit given a frame rtype as
  // index. Indices: WIENER, SGRPROJ, SWITCHABLE.
  RestorationType best_rtype[RESTORE_TYPES - 1];
} RestUnitSearchInfo;

typedef struct {
  const YV12_BUFFER_CONFIG *src;
  YV12_BUFFER_CONFIG *dst;

  const AV1_COMMON *cm;
  const MACROBLOCK *x;
  int plane;
  int plane_width;
  int plane_height;
  RestUnitSearchInfo *rusi;

  // Speed features
  const SPEED_FEATURES *sf;

  uint8_t *dgd_buffer;
  int dgd_stride;
  const uint8_t *src_buffer;
  int src_stride;

  // sse and bits are initialised by reset_rsc in search_rest_type
  int64_t sse;
  int64_t bits;
  int tile_y0, tile_stripe0;

  // sgrproj and wiener are initialised by rsc_on_tile when starting the first
  // tile in the frame.
  SgrprojInfo sgrproj;
  WienerInfo wiener;
  AV1PixelRect tile_rect;
} RestSearchCtxt;

static void rsc_on_tile(int tile_row, int tile_col, void *priv) {
  (void)tile_col;

  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  set_default_sgrproj(&rsc->sgrproj);
  set_default_wiener(&rsc->wiener);

  rsc->tile_stripe0 =
      (tile_row == 0) ? 0 : rsc->cm->rst_end_stripe[tile_row - 1];
}

static void reset_rsc(RestSearchCtxt *rsc) {
  rsc->sse = 0;
  rsc->bits = 0;
}

static void init_rsc(const YV12_BUFFER_CONFIG *src, const AV1_COMMON *cm,
                     const MACROBLOCK *x, const SPEED_FEATURES *sf, int plane,
                     RestUnitSearchInfo *rusi, YV12_BUFFER_CONFIG *dst,
                     RestSearchCtxt *rsc) {
  rsc->src = src;
  rsc->dst = dst;
  rsc->cm = cm;
  rsc->x = x;
  rsc->plane = plane;
  rsc->rusi = rusi;
  rsc->sf = sf;

  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  const int is_uv = plane != AOM_PLANE_Y;
  rsc->plane_width = src->crop_widths[is_uv];
  rsc->plane_height = src->crop_heights[is_uv];
  rsc->src_buffer = src->buffers[plane];
  rsc->src_stride = src->strides[is_uv];
  rsc->dgd_buffer = dgd->buffers[plane];
  rsc->dgd_stride = dgd->strides[is_uv];
  rsc->tile_rect = av1_whole_frame_rect(cm, is_uv);
  assert(src->crop_widths[is_uv] == dgd->crop_widths[is_uv]);
  assert(src->crop_heights[is_uv] == dgd->crop_heights[is_uv]);
}

static int64_t try_restoration_unit(const RestSearchCtxt *rsc,
                                    const RestorationTileLimits *limits,
                                    const AV1PixelRect *tile_rect,
                                    const RestorationUnitInfo *rui) {
  const AV1_COMMON *const cm = rsc->cm;
  const int plane = rsc->plane;
  const int is_uv = plane > 0;
  const RestorationInfo *rsi = &cm->rst_info[plane];
  RestorationLineBuffers rlbs;
  const int bit_depth = cm->bit_depth;
  const int highbd = cm->use_highbitdepth;

  const YV12_BUFFER_CONFIG *fts = cm->frame_to_show;
  // TODO(yunqing): For now, only use optimized LR filter in decoder. Can be
  // also used in encoder.
  const int optimized_lr = 0;

  av1_loop_restoration_filter_unit(
      limits, rui, &rsi->boundaries, &rlbs, tile_rect, rsc->tile_stripe0,
      is_uv && cm->subsampling_x, is_uv && cm->subsampling_y, highbd, bit_depth,
      fts->buffers[plane], fts->strides[is_uv], rsc->dst->buffers[plane],
      rsc->dst->strides[is_uv], cm->rst_tmpbuf, optimized_lr);

  return sse_restoration_unit(limits, rsc->src, rsc->dst, plane, highbd);
}

static int64_t get_pixel_proj_error(const uint8_t *src8, int width, int height,
                                    int src_stride, const uint8_t *dat8,
                                    int dat_stride, int use_highbitdepth,
                                    int32_t *flt0, int flt0_stride,
                                    int32_t *flt1, int flt1_stride, int *xqd,
                                    const sgr_params_type *params) {
  int i, j;
  int64_t err = 0;
  int xq[2];
  decode_xq(xqd, xq, params);
  if (!use_highbitdepth) {
    const uint8_t *src = src8;
    const uint8_t *dat = dat8;
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; ++j) {
        const int32_t u =
            (int32_t)(dat[i * dat_stride + j] << SGRPROJ_RST_BITS);
        int32_t v = u << SGRPROJ_PRJ_BITS;
        if (params->r[0] > 0) v += xq[0] * (flt0[i * flt0_stride + j] - u);
        if (params->r[1] > 0) v += xq[1] * (flt1[i * flt1_stride + j] - u);
        const int32_t e =
            ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) -
            src[i * src_stride + j];
        err += e * e;
      }
    }
  } else {
    const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
    const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
    const int32_t half = 1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1);
    if (params->r[0] > 0 && params->r[1] > 0) {
      int xq0 = xq[0];
      int xq1 = xq[1];
      for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
          const int32_t d = dat[j];
          const int32_t s = src[j];
          const int32_t u = (int32_t)(d << SGRPROJ_RST_BITS);
          int32_t v0 = flt0[j] - u;
          int32_t v1 = flt1[j] - u;
          int32_t v = half;
          v += xq0 * v0;
          v += xq1 * v1;
          const int32_t e =
              (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + d - s;
          err += e * e;
        }
        dat += dat_stride;
        flt0 += flt0_stride;
        flt1 += flt1_stride;
        src += src_stride;
      }
    } else if (params->r[0] > 0 || params->r[1] > 0) {
      int exq;
      int32_t *flt;
      int flt_stride;
      if (params->r[0] > 0) {
        exq = xq[0];
        flt = flt0;
        flt_stride = flt0_stride;
      } else {
        exq = xq[1];
        flt = flt1;
        flt_stride = flt1_stride;
      }
      for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
          const int32_t d = dat[j];
          const int32_t s = src[j];
          const int32_t u = (int32_t)(d << SGRPROJ_RST_BITS);
          int32_t v = half;
          v += exq * (flt[j] - u);
          const int32_t e =
              (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + d - s;
          err += e * e;
        }
        dat += dat_stride;
        flt += flt_stride;
        src += src_stride;
      }
    } else {
      for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
          const int32_t d = dat[j];
          const int32_t s = src[j];
          const int32_t e = d - s;
          err += e * e;
        }
        dat += dat_stride;
        src += src_stride;
      }
    }
  }
  return err;
}

#define USE_SGRPROJ_REFINEMENT_SEARCH 1
static int64_t finer_search_pixel_proj_error(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int use_highbitdepth, int32_t *flt0,
    int flt0_stride, int32_t *flt1, int flt1_stride, int start_step, int *xqd,
    const sgr_params_type *params) {
  int64_t err = get_pixel_proj_error(
      src8, width, height, src_stride, dat8, dat_stride, use_highbitdepth, flt0,
      flt0_stride, flt1, flt1_stride, xqd, params);
  (void)start_step;
#if USE_SGRPROJ_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MIN1 };
  int tap_max[] = { SGRPROJ_PRJ_MAX0, SGRPROJ_PRJ_MAX1 };
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = 0; p < 2; ++p) {
      if ((params->r[0] == 0 && p == 0) || (params->r[1] == 0 && p == 1)) {
        continue;
      }
      int skip = 0;
      do {
        if (xqd[p] - s >= tap_min[p]) {
          xqd[p] -= s;
          err2 =
              get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                   dat_stride, use_highbitdepth, flt0,
                                   flt0_stride, flt1, flt1_stride, xqd, params);
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
          err2 =
              get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                   dat_stride, use_highbitdepth, flt0,
                                   flt0_stride, flt1, flt1_stride, xqd, params);
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
                              int src_stride, const uint8_t *dat8,
                              int dat_stride, int use_highbitdepth,
                              int32_t *flt0, int flt0_stride, int32_t *flt1,
                              int flt1_stride, int *xq,
                              const sgr_params_type *params) {
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
        const double f1 =
            (params->r[0] > 0) ? (double)flt0[i * flt0_stride + j] - u : 0;
        const double f2 =
            (params->r[1] > 0) ? (double)flt1[i * flt1_stride + j] - u : 0;
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
        const double f1 =
            (params->r[0] > 0) ? (double)flt0[i * flt0_stride + j] - u : 0;
        const double f2 =
            (params->r[1] > 0) ? (double)flt1[i * flt1_stride + j] - u : 0;
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
  if (params->r[0] == 0) {
    // H matrix is now only the scalar H[1][1]
    // C vector is now only the scalar C[1]
    Det = H[1][1];
    if (Det < 1e-8) return;  // ill-posed, return default values
    x[0] = 0;
    x[1] = C[1] / Det;

    xq[0] = 0;
    xq[1] = (int)rint(x[1] * (1 << SGRPROJ_PRJ_BITS));
  } else if (params->r[1] == 0) {
    // H matrix is now only the scalar H[0][0]
    // C vector is now only the scalar C[0]
    Det = H[0][0];
    if (Det < 1e-8) return;  // ill-posed, return default values
    x[0] = C[0] / Det;
    x[1] = 0;

    xq[0] = (int)rint(x[0] * (1 << SGRPROJ_PRJ_BITS));
    xq[1] = 0;
  } else {
    Det = (H[0][0] * H[1][1] - H[0][1] * H[1][0]);
    if (Det < 1e-8) return;  // ill-posed, return default values
    x[0] = (H[1][1] * C[0] - H[0][1] * C[1]) / Det;
    x[1] = (H[0][0] * C[1] - H[1][0] * C[0]) / Det;

    xq[0] = (int)rint(x[0] * (1 << SGRPROJ_PRJ_BITS));
    xq[1] = (int)rint(x[1] * (1 << SGRPROJ_PRJ_BITS));
  }
}

void encode_xq(int *xq, int *xqd, const sgr_params_type *params) {
  if (params->r[0] == 0) {
    xqd[0] = 0;
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xq[1], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  } else if (params->r[1] == 0) {
    xqd[0] = clamp(xq[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xqd[0], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  } else {
    xqd[0] = clamp(xq[0], SGRPROJ_PRJ_MIN0, SGRPROJ_PRJ_MAX0);
    xqd[1] = clamp((1 << SGRPROJ_PRJ_BITS) - xqd[0] - xq[1], SGRPROJ_PRJ_MIN1,
                   SGRPROJ_PRJ_MAX1);
  }
}

// Apply the self-guided filter across an entire restoration unit.
static void apply_sgr(int sgr_params_idx, const uint8_t *dat8, int width,
                      int height, int dat_stride, int use_highbd, int bit_depth,
                      int pu_width, int pu_height, int32_t *flt0, int32_t *flt1,
                      int flt_stride) {
  for (int i = 0; i < height; i += pu_height) {
    const int h = AOMMIN(pu_height, height - i);
    int32_t *flt0_row = flt0 + i * flt_stride;
    int32_t *flt1_row = flt1 + i * flt_stride;
    const uint8_t *dat8_row = dat8 + i * dat_stride;

    // Iterate over the stripe in blocks of width pu_width
    for (int j = 0; j < width; j += pu_width) {
      const int w = AOMMIN(pu_width, width - j);
      av1_selfguided_restoration(dat8_row + j, w, h, dat_stride, flt0_row + j,
                                 flt1_row + j, flt_stride, sgr_params_idx,
                                 bit_depth, use_highbd);
    }
  }
}

static SgrprojInfo search_selfguided_restoration(
    const uint8_t *dat8, int width, int height, int dat_stride,
    const uint8_t *src8, int src_stride, int use_highbitdepth, int bit_depth,
    int pu_width, int pu_height, int32_t *rstbuf) {
  int32_t *flt0 = rstbuf;
  int32_t *flt1 = flt0 + RESTORATION_UNITPELS_MAX;
  int ep, bestep = 0;
  int64_t besterr = -1;
  int exqd[2], bestxqd[2] = { 0, 0 };
  int flt_stride = ((width + 7) & ~7) + 8;
  assert(pu_width == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_width == RESTORATION_PROC_UNIT_SIZE);
  assert(pu_height == (RESTORATION_PROC_UNIT_SIZE >> 1) ||
         pu_height == RESTORATION_PROC_UNIT_SIZE);

  for (ep = 0; ep < SGRPROJ_PARAMS; ep++) {
    int exq[2];
    apply_sgr(ep, dat8, width, height, dat_stride, use_highbitdepth, bit_depth,
              pu_width, pu_height, flt0, flt1, flt_stride);
    aom_clear_system_state();
    const sgr_params_type *const params = &sgr_params[ep];
    get_proj_subspace(src8, width, height, src_stride, dat8, dat_stride,
                      use_highbitdepth, flt0, flt_stride, flt1, flt_stride, exq,
                      params);
    aom_clear_system_state();
    encode_xq(exq, exqd, params);
    int64_t err = finer_search_pixel_proj_error(
        src8, width, height, src_stride, dat8, dat_stride, use_highbitdepth,
        flt0, flt_stride, flt1, flt_stride, 2, exqd, params);
    if (besterr == -1 || err < besterr) {
      bestep = ep;
      besterr = err;
      bestxqd[0] = exqd[0];
      bestxqd[1] = exqd[1];
    }
  }

  SgrprojInfo ret;
  ret.ep = bestep;
  ret.xqd[0] = bestxqd[0];
  ret.xqd[1] = bestxqd[1];
  return ret;
}

static int count_sgrproj_bits(SgrprojInfo *sgrproj_info,
                              SgrprojInfo *ref_sgrproj_info) {
  int bits = SGRPROJ_PARAMS_BITS;
  const sgr_params_type *params = &sgr_params[sgrproj_info->ep];
  if (params->r[0] > 0)
    bits += aom_count_primitive_refsubexpfin(
        SGRPROJ_PRJ_MAX0 - SGRPROJ_PRJ_MIN0 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0,
        sgrproj_info->xqd[0] - SGRPROJ_PRJ_MIN0);
  if (params->r[1] > 0)
    bits += aom_count_primitive_refsubexpfin(
        SGRPROJ_PRJ_MAX1 - SGRPROJ_PRJ_MIN1 + 1, SGRPROJ_PRJ_SUBEXP_K,
        ref_sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1,
        sgrproj_info->xqd[1] - SGRPROJ_PRJ_MIN1);
  return bits;
}

static void search_sgrproj(const RestorationTileLimits *limits,
                           const AV1PixelRect *tile, int rest_unit_idx,
                           void *priv, int32_t *tmpbuf,
                           RestorationLineBuffers *rlbs) {
  (void)rlbs;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const MACROBLOCK *const x = rsc->x;
  const AV1_COMMON *const cm = rsc->cm;
  const int highbd = cm->use_highbitdepth;
  const int bit_depth = cm->bit_depth;

  uint8_t *dgd_start =
      rsc->dgd_buffer + limits->v_start * rsc->dgd_stride + limits->h_start;
  const uint8_t *src_start =
      rsc->src_buffer + limits->v_start * rsc->src_stride + limits->h_start;

  const int is_uv = rsc->plane > 0;
  const int ss_x = is_uv && cm->subsampling_x;
  const int ss_y = is_uv && cm->subsampling_y;
  const int procunit_width = RESTORATION_PROC_UNIT_SIZE >> ss_x;
  const int procunit_height = RESTORATION_PROC_UNIT_SIZE >> ss_y;

  rusi->sgrproj = search_selfguided_restoration(
      dgd_start, limits->h_end - limits->h_start,
      limits->v_end - limits->v_start, rsc->dgd_stride, src_start,
      rsc->src_stride, highbd, bit_depth, procunit_width, procunit_height,
      tmpbuf);

  RestorationUnitInfo rui;
  rui.restoration_type = RESTORE_SGRPROJ;
  rui.sgrproj_info = rusi->sgrproj;

  rusi->sse[RESTORE_SGRPROJ] = try_restoration_unit(rsc, limits, tile, &rui);

  const int64_t bits_none = x->sgrproj_restore_cost[0];
  const int64_t bits_sgr = x->sgrproj_restore_cost[1] +
                           (count_sgrproj_bits(&rusi->sgrproj, &rsc->sgrproj)
                            << AV1_PROB_COST_SHIFT);

  double cost_none =
      RDCOST_DBL(x->rdmult, bits_none >> 4, rusi->sse[RESTORE_NONE]);
  double cost_sgr =
      RDCOST_DBL(x->rdmult, bits_sgr >> 4, rusi->sse[RESTORE_SGRPROJ]);
  if (rusi->sgrproj.ep < 10)
    cost_sgr *= (1 + DUAL_SGR_PENALTY_MULT * rsc->sf->dual_sgr_penalty_level);

  RestorationType rtype =
      (cost_sgr < cost_none) ? RESTORE_SGRPROJ : RESTORE_NONE;
  rusi->best_rtype[RESTORE_SGRPROJ - 1] = rtype;

  rsc->sse += rusi->sse[rtype];
  rsc->bits += (cost_sgr < cost_none) ? bits_sgr : bits_none;
  if (cost_sgr < cost_none) rsc->sgrproj = rusi->sgrproj;
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

static AOM_FORCE_INLINE void compute_stats_highbd(
    int wiener_win, const uint8_t *dgd8, const uint8_t *src8, int h_start,
    int h_end, int v_start, int v_end, int dgd_stride, int src_stride,
    double *M, double *H) {
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
        double Yk = Y[k];
        M[k] += Yk * X;
        double *H2 = &H[k * wiener_win2];
        H2[k] += Yk * Yk;
        for (l = k + 1; l < wiener_win2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H2[l] += Yk * Y[l];
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
static int64_t finer_tile_search_wiener(const RestSearchCtxt *rsc,
                                        const RestorationTileLimits *limits,
                                        const AV1PixelRect *tile,
                                        RestorationUnitInfo *rui,
                                        int wiener_win) {
  const int plane_off = (WIENER_WIN - wiener_win) >> 1;
  int64_t err = try_restoration_unit(rsc, limits, tile, rui);
#if USE_WIENER_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP1_MINV,
                    WIENER_FILT_TAP2_MINV };
  int tap_max[] = { WIENER_FILT_TAP0_MAXV, WIENER_FILT_TAP1_MAXV,
                    WIENER_FILT_TAP2_MAXV };

  WienerInfo *plane_wiener = &rui->wiener_info;

  // printf("err  pre = %"PRId64"\n", err);
  const int start_step = 4;
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = plane_off; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (plane_wiener->hfilter[p] - s >= tap_min[p]) {
          plane_wiener->hfilter[p] -= s;
          plane_wiener->hfilter[WIENER_WIN - p - 1] -= s;
          plane_wiener->hfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_unit(rsc, limits, tile, rui);
          if (err2 > err) {
            plane_wiener->hfilter[p] += s;
            plane_wiener->hfilter[WIENER_WIN - p - 1] += s;
            plane_wiener->hfilter[WIENER_HALFWIN] -= 2 * s;
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
        if (plane_wiener->hfilter[p] + s <= tap_max[p]) {
          plane_wiener->hfilter[p] += s;
          plane_wiener->hfilter[WIENER_WIN - p - 1] += s;
          plane_wiener->hfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_unit(rsc, limits, tile, rui);
          if (err2 > err) {
            plane_wiener->hfilter[p] -= s;
            plane_wiener->hfilter[WIENER_WIN - p - 1] -= s;
            plane_wiener->hfilter[WIENER_HALFWIN] += 2 * s;
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
        if (plane_wiener->vfilter[p] - s >= tap_min[p]) {
          plane_wiener->vfilter[p] -= s;
          plane_wiener->vfilter[WIENER_WIN - p - 1] -= s;
          plane_wiener->vfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_unit(rsc, limits, tile, rui);
          if (err2 > err) {
            plane_wiener->vfilter[p] += s;
            plane_wiener->vfilter[WIENER_WIN - p - 1] += s;
            plane_wiener->vfilter[WIENER_HALFWIN] -= 2 * s;
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
        if (plane_wiener->vfilter[p] + s <= tap_max[p]) {
          plane_wiener->vfilter[p] += s;
          plane_wiener->vfilter[WIENER_WIN - p - 1] += s;
          plane_wiener->vfilter[WIENER_HALFWIN] -= 2 * s;
          err2 = try_restoration_unit(rsc, limits, tile, rui);
          if (err2 > err) {
            plane_wiener->vfilter[p] -= s;
            plane_wiener->vfilter[WIENER_WIN - p - 1] -= s;
            plane_wiener->vfilter[WIENER_HALFWIN] += 2 * s;
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

static void search_wiener(const RestorationTileLimits *limits,
                          const AV1PixelRect *tile_rect, int rest_unit_idx,
                          void *priv, int32_t *tmpbuf,
                          RestorationLineBuffers *rlbs) {
  (void)tmpbuf;
  (void)rlbs;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const int wiener_win =
      (rsc->plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  double M[WIENER_WIN2];
  double H[WIENER_WIN2 * WIENER_WIN2];
  double vfilterd[WIENER_WIN], hfilterd[WIENER_WIN];

  const AV1_COMMON *const cm = rsc->cm;
  if (cm->use_highbitdepth) {
    compute_stats_highbd(wiener_win, rsc->dgd_buffer, rsc->src_buffer,
                         limits->h_start, limits->h_end, limits->v_start,
                         limits->v_end, rsc->dgd_stride, rsc->src_stride, M, H);
  } else {
    compute_stats(wiener_win, rsc->dgd_buffer, rsc->src_buffer, limits->h_start,
                  limits->h_end, limits->v_start, limits->v_end,
                  rsc->dgd_stride, rsc->src_stride, M, H);
  }

  const MACROBLOCK *const x = rsc->x;
  const int64_t bits_none = x->wiener_restore_cost[0];

  if (!wiener_decompose_sep_sym(wiener_win, M, H, vfilterd, hfilterd)) {
    rsc->bits += bits_none;
    rsc->sse += rusi->sse[RESTORE_NONE];
    rusi->best_rtype[RESTORE_WIENER - 1] = RESTORE_NONE;
    rusi->sse[RESTORE_WIENER] = INT64_MAX;
    return;
  }

  RestorationUnitInfo rui;
  memset(&rui, 0, sizeof(rui));
  rui.restoration_type = RESTORE_WIENER;
  quantize_sym_filter(wiener_win, vfilterd, rui.wiener_info.vfilter);
  quantize_sym_filter(wiener_win, hfilterd, rui.wiener_info.hfilter);

  // Filter score computes the value of the function x'*A*x - x'*b for the
  // learned filter and compares it against identity filer. If there is no
  // reduction in the function, the filter is reverted back to identity
  if (compute_score(wiener_win, M, H, rui.wiener_info.vfilter,
                    rui.wiener_info.hfilter) > 0) {
    rsc->bits += bits_none;
    rsc->sse += rusi->sse[RESTORE_NONE];
    rusi->best_rtype[RESTORE_WIENER - 1] = RESTORE_NONE;
    rusi->sse[RESTORE_WIENER] = INT64_MAX;
    return;
  }

  aom_clear_system_state();

  rusi->sse[RESTORE_WIENER] =
      finer_tile_search_wiener(rsc, limits, tile_rect, &rui, wiener_win);
  rusi->wiener = rui.wiener_info;

  if (wiener_win != WIENER_WIN) {
    assert(rui.wiener_info.vfilter[0] == 0 &&
           rui.wiener_info.vfilter[WIENER_WIN - 1] == 0);
    assert(rui.wiener_info.hfilter[0] == 0 &&
           rui.wiener_info.hfilter[WIENER_WIN - 1] == 0);
  }

  const int64_t bits_wiener =
      x->wiener_restore_cost[1] +
      (count_wiener_bits(wiener_win, &rusi->wiener, &rsc->wiener)
       << AV1_PROB_COST_SHIFT);

  double cost_none =
      RDCOST_DBL(x->rdmult, bits_none >> 4, rusi->sse[RESTORE_NONE]);
  double cost_wiener =
      RDCOST_DBL(x->rdmult, bits_wiener >> 4, rusi->sse[RESTORE_WIENER]);

  RestorationType rtype =
      (cost_wiener < cost_none) ? RESTORE_WIENER : RESTORE_NONE;
  rusi->best_rtype[RESTORE_WIENER - 1] = rtype;

  rsc->sse += rusi->sse[rtype];
  rsc->bits += (cost_wiener < cost_none) ? bits_wiener : bits_none;
  if (cost_wiener < cost_none) rsc->wiener = rusi->wiener;
}

static void search_norestore(const RestorationTileLimits *limits,
                             const AV1PixelRect *tile_rect, int rest_unit_idx,
                             void *priv, int32_t *tmpbuf,
                             RestorationLineBuffers *rlbs) {
  (void)tile_rect;
  (void)tmpbuf;
  (void)rlbs;

  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const int highbd = rsc->cm->use_highbitdepth;
  rusi->sse[RESTORE_NONE] = sse_restoration_unit(
      limits, rsc->src, rsc->cm->frame_to_show, rsc->plane, highbd);

  rsc->sse += rusi->sse[RESTORE_NONE];
}

static void search_switchable(const RestorationTileLimits *limits,
                              const AV1PixelRect *tile_rect, int rest_unit_idx,
                              void *priv, int32_t *tmpbuf,
                              RestorationLineBuffers *rlbs) {
  (void)limits;
  (void)tile_rect;
  (void)tmpbuf;
  (void)rlbs;
  RestSearchCtxt *rsc = (RestSearchCtxt *)priv;
  RestUnitSearchInfo *rusi = &rsc->rusi[rest_unit_idx];

  const MACROBLOCK *const x = rsc->x;

  const int wiener_win =
      (rsc->plane == AOM_PLANE_Y) ? WIENER_WIN : WIENER_WIN_CHROMA;

  double best_cost = 0;
  int64_t best_bits = 0;
  RestorationType best_rtype = RESTORE_NONE;

  for (RestorationType r = 0; r < RESTORE_SWITCHABLE_TYPES; ++r) {
    // Check for the condition that wiener or sgrproj search could not
    // find a solution or the solution was worse than RESTORE_NONE.
    // In either case the best_rtype will be set as RESTORE_NONE. These
    // should be skipped from the test below.
    if (r > RESTORE_NONE) {
      if (rusi->best_rtype[r - 1] == RESTORE_NONE) continue;
    }

    const int64_t sse = rusi->sse[r];
    int64_t coeff_pcost = 0;
    switch (r) {
      case RESTORE_NONE: coeff_pcost = 0; break;
      case RESTORE_WIENER:
        coeff_pcost =
            count_wiener_bits(wiener_win, &rusi->wiener, &rsc->wiener);
        break;
      case RESTORE_SGRPROJ:
        coeff_pcost = count_sgrproj_bits(&rusi->sgrproj, &rsc->sgrproj);
        break;
      default: assert(0); break;
    }
    const int64_t coeff_bits = coeff_pcost << AV1_PROB_COST_SHIFT;
    const int64_t bits = x->switchable_restore_cost[r] + coeff_bits;
    double cost = RDCOST_DBL(x->rdmult, bits >> 4, sse);
    if (r == RESTORE_SGRPROJ && rusi->sgrproj.ep < 10)
      cost *= (1 + DUAL_SGR_PENALTY_MULT * rsc->sf->dual_sgr_penalty_level);
    if (r == 0 || cost < best_cost) {
      best_cost = cost;
      best_bits = bits;
      best_rtype = r;
    }
  }

  rusi->best_rtype[RESTORE_SWITCHABLE - 1] = best_rtype;

  rsc->sse += rusi->sse[best_rtype];
  rsc->bits += best_bits;
  if (best_rtype == RESTORE_WIENER) rsc->wiener = rusi->wiener;
  if (best_rtype == RESTORE_SGRPROJ) rsc->sgrproj = rusi->sgrproj;
}

static void copy_unit_info(RestorationType frame_rtype,
                           const RestUnitSearchInfo *rusi,
                           RestorationUnitInfo *rui) {
  assert(frame_rtype > 0);
  rui->restoration_type = rusi->best_rtype[frame_rtype - 1];
  if (rui->restoration_type == RESTORE_WIENER)
    rui->wiener_info = rusi->wiener;
  else
    rui->sgrproj_info = rusi->sgrproj;
}

static double search_rest_type(RestSearchCtxt *rsc, RestorationType rtype) {
  static const rest_unit_visitor_t funs[RESTORE_TYPES] = {
    search_norestore, search_wiener, search_sgrproj, search_switchable
  };

  reset_rsc(rsc);
  rsc_on_tile(LR_TILE_ROW, LR_TILE_COL, rsc);
  av1_foreach_rest_unit_in_plane(rsc->cm, rsc->plane, funs[rtype], rsc,
                                 &rsc->tile_rect, rsc->cm->rst_tmpbuf, NULL);
  return RDCOST_DBL(rsc->x->rdmult, rsc->bits >> 4, rsc->sse);
}

static int rest_tiles_in_plane(const AV1_COMMON *cm, int plane) {
  const RestorationInfo *rsi = &cm->rst_info[plane];
  return rsi->units_per_tile;
}

void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  assert(!cm->all_lossless);

  int ntiles[2];
  for (int is_uv = 0; is_uv < 2; ++is_uv)
    ntiles[is_uv] = rest_tiles_in_plane(cm, is_uv);

  assert(ntiles[1] <= ntiles[0]);
  RestUnitSearchInfo *rusi =
      (RestUnitSearchInfo *)aom_memalign(16, sizeof(*rusi) * ntiles[0]);

  // If the restoration unit dimensions are not multiples of
  // rsi->restoration_unit_size then some elements of the rusi array may be
  // left uninitialised when we reach copy_unit_info(...). This is not a
  // problem, as these elements are ignored later, but in order to quiet
  // Valgrind's warnings we initialise the array below.
  memset(rusi, 0, sizeof(*rusi) * ntiles[0]);

  RestSearchCtxt rsc;
  const int plane_start = AOM_PLANE_Y;
  const int plane_end = num_planes > 1 ? AOM_PLANE_V : AOM_PLANE_Y;
  for (int plane = plane_start; plane <= plane_end; ++plane) {
    init_rsc(src, &cpi->common, &cpi->td.mb, &cpi->sf, plane, rusi,
             &cpi->trial_frame_rst, &rsc);

    const int plane_ntiles = ntiles[plane > 0];
    const RestorationType num_rtypes =
        (plane_ntiles > 1) ? RESTORE_TYPES : RESTORE_SWITCHABLE_TYPES;

    double best_cost = 0;
    RestorationType best_rtype = RESTORE_NONE;

    const int highbd = rsc.cm->use_highbitdepth;
    extend_frame(rsc.dgd_buffer, rsc.plane_width, rsc.plane_height,
                 rsc.dgd_stride, RESTORATION_BORDER, RESTORATION_BORDER,
                 highbd);

    for (RestorationType r = 0; r < num_rtypes; ++r) {
      if ((force_restore_type != RESTORE_TYPES) && (r != RESTORE_NONE) &&
          (r != force_restore_type))
        continue;

      double cost = search_rest_type(&rsc, r);

      if (r == 0 || cost < best_cost) {
        best_cost = cost;
        best_rtype = r;
      }
    }

    cm->rst_info[plane].frame_restoration_type = best_rtype;
    if (force_restore_type != RESTORE_TYPES)
      assert(best_rtype == force_restore_type || best_rtype == RESTORE_NONE);

    if (best_rtype != RESTORE_NONE) {
      for (int u = 0; u < plane_ntiles; ++u) {
        copy_unit_info(best_rtype, &rusi[u], &cm->rst_info[plane].unit_info[u]);
      }
    }
  }

  aom_free(rusi);
}
