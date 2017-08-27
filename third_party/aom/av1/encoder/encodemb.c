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

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/bitwriter.h"
#include "aom_dsp/quantize.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/idct.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/scan.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encodemb.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/tokenize.h"

#if CONFIG_PVQ
#include "av1/encoder/encint.h"
#include "av1/common/partition.h"
#include "av1/encoder/pvq_encoder.h"
#endif

#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

// Check if one needs to use c version subtraction.
static int check_subtract_block_size(int w, int h) { return w < 4 || h < 4; }

static void subtract_block(const MACROBLOCKD *xd, int rows, int cols,
                           int16_t *diff, ptrdiff_t diff_stride,
                           const uint8_t *src8, ptrdiff_t src_stride,
                           const uint8_t *pred8, ptrdiff_t pred_stride) {
#if !CONFIG_HIGHBITDEPTH
  (void)xd;
#endif

  if (check_subtract_block_size(rows, cols)) {
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      aom_highbd_subtract_block_c(rows, cols, diff, diff_stride, src8,
                                  src_stride, pred8, pred_stride, xd->bd);
      return;
    }
#endif  // CONFIG_HIGHBITDEPTH
    aom_subtract_block_c(rows, cols, diff, diff_stride, src8, src_stride, pred8,
                         pred_stride);

    return;
  }

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    aom_highbd_subtract_block(rows, cols, diff, diff_stride, src8, src_stride,
                              pred8, pred_stride, xd->bd);
    return;
  }
#endif  // CONFIG_HIGHBITDEPTH
  aom_subtract_block(rows, cols, diff, diff_stride, src8, src_stride, pred8,
                     pred_stride);
}

void av1_subtract_txb(MACROBLOCK *x, int plane, BLOCK_SIZE plane_bsize,
                      int blk_col, int blk_row, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &x->e_mbd.plane[plane];
  const int diff_stride = block_size_wide[plane_bsize];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const int tx1d_width = tx_size_wide[tx_size];
  const int tx1d_height = tx_size_high[tx_size];
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  int16_t *src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  subtract_block(xd, tx1d_height, tx1d_width, src_diff, diff_stride, src,
                 src_stride, dst, dst_stride);
}

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &x->e_mbd.plane[plane];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];
  const MACROBLOCKD *xd = &x->e_mbd;

  subtract_block(xd, bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                 pd->dst.buf, pd->dst.stride);
}

// These numbers are empirically obtained.
static const int plane_rd_mult[REF_TYPES][PLANE_TYPES] = {
  { 10, 7 }, { 8, 5 },
};

static INLINE unsigned int get_token_bit_costs(
    unsigned int token_costs[2][COEFF_CONTEXTS][ENTROPY_TOKENS], int skip_eob,
    int ctx, int token) {
  (void)skip_eob;
  return token_costs[token == ZERO_TOKEN || token == EOB_TOKEN][ctx][token];
}

#if !CONFIG_LV_MAP

static int optimize_b_greedy(const AV1_COMMON *cm, MACROBLOCK *mb, int plane,
                             int blk_row, int blk_col, int block,
                             TX_SIZE tx_size, int ctx) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblock_plane *const p = &mb->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ref = is_inter_block(&xd->mi[0]->mbmi);
  uint8_t token_cache[MAX_TX_SQUARE];
  const tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const int eob = p->eobs[block];
  const PLANE_TYPE plane_type = pd->plane_type;
  const int16_t *const dequant_ptr = pd->dequant;
  const uint8_t *const band_translate = get_band_translate(tx_size);
  TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, &xd->mi[0]->mbmi);
  const int16_t *const scan = scan_order->scan;
  const int16_t *const nb = scan_order->neighbors;
  int dqv;
  const int shift = av1_get_tx_scale(tx_size);
#if CONFIG_AOM_QM
  int seg_id = xd->mi[0]->mbmi.segment_id;
  // Use a flat matrix (i.e. no weighting) for 1D and Identity transforms
  const qm_val_t *iqmatrix =
      IS_2D_TRANSFORM(tx_type)
          ? pd->seg_iqmatrix[seg_id][!ref][tx_size]
          : cm->giqmatrix[NUM_QM_LEVELS - 1][0][0][tx_size];
#endif
#if CONFIG_NEW_QUANT
  int dq = get_dq_profile_from_ctx(mb->qindex, ctx, ref, plane_type);
  const dequant_val_type_nuq *dequant_val = pd->dequant_val_nuq[dq];
