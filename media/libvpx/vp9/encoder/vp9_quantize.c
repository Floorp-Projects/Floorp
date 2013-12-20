/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include "vpx_mem/vpx_mem.h"

#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/common/vp9_quant_common.h"

#include "vp9/common/vp9_seg_common.h"

#ifdef ENC_DEBUG
extern int enc_debug;
#endif

void vp9_quantize_b_c(const int16_t *coeff_ptr, intptr_t count,
                      int skip_block,
                      const int16_t *zbin_ptr, const int16_t *round_ptr,
                      const int16_t *quant_ptr, const int16_t *quant_shift_ptr,
                      int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                      const int16_t *dequant_ptr,
                      int zbin_oq_value, uint16_t *eob_ptr,
                      const int16_t *scan, const int16_t *iscan) {
  int i, non_zero_count = count, eob = -1;
  const int zbins[2] = { zbin_ptr[0] + zbin_oq_value,
                         zbin_ptr[1] + zbin_oq_value };
  const int nzbins[2] = { zbins[0] * -1,
                          zbins[1] * -1 };

  vpx_memset(qcoeff_ptr, 0, count * sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, count * sizeof(int16_t));

  if (!skip_block) {
    // Pre-scan pass
    for (i = count - 1; i >= 0; i--) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];

      if (coeff < zbins[rc != 0] && coeff > nzbins[rc != 0])
        non_zero_count--;
      else
        break;
    }

    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < non_zero_count; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

      if (abs_coeff >= zbins[rc != 0]) {
        int tmp = clamp(abs_coeff + round_ptr[rc != 0], INT16_MIN, INT16_MAX);
        tmp = ((((tmp * quant_ptr[rc != 0]) >> 16) + tmp) *
                  quant_shift_ptr[rc != 0]) >> 16;  // quantization
        qcoeff_ptr[rc]  = (tmp ^ coeff_sign) - coeff_sign;
        dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];

        if (tmp)
          eob = i;
      }
    }
  }
  *eob_ptr = eob + 1;
}

void vp9_quantize_b_32x32_c(const int16_t *coeff_ptr, intptr_t n_coeffs,
                            int skip_block,
                            const int16_t *zbin_ptr, const int16_t *round_ptr,
                            const int16_t *quant_ptr,
                            const int16_t *quant_shift_ptr,
                            int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                            const int16_t *dequant_ptr,
                            int zbin_oq_value, uint16_t *eob_ptr,
                            const int16_t *scan, const int16_t *iscan) {
  int i, rc, eob;
  int zbins[2], nzbins[2];
  int x, y, z, sz;
  int idx = 0;
  int idx_arr[1024];

  vpx_memset(qcoeff_ptr, 0, n_coeffs*sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, n_coeffs*sizeof(int16_t));

  eob = -1;

  // Base ZBIN
  zbins[0] = ROUND_POWER_OF_TWO(zbin_ptr[0] + zbin_oq_value, 1);
  zbins[1] = ROUND_POWER_OF_TWO(zbin_ptr[1] + zbin_oq_value, 1);
  nzbins[0] = zbins[0] * -1;
  nzbins[1] = zbins[1] * -1;

  if (!skip_block) {
    // Pre-scan pass
    for (i = 0; i < n_coeffs; i++) {
      rc = scan[i];
      z = coeff_ptr[rc];

      // If the coefficient is out of the base ZBIN range, keep it for
      // quantization.
      if (z >= zbins[rc != 0] || z <= nzbins[rc != 0])
        idx_arr[idx++] = i;
    }

    // Quantization pass: only process the coefficients selected in
    // pre-scan pass. Note: idx can be zero.
    for (i = 0; i < idx; i++) {
      rc = scan[idx_arr[i]];

      z = coeff_ptr[rc];
      sz = (z >> 31);                               // sign of z
      x  = (z ^ sz) - sz;                           // x = abs(z)

      x += ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1);
      x  = clamp(x, INT16_MIN, INT16_MAX);
      y  = ((((x * quant_ptr[rc != 0]) >> 16) + x) *
            quant_shift_ptr[rc != 0]) >> 15;      // quantize (x)

      x  = (y ^ sz) - sz;                         // get the sign back
      qcoeff_ptr[rc]  = x;                        // write to destination
      dqcoeff_ptr[rc] = x * dequant_ptr[rc != 0] / 2;  // dequantized value

      if (y)
        eob = idx_arr[i];                         // last nonzero coeffs
    }
  }
  *eob_ptr = eob + 1;
}

