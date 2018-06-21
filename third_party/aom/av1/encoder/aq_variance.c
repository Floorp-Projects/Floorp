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

#include "aom_ports/mem.h"

#include "av1/encoder/aq_variance.h"

#include "av1/common/seg_common.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/dwt.h"
#include "aom_ports/system_state.h"

#define ENERGY_MIN (-4)
#define ENERGY_MAX (1)
#define ENERGY_SPAN (ENERGY_MAX - ENERGY_MIN + 1)
#define ENERGY_IN_BOUNDS(energy) \
  assert((energy) >= ENERGY_MIN && (energy) <= ENERGY_MAX)

static const double rate_ratio[MAX_SEGMENTS] = { 2.5,  2.0, 1.5, 1.0,
                                                 0.75, 1.0, 1.0, 1.0 };
static const int segment_id[ENERGY_SPAN] = { 0, 1, 1, 2, 3, 4 };

#define SEGMENT_ID(i) segment_id[(i)-ENERGY_MIN]

DECLARE_ALIGNED(16, static const uint8_t, av1_all_zeros[MAX_SB_SIZE]) = { 0 };
DECLARE_ALIGNED(16, static const uint16_t,
                av1_highbd_all_zeros[MAX_SB_SIZE]) = { 0 };

unsigned int av1_vaq_segment_id(int energy) {
  ENERGY_IN_BOUNDS(energy);
  return SEGMENT_ID(energy);
}

void av1_vaq_frame_setup(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;
  int i;

  int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);
  if (resolution_change) {
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    av1_clearall_segfeatures(seg);
    aom_clear_system_state();
    av1_disable_segmentation(seg);
    return;
  }
  if (frame_is_intra_only(cm) || cm->error_resilient_mode ||
      cpi->refresh_alt_ref_frame ||
      (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
    cpi->vaq_refresh = 1;

    av1_enable_segmentation(seg);
    av1_clearall_segfeatures(seg);

    aom_clear_system_state();

    for (i = 0; i < MAX_SEGMENTS; ++i) {
      int qindex_delta =
          av1_compute_qdelta_by_rate(&cpi->rc, cm->frame_type, cm->base_qindex,
                                     rate_ratio[i], cm->bit_depth);

      // We don't allow qindex 0 in a segment if the base value is not 0.
      // Q index 0 (lossless) implies 4x4 encoding only and in AQ mode a segment
      // Q delta is sometimes applied without going back around the rd loop.
      // This could lead to an illegal combination of partition size and q.
      if ((cm->base_qindex != 0) && ((cm->base_qindex + qindex_delta) == 0)) {
        qindex_delta = -cm->base_qindex + 1;
      }

      av1_set_segdata(seg, i, SEG_LVL_ALT_Q, qindex_delta);
      av1_enable_segfeature(seg, i, SEG_LVL_ALT_Q);
    }
  }
}

/* TODO(agrange, paulwilkins): The block_variance calls the unoptimized versions
 * of variance() and highbd_8_variance(). It should not.
 */
static void aq_variance(const uint8_t *a, int a_stride, const uint8_t *b,
                        int b_stride, int w, int h, unsigned int *sse,
                        int *sum) {
  int i, j;

  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
}

static void aq_highbd_variance64(const uint8_t *a8, int a_stride,
                                 const uint8_t *b8, int b_stride, int w, int h,
                                 uint64_t *sse, uint64_t *sum) {
  int i, j;

  uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  *sum = 0;
  *sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      *sum += diff;
      *sse += diff * diff;
    }
    a += a_stride;
    b += b_stride;
  }
}

static void aq_highbd_8_variance(const uint8_t *a8, int a_stride,
                                 const uint8_t *b8, int b_stride, int w, int h,
                                 unsigned int *sse, int *sum) {
  uint64_t sse_long = 0;
  uint64_t sum_long = 0;
  aq_highbd_variance64(a8, a_stride, b8, b_stride, w, h, &sse_long, &sum_long);
  *sse = (unsigned int)sse_long;
  *sum = (int)sum_long;
}

