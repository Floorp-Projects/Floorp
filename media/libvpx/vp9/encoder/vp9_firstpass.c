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
#include <limits.h>
#include <stdio.h>
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_block.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_variance.h"
#include "vpx_scale/vpx_scale.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/yv12config.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_entropymv.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_vaq.h"
#include "./vpx_scale_rtcd.h"
// TODO(jkoleszar): for setup_dst_planes
#include "vp9/common/vp9_reconinter.h"

#define OUTPUT_FPF 0

#define IIFACTOR   12.5
#define IIKFACTOR1 12.5
#define IIKFACTOR2 15.0
#define RMAX       512.0
#define GF_RMAX    96.0
#define ERR_DIVISOR   150.0
#define MIN_DECAY_FACTOR 0.1

#define KF_MB_INTRA_MIN 150
#define GF_MB_INTRA_MIN 100

#define DOUBLE_DIVIDE_CHECK(x) ((x) < 0 ? (x) - 0.000001 : (x) + 0.000001)

#define MIN_KF_BOOST        300

#define DISABLE_RC_LONG_TERM_MEM 0

static void swap_yv12(YV12_BUFFER_CONFIG *a, YV12_BUFFER_CONFIG *b) {
  YV12_BUFFER_CONFIG temp = *a;
  *a = *b;
  *b = temp;
}

static int select_cq_level(int qindex) {
  int ret_val = QINDEX_RANGE - 1;
  int i;

  double target_q = (vp9_convert_qindex_to_q(qindex) * 0.5847) + 1.0;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (target_q <= vp9_convert_qindex_to_q(i)) {
      ret_val = i;
      break;
    }
  }

  return ret_val;
}

static int gfboost_qadjust(int qindex) {
  const double q = vp9_convert_qindex_to_q(qindex);
  return (int)((0.00000828 * q * q * q) +
               (-0.0055 * q * q) +
               (1.32 * q) + 79.3);
}

static int kfboost_qadjust(int qindex) {
  const double q = vp9_convert_qindex_to_q(qindex);
  return (int)((0.00000973 * q * q * q) +
               (-0.00613 * q * q) +
               (1.316 * q) + 121.2);
}

// Resets the first pass file to the given position using a relative seek from
// the current position.
static void reset_fpf_position(struct twopass_rc *p,
                               FIRSTPASS_STATS *position) {
  p->stats_in = position;
}

static int lookup_next_frame_stats(const struct twopass_rc *p,
                                   FIRSTPASS_STATS *next_frame) {
  if (p->stats_in >= p->stats_in_end)
    return EOF;

  *next_frame = *p->stats_in;
  return 1;
}


// Read frame stats at an offset from the current position
static int read_frame_stats(const struct twopass_rc *p,
                            FIRSTPASS_STATS *frame_stats, int offset) {
  const FIRSTPASS_STATS *fps_ptr = p->stats_in;

  // Check legality of offset
  if (offset >= 0) {
    if (&fps_ptr[offset] >= p->stats_in_end)
      return EOF;
  } else if (offset < 0) {
    if (&fps_ptr[offset] < p->stats_in_start)
      return EOF;
  }

  *frame_stats = fps_ptr[offset];
  return 1;
}

static int input_stats(struct twopass_rc *p, FIRSTPASS_STATS *fps) {
  if (p->stats_in >= p->stats_in_end)
    return EOF;

  *fps = *p->stats_in;
  ++p->stats_in;
  return 1;
}

static void output_stats(const VP9_COMP *cpi,
                         struct vpx_codec_pkt_list *pktlist,
                         FIRSTPASS_STATS *stats) {
  struct vpx_codec_cx_pkt pkt;
  pkt.kind = VPX_CODEC_STATS_PKT;
  pkt.data.twopass_stats.buf = stats;
  pkt.data.twopass_stats.sz = sizeof(FIRSTPASS_STATS);
  vpx_codec_pkt_list_add(pktlist, &pkt);

// TEMP debug code
#if OUTPUT_FPF

  {
    FILE *fpfile;
    fpfile = fopen("firstpass.stt", "a");

    fprintf(fpfile, "%12.0f %12.0f %12.0f %12.0f %12.0f %12.4f %12.4f"
            "%12.4f %12.4f %12.4f %12.4f %12.4f %12.4f %12.4f"
            "%12.0f %12.0f %12.4f %12.0f %12.0f %12.4f\n",
            stats->frame,
            stats->intra_error,
            stats->coded_error,
            stats->sr_coded_error,
            stats->ssim_weighted_pred_err,
            stats->pcnt_inter,
            stats->pcnt_motion,
            stats->pcnt_second_ref,
            stats->pcnt_neutral,
            stats->MVr,
            stats->mvr_abs,
            stats->MVc,
            stats->mvc_abs,
            stats->MVrv,
            stats->MVcv,
            stats->mv_in_out_count,
            stats->new_mv_count,
            stats->count,
            stats->duration);
    fclose(fpfile);
  }
#endif
}

static void zero_stats(FIRSTPASS_STATS *section) {
  section->frame      = 0.0;
  section->intra_error = 0.0;
  section->coded_error = 0.0;
  section->sr_coded_error = 0.0;
  section->ssim_weighted_pred_err = 0.0;
  section->pcnt_inter  = 0.0;
  section->pcnt_motion  = 0.0;
  section->pcnt_second_ref = 0.0;
  section->pcnt_neutral = 0.0;
  section->MVr        = 0.0;
  section->mvr_abs     = 0.0;
  section->MVc        = 0.0;
  section->mvc_abs     = 0.0;
  section->MVrv       = 0.0;
  section->MVcv       = 0.0;
  section->mv_in_out_count  = 0.0;
  section->new_mv_count = 0.0;
  section->count      = 0.0;
  section->duration   = 1.0;
}

static void accumulate_stats(FIRSTPASS_STATS *section, FIRSTPASS_STATS *frame) {
  section->frame += frame->frame;
  section->intra_error += frame->intra_error;
  section->coded_error += frame->coded_error;
  section->sr_coded_error += frame->sr_coded_error;
  section->ssim_weighted_pred_err += frame->ssim_weighted_pred_err;
  section->pcnt_inter  += frame->pcnt_inter;
  section->pcnt_motion += frame->pcnt_motion;
  section->pcnt_second_ref += frame->pcnt_second_ref;
  section->pcnt_neutral += frame->pcnt_neutral;
  section->MVr        += frame->MVr;
  section->mvr_abs     += frame->mvr_abs;
  section->MVc        += frame->MVc;
  section->mvc_abs     += frame->mvc_abs;
  section->MVrv       += frame->MVrv;
  section->MVcv       += frame->MVcv;
  section->mv_in_out_count  += frame->mv_in_out_count;
  section->new_mv_count += frame->new_mv_count;
  section->count      += frame->count;
  section->duration   += frame->duration;
}

static void subtract_stats(FIRSTPASS_STATS *section, FIRSTPASS_STATS *frame) {
  section->frame -= frame->frame;
  section->intra_error -= frame->intra_error;
  section->coded_error -= frame->coded_error;
  section->sr_coded_error -= frame->sr_coded_error;
  section->ssim_weighted_pred_err -= frame->ssim_weighted_pred_err;
  section->pcnt_inter  -= frame->pcnt_inter;
  section->pcnt_motion -= frame->pcnt_motion;
  section->pcnt_second_ref -= frame->pcnt_second_ref;
  section->pcnt_neutral -= frame->pcnt_neutral;
  section->MVr        -= frame->MVr;
  section->mvr_abs     -= frame->mvr_abs;
  section->MVc        -= frame->MVc;
  section->mvc_abs     -= frame->mvc_abs;
  section->MVrv       -= frame->MVrv;
  section->MVcv       -= frame->MVcv;
  section->mv_in_out_count  -= frame->mv_in_out_count;
  section->new_mv_count -= frame->new_mv_count;
  section->count      -= frame->count;
  section->duration   -= frame->duration;
}

static void avg_stats(FIRSTPASS_STATS *section) {
  if (section->count < 1.0)
    return;

  section->intra_error /= section->count;
  section->coded_error /= section->count;
  section->sr_coded_error /= section->count;
  section->ssim_weighted_pred_err /= section->count;
  section->pcnt_inter  /= section->count;
  section->pcnt_second_ref /= section->count;
  section->pcnt_neutral /= section->count;
  section->pcnt_motion /= section->count;
  section->MVr        /= section->count;
  section->mvr_abs     /= section->count;
  section->MVc        /= section->count;
  section->mvc_abs     /= section->count;
  section->MVrv       /= section->count;
  section->MVcv       /= section->count;
  section->mv_in_out_count   /= section->count;
  section->duration   /= section->count;
}

// Calculate a modified Error used in distributing bits between easier and
// harder frames.
static double calculate_modified_err(const VP9_COMP *cpi,
                                     const FIRSTPASS_STATS *this_frame) {
  const struct twopass_rc *const twopass = &cpi->twopass;
  const FIRSTPASS_STATS *const stats = &twopass->total_stats;
  const double av_err = stats->ssim_weighted_pred_err / stats->count;
  double modified_error = av_err * pow(this_frame->ssim_weighted_pred_err /
                                           DOUBLE_DIVIDE_CHECK(av_err),
                                       cpi->oxcf.two_pass_vbrbias / 100.0);

  return fclamp(modified_error,
                twopass->modified_error_min, twopass->modified_error_max);
}

static const double weight_table[256] = {
  0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000,
  0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000,
  0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000,
  0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.020000,
  0.020000, 0.020000, 0.020000, 0.020000, 0.020000, 0.031250, 0.062500,
  0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.281250,
  0.312500, 0.343750, 0.375000, 0.406250, 0.437500, 0.468750, 0.500000,
  0.531250, 0.562500, 0.593750, 0.625000, 0.656250, 0.687500, 0.718750,
  0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500,
  0.968750, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
  1.000000, 1.000000, 1.000000, 1.000000
};

static double simple_weight(const YV12_BUFFER_CONFIG *buf) {
  int i, j;
  double sum = 0.0;
  const int w = buf->y_crop_width;
  const int h = buf->y_crop_height;
  const uint8_t *row = buf->y_buffer;

  for (i = 0; i < h; ++i) {
    const uint8_t *pixel = row;
    for (j = 0; j < w; ++j)
      sum += weight_table[*pixel++];
    row += buf->y_stride;
  }

  return MAX(0.1, sum / (w * h));
}

