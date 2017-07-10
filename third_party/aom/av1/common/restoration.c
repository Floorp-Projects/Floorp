/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 */

#include <math.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/restoration.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

const sgr_params_type sgr_params[SGRPROJ_PARAMS] = {
#if USE_HIGHPASS_IN_SGRPROJ
  // corner, edge, r2, eps2
  { -1, 2, 1, 1 }, { -1, 2, 1, 2 }, { -1, 2, 1, 3 }, { -1, 2, 1, 4 },
  { -1, 2, 1, 5 }, { -2, 3, 1, 2 }, { -2, 3, 1, 3 }, { -2, 3, 1, 4 },
  { -2, 3, 1, 5 }, { -2, 3, 1, 6 }, { -3, 4, 1, 3 }, { -3, 4, 1, 4 },
  { -3, 4, 1, 5 }, { -3, 4, 1, 6 }, { -3, 4, 1, 7 }, { -3, 4, 1, 8 }
#else
  // r1, eps1, r2, eps2
  { 2, 12, 1, 4 },  { 2, 15, 1, 6 },  { 2, 18, 1, 8 },  { 2, 20, 1, 9 },
  { 2, 22, 1, 10 }, { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 },
  { 2, 55, 1, 14 }, { 2, 65, 1, 15 }, { 2, 75, 1, 16 }, { 3, 30, 1, 10 },
  { 3, 50, 1, 12 }, { 3, 50, 2, 25 }, { 3, 60, 2, 35 }, { 3, 70, 2, 45 },
#endif
};

typedef void (*restore_func_type)(uint8_t *data8, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint8_t *dst8, int dst_stride);
#if CONFIG_HIGHBITDEPTH
typedef void (*restore_func_highbd_type)(uint8_t *data8, int width, int height,
                                         int stride, RestorationInternal *rst,
                                         int bit_depth, uint8_t *dst8,
                                         int dst_stride);
#endif  // CONFIG_HIGHBITDEPTH

int av1_alloc_restoration_struct(AV1_COMMON *cm, RestorationInfo *rst_info,
                                 int width, int height) {
  const int ntiles = av1_get_rest_ntiles(
      width, height, rst_info->restoration_tilesize, NULL, NULL, NULL, NULL);
  aom_free(rst_info->restoration_type);
  CHECK_MEM_ERROR(cm, rst_info->restoration_type,
                  (RestorationType *)aom_malloc(
                      sizeof(*rst_info->restoration_type) * ntiles));
  aom_free(rst_info->wiener_info);
  CHECK_MEM_ERROR(
      cm, rst_info->wiener_info,
      (WienerInfo *)aom_memalign(16, sizeof(*rst_info->wiener_info) * ntiles));
  memset(rst_info->wiener_info, 0, sizeof(*rst_info->wiener_info) * ntiles);
  aom_free(rst_info->sgrproj_info);
  CHECK_MEM_ERROR(
      cm, rst_info->sgrproj_info,
      (SgrprojInfo *)aom_malloc(sizeof(*rst_info->sgrproj_info) * ntiles));
  return ntiles;
}

void av1_free_restoration_struct(RestorationInfo *rst_info) {
  aom_free(rst_info->restoration_type);
  rst_info->restoration_type = NULL;
  aom_free(rst_info->wiener_info);
  rst_info->wiener_info = NULL;
  aom_free(rst_info->sgrproj_info);
  rst_info->sgrproj_info = NULL;
}

#define MAX_RADIUS 3  // Only 1, 2, 3 allowed
#define MAX_EPS 80    // Max value of eps
#define MAX_NELEM ((2 * MAX_RADIUS + 1) * (2 * MAX_RADIUS + 1))
#define SGRPROJ_MTABLE_BITS 20
#define SGRPROJ_RECIP_BITS 12

// TODO(debargha): This table can be substantially reduced since only a few
// values are actually used.
int sgrproj_mtable[MAX_EPS][MAX_NELEM];

static void GenSgrprojVtable() {
  int e, n;
  for (e = 1; e <= MAX_EPS; ++e)
    for (n = 1; n <= MAX_NELEM; ++n) {
      const int n2e = n * n * e;
      sgrproj_mtable[e - 1][n - 1] =
          (((1 << SGRPROJ_MTABLE_BITS) + n2e / 2) / n2e);
    }
}

void av1_loop_restoration_precal() { GenSgrprojVtable(); }