#endif  // CONFIG_NEW_QUANT
  int sz = 0;
  int64_t rd_cost0, rd_cost1;
  int16_t t0, t1;
  int i, final_eob;
  const int cat6_bits = av1_get_cat6_extrabits_size(tx_size, xd->bd);
  unsigned int(*token_costs)[2][COEFF_CONTEXTS][ENTROPY_TOKENS] =
      mb->token_costs[txsize_sqr_map[tx_size]][plane_type][ref];
  const int default_eob = tx_size_2d[tx_size];

  assert(mb->qindex > 0);

  assert((!plane_type && !plane) || (plane_type && plane));
  assert(eob <= default_eob);

  int64_t rdmult = (mb->rdmult * plane_rd_mult[ref][plane_type]) >> 1;

  int64_t rate0, rate1;
  for (i = 0; i < eob; i++) {
    const int rc = scan[i];
    token_cache[rc] = av1_pt_energy_class[av1_get_token(qcoeff[rc])];
  }

  unsigned int(*token_costs_ptr)[2][COEFF_CONTEXTS][ENTROPY_TOKENS] =
      token_costs;

  final_eob = 0;

  int64_t eob_cost0, eob_cost1;
  tran_low_t before_best_eob_qc = 0;
  tran_low_t before_best_eob_dqc = 0;

  const int ctx0 = ctx;
  /* Record the r-d cost */
  int64_t accu_rate = 0;
  // Initialized to the worst possible error for the largest transform size.
  // This ensures that it never goes negative.
  int64_t accu_error = ((int64_t)1) << 50;

  rate0 = get_token_bit_costs(*(token_costs_ptr + band_translate[0]), 0, ctx0,
                              EOB_TOKEN);
  int64_t best_block_rd_cost = RDCOST(rdmult, rate0, accu_error);

  // int64_t best_block_rd_cost_all0 = best_block_rd_cost;
  int x_prev = 1;
  for (i = 0; i < eob; i++) {
    const int rc = scan[i];
    int x = qcoeff[rc];
    sz = -(x < 0);

    int band_cur = band_translate[i];
    int ctx_cur = (i == 0) ? ctx : get_coef_context(nb, token_cache, i);
    int token_tree_sel_cur = (x_prev == 0);

    if (x == 0) {
      // no need to search when x == 0
      int token = av1_get_token(x);
      rate0 = get_token_bit_costs(*(token_costs_ptr + band_cur),
                                  token_tree_sel_cur, ctx_cur, token);
      accu_rate += rate0;
      x_prev = 0;
      // accu_error does not change when x==0
    } else {
      /*  Computing distortion
       */
      // compute the distortion for the first candidate
      // and the distortion for quantizing to 0.
      int dx0 = abs(coeff[rc]) * (1 << shift);
#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        dx0 >>= xd->bd - 8;
      }
#endif
      int64_t d0 = (int64_t)dx0 * dx0;

      int x_a = x - 2 * sz - 1;
      int64_t d2, d2_a;

      int dx;

#if CONFIG_AOM_QM
      int iwt = iqmatrix[rc];
      dqv = dequant_ptr[rc != 0];
      dqv = ((iwt * (int)dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
#else
      dqv = dequant_ptr[rc != 0];
#endif

      dx = (dqcoeff[rc] - coeff[rc]) * (1 << shift);
#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        int dx_sign = dx < 0 ? 1 : 0;
        dx = abs(dx) >> (xd->bd - 8);
        if (dx_sign) dx = -dx;
      }
#endif  // CONFIG_HIGHBITDEPTH
      d2 = (int64_t)dx * dx;

      /* compute the distortion for the second candidate
       * x_a = x - 2 * sz + 1;
       */
      if (x_a != 0) {
#if CONFIG_NEW_QUANT
        dx = av1_dequant_coeff_nuq(x, dqv, dequant_val[band_translate[i]]) -
             (coeff[rc] << shift);
#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          dx >>= xd->bd - 8;
        }
#endif  // CONFIG_HIGHBITDEPTH
#else   // CONFIG_NEW_QUANT
#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          dx -= ((dqv >> (xd->bd - 8)) + sz) ^ sz;
        } else {
          dx -= (dqv + sz) ^ sz;
        }
#else
        dx -= (dqv + sz) ^ sz;
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_NEW_QUANT
        d2_a = (int64_t)dx * dx;
      } else {
        d2_a = d0;
      }
      /*  Computing rates and r-d cost
       */

      int best_x, best_eob_x;
      int64_t base_bits, next_bits0, next_bits1;
      int64_t next_eob_bits0, next_eob_bits1;

      // rate cost of x
      base_bits = av1_get_token_cost(x, &t0, cat6_bits);
      rate0 = base_bits + get_token_bit_costs(*(token_costs_ptr + band_cur),
                                              token_tree_sel_cur, ctx_cur, t0);

      base_bits = av1_get_token_cost(x_a, &t1, cat6_bits);
      rate1 = base_bits + get_token_bit_costs(*(token_costs_ptr + band_cur),
                                              token_tree_sel_cur, ctx_cur, t1);

      next_bits0 = 0;
      next_bits1 = 0;
      next_eob_bits0 = 0;
      next_eob_bits1 = 0;

      if (i < default_eob - 1) {
        int ctx_next, token_tree_sel_next;
        int band_next = band_translate[i + 1];
        int token_next =
            i + 1 != eob ? av1_get_token(qcoeff[scan[i + 1]]) : EOB_TOKEN;

        token_cache[rc] = av1_pt_energy_class[t0];
        ctx_next = get_coef_context(nb, token_cache, i + 1);
        token_tree_sel_next = (x == 0);

        next_bits0 =
            get_token_bit_costs(*(token_costs_ptr + band_next),
                                token_tree_sel_next, ctx_next, token_next);
        next_eob_bits0 =
            get_token_bit_costs(*(token_costs_ptr + band_next),
                                token_tree_sel_next, ctx_next, EOB_TOKEN);

        token_cache[rc] = av1_pt_energy_class[t1];
        ctx_next = get_coef_context(nb, token_cache, i + 1);
        token_tree_sel_next = (x_a == 0);

        next_bits1 =
            get_token_bit_costs(*(token_costs_ptr + band_next),
                                token_tree_sel_next, ctx_next, token_next);

        if (x_a != 0) {
          next_eob_bits1 =
              get_token_bit_costs(*(token_costs_ptr + band_next),
                                  token_tree_sel_next, ctx_next, EOB_TOKEN);
        }
      }

      rd_cost0 = RDCOST(rdmult, (rate0 + next_bits0), d2);
      rd_cost1 = RDCOST(rdmult, (rate1 + next_bits1), d2_a);

      best_x = (rd_cost1 < rd_cost0);

      eob_cost0 = RDCOST(rdmult, (accu_rate + rate0 + next_eob_bits0),
                         (accu_error + d2 - d0));
      eob_cost1 = eob_cost0;
      if (x_a != 0) {
        eob_cost1 = RDCOST(rdmult, (accu_rate + rate1 + next_eob_bits1),
                           (accu_error + d2_a - d0));
        best_eob_x = (eob_cost1 < eob_cost0);
      } else {
        best_eob_x = 0;
      }

      int dqc, dqc_a = 0;

      dqc = dqcoeff[rc];
      if (best_x + best_eob_x) {
        if (x_a != 0) {
#if CONFIG_NEW_QUANT
          dqc_a = av1_dequant_abscoeff_nuq(abs(x_a), dqv,
                                           dequant_val[band_translate[i]]);
          dqc_a = shift ? ROUND_POWER_OF_TWO(dqc_a, shift) : dqc_a;
          if (sz) dqc_a = -dqc_a;
#else
          if (x_a < 0)
            dqc_a = -((-x_a * dqv) >> shift);
          else
            dqc_a = (x_a * dqv) >> shift;
#endif  // CONFIG_NEW_QUANT
        } else {
          dqc_a = 0;
        }  // if (x_a != 0)
      }

      // record the better quantized value
      if (best_x) {
        qcoeff[rc] = x_a;
        dqcoeff[rc] = dqc_a;

        accu_rate += rate1;
        accu_error += d2_a - d0;
        assert(d2_a <= d0);

        token_cache[rc] = av1_pt_energy_class[t1];
      } else {
        accu_rate += rate0;
        accu_error += d2 - d0;
        assert(d2 <= d0);

        token_cache[rc] = av1_pt_energy_class[t0];
      }
      assert(accu_error >= 0);

      x_prev = qcoeff[rc];

      // determine whether to move the eob position to i+1
      int use_a = (x_a != 0) && (best_eob_x);
      int64_t best_eob_cost_i = use_a ? eob_cost1 : eob_cost0;

      if (best_eob_cost_i < best_block_rd_cost) {
        best_block_rd_cost = best_eob_cost_i;
        final_eob = i + 1;
        if (use_a) {
          before_best_eob_qc = x_a;
          before_best_eob_dqc = dqc_a;
        } else {
          before_best_eob_qc = x;
          before_best_eob_dqc = dqc;
        }
      }
    }  // if (x==0)
  }    // for (i)

  assert(final_eob <= eob);
  if (final_eob > 0) {
    assert(before_best_eob_qc != 0);
    i = final_eob - 1;
    int rc = scan[i];
    qcoeff[rc] = before_best_eob_qc;
    dqcoeff[rc] = before_best_eob_dqc;
  }

  for (i = final_eob; i < eob; i++) {
    int rc = scan[i];
    qcoeff[rc] = 0;
    dqcoeff[rc] = 0;
  }

  mb->plane[plane].eobs[block] = final_eob;
  return final_eob;
}
#endif  // !CONFIG_LV_MAP

int av1_optimize_b(const AV1_COMMON *cm, MACROBLOCK *mb, int plane, int blk_row,
                   int blk_col, int block, BLOCK_SIZE plane_bsize,
                   TX_SIZE tx_size, const ENTROPY_CONTEXT *a,
                   const ENTROPY_CONTEXT *l) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblock_plane *const p = &mb->plane[plane];
  const int eob = p->eobs[block];
  assert((mb->qindex == 0) ^ (xd->lossless[xd->mi[0]->mbmi.segment_id] == 0));
  if (eob == 0) return eob;
  if (xd->lossless[xd->mi[0]->mbmi.segment_id]) return eob;

#if CONFIG_PVQ
  (void)cm;
  (void)tx_size;
  (void)a;
  (void)l;
  return eob;
#endif

#if !CONFIG_LV_MAP
  (void)plane_bsize;
  (void)blk_row;
  (void)blk_col;
#if CONFIG_VAR_TX
  int ctx = get_entropy_context(tx_size, a, l);