// This function returns the maximum target rate per frame.
static int frame_max_bits(const VP9_COMP *cpi) {
  int64_t max_bits =
    ((int64_t)cpi->rc.av_per_frame_bandwidth *
     (int64_t)cpi->oxcf.two_pass_vbrmax_section) / 100;

  if (max_bits < 0)
    max_bits = 0;
  else if (max_bits > cpi->rc.max_frame_bandwidth)
    max_bits = cpi->rc.max_frame_bandwidth;

  return (int)max_bits;
}

void vp9_init_first_pass(VP9_COMP *cpi) {
  zero_stats(&cpi->twopass.total_stats);
}

void vp9_end_first_pass(VP9_COMP *cpi) {
  output_stats(cpi, cpi->output_pkt_list, &cpi->twopass.total_stats);
}

static vp9_variance_fn_t get_block_variance_fn(BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_8X8:
      return vp9_mse8x8;
    case BLOCK_16X8:
      return vp9_mse16x8;
    case BLOCK_8X16:
      return vp9_mse8x16;
    default:
      return vp9_mse16x16;
  }
}

static unsigned int zz_motion_search(const VP9_COMP *cpi, const MACROBLOCK *x) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *const src = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const ref = xd->plane[0].pre[0].buf;
  const int ref_stride = xd->plane[0].pre[0].stride;

  unsigned int sse;
  vp9_variance_fn_t fn = get_block_variance_fn(xd->mi_8x8[0]->mbmi.sb_type);
  fn(src, src_stride, ref, ref_stride, &sse);
  return sse;
}

static void first_pass_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                     const MV *ref_mv, MV *best_mv,
                                     int *best_motion_err) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MV tmp_mv = {0, 0};
  MV ref_mv_full = {ref_mv->row >> 3, ref_mv->col >> 3};
  int num00, tmp_err, n, sr = 0;
  int step_param = 3;
  int further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;
  const BLOCK_SIZE bsize = xd->mi_8x8[0]->mbmi.sb_type;
  vp9_variance_fn_ptr_t v_fn_ptr = cpi->fn_ptr[bsize];
  int new_mv_mode_penalty = 256;
  const int quart_frm = MIN(cpi->common.width, cpi->common.height);

  // refine the motion search range accroding to the frame dimension
  // for first pass test
  while ((quart_frm << sr) < MAX_FULL_PEL_VAL)
    sr++;

  step_param += sr;
  further_steps -= sr;

  // override the default variance function to use MSE
  v_fn_ptr.vf = get_block_variance_fn(bsize);

  // Initial step/diamond search centred on best mv
  tmp_err = cpi->diamond_search_sad(x, &ref_mv_full, &tmp_mv,
                                    step_param,
                                    x->sadperbit16, &num00, &v_fn_ptr,
                                    x->nmvjointcost,
                                    x->mvcost, ref_mv);
  if (tmp_err < INT_MAX - new_mv_mode_penalty)
    tmp_err += new_mv_mode_penalty;

  if (tmp_err < *best_motion_err) {
    *best_motion_err = tmp_err;
    best_mv->row = tmp_mv.row;
    best_mv->col = tmp_mv.col;
  }

  // Further step/diamond searches as necessary
  n = num00;
  num00 = 0;

  while (n < further_steps) {
    n++;

    if (num00) {
      num00--;
    } else {
      tmp_err = cpi->diamond_search_sad(x, &ref_mv_full, &tmp_mv,
                                        step_param + n, x->sadperbit16,
                                        &num00, &v_fn_ptr,
                                        x->nmvjointcost,
                                        x->mvcost, ref_mv);
      if (tmp_err < INT_MAX - new_mv_mode_penalty)
        tmp_err += new_mv_mode_penalty;

      if (tmp_err < *best_motion_err) {
        *best_motion_err = tmp_err;
        best_mv->row = tmp_mv.row;
        best_mv->col = tmp_mv.col;
      }
    }
  }
}

static BLOCK_SIZE get_bsize(const VP9_COMMON *cm, int mb_row, int mb_col) {
  if (2 * mb_col + 1 < cm->mi_cols) {
    return 2 * mb_row + 1 < cm->mi_rows ? BLOCK_16X16
                                        : BLOCK_16X8;
  } else {
    return 2 * mb_row + 1 < cm->mi_rows ? BLOCK_8X16
                                        : BLOCK_8X8;
  }
}