static void loop_restoration_init(RestorationInternal *rst, int kf) {
  rst->keyframe = kf;
}

void extend_frame(uint8_t *data, int width, int height, int stride) {
  uint8_t *data_p;
  int i;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    memset(data_p - WIENER_HALFWIN, data_p[0], WIENER_HALFWIN);
    memset(data_p + width, data_p[width - 1], WIENER_HALFWIN);
  }
  data_p = data - WIENER_HALFWIN;
  for (i = -WIENER_HALFWIN; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p, width + 2 * WIENER_HALFWIN);
  }
  for (i = height; i < height + WIENER_HALFWIN; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           width + 2 * WIENER_HALFWIN);
  }
}

static void loop_copy_tile(uint8_t *data, int tile_idx, int subtile_idx,
                           int subtile_bits, int width, int height, int stride,
                           RestorationInternal *rst, uint8_t *dst,
                           int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           h_end - h_start);
}

static void loop_wiener_filter_tile(uint8_t *data, int tile_idx, int width,
                                    int height, int stride,
                                    RestorationInternal *rst, uint8_t *dst,
                                    int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i, j;
  int h_start, h_end, v_start, v_end;
  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  // Convolve the whole tile (done in blocks here to match the requirements
  // of the vectorized convolve functions, but the result is equivalent)
  for (i = v_start; i < v_end; i += MAX_SB_SIZE)
    for (j = h_start; j < h_end; j += MAX_SB_SIZE) {
      int w = AOMMIN(MAX_SB_SIZE, (h_end - j + 15) & ~15);
      int h = AOMMIN(MAX_SB_SIZE, (v_end - i + 15) & ~15);
      const uint8_t *data_p = data + i * stride + j;
      uint8_t *dst_p = dst + i * dst_stride + j;
#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
      aom_convolve8_add_src_hip(data_p, stride, dst_p, dst_stride,
                                rst->rsi->wiener_info[tile_idx].hfilter, 16,
                                rst->rsi->wiener_info[tile_idx].vfilter, 16, w,
                                h);
#else
      aom_convolve8_add_src(data_p, stride, dst_p, dst_stride,
                            rst->rsi->wiener_info[tile_idx].hfilter, 16,
                            rst->rsi->wiener_info[tile_idx].vfilter, 16, w, h);
#endif  // USE_WIENER_HIGH_INTERMEDIATE_PRECISION
    }
}

static void loop_wiener_filter(uint8_t *data, int width, int height, int stride,
                               RestorationInternal *rst, uint8_t *dst,
                               int dst_stride) {
  int tile_idx;
  extend_frame(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                            dst_stride);
  }
}

/* Calculate windowed sums (if sqr=0) or sums of squares (if sqr=1)
   over the input. The window is of size (2r + 1)x(2r + 1), and we
   specialize to r = 1, 2, 3. A default function is used for r > 3.

   Each loop follows the same format: We keep a window's worth of input
   in individual variables and select data out of that as appropriate.
*/
static void boxsum1(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c;

  // Vertical sum over 3-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 1) * src_stride + j]
        // b = src[(i    ) * src_stride + j]
        // c = src[(i + 1) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j] * src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  }

  // Horizontal sum over 3-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];

    dst[i * dst_stride] = a + b;
    for (j = 1; j < width - 2; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 1)]
      // b = src[i * src_stride + (j    )]
      // c = src[i * src_stride + (j + 1)]
      dst[i * dst_stride + j] = a + b + c;
      a = b;
      b = c;
      c = dst[i * dst_stride + (j + 2)];
    }
    dst[i * dst_stride + j] = a + b + c;
    dst[i * dst_stride + (j + 1)] = b + c;
  }
}

static void boxsum2(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e;

  // Vertical sum over 5-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 2) * src_stride + j]
        // b = src[(i - 1) * src_stride + j]
        // c = src[(i    ) * src_stride + j]
        // d = src[(i + 1) * src_stride + j]
        // e = src[(i + 2) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j] * src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  }

  // Horizontal sum over 5-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];

    dst[i * dst_stride] = a + b + c;
    dst[i * dst_stride + 1] = a + b + c + d;
    for (j = 2; j < width - 3; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 2)]
      // b = src[i * src_stride + (j - 1)]
      // c = src[i * src_stride + (j    )]
      // d = src[i * src_stride + (j + 1)]
      // e = src[i * src_stride + (j + 2)]
      dst[i * dst_stride + j] = a + b + c + d + e;
      a = b;
      b = c;
      c = d;
      d = e;
      e = dst[i * dst_stride + (j + 3)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e;
    dst[i * dst_stride + (j + 1)] = b + c + d + e;
    dst[i * dst_stride + (j + 2)] = c + d + e;
  }
}