#else
  int ctx = combine_entropy_contexts(*a, *l);
#endif  // CONFIG_VAR_TX
  return optimize_b_greedy(cm, mb, plane, blk_row, blk_col, block, tx_size,
                           ctx);
#else   // !CONFIG_LV_MAP
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, a, l, &txb_ctx);
  return av1_optimize_txb(cm, mb, plane, blk_row, blk_col, block, tx_size,
                          &txb_ctx);
#endif  // !CONFIG_LV_MAP
}

#if !CONFIG_PVQ
typedef enum QUANT_FUNC {
  QUANT_FUNC_LOWBD = 0,
  QUANT_FUNC_HIGHBD = 1,
  QUANT_FUNC_TYPES = 2
} QUANT_FUNC;

static AV1_QUANT_FACADE
    quant_func_list[AV1_XFORM_QUANT_TYPES][QUANT_FUNC_TYPES] = {
#if !CONFIG_NEW_QUANT
      { av1_quantize_fp_facade, av1_highbd_quantize_fp_facade },
      { av1_quantize_b_facade, av1_highbd_quantize_b_facade },
      { av1_quantize_dc_facade, av1_highbd_quantize_dc_facade },
#else   // !CONFIG_NEW_QUANT
      { av1_quantize_fp_nuq_facade, av1_highbd_quantize_fp_nuq_facade },
      { av1_quantize_b_nuq_facade, av1_highbd_quantize_b_nuq_facade },
      { av1_quantize_dc_nuq_facade, av1_highbd_quantize_dc_nuq_facade },
#endif  // !CONFIG_NEW_QUANT
      { NULL, NULL }
    };
#endif  // !CONFIG_PVQ

typedef void (*fwdTxfmFunc)(const int16_t *diff, tran_low_t *coeff, int stride,
                            TxfmParam *txfm_param);
static const fwdTxfmFunc fwd_txfm_func[2] = { av1_fwd_txfm,
                                              av1_highbd_fwd_txfm };

void av1_xform_quant(const AV1_COMMON *cm, MACROBLOCK *x, int plane, int block,
                     int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, int ctx,
                     AV1_XFORM_QUANT xform_quant_idx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if !(CONFIG_PVQ || CONFIG_DIST_8X8)
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
#else
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
#endif
  PLANE_TYPE plane_type = get_plane_type(plane);
  TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);

#if CONFIG_AOM_QM || CONFIG_NEW_QUANT
  const int is_inter = is_inter_block(mbmi);
#endif

  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  tran_low_t *const coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  uint16_t *const eob = &p->eobs[block];
  const int diff_stride = block_size_wide[plane_bsize];
#if CONFIG_AOM_QM
  int seg_id = mbmi->segment_id;
  // Use a flat matrix (i.e. no weighting) for 1D and Identity transforms
  const qm_val_t *qmatrix =
      IS_2D_TRANSFORM(tx_type) ? pd->seg_qmatrix[seg_id][!is_inter][tx_size]
                               : cm->gqmatrix[NUM_QM_LEVELS - 1][0][0][tx_size];
  const qm_val_t *iqmatrix =
      IS_2D_TRANSFORM(tx_type)
          ? pd->seg_iqmatrix[seg_id][!is_inter][tx_size]
          : cm->giqmatrix[NUM_QM_LEVELS - 1][0][0][tx_size];
#endif

  TxfmParam txfm_param;

#if CONFIG_PVQ || CONFIG_DIST_8X8 || CONFIG_LGT || CONFIG_MRC_TX
  uint8_t *dst;
  const int dst_stride = pd->dst.stride;
#if CONFIG_PVQ || CONFIG_DIST_8X8
  int16_t *pred;
  const int txw = tx_size_wide[tx_size];
  const int txh = tx_size_high[tx_size];
  int i, j;
#endif
#endif

#if !CONFIG_PVQ
  const int tx2d_size = tx_size_2d[tx_size];
  QUANT_PARAM qparam;
  const int16_t *src_diff;

  src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  qparam.log_scale = av1_get_tx_scale(tx_size);
#if CONFIG_NEW_QUANT
  qparam.tx_size = tx_size;
  qparam.dq = get_dq_profile_from_ctx(x->qindex, ctx, is_inter, plane_type);
#endif  // CONFIG_NEW_QUANT
#if CONFIG_AOM_QM
  qparam.qmatrix = qmatrix;
  qparam.iqmatrix = iqmatrix;
#endif  // CONFIG_AOM_QM
#else
  tran_low_t *ref_coeff = BLOCK_OFFSET(pd->pvq_ref_coeff, block);
  int skip = 1;
  PVQ_INFO *pvq_info = NULL;
  uint8_t *src;
  int16_t *src_int16;
  const int src_stride = p->src.stride;

  (void)ctx;
  (void)scan_order;
  (void)qcoeff;

  if (x->pvq_coded) {
    assert(block < MAX_PVQ_BLOCKS_IN_SB);
    pvq_info = &x->pvq[block][plane];
  }
  src = &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  src_int16 =
      &p->src_int16[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (j = 0; j < txh; j++)
      for (i = 0; i < txw; i++)
        src_int16[diff_stride * j + i] =
            CONVERT_TO_SHORTPTR(src)[src_stride * j + i];
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    for (j = 0; j < txh; j++)
      for (i = 0; i < txw; i++)
        src_int16[diff_stride * j + i] = src[src_stride * j + i];
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
#endif

#if CONFIG_PVQ || CONFIG_DIST_8X8 || CONFIG_LGT || CONFIG_MRC_TX
  dst = &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
#if CONFIG_PVQ || CONFIG_DIST_8X8
  pred = &pd->pred[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];

// copy uint8 orig and predicted block to int16 buffer
// in order to use existing VP10 transform functions
#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (j = 0; j < txh; j++)
      for (i = 0; i < txw; i++)
        pred[diff_stride * j + i] =
            CONVERT_TO_SHORTPTR(dst)[dst_stride * j + i];
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    for (j = 0; j < txh; j++)
      for (i = 0; i < txw; i++)
        pred[diff_stride * j + i] = dst[dst_stride * j + i];
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_PVQ || CONFIG_DIST_8X8
#endif  // CONFIG_PVQ || CONFIG_DIST_8X8 || CONFIG_LGT || CONFIG_MRC_TX

  (void)ctx;

  txfm_param.tx_type = tx_type;
  txfm_param.tx_size = tx_size;
  txfm_param.lossless = xd->lossless[mbmi->segment_id];
#if CONFIG_MRC_TX || CONFIG_LGT
  txfm_param.dst = dst;
  txfm_param.stride = dst_stride;
#endif  // CONFIG_MRC_TX || CONFIG_LGT
#if CONFIG_LGT
  txfm_param.is_inter = is_inter_block(mbmi);
  txfm_param.mode = get_prediction_mode(xd->mi[0], plane, tx_size, block);
#endif

#if !CONFIG_PVQ
  txfm_param.bd = xd->bd;
  const int is_hbd = get_bitdepth_data_path_index(xd);
  fwd_txfm_func[is_hbd](src_diff, coeff, diff_stride, &txfm_param);

  if (xform_quant_idx != AV1_XFORM_QUANT_SKIP_QUANT) {
    if (LIKELY(!x->skip_block)) {
      quant_func_list[xform_quant_idx][is_hbd](
          coeff, tx2d_size, p, qcoeff, pd, dqcoeff, eob, scan_order, &qparam);
    } else {
      av1_quantize_skip(tx2d_size, qcoeff, dqcoeff, eob);
    }
  }
#if CONFIG_LV_MAP
  p->txb_entropy_ctx[block] =
      (uint8_t)av1_get_txb_entropy_context(qcoeff, scan_order, *eob);
#endif  // CONFIG_LV_MAP
  return;
#else  // CONFIG_PVQ
  (void)xform_quant_idx;
#if CONFIG_HIGHBITDEPTH
  txfm_param.bd = xd->bd;
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    av1_highbd_fwd_txfm(src_int16, coeff, diff_stride, &txfm_param);
    av1_highbd_fwd_txfm(pred, ref_coeff, diff_stride, &txfm_param);
  } else {
#endif
    av1_fwd_txfm(src_int16, coeff, diff_stride, &txfm_param);
    av1_fwd_txfm(pred, ref_coeff, diff_stride, &txfm_param);
#if CONFIG_HIGHBITDEPTH
  }