void vp9_first_pass(VP9_COMP *cpi) {
  int mb_row, mb_col;
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  TileInfo tile;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const PICK_MODE_CONTEXT *ctx = &x->sb64_context;
  int i;

  int recon_yoffset, recon_uvoffset;
  YV12_BUFFER_CONFIG *const lst_yv12 = get_ref_frame_buffer(cpi, LAST_FRAME);
  YV12_BUFFER_CONFIG *const gld_yv12 = get_ref_frame_buffer(cpi, GOLDEN_FRAME);
  YV12_BUFFER_CONFIG *const new_yv12 = get_frame_new_buffer(cm);
  const int recon_y_stride = lst_yv12->y_stride;
  const int recon_uv_stride = lst_yv12->uv_stride;
  const int uv_mb_height = 16 >> (lst_yv12->y_height > lst_yv12->uv_height);
  int64_t intra_error = 0;
  int64_t coded_error = 0;
  int64_t sr_coded_error = 0;

  int sum_mvr = 0, sum_mvc = 0;
  int sum_mvr_abs = 0, sum_mvc_abs = 0;
  int64_t sum_mvrs = 0, sum_mvcs = 0;
  int mvcount = 0;
  int intercount = 0;
  int second_ref_count = 0;
  int intrapenalty = 256;
  int neutral_count = 0;
  int new_mv_count = 0;
  int sum_in_vectors = 0;
  uint32_t lastmv_as_int = 0;
  struct twopass_rc *const twopass = &cpi->twopass;
  const MV zero_mv = {0, 0};

  vp9_clear_system_state();  // __asm emms;

  vp9_setup_src_planes(x, cpi->Source, 0, 0);
  setup_pre_planes(xd, 0, lst_yv12, 0, 0, NULL);
  setup_dst_planes(xd, new_yv12, 0, 0);

  xd->mi_8x8 = cm->mi_grid_visible;
  xd->mi_8x8[0] = cm->mi;  // required for vp9_frame_init_quantizer

  vp9_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y);

  vp9_frame_init_quantizer(cpi);

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    p[i].coeff = ctx->coeff_pbuf[i][1];
    p[i].qcoeff = ctx->qcoeff_pbuf[i][1];
    pd[i].dqcoeff = ctx->dqcoeff_pbuf[i][1];
    p[i].eobs = ctx->eobs_pbuf[i][1];
  }
  x->skip_recode = 0;

  vp9_init_mv_probs(cm);
  vp9_initialize_rd_consts(cpi);

  // tiling is ignored in the first pass
  vp9_tile_init(&tile, cm, 0, 0);

  // for each macroblock row in image
  for (mb_row = 0; mb_row < cm->mb_rows; mb_row++) {
    int_mv best_ref_mv;

    best_ref_mv.as_int = 0;

    // reset above block coeffs
    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
    recon_uvoffset = (mb_row * recon_uv_stride * uv_mb_height);

    // Set up limit values for motion vectors to prevent them extending
    // outside the UMV borders
    x->mv_row_min = -((mb_row * 16) + BORDER_MV_PIXELS_B16);
    x->mv_row_max = ((cm->mb_rows - 1 - mb_row) * 16)
                    + BORDER_MV_PIXELS_B16;

    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++) {
      int this_error;
      const int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);
      double error_weight = 1.0;
      const BLOCK_SIZE bsize = get_bsize(cm, mb_row, mb_col);

      vp9_clear_system_state();  // __asm emms;

      xd->plane[0].dst.buf = new_yv12->y_buffer + recon_yoffset;
      xd->plane[1].dst.buf = new_yv12->u_buffer + recon_uvoffset;
      xd->plane[2].dst.buf = new_yv12->v_buffer + recon_uvoffset;
      xd->left_available = (mb_col != 0);
      xd->mi_8x8[0]->mbmi.sb_type = bsize;
      xd->mi_8x8[0]->mbmi.ref_frame[0] = INTRA_FRAME;
      set_mi_row_col(xd, &tile,
                     mb_row << 1, num_8x8_blocks_high_lookup[bsize],
                     mb_col << 1, num_8x8_blocks_wide_lookup[bsize],
                     cm->mi_rows, cm->mi_cols);

      if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
        const int energy = vp9_block_energy(cpi, x, bsize);
        error_weight = vp9_vaq_inv_q_ratio(energy);
      }

      // do intra 16x16 prediction
      this_error = vp9_encode_intra(x, use_dc_pred);
      if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
        vp9_clear_system_state();  // __asm emms;
        this_error *= error_weight;
      }

      // intrapenalty below deals with situations where the intra and inter
      // error scores are very low (eg a plain black frame).
      // We do not have special cases in first pass for 0,0 and nearest etc so
      // all inter modes carry an overhead cost estimate for the mv.
      // When the error score is very low this causes us to pick all or lots of
      // INTRA modes and throw lots of key frames.
      // This penalty adds a cost matching that of a 0,0 mv to the intra case.
      this_error += intrapenalty;

      // Cumulative intra error total
      intra_error += (int64_t)this_error;

      // Set up limit values for motion vectors to prevent them extending
      // outside the UMV borders.
      x->mv_col_min = -((mb_col * 16) + BORDER_MV_PIXELS_B16);
      x->mv_col_max = ((cm->mb_cols - 1 - mb_col) * 16) + BORDER_MV_PIXELS_B16;

      // Other than for the first frame do a motion search
      if (cm->current_video_frame > 0) {
        int tmp_err, motion_error;
        int_mv mv, tmp_mv;

        xd->plane[0].pre[0].buf = lst_yv12->y_buffer + recon_yoffset;
        motion_error = zz_motion_search(cpi, x);
        // Simple 0,0 motion with no mv overhead
        mv.as_int = tmp_mv.as_int = 0;

        // Test last reference frame using the previous best mv as the
        // starting point (best reference) for the search
        first_pass_motion_search(cpi, x, &best_ref_mv.as_mv, &mv.as_mv,
                                 &motion_error);
        if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
          vp9_clear_system_state();  // __asm emms;
          motion_error *= error_weight;
        }

        // If the current best reference mv is not centered on 0,0 then do a 0,0
        // based search as well.
        if (best_ref_mv.as_int) {
          tmp_err = INT_MAX;
          first_pass_motion_search(cpi, x, &zero_mv, &tmp_mv.as_mv,
                                   &tmp_err);
          if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
            vp9_clear_system_state();  // __asm emms;
            tmp_err *= error_weight;
          }

          if (tmp_err < motion_error) {
            motion_error = tmp_err;
            mv.as_int = tmp_mv.as_int;
          }
        }

        // Experimental search in an older reference frame
        if (cm->current_video_frame > 1) {
          // Simple 0,0 motion with no mv overhead
          int gf_motion_error;

          xd->plane[0].pre[0].buf = gld_yv12->y_buffer + recon_yoffset;
          gf_motion_error = zz_motion_search(cpi, x);

          first_pass_motion_search(cpi, x, &zero_mv, &tmp_mv.as_mv,
                                   &gf_motion_error);
          if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
            vp9_clear_system_state();  // __asm emms;
            gf_motion_error *= error_weight;
          }

          if (gf_motion_error < motion_error && gf_motion_error < this_error)
            second_ref_count++;

          // Reset to last frame as reference buffer
          xd->plane[0].pre[0].buf = lst_yv12->y_buffer + recon_yoffset;
          xd->plane[1].pre[0].buf = lst_yv12->u_buffer + recon_uvoffset;
          xd->plane[2].pre[0].buf = lst_yv12->v_buffer + recon_uvoffset;

          // In accumulating a score for the older reference frame
          // take the best of the motion predicted score and
          // the intra coded error (just as will be done for)
          // accumulation of "coded_error" for the last frame.
          if (gf_motion_error < this_error)
            sr_coded_error += gf_motion_error;
          else
            sr_coded_error += this_error;
        } else {
          sr_coded_error += motion_error;
        }
        /* Intra assumed best */
        best_ref_mv.as_int = 0;

        if (motion_error <= this_error) {
          // Keep a count of cases where the inter and intra were
          // very close and very low. This helps with scene cut
          // detection for example in cropped clips with black bars
          // at the sides or top and bottom.
          if (((this_error - intrapenalty) * 9 <= motion_error * 10) &&
              this_error < 2 * intrapenalty)
            neutral_count++;

          mv.as_mv.row *= 8;
          mv.as_mv.col *= 8;
          this_error = motion_error;
          vp9_set_mbmode_and_mvs(xd, NEWMV, &mv.as_mv);
          xd->mi_8x8[0]->mbmi.tx_size = TX_4X4;
          xd->mi_8x8[0]->mbmi.ref_frame[0] = LAST_FRAME;
          xd->mi_8x8[0]->mbmi.ref_frame[1] = NONE;
          vp9_build_inter_predictors_sby(xd, mb_row << 1, mb_col << 1, bsize);
          vp9_encode_sby(x, bsize);
          sum_mvr += mv.as_mv.row;
          sum_mvr_abs += abs(mv.as_mv.row);
          sum_mvc += mv.as_mv.col;
          sum_mvc_abs += abs(mv.as_mv.col);
          sum_mvrs += mv.as_mv.row * mv.as_mv.row;
          sum_mvcs += mv.as_mv.col * mv.as_mv.col;
          intercount++;

          best_ref_mv.as_int = mv.as_int;

          // Was the vector non-zero
          if (mv.as_int) {
            mvcount++;

            // Was it different from the last non zero vector
            if (mv.as_int != lastmv_as_int)
              new_mv_count++;
            lastmv_as_int = mv.as_int;

            // Does the Row vector point inwards or outwards
            if (mb_row < cm->mb_rows / 2) {
              if (mv.as_mv.row > 0)
                sum_in_vectors--;
              else if (mv.as_mv.row < 0)
                sum_in_vectors++;
            } else if (mb_row > cm->mb_rows / 2) {
              if (mv.as_mv.row > 0)
                sum_in_vectors++;
              else if (mv.as_mv.row < 0)
                sum_in_vectors--;
            }

            // Does the Row vector point inwards or outwards
            if (mb_col < cm->mb_cols / 2) {
              if (mv.as_mv.col > 0)
                sum_in_vectors--;
              else if (mv.as_mv.col < 0)
                sum_in_vectors++;
            } else if (mb_col > cm->mb_cols / 2) {
              if (mv.as_mv.col > 0)
                sum_in_vectors++;
              else if (mv.as_mv.col < 0)
                sum_in_vectors--;
            }
          }
        }
      } else {
        sr_coded_error += (int64_t)this_error;
      }
      coded_error += (int64_t)this_error;

      // adjust to the next column of macroblocks
      x->plane[0].src.buf += 16;
      x->plane[1].src.buf += uv_mb_height;
      x->plane[2].src.buf += uv_mb_height;

      recon_yoffset += 16;
      recon_uvoffset += uv_mb_height;
    }

    // adjust to the next row of mbs
    x->plane[0].src.buf += 16 * x->plane[0].src.stride - 16 * cm->mb_cols;
    x->plane[1].src.buf += uv_mb_height * x->plane[1].src.stride -
                           uv_mb_height * cm->mb_cols;
    x->plane[2].src.buf += uv_mb_height * x->plane[1].src.stride -
                           uv_mb_height * cm->mb_cols;

    vp9_clear_system_state();  // __asm emms;
  }

  vp9_clear_system_state();  // __asm emms;
  {
    FIRSTPASS_STATS fps;

    fps.frame = cm->current_video_frame;
    fps.intra_error = intra_error >> 8;
    fps.coded_error = coded_error >> 8;
    fps.sr_coded_error = sr_coded_error >> 8;
    fps.ssim_weighted_pred_err = fps.coded_error * simple_weight(cpi->Source);
    fps.count = 1.0;
    fps.pcnt_inter = (double)intercount / cm->MBs;
    fps.pcnt_second_ref = (double)second_ref_count / cm->MBs;
    fps.pcnt_neutral = (double)neutral_count / cm->MBs;

    if (mvcount > 0) {
      fps.MVr = (double)sum_mvr / mvcount;
      fps.mvr_abs = (double)sum_mvr_abs / mvcount;
      fps.MVc = (double)sum_mvc / mvcount;
      fps.mvc_abs = (double)sum_mvc_abs / mvcount;
      fps.MVrv = ((double)sum_mvrs - (fps.MVr * fps.MVr / mvcount)) / mvcount;
      fps.MVcv = ((double)sum_mvcs - (fps.MVc * fps.MVc / mvcount)) / mvcount;
      fps.mv_in_out_count = (double)sum_in_vectors / (mvcount * 2);
      fps.new_mv_count = new_mv_count;
      fps.pcnt_motion = (double)mvcount / cm->MBs;
    } else {
      fps.MVr = 0.0;
      fps.mvr_abs = 0.0;
      fps.MVc = 0.0;
      fps.mvc_abs = 0.0;
      fps.MVrv = 0.0;
      fps.MVcv = 0.0;
      fps.mv_in_out_count = 0.0;
      fps.new_mv_count = 0.0;
      fps.pcnt_motion = 0.0;
    }

    // TODO(paulwilkins):  Handle the case when duration is set to 0, or
    // something less than the full time between subsequent values of
    // cpi->source_time_stamp.
    fps.duration = (double)(cpi->source->ts_end - cpi->source->ts_start);

    // don't want to do output stats with a stack variable!
    twopass->this_frame_stats = fps;
    output_stats(cpi, cpi->output_pkt_list, &twopass->this_frame_stats);
    accumulate_stats(&twopass->total_stats, &fps);
  }

  // Copy the previous Last Frame back into gf and and arf buffers if
  // the prediction is good enough... but also dont allow it to lag too far
  if ((twopass->sr_update_lag > 3) ||
      ((cm->current_video_frame > 0) &&
       (twopass->this_frame_stats.pcnt_inter > 0.20) &&
       ((twopass->this_frame_stats.intra_error /
         DOUBLE_DIVIDE_CHECK(twopass->this_frame_stats.coded_error)) > 2.0))) {
    vp8_yv12_copy_frame(lst_yv12, gld_yv12);
    twopass->sr_update_lag = 1;
  } else {
    twopass->sr_update_lag++;
  }
  // swap frame pointers so last frame refers to the frame we just compressed
  swap_yv12(lst_yv12, new_yv12);

  vp9_extend_frame_borders(lst_yv12, cm->subsampling_x, cm->subsampling_y);

  // Special case for the first frame. Copy into the GF buffer as a second
  // reference.
  if (cm->current_video_frame == 0)
    vp8_yv12_copy_frame(lst_yv12, gld_yv12);

  // use this to see what the first pass reconstruction looks like
  if (0) {
    char filename[512];
    FILE *recon_file;
    snprintf(filename, sizeof(filename), "enc%04d.yuv",
             (int)cm->current_video_frame);

    if (cm->current_video_frame == 0)
      recon_file = fopen(filename, "wb");
    else
      recon_file = fopen(filename, "ab");

    (void)fwrite(lst_yv12->buffer_alloc, lst_yv12->frame_size, 1, recon_file);
    fclose(recon_file);
  }

  cm->current_video_frame++;
}

// Estimate a cost per mb attributable to overheads such as the coding of
// modes and motion vectors.
// Currently simplistic in its assumptions for testing.
//


static double bitcost(double prob) {
  return -(log(prob) / log(2.0));
}

static int64_t estimate_modemvcost(VP9_COMP *cpi,
                                     FIRSTPASS_STATS *fpstats) {
#if 0
  int mv_cost;
  int mode_cost;

  double av_pct_inter = fpstats->pcnt_inter / fpstats->count;
  double av_pct_motion = fpstats->pcnt_motion / fpstats->count;
  double av_intra = (1.0 - av_pct_inter);

  double zz_cost;
  double motion_cost;
  double intra_cost;

  zz_cost = bitcost(av_pct_inter - av_pct_motion);
  motion_cost = bitcost(av_pct_motion);
  intra_cost = bitcost(av_intra);

  // Estimate of extra bits per mv overhead for mbs
  // << 9 is the normalization to the (bits * 512) used in vp9_rc_bits_per_mb
  mv_cost = ((int)(fpstats->new_mv_count / fpstats->count) * 8) << 9;

  // Crude estimate of overhead cost from modes
  // << 9 is the normalization to (bits * 512) used in vp9_rc_bits_per_mb
  mode_cost =
    (int)((((av_pct_inter - av_pct_motion) * zz_cost) +
           (av_pct_motion * motion_cost) +
           (av_intra * intra_cost)) * cpi->common.MBs) << 9;

  // return mv_cost + mode_cost;
  // TODO(paulwilkins): Fix overhead costs for extended Q range.
#endif
  return 0;
}

static double calc_correction_factor(double err_per_mb,
                                     double err_divisor,
                                     double pt_low,
                                     double pt_high,
                                     int q) {
  const double error_term = err_per_mb / err_divisor;

  // Adjustment based on actual quantizer to power term.
  const double power_term = MIN(vp9_convert_qindex_to_q(q) * 0.0125 + pt_low,
                                pt_high);

  // Calculate correction factor
  if (power_term < 1.0)
    assert(error_term >= 0.0);

  return fclamp(pow(error_term, power_term), 0.05, 5.0);
}