static void boxsum3(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e, f, g;

  // Vertical sum over 7-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[1 * src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];
      f = src[5 * src_stride + j];
      g = src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[1 * src_stride + j] * src[1 * src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];
      f = src[5 * src_stride + j] * src[5 * src_stride + j];
      g = src[6 * src_stride + j] * src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j] * src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  }

  // Horizontal sum over 7-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];
    f = dst[i * dst_stride + 5];
    g = dst[i * dst_stride + 6];

    dst[i * dst_stride] = a + b + c + d;
    dst[i * dst_stride + 1] = a + b + c + d + e;
    dst[i * dst_stride + 2] = a + b + c + d + e + f;
    for (j = 3; j < width - 4; ++j) {
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      a = b;
      b = c;
      c = d;
      d = e;
      e = f;
      f = g;
      g = dst[i * dst_stride + (j + 4)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e + f + g;
    dst[i * dst_stride + (j + 1)] = b + c + d + e + f + g;
    dst[i * dst_stride + (j + 2)] = c + d + e + f + g;
    dst[i * dst_stride + (j + 3)] = d + e + f + g;
  }
}

// Generic version for any r. To be removed after experiments are done.
static void boxsumr(int32_t *src, int width, int height, int src_stride, int r,
                    int sqr, int32_t *dst, int dst_stride) {
  int32_t *tmp = aom_malloc(width * height * sizeof(*tmp));
  int tmp_stride = width;
  int i, j;
  if (sqr) {
    for (j = 0; j < width; ++j) tmp[j] = src[j] * src[j];
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] +
            src[i * src_stride + j] * src[i * src_stride + j];
  } else {
    memcpy(tmp, src, sizeof(*tmp) * width);
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] + src[i * src_stride + j];
  }
  for (i = 0; i <= r; ++i)
    memcpy(&dst[i * dst_stride], &tmp[(i + r) * tmp_stride],
           sizeof(*tmp) * width);
  for (i = r + 1; i < height - r; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] =
          tmp[(i + r) * tmp_stride + j] - tmp[(i - r - 1) * tmp_stride + j];
  for (i = height - r; i < height; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] = tmp[(height - 1) * tmp_stride + j] -
                                tmp[(i - r - 1) * tmp_stride + j];

  for (i = 0; i < height; ++i) tmp[i * tmp_stride] = dst[i * dst_stride];
  for (i = 0; i < height; ++i)
    for (j = 1; j < width; ++j)
      tmp[i * tmp_stride + j] =
          tmp[i * tmp_stride + j - 1] + dst[i * src_stride + j];

  for (j = 0; j <= r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] = tmp[i * tmp_stride + j + r];
  for (j = r + 1; j < width - r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + j + r] - tmp[i * tmp_stride + j - r - 1];
  for (j = width - r; j < width; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + width - 1] - tmp[i * tmp_stride + j - r - 1];
  aom_free(tmp);
}

static void boxsum(int32_t *src, int width, int height, int src_stride, int r,
                   int sqr, int32_t *dst, int dst_stride) {
  if (r == 1)
    boxsum1(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 2)
    boxsum2(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 3)
    boxsum3(src, width, height, src_stride, sqr, dst, dst_stride);
  else
    boxsumr(src, width, height, src_stride, r, sqr, dst, dst_stride);
}

static void boxnum(int width, int height, int r, int8_t *num, int num_stride) {
  int i, j;
  for (i = 0; i <= r; ++i) {
    for (j = 0; j <= r; ++j) {
      num[i * num_stride + j] = (r + 1 + i) * (r + 1 + j);
      num[i * num_stride + (width - 1 - j)] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + j] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + (width - 1 - j)] =
          num[i * num_stride + j];
    }
  }
  for (j = 0; j <= r; ++j) {
    const int val = (2 * r + 1) * (r + 1 + j);
    for (i = r + 1; i < height - r; ++i) {
      num[i * num_stride + j] = val;
      num[i * num_stride + (width - 1 - j)] = val;
    }
  }
  for (i = 0; i <= r; ++i) {
    const int val = (2 * r + 1) * (r + 1 + i);
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = val;
      num[(height - 1 - i) * num_stride + j] = val;
    }
  }
  for (i = r + 1; i < height - r; ++i) {
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = (2 * r + 1) * (2 * r + 1);
    }
  }
}