#endif

  // PVQ for inter mode block
  if (!x->skip_block) {
    PVQ_SKIP_TYPE ac_dc_coded =
        av1_pvq_encode_helper(x,
                              coeff,        // target original vector
                              ref_coeff,    // reference vector
                              dqcoeff,      // de-quantized vector
                              eob,          // End of Block marker
                              pd->dequant,  // aom's quantizers
                              plane,        // image plane
                              tx_size,      // block size in log_2 - 2
                              tx_type,
                              &x->rate,  // rate measured
                              x->pvq_speed,
                              pvq_info);  // PVQ info for a block
    skip = ac_dc_coded == PVQ_SKIP;
  }
  x->pvq_skip[plane] = skip;

  if (!skip) mbmi->skip = 0;
#endif  // #if !CONFIG_PVQ
}

static void encode_block(int plane, int block, int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct encode_b_args *const args = arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  int ctx;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  uint8_t *dst;
#if !CONFIG_PVQ
  ENTROPY_CONTEXT *a, *l;
#endif
#if CONFIG_VAR_TX
  int bw = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
#endif
  dst = &pd->dst
             .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];

#if !CONFIG_PVQ
  a = &args->ta[blk_col];
  l = &args->tl[blk_row];
#if CONFIG_VAR_TX
  ctx = get_entropy_context(tx_size, a, l);
#else
  ctx = combine_entropy_contexts(*a, *l);
#endif
#else
  ctx = 0;
#endif  // CONFIG_PVQ

#if CONFIG_VAR_TX
  // Assert not magic number (uninitialized).
  assert(x->blk_skip[plane][blk_row * bw + blk_col] != 234);

  if (x->blk_skip[plane][blk_row * bw + blk_col] == 0) {
#else
  {
#endif
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_FP);
  }
#if CONFIG_VAR_TX
  else {
    p->eobs[block] = 0;
  }
#endif

#if !CONFIG_PVQ
  av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size, a,
                 l);

  av1_set_txb_context(x, plane, block, tx_size, a, l);

  if (p->eobs[block]) *(args->skip) = 0;

  if (p->eobs[block] == 0) return;
#else
  (void)ctx;
  if (!x->pvq_skip[plane]) *(args->skip) = 0;

  if (x->pvq_skip[plane]) return;
#endif
  TX_TYPE tx_type =
      av1_get_tx_type(pd->plane_type, xd, blk_row, blk_col, block, tx_size);
#if CONFIG_LGT
  PREDICTION_MODE mode = get_prediction_mode(xd->mi[0], plane, tx_size, block);
  av1_inverse_transform_block(xd, dqcoeff, mode, tx_type, tx_size, dst,
                              pd->dst.stride, p->eobs[block]);
#else
  av1_inverse_transform_block(xd, dqcoeff, tx_type, tx_size, dst,
                              pd->dst.stride, p->eobs[block]);
#endif
}

#if CONFIG_VAR_TX
static void encode_block_inter(int plane, int block, int blk_row, int blk_col,
                               BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               void *arg) {
  struct encode_b_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  TX_SIZE plane_tx_size;
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    encode_block(plane, block, blk_row, blk_col, plane_bsize, tx_size, arg);
  } else {
    assert(tx_size < TX_SIZES_ALL);
#if CONFIG_RECT_TX_EXT
    int is_qttx = plane_tx_size == quarter_txsize_lookup[plane_bsize];
    const TX_SIZE sub_txs = is_qttx ? plane_tx_size : sub_tx_size_map[tx_size];
    if (is_qttx) assert(blk_row == 0 && blk_col == 0 && block == 0);
#else
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    assert(sub_txs < tx_size);
#endif
    // This is the square transform block partition entry point.
    int bsl = tx_size_wide_unit[sub_txs];
    int i;
    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
#if CONFIG_RECT_TX_EXT
      int is_wide_tx = tx_size_wide_unit[sub_txs] > tx_size_high_unit[sub_txs];
      const int offsetr =
          is_qttx ? (is_wide_tx ? i * tx_size_high_unit[sub_txs] : 0)
                  : blk_row + ((i >> 1) * bsl);
      const int offsetc =
          is_qttx ? (is_wide_tx ? 0 : i * tx_size_wide_unit[sub_txs])
                  : blk_col + ((i & 0x01) * bsl);
#else
      const int offsetr = blk_row + ((i >> 1) * bsl);
      const int offsetc = blk_col + ((i & 0x01) * bsl);
#endif
      int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      encode_block_inter(plane, block, offsetr, offsetc, plane_bsize, sub_txs,
                         arg);
      block += step;
    }
  }
}
#endif

typedef struct encode_block_pass1_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
} encode_block_pass1_args;

static void encode_block_pass1(int plane, int block, int blk_row, int blk_col,
                               BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                               void *arg) {
  encode_block_pass1_args *args = (encode_block_pass1_args *)arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  TxfmParam txfm_param;
  uint8_t *dst;
  int ctx = 0;
  dst = &pd->dst
             .buf[(blk_row * pd->dst.stride + blk_col) << tx_size_wide_log2[0]];

  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  ctx, AV1_XFORM_QUANT_B);
#if !CONFIG_PVQ
  if (p->eobs[block] > 0) {
#else
  if (!x->pvq_skip[plane]) {
    {
      int tx_blk_size;
      int i, j;
      // transform block size in pixels
      tx_blk_size = tx_size_wide[tx_size];

// Since av1 does not have separate function which does inverse transform
// but av1_inv_txfm_add_*x*() also does addition of predicted image to
// inverse transformed image,
// pass blank dummy image to av1_inv_txfm_add_*x*(), i.e. set dst as zeros
#if CONFIG_HIGHBITDEPTH
      if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
        for (j = 0; j < tx_blk_size; j++)
          for (i = 0; i < tx_blk_size; i++)
            CONVERT_TO_SHORTPTR(dst)[j * pd->dst.stride + i] = 0;
      } else {
#endif  // CONFIG_HIGHBITDEPTH
        for (j = 0; j < tx_blk_size; j++)
          for (i = 0; i < tx_blk_size; i++) dst[j * pd->dst.stride + i] = 0;
#if CONFIG_HIGHBITDEPTH
      }
#endif  // CONFIG_HIGHBITDEPTH
    }
#endif  // !CONFIG_PVQ
    txfm_param.bd = xd->bd;
    txfm_param.tx_type = DCT_DCT;
    txfm_param.eob = p->eobs[block];
    txfm_param.lossless = xd->lossless[xd->mi[0]->mbmi.segment_id];
#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      av1_highbd_inv_txfm_add_4x4(dqcoeff, dst, pd->dst.stride, &txfm_param);
      return;
    }
