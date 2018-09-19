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
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/dwt.h"
#include "aom_ports/system_state.h"

static const double rate_ratio[MAX_SEGMENTS] = { 2.2, 1.7, 1.3, 1.0,
                                                 0.9, .8,  .7,  .6 };

static const double deltaq_rate_ratio[MAX_SEGMENTS] = { 2.5,  2.0, 1.5, 1.0,
                                                        0.75, 1.0, 1.0, 1.0 };
#define ENERGY_MIN (-4)
#define ENERGY_MAX (1)
#define ENERGY_SPAN (ENERGY_MAX - ENERGY_MIN + 1)
#define ENERGY_IN_BOUNDS(energy) \
  assert((energy) >= ENERGY_MIN && (energy) <= ENERGY_MAX)

DECLARE_ALIGNED(16, static const uint8_t, av1_all_zeros[MAX_SB_SIZE]) = { 0 };

DECLARE_ALIGNED(16, static const uint16_t,
                av1_highbd_all_zeros[MAX_SB_SIZE]) = { 0 };

static const int segment_id[ENERGY_SPAN] = { 0, 1, 1, 2, 3, 4 };

#define SEGMENT_ID(i) segment_id[(i)-ENERGY_MIN]

void av1_vaq_frame_setup(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;
  int i;

  int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);
  int avg_energy = (int)(cpi->twopass.mb_av_energy - 2);
  double avg_ratio;
  if (avg_energy > 7) avg_energy = 7;
  if (avg_energy < 0) avg_energy = 0;
  avg_ratio = rate_ratio[avg_energy];

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
      // Set up avg segment id to be 1.0 and adjust the other segments around
      // it.
      int qindex_delta = av1_compute_qdelta_by_rate(
          &cpi->rc, cm->frame_type, cm->base_qindex, rate_ratio[i] / avg_ratio,
          cm->seq_params.bit_depth);

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

int av1_log_block_var(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs) {
  // This functions returns a score for the blocks local variance as calculated
  // by: sum of the log of the (4x4 variances) of each subblock to the current
  // block (x,bs)
  // * 32 / number of pixels in the block_size.
  // This is used for segmentation because to avoid situations in which a large
  // block with a gentle gradient gets marked high variance even though each
  // subblock has a low variance.   This allows us to assign the same segment
  // number for the same sorts of area regardless of how the partitioning goes.

  MACROBLOCKD *xd = &x->e_mbd;
  double var = 0;
  unsigned int sse;
  int i, j;

  int right_overflow =
      (xd->mb_to_right_edge < 0) ? ((-xd->mb_to_right_edge) >> 3) : 0;
  int bottom_overflow =
      (xd->mb_to_bottom_edge < 0) ? ((-xd->mb_to_bottom_edge) >> 3) : 0;

  const int bw = MI_SIZE * mi_size_wide[bs] - right_overflow;
  const int bh = MI_SIZE * mi_size_high[bs] - bottom_overflow;

  aom_clear_system_state();

  for (i = 0; i < bh; i += 4) {
    for (j = 0; j < bw; j += 4) {
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        var +=
            log(1.0 + cpi->fn_ptr[BLOCK_4X4].vf(
                          x->plane[0].src.buf + i * x->plane[0].src.stride + j,
                          x->plane[0].src.stride,
                          CONVERT_TO_BYTEPTR(av1_highbd_all_zeros), 0, &sse) /
                          16);
      } else {
        var +=
            log(1.0 + cpi->fn_ptr[BLOCK_4X4].vf(
                          x->plane[0].src.buf + i * x->plane[0].src.stride + j,
                          x->plane[0].src.stride, av1_all_zeros, 0, &sse) /
                          16);
      }
    }
  }
  // Use average of 4x4 log variance. The range for 8 bit 0 - 9.704121561.
  var /= (bw / 4 * bh / 4);
  if (var > 7) var = 7;

  aom_clear_system_state();
  return (int)(var);
}

#define DEFAULT_E_MIDPOINT 10.0

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
  int rate_level;
  const AV1_COMMON *const cm = &cpi->common;

  if (DELTAQ_MODULATION == 1) {
    ENERGY_IN_BOUNDS(block_var_level);
    rate_level = SEGMENT_ID(block_var_level);
  } else {
    rate_level = block_var_level;
  }
  int qindex_delta = av1_compute_qdelta_by_rate(
      &cpi->rc, cm->frame_type, cm->base_qindex, deltaq_rate_ratio[rate_level],
      cm->seq_params.bit_depth);

  if ((cm->base_qindex != 0) && ((cm->base_qindex + qindex_delta) == 0)) {
    qindex_delta = -cm->base_qindex + 1;
  }
  return qindex_delta;
}
