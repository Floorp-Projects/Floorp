/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "./vp9_rtcd.h"
#include "./vpx_config.h"

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_idct.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_reconintra.h"
#include "vp9/common/vp9_systemdependent.h"

#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_tokenize.h"

struct optimize_ctx {
  ENTROPY_CONTEXT ta[MAX_MB_PLANE][16];
  ENTROPY_CONTEXT tl[MAX_MB_PLANE][16];
};

struct encode_b_args {
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  unsigned char *skip;
};

void vp9_subtract_block_c(int rows, int cols,
                          int16_t *diff_ptr, ptrdiff_t diff_stride,
                          const uint8_t *src_ptr, ptrdiff_t src_stride,
                          const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int r, c;

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++)
      diff_ptr[c] = src_ptr[c] - pred_ptr[c];

    diff_ptr += diff_stride;
    pred_ptr += pred_stride;
    src_ptr  += src_stride;
  }
}

static void subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &x->e_mbd.plane[plane];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int bw = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  const int bh = 4 * num_4x4_blocks_high_lookup[plane_bsize];

  vp9_subtract_block(bh, bw, p->src_diff, bw, p->src.buf, p->src.stride,
                     pd->dst.buf, pd->dst.stride);
}

void vp9_subtract_sby(MACROBLOCK *x, BLOCK_SIZE bsize) {
  subtract_plane(x, bsize, 0);
}

void vp9_subtract_sbuv(MACROBLOCK *x, BLOCK_SIZE bsize) {
  int i;

  for (i = 1; i < MAX_MB_PLANE; i++)
    subtract_plane(x, bsize, i);
}

void vp9_subtract_sb(MACROBLOCK *x, BLOCK_SIZE bsize) {
  vp9_subtract_sby(x, bsize);
  vp9_subtract_sbuv(x, bsize);
}

#define RDTRUNC(RM, DM, R, D) ((128 + (R) * (RM)) & 0xFF)
typedef struct vp9_token_state vp9_token_state;

struct vp9_token_state {
  int           rate;
  int           error;
  int           next;
  signed char   token;
  short         qc;
};

// TODO(jimbankoski): experiment to find optimal RD numbers.
#define Y1_RD_MULT 4
#define UV_RD_MULT 2

static const int plane_rd_mult[4] = {
  Y1_RD_MULT,
  UV_RD_MULT,
};

#define UPDATE_RD_COST()\
{\
  rd_cost0 = RDCOST(rdmult, rddiv, rate0, error0);\
  rd_cost1 = RDCOST(rdmult, rddiv, rate1, error1);\
  if (rd_cost0 == rd_cost1) {\
    rd_cost0 = RDTRUNC(rdmult, rddiv, rate0, error0);\
    rd_cost1 = RDTRUNC(rdmult, rddiv, rate1, error1);\
  }\
}

// This function is a place holder for now but may ultimately need
// to scan previous tokens to work out the correct context.
static int trellis_get_coeff_context(const int16_t *scan,
                                     const int16_t *nb,
                                     int idx, int token,
                                     uint8_t *token_cache) {
  int bak = token_cache[scan[idx]], pt;
  token_cache[scan[idx]] = vp9_pt_energy_class[token];
  pt = get_coef_context(nb, token_cache, idx + 1);
  token_cache[scan[idx]] = bak;
  return pt;
}

