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
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"
#include "av1/encoder/mathutils.h"

// When set to RESTORE_WIENER or RESTORE_SGRPROJ only those are allowed.
// When set to RESTORE_NONE (0) we allow switchable.
const RestorationType force_restore_type = RESTORE_NONE;

// Number of Wiener iterations
#define NUM_WIENER_ITERS 5

typedef double (*search_restore_type)(const YV12_BUFFER_CONFIG *src,
                                      AV1_COMP *cpi, int partial_frame,
                                      int plane, RestorationInfo *info,
                                      RestorationType *rest_level,
                                      double *best_tile_cost,
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
                                    int tile_idx, int subtile_idx,
                                    int subtile_bits,
                                    YV12_BUFFER_CONFIG *dst_frame) {
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;
  int tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
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
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, nhtiles,
                           nvtiles, tile_width, tile_height, width, height, 0,
                           0, &h_start, &h_end, &v_start, &v_end);
  filt_err = sse_restoration_tile(src, dst_frame, cm, h_start, h_end - h_start,
                                  v_start, v_end - v_start, components_pattern);

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

static int64_t get_pixel_proj_error(uint8_t *src8, int width, int height,
                                    int src_stride, uint8_t *dat8,
                                    int dat_stride, int bit_depth,
                                    int32_t *flt1, int flt1_stride,
                                    int32_t *flt2, int flt2_stride, int *xqd) {
  int i, j;
  int64_t err = 0;
  int xq[2];
  decode_xq(xqd, xq);
  if (bit_depth == 8) {
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
    uint8_t *src8, int width, int height, int src_stride, uint8_t *dat8,
    int dat_stride, int bit_depth, int32_t *flt1, int flt1_stride,
    int32_t *flt2, int flt2_stride, int start_step, int *xqd) {
  int64_t err = get_pixel_proj_error(src8, width, height, src_stride, dat8,
                                     dat_stride, bit_depth, flt1, flt1_stride,
                                     flt2, flt2_stride, xqd);
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
                                      dat_stride, bit_depth, flt1, flt1_stride,
                                      flt2, flt2_stride, xqd);
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
                                      dat_stride, bit_depth, flt1, flt1_stride,
                                      flt2, flt2_stride, xqd);
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