void decode_xq(int *xqd, int *xq) {
  xq[0] = xqd[0];
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
}

const int32_t x_by_xplus1[256] = {
  0,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
  240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
  248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
  250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
  252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  256,
};

const int32_t one_by_x[MAX_NELEM] = {
  4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
  293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164, 158,
  152,  146,  141,  137,  132, 128, 124, 120, 117, 114, 111, 108, 105,
  102,  100,  98,   95,   93,  91,  89,  87,  85,  84
};

static void av1_selfguided_restoration_internal(int32_t *dgd, int width,
                                                int height, int stride,
                                                int bit_depth, int r, int eps,
                                                int32_t *tmpbuf) {
  int32_t *A = tmpbuf;
  int32_t *B = A + SGRPROJ_OUTBUF_SIZE;
  int8_t num[RESTORATION_TILEPELS_MAX];
  int i, j;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width + 3) & ~3) + 16;

  // Don't filter tiles with dimensions < 5 on any axis
  if ((width < 5) || (height < 5)) return;

  boxsum(dgd, width, height, stride, r, 0, B, buf_stride);
  boxsum(dgd, width, height, stride, r, 1, A, buf_stride);
  boxnum(width, height, r, num, width);
  assert(r <= 3);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * buf_stride + j;
      const int n = num[i * width + j];

      // a < 2^16 * n < 2^22 regardless of bit depth
      uint32_t a = ROUND_POWER_OF_TWO(A[k], 2 * (bit_depth - 8));
      // b < 2^8 * n < 2^14 regardless of bit depth
      uint32_t b = ROUND_POWER_OF_TWO(B[k], bit_depth - 8);

      // Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // Note: Sometimes, in high bit depth, we can end up with a*n < b*b.
      // This is an artefact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      uint32_t p = (a * n < b * b) ? 0 : a * n - b * b;
      uint32_t s = sgrproj_mtable[eps - 1][n - 1];

      // p * s < (2^14 * n^2) * round(2^20 / n^2 eps) < 2^34 / eps < 2^32
      // as long as eps >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = ROUND_POWER_OF_TWO(p * s, SGRPROJ_MTABLE_BITS);

      A[k] = x_by_xplus1[AOMMIN(z, 255)];  // < 2^8

      // SGRPROJ_SGR - A[k] < 2^8, B[k] < 2^(bit_depth) * n,
      // one_by_x[n - 1] = round(2^12 / n)
      // => the product here is < 2^(20 + bit_depth) <= 2^32,
      // and B[k] is set to a value < 2^(8 + bit depth)
      B[k] = (int32_t)ROUND_POWER_OF_TWO((uint32_t)(SGRPROJ_SGR - A[k]) *
                                             (uint32_t)B[k] *
                                             (uint32_t)one_by_x[n - 1],
                                         SGRPROJ_RECIP_BITS);
    }
  }
  i = 0;
  j = 0;
  {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k + buf_stride] + A[k + buf_stride + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k + buf_stride] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k + buf_stride] + A[k + buf_stride - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k + buf_stride] + B[k + buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k - buf_stride] + A[k - buf_stride + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k - buf_stride] + B[k - buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k - buf_stride] + A[k - buf_stride - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k - buf_stride] + B[k - buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + buf_stride] +
                      A[k + buf_stride - 1] + A[k + buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + buf_stride] +
                      B[k + buf_stride - 1] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - buf_stride] +
                      A[k - buf_stride - 1] + A[k - buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - buf_stride] +
                      B[k - buf_stride - 1] + B[k - buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                      A[k + 1] + A[k - buf_stride + 1] + A[k + buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                      B[k + 1] + B[k - buf_stride + 1] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * buf_stride + j;
    const int l = i * stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                      A[k - 1] + A[k - buf_stride - 1] + A[k + buf_stride - 1];
    const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                      B[k - 1] + B[k - buf_stride - 1] + B[k + buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dgd[l] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

void av1_selfguided_restoration_c(uint8_t *dgd, int width, int height,
                                  int stride, int32_t *dst, int dst_stride,
                                  int r, int eps, int32_t *tmpbuf) {
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dst[i * dst_stride + j] = dgd[i * stride + j];
    }
  }
  av1_selfguided_restoration_internal(dst, width, height, dst_stride, 8, r, eps,
                                      tmpbuf);
}

void av1_highpass_filter_c(uint8_t *dgd, int width, int height, int stride,
                           int32_t *dst, int dst_stride, int corner, int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  i = 0;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k + stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k - stride - 1] + dgd[k - stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
  }
}