static void optimize_b(MACROBLOCK *mb,
                       int plane, int block, BLOCK_SIZE plane_bsize,
                       ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l,
                       TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblock_plane *p = &mb->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int ref = is_inter_block(&xd->mi_8x8[0]->mbmi);
  vp9_token_state tokens[1025][2];
  unsigned best_index[1025][2];
  const int16_t *coeff_ptr = BLOCK_OFFSET(mb->plane[plane].coeff, block);
  int16_t *qcoeff_ptr;
  int16_t *dqcoeff_ptr;
  int eob = p->eobs[block], final_eob, sz = 0;
  const int i0 = 0;
  int rc, x, next, i;
  int64_t rdmult, rddiv, rd_cost0, rd_cost1;
  int rate0, rate1, error0, error1, t0, t1;
  int best, band, pt;
  PLANE_TYPE type = pd->plane_type;
  int err_mult = plane_rd_mult[type];
  const int default_eob = 16 << (tx_size << 1);

  const int mul = 1 + (tx_size == TX_32X32);
  uint8_t token_cache[1024];
  const int16_t *dequant_ptr = pd->dequant;
  const uint8_t *const band_translate = get_band_translate(tx_size);
  const scan_order *so = get_scan(xd, tx_size, type, block);
  const int16_t *scan = so->scan;
  const int16_t *nb = so->neighbors;

  assert((!type && !plane) || (type && plane));
  dqcoeff_ptr = BLOCK_OFFSET(pd->dqcoeff, block);
  qcoeff_ptr = BLOCK_OFFSET(p->qcoeff, block);
  assert(eob <= default_eob);

  /* Now set up a Viterbi trellis to evaluate alternative roundings. */
  rdmult = mb->rdmult * err_mult;
  if (!is_inter_block(&mb->e_mbd.mi_8x8[0]->mbmi))
    rdmult = (rdmult * 9) >> 4;
  rddiv = mb->rddiv;
  /* Initialize the sentinel node of the trellis. */
  tokens[eob][0].rate = 0;
  tokens[eob][0].error = 0;
  tokens[eob][0].next = default_eob;
  tokens[eob][0].token = EOB_TOKEN;
  tokens[eob][0].qc = 0;
  *(tokens[eob] + 1) = *(tokens[eob] + 0);
  next = eob;
  for (i = 0; i < eob; i++)
    token_cache[scan[i]] = vp9_pt_energy_class[vp9_dct_value_tokens_ptr[
        qcoeff_ptr[scan[i]]].token];

  for (i = eob; i-- > i0;) {
    int base_bits, d2, dx;

    rc = scan[i];
    x = qcoeff_ptr[rc];
    /* Only add a trellis state for non-zero coefficients. */
    if (x) {
      int shortcut = 0;
      error0 = tokens[next][0].error;
      error1 = tokens[next][1].error;
      /* Evaluate the first possibility for this state. */
      rate0 = tokens[next][0].rate;
      rate1 = tokens[next][1].rate;
      t0 = (vp9_dct_value_tokens_ptr + x)->token;
      /* Consider both possible successor states. */
      if (next < default_eob) {
        band = band_translate[i + 1];
        pt = trellis_get_coeff_context(scan, nb, i, t0, token_cache);
        rate0 +=
          mb->token_costs[tx_size][type][ref][band][0][pt]
                         [tokens[next][0].token];
        rate1 +=
          mb->token_costs[tx_size][type][ref][band][0][pt]
                         [tokens[next][1].token];
      }
      UPDATE_RD_COST();
      /* And pick the best. */
      best = rd_cost1 < rd_cost0;
      base_bits = *(vp9_dct_value_cost_ptr + x);
      dx = mul * (dqcoeff_ptr[rc] - coeff_ptr[rc]);
      d2 = dx * dx;
      tokens[i][0].rate = base_bits + (best ? rate1 : rate0);
      tokens[i][0].error = d2 + (best ? error1 : error0);
      tokens[i][0].next = next;
      tokens[i][0].token = t0;
      tokens[i][0].qc = x;
      best_index[i][0] = best;

      /* Evaluate the second possibility for this state. */
      rate0 = tokens[next][0].rate;
      rate1 = tokens[next][1].rate;

      if ((abs(x)*dequant_ptr[rc != 0] > abs(coeff_ptr[rc]) * mul) &&
          (abs(x)*dequant_ptr[rc != 0] < abs(coeff_ptr[rc]) * mul +
                                         dequant_ptr[rc != 0]))
        shortcut = 1;
      else
        shortcut = 0;

      if (shortcut) {
        sz = -(x < 0);
        x -= 2 * sz + 1;
      }

      /* Consider both possible successor states. */
      if (!x) {
        /* If we reduced this coefficient to zero, check to see if
         *  we need to move the EOB back here.
         */
        t0 = tokens[next][0].token == EOB_TOKEN ? EOB_TOKEN : ZERO_TOKEN;
        t1 = tokens[next][1].token == EOB_TOKEN ? EOB_TOKEN : ZERO_TOKEN;
      } else {
        t0 = t1 = (vp9_dct_value_tokens_ptr + x)->token;
      }
      if (next < default_eob) {
        band = band_translate[i + 1];
        if (t0 != EOB_TOKEN) {
          pt = trellis_get_coeff_context(scan, nb, i, t0, token_cache);
          rate0 += mb->token_costs[tx_size][type][ref][band][!x][pt]
                                  [tokens[next][0].token];
        }
        if (t1 != EOB_TOKEN) {
          pt = trellis_get_coeff_context(scan, nb, i, t1, token_cache);
          rate1 += mb->token_costs[tx_size][type][ref][band][!x][pt]
                                  [tokens[next][1].token];
        }
      }

      UPDATE_RD_COST();
      /* And pick the best. */
      best = rd_cost1 < rd_cost0;
      base_bits = *(vp9_dct_value_cost_ptr + x);

      if (shortcut) {
        dx -= (dequant_ptr[rc != 0] + sz) ^ sz;
        d2 = dx * dx;
      }
      tokens[i][1].rate = base_bits + (best ? rate1 : rate0);
      tokens[i][1].error = d2 + (best ? error1 : error0);
      tokens[i][1].next = next;
      tokens[i][1].token = best ? t1 : t0;
      tokens[i][1].qc = x;
      best_index[i][1] = best;
      /* Finally, make this the new head of the trellis. */
      next = i;
    } else {
      /* There's no choice to make for a zero coefficient, so we don't
       *  add a new trellis node, but we do need to update the costs.
       */
      band = band_translate[i + 1];
      t0 = tokens[next][0].token;
      t1 = tokens[next][1].token;
      /* Update the cost of each path if we're past the EOB token. */
      if (t0 != EOB_TOKEN) {
        tokens[next][0].rate +=
            mb->token_costs[tx_size][type][ref][band][1][0][t0];
        tokens[next][0].token = ZERO_TOKEN;
      }
      if (t1 != EOB_TOKEN) {
        tokens[next][1].rate +=
            mb->token_costs[tx_size][type][ref][band][1][0][t1];
        tokens[next][1].token = ZERO_TOKEN;
      }
      best_index[i][0] = best_index[i][1] = 0;
      /* Don't update next, because we didn't add a new node. */
    }
  }

  /* Now pick the best path through the whole trellis. */
  band = band_translate[i + 1];
  pt = combine_entropy_contexts(*a, *l);
  rate0 = tokens[next][0].rate;
  rate1 = tokens[next][1].rate;
  error0 = tokens[next][0].error;
  error1 = tokens[next][1].error;
  t0 = tokens[next][0].token;
  t1 = tokens[next][1].token;
  rate0 += mb->token_costs[tx_size][type][ref][band][0][pt][t0];
  rate1 += mb->token_costs[tx_size][type][ref][band][0][pt][t1];
  UPDATE_RD_COST();
  best = rd_cost1 < rd_cost0;
  final_eob = i0 - 1;
  vpx_memset(qcoeff_ptr, 0, sizeof(*qcoeff_ptr) * (16 << (tx_size * 2)));
  vpx_memset(dqcoeff_ptr, 0, sizeof(*dqcoeff_ptr) * (16 << (tx_size * 2)));
  for (i = next; i < eob; i = next) {
    x = tokens[i][best].qc;
    if (x) {
      final_eob = i;
    }
    rc = scan[i];
    qcoeff_ptr[rc] = x;
    dqcoeff_ptr[rc] = (x * dequant_ptr[rc != 0]) / mul;

    next = tokens[i][best].next;
    best = best_index[i][best];
  }
  final_eob++;

  mb->plane[plane].eobs[block] = final_eob;
  *a = *l = (final_eob > 0);
}