#endif  //  CONFIG_HIGHBITDEPTH
    if (xd->lossless[xd->mi[0]->mbmi.segment_id]) {
      av1_iwht4x4_add(dqcoeff, dst, pd->dst.stride, &txfm_param);
    } else {
      av1_idct4x4_add(dqcoeff, dst, pd->dst.stride, &txfm_param);
    }
  }
}

void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize) {
  encode_block_pass1_args args = { cm, x };
  av1_subtract_plane(x, bsize, 0);
  av1_foreach_transformed_block_in_plane(&x->e_mbd, bsize, 0,
                                         encode_block_pass1, &args);
}

void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize, int mi_row,
                   int mi_col) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct encode_b_args arg = { cm, x, &ctx, &mbmi->skip, NULL, NULL, 1 };
  int plane;

  mbmi->skip = 1;

  if (x->skip) return;

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2
    const int subsampling_x = xd->plane[plane].subsampling_x;
    const int subsampling_y = xd->plane[plane].subsampling_y;

    if (!is_chroma_reference(mi_row, mi_col, bsize, subsampling_x,
                             subsampling_y))
      continue;

    bsize = scale_chroma_bsize(bsize, subsampling_x, subsampling_y);
#else
    (void)mi_row;
    (void)mi_col;
#endif

#if CONFIG_VAR_TX
    // TODO(jingning): Clean this up.
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_wide_log2[0];
    const TX_SIZE max_tx_size = get_vartx_max_txsize(mbmi, plane_bsize);
    const BLOCK_SIZE txb_size = txsize_to_bsize[max_tx_size];
    const int bw = block_size_wide[txb_size] >> tx_size_wide_log2[0];
    const int bh = block_size_high[txb_size] >> tx_size_wide_log2[0];
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];
    av1_get_entropy_contexts(bsize, 0, pd, ctx.ta[plane], ctx.tl[plane]);
#else
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
    av1_get_entropy_contexts(bsize, tx_size, pd, ctx.ta[plane], ctx.tl[plane]);
#endif

#if !CONFIG_PVQ
    av1_subtract_plane(x, bsize, plane);
#endif
    arg.ta = ctx.ta[plane];
    arg.tl = ctx.tl[plane];

#if CONFIG_VAR_TX
    const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
    int mu_blocks_wide =
        block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
    int mu_blocks_high =
        block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

    mu_blocks_wide = AOMMIN(mi_width, mu_blocks_wide);
    mu_blocks_high = AOMMIN(mi_height, mu_blocks_high);

    for (idy = 0; idy < mi_height; idy += mu_blocks_high) {
      for (idx = 0; idx < mi_width; idx += mu_blocks_wide) {
        int blk_row, blk_col;
        const int unit_height = AOMMIN(mu_blocks_high + idy, mi_height);
        const int unit_width = AOMMIN(mu_blocks_wide + idx, mi_width);
        for (blk_row = idy; blk_row < unit_height; blk_row += bh) {
          for (blk_col = idx; blk_col < unit_width; blk_col += bw) {
            encode_block_inter(plane, block, blk_row, blk_col, plane_bsize,
                               max_tx_size, &arg);
            block += step;
          }
        }
      }
    }
#else
    av1_foreach_transformed_block_in_plane(xd, bsize, plane, encode_block,
                                           &arg);
#endif
  }
}

#if CONFIG_SUPERTX
void av1_encode_sb_supertx(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct encode_b_args arg = { cm, x, &ctx, &mbmi->skip, NULL, NULL, 1 };
  int plane;

  mbmi->skip = 1;
  if (x->skip) return;

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
    const struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_VAR_TX
    const TX_SIZE tx_size = TX_4X4;
#else
    const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
#endif
    av1_subtract_plane(x, bsize, plane);
    av1_get_entropy_contexts(bsize, tx_size, pd, ctx.ta[plane], ctx.tl[plane]);
    arg.ta = ctx.ta[plane];
    arg.tl = ctx.tl[plane];
    av1_foreach_transformed_block_in_plane(xd, bsize, plane, encode_block,
                                           &arg);
  }
}
#endif  // CONFIG_SUPERTX

#if !CONFIG_PVQ
void av1_set_txb_context(MACROBLOCK *x, int plane, int block, TX_SIZE tx_size,
                         ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l) {
  (void)tx_size;
  struct macroblock_plane *p = &x->plane[plane];

#if !CONFIG_LV_MAP
  *a = *l = p->eobs[block] > 0;
#else   // !CONFIG_LV_MAP
  *a = *l = p->txb_entropy_ctx[block];
#endif  // !CONFIG_LV_MAP

#if CONFIG_VAR_TX || CONFIG_LV_MAP
  int i;
  for (i = 0; i < tx_size_wide_unit[tx_size]; ++i) a[i] = a[0];

  for (i = 0; i < tx_size_high_unit[tx_size]; ++i) l[i] = l[0];
#endif
}
#endif

static void encode_block_intra_and_set_context(int plane, int block,
                                               int blk_row, int blk_col,
                                               BLOCK_SIZE plane_bsize,
                                               TX_SIZE tx_size, void *arg) {
  av1_encode_block_intra(plane, block, blk_row, blk_col, plane_bsize, tx_size,
                         arg);
#if !CONFIG_PVQ
  struct encode_b_args *const args = arg;
  MACROBLOCK *x = args->x;
  ENTROPY_CONTEXT *a = &args->ta[blk_col];
  ENTROPY_CONTEXT *l = &args->tl[blk_row];
  av1_set_txb_context(x, plane, block, tx_size, a, l);
#endif
}

#if CONFIG_DPCM_INTRA
static int get_eob(const tran_low_t *qcoeff, intptr_t n_coeffs,
                   const int16_t *scan) {
  int eob = -1;
  for (int i = (int)n_coeffs - 1; i >= 0; i--) {
    const int rc = scan[i];
    if (qcoeff[rc]) {
      eob = i;
      break;
    }
  }
  return eob + 1;
}

static void quantize_scaler(int coeff, int16_t zbin, int16_t round_value,
                            int16_t quant, int16_t quant_shift, int16_t dequant,
                            int log_scale, tran_low_t *const qcoeff,
                            tran_low_t *const dqcoeff) {
  zbin = ROUND_POWER_OF_TWO(zbin, log_scale);
  round_value = ROUND_POWER_OF_TWO(round_value, log_scale);
  const int coeff_sign = (coeff >> 31);
  const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
  if (abs_coeff >= zbin) {
    int tmp = clamp(abs_coeff + round_value, INT16_MIN, INT16_MAX);
    tmp = ((((tmp * quant) >> 16) + tmp) * quant_shift) >> (16 - log_scale);
    *qcoeff = (tmp ^ coeff_sign) - coeff_sign;
    *dqcoeff = (*qcoeff * dequant) / (1 << log_scale);
  }
}

#if CONFIG_HIGHBITDEPTH
typedef void (*hbd_dpcm_fwd_tx_func)(const int16_t *input, int stride,
                                     TX_TYPE_1D tx_type, tran_low_t *output,
                                     int dir);