static void get_proj_subspace(uint8_t *src8, int width, int height,
                              int src_stride, uint8_t *dat8, int dat_stride,
                              int bit_depth, int32_t *flt1, int flt1_stride,
                              int32_t *flt2, int flt2_stride, int *xq) {
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
  if (bit_depth == 8) {
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
                                          int dat_stride, uint8_t *src8,
                                          int src_stride, int bit_depth,
                                          int *eps, int *xqd, int32_t *rstbuf) {
  int32_t *flt1 = rstbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int32_t *tmpbuf2 = flt2 + RESTORATION_TILEPELS_MAX;
  int ep, bestep = 0;
  int64_t err, besterr = -1;
  int exqd[2], bestxqd[2] = { 0, 0 };

  for (ep = 0; ep < SGRPROJ_PARAMS; ep++) {
    int exq[2];
#if CONFIG_HIGHBITDEPTH
    if (bit_depth > 8) {
      uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
#if USE_HIGHPASS_IN_SGRPROJ
      av1_highpass_filter_highbd(dat, width, height, dat_stride, flt1, width,
                                 sgr_params[ep].corner, sgr_params[ep].edge);
#else
      av1_selfguided_restoration_highbd(dat, width, height, dat_stride, flt1,
                                        width, bit_depth, sgr_params[ep].r1,
                                        sgr_params[ep].e1, tmpbuf2);
#endif  // USE_HIGHPASS_IN_SGRPROJ
      av1_selfguided_restoration_highbd(dat, width, height, dat_stride, flt2,
                                        width, bit_depth, sgr_params[ep].r2,
                                        sgr_params[ep].e2, tmpbuf2);
    } else {
#endif
#if USE_HIGHPASS_IN_SGRPROJ
      av1_highpass_filter(dat8, width, height, dat_stride, flt1, width,
                          sgr_params[ep].corner, sgr_params[ep].edge);
#else
    av1_selfguided_restoration(dat8, width, height, dat_stride, flt1, width,
                               sgr_params[ep].r1, sgr_params[ep].e1, tmpbuf2);
#endif  // USE_HIGHPASS_IN_SGRPROJ
      av1_selfguided_restoration(dat8, width, height, dat_stride, flt2, width,
                                 sgr_params[ep].r2, sgr_params[ep].e2, tmpbuf2);
#if CONFIG_HIGHBITDEPTH
    }
#endif
    aom_clear_system_state();
    get_proj_subspace(src8, width, height, src_stride, dat8, dat_stride,
                      bit_depth, flt1, width, flt2, width, exq);
    aom_clear_system_state();
    encode_xq(exq, exqd);
    err = finer_search_pixel_proj_error(src8, width, height, src_stride, dat8,
                                        dat_stride, bit_depth, flt1, width,
                                        flt2, width, 2, exqd);
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

static double search_sgrproj(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                             int partial_frame, int plane,
                             RestorationInfo *info, RestorationType *type,
                             double *best_tile_cost,
                             YV12_BUFFER_CONFIG *dst_frame) {
  SgrprojInfo *sgrproj_info = info->sgrproj_info;
  double err, cost_norestore, cost_sgrproj;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  RestorationInfo *rsi = &cpi->rst_search[0];
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  int width, height, src_stride, dgd_stride;
  uint8_t *dgd_buffer, *src_buffer;
  if (plane == AOM_PLANE_Y) {
    width = src->y_crop_width;
    height = src->y_crop_height;
    src_buffer = src->y_buffer;
    src_stride = src->y_stride;
    dgd_buffer = dgd->y_buffer;
    dgd_stride = dgd->y_stride;
    assert(width == dgd->y_crop_width);
    assert(height == dgd->y_crop_height);
    assert(width == src->y_crop_width);
    assert(height == src->y_crop_height);
  } else {
    width = src->uv_crop_width;
    height = src->uv_crop_height;
    src_stride = src->uv_stride;
    dgd_stride = dgd->uv_stride;
    src_buffer = plane == AOM_PLANE_U ? src->u_buffer : src->v_buffer;
    dgd_buffer = plane == AOM_PLANE_U ? dgd->u_buffer : dgd->v_buffer;
    assert(width == dgd->uv_crop_width);
    assert(height == dgd->uv_crop_height);
  }
  const int ntiles =
      av1_get_rest_ntiles(width, height, cm->rst_info[0].restoration_tilesize,
                          &tile_width, &tile_height, &nhtiles, &nvtiles);
  SgrprojInfo ref_sgrproj_info;
  set_default_sgrproj(&ref_sgrproj_info);

  rsi[plane].frame_restoration_type = RESTORE_SGRPROJ;

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    rsi[plane].restoration_type[tile_idx] = RESTORE_NONE;
  }
  // Compute best Sgrproj filters for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 0, 0, &h_start, &h_end,
                             &v_start, &v_end);
    err = sse_restoration_tile(src, cm->frame_to_show, cm, h_start,
                               h_end - h_start, v_start, v_end - v_start,
                               (1 << plane));
    // #bits when a tile is not restored
    bits = av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 0);
    cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
    best_tile_cost[tile_idx] = DBL_MAX;
    search_selfguided_restoration(
        dgd_buffer + v_start * dgd_stride + h_start, h_end - h_start,
        v_end - v_start, dgd_stride,
        src_buffer + v_start * src_stride + h_start, src_stride,
#if CONFIG_HIGHBITDEPTH
        cm->bit_depth,
#else
        8,
#endif  // CONFIG_HIGHBITDEPTH
        &rsi[plane].sgrproj_info[tile_idx].ep,
        rsi[plane].sgrproj_info[tile_idx].xqd, cm->rst_internal.tmpbuf);
    rsi[plane].restoration_type[tile_idx] = RESTORE_SGRPROJ;
    err = try_restoration_tile(src, cpi, rsi, (1 << plane), partial_frame,
                               tile_idx, 0, 0, dst_frame);
    bits = count_sgrproj_bits(&rsi[plane].sgrproj_info[tile_idx],
                              &ref_sgrproj_info)
           << AV1_PROB_COST_SHIFT;
    bits += av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, 1);
    cost_sgrproj = RDCOST_DBL(x->rdmult, (bits >> 4), err);
    if (cost_sgrproj >= cost_norestore) {
      type[tile_idx] = RESTORE_NONE;
    } else {
      type[tile_idx] = RESTORE_SGRPROJ;
      memcpy(&sgrproj_info[tile_idx], &rsi[plane].sgrproj_info[tile_idx],
             sizeof(sgrproj_info[tile_idx]));
      memcpy(&ref_sgrproj_info, &sgrproj_info[tile_idx],
             sizeof(ref_sgrproj_info));
      best_tile_cost[tile_idx] = err;
    }
    rsi[plane].restoration_type[tile_idx] = RESTORE_NONE;
  }
  // Cost for Sgrproj filtering
  set_default_sgrproj(&ref_sgrproj_info);
  bits = frame_level_restore_bits[rsi[plane].frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    bits +=
        av1_cost_bit(RESTORE_NONE_SGRPROJ_PROB, type[tile_idx] != RESTORE_NONE);
    memcpy(&rsi[plane].sgrproj_info[tile_idx], &sgrproj_info[tile_idx],
           sizeof(sgrproj_info[tile_idx]));
    if (type[tile_idx] == RESTORE_SGRPROJ) {
      bits += count_sgrproj_bits(&rsi[plane].sgrproj_info[tile_idx],
                                 &ref_sgrproj_info)
              << AV1_PROB_COST_SHIFT;
      memcpy(&ref_sgrproj_info, &rsi[plane].sgrproj_info[tile_idx],
             sizeof(ref_sgrproj_info));
    }
    rsi[plane].restoration_type[tile_idx] = type[tile_idx];
  }
  err = try_restoration_frame(src, cpi, rsi, (1 << plane), partial_frame,
                              dst_frame);
  cost_sgrproj = RDCOST_DBL(x->rdmult, (bits >> 4), err);

  return cost_sgrproj;
}