void apply_selfguided_restoration_c(uint8_t *dat, int width, int height,
                                    int stride, int eps, int *xqd, uint8_t *dst,
                                    int dst_stride, int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int32_t *tmpbuf2 = flt2 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_c(dat, width, height, stride, flt1, width,
                        sgr_params[eps].corner, sgr_params[eps].edge);
#else
  av1_selfguided_restoration_c(dat, width, height, stride, flt1, width,
                               sgr_params[eps].r1, sgr_params[eps].e1, tmpbuf2);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_c(dat, width, height, stride, flt2, width,
                               sgr_params[eps].r2, sgr_params[eps].e2, tmpbuf2);
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = clip_pixel(w);
    }
  }
}

static void loop_sgrproj_filter_tile(uint8_t *data, int tile_idx, int width,
                                     int height, int stride,
                                     RestorationInternal *rst, uint8_t *dst,
                                     int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  uint8_t *data_p, *dst_p;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                   dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  dst_p = dst + h_start + v_start * dst_stride;
  apply_selfguided_restoration(data_p, h_end - h_start, v_end - v_start, stride,
                               rst->rsi->sgrproj_info[tile_idx].ep,
                               rst->rsi->sgrproj_info[tile_idx].xqd, dst_p,
                               dst_stride, rst->tmpbuf);
}

static void loop_sgrproj_filter(uint8_t *data, int width, int height,
                                int stride, RestorationInternal *rst,
                                uint8_t *dst, int dst_stride) {
  int tile_idx;
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                             dst_stride);
  }
}

static void loop_switchable_filter(uint8_t *data, int width, int height,
                                   int stride, RestorationInternal *rst,
                                   uint8_t *dst, int dst_stride) {
  int tile_idx;
  extend_frame(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                     dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                              dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile(data, tile_idx, width, height, stride, rst, dst,
                               dst_stride);
    }
  }
}

#if CONFIG_HIGHBITDEPTH
void extend_frame_highbd(uint16_t *data, int width, int height, int stride) {
  uint16_t *data_p;
  int i, j;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    for (j = -WIENER_HALFWIN; j < 0; ++j) data_p[j] = data_p[0];
    for (j = width; j < width + WIENER_HALFWIN; ++j)
      data_p[j] = data_p[width - 1];
  }
  data_p = data - WIENER_HALFWIN;
  for (i = -WIENER_HALFWIN; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p,
           (width + 2 * WIENER_HALFWIN) * sizeof(uint16_t));
  }
  for (i = height; i < height + WIENER_HALFWIN; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           (width + 2 * WIENER_HALFWIN) * sizeof(uint16_t));
  }
}

static void loop_copy_tile_highbd(uint16_t *data, int tile_idx, int subtile_idx,
                                  int subtile_bits, int width, int height,
                                  int stride, RestorationInternal *rst,
                                  uint16_t *dst, int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int i;
  int h_start, h_end, v_start, v_end;
  av1_get_rest_tile_limits(tile_idx, subtile_idx, subtile_bits, rst->nhtiles,
                           rst->nvtiles, tile_width, tile_height, width, height,
                           0, 0, &h_start, &h_end, &v_start, &v_end);
  for (i = v_start; i < v_end; ++i)
    memcpy(dst + i * dst_stride + h_start, data + i * stride + h_start,
           (h_end - h_start) * sizeof(*dst));
}