void vp9_optimize_b(int plane, int block, BLOCK_SIZE plane_bsize,
                    TX_SIZE tx_size, MACROBLOCK *mb, struct optimize_ctx *ctx) {
  int x, y;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &x, &y);
  optimize_b(mb, plane, block, plane_bsize,
             &ctx->ta[plane][x], &ctx->tl[plane][y], tx_size);
}

static void optimize_init_b(int plane, BLOCK_SIZE bsize,
                            struct encode_b_args *args) {
  const MACROBLOCKD *xd = &args->x->e_mbd;
  const struct macroblockd_plane* const pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
  const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
  const MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  const TX_SIZE tx_size = plane ? get_uv_tx_size(mbmi) : mbmi->tx_size;

  vp9_get_entropy_contexts(tx_size, args->ctx->ta[plane], args->ctx->tl[plane],
                           pd->above_context, pd->left_context,
                           num_4x4_w, num_4x4_h);
}
void vp9_xform_quant(MACROBLOCK *x, int plane, int block,
                     BLOCK_SIZE plane_bsize, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *coeff = BLOCK_OFFSET(p->coeff, block);
  int16_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int16_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const scan_order *scan_order;
  uint16_t *eob = &p->eobs[block];
  const int diff_stride = 4 * num_4x4_blocks_wide_lookup[plane_bsize];
  int i, j;
  int16_t *src_diff;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &i, &j);
  src_diff = &p->src_diff[4 * (j * diff_stride + i)];

  switch (tx_size) {
    case TX_32X32:
      scan_order = &vp9_default_scan_orders[TX_32X32];
      if (x->use_lp32x32fdct)
        vp9_fdct32x32_rd(src_diff, coeff, diff_stride);
      else
        vp9_fdct32x32(src_diff, coeff, diff_stride);
      vp9_quantize_b_32x32(coeff, 1024, x->skip_block, p->zbin, p->round,
                           p->quant, p->quant_shift, qcoeff, dqcoeff,
                           pd->dequant, p->zbin_extra, eob, scan_order->scan,
                           scan_order->iscan);
      break;
    case TX_16X16:
      scan_order = &vp9_default_scan_orders[TX_16X16];
      vp9_fdct16x16(src_diff, coeff, diff_stride);
      vp9_quantize_b(coeff, 256, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob,
                     scan_order->scan, scan_order->iscan);
      break;
    case TX_8X8:
      scan_order = &vp9_default_scan_orders[TX_8X8];
      vp9_fdct8x8(src_diff, coeff, diff_stride);
      vp9_quantize_b(coeff, 64, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob,
                     scan_order->scan, scan_order->iscan);
      break;
    case TX_4X4:
      scan_order = &vp9_default_scan_orders[TX_4X4];
      x->fwd_txm4x4(src_diff, coeff, diff_stride);
      vp9_quantize_b(coeff, 16, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob,
                     scan_order->scan, scan_order->iscan);
      break;
    default:
      assert(0);
  }
}