static double find_average(uint8_t *src, int h_start, int h_end, int v_start,
                           int v_end, int stride) {
  uint64_t sum = 0;
  double avg = 0;
  int i, j;
  aom_clear_system_state();
  for (i = v_start; i < v_end; i++)
    for (j = h_start; j < h_end; j++) sum += src[i * stride + j];
  avg = (double)sum / ((v_end - v_start) * (h_end - h_start));
  return avg;
}

static void compute_stats(uint8_t *dgd, uint8_t *src, int h_start, int h_end,
                          int v_start, int v_end, int dgd_stride,
                          int src_stride, double *M, double *H) {
  int i, j, k, l;
  double Y[WIENER_WIN2];
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * WIENER_WIN2);
  memset(H, 0, sizeof(*H) * WIENER_WIN2 * WIENER_WIN2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -WIENER_HALFWIN; k <= WIENER_HALFWIN; k++) {
        for (l = -WIENER_HALFWIN; l <= WIENER_HALFWIN; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      for (k = 0; k < WIENER_WIN2; ++k) {
        M[k] += Y[k] * X;
        H[k * WIENER_WIN2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < WIENER_WIN2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H[k * WIENER_WIN2 + l] += Y[k] * Y[l];
        }
      }
    }
  }
  for (k = 0; k < WIENER_WIN2; ++k) {
    for (l = k + 1; l < WIENER_WIN2; ++l) {
      H[l * WIENER_WIN2 + k] = H[k * WIENER_WIN2 + l];
    }
  }
}