struct plane_block_idx {
  int plane;
  int block;
};

// TODO(jkoleszar): returning a struct so it can be used in a const context,
// expect to refactor this further later.
static INLINE struct plane_block_idx plane_block_idx(int y_blocks,
                                                     int b_idx) {
  const int v_offset = y_blocks * 5 / 4;
  struct plane_block_idx res;

  if (b_idx < y_blocks) {
    res.plane = 0;
    res.block = b_idx;
  } else if (b_idx < v_offset) {
    res.plane = 1;
    res.block = b_idx - y_blocks;
  } else {
    assert(b_idx < y_blocks * 3 / 2);
    res.plane = 2;
    res.block = b_idx - v_offset;
  }
  return res;
}

void vp9_regular_quantize_b_4x4(MACROBLOCK *x, int y_blocks, int b_idx,
                                const int16_t *scan, const int16_t *iscan) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct plane_block_idx pb_idx = plane_block_idx(y_blocks, b_idx);
  struct macroblock_plane* p = &x->plane[pb_idx.plane];
  struct macroblockd_plane* pd = &xd->plane[pb_idx.plane];

  vp9_quantize_b(BLOCK_OFFSET(p->coeff, pb_idx.block),
           16, x->skip_block,
           p->zbin, p->round, p->quant, p->quant_shift,
           BLOCK_OFFSET(pd->qcoeff, pb_idx.block),
           BLOCK_OFFSET(pd->dqcoeff, pb_idx.block),
           pd->dequant, p->zbin_extra, &pd->eobs[pb_idx.block], scan, iscan);
}

static void invert_quant(int16_t *quant, int16_t *shift, int d) {
  unsigned t;
  int l;
  t = d;
  for (l = 0; t > 1; l++)
    t >>= 1;
  t = 1 + (1 << (16 + l)) / d;
  *quant = (int16_t)(t - (1 << 16));
  *shift = 1 << (16 - l);
}

void vp9_init_quantizer(VP9_COMP *cpi) {
  int i, q;
  VP9_COMMON *const cm = &cpi->common;

  for (q = 0; q < QINDEX_RANGE; q++) {
    const int qzbin_factor = q == 0 ? 64 : (vp9_dc_quant(q, 0) < 148 ? 84 : 80);
    const int qrounding_factor = q == 0 ? 64 : 48;

    // y
    for (i = 0; i < 2; ++i) {
      const int quant = i == 0 ? vp9_dc_quant(q, cm->y_dc_delta_q)
                               : vp9_ac_quant(q, 0);
      invert_quant(&cpi->y_quant[q][i], &cpi->y_quant_shift[q][i], quant);
      cpi->y_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      cpi->y_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->y_dequant[q][i] = quant;
    }

    // uv
    for (i = 0; i < 2; ++i) {
      const int quant = i == 0 ? vp9_dc_quant(q, cm->uv_dc_delta_q)
                               : vp9_ac_quant(q, cm->uv_ac_delta_q);
      invert_quant(&cpi->uv_quant[q][i], &cpi->uv_quant_shift[q][i], quant);
      cpi->uv_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      cpi->uv_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->uv_dequant[q][i] = quant;
    }

#if CONFIG_ALPHA
    // alpha
    for (i = 0; i < 2; ++i) {
      const int quant = i == 0 ? vp9_dc_quant(q, cm->a_dc_delta_q)
                               : vp9_ac_quant(q, cm->a_ac_delta_q);
      invert_quant(&cpi->a_quant[q][i], &cpi->a_quant_shift[q][i], quant);
      cpi->a_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      cpi->a_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->a_dequant[q][i] = quant;
    }
#endif

    for (i = 2; i < 8; i++) {
      cpi->y_quant[q][i] = cpi->y_quant[q][1];
      cpi->y_quant_shift[q][i] = cpi->y_quant_shift[q][1];
      cpi->y_zbin[q][i] = cpi->y_zbin[q][1];
      cpi->y_round[q][i] = cpi->y_round[q][1];
      cm->y_dequant[q][i] = cm->y_dequant[q][1];

      cpi->uv_quant[q][i] = cpi->uv_quant[q][1];
      cpi->uv_quant_shift[q][i] = cpi->uv_quant_shift[q][1];
      cpi->uv_zbin[q][i] = cpi->uv_zbin[q][1];
      cpi->uv_round[q][i] = cpi->uv_round[q][1];
      cm->uv_dequant[q][i] = cm->uv_dequant[q][1];

#if CONFIG_ALPHA
      cpi->a_quant[q][i] = cpi->a_quant[q][1];
      cpi->a_quant_shift[q][i] = cpi->a_quant_shift[q][1];
      cpi->a_zbin[q][i] = cpi->a_zbin[q][1];
      cpi->a_round[q][i] = cpi->a_round[q][1];
      cm->a_dequant[q][i] = cm->a_dequant[q][1];
#endif
    }
  }
}