static unsigned int block_variance(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   BLOCK_SIZE bs) {
  MACROBLOCKD *xd = &x->e_mbd;
  unsigned int var, sse;
  int right_overflow =
      (xd->mb_to_right_edge < 0) ? ((-xd->mb_to_right_edge) >> 3) : 0;
  int bottom_overflow =
      (xd->mb_to_bottom_edge < 0) ? ((-xd->mb_to_bottom_edge) >> 3) : 0;

  if (right_overflow || bottom_overflow) {
    const int bw = MI_SIZE * mi_size_wide[bs] - right_overflow;
    const int bh = MI_SIZE * mi_size_high[bs] - bottom_overflow;
    int avg;
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      aq_highbd_8_variance(x->plane[0].src.buf, x->plane[0].src.stride,
                           CONVERT_TO_BYTEPTR(av1_highbd_all_zeros), 0, bw, bh,
                           &sse, &avg);
      sse >>= 2 * (xd->bd - 8);
      avg >>= (xd->bd - 8);
    } else {
      aq_variance(x->plane[0].src.buf, x->plane[0].src.stride, av1_all_zeros, 0,
                  bw, bh, &sse, &avg);
    }
    var = sse - (unsigned int)(((int64_t)avg * avg) / (bw * bh));
    return (unsigned int)((uint64_t)var * 256) / (bw * bh);
  } else {
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      var =
          cpi->fn_ptr[bs].vf(x->plane[0].src.buf, x->plane[0].src.stride,
                             CONVERT_TO_BYTEPTR(av1_highbd_all_zeros), 0, &sse);
    } else {
      var = cpi->fn_ptr[bs].vf(x->plane[0].src.buf, x->plane[0].src.stride,
                               av1_all_zeros, 0, &sse);
    }
    return (unsigned int)((uint64_t)var * 256) >> num_pels_log2_lookup[bs];
  }
}

double av1_log_block_var(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs) {
  unsigned int var = block_variance(cpi, x, bs);
  aom_clear_system_state();
  return log(var + 1.0);
}

#define DEFAULT_E_MIDPOINT 10.0
int av1_block_energy(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs) {
  double energy;
  double energy_midpoint;
  aom_clear_system_state();
  energy_midpoint =
      (cpi->oxcf.pass == 2) ? cpi->twopass.mb_av_energy : DEFAULT_E_MIDPOINT;
  energy = av1_log_block_var(cpi, x, bs) - energy_midpoint;
  return clamp((int)round(energy), ENERGY_MIN, ENERGY_MAX);
}

unsigned int haar_ac_energy(MACROBLOCK *x, BLOCK_SIZE bs) {
  MACROBLOCKD *xd = &x->e_mbd;
  int stride = x->plane[0].src.stride;
  uint8_t *buf = x->plane[0].src.buf;
  const int bw = MI_SIZE * mi_size_wide[bs];
  const int bh = MI_SIZE * mi_size_high[bs];
  int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;

  int var = 0;
  for (int r = 0; r < bh; r += 8)
    for (int c = 0; c < bw; c += 8) {
      var += av1_haar_ac_sad_8x8_uint8_input(buf + c + r * stride, stride, hbd);
    }

  return (unsigned int)((uint64_t)var * 256) >> num_pels_log2_lookup[bs];
}

double av1_log_block_wavelet_energy(MACROBLOCK *x, BLOCK_SIZE bs) {
  unsigned int haar_sad = haar_ac_energy(x, bs);
  aom_clear_system_state();
  return log(haar_sad + 1.0);
}

int av1_block_wavelet_energy_level(const AV1_COMP *cpi, MACROBLOCK *x,
                                   BLOCK_SIZE bs) {
  double energy, energy_midpoint;
  aom_clear_system_state();
  energy_midpoint = (cpi->oxcf.pass == 2) ? cpi->twopass.frame_avg_haar_energy
                                          : DEFAULT_E_MIDPOINT;
  energy = av1_log_block_wavelet_energy(x, bs) - energy_midpoint;
  return clamp((int)round(energy), ENERGY_MIN, ENERGY_MAX);
}

int av1_compute_deltaq_from_energy_level(const AV1_COMP *const cpi,
                                         int block_var_level) {
  ENERGY_IN_BOUNDS(block_var_level);

  const int rate_level = SEGMENT_ID(block_var_level);
  const AV1_COMMON *const cm = &cpi->common;
  int qindex_delta =
      av1_compute_qdelta_by_rate(&cpi->rc, cm->frame_type, cm->base_qindex,
                                 rate_ratio[rate_level], cm->bit_depth);

  if ((cm->base_qindex != 0) && ((cm->base_qindex + qindex_delta) == 0)) {
    qindex_delta = -cm->base_qindex + 1;
  }

  return qindex_delta;
}