static hbd_dpcm_fwd_tx_func get_hbd_dpcm_fwd_tx_func(int tx_length) {
  switch (tx_length) {
    case 4: return av1_hbd_dpcm_ft4_c;
    case 8: return av1_hbd_dpcm_ft8_c;
    case 16: return av1_hbd_dpcm_ft16_c;
    case 32:
      return av1_hbd_dpcm_ft32_c;
    // TODO(huisu): add support for TX_64X64.
    default: assert(0); return NULL;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

typedef void (*dpcm_fwd_tx_func)(const int16_t *input, int stride,
                                 TX_TYPE_1D tx_type, tran_low_t *output);

static dpcm_fwd_tx_func get_dpcm_fwd_tx_func(int tx_length) {
  switch (tx_length) {
    case 4: return av1_dpcm_ft4_c;
    case 8: return av1_dpcm_ft8_c;
    case 16: return av1_dpcm_ft16_c;
    case 32:
      return av1_dpcm_ft32_c;
    // TODO(huisu): add support for TX_64X64.
    default: assert(0); return NULL;
  }
}

static void process_block_dpcm_vert(TX_SIZE tx_size, TX_TYPE_1D tx_type_1d,
                                    struct macroblockd_plane *const pd,
                                    struct macroblock_plane *const p,
                                    uint8_t *src, int src_stride, uint8_t *dst,
                                    int dst_stride, int16_t *src_diff,
                                    int diff_stride, tran_low_t *coeff,
                                    tran_low_t *qcoeff, tran_low_t *dqcoeff) {
  const int tx1d_width = tx_size_wide[tx_size];
  dpcm_fwd_tx_func forward_tx = get_dpcm_fwd_tx_func(tx1d_width);
  dpcm_inv_txfm_add_func inverse_tx =
      av1_get_dpcm_inv_txfm_add_func(tx1d_width);
  const int tx1d_height = tx_size_high[tx_size];
  const int log_scale = av1_get_tx_scale(tx_size);
  int q_idx = 0;
  for (int r = 0; r < tx1d_height; ++r) {
    // Update prediction.
    if (r > 0) memcpy(dst, dst - dst_stride, tx1d_width * sizeof(dst[0]));
    // Subtraction.
    for (int c = 0; c < tx1d_width; ++c) src_diff[c] = src[c] - dst[c];
    // Forward transform.
    forward_tx(src_diff, 1, tx_type_1d, coeff);
    // Quantization.
    for (int c = 0; c < tx1d_width; ++c) {
      quantize_scaler(coeff[c], p->zbin[q_idx], p->round[q_idx],
                      p->quant[q_idx], p->quant_shift[q_idx],
                      pd->dequant[q_idx], log_scale, &qcoeff[c], &dqcoeff[c]);
      q_idx = 1;
    }
    // Inverse transform.
    inverse_tx(dqcoeff, 1, tx_type_1d, dst);
    // Move to the next row.
    coeff += tx1d_width;
    qcoeff += tx1d_width;
    dqcoeff += tx1d_width;
    src_diff += diff_stride;
    dst += dst_stride;
    src += src_stride;
  }
}

static void process_block_dpcm_horz(TX_SIZE tx_size, TX_TYPE_1D tx_type_1d,
                                    struct macroblockd_plane *const pd,
                                    struct macroblock_plane *const p,
                                    uint8_t *src, int src_stride, uint8_t *dst,
                                    int dst_stride, int16_t *src_diff,
                                    int diff_stride, tran_low_t *coeff,
                                    tran_low_t *qcoeff, tran_low_t *dqcoeff) {
  const int tx1d_height = tx_size_high[tx_size];
  dpcm_fwd_tx_func forward_tx = get_dpcm_fwd_tx_func(tx1d_height);
  dpcm_inv_txfm_add_func inverse_tx =
      av1_get_dpcm_inv_txfm_add_func(tx1d_height);
  const int tx1d_width = tx_size_wide[tx_size];
  const int log_scale = av1_get_tx_scale(tx_size);
  int q_idx = 0;
  for (int c = 0; c < tx1d_width; ++c) {
    for (int r = 0; r < tx1d_height; ++r) {
      // Update prediction.
      if (c > 0) dst[r * dst_stride] = dst[r * dst_stride - 1];
      // Subtraction.
      src_diff[r * diff_stride] = src[r * src_stride] - dst[r * dst_stride];
    }
    // Forward transform.
    tran_low_t tx_buff[64];
    forward_tx(src_diff, diff_stride, tx_type_1d, tx_buff);
    for (int r = 0; r < tx1d_height; ++r) coeff[r * tx1d_width] = tx_buff[r];
    // Quantization.
    for (int r = 0; r < tx1d_height; ++r) {
      quantize_scaler(coeff[r * tx1d_width], p->zbin[q_idx], p->round[q_idx],
                      p->quant[q_idx], p->quant_shift[q_idx],
                      pd->dequant[q_idx], log_scale, &qcoeff[r * tx1d_width],
                      &dqcoeff[r * tx1d_width]);
      q_idx = 1;
    }
    // Inverse transform.
    for (int r = 0; r < tx1d_height; ++r) tx_buff[r] = dqcoeff[r * tx1d_width];
    inverse_tx(tx_buff, dst_stride, tx_type_1d, dst);
    // Move to the next column.
    ++coeff, ++qcoeff, ++dqcoeff, ++src_diff, ++dst, ++src;
  }
}

#if CONFIG_HIGHBITDEPTH
static void hbd_process_block_dpcm_vert(
    TX_SIZE tx_size, TX_TYPE_1D tx_type_1d, int bd,
    struct macroblockd_plane *const pd, struct macroblock_plane *const p,
    uint8_t *src8, int src_stride, uint8_t *dst8, int dst_stride,
    int16_t *src_diff, int diff_stride, tran_low_t *coeff, tran_low_t *qcoeff,
    tran_low_t *dqcoeff) {
  const int tx1d_width = tx_size_wide[tx_size];
  hbd_dpcm_fwd_tx_func forward_tx = get_hbd_dpcm_fwd_tx_func(tx1d_width);
  hbd_dpcm_inv_txfm_add_func inverse_tx =
      av1_get_hbd_dpcm_inv_txfm_add_func(tx1d_width);
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const int tx1d_height = tx_size_high[tx_size];
  const int log_scale = av1_get_tx_scale(tx_size);
  int q_idx = 0;
  for (int r = 0; r < tx1d_height; ++r) {
    // Update prediction.
    if (r > 0) memcpy(dst, dst - dst_stride, tx1d_width * sizeof(dst[0]));
    // Subtraction.
    for (int c = 0; c < tx1d_width; ++c) src_diff[c] = src[c] - dst[c];
    // Forward transform.
    forward_tx(src_diff, 1, tx_type_1d, coeff, 1);
    // Quantization.
    for (int c = 0; c < tx1d_width; ++c) {
      quantize_scaler(coeff[c], p->zbin[q_idx], p->round[q_idx],
                      p->quant[q_idx], p->quant_shift[q_idx],
                      pd->dequant[q_idx], log_scale, &qcoeff[c], &dqcoeff[c]);
      q_idx = 1;
    }
    // Inverse transform.
    inverse_tx(dqcoeff, 1, tx_type_1d, bd, dst, 1);
    // Move to the next row.
    coeff += tx1d_width;
    qcoeff += tx1d_width;
    dqcoeff += tx1d_width;
    src_diff += diff_stride;
    dst += dst_stride;
    src += src_stride;
  }
}

static void hbd_process_block_dpcm_horz(
    TX_SIZE tx_size, TX_TYPE_1D tx_type_1d, int bd,
    struct macroblockd_plane *const pd, struct macroblock_plane *const p,
    uint8_t *src8, int src_stride, uint8_t *dst8, int dst_stride,
    int16_t *src_diff, int diff_stride, tran_low_t *coeff, tran_low_t *qcoeff,
    tran_low_t *dqcoeff) {
  const int tx1d_height = tx_size_high[tx_size];
  hbd_dpcm_fwd_tx_func forward_tx = get_hbd_dpcm_fwd_tx_func(tx1d_height);
  hbd_dpcm_inv_txfm_add_func inverse_tx =
      av1_get_hbd_dpcm_inv_txfm_add_func(tx1d_height);
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const int tx1d_width = tx_size_wide[tx_size];
  const int log_scale = av1_get_tx_scale(tx_size);
  int q_idx = 0;
  for (int c = 0; c < tx1d_width; ++c) {
    for (int r = 0; r < tx1d_height; ++r) {
      // Update prediction.
      if (c > 0) dst[r * dst_stride] = dst[r * dst_stride - 1];
      // Subtraction.
      src_diff[r * diff_stride] = src[r * src_stride] - dst[r * dst_stride];
    }
    // Forward transform.
    tran_low_t tx_buff[64];
    forward_tx(src_diff, diff_stride, tx_type_1d, tx_buff, 0);
    for (int r = 0; r < tx1d_height; ++r) coeff[r * tx1d_width] = tx_buff[r];
    // Quantization.
    for (int r = 0; r < tx1d_height; ++r) {
      quantize_scaler(coeff[r * tx1d_width], p->zbin[q_idx], p->round[q_idx],
                      p->quant[q_idx], p->quant_shift[q_idx],
                      pd->dequant[q_idx], log_scale, &qcoeff[r * tx1d_width],
                      &dqcoeff[r * tx1d_width]);
      q_idx = 1;
    }
    // Inverse transform.
    for (int r = 0; r < tx1d_height; ++r) tx_buff[r] = dqcoeff[r * tx1d_width];
    inverse_tx(tx_buff, dst_stride, tx_type_1d, bd, dst, 0);
    // Move to the next column.
    ++coeff, ++qcoeff, ++dqcoeff, ++src_diff, ++dst, ++src;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

void av1_encode_block_intra_dpcm(const AV1_COMMON *cm, MACROBLOCK *x,
                                 PREDICTION_MODE mode, int plane, int block,
                                 int blk_row, int blk_col,
                                 BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                                 TX_TYPE tx_type, ENTROPY_CONTEXT *ta,
                                 ENTROPY_CONTEXT *tl, int8_t *skip) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const int diff_stride = block_size_wide[plane_bsize];
  const int src_stride = p->src.stride;
  const int dst_stride = pd->dst.stride;
  const int tx1d_width = tx_size_wide[tx_size];
  const int tx1d_height = tx_size_high[tx_size];
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, &xd->mi[0]->mbmi);
  tran_low_t *coeff = BLOCK_OFFSET(p->coeff, block);
  tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << tx_size_wide_log2[0]];
  int16_t *src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << tx_size_wide_log2[0]];
  uint16_t *eob = &p->eobs[block];
  *eob = 0;
  memset(qcoeff, 0, tx1d_height * tx1d_width * sizeof(*qcoeff));
  memset(dqcoeff, 0, tx1d_height * tx1d_width * sizeof(*dqcoeff));

  if (LIKELY(!x->skip_block)) {
    TX_TYPE_1D tx_type_1d = DCT_1D;
    switch (tx_type) {
      case IDTX: tx_type_1d = IDTX_1D; break;
      case V_DCT:
        assert(mode == H_PRED);
        tx_type_1d = DCT_1D;
        break;
      case H_DCT:
        assert(mode == V_PRED);
        tx_type_1d = DCT_1D;
        break;
      default: assert(0);
    }
    switch (mode) {
      case V_PRED:
#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          hbd_process_block_dpcm_vert(tx_size, tx_type_1d, xd->bd, pd, p, src,
                                      src_stride, dst, dst_stride, src_diff,
                                      diff_stride, coeff, qcoeff, dqcoeff);
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          process_block_dpcm_vert(tx_size, tx_type_1d, pd, p, src, src_stride,
                                  dst, dst_stride, src_diff, diff_stride, coeff,
                                  qcoeff, dqcoeff);
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
        break;
      case H_PRED:
#if CONFIG_HIGHBITDEPTH
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          hbd_process_block_dpcm_horz(tx_size, tx_type_1d, xd->bd, pd, p, src,
                                      src_stride, dst, dst_stride, src_diff,
                                      diff_stride, coeff, qcoeff, dqcoeff);
        } else {
#endif  // CONFIG_HIGHBITDEPTH
          process_block_dpcm_horz(tx_size, tx_type_1d, pd, p, src, src_stride,
                                  dst, dst_stride, src_diff, diff_stride, coeff,
                                  qcoeff, dqcoeff);
#if CONFIG_HIGHBITDEPTH
        }
#endif  // CONFIG_HIGHBITDEPTH
        break;
      default: assert(0);
    }
    *eob = get_eob(qcoeff, tx1d_height * tx1d_width, scan_order->scan);
  }

  ta[blk_col] = tl[blk_row] = *eob > 0;
  if (*eob) *skip = 0;
}
#endif  // CONFIG_DPCM_INTRA

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            void *arg) {
  struct encode_b_args *const args = arg;
  AV1_COMMON *cm = args->cm;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  tran_low_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type =
      av1_get_tx_type(plane_type, xd, blk_row, blk_col, block, tx_size);
  uint16_t *eob = &p->eobs[block];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];

  av1_predict_intra_block_facade(xd, plane, block, blk_col, blk_row, tx_size);