void vp9_mb_init_quantizer(VP9_COMP *cpi, MACROBLOCK *x) {
  int i;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  int zbin_extra;
  int segment_id = xd->mi_8x8[0]->mbmi.segment_id;
  const int qindex = vp9_get_qindex(&cpi->common.seg, segment_id,
                                    cpi->common.base_qindex);

  int rdmult = vp9_compute_rd_mult(cpi, qindex + cm->y_dc_delta_q);

  // Y
  zbin_extra = (cpi->common.y_dequant[qindex][1] *
                 (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  x->plane[0].quant = cpi->y_quant[qindex];
  x->plane[0].quant_shift = cpi->y_quant_shift[qindex];
  x->plane[0].zbin = cpi->y_zbin[qindex];
  x->plane[0].round = cpi->y_round[qindex];
  x->plane[0].zbin_extra = (int16_t)zbin_extra;
  x->e_mbd.plane[0].dequant = cpi->common.y_dequant[qindex];

  // UV
  zbin_extra = (cpi->common.uv_dequant[qindex][1] *
                (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  for (i = 1; i < 3; i++) {
    x->plane[i].quant = cpi->uv_quant[qindex];
    x->plane[i].quant_shift = cpi->uv_quant_shift[qindex];
    x->plane[i].zbin = cpi->uv_zbin[qindex];
    x->plane[i].round = cpi->uv_round[qindex];
    x->plane[i].zbin_extra = (int16_t)zbin_extra;
    x->e_mbd.plane[i].dequant = cpi->common.uv_dequant[qindex];
  }

#if CONFIG_ALPHA
  x->plane[3].quant = cpi->a_quant[qindex];
  x->plane[3].quant_shift = cpi->a_quant_shift[qindex];
  x->plane[3].zbin = cpi->a_zbin[qindex];
  x->plane[3].round = cpi->a_round[qindex];
  x->plane[3].zbin_extra = (int16_t)zbin_extra;
  x->e_mbd.plane[3].dequant = cpi->common.a_dequant[qindex];
#endif

  x->skip_block = vp9_segfeature_active(&cpi->common.seg, segment_id,
                                        SEG_LVL_SKIP);

  /* save this macroblock QIndex for vp9_update_zbin_extra() */
  x->q_index = qindex;

  /* R/D setup */
  cpi->mb.errorperbit = rdmult >> 6;
  cpi->mb.errorperbit += (cpi->mb.errorperbit == 0);

  vp9_initialize_me_consts(cpi, x->q_index);
}

void vp9_update_zbin_extra(VP9_COMP *cpi, MACROBLOCK *x) {
  const int qindex = x->q_index;
  const int y_zbin_extra = (cpi->common.y_dequant[qindex][1] *
                (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;
  const int uv_zbin_extra = (cpi->common.uv_dequant[qindex][1] *
                  (cpi->zbin_mode_boost + x->act_zbin_adj)) >> 7;

  x->plane[0].zbin_extra = (int16_t)y_zbin_extra;
  x->plane[1].zbin_extra = (int16_t)uv_zbin_extra;
  x->plane[2].zbin_extra = (int16_t)uv_zbin_extra;
}

void vp9_frame_init_quantizer(VP9_COMP *cpi) {
  // Clear Zbin mode boost for default case
  cpi->zbin_mode_boost = 0;

  // MB level quantizer setup
  vp9_mb_init_quantizer(cpi, &cpi->mb);
}

void vp9_set_quantizer(struct VP9_COMP *cpi, int q) {
  VP9_COMMON *cm = &cpi->common;

  cm->base_qindex = q;

  // if any of the delta_q values are changing update flag will
  // have to be set.
  cm->y_dc_delta_q = 0;
  cm->uv_dc_delta_q = 0;
  cm->uv_ac_delta_q = 0;

  // quantizer has to be reinitialized if any delta_q changes.
  // As there are not any here for now this is inactive code.
  // if(update)
  //    vp9_init_quantizer(cpi);
}