static void encode_block(int plane, int block, BLOCK_SIZE plane_bsize,
                         TX_SIZE tx_size, void *arg) {
  struct encode_b_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx *const ctx = args->ctx;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  int i, j;
  uint8_t *dst;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &i, &j);
  dst = &pd->dst.buf[4 * j * pd->dst.stride + 4 * i];

  // TODO(jingning): per transformed block zero forcing only enabled for
  // luma component. will integrate chroma components as well.
  if (x->zcoeff_blk[tx_size][block] && plane == 0) {
    p->eobs[block] = 0;
    ctx->ta[plane][i] = 0;
    ctx->tl[plane][j] = 0;
    return;
  }

  if (!x->skip_recode)
    vp9_xform_quant(x, plane, block, plane_bsize, tx_size);

  if (x->optimize && (!x->skip_recode || !x->skip_optimize)) {
    vp9_optimize_b(plane, block, plane_bsize, tx_size, x, ctx);
  } else {
    ctx->ta[plane][i] = p->eobs[block] > 0;
    ctx->tl[plane][j] = p->eobs[block] > 0;
  }

  if (p->eobs[block])
    *(args->skip) = 0;

  if (x->skip_encode || p->eobs[block] == 0)
    return;

  switch (tx_size) {
    case TX_32X32:
      vp9_idct32x32_add(dqcoeff, dst, pd->dst.stride, p->eobs[block]);
      break;
    case TX_16X16:
      vp9_idct16x16_add(dqcoeff, dst, pd->dst.stride, p->eobs[block]);
      break;
    case TX_8X8:
      vp9_idct8x8_add(dqcoeff, dst, pd->dst.stride, p->eobs[block]);
      break;
    case TX_4X4:
      // this is like vp9_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      xd->itxm_add(dqcoeff, dst, pd->dst.stride, p->eobs[block]);
      break;
    default:
      assert(0 && "Invalid transform size");
  }
}
static void encode_block_pass1(int plane, int block, BLOCK_SIZE plane_bsize,
                               TX_SIZE tx_size, void *arg) {
  struct encode_b_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  int i, j;
  uint8_t *dst;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &i, &j);
  dst = &pd->dst.buf[4 * j * pd->dst.stride + 4 * i];

  vp9_xform_quant(x, plane, block, plane_bsize, tx_size);

  if (p->eobs[block] == 0)
    return;

  xd->itxm_add(dqcoeff, dst, pd->dst.stride, p->eobs[block]);
}