int vp9_twopass_worst_quality(VP9_COMP *cpi, FIRSTPASS_STATS *fpstats,
                              int section_target_bandwitdh) {
  int q;
  const int num_mbs = cpi->common.MBs;
  int target_norm_bits_per_mb;
  const RATE_CONTROL *const rc = &cpi->rc;

  const double section_err = fpstats->coded_error / fpstats->count;
  const double err_per_mb = section_err / num_mbs;

  if (section_target_bandwitdh <= 0)
    return rc->worst_quality;          // Highest value allowed

  target_norm_bits_per_mb = section_target_bandwitdh < (1 << 20)
                              ? (512 * section_target_bandwitdh) / num_mbs
                              : 512 * (section_target_bandwitdh / num_mbs);

  // Try and pick a max Q that will be high enough to encode the
  // content at the given rate.
  for (q = rc->best_quality; q < rc->worst_quality; q++) {
    const double err_correction_factor = calc_correction_factor(err_per_mb,
                                             ERR_DIVISOR, 0.5, 0.90, q);
    const int bits_per_mb_at_this_q = vp9_rc_bits_per_mb(INTER_FRAME, q,
                                                         err_correction_factor);
    if (bits_per_mb_at_this_q <= target_norm_bits_per_mb)
      break;
  }

  // Restriction on active max q for constrained quality mode.
  if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY)
    q = MAX(q, cpi->cq_target_quality);

  return q;
}

extern void vp9_new_framerate(VP9_COMP *cpi, double framerate);

void vp9_init_second_pass(VP9_COMP *cpi) {
  FIRSTPASS_STATS this_frame;
  FIRSTPASS_STATS *start_pos;
  struct twopass_rc *const twopass = &cpi->twopass;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;

  zero_stats(&twopass->total_stats);
  zero_stats(&twopass->total_left_stats);

  if (!twopass->stats_in_end)
    return;

  twopass->total_stats = *twopass->stats_in_end;
  twopass->total_left_stats = twopass->total_stats;

  // each frame can have a different duration, as the frame rate in the source
  // isn't guaranteed to be constant.   The frame rate prior to the first frame
  // encoded in the second pass is a guess.  However the sum duration is not.
  // Its calculated based on the actual durations of all frames from the first
  // pass.
  vp9_new_framerate(cpi, 10000000.0 * twopass->total_stats.count /
                        twopass->total_stats.duration);

  cpi->output_framerate = oxcf->framerate;
  twopass->bits_left = (int64_t)(twopass->total_stats.duration *
                                 oxcf->target_bandwidth / 10000000.0);

  // Calculate a minimum intra value to be used in determining the IIratio
  // scores used in the second pass. We have this minimum to make sure
  // that clips that are static but "low complexity" in the intra domain
  // are still boosted appropriately for KF/GF/ARF
  twopass->kf_intra_err_min = KF_MB_INTRA_MIN * cpi->common.MBs;
  twopass->gf_intra_err_min = GF_MB_INTRA_MIN * cpi->common.MBs;

  // This variable monitors how far behind the second ref update is lagging
  twopass->sr_update_lag = 1;

  // Scan the first pass file and calculate an average Intra / Inter error score
  // ratio for the sequence.
  {
    double sum_iiratio = 0.0;
    start_pos = twopass->stats_in;  // Note the starting "file" position.

    while (input_stats(twopass, &this_frame) != EOF) {
      const double iiratio = this_frame.intra_error /
                                 DOUBLE_DIVIDE_CHECK(this_frame.coded_error);
      sum_iiratio += fclamp(iiratio, 1.0, 20.0);
    }

    twopass->avg_iiratio = sum_iiratio /
        DOUBLE_DIVIDE_CHECK((double)twopass->total_stats.count);

    // Reset file position
    reset_fpf_position(twopass, start_pos);
  }

  // Scan the first pass file and calculate a modified total error based upon
  // the bias/power function used to allocate bits.
  {
    double av_error = twopass->total_stats.ssim_weighted_pred_err /
                      DOUBLE_DIVIDE_CHECK(twopass->total_stats.count);

    start_pos = twopass->stats_in;  // Note starting "file" position

    twopass->modified_error_total = 0.0;
    twopass->modified_error_min =
      (av_error * oxcf->two_pass_vbrmin_section) / 100;
    twopass->modified_error_max =
      (av_error * oxcf->two_pass_vbrmax_section) / 100;

    while (input_stats(twopass, &this_frame) != EOF) {
      twopass->modified_error_total +=
          calculate_modified_err(cpi, &this_frame);
    }
    twopass->modified_error_left = twopass->modified_error_total;

    reset_fpf_position(twopass, start_pos);
  }
}

void vp9_end_second_pass(VP9_COMP *cpi) {
}

// This function gives and estimate of how badly we believe
// the prediction quality is decaying from frame to frame.
static double get_prediction_decay_rate(const VP9_COMMON *cm,
                                        const FIRSTPASS_STATS *next_frame) {
  // Look at the observed drop in prediction quality between the last frame
  // and the GF buffer (which contains an older frame).
  const double mb_sr_err_diff = (next_frame->sr_coded_error -
                                     next_frame->coded_error) / cm->MBs;
  const double second_ref_decay = mb_sr_err_diff <= 512.0
      ? fclamp(pow(1.0 - (mb_sr_err_diff / 512.0), 0.5), 0.85, 1.0)
      : 0.85;

  return MIN(second_ref_decay, next_frame->pcnt_inter);
}

// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static int detect_transition_to_still(VP9_COMP *cpi, int frame_interval,
                                      int still_interval,
                                      double loop_decay_rate,
                                      double last_decay_rate) {
  int trans_to_still = 0;

  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > MIN_GF_INTERVAL &&
      loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int j;
    FIRSTPASS_STATS *position = cpi->twopass.stats_in;
    FIRSTPASS_STATS tmp_next_frame;

    // Look ahead a few frames to see if static condition
    // persists...
    for (j = 0; j < still_interval; j++) {
      if (EOF == input_stats(&cpi->twopass, &tmp_next_frame))
        break;

      if (tmp_next_frame.pcnt_inter - tmp_next_frame.pcnt_motion < 0.999)
        break;
    }

    reset_fpf_position(&cpi->twopass, position);

    // Only if it does do we signal a transition to still
    if (j == still_interval)
      trans_to_still = 1;
  }

  return trans_to_still;
}

// This function detects a flash through the high relative pcnt_second_ref
// score in the frame following a flash frame. The offset passed in should
// reflect this
static int detect_flash(const struct twopass_rc *twopass, int offset) {
  FIRSTPASS_STATS next_frame;

  int flash_detected = 0;

  // Read the frame data.
  // The return is FALSE (no flash detected) if not a valid frame
  if (read_frame_stats(twopass, &next_frame, offset) != EOF) {
    // What we are looking for here is a situation where there is a
    // brief break in prediction (such as a flash) but subsequent frames
    // are reasonably well predicted by an earlier (pre flash) frame.
    // The recovery after a flash is indicated by a high pcnt_second_ref
    // comapred to pcnt_inter.
    if (next_frame.pcnt_second_ref > next_frame.pcnt_inter &&
        next_frame.pcnt_second_ref >= 0.5)
      flash_detected = 1;
  }

  return flash_detected;
}

// Update the motion related elements to the GF arf boost calculation
static void accumulate_frame_motion_stats(
  FIRSTPASS_STATS *this_frame,
  double *this_frame_mv_in_out,
  double *mv_in_out_accumulator,
  double *abs_mv_in_out_accumulator,
  double *mv_ratio_accumulator) {
  double motion_pct;

  // Accumulate motion stats.
  motion_pct = this_frame->pcnt_motion;

  // Accumulate Motion In/Out of frame stats
  *this_frame_mv_in_out = this_frame->mv_in_out_count * motion_pct;
  *mv_in_out_accumulator += this_frame->mv_in_out_count * motion_pct;
  *abs_mv_in_out_accumulator += fabs(this_frame->mv_in_out_count * motion_pct);

  // Accumulate a measure of how uniform (or conversely how random)
  // the motion field is. (A ratio of absmv / mv)
  if (motion_pct > 0.05) {
    const double this_frame_mvr_ratio = fabs(this_frame->mvr_abs) /
                           DOUBLE_DIVIDE_CHECK(fabs(this_frame->MVr));

    const double this_frame_mvc_ratio = fabs(this_frame->mvc_abs) /
                           DOUBLE_DIVIDE_CHECK(fabs(this_frame->MVc));

    *mv_ratio_accumulator += (this_frame_mvr_ratio < this_frame->mvr_abs)
      ? (this_frame_mvr_ratio * motion_pct)
      : this_frame->mvr_abs * motion_pct;

    *mv_ratio_accumulator += (this_frame_mvc_ratio < this_frame->mvc_abs)
      ? (this_frame_mvc_ratio * motion_pct)
      : this_frame->mvc_abs * motion_pct;
  }
}

// Calculate a baseline boost number for the current frame.
static double calc_frame_boost(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame,
                               double this_frame_mv_in_out) {
  double frame_boost;

  // Underlying boost factor is based on inter intra error ratio
  if (this_frame->intra_error > cpi->twopass.gf_intra_err_min)
    frame_boost = (IIFACTOR * this_frame->intra_error /
                   DOUBLE_DIVIDE_CHECK(this_frame->coded_error));
  else
    frame_boost = (IIFACTOR * cpi->twopass.gf_intra_err_min /
                   DOUBLE_DIVIDE_CHECK(this_frame->coded_error));

  // Increase boost for frames where new data coming into frame
  // (eg zoom out). Slightly reduce boost if there is a net balance
  // of motion out of the frame (zoom in).
  // The range for this_frame_mv_in_out is -1.0 to +1.0
  if (this_frame_mv_in_out > 0.0)
    frame_boost += frame_boost * (this_frame_mv_in_out * 2.0);
  // In extreme case boost is halved
  else
    frame_boost += frame_boost * (this_frame_mv_in_out / 2.0);

  return MIN(frame_boost, GF_RMAX);
}

