/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vp9/encoder/vp9_vaq.h"

#include "vp9/common/vp9_seg_common.h"

#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/common/vp9_systemdependent.h"

#define ENERGY_MIN (-1)
#define ENERGY_MAX (1)
#define ENERGY_SPAN (ENERGY_MAX - ENERGY_MIN +  1)
#define ENERGY_IN_BOUNDS(energy)\
  assert((energy) >= ENERGY_MIN && (energy) <= ENERGY_MAX)

static double q_ratio[MAX_SEGMENTS] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static double rdmult_ratio[MAX_SEGMENTS] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static int segment_id[MAX_SEGMENTS] = { 5, 3, 1, 0, 2, 4, 6, 7 };

#define Q_RATIO(i) q_ratio[(i) - ENERGY_MIN]
#define RDMULT_RATIO(i) rdmult_ratio[(i) - ENERGY_MIN]
#define SEGMENT_ID(i) segment_id[(i) - ENERGY_MIN]

DECLARE_ALIGNED(16, static const uint8_t, vp9_64_zeros[64]) = {0};

unsigned int vp9_vaq_segment_id(int energy) {
  ENERGY_IN_BOUNDS(energy);

  return SEGMENT_ID(energy);
}

double vp9_vaq_rdmult_ratio(int energy) {
  ENERGY_IN_BOUNDS(energy);

  vp9_clear_system_state();  // __asm emms;

  return RDMULT_RATIO(energy);
}

double vp9_vaq_inv_q_ratio(int energy) {
  ENERGY_IN_BOUNDS(energy);

  vp9_clear_system_state();  // __asm emms;

  return Q_RATIO(-energy);
}

void vp9_vaq_init() {
  int i;
  double base_ratio;

  assert(ENERGY_SPAN <= MAX_SEGMENTS);

  vp9_clear_system_state();  // __asm emms;

  base_ratio = 1.5;

  for (i = ENERGY_MIN; i <= ENERGY_MAX; i++) {
    Q_RATIO(i) = pow(base_ratio, i/3.0);
  }
}

void vp9_vaq_frame_setup(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;
  int base_q = vp9_convert_qindex_to_q(cm->base_qindex);
  int base_rdmult = vp9_compute_rd_mult(cpi, cm->base_qindex +
                                        cm->y_dc_delta_q);
  int i;

  if (cm->frame_type == KEY_FRAME ||
      cpi->refresh_alt_ref_frame ||
      (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
    vp9_enable_segmentation((VP9_PTR)cpi);
    vp9_clearall_segfeatures(seg);

    seg->abs_delta = SEGMENT_DELTADATA;

    vp9_clear_system_state();  // __asm emms;

    for (i = ENERGY_MIN; i <= ENERGY_MAX; i++) {
      int qindex_delta, segment_rdmult;

      if (Q_RATIO(i) == 1) {
        // No need to enable SEG_LVL_ALT_Q for this segment
        RDMULT_RATIO(i) = 1;
        continue;
      }

      qindex_delta = vp9_compute_qdelta(cpi, base_q, base_q * Q_RATIO(i));
      vp9_set_segdata(seg, SEGMENT_ID(i), SEG_LVL_ALT_Q, qindex_delta);
      vp9_enable_segfeature(seg, SEGMENT_ID(i), SEG_LVL_ALT_Q);

      segment_rdmult = vp9_compute_rd_mult(cpi, cm->base_qindex + qindex_delta +
                                           cm->y_dc_delta_q);

      RDMULT_RATIO(i) = (double) segment_rdmult / base_rdmult;
    }
  }
}


static unsigned int block_variance(VP9_COMP *cpi, MACROBLOCK *x,
                                   BLOCK_SIZE bs) {
  MACROBLOCKD *xd = &x->e_mbd;
  unsigned int var, sse;
  int right_overflow = (xd->mb_to_right_edge < 0) ?
      ((-xd->mb_to_right_edge) >> 3) : 0;
  int bottom_overflow = (xd->mb_to_bottom_edge < 0) ?
      ((-xd->mb_to_bottom_edge) >> 3) : 0;

  if (right_overflow || bottom_overflow) {
    const int bw = 8 * num_8x8_blocks_wide_lookup[bs] - right_overflow;
    const int bh = 8 * num_8x8_blocks_high_lookup[bs] - bottom_overflow;
    int avg;
    variance(x->plane[0].src.buf, x->plane[0].src.stride,
             vp9_64_zeros, 0, bw, bh, &sse, &avg);
    var = sse - (((int64_t)avg * avg) / (bw * bh));
    return (256 * var) / (bw * bh);
  } else {
    var = cpi->fn_ptr[bs].vf(x->plane[0].src.buf,
                             x->plane[0].src.stride,
                             vp9_64_zeros, 0, &sse);
    return (256 * var) >> num_pels_log2_lookup[bs];
  }
}

int vp9_block_energy(VP9_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs) {
  double energy;
  unsigned int var = block_variance(cpi, x, bs);

  vp9_clear_system_state();  // __asm emms;

  // if (var <= 1000)
  //   return 0;

  energy = 0.9*(logf(var + 1) - 10.0);
  return clamp(round(energy), ENERGY_MIN, ENERGY_MAX);
}