static void loop_wiener_filter_tile_highbd(uint16_t *data, int tile_idx,
                                           int width, int height, int stride,
                                           RestorationInternal *rst,
                                           int bit_depth, uint16_t *dst,
                                           int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  int i, j;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  // Convolve the whole tile (done in blocks here to match the requirements
  // of the vectorized convolve functions, but the result is equivalent)
  for (i = v_start; i < v_end; i += MAX_SB_SIZE)
    for (j = h_start; j < h_end; j += MAX_SB_SIZE) {
      int w = AOMMIN(MAX_SB_SIZE, (h_end - j + 15) & ~15);
      int h = AOMMIN(MAX_SB_SIZE, (v_end - i + 15) & ~15);
      const uint16_t *data_p = data + i * stride + j;
      uint16_t *dst_p = dst + i * dst_stride + j;
#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
      aom_highbd_convolve8_add_src_hip(
          CONVERT_TO_BYTEPTR(data_p), stride, CONVERT_TO_BYTEPTR(dst_p),
          dst_stride, rst->rsi->wiener_info[tile_idx].hfilter, 16,
          rst->rsi->wiener_info[tile_idx].vfilter, 16, w, h, bit_depth);
#else
      aom_highbd_convolve8_add_src(
          CONVERT_TO_BYTEPTR(data_p), stride, CONVERT_TO_BYTEPTR(dst_p),
          dst_stride, rst->rsi->wiener_info[tile_idx].hfilter, 16,
          rst->rsi->wiener_info[tile_idx].vfilter, 16, w, h, bit_depth);
#endif  // USE_WIENER_HIGH_INTERMEDIATE_PRECISION
    }
}

static void loop_wiener_filter_highbd(uint8_t *data8, int width, int height,
                                      int stride, RestorationInternal *rst,
                                      int bit_depth, uint8_t *dst8,
                                      int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int tile_idx;
  extend_frame_highbd(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                   bit_depth, dst, dst_stride);
  }
}

void av1_selfguided_restoration_highbd_c(uint16_t *dgd, int width, int height,
                                         int stride, int32_t *dst,
                                         int dst_stride, int bit_depth, int r,
                                         int eps, int32_t *tmpbuf) {
  int i, j;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      dst[i * dst_stride + j] = dgd[i * stride + j];
    }
  }
  av1_selfguided_restoration_internal(dst, width, height, dst_stride, bit_depth,
                                      r, eps, tmpbuf);
}

void av1_highpass_filter_highbd_c(uint16_t *dgd, int width, int height,
                                  int stride, int32_t *dst, int dst_stride,
                                  int corner, int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  i = 0;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k + stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k - stride - 1] + dgd[k - stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
  }
}

void apply_selfguided_restoration_highbd_c(uint16_t *dat, int width, int height,
                                           int stride, int bit_depth, int eps,
                                           int *xqd, uint16_t *dst,
                                           int dst_stride, int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int32_t *tmpbuf2 = flt2 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_highbd_c(dat, width, height, stride, flt1, width,
                               sgr_params[eps].corner, sgr_params[eps].edge);
#else
  av1_selfguided_restoration_highbd_c(dat, width, height, stride, flt1, width,
                                      bit_depth, sgr_params[eps].r1,
                                      sgr_params[eps].e1, tmpbuf2);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_highbd_c(dat, width, height, stride, flt2, width,
                                      bit_depth, sgr_params[eps].r2,
                                      sgr_params[eps].e2, tmpbuf2);
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = (uint16_t)clip_pixel_highbd(w, bit_depth);
    }
  }
}

static void loop_sgrproj_filter_tile_highbd(uint16_t *data, int tile_idx,
                                            int width, int height, int stride,
                                            RestorationInternal *rst,
                                            int bit_depth, uint16_t *dst,
                                            int dst_stride) {
  const int tile_width = rst->tile_width;
  const int tile_height = rst->tile_height;
  int h_start, h_end, v_start, v_end;
  uint16_t *data_p, *dst_p;

  if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
    loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst, dst,
                          dst_stride);
    return;
  }
  av1_get_rest_tile_limits(tile_idx, 0, 0, rst->nhtiles, rst->nvtiles,
                           tile_width, tile_height, width, height, 0, 0,
                           &h_start, &h_end, &v_start, &v_end);
  data_p = data + h_start + v_start * stride;
  dst_p = dst + h_start + v_start * dst_stride;
  apply_selfguided_restoration_highbd(
      data_p, h_end - h_start, v_end - v_start, stride, bit_depth,
      rst->rsi->sgrproj_info[tile_idx].ep, rst->rsi->sgrproj_info[tile_idx].xqd,
      dst_p, dst_stride, rst->tmpbuf);
}

static void loop_sgrproj_filter_highbd(uint8_t *data8, int width, int height,
                                       int stride, RestorationInternal *rst,
                                       int bit_depth, uint8_t *dst8,
                                       int dst_stride) {
  int tile_idx;
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                    bit_depth, dst, dst_stride);
  }
}