static int calc_arf_boost(VP9_COMP *cpi, int offset,
                          int f_frames, int b_frames,
                          int *f_boost, int *b_boost) {
  FIRSTPASS_STATS this_frame;
  struct twopass_rc *const twopass = &cpi->twopass;
  int i;
  double boost_score = 0.0;
  double mv_ratio_accumulator = 0.0;
  double decay_accumulator = 1.0;
  double this_frame_mv_in_out = 0.0;
  double mv_in_out_accumulator = 0.0;
  double abs_mv_in_out_accumulator = 0.0;
  int arf_boost;
  int flash_detected = 0;

  // Search forward from the proposed arf/next gf position
  for (i = 0; i < f_frames; i++) {
    if (read_frame_stats(twopass, &this_frame, (i + offset)) == EOF)
      break;

    // Update the motion related elements to the boost calculation
    accumulate_frame_motion_stats(&this_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // We want to discount the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Cumulative effect of prediction quality decay
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(&cpi->common, &this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                          ? MIN_DECAY_FACTOR : decay_accumulator;
    }

    boost_score += (decay_accumulator *
                    calc_frame_boost(cpi, &this_frame, this_frame_mv_in_out));
  }

  *f_boost = (int)boost_score;

  // Reset for backward looking loop
  boost_score = 0.0;
  mv_ratio_accumulator = 0.0;
  decay_accumulator = 1.0;
  this_frame_mv_in_out = 0.0;
  mv_in_out_accumulator = 0.0;
  abs_mv_in_out_accumulator = 0.0;

  // Search backward towards last gf position
  for (i = -1; i >= -b_frames; i--) {
    if (read_frame_stats(twopass, &this_frame, (i + offset)) == EOF)
      break;

    // Update the motion related elements to the boost calculation
    accumulate_frame_motion_stats(&this_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // We want to discount the the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Cumulative effect of prediction quality decay
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(&cpi->common, &this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                              ? MIN_DECAY_FACTOR : decay_accumulator;
    }

    boost_score += (decay_accumulator *
                    calc_frame_boost(cpi, &this_frame, this_frame_mv_in_out));
  }
  *b_boost = (int)boost_score;

  arf_boost = (*f_boost + *b_boost);
  if (arf_boost < ((b_frames + f_frames) * 20))
    arf_boost = ((b_frames + f_frames) * 20);

  return arf_boost;
}

#if CONFIG_MULTIPLE_ARF
// Work out the frame coding order for a GF or an ARF group.
// The current implementation codes frames in their natural order for a
// GF group, and inserts additional ARFs into an ARF group using a
// binary split approach.
// NOTE: this function is currently implemented recursively.
static void schedule_frames(VP9_COMP *cpi, const int start, const int end,
                            const int arf_idx, const int gf_or_arf_group,
                            const int level) {
  int i, abs_end, half_range;
  int *cfo = cpi->frame_coding_order;
  int idx = cpi->new_frame_coding_order_period;

  // If (end < 0) an ARF should be coded at position (-end).
  assert(start >= 0);

  // printf("start:%d end:%d\n", start, end);

  // GF Group: code frames in logical order.
  if (gf_or_arf_group == 0) {
    assert(end >= start);
    for (i = start; i <= end; ++i) {
      cfo[idx] = i;
      cpi->arf_buffer_idx[idx] = arf_idx;
      cpi->arf_weight[idx] = -1;
      ++idx;
    }
    cpi->new_frame_coding_order_period = idx;
    return;
  }

  // ARF Group: work out the ARF schedule.
  // Mark ARF frames as negative.
  if (end < 0) {
    // printf("start:%d end:%d\n", -end, -end);
    // ARF frame is at the end of the range.
    cfo[idx] = end;
    // What ARF buffer does this ARF use as predictor.
    cpi->arf_buffer_idx[idx] = (arf_idx > 2) ? (arf_idx - 1) : 2;
    cpi->arf_weight[idx] = level;
    ++idx;
    abs_end = -end;
  } else {
    abs_end = end;
  }

  half_range = (abs_end - start) >> 1;

  // ARFs may not be adjacent, they must be separated by at least
  // MIN_GF_INTERVAL non-ARF frames.
  if ((start + MIN_GF_INTERVAL) >= (abs_end - MIN_GF_INTERVAL)) {
    // printf("start:%d end:%d\n", start, abs_end);
    // Update the coding order and active ARF.
    for (i = start; i <= abs_end; ++i) {
      cfo[idx] = i;
      cpi->arf_buffer_idx[idx] = arf_idx;
      cpi->arf_weight[idx] = -1;
      ++idx;
    }
    cpi->new_frame_coding_order_period = idx;
  } else {
    // Place a new ARF at the mid-point of the range.
    cpi->new_frame_coding_order_period = idx;
    schedule_frames(cpi, start, -(start + half_range), arf_idx + 1,
                    gf_or_arf_group, level + 1);
    schedule_frames(cpi, start + half_range + 1, abs_end, arf_idx,
                    gf_or_arf_group, level + 1);
  }
}

#define FIXED_ARF_GROUP_SIZE 16

void define_fixed_arf_period(VP9_COMP *cpi) {
  int i;
  int max_level = INT_MIN;

  assert(cpi->multi_arf_enabled);
  assert(cpi->oxcf.lag_in_frames >= FIXED_ARF_GROUP_SIZE);

  // Save the weight of the last frame in the sequence before next
  // sequence pattern overwrites it.
  cpi->this_frame_weight = cpi->arf_weight[cpi->sequence_number];
  assert(cpi->this_frame_weight >= 0);

  cpi->twopass.gf_zeromotion_pct = 0;

  // Initialize frame coding order variables.
  cpi->new_frame_coding_order_period = 0;
  cpi->next_frame_in_order = 0;
  cpi->arf_buffered = 0;
  vp9_zero(cpi->frame_coding_order);
  vp9_zero(cpi->arf_buffer_idx);
  vpx_memset(cpi->arf_weight, -1, sizeof(cpi->arf_weight));

  if (cpi->rc.frames_to_key <= (FIXED_ARF_GROUP_SIZE + 8)) {
    // Setup a GF group close to the keyframe.
    cpi->rc.source_alt_ref_pending = 0;
    cpi->rc.baseline_gf_interval = cpi->rc.frames_to_key;
    schedule_frames(cpi, 0, (cpi->rc.baseline_gf_interval - 1), 2, 0, 0);
  } else {
    // Setup a fixed period ARF group.
    cpi->rc.source_alt_ref_pending = 1;
    cpi->rc.baseline_gf_interval = FIXED_ARF_GROUP_SIZE;
    schedule_frames(cpi, 0, -(cpi->rc.baseline_gf_interval - 1), 2, 1, 0);
  }

  // Replace level indicator of -1 with correct level.
  for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
    if (cpi->arf_weight[i] > max_level) {
      max_level = cpi->arf_weight[i];
    }
  }
  ++max_level;
  for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
    if (cpi->arf_weight[i] == -1) {
      cpi->arf_weight[i] = max_level;
    }
  }
  cpi->max_arf_level = max_level;
#if 0
  printf("\nSchedule: ");
  for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
    printf("%4d ", cpi->frame_coding_order[i]);
  }
  printf("\n");
  printf("ARFref:   ");
  for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
    printf("%4d ", cpi->arf_buffer_idx[i]);
  }
  printf("\n");
  printf("Weight:   ");
  for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
    printf("%4d ", cpi->arf_weight[i]);
  }
  printf("\n");
#endif
}
#endif