#if CONFIG_HIGHBITDEPTH
static double find_average_highbd(uint16_t *src, int h_start, int h_end,
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

static void compute_stats_highbd(uint8_t *dgd8, uint8_t *src8, int h_start,
                                 int h_end, int v_start, int v_end,
                                 int dgd_stride, int src_stride, double *M,
                                 double *H) {
  int i, j, k, l;
  double Y[WIENER_WIN2];
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const double avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  memset(M, 0, sizeof(*M) * WIENER_WIN2);
  memset(H, 0, sizeof(*H) * WIENER_WIN2 * WIENER_WIN2);
  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j++) {
      const double X = (double)src[i * src_stride + j] - avg;
      int idx = 0;
      for (k = -WIENER_HALFWIN; k <= WIENER_HALFWIN; k++) {
        for (l = -WIENER_HALFWIN; l <= WIENER_HALFWIN; l++) {
          Y[idx] = (double)dgd[(i + l) * dgd_stride + (j + k)] - avg;
          idx++;
        }
      }
      for (k = 0; k < WIENER_WIN2; ++k) {
        M[k] += Y[k] * X;
        H[k * WIENER_WIN2 + k] += Y[k] * Y[k];
        for (l = k + 1; l < WIENER_WIN2; ++l) {
          // H is a symmetric matrix, so we only need to fill out the upper
          // triangle here. We can copy it down to the lower triangle outside
          // the (i, j) loops.
          H[k * WIENER_WIN2 + l] += Y[k] * Y[l];
        }
      }
    }
  }
  for (k = 0; k < WIENER_WIN2; ++k) {
    for (l = k + 1; l < WIENER_WIN2; ++l) {
      H[l * WIENER_WIN2 + k] = H[k * WIENER_WIN2 + l];
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static INLINE int wrap_index(int i) {
  return (i >= WIENER_HALFWIN1 ? WIENER_WIN - 1 - i : i);
}

// Fix vector b, update vector a
static void update_a_sep_sym(double **Mc, double **Hc, double *a, double *b) {
  int i, j;
  double S[WIENER_WIN];
  double A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  int w, w2;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < WIENER_WIN; i++) {
    for (j = 0; j < WIENER_WIN; ++j) {
      const int jj = wrap_index(j);
      A[jj] += Mc[i][j] * b[i];
    }
  }
  for (i = 0; i < WIENER_WIN; i++) {
    for (j = 0; j < WIENER_WIN; j++) {
      int k, l;
      for (k = 0; k < WIENER_WIN; ++k)
        for (l = 0; l < WIENER_WIN; ++l) {
          const int kk = wrap_index(k);
          const int ll = wrap_index(l);
          B[ll * WIENER_HALFWIN1 + kk] +=
              Hc[j * WIENER_WIN + i][k * WIENER_WIN2 + l] * b[i] * b[j];
        }
    }
  }
  // Normalization enforcement in the system of equations itself
  w = WIENER_WIN;
  w2 = (w >> 1) + 1;
  for (i = 0; i < w2 - 1; ++i)
    A[i] -=
        A[w2 - 1] * 2 + B[i * w2 + w2 - 1] - 2 * B[(w2 - 1) * w2 + (w2 - 1)];
  for (i = 0; i < w2 - 1; ++i)
    for (j = 0; j < w2 - 1; ++j)
      B[i * w2 + j] -= 2 * (B[i * w2 + (w2 - 1)] + B[(w2 - 1) * w2 + j] -
                            2 * B[(w2 - 1) * w2 + (w2 - 1)]);
  if (linsolve(w2 - 1, B, w2, A, S)) {
    S[w2 - 1] = 1.0;
    for (i = w2; i < w; ++i) {
      S[i] = S[w - 1 - i];
      S[w2 - 1] -= 2 * S[i];
    }
    memcpy(a, S, w * sizeof(*a));
  }
}

// Fix vector a, update vector b
static void update_b_sep_sym(double **Mc, double **Hc, double *a, double *b) {
  int i, j;
  double S[WIENER_WIN];
  double A[WIENER_HALFWIN1], B[WIENER_HALFWIN1 * WIENER_HALFWIN1];
  int w, w2;
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  for (i = 0; i < WIENER_WIN; i++) {
    const int ii = wrap_index(i);
    for (j = 0; j < WIENER_WIN; j++) A[ii] += Mc[i][j] * a[j];
  }

  for (i = 0; i < WIENER_WIN; i++) {
    for (j = 0; j < WIENER_WIN; j++) {
      const int ii = wrap_index(i);
      const int jj = wrap_index(j);
      int k, l;
      for (k = 0; k < WIENER_WIN; ++k)
        for (l = 0; l < WIENER_WIN; ++l)
          B[jj * WIENER_HALFWIN1 + ii] +=
              Hc[i * WIENER_WIN + j][k * WIENER_WIN2 + l] * a[k] * a[l];
    }
  }
  // Normalization enforcement in the system of equations itself
  w = WIENER_WIN;
  w2 = WIENER_HALFWIN1;
  for (i = 0; i < w2 - 1; ++i)
    A[i] -=
        A[w2 - 1] * 2 + B[i * w2 + w2 - 1] - 2 * B[(w2 - 1) * w2 + (w2 - 1)];
  for (i = 0; i < w2 - 1; ++i)
    for (j = 0; j < w2 - 1; ++j)
      B[i * w2 + j] -= 2 * (B[i * w2 + (w2 - 1)] + B[(w2 - 1) * w2 + j] -
                            2 * B[(w2 - 1) * w2 + (w2 - 1)]);
  if (linsolve(w2 - 1, B, w2, A, S)) {
    S[w2 - 1] = 1.0;
    for (i = w2; i < w; ++i) {
      S[i] = S[w - 1 - i];
      S[w2 - 1] -= 2 * S[i];
    }
    memcpy(b, S, w * sizeof(*b));
  }
}

static int wiener_decompose_sep_sym(double *M, double *H, double *a,
                                    double *b) {
  static const int init_filt[WIENER_WIN] = {
    WIENER_FILT_TAP0_MIDV, WIENER_FILT_TAP1_MIDV, WIENER_FILT_TAP2_MIDV,
    WIENER_FILT_TAP3_MIDV, WIENER_FILT_TAP2_MIDV, WIENER_FILT_TAP1_MIDV,
    WIENER_FILT_TAP0_MIDV,
  };
  int i, j, iter;
  double *Hc[WIENER_WIN2];
  double *Mc[WIENER_WIN];
  for (i = 0; i < WIENER_WIN; i++) {
    Mc[i] = M + i * WIENER_WIN;
    for (j = 0; j < WIENER_WIN; j++) {
      Hc[i * WIENER_WIN + j] =
          H + i * WIENER_WIN * WIENER_WIN2 + j * WIENER_WIN;
    }
  }
  for (i = 0; i < WIENER_WIN; i++) {
    a[i] = b[i] = (double)init_filt[i] / WIENER_FILT_STEP;
  }

  iter = 1;
  while (iter < NUM_WIENER_ITERS) {
    update_a_sep_sym(Mc, Hc, a, b);
    update_b_sep_sym(Mc, Hc, a, b);
    iter++;
  }
  return 1;
}

// Computes the function x'*H*x - x'*M for the learned 2D filter x, and compares
// against identity filters; Final score is defined as the difference between
// the function values
static double compute_score(double *M, double *H, InterpKernel vfilt,
                            InterpKernel hfilt) {
  double ab[WIENER_WIN * WIENER_WIN];
  int i, k, l;
  double P = 0, Q = 0;
  double iP = 0, iQ = 0;
  double Score, iScore;
  double a[WIENER_WIN], b[WIENER_WIN];

  aom_clear_system_state();

  a[WIENER_HALFWIN] = b[WIENER_HALFWIN] = 1.0;
  for (i = 0; i < WIENER_HALFWIN; ++i) {
    a[i] = a[WIENER_WIN - i - 1] = (double)vfilt[i] / WIENER_FILT_STEP;
    b[i] = b[WIENER_WIN - i - 1] = (double)hfilt[i] / WIENER_FILT_STEP;
    a[WIENER_HALFWIN] -= 2 * a[i];
    b[WIENER_HALFWIN] -= 2 * b[i];
  }
  for (k = 0; k < WIENER_WIN; ++k) {
    for (l = 0; l < WIENER_WIN; ++l) ab[k * WIENER_WIN + l] = a[l] * b[k];
  }
  for (k = 0; k < WIENER_WIN2; ++k) {
    P += ab[k] * M[k];
    for (l = 0; l < WIENER_WIN2; ++l)
      Q += ab[k] * H[k * WIENER_WIN2 + l] * ab[l];
  }
  Score = Q - 2 * P;

  iP = M[WIENER_WIN2 >> 1];
  iQ = H[(WIENER_WIN2 >> 1) * WIENER_WIN2 + (WIENER_WIN2 >> 1)];
  iScore = iQ - 2 * iP;

  return Score - iScore;
}

static void quantize_sym_filter(double *f, InterpKernel fi) {
  int i;
  for (i = 0; i < WIENER_HALFWIN; ++i) {
    fi[i] = RINT(f[i] * WIENER_FILT_STEP);
  }
  // Specialize for 7-tap filter
  fi[0] = CLIP(fi[0], WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP0_MAXV);
  fi[1] = CLIP(fi[1], WIENER_FILT_TAP1_MINV, WIENER_FILT_TAP1_MAXV);
  fi[2] = CLIP(fi[2], WIENER_FILT_TAP2_MINV, WIENER_FILT_TAP2_MAXV);
  // Satisfy filter constraints
  fi[WIENER_WIN - 1] = fi[0];
  fi[WIENER_WIN - 2] = fi[1];
  fi[WIENER_WIN - 3] = fi[2];
  // The central element has an implicit +WIENER_FILT_STEP
  fi[3] = -2 * (fi[0] + fi[1] + fi[2]);
}

static int count_wiener_bits(WienerInfo *wiener_info,
                             WienerInfo *ref_wiener_info) {
  int bits = 0;
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
                                        int start_step, int plane, int tile_idx,
                                        int partial_frame,
                                        YV12_BUFFER_CONFIG *dst_frame) {
  int64_t err = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                     tile_idx, 0, 0, dst_frame);
  (void)start_step;
#if USE_WIENER_REFINEMENT_SEARCH
  int64_t err2;
  int tap_min[] = { WIENER_FILT_TAP0_MINV, WIENER_FILT_TAP1_MINV,
                    WIENER_FILT_TAP2_MINV };
  int tap_max[] = { WIENER_FILT_TAP0_MAXV, WIENER_FILT_TAP1_MAXV,
                    WIENER_FILT_TAP2_MAXV };
  // printf("err  pre = %"PRId64"\n", err);
  for (int s = start_step; s >= 1; s >>= 1) {
    for (int p = 0; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (rsi[plane].wiener_info[tile_idx].hfilter[p] - s >= tap_min[p]) {
          rsi[plane].wiener_info[tile_idx].hfilter[p] -= s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_WIN - p - 1] -= s;
          rsi[plane].wiener_info[tile_idx].hfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, 0, 0, dst_frame);
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
                                      tile_idx, 0, 0, dst_frame);
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
    for (int p = 0; p < WIENER_HALFWIN; ++p) {
      int skip = 0;
      do {
        if (rsi[plane].wiener_info[tile_idx].vfilter[p] - s >= tap_min[p]) {
          rsi[plane].wiener_info[tile_idx].vfilter[p] -= s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_WIN - p - 1] -= s;
          rsi[plane].wiener_info[tile_idx].vfilter[WIENER_HALFWIN] += 2 * s;
          err2 = try_restoration_tile(src, cpi, rsi, 1 << plane, partial_frame,
                                      tile_idx, 0, 0, dst_frame);
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
                                      tile_idx, 0, 0, dst_frame);
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

static double search_wiener(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                            int partial_frame, int plane, RestorationInfo *info,
                            RestorationType *type, double *best_tile_cost,
                            YV12_BUFFER_CONFIG *dst_frame) {
  WienerInfo *wiener_info = info->wiener_info;
  AV1_COMMON *const cm = &cpi->common;
  RestorationInfo *rsi = cpi->rst_search;
  int64_t err;
  int bits;
  double cost_wiener, cost_norestore;
  MACROBLOCK *x = &cpi->td.mb;
  double M[WIENER_WIN2];
  double H[WIENER_WIN2 * WIENER_WIN2];
  double vfilterd[WIENER_WIN], hfilterd[WIENER_WIN];
  const YV12_BUFFER_CONFIG *dgd = cm->frame_to_show;
  int width, height, src_stride, dgd_stride;
  uint8_t *dgd_buffer, *src_buffer;
  if (plane == AOM_PLANE_Y) {
    width = src->y_crop_width;
    height = src->y_crop_height;
    src_buffer = src->y_buffer;
    src_stride = src->y_stride;
    dgd_buffer = dgd->y_buffer;
    dgd_stride = dgd->y_stride;
    assert(width == dgd->y_crop_width);
    assert(height == dgd->y_crop_height);
    assert(width == src->y_crop_width);
    assert(height == src->y_crop_height);
  } else {
    width = src->uv_crop_width;
    height = src->uv_crop_height;
    src_stride = src->uv_stride;
    dgd_stride = dgd->uv_stride;
    src_buffer = plane == AOM_PLANE_U ? src->u_buffer : src->v_buffer;
    dgd_buffer = plane == AOM_PLANE_U ? dgd->u_buffer : dgd->v_buffer;
    assert(width == dgd->uv_crop_width);
    assert(height == dgd->uv_crop_height);
  }
  double score;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
  const int ntiles = av1_get_rest_ntiles(
      width, height, cm->rst_info[plane].restoration_tilesize, &tile_width,
      &tile_height, &nhtiles, &nvtiles);
  WienerInfo ref_wiener_info;
  set_default_wiener(&ref_wiener_info);

  rsi[plane].frame_restoration_type = RESTORE_WIENER;

  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    rsi[plane].restoration_type[tile_idx] = RESTORE_NONE;
  }

// Construct a (WIENER_HALFWIN)-pixel border around the frame
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth)
    extend_frame_highbd(CONVERT_TO_SHORTPTR(dgd_buffer), width, height,
                        dgd_stride);
  else
#endif
    extend_frame(dgd_buffer, width, height, dgd_stride);

  // Compute best Wiener filters for each tile
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 0, 0, &h_start, &h_end,
                             &v_start, &v_end);
    err = sse_restoration_tile(src, cm->frame_to_show, cm, h_start,
                               h_end - h_start, v_start, v_end - v_start,
                               (1 << plane));
    // #bits when a tile is not restored
    bits = av1_cost_bit(RESTORE_NONE_WIENER_PROB, 0);
    cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
    best_tile_cost[tile_idx] = DBL_MAX;

    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 0, 0, &h_start, &h_end,
                             &v_start, &v_end);