#if CONFIG_DPCM_INTRA || CONFIG_LGT
  const PREDICTION_MODE mode =
      get_prediction_mode(xd->mi[0], plane, tx_size, block);
#if CONFIG_DPCM_INTRA
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  if (av1_use_dpcm_intra(plane, mode, tx_type, mbmi)) {
    av1_encode_block_intra_dpcm(cm, x, mode, plane, block, blk_row, blk_col,
                                plane_bsize, tx_size, tx_type, args->ta,
                                args->tl, args->skip);
    return;
  }
#endif  // CONFIG_DPCM_INTRA
#endif  // CONFIG_DPCM_INTRA || CONFIG_LGT

  av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);

  const ENTROPY_CONTEXT *a = &args->ta[blk_col];
  const ENTROPY_CONTEXT *l = &args->tl[blk_row];
  int ctx = combine_entropy_contexts(*a, *l);
  if (args->enable_optimize_b) {
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, blk_row, blk_col, block, plane_bsize, tx_size,
                   a, l);
  } else {
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    ctx, AV1_XFORM_QUANT_B);
  }

#if CONFIG_PVQ
  // *(args->skip) == mbmi->skip
  if (!x->pvq_skip[plane]) *(args->skip) = 0;

  if (x->pvq_skip[plane]) return;
#endif  // CONFIG_PVQ
  av1_inverse_transform_block(xd, dqcoeff,
#if CONFIG_LGT
                              mode,
#endif
                              tx_type, tx_size, dst, dst_stride, *eob);
#if !CONFIG_PVQ
  if (*eob) *(args->skip) = 0;
#else
// Note : *(args->skip) == mbmi->skip
#endif
#if CONFIG_CFL
  if (plane == AOM_PLANE_Y && x->cfl_store_y) {
    // TODO (ltrudeau) Store sub-8x8 inter blocks when bottom right block is
    // intra predicted.
    cfl_store(xd->cfl, dst, dst_stride, blk_row, blk_col, tx_size, plane_bsize);
  }
#endif
}

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane,
                                  int enable_optimize_b, int mi_row,
                                  int mi_col) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  ENTROPY_CONTEXT ta[2 * MAX_MIB_SIZE] = { 0 };
  ENTROPY_CONTEXT tl[2 * MAX_MIB_SIZE] = { 0 };

  struct encode_b_args arg = {
    cm, x, NULL, &xd->mi[0]->mbmi.skip, ta, tl, enable_optimize_b
  };