// Analyse and define a gf/arf group.
static void define_gf_group(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  FIRSTPASS_STATS next_frame = { 0 };
  FIRSTPASS_STATS *start_pos;
  struct twopass_rc *const twopass = &cpi->twopass;
  int i;
  double boost_score = 0.0;
  double old_boost_score = 0.0;
  double gf_group_err = 0.0;
  double gf_first_frame_err = 0.0;
  double mod_frame_err = 0.0;

  double mv_ratio_accumulator = 0.0;
  double decay_accumulator = 1.0;
  double zero_motion_accumulator = 1.0;

  double loop_decay_rate = 1.00;          // Starting decay rate
  double last_loop_decay_rate = 1.00;

  double this_frame_mv_in_out = 0.0;
  double mv_in_out_accumulator = 0.0;
  double abs_mv_in_out_accumulator = 0.0;
  double mv_ratio_accumulator_thresh;
  const int max_bits = frame_max_bits(cpi);     // Max for a single frame

  unsigned int allow_alt_ref = cpi->oxcf.play_alternate &&
                               cpi->oxcf.lag_in_frames;

  int f_boost = 0;
  int b_boost = 0;
  int flash_detected;
  int active_max_gf_interval;
  RATE_CONTROL *const rc = &cpi->rc;

  twopass->gf_group_bits = 0;

  vp9_clear_system_state();  // __asm emms;

  start_pos = twopass->stats_in;

  // Load stats for the current frame.
  mod_frame_err = calculate_modified_err(cpi, this_frame);

  // Note the error of the frame at the start of the group (this will be
  // the GF frame error if we code a normal gf
  gf_first_frame_err = mod_frame_err;

  // If this is a key frame or the overlay from a previous arf then
  // The error score / cost of this frame has already been accounted for.
  if (cpi->common.frame_type == KEY_FRAME || rc->source_alt_ref_active)
    gf_group_err -= gf_first_frame_err;

  // Motion breakout threshold for loop below depends on image size.
  mv_ratio_accumulator_thresh = (cpi->common.width + cpi->common.height) / 10.0;

  // Work out a maximum interval for the GF.
  // If the image appears completely static we can extend beyond this.
  // The value chosen depends on the active Q range. At low Q we have
  // bits to spare and are better with a smaller interval and smaller boost.
  // At high Q when there are few bits to spare we are better with a longer
  // interval to spread the cost of the GF.
  //
  active_max_gf_interval =
    12 + ((int)vp9_convert_qindex_to_q(rc->last_q[INTER_FRAME]) >> 5);

  if (active_max_gf_interval > rc->max_gf_interval)
    active_max_gf_interval = rc->max_gf_interval;

  i = 0;
  while (i < twopass->static_scene_max_gf_interval && i < rc->frames_to_key) {
    i++;    // Increment the loop counter

    // Accumulate error score of frames in this gf group
    mod_frame_err = calculate_modified_err(cpi, this_frame);
    gf_group_err += mod_frame_err;

    if (EOF == input_stats(twopass, &next_frame))
      break;

    // Test for the case where there is a brief flash but the prediction
    // quality back to an earlier frame is then restored.
    flash_detected = detect_flash(twopass, 0);

    // Update the motion related elements to the boost calculation
    accumulate_frame_motion_stats(&next_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // Cumulative effect of prediction quality decay
    if (!flash_detected) {
      last_loop_decay_rate = loop_decay_rate;
      loop_decay_rate = get_prediction_decay_rate(&cpi->common, &next_frame);
      decay_accumulator = decay_accumulator * loop_decay_rate;

      // Monitor for static sections.
      if ((next_frame.pcnt_inter - next_frame.pcnt_motion) <
          zero_motion_accumulator) {
        zero_motion_accumulator = next_frame.pcnt_inter -
                                      next_frame.pcnt_motion;
      }

      // Break clause to detect very still sections after motion
      // (for example a static image after a fade or other transition).
      if (detect_transition_to_still(cpi, i, 5, loop_decay_rate,
                                     last_loop_decay_rate)) {
        allow_alt_ref = 0;
        break;
      }
    }

    // Calculate a boost number for this frame
    boost_score += (decay_accumulator *
       calc_frame_boost(cpi, &next_frame, this_frame_mv_in_out));

    // Break out conditions.
    if (
      // Break at cpi->max_gf_interval unless almost totally static
      (i >= active_max_gf_interval && (zero_motion_accumulator < 0.995)) ||
      (
        // Don't break out with a very short interval
        (i > MIN_GF_INTERVAL) &&
        ((boost_score > 125.0) || (next_frame.pcnt_inter < 0.75)) &&
        (!flash_detected) &&
        ((mv_ratio_accumulator > mv_ratio_accumulator_thresh) ||
         (abs_mv_in_out_accumulator > 3.0) ||
         (mv_in_out_accumulator < -2.0) ||
         ((boost_score - old_boost_score) < IIFACTOR)))) {
      boost_score = old_boost_score;
      break;
    }

    *this_frame = next_frame;

    old_boost_score = boost_score;
  }

  twopass->gf_zeromotion_pct = (int)(zero_motion_accumulator * 1000.0);

  // Don't allow a gf too near the next kf
  if ((rc->frames_to_key - i) < MIN_GF_INTERVAL) {
    while (i < (rc->frames_to_key + !rc->next_key_frame_forced)) {
      i++;

      if (EOF == input_stats(twopass, this_frame))
        break;

      if (i < rc->frames_to_key) {
        mod_frame_err = calculate_modified_err(cpi, this_frame);
        gf_group_err += mod_frame_err;
      }
    }
  }

#if CONFIG_MULTIPLE_ARF
  if (cpi->multi_arf_enabled) {
    // Initialize frame coding order variables.
    cpi->new_frame_coding_order_period = 0;
    cpi->next_frame_in_order = 0;
    cpi->arf_buffered = 0;
    vp9_zero(cpi->frame_coding_order);
    vp9_zero(cpi->arf_buffer_idx);
    vpx_memset(cpi->arf_weight, -1, sizeof(cpi->arf_weight));
  }
#endif

  // Set the interval until the next gf.
  if (cpi->common.frame_type == KEY_FRAME || rc->source_alt_ref_active)
    rc->baseline_gf_interval = i - 1;
  else
    rc->baseline_gf_interval = i;

  // Should we use the alternate reference frame
  if (allow_alt_ref &&
      (i < cpi->oxcf.lag_in_frames) &&
      (i >= MIN_GF_INTERVAL) &&
      // for real scene cuts (not forced kfs) dont allow arf very near kf.
      (rc->next_key_frame_forced ||
      (i <= (rc->frames_to_key - MIN_GF_INTERVAL)))) {
    // Alternative boost calculation for alt ref
    rc->gfu_boost = calc_arf_boost(cpi, 0, (i - 1), (i - 1), &f_boost,
                                   &b_boost);
    rc->source_alt_ref_pending = 1;

#if CONFIG_MULTIPLE_ARF
    // Set the ARF schedule.
    if (cpi->multi_arf_enabled) {
      schedule_frames(cpi, 0, -(rc->baseline_gf_interval - 1), 2, 1, 0);
    }
#endif
  } else {
    rc->gfu_boost = (int)boost_score;
    rc->source_alt_ref_pending = 0;
#if CONFIG_MULTIPLE_ARF
    // Set the GF schedule.
    if (cpi->multi_arf_enabled) {
      schedule_frames(cpi, 0, rc->baseline_gf_interval - 1, 2, 0, 0);
      assert(cpi->new_frame_coding_order_period ==
             rc->baseline_gf_interval);
    }
#endif
  }

#if CONFIG_MULTIPLE_ARF
  if (cpi->multi_arf_enabled && (cpi->common.frame_type != KEY_FRAME)) {
    int max_level = INT_MIN;
    // Replace level indicator of -1 with correct level.
    for (i = 0; i < cpi->frame_coding_order_period; ++i) {
      if (cpi->arf_weight[i] > max_level) {
        max_level = cpi->arf_weight[i];
      }
    }
    ++max_level;
    for (i = 0; i < cpi->frame_coding_order_period; ++i) {
      if (cpi->arf_weight[i] == -1) {
        cpi->arf_weight[i] = max_level;
      }
    }
    cpi->max_arf_level = max_level;
  }
#if 0
  if (cpi->multi_arf_enabled) {
    printf("\nSchedule: ");
    for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
      printf("%4d ", cpi->frame_coding_order[i]);
    }
    printf("\n");
    printf("ARFref:   ");
    for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
      printf("%4d ", cpi->arf_buffer_idx[i]);
    }
    printf("\n");
    printf("Weight:   ");
    for (i = 0; i < cpi->new_frame_coding_order_period; ++i) {
      printf("%4d ", cpi->arf_weight[i]);
    }
    printf("\n");
  }
#endif
#endif

  // Calculate the bits to be allocated to the group as a whole
  if (twopass->kf_group_bits > 0 && twopass->kf_group_error_left > 0) {
    twopass->gf_group_bits = (int64_t)(cpi->twopass.kf_group_bits *
                (gf_group_err / cpi->twopass.kf_group_error_left));
  } else {
    twopass->gf_group_bits = 0;
  }
  twopass->gf_group_bits = (twopass->gf_group_bits < 0) ?
     0 : (twopass->gf_group_bits > twopass->kf_group_bits) ?
     twopass->kf_group_bits : twopass->gf_group_bits;

  // Clip cpi->twopass.gf_group_bits based on user supplied data rate
  // variability limit (cpi->oxcf.two_pass_vbrmax_section)
  if (twopass->gf_group_bits > (int64_t)max_bits * rc->baseline_gf_interval)
    twopass->gf_group_bits = (int64_t)max_bits * rc->baseline_gf_interval;

  // Reset the file position
  reset_fpf_position(twopass, start_pos);

  // Assign  bits to the arf or gf.
  for (i = 0; i <= (rc->source_alt_ref_pending &&
                    cpi->common.frame_type != KEY_FRAME); ++i) {
    int allocation_chunks;
    int q = rc->last_q[INTER_FRAME];
    int gf_bits;

    int boost = (rc->gfu_boost * gfboost_qadjust(q)) / 100;

    // Set max and minimum boost and hence minimum allocation
    boost = clamp(boost, 125, (rc->baseline_gf_interval + 1) * 200);

    if (rc->source_alt_ref_pending && i == 0)
      allocation_chunks = ((rc->baseline_gf_interval + 1) * 100) + boost;
    else
      allocation_chunks = (rc->baseline_gf_interval * 100) + (boost - 100);

    // Prevent overflow
    if (boost > 1023) {
      int divisor = boost >> 10;
      boost /= divisor;
      allocation_chunks /= divisor;
    }

    // Calculate the number of bits to be spent on the gf or arf based on
    // the boost number
    gf_bits = (int)((double)boost * (twopass->gf_group_bits /
                  (double)allocation_chunks));

    // If the frame that is to be boosted is simpler than the average for
    // the gf/arf group then use an alternative calculation
    // based on the error score of the frame itself
    if (rc->baseline_gf_interval < 1 ||
        mod_frame_err < gf_group_err / (double)rc->baseline_gf_interval) {
      double alt_gf_grp_bits = (double)twopass->kf_group_bits  *
        (mod_frame_err * (double)rc->baseline_gf_interval) /
        DOUBLE_DIVIDE_CHECK(twopass->kf_group_error_left);

      int alt_gf_bits = (int)((double)boost * (alt_gf_grp_bits /
                                           (double)allocation_chunks));

      if (gf_bits > alt_gf_bits)
        gf_bits = alt_gf_bits;
    } else {
      // If it is harder than other frames in the group make sure it at
      // least receives an allocation in keeping with its relative error
      // score, otherwise it may be worse off than an "un-boosted" frame.
      int alt_gf_bits = (int)((double)twopass->kf_group_bits *
                        mod_frame_err /
                        DOUBLE_DIVIDE_CHECK(twopass->kf_group_error_left));

      if (alt_gf_bits > gf_bits)
        gf_bits = alt_gf_bits;
    }

    // Dont allow a negative value for gf_bits
    if (gf_bits < 0)
      gf_bits = 0;

    if (i == 0) {
      twopass->gf_bits = gf_bits;
    }
    if (i == 1 ||
        (!rc->source_alt_ref_pending &&
         cpi->common.frame_type != KEY_FRAME)) {
      // Per frame bit target for this frame
      vp9_rc_set_frame_target(cpi, gf_bits);
    }
  }

  {
    // Adjust KF group bits and error remaining
    twopass->kf_group_error_left -= (int64_t)gf_group_err;
    twopass->kf_group_bits -= twopass->gf_group_bits;

    if (twopass->kf_group_bits < 0)
      twopass->kf_group_bits = 0;

    // If this is an arf update we want to remove the score for the
    // overlay frame at the end which will usually be very cheap to code.
    // The overlay frame has already in effect been coded so we want to spread
    // the remaining bits amoung the other frames/
    // For normal GFs remove the score for the GF itself unless this is
    // also a key frame in which case it has already been accounted for.
    if (rc->source_alt_ref_pending) {
      twopass->gf_group_error_left = (int64_t)gf_group_err - mod_frame_err;
    } else if (cpi->common.frame_type != KEY_FRAME) {
      twopass->gf_group_error_left = (int64_t)(gf_group_err
                                                   - gf_first_frame_err);
    } else {
      twopass->gf_group_error_left = (int64_t)gf_group_err;
    }

    twopass->gf_group_bits -= twopass->gf_bits;

    if (twopass->gf_group_bits < 0)
      twopass->gf_group_bits = 0;

    // This condition could fail if there are two kfs very close together
    // despite (MIN_GF_INTERVAL) and would cause a divide by 0 in the
    // calculation of alt_extra_bits.
    if (rc->baseline_gf_interval >= 3) {
      const int boost = rc->source_alt_ref_pending ? b_boost : rc->gfu_boost;

      if (boost >= 150) {
        const int pct_extra = MIN(20, (boost - 100) / 50);
        const int alt_extra_bits = (int)((twopass->gf_group_bits * pct_extra) /
                                       100);
        twopass->gf_group_bits -= alt_extra_bits;
      }
    }
  }

  if (cpi->common.frame_type != KEY_FRAME) {
    FIRSTPASS_STATS sectionstats;

    zero_stats(&sectionstats);
    reset_fpf_position(twopass, start_pos);

    for (i = 0; i < rc->baseline_gf_interval; i++) {
      input_stats(twopass, &next_frame);
      accumulate_stats(&sectionstats, &next_frame);
    }

    avg_stats(&sectionstats);

    twopass->section_intra_rating = (int)
      (sectionstats.intra_error /
      DOUBLE_DIVIDE_CHECK(sectionstats.coded_error));

    reset_fpf_position(twopass, start_pos);
  }
}