#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth)
      compute_stats_highbd(dgd_buffer, src_buffer, h_start, h_end, v_start,
                           v_end, dgd_stride, src_stride, M, H);
    else
#endif  // CONFIG_HIGHBITDEPTH
      compute_stats(dgd_buffer, src_buffer, h_start, h_end, v_start, v_end,
                    dgd_stride, src_stride, M, H);

    type[tile_idx] = RESTORE_WIENER;

    if (!wiener_decompose_sep_sym(M, H, vfilterd, hfilterd)) {
      type[tile_idx] = RESTORE_NONE;
      continue;
    }
    quantize_sym_filter(vfilterd, rsi[plane].wiener_info[tile_idx].vfilter);
    quantize_sym_filter(hfilterd, rsi[plane].wiener_info[tile_idx].hfilter);

    // Filter score computes the value of the function x'*A*x - x'*b for the
    // learned filter and compares it against identity filer. If there is no
    // reduction in the function, the filter is reverted back to identity
    score = compute_score(M, H, rsi[plane].wiener_info[tile_idx].vfilter,
                          rsi[plane].wiener_info[tile_idx].hfilter);
    if (score > 0.0) {
      type[tile_idx] = RESTORE_NONE;
      continue;
    }
    aom_clear_system_state();

    rsi[plane].restoration_type[tile_idx] = RESTORE_WIENER;
    err = finer_tile_search_wiener(src, cpi, rsi, 4, plane, tile_idx,
                                   partial_frame, dst_frame);
    bits =
        count_wiener_bits(&rsi[plane].wiener_info[tile_idx], &ref_wiener_info)
        << AV1_PROB_COST_SHIFT;
    bits += av1_cost_bit(RESTORE_NONE_WIENER_PROB, 1);
    cost_wiener = RDCOST_DBL(x->rdmult, (bits >> 4), err);
    if (cost_wiener >= cost_norestore) {
      type[tile_idx] = RESTORE_NONE;
    } else {
      type[tile_idx] = RESTORE_WIENER;
      memcpy(&wiener_info[tile_idx], &rsi[plane].wiener_info[tile_idx],
             sizeof(wiener_info[tile_idx]));
      memcpy(&ref_wiener_info, &rsi[plane].wiener_info[tile_idx],
             sizeof(ref_wiener_info));
      best_tile_cost[tile_idx] = err;
    }
    rsi[plane].restoration_type[tile_idx] = RESTORE_NONE;
  }
  // Cost for Wiener filtering
  set_default_wiener(&ref_wiener_info);
  bits = frame_level_restore_bits[rsi[plane].frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    bits +=
        av1_cost_bit(RESTORE_NONE_WIENER_PROB, type[tile_idx] != RESTORE_NONE);
    memcpy(&rsi[plane].wiener_info[tile_idx], &wiener_info[tile_idx],
           sizeof(wiener_info[tile_idx]));
    if (type[tile_idx] == RESTORE_WIENER) {
      bits +=
          count_wiener_bits(&rsi[plane].wiener_info[tile_idx], &ref_wiener_info)
          << AV1_PROB_COST_SHIFT;
      memcpy(&ref_wiener_info, &rsi[plane].wiener_info[tile_idx],
             sizeof(ref_wiener_info));
    }
    rsi[plane].restoration_type[tile_idx] = type[tile_idx];
  }
  err = try_restoration_frame(src, cpi, rsi, 1 << plane, partial_frame,
                              dst_frame);
  cost_wiener = RDCOST_DBL(x->rdmult, (bits >> 4), err);

  return cost_wiener;
}