#if CONFIG_CB4X4
  if (!is_chroma_reference(mi_row, mi_col, bsize,
                           xd->plane[plane].subsampling_x,
                           xd->plane[plane].subsampling_y))
    return;
#else
  (void)mi_row;
  (void)mi_col;
#endif

  if (enable_optimize_b) {
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const TX_SIZE tx_size = av1_get_tx_size(plane, xd);
    av1_get_entropy_contexts(bsize, tx_size, pd, ta, tl);
  }
  av1_foreach_transformed_block_in_plane(
      xd, bsize, plane, encode_block_intra_and_set_context, &arg);
}

#if CONFIG_PVQ
PVQ_SKIP_TYPE av1_pvq_encode_helper(MACROBLOCK *x, tran_low_t *const coeff,
                                    tran_low_t *ref_coeff,
                                    tran_low_t *const dqcoeff, uint16_t *eob,
                                    const int16_t *quant, int plane,
                                    int tx_size, TX_TYPE tx_type, int *rate,
                                    int speed, PVQ_INFO *pvq_info) {
  const int tx_blk_size = tx_size_wide[tx_size];
  daala_enc_ctx *daala_enc = &x->daala_enc;
  PVQ_SKIP_TYPE ac_dc_coded;
  int coeff_shift = 3 - av1_get_tx_scale(tx_size);
  int hbd_downshift = 0;
  int rounding_mask;
  int pvq_dc_quant;
  int use_activity_masking = daala_enc->use_activity_masking;
  int tell;
  int has_dc_skip = 1;
  int i;
  int off = od_qm_offset(tx_size, plane ? 1 : 0);

  DECLARE_ALIGNED(16, tran_low_t, coeff_pvq[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
  DECLARE_ALIGNED(16, tran_low_t, ref_coeff_pvq[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff_pvq[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);

  DECLARE_ALIGNED(16, int32_t, in_int32[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
  DECLARE_ALIGNED(16, int32_t, ref_int32[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
  DECLARE_ALIGNED(16, int32_t, out_int32[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);

  hbd_downshift = x->e_mbd.bd - 8;

  assert(OD_COEFF_SHIFT >= 4);
  // DC quantizer for PVQ
  if (use_activity_masking)
    pvq_dc_quant =
        OD_MAXI(1, (quant[0] << (OD_COEFF_SHIFT - 3) >> hbd_downshift) *
                           daala_enc->state
                               .pvq_qm_q4[plane][od_qm_get_index(tx_size, 0)] >>
                       4);
  else
    pvq_dc_quant =
        OD_MAXI(1, quant[0] << (OD_COEFF_SHIFT - 3) >> hbd_downshift);

  *eob = 0;

#if !CONFIG_ANS
  tell = od_ec_enc_tell_frac(&daala_enc->w.ec);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif

  // Change coefficient ordering for pvq encoding.
  od_raster_to_coding_order(coeff_pvq, tx_blk_size, tx_type, coeff,
                            tx_blk_size);
  od_raster_to_coding_order(ref_coeff_pvq, tx_blk_size, tx_type, ref_coeff,
                            tx_blk_size);

  // copy int16 inputs to int32
  for (i = 0; i < tx_blk_size * tx_blk_size; i++) {
    ref_int32[i] =
        AOM_SIGNED_SHL(ref_coeff_pvq[i], OD_COEFF_SHIFT - coeff_shift) >>
        hbd_downshift;
    in_int32[i] = AOM_SIGNED_SHL(coeff_pvq[i], OD_COEFF_SHIFT - coeff_shift) >>
                  hbd_downshift;
  }

  if (abs(in_int32[0] - ref_int32[0]) < pvq_dc_quant * 141 / 256) { /* 0.55 */
    out_int32[0] = 0;
  } else {
    out_int32[0] = OD_DIV_R0(in_int32[0] - ref_int32[0], pvq_dc_quant);
  }

  ac_dc_coded =
      od_pvq_encode(daala_enc, ref_int32, in_int32, out_int32,
                    OD_MAXI(1, quant[0] << (OD_COEFF_SHIFT - 3) >>
                                   hbd_downshift),  // scale/quantizer
                    OD_MAXI(1, quant[1] << (OD_COEFF_SHIFT - 3) >>
                                   hbd_downshift),  // scale/quantizer
                    plane,
                    tx_size, OD_PVQ_BETA[use_activity_masking][plane][tx_size],
                    0,  // is_keyframe,
                    daala_enc->state.qm + off, daala_enc->state.qm_inv + off,
                    speed,  // speed
                    pvq_info);

  // Encode residue of DC coeff, if required.
  if (!has_dc_skip || out_int32[0]) {
    generic_encode(&daala_enc->w, &daala_enc->state.adapt->model_dc[plane],
                   abs(out_int32[0]) - has_dc_skip,
                   &daala_enc->state.adapt->ex_dc[plane][tx_size][0], 2);
  }
  if (out_int32[0]) {
    aom_write_bit(&daala_enc->w, out_int32[0] < 0);
  }

  // need to save quantized residue of DC coeff
  // so that final pvq bitstream writing can know whether DC is coded.
  if (pvq_info) pvq_info->dq_dc_residue = out_int32[0];

  out_int32[0] = out_int32[0] * pvq_dc_quant;
  out_int32[0] += ref_int32[0];

  // copy int32 result back to int16
  assert(OD_COEFF_SHIFT > coeff_shift);
  rounding_mask = (1 << (OD_COEFF_SHIFT - coeff_shift - 1)) - 1;
  for (i = 0; i < tx_blk_size * tx_blk_size; i++) {
    out_int32[i] = AOM_SIGNED_SHL(out_int32[i], hbd_downshift);
    dqcoeff_pvq[i] = (out_int32[i] + (out_int32[i] < 0) + rounding_mask) >>
                     (OD_COEFF_SHIFT - coeff_shift);
  }

  // Back to original coefficient order
  od_coding_order_to_raster(dqcoeff, tx_blk_size, tx_type, dqcoeff_pvq,
                            tx_blk_size);

  *eob = tx_blk_size * tx_blk_size;

#if !CONFIG_ANS
  *rate = (od_ec_enc_tell_frac(&daala_enc->w.ec) - tell)
          << (AV1_PROB_COST_SHIFT - OD_BITRES);
#else
#error "CONFIG_PVQ currently requires !CONFIG_ANS."
#endif
  assert(*rate >= 0);

  return ac_dc_coded;
}

void av1_store_pvq_enc_info(PVQ_INFO *pvq_info, int *qg, int *theta, int *k,
                            od_coeff *y, int nb_bands, const int *off,
                            int *size, int skip_rest, int skip_dir,
                            int bs) {  // block size in log_2 -2
  int i;
  const int tx_blk_size = tx_size_wide[bs];

  for (i = 0; i < nb_bands; i++) {
    pvq_info->qg[i] = qg[i];
    pvq_info->theta[i] = theta[i];
    pvq_info->k[i] = k[i];
    pvq_info->off[i] = off[i];
    pvq_info->size[i] = size[i];
  }

  memcpy(pvq_info->y, y, tx_blk_size * tx_blk_size * sizeof(od_coeff));

  pvq_info->nb_bands = nb_bands;
  pvq_info->skip_rest = skip_rest;
  pvq_info->skip_dir = skip_dir;
  pvq_info->bs = bs;
}
#endif