// Allocate bits to a normal frame that is neither a gf an arf or a key frame.
static void assign_std_frame_bits(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  int target_frame_size;
  double modified_err;
  double err_fraction;
  const int max_bits = frame_max_bits(cpi);  // Max for a single frame.

  // Calculate modified prediction error used in bit allocation.
  modified_err = calculate_modified_err(cpi, this_frame);

  if (cpi->twopass.gf_group_error_left > 0)
    // What portion of the remaining GF group error is used by this frame.
    err_fraction = modified_err / cpi->twopass.gf_group_error_left;
  else
    err_fraction = 0.0;

  // How many of those bits available for allocation should we give it?
  target_frame_size = (int)((double)cpi->twopass.gf_group_bits * err_fraction);

  // Clip target size to 0 - max_bits (or cpi->twopass.gf_group_bits) at
  // the top end.
  target_frame_size = clamp(target_frame_size, 0,
                            MIN(max_bits, (int)cpi->twopass.gf_group_bits));

  // Adjust error and bits remaining.
  cpi->twopass.gf_group_error_left -= (int64_t)modified_err;
  cpi->twopass.gf_group_bits -= target_frame_size;

  if (cpi->twopass.gf_group_bits < 0)
    cpi->twopass.gf_group_bits = 0;

  // Per frame bit target for this frame.
  vp9_rc_set_frame_target(cpi, target_frame_size);
}

static int test_candidate_kf(VP9_COMP *cpi,
                             const FIRSTPASS_STATS *last_frame,
                             const FIRSTPASS_STATS *this_frame,
                             const FIRSTPASS_STATS *next_frame) {
  int is_viable_kf = 0;

  // Does the frame satisfy the primary criteria of a key frame
  //      If so, then examine how well it predicts subsequent frames
  if ((this_frame->pcnt_second_ref < 0.10) &&
      (next_frame->pcnt_second_ref < 0.10) &&
      ((this_frame->pcnt_inter < 0.05) ||
       (((this_frame->pcnt_inter - this_frame->pcnt_neutral) < .35) &&
        ((this_frame->intra_error /
          DOUBLE_DIVIDE_CHECK(this_frame->coded_error)) < 2.5) &&
        ((fabs(last_frame->coded_error - this_frame->coded_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->coded_error) >
          .40) ||
         (fabs(last_frame->intra_error - this_frame->intra_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->intra_error) >
          .40) ||
         ((next_frame->intra_error /
           DOUBLE_DIVIDE_CHECK(next_frame->coded_error)) > 3.5))))) {
    int i;
    FIRSTPASS_STATS *start_pos;

    FIRSTPASS_STATS local_next_frame;

    double boost_score = 0.0;
    double old_boost_score = 0.0;
    double decay_accumulator = 1.0;

    local_next_frame = *next_frame;

    // Note the starting file position so we can reset to it
    start_pos = cpi->twopass.stats_in;

    // Examine how well the key frame predicts subsequent frames
    for (i = 0; i < 16; i++) {
      double next_iiratio = (IIKFACTOR1 * local_next_frame.intra_error /
                             DOUBLE_DIVIDE_CHECK(local_next_frame.coded_error));

      if (next_iiratio > RMAX)
        next_iiratio = RMAX;

      // Cumulative effect of decay in prediction quality
      if (local_next_frame.pcnt_inter > 0.85)
        decay_accumulator *= local_next_frame.pcnt_inter;
      else
        decay_accumulator *= (0.85 + local_next_frame.pcnt_inter) / 2.0;

      // decay_accumulator = decay_accumulator * local_next_frame.pcnt_inter;

      // Keep a running total
      boost_score += (decay_accumulator * next_iiratio);

      // Test various breakout clauses
      if ((local_next_frame.pcnt_inter < 0.05) ||
          (next_iiratio < 1.5) ||
          (((local_next_frame.pcnt_inter -
             local_next_frame.pcnt_neutral) < 0.20) &&
           (next_iiratio < 3.0)) ||
          ((boost_score - old_boost_score) < 3.0) ||
          (local_next_frame.intra_error < 200)
         ) {
        break;
      }

      old_boost_score = boost_score;

      // Get the next frame details
      if (EOF == input_stats(&cpi->twopass, &local_next_frame))
        break;
    }

    // If there is tolerable prediction for at least the next 3 frames then
    // break out else discard this potential key frame and move on
    if (boost_score > 30.0 && (i > 3)) {
      is_viable_kf = 1;
    } else {
      // Reset the file position
      reset_fpf_position(&cpi->twopass, start_pos);

      is_viable_kf = 0;
    }
  }

  return is_viable_kf;
}

static void find_next_key_frame(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  int i, j;
  FIRSTPASS_STATS last_frame;
  FIRSTPASS_STATS first_frame;
  FIRSTPASS_STATS next_frame;
  FIRSTPASS_STATS *start_position;

  double decay_accumulator = 1.0;
  double zero_motion_accumulator = 1.0;
  double boost_score = 0;
  double loop_decay_rate;

  double kf_mod_err = 0.0;
  double kf_group_err = 0.0;
  double recent_loop_decay[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

  RATE_CONTROL *const rc = &cpi->rc;
  struct twopass_rc *const twopass = &cpi->twopass;

  vp9_zero(next_frame);

  vp9_clear_system_state();  // __asm emms;

  start_position = twopass->stats_in;
  cpi->common.frame_type = KEY_FRAME;

  // is this a forced key frame by interval
  rc->this_key_frame_forced = rc->next_key_frame_forced;

  // Clear the alt ref active flag as this can never be active on a key frame
  rc->source_alt_ref_active = 0;

  // Kf is always a gf so clear frames till next gf counter
  rc->frames_till_gf_update_due = 0;

  rc->frames_to_key = 1;

  // Take a copy of the initial frame details
  first_frame = *this_frame;

  twopass->kf_group_bits = 0;        // Total bits available to kf group
  twopass->kf_group_error_left = 0;  // Group modified error score.

  kf_mod_err = calculate_modified_err(cpi, this_frame);

  // find the next keyframe
  i = 0;
  while (twopass->stats_in < twopass->stats_in_end) {
    // Accumulate kf group error
    kf_group_err += calculate_modified_err(cpi, this_frame);

    // load a the next frame's stats
    last_frame = *this_frame;
    input_stats(twopass, this_frame);

    // Provided that we are not at the end of the file...
    if (cpi->oxcf.auto_key &&
        lookup_next_frame_stats(twopass, &next_frame) != EOF) {
      // Normal scene cut check
      if (test_candidate_kf(cpi, &last_frame, this_frame, &next_frame))
        break;


      // How fast is prediction quality decaying
      loop_decay_rate = get_prediction_decay_rate(&cpi->common, &next_frame);

      // We want to know something about the recent past... rather than
      // as used elsewhere where we are concened with decay in prediction
      // quality since the last GF or KF.
      recent_loop_decay[i % 8] = loop_decay_rate;
      decay_accumulator = 1.0;
      for (j = 0; j < 8; j++)
        decay_accumulator *= recent_loop_decay[j];

      // Special check for transition or high motion followed by a
      // to a static scene.
      if (detect_transition_to_still(cpi, i, cpi->key_frame_frequency - i,
                                     loop_decay_rate, decay_accumulator))
        break;

      // Step on to the next frame
      rc->frames_to_key++;

      // If we don't have a real key frame within the next two
      // forcekeyframeevery intervals then break out of the loop.
      if (rc->frames_to_key >= 2 * (int)cpi->key_frame_frequency)
        break;
    } else {
      rc->frames_to_key++;
    }
    i++;
  }

  // If there is a max kf interval set by the user we must obey it.
  // We already breakout of the loop above at 2x max.
  // This code centers the extra kf if the actual natural
  // interval is between 1x and 2x
  if (cpi->oxcf.auto_key &&
      rc->frames_to_key > (int)cpi->key_frame_frequency) {
    FIRSTPASS_STATS tmp_frame;

    rc->frames_to_key /= 2;

    // Copy first frame details
    tmp_frame = first_frame;

    // Reset to the start of the group
    reset_fpf_position(twopass, start_position);

    kf_group_err = 0;

    // Rescan to get the correct error data for the forced kf group
    for (i = 0; i < rc->frames_to_key; i++) {
      // Accumulate kf group errors
      kf_group_err += calculate_modified_err(cpi, &tmp_frame);

      // Load the next frame's stats.
      input_stats(twopass, &tmp_frame);
    }
    rc->next_key_frame_forced = 1;
  } else if (twopass->stats_in == twopass->stats_in_end) {
    rc->next_key_frame_forced = 1;
  } else {
    rc->next_key_frame_forced = 0;
  }

  // Special case for the last key frame of the file
  if (twopass->stats_in >= twopass->stats_in_end) {
    // Accumulate kf group error
    kf_group_err += calculate_modified_err(cpi, this_frame);
  }

  // Calculate the number of bits that should be assigned to the kf group.
  if (twopass->bits_left > 0 && twopass->modified_error_left > 0.0) {
    // Max for a single normal frame (not key frame)
    int max_bits = frame_max_bits(cpi);

    // Maximum bits for the kf group
    int64_t max_grp_bits;

    // Default allocation based on bits left and relative
    // complexity of the section
    twopass->kf_group_bits = (int64_t)(twopass->bits_left *
       (kf_group_err / twopass->modified_error_left));

    // Clip based on maximum per frame rate defined by the user.
    max_grp_bits = (int64_t)max_bits * (int64_t)rc->frames_to_key;
    if (twopass->kf_group_bits > max_grp_bits)
      twopass->kf_group_bits = max_grp_bits;
  } else {
    twopass->kf_group_bits = 0;
  }
  // Reset the first pass file position
  reset_fpf_position(twopass, start_position);

  // Determine how big to make this keyframe based on how well the subsequent
  // frames use inter blocks.
  decay_accumulator = 1.0;
  boost_score = 0.0;

  // Scan through the kf group collating various stats.
  for (i = 0; i < rc->frames_to_key; i++) {
    double r;

    if (EOF == input_stats(twopass, &next_frame))
      break;

    // Monitor for static sections.
    if ((next_frame.pcnt_inter - next_frame.pcnt_motion) <
        zero_motion_accumulator) {
      zero_motion_accumulator =
        (next_frame.pcnt_inter - next_frame.pcnt_motion);
    }

    // For the first few frames collect data to decide kf boost.
    if (i <= (rc->max_gf_interval * 2)) {
      if (next_frame.intra_error > twopass->kf_intra_err_min)
        r = (IIKFACTOR2 * next_frame.intra_error /
             DOUBLE_DIVIDE_CHECK(next_frame.coded_error));
      else
        r = (IIKFACTOR2 * twopass->kf_intra_err_min /
             DOUBLE_DIVIDE_CHECK(next_frame.coded_error));

      if (r > RMAX)
        r = RMAX;

      // How fast is prediction quality decaying
      if (!detect_flash(twopass, 0)) {
        loop_decay_rate = get_prediction_decay_rate(&cpi->common, &next_frame);
        decay_accumulator *= loop_decay_rate;
        decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                              ? MIN_DECAY_FACTOR : decay_accumulator;
      }

      boost_score += (decay_accumulator * r);
    }
  }

  {
    FIRSTPASS_STATS sectionstats;

    zero_stats(&sectionstats);
    reset_fpf_position(twopass, start_position);

    for (i = 0; i < rc->frames_to_key; i++) {
      input_stats(twopass, &next_frame);
      accumulate_stats(&sectionstats, &next_frame);
    }

    avg_stats(&sectionstats);

    twopass->section_intra_rating = (int) (sectionstats.intra_error /
        DOUBLE_DIVIDE_CHECK(sectionstats.coded_error));
  }

  // Reset the first pass file position
  reset_fpf_position(twopass, start_position);

  // Work out how many bits to allocate for the key frame itself
  if (1) {
    int kf_boost = (int)boost_score;
    int allocation_chunks;
    int alt_kf_bits;

    if (kf_boost < (rc->frames_to_key * 3))
      kf_boost = (rc->frames_to_key * 3);

    if (kf_boost < MIN_KF_BOOST)
      kf_boost = MIN_KF_BOOST;

    // Make a note of baseline boost and the zero motion
    // accumulator value for use elsewhere.
    rc->kf_boost = kf_boost;
    twopass->kf_zeromotion_pct = (int)(zero_motion_accumulator * 100.0);

    // We do three calculations for kf size.
    // The first is based on the error score for the whole kf group.
    // The second (optionally) on the key frames own error if this is
    // smaller than the average for the group.
    // The final one insures that the frame receives at least the
    // allocation it would have received based on its own error score vs
    // the error score remaining
    // Special case if the sequence appears almost totaly static
    // In this case we want to spend almost all of the bits on the
    // key frame.
    // cpi->rc.frames_to_key-1 because key frame itself is taken
    // care of by kf_boost.
    if (zero_motion_accumulator >= 0.99) {
      allocation_chunks = ((rc->frames_to_key - 1) * 10) + kf_boost;
    } else {
      allocation_chunks = ((rc->frames_to_key - 1) * 100) + kf_boost;
    }

    // Prevent overflow
    if (kf_boost > 1028) {
      int divisor = kf_boost >> 10;
      kf_boost /= divisor;
      allocation_chunks /= divisor;
    }

    twopass->kf_group_bits = (twopass->kf_group_bits < 0) ? 0
           : twopass->kf_group_bits;

    // Calculate the number of bits to be spent on the key frame
    twopass->kf_bits = (int)((double)kf_boost *
        ((double)twopass->kf_group_bits / allocation_chunks));

    // If the key frame is actually easier than the average for the
    // kf group (which does sometimes happen, e.g. a blank intro frame)
    // then use an alternate calculation based on the kf error score
    // which should give a smaller key frame.
    if (kf_mod_err < kf_group_err / rc->frames_to_key) {
      double  alt_kf_grp_bits = ((double)twopass->bits_left *
         (kf_mod_err * (double)rc->frames_to_key) /
         DOUBLE_DIVIDE_CHECK(twopass->modified_error_left));

      alt_kf_bits = (int)((double)kf_boost *
                          (alt_kf_grp_bits / (double)allocation_chunks));

      if (twopass->kf_bits > alt_kf_bits)
        twopass->kf_bits = alt_kf_bits;
    } else {
    // Else if it is much harder than other frames in the group make sure
    // it at least receives an allocation in keeping with its relative
    // error score
      alt_kf_bits = (int)((double)twopass->bits_left * (kf_mod_err /
               DOUBLE_DIVIDE_CHECK(twopass->modified_error_left)));

      if (alt_kf_bits > twopass->kf_bits) {
        twopass->kf_bits = alt_kf_bits;
      }
    }
    twopass->kf_group_bits -= twopass->kf_bits;
    // Per frame bit target for this frame.
    vp9_rc_set_frame_target(cpi, twopass->kf_bits);
  }

  // Note the total error score of the kf group minus the key frame itself
  twopass->kf_group_error_left = (int)(kf_group_err - kf_mod_err);

  // Adjust the count of total modified error left.
  // The count of bits left is adjusted elsewhere based on real coded frame
  // sizes.
  twopass->modified_error_left -= kf_group_err;
}

void vp9_rc_get_first_pass_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (!cpi->refresh_alt_ref_frame &&
      (cm->current_video_frame == 0 ||
       cm->frame_flags & FRAMEFLAGS_KEY)) {
    cm->frame_type = KEY_FRAME;
  } else {
    cm->frame_type = INTER_FRAME;
  }
  // Do not use periodic key frames
  cpi->rc.frames_to_key = INT_MAX;
}