static double search_norestore(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                               int partial_frame, int plane,
                               RestorationInfo *info, RestorationType *type,
                               double *best_tile_cost,
                               YV12_BUFFER_CONFIG *dst_frame) {
  int64_t err;
  double cost_norestore;
  int bits;
  MACROBLOCK *x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  int tile_idx, tile_width, tile_height, nhtiles, nvtiles;
  int h_start, h_end, v_start, v_end;
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
    av1_get_rest_tile_limits(tile_idx, 0, 0, nhtiles, nvtiles, tile_width,
                             tile_height, width, height, 0, 0, &h_start, &h_end,
                             &v_start, &v_end);
    err = sse_restoration_tile(src, cm->frame_to_show, cm, h_start,
                               h_end - h_start, v_start, v_end - v_start,
                               1 << plane);
    type[tile_idx] = RESTORE_NONE;
    best_tile_cost[tile_idx] = err;
  }
  // RD cost associated with no restoration
  err = sse_restoration_frame(cm, src, cm->frame_to_show, (1 << plane));
  bits = frame_level_restore_bits[RESTORE_NONE] << AV1_PROB_COST_SHIFT;
  cost_norestore = RDCOST_DBL(x->rdmult, (bits >> 4), err);
  return cost_norestore;
}

static double search_switchable_restoration(
    const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi, int partial_frame, int plane,
    RestorationType *const restore_types[RESTORE_SWITCHABLE_TYPES],
    double *const tile_cost[RESTORE_SWITCHABLE_TYPES], RestorationInfo *rsi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *x = &cpi->td.mb;
  double cost_switchable = 0;
  int bits, tile_idx;
  RestorationType r;
  int width, height;
  if (plane == AOM_PLANE_Y) {
    width = src->y_crop_width;
    height = src->y_crop_height;
  } else {
    width = src->uv_crop_width;
    height = src->uv_crop_height;
  }
  const int ntiles = av1_get_rest_ntiles(
      width, height, cm->rst_info[plane].restoration_tilesize, NULL, NULL, NULL,
      NULL);
  SgrprojInfo ref_sgrproj_info;
  set_default_sgrproj(&ref_sgrproj_info);
  WienerInfo ref_wiener_info;
  set_default_wiener(&ref_wiener_info);
  (void)partial_frame;

  rsi->frame_restoration_type = RESTORE_SWITCHABLE;
  bits = frame_level_restore_bits[rsi->frame_restoration_type]
         << AV1_PROB_COST_SHIFT;
  cost_switchable = RDCOST_DBL(x->rdmult, bits >> 4, 0);
  for (tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
    double best_cost =
        RDCOST_DBL(x->rdmult, (cpi->switchable_restore_cost[RESTORE_NONE] >> 4),
                   tile_cost[RESTORE_NONE][tile_idx]);
    rsi->restoration_type[tile_idx] = RESTORE_NONE;
    for (r = 1; r < RESTORE_SWITCHABLE_TYPES; r++) {
      if (force_restore_type != 0)
        if (r != force_restore_type) continue;
      int tilebits = 0;
      if (restore_types[r][tile_idx] != r) continue;
      if (r == RESTORE_WIENER)
        tilebits +=
            count_wiener_bits(&rsi->wiener_info[tile_idx], &ref_wiener_info);
      else if (r == RESTORE_SGRPROJ)
        tilebits +=
            count_sgrproj_bits(&rsi->sgrproj_info[tile_idx], &ref_sgrproj_info);
      tilebits <<= AV1_PROB_COST_SHIFT;
      tilebits += cpi->switchable_restore_cost[r];
      double cost =
          RDCOST_DBL(x->rdmult, tilebits >> 4, tile_cost[r][tile_idx]);

      if (cost < best_cost) {
        rsi->restoration_type[tile_idx] = r;
        best_cost = cost;
      }
    }
    if (rsi->restoration_type[tile_idx] == RESTORE_WIENER)
      memcpy(&ref_wiener_info, &rsi->wiener_info[tile_idx],
             sizeof(ref_wiener_info));
    else if (rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ)
      memcpy(&ref_sgrproj_info, &rsi->sgrproj_info[tile_idx],
             sizeof(ref_sgrproj_info));
    if (force_restore_type != 0)
      assert(rsi->restoration_type[tile_idx] == force_restore_type ||
             rsi->restoration_type[tile_idx] == RESTORE_NONE);
    cost_switchable += best_cost;
  }
  return cost_switchable;
}