static void loop_switchable_filter_highbd(uint8_t *data8, int width, int height,
                                          int stride, RestorationInternal *rst,
                                          int bit_depth, uint8_t *dst8,
                                          int dst_stride) {
  uint16_t *data = CONVERT_TO_SHORTPTR(data8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  int tile_idx;
  extend_frame_highbd(data, width, height, stride);
  for (tile_idx = 0; tile_idx < rst->ntiles; ++tile_idx) {
    if (rst->rsi->restoration_type[tile_idx] == RESTORE_NONE) {
      loop_copy_tile_highbd(data, tile_idx, 0, 0, width, height, stride, rst,
                            dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_WIENER) {
      loop_wiener_filter_tile_highbd(data, tile_idx, width, height, stride, rst,
                                     bit_depth, dst, dst_stride);
    } else if (rst->rsi->restoration_type[tile_idx] == RESTORE_SGRPROJ) {
      loop_sgrproj_filter_tile_highbd(data, tile_idx, width, height, stride,
                                      rst, bit_depth, dst, dst_stride);
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void loop_restoration_rows(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                  int start_mi_row, int end_mi_row,
                                  int components_pattern, RestorationInfo *rsi,
                                  YV12_BUFFER_CONFIG *dst) {
  const int ywidth = frame->y_crop_width;
  const int yheight = frame->y_crop_height;
  const int uvwidth = frame->uv_crop_width;
  const int uvheight = frame->uv_crop_height;
  const int ystride = frame->y_stride;
  const int uvstride = frame->uv_stride;
  const int ystart = start_mi_row << MI_SIZE_LOG2;
  const int uvstart = ystart >> cm->subsampling_y;
  int yend = end_mi_row << MI_SIZE_LOG2;
  int uvend = yend >> cm->subsampling_y;
  restore_func_type restore_funcs[RESTORE_TYPES] = {
    NULL, loop_wiener_filter, loop_sgrproj_filter, loop_switchable_filter
  };
#if CONFIG_HIGHBITDEPTH
  restore_func_highbd_type restore_funcs_highbd[RESTORE_TYPES] = {
    NULL, loop_wiener_filter_highbd, loop_sgrproj_filter_highbd,
    loop_switchable_filter_highbd
  };
#endif  // CONFIG_HIGHBITDEPTH
  restore_func_type restore_func;
#if CONFIG_HIGHBITDEPTH
  restore_func_highbd_type restore_func_highbd;
#endif  // CONFIG_HIGHBITDEPTH
  YV12_BUFFER_CONFIG dst_;

  yend = AOMMIN(yend, yheight);
  uvend = AOMMIN(uvend, uvheight);

  if (components_pattern == (1 << AOM_PLANE_Y)) {
    // Only y
    if (rsi[0].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_y(frame, dst);
      return;
    }
  } else if (components_pattern == (1 << AOM_PLANE_U)) {
    // Only U
    if (rsi[1].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_u(frame, dst);
      return;
    }
  } else if (components_pattern == (1 << AOM_PLANE_V)) {
    // Only V
    if (rsi[2].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_v(frame, dst);
      return;
    }
  } else if (components_pattern ==
             ((1 << AOM_PLANE_Y) | (1 << AOM_PLANE_U) | (1 << AOM_PLANE_V))) {
    // All components
    if (rsi[0].frame_restoration_type == RESTORE_NONE &&
        rsi[1].frame_restoration_type == RESTORE_NONE &&
        rsi[2].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_frame(frame, dst);
      return;
    }
  }

  if (!dst) {
    dst = &dst_;
    memset(dst, 0, sizeof(YV12_BUFFER_CONFIG));
    if (aom_realloc_frame_buffer(
            dst, ywidth, yheight, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
            cm->use_highbitdepth,
#endif
            AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL) < 0)
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate restoration dst buffer");
  }

  if ((components_pattern >> AOM_PLANE_Y) & 1) {
    if (rsi[0].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          ywidth, yheight, cm->rst_info[AOM_PLANE_Y].restoration_tilesize,
          &cm->rst_internal.tile_width, &cm->rst_internal.tile_height,
          &cm->rst_internal.nhtiles, &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[0];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->y_buffer + ystart * ystride, ywidth, yend - ystart, ystride,
            &cm->rst_internal, cm->bit_depth,
            dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
      else
#endif  // CONFIG_HIGHBITDEPTH
        restore_func(frame->y_buffer + ystart * ystride, ywidth, yend - ystart,
                     ystride, &cm->rst_internal,
                     dst->y_buffer + ystart * dst->y_stride, dst->y_stride);
    } else {
      aom_yv12_copy_y(frame, dst);
    }
  }

  if ((components_pattern >> AOM_PLANE_U) & 1) {
    if (rsi[AOM_PLANE_U].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          uvwidth, uvheight, cm->rst_info[AOM_PLANE_U].restoration_tilesize,
          &cm->rst_internal.tile_width, &cm->rst_internal.tile_height,
          &cm->rst_internal.nhtiles, &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[AOM_PLANE_U];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->u_buffer + uvstart * uvstride, uvwidth, uvend - uvstart,
            uvstride, &cm->rst_internal, cm->bit_depth,
            dst->u_buffer + uvstart * dst->uv_stride, dst->uv_stride);
      else
#endif  // CONFIG_HIGHBITDEPTH
        restore_func(frame->u_buffer + uvstart * uvstride, uvwidth,
                     uvend - uvstart, uvstride, &cm->rst_internal,
                     dst->u_buffer + uvstart * dst->uv_stride, dst->uv_stride);
    } else {
      aom_yv12_copy_u(frame, dst);
    }
  }

  if ((components_pattern >> AOM_PLANE_V) & 1) {
    if (rsi[AOM_PLANE_V].frame_restoration_type != RESTORE_NONE) {
      cm->rst_internal.ntiles = av1_get_rest_ntiles(
          uvwidth, uvheight, cm->rst_info[AOM_PLANE_V].restoration_tilesize,
          &cm->rst_internal.tile_width, &cm->rst_internal.tile_height,
          &cm->rst_internal.nhtiles, &cm->rst_internal.nvtiles);
      cm->rst_internal.rsi = &rsi[AOM_PLANE_V];
      restore_func =
          restore_funcs[cm->rst_internal.rsi->frame_restoration_type];
#if CONFIG_HIGHBITDEPTH
      restore_func_highbd =
          restore_funcs_highbd[cm->rst_internal.rsi->frame_restoration_type];
      if (cm->use_highbitdepth)
        restore_func_highbd(
            frame->v_buffer + uvstart * uvstride, uvwidth, uvend - uvstart,
            uvstride, &cm->rst_internal, cm->bit_depth,
            dst->v_buffer + uvstart * dst->uv_stride, dst->uv_stride);
      else
#endif  // CONFIG_HIGHBITDEPTH
        restore_func(frame->v_buffer + uvstart * uvstride, uvwidth,
                     uvend - uvstart, uvstride, &cm->rst_internal,
                     dst->v_buffer + uvstart * dst->uv_stride, dst->uv_stride);
    } else {
      aom_yv12_copy_v(frame, dst);
    }
  }

  if (dst == &dst_) {
    if ((components_pattern >> AOM_PLANE_Y) & 1) aom_yv12_copy_y(dst, frame);
    if ((components_pattern >> AOM_PLANE_U) & 1) aom_yv12_copy_u(dst, frame);
    if ((components_pattern >> AOM_PLANE_V) & 1) aom_yv12_copy_v(dst, frame);
    aom_free_frame_buffer(dst);
  }
}

void av1_loop_restoration_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                                RestorationInfo *rsi, int components_pattern,
                                int partial_frame, YV12_BUFFER_CONFIG *dst) {
  int start_mi_row, end_mi_row, mi_rows_to_filter;
  start_mi_row = 0;
#if CONFIG_FRAME_SUPERRES
  mi_rows_to_filter =
      ALIGN_POWER_OF_TWO(cm->superres_upscaled_height, 3) >> MI_SIZE_LOG2;
#else
  mi_rows_to_filter = cm->mi_rows;
#endif  // CONFIG_FRAME_SUPERRES
  if (partial_frame && mi_rows_to_filter > 8) {
    start_mi_row = mi_rows_to_filter >> 1;
    start_mi_row &= 0xfffffff8;
    mi_rows_to_filter = AOMMAX(mi_rows_to_filter / 8, 8);
  }
  end_mi_row = start_mi_row + mi_rows_to_filter;
  loop_restoration_init(&cm->rst_internal, cm->frame_type == KEY_FRAME);
  loop_restoration_rows(frame, cm, start_mi_row, end_mi_row, components_pattern,
                        rsi, dst);
}