void vp9_rc_get_second_pass_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  struct twopass_rc *const twopass = &cpi->twopass;
  const int frames_left = (int)(twopass->total_stats.count -
                              cm->current_video_frame);
  FIRSTPASS_STATS this_frame;
  FIRSTPASS_STATS this_frame_copy;

  double this_frame_intra_error;
  double this_frame_coded_error;
  int target;

  if (!twopass->stats_in)
    return;

  if (cpi->refresh_alt_ref_frame) {
    cm->frame_type = INTER_FRAME;
    vp9_rc_set_frame_target(cpi, twopass->gf_bits);
    return;
  }

  vp9_clear_system_state();

  if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
    twopass->active_worst_quality = cpi->oxcf.cq_level;
  } else if (cm->current_video_frame == 0) {
    // Special case code for first frame.
    const int section_target_bandwidth = (int)(twopass->bits_left /
                                               frames_left);
    const int tmp_q = vp9_twopass_worst_quality(cpi, &twopass->total_left_stats,
                                                section_target_bandwidth);
    twopass->active_worst_quality = tmp_q;
    rc->ni_av_qi = tmp_q;
    rc->avg_q = vp9_convert_qindex_to_q(tmp_q);

    // Limit the maxq value returned subsequently.
    // This increases the risk of overspend or underspend if the initial
    // estimate for the clip is bad, but helps prevent excessive
    // variation in Q, especially near the end of a clip
    // where for example a small overspend may cause Q to crash
    // adjust_maxq_qrange(cpi);
  }
  vp9_zero(this_frame);
  if (EOF == input_stats(twopass, &this_frame))
    return;

  this_frame_intra_error = this_frame.intra_error;
  this_frame_coded_error = this_frame.coded_error;

  // keyframe and section processing !
  if (rc->frames_to_key == 0 ||
      (cm->frame_flags & FRAMEFLAGS_KEY)) {
    // Define next KF group and assign bits to it
    this_frame_copy = this_frame;
    find_next_key_frame(cpi, &this_frame_copy);
  } else {
    cm->frame_type = INTER_FRAME;
  }

  // Is this a GF / ARF (Note that a KF is always also a GF)
  if (rc->frames_till_gf_update_due == 0) {
    // Define next gf group and assign bits to it
    this_frame_copy = this_frame;

#if CONFIG_MULTIPLE_ARF
    if (cpi->multi_arf_enabled) {
      define_fixed_arf_period(cpi);
    } else {
#endif
      define_gf_group(cpi, &this_frame_copy);
#if CONFIG_MULTIPLE_ARF
    }
#endif

    if (twopass->gf_zeromotion_pct > 995) {
      // As long as max_thresh for encode breakout is small enough, it is ok
      // to enable it for show frame, i.e. set allow_encode_breakout to 2.
      if (!cm->show_frame)
        cpi->allow_encode_breakout = ENCODE_BREAKOUT_DISABLED;
      else
        cpi->allow_encode_breakout = ENCODE_BREAKOUT_LIMITED;
    }

    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    cpi->refresh_golden_frame = 1;
  } else {
    // Otherwise this is an ordinary frame
    // Assign bits from those allocated to the GF group
    this_frame_copy =  this_frame;
    assign_std_frame_bits(cpi, &this_frame_copy);
  }

  // Keep a globally available copy of this and the next frame's iiratio.
  twopass->this_iiratio = (int)(this_frame_intra_error /
                              DOUBLE_DIVIDE_CHECK(this_frame_coded_error));
  {
    FIRSTPASS_STATS next_frame;
    if (lookup_next_frame_stats(twopass, &next_frame) != EOF) {
      twopass->next_iiratio = (int)(next_frame.intra_error /
                                 DOUBLE_DIVIDE_CHECK(next_frame.coded_error));
    }
  }

  if (cpi->common.frame_type == KEY_FRAME)
    target = vp9_rc_clamp_iframe_target_size(cpi, rc->this_frame_target);
  else
    target = vp9_rc_clamp_pframe_target_size(cpi, rc->this_frame_target);
  vp9_rc_set_frame_target(cpi, target);

  // Update the total stats remaining structure
  subtract_stats(&twopass->total_left_stats, &this_frame);
}

void vp9_twopass_postencode_update(VP9_COMP *cpi, uint64_t bytes_used) {
#ifdef DISABLE_RC_LONG_TERM_MEM
  cpi->twopass.bits_left -=  cpi->rc.this_frame_target;
#else
  cpi->twopass.bits_left -= 8 * bytes_used;
  // Update bits left to the kf and gf groups to account for overshoot or
  // undershoot on these frames
  if (cm->frame_type == KEY_FRAME) {
    cpi->twopass.kf_group_bits += cpi->rc.this_frame_target -
        cpi->rc.projected_frame_size;

    cpi->twopass.kf_group_bits = MAX(cpi->twopass.kf_group_bits, 0);
  } else if (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame) {
    cpi->twopass.gf_group_bits += cpi->rc.this_frame_target -
        cpi->rc.projected_frame_size;

    cpi->twopass.gf_group_bits = MAX(cpi->twopass.gf_group_bits, 0);
  }
#endif
}