void vp9_encode_sby(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct encode_b_args arg = {x, &ctx, &mbmi->skip};

  vp9_subtract_sby(x, bsize);
  if (x->optimize)
    optimize_init_b(0, bsize, &arg);

  vp9_foreach_transformed_block_in_plane(xd, bsize, 0, encode_block_pass1,
                                         &arg);
}

void vp9_encode_sb(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct encode_b_args arg = {x, &ctx, &mbmi->skip};

  if (!x->skip_recode)
    vp9_subtract_sb(x, bsize);

  if (x->optimize && (!x->skip_recode || !x->skip_optimize)) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; ++i)
      optimize_init_b(i, bsize, &arg);
  }

  vp9_foreach_transformed_block(xd, bsize, encode_block, &arg);
}

static void encode_block_intra(int plane, int block, BLOCK_SIZE plane_bsize,
                               TX_SIZE tx_size, void *arg) {
  struct encode_b_args* const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi_8x8[0]->mbmi;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *coeff = BLOCK_OFFSET(p->coeff, block);
  int16_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int16_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const scan_order *scan_order;
  TX_TYPE tx_type;
  MB_PREDICTION_MODE mode;
  const int bwl = b_width_log2(plane_bsize);
  const int diff_stride = 4 * (1 << bwl);
  uint8_t *src, *dst;
  int16_t *src_diff;
  uint16_t *eob = &p->eobs[block];
  int i, j;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &i, &j);
  dst = &pd->dst.buf[4 * (j * pd->dst.stride + i)];
  src = &p->src.buf[4 * (j * p->src.stride + i)];
  src_diff = &p->src_diff[4 * (j * diff_stride + i)];

  // if (x->optimize)
  // vp9_optimize_b(plane, block, plane_bsize, tx_size, x, args->ctx);

  switch (tx_size) {
    case TX_32X32:
      scan_order = &vp9_default_scan_orders[TX_32X32];
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      vp9_predict_intra_block(xd, block >> 6, bwl, TX_32X32, mode,
                              x->skip_encode ? src : dst,
                              x->skip_encode ? p->src.stride : pd->dst.stride,
                              dst, pd->dst.stride, i, j, plane);
      if (!x->skip_recode) {
        vp9_subtract_block(32, 32, src_diff, diff_stride,
                           src, p->src.stride, dst, pd->dst.stride);
        if (x->use_lp32x32fdct)
          vp9_fdct32x32_rd(src_diff, coeff, diff_stride);
        else
          vp9_fdct32x32(src_diff, coeff, diff_stride);
        vp9_quantize_b_32x32(coeff, 1024, x->skip_block, p->zbin, p->round,
                             p->quant, p->quant_shift, qcoeff, dqcoeff,
                             pd->dequant, p->zbin_extra, eob, scan_order->scan,
                             scan_order->iscan);
      }
      if (!x->skip_encode && *eob)
        vp9_idct32x32_add(dqcoeff, dst, pd->dst.stride, *eob);
      break;
    case TX_16X16:
      tx_type = get_tx_type_16x16(pd->plane_type, xd);
      scan_order = &vp9_scan_orders[TX_16X16][tx_type];
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      vp9_predict_intra_block(xd, block >> 4, bwl, TX_16X16, mode,
                              x->skip_encode ? src : dst,
                              x->skip_encode ? p->src.stride : pd->dst.stride,
                              dst, pd->dst.stride, i, j, plane);
      if (!x->skip_recode) {
        vp9_subtract_block(16, 16, src_diff, diff_stride,
                           src, p->src.stride, dst, pd->dst.stride);
        vp9_fht16x16(src_diff, coeff, diff_stride, tx_type);
        vp9_quantize_b(coeff, 256, x->skip_block, p->zbin, p->round,
                       p->quant, p->quant_shift, qcoeff, dqcoeff,
                       pd->dequant, p->zbin_extra, eob, scan_order->scan,
                       scan_order->iscan);
      }
      if (!x->skip_encode && *eob)
        vp9_iht16x16_add(tx_type, dqcoeff, dst, pd->dst.stride, *eob);
      break;
    case TX_8X8:
      tx_type = get_tx_type_8x8(pd->plane_type, xd);
      scan_order = &vp9_scan_orders[TX_8X8][tx_type];
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      vp9_predict_intra_block(xd, block >> 2, bwl, TX_8X8, mode,
                              x->skip_encode ? src : dst,
                              x->skip_encode ? p->src.stride : pd->dst.stride,
                              dst, pd->dst.stride, i, j, plane);
      if (!x->skip_recode) {
        vp9_subtract_block(8, 8, src_diff, diff_stride,
                           src, p->src.stride, dst, pd->dst.stride);
        vp9_fht8x8(src_diff, coeff, diff_stride, tx_type);
        vp9_quantize_b(coeff, 64, x->skip_block, p->zbin, p->round, p->quant,
                       p->quant_shift, qcoeff, dqcoeff,
                       pd->dequant, p->zbin_extra, eob, scan_order->scan,
                       scan_order->iscan);
      }
      if (!x->skip_encode && *eob)
        vp9_iht8x8_add(tx_type, dqcoeff, dst, pd->dst.stride, *eob);
      break;
    case TX_4X4:
      tx_type = get_tx_type_4x4(pd->plane_type, xd, block);
      scan_order = &vp9_scan_orders[TX_4X4][tx_type];
      if (mbmi->sb_type < BLOCK_8X8 && plane == 0)
        mode = xd->mi_8x8[0]->bmi[block].as_mode;
      else
        mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;

      vp9_predict_intra_block(xd, block, bwl, TX_4X4, mode,
                              x->skip_encode ? src : dst,
                              x->skip_encode ? p->src.stride : pd->dst.stride,
                              dst, pd->dst.stride, i, j, plane);

      if (!x->skip_recode) {
        vp9_subtract_block(4, 4, src_diff, diff_stride,
                           src, p->src.stride, dst, pd->dst.stride);
        if (tx_type != DCT_DCT)
          vp9_fht4x4(src_diff, coeff, diff_stride, tx_type);
        else
          x->fwd_txm4x4(src_diff, coeff, diff_stride);
        vp9_quantize_b(coeff, 16, x->skip_block, p->zbin, p->round, p->quant,
                       p->quant_shift, qcoeff, dqcoeff,
                       pd->dequant, p->zbin_extra, eob, scan_order->scan,
                       scan_order->iscan);
      }

      if (!x->skip_encode && *eob) {
        if (tx_type == DCT_DCT)
          // this is like vp9_short_idct4x4 but has a special case around eob<=1
          // which is significant (not just an optimization) for the lossless
          // case.
          xd->itxm_add(dqcoeff, dst, pd->dst.stride, *eob);
        else
          vp9_iht4x4_16_add(dqcoeff, dst, pd->dst.stride, tx_type);
      }
      break;
    default:
      assert(0);
  }
  if (*eob)
    *(args->skip) = 0;
}

void vp9_encode_block_intra(MACROBLOCK *x, int plane, int block,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            unsigned char *skip) {
  struct encode_b_args arg = {x, NULL, skip};
  encode_block_intra(plane, block, plane_bsize, tx_size, &arg);
}


void vp9_encode_intra_block_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  struct encode_b_args arg = {x, NULL, &xd->mi_8x8[0]->mbmi.skip};

  vp9_foreach_transformed_block_in_plane(xd, bsize, plane, encode_block_intra,
                                         &arg);
}

int vp9_encode_intra(MACROBLOCK *x, int use_16x16_pred) {
  MB_MODE_INFO * mbmi = &x->e_mbd.mi_8x8[0]->mbmi;
  x->skip_encode = 0;
  mbmi->mode = DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->tx_size = use_16x16_pred ? (mbmi->sb_type >= BLOCK_16X16 ? TX_16X16
                                                                 : TX_8X8)
                                   : TX_4X4;
  vp9_encode_intra_block_plane(x, mbmi->sb_type, 0);
  return vp9_get_mb_ss(x->plane[0].src_diff);
}