void av1_pick_filter_restoration(const YV12_BUFFER_CONFIG *src, AV1_COMP *cpi,
                                 LPF_PICK_METHOD method) {
  static search_restore_type search_restore_fun[RESTORE_SWITCHABLE_TYPES] = {
    search_norestore, search_wiener, search_sgrproj,
  };
  AV1_COMMON *const cm = &cpi->common;
  double cost_restore[RESTORE_TYPES];
  double *tile_cost[RESTORE_SWITCHABLE_TYPES];
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
    tile_cost[r] = (double *)aom_malloc(sizeof(*tile_cost[0]) * ntiles_y);
    restore_types[r] =
        (RestorationType *)aom_malloc(sizeof(*restore_types[0]) * ntiles_y);
  }

  for (int plane = AOM_PLANE_Y; plane <= AOM_PLANE_V; ++plane) {
    for (r = 0; r < RESTORE_SWITCHABLE_TYPES; ++r) {
      cost_restore[r] = DBL_MAX;
      if (force_restore_type != 0)
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
      if (force_restore_type != 0)
        if (r != RESTORE_NONE && r != force_restore_type) continue;
      if (cost_restore[r] < best_cost_restore) {
        best_restore = r;
        best_cost_restore = cost_restore[r];
      }
    }
    cm->rst_info[plane].frame_restoration_type = best_restore;
    if (force_restore_type != 0)
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
