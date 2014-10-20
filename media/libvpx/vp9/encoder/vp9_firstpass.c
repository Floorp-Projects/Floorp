/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "./vpx_scale_rtcd.h"

#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/vpx_scale.h"
#include "vpx_scale/yv12config.h"

#include "vp9/common/vp9_entropymv.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_reconinter.h"  // vp9_setup_dst_planes()
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/encoder/vp9_aq_variance.h"
#include "vp9/encoder/vp9_block.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_extend.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mcomp.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_rd.h"
#include "vp9/encoder/vp9_variance.h"

#define OUTPUT_FPF 0

#define IIFACTOR   12.5
#define IIKFACTOR1 12.5
#define IIKFACTOR2 15.0
#define RMAX       512.0
#define GF_RMAX    96.0
#define ERR_DIVISOR   150.0
#define MIN_DECAY_FACTOR 0.1
#define SVC_FACTOR_PT_LOW 0.45
#define FACTOR_PT_LOW 0.5
#define FACTOR_PT_HIGH 0.9

#define KF_MB_INTRA_MIN 150
#define GF_MB_INTRA_MIN 100

#define DOUBLE_DIVIDE_CHECK(x) ((x) < 0 ? (x) - 0.000001 : (x) + 0.000001)

#define MIN_KF_BOOST        300
#define MIN_GF_INTERVAL     4

static void swap_yv12(YV12_BUFFER_CONFIG *a, YV12_BUFFER_CONFIG *b) {
  YV12_BUFFER_CONFIG temp = *a;
  *a = *b;
  *b = temp;
}

static int gfboost_qadjust(int qindex) {
  const double q = vp9_convert_qindex_to_q(qindex);
  return (int)((0.00000828 * q * q * q) +
               (-0.0055 * q * q) +
               (1.32 * q) + 79.3);
}

// Resets the first pass file to the given position using a relative seek from
// the current position.
static void reset_fpf_position(TWO_PASS *p,
                               const FIRSTPASS_STATS *position) {
  p->stats_in = position;
}

// Read frame stats at an offset from the current position.
static const FIRSTPASS_STATS *read_frame_stats(const TWO_PASS *p, int offset) {
  if ((offset >= 0 && p->stats_in + offset >= p->stats_in_end) ||
      (offset < 0 && p->stats_in + offset < p->stats_in_start)) {
    return NULL;
  }

  return &p->stats_in[offset];
}

static int input_stats(TWO_PASS *p, FIRSTPASS_STATS *fps) {
  if (p->stats_in >= p->stats_in_end)
    return EOF;

  *fps = *p->stats_in;
  ++p->stats_in;
  return 1;
}

static void output_stats(FIRSTPASS_STATS *stats,
                         struct vpx_codec_pkt_list *pktlist) {
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

    fprintf(fpfile, "%12.0f %12.0f %12.0f %12.0f %12.4f %12.4f"
            "%12.4f %12.4f %12.4f %12.4f %12.4f %12.4f %12.4f"
            "%12.0f %12.0f %12.4f %12.0f %12.0f %12.4f\n",
            stats->frame,
            stats->intra_error,
            stats->coded_error,
            stats->sr_coded_error,
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

#if CONFIG_FP_MB_STATS
static void output_fpmb_stats(uint8_t *this_frame_mb_stats, VP9_COMMON *cm,
                         struct vpx_codec_pkt_list *pktlist) {
  struct vpx_codec_cx_pkt pkt;
  pkt.kind = VPX_CODEC_FPMB_STATS_PKT;
  pkt.data.firstpass_mb_stats.buf = this_frame_mb_stats;
  pkt.data.firstpass_mb_stats.sz = cm->MBs * sizeof(uint8_t);
  vpx_codec_pkt_list_add(pktlist, &pkt);
}
#endif

static void zero_stats(FIRSTPASS_STATS *section) {
  section->frame      = 0.0;
  section->intra_error = 0.0;
  section->coded_error = 0.0;
  section->sr_coded_error = 0.0;
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
  section->spatial_layer_id = 0;
}

static void accumulate_stats(FIRSTPASS_STATS *section,
                             const FIRSTPASS_STATS *frame) {
  section->frame += frame->frame;
  section->spatial_layer_id = frame->spatial_layer_id;
  section->intra_error += frame->intra_error;
  section->coded_error += frame->coded_error;
  section->sr_coded_error += frame->sr_coded_error;
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

static void subtract_stats(FIRSTPASS_STATS *section,
                           const FIRSTPASS_STATS *frame) {
  section->frame -= frame->frame;
  section->intra_error -= frame->intra_error;
  section->coded_error -= frame->coded_error;
  section->sr_coded_error -= frame->sr_coded_error;
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


// Calculate a modified Error used in distributing bits between easier and
// harder frames.
static double calculate_modified_err(const TWO_PASS *twopass,
                                     const VP9EncoderConfig *oxcf,
                                     const FIRSTPASS_STATS *this_frame) {
  const FIRSTPASS_STATS *const stats = &twopass->total_stats;
  const double av_err = stats->coded_error / stats->count;
  const double modified_error = av_err *
      pow(this_frame->coded_error / DOUBLE_DIVIDE_CHECK(av_err),
          oxcf->two_pass_vbrbias / 100.0);
  return fclamp(modified_error,
                twopass->modified_error_min, twopass->modified_error_max);
}

// This function returns the maximum target rate per frame.
static int frame_max_bits(const RATE_CONTROL *rc,
                          const VP9EncoderConfig *oxcf) {
  int64_t max_bits = ((int64_t)rc->avg_frame_bandwidth *
                          (int64_t)oxcf->two_pass_vbrmax_section) / 100;
  if (max_bits < 0)
    max_bits = 0;
  else if (max_bits > rc->max_frame_bandwidth)
    max_bits = rc->max_frame_bandwidth;

  return (int)max_bits;
}

void vp9_init_first_pass(VP9_COMP *cpi) {
  zero_stats(&cpi->twopass.total_stats);
}

void vp9_end_first_pass(VP9_COMP *cpi) {
  if (is_two_pass_svc(cpi)) {
    int i;
    for (i = 0; i < cpi->svc.number_spatial_layers; ++i) {
      output_stats(&cpi->svc.layer_context[i].twopass.total_stats,
                   cpi->output_pkt_list);
    }
  } else {
    output_stats(&cpi->twopass.total_stats, cpi->output_pkt_list);
  }
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

static unsigned int get_prediction_error(BLOCK_SIZE bsize,
                                         const struct buf_2d *src,
                                         const struct buf_2d *ref) {
  unsigned int sse;
  const vp9_variance_fn_t fn = get_block_variance_fn(bsize);
  fn(src->buf, src->stride, ref->buf, ref->stride, &sse);
  return sse;
}

// Refine the motion search range according to the frame dimension
// for first pass test.
static int get_search_range(const VP9_COMMON *cm) {
  int sr = 0;
  const int dim = MIN(cm->width, cm->height);

  while ((dim << sr) < MAX_FULL_PEL_VAL)
    ++sr;
  return sr;
}

static void first_pass_motion_search(VP9_COMP *cpi, MACROBLOCK *x,
                                     const MV *ref_mv, MV *best_mv,
                                     int *best_motion_err) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MV tmp_mv = {0, 0};
  MV ref_mv_full = {ref_mv->row >> 3, ref_mv->col >> 3};
  int num00, tmp_err, n;
  const BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  vp9_variance_fn_ptr_t v_fn_ptr = cpi->fn_ptr[bsize];
  const int new_mv_mode_penalty = 256;

  int step_param = 3;
  int further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;
  const int sr = get_search_range(&cpi->common);
  step_param += sr;
  further_steps -= sr;

  // Override the default variance function to use MSE.
  v_fn_ptr.vf = get_block_variance_fn(bsize);

  // Center the initial step/diamond search on best mv.
  tmp_err = cpi->diamond_search_sad(x, &cpi->ss_cfg, &ref_mv_full, &tmp_mv,
                                    step_param,
                                    x->sadperbit16, &num00, &v_fn_ptr, ref_mv);
  if (tmp_err < INT_MAX)
    tmp_err = vp9_get_mvpred_var(x, &tmp_mv, ref_mv, &v_fn_ptr, 1);
  if (tmp_err < INT_MAX - new_mv_mode_penalty)
    tmp_err += new_mv_mode_penalty;

  if (tmp_err < *best_motion_err) {
    *best_motion_err = tmp_err;
    *best_mv = tmp_mv;
  }

  // Carry out further step/diamond searches as necessary.
  n = num00;
  num00 = 0;

  while (n < further_steps) {
    ++n;

    if (num00) {
      --num00;
    } else {
      tmp_err = cpi->diamond_search_sad(x, &cpi->ss_cfg, &ref_mv_full, &tmp_mv,
                                        step_param + n, x->sadperbit16,
                                        &num00, &v_fn_ptr, ref_mv);
      if (tmp_err < INT_MAX)
        tmp_err = vp9_get_mvpred_var(x, &tmp_mv, ref_mv, &v_fn_ptr, 1);
      if (tmp_err < INT_MAX - new_mv_mode_penalty)
        tmp_err += new_mv_mode_penalty;

      if (tmp_err < *best_motion_err) {
        *best_motion_err = tmp_err;
        *best_mv = tmp_mv;
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

static int find_fp_qindex() {
  int i;

  for (i = 0; i < QINDEX_RANGE; ++i)
    if (vp9_convert_qindex_to_q(i) >= 30.0)
      break;

  if (i == QINDEX_RANGE)
    i--;

  return i;
}

static void set_first_pass_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  if (!cpi->refresh_alt_ref_frame &&
      (cm->current_video_frame == 0 ||
       (cpi->frame_flags & FRAMEFLAGS_KEY))) {
    cm->frame_type = KEY_FRAME;
  } else {
    cm->frame_type = INTER_FRAME;
  }
  // Do not use periodic key frames.
  cpi->rc.frames_to_key = INT_MAX;
}

void vp9_first_pass(VP9_COMP *cpi, const struct lookahead_entry *source) {
  int mb_row, mb_col;
  MACROBLOCK *const x = &cpi->mb;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  TileInfo tile;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const PICK_MODE_CONTEXT *ctx = &cpi->pc_root->none;
  int i;

  int recon_yoffset, recon_uvoffset;
  YV12_BUFFER_CONFIG *const lst_yv12 = get_ref_frame_buffer(cpi, LAST_FRAME);
  YV12_BUFFER_CONFIG *gld_yv12 = get_ref_frame_buffer(cpi, GOLDEN_FRAME);
  YV12_BUFFER_CONFIG *const new_yv12 = get_frame_new_buffer(cm);
  int recon_y_stride = lst_yv12->y_stride;
  int recon_uv_stride = lst_yv12->uv_stride;
  int uv_mb_height = 16 >> (lst_yv12->y_height > lst_yv12->uv_height);
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
  MV lastmv = {0, 0};
  TWO_PASS *twopass = &cpi->twopass;
  const MV zero_mv = {0, 0};
  const YV12_BUFFER_CONFIG *first_ref_buf = lst_yv12;
  LAYER_CONTEXT *const lc = is_two_pass_svc(cpi) ?
        &cpi->svc.layer_context[cpi->svc.spatial_layer_id] : NULL;

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    vp9_zero_array(cpi->twopass.frame_mb_stats_buf, cm->MBs);
  }
#endif

  vp9_clear_system_state();

  set_first_pass_params(cpi);
  vp9_set_quantizer(cm, find_fp_qindex());

  if (lc != NULL) {
    MV_REFERENCE_FRAME ref_frame = LAST_FRAME;
    twopass = &lc->twopass;

    if (cpi->common.current_video_frame == 0) {
      cpi->ref_frame_flags = 0;
    } else {
    if (lc->current_video_frame_in_layer <
        (unsigned int)cpi->svc.number_temporal_layers)
        cpi->ref_frame_flags = VP9_GOLD_FLAG;
      else
        cpi->ref_frame_flags = VP9_LAST_FLAG | VP9_GOLD_FLAG;
    }

    vp9_scale_references(cpi);

    // Use either last frame or alt frame for motion search.
    if (cpi->ref_frame_flags & VP9_LAST_FLAG) {
      first_ref_buf = vp9_get_scaled_ref_frame(cpi, LAST_FRAME);
      ref_frame = LAST_FRAME;
      if (first_ref_buf == NULL)
        first_ref_buf = get_ref_frame_buffer(cpi, LAST_FRAME);
    } else if (cpi->ref_frame_flags & VP9_GOLD_FLAG) {
      first_ref_buf = vp9_get_scaled_ref_frame(cpi, GOLDEN_FRAME);
      ref_frame = GOLDEN_FRAME;
      if (first_ref_buf == NULL)
        first_ref_buf = get_ref_frame_buffer(cpi, GOLDEN_FRAME);
    }

    recon_y_stride = new_yv12->y_stride;
    recon_uv_stride = new_yv12->uv_stride;
    uv_mb_height = 16 >> (new_yv12->y_height > new_yv12->uv_height);

    // Disable golden frame for svc first pass for now.
    gld_yv12 = NULL;
    set_ref_ptrs(cm, xd, ref_frame, NONE);

    cpi->Source = vp9_scale_if_required(cm, cpi->un_scaled_source,
                                        &cpi->scaled_source);
  }

  vp9_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y);

  vp9_setup_src_planes(x, cpi->Source, 0, 0);
  vp9_setup_pre_planes(xd, 0, first_ref_buf, 0, 0, NULL);
  vp9_setup_dst_planes(xd->plane, new_yv12, 0, 0);

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

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

  // Tiling is ignored in the first pass.
  vp9_tile_init(&tile, cm, 0, 0);

  for (mb_row = 0; mb_row < cm->mb_rows; ++mb_row) {
    MV best_ref_mv = {0, 0};

    // Reset above block coeffs.
    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
    recon_uvoffset = (mb_row * recon_uv_stride * uv_mb_height);

    // Set up limit values for motion vectors to prevent them extending
    // outside the UMV borders.
    x->mv_row_min = -((mb_row * 16) + BORDER_MV_PIXELS_B16);
    x->mv_row_max = ((cm->mb_rows - 1 - mb_row) * 16)
                    + BORDER_MV_PIXELS_B16;

    for (mb_col = 0; mb_col < cm->mb_cols; ++mb_col) {
      int this_error;
      const int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);
      double error_weight = 1.0;
      const BLOCK_SIZE bsize = get_bsize(cm, mb_row, mb_col);
#if CONFIG_FP_MB_STATS
      const int mb_index = mb_row * cm->mb_cols + mb_col;
#endif

      vp9_clear_system_state();

      xd->plane[0].dst.buf = new_yv12->y_buffer + recon_yoffset;
      xd->plane[1].dst.buf = new_yv12->u_buffer + recon_uvoffset;
      xd->plane[2].dst.buf = new_yv12->v_buffer + recon_uvoffset;
      xd->left_available = (mb_col != 0);
      xd->mi[0]->mbmi.sb_type = bsize;
      xd->mi[0]->mbmi.ref_frame[0] = INTRA_FRAME;
      set_mi_row_col(xd, &tile,
                     mb_row << 1, num_8x8_blocks_high_lookup[bsize],
                     mb_col << 1, num_8x8_blocks_wide_lookup[bsize],
                     cm->mi_rows, cm->mi_cols);

      if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
        const int energy = vp9_block_energy(cpi, x, bsize);
        error_weight = vp9_vaq_inv_q_ratio(energy);
      }

      // Do intra 16x16 prediction.
      x->skip_encode = 0;
      xd->mi[0]->mbmi.mode = DC_PRED;
      xd->mi[0]->mbmi.tx_size = use_dc_pred ?
         (bsize >= BLOCK_16X16 ? TX_16X16 : TX_8X8) : TX_4X4;
      vp9_encode_intra_block_plane(x, bsize, 0);
      this_error = vp9_get_mb_ss(x->plane[0].src_diff);

      if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
        vp9_clear_system_state();
        this_error = (int)(this_error * error_weight);
      }

      // Intrapenalty below deals with situations where the intra and inter
      // error scores are very low (e.g. a plain black frame).
      // We do not have special cases in first pass for 0,0 and nearest etc so
      // all inter modes carry an overhead cost estimate for the mv.
      // When the error score is very low this causes us to pick all or lots of
      // INTRA modes and throw lots of key frames.
      // This penalty adds a cost matching that of a 0,0 mv to the intra case.
      this_error += intrapenalty;

      // Accumulate the intra error.
      intra_error += (int64_t)this_error;

#if CONFIG_FP_MB_STATS
      if (cpi->use_fp_mb_stats) {
        // initialization
        cpi->twopass.frame_mb_stats_buf[mb_index] = 0;
      }
#endif

      // Set up limit values for motion vectors to prevent them extending
      // outside the UMV borders.
      x->mv_col_min = -((mb_col * 16) + BORDER_MV_PIXELS_B16);
      x->mv_col_max = ((cm->mb_cols - 1 - mb_col) * 16) + BORDER_MV_PIXELS_B16;

      // Other than for the first frame do a motion search.
      if (cm->current_video_frame > 0) {
        int tmp_err, motion_error, raw_motion_error;
        // Assume 0,0 motion with no mv overhead.
        MV mv = {0, 0} , tmp_mv = {0, 0};
        struct buf_2d unscaled_last_source_buf_2d;

        xd->plane[0].pre[0].buf = first_ref_buf->y_buffer + recon_yoffset;
        motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                            &xd->plane[0].pre[0]);

        // Compute the motion error of the 0,0 motion using the last source
        // frame as the reference. Skip the further motion search on
        // reconstructed frame if this error is small.
        unscaled_last_source_buf_2d.buf =
            cpi->unscaled_last_source->y_buffer + recon_yoffset;
        unscaled_last_source_buf_2d.stride =
            cpi->unscaled_last_source->y_stride;
        raw_motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                                &unscaled_last_source_buf_2d);

        // TODO(pengchong): Replace the hard-coded threshold
        if (raw_motion_error > 25 || lc != NULL) {
          // Test last reference frame using the previous best mv as the
          // starting point (best reference) for the search.
          first_pass_motion_search(cpi, x, &best_ref_mv, &mv, &motion_error);
          if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
            vp9_clear_system_state();
            motion_error = (int)(motion_error * error_weight);
          }

          // If the current best reference mv is not centered on 0,0 then do a
          // 0,0 based search as well.
          if (!is_zero_mv(&best_ref_mv)) {
            tmp_err = INT_MAX;
            first_pass_motion_search(cpi, x, &zero_mv, &tmp_mv, &tmp_err);
            if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
              vp9_clear_system_state();
              tmp_err = (int)(tmp_err * error_weight);
            }

            if (tmp_err < motion_error) {
              motion_error = tmp_err;
              mv = tmp_mv;
            }
          }

          // Search in an older reference frame.
          if (cm->current_video_frame > 1 && gld_yv12 != NULL) {
            // Assume 0,0 motion with no mv overhead.
            int gf_motion_error;

            xd->plane[0].pre[0].buf = gld_yv12->y_buffer + recon_yoffset;
            gf_motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                                   &xd->plane[0].pre[0]);

            first_pass_motion_search(cpi, x, &zero_mv, &tmp_mv,
                                     &gf_motion_error);
            if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
              vp9_clear_system_state();
              gf_motion_error = (int)(gf_motion_error * error_weight);
            }

            if (gf_motion_error < motion_error && gf_motion_error < this_error)
              ++second_ref_count;

            // Reset to last frame as reference buffer.
            xd->plane[0].pre[0].buf = first_ref_buf->y_buffer + recon_yoffset;
            xd->plane[1].pre[0].buf = first_ref_buf->u_buffer + recon_uvoffset;
            xd->plane[2].pre[0].buf = first_ref_buf->v_buffer + recon_uvoffset;

            // In accumulating a score for the older reference frame take the
            // best of the motion predicted score and the intra coded error
            // (just as will be done for) accumulation of "coded_error" for
            // the last frame.
            if (gf_motion_error < this_error)
              sr_coded_error += gf_motion_error;
            else
              sr_coded_error += this_error;
          } else {
            sr_coded_error += motion_error;
          }
        } else {
          sr_coded_error += motion_error;
        }

        // Start by assuming that intra mode is best.
        best_ref_mv.row = 0;
        best_ref_mv.col = 0;

#if CONFIG_FP_MB_STATS
        if (cpi->use_fp_mb_stats) {
          // intra predication statistics
          cpi->twopass.frame_mb_stats_buf[mb_index] = 0;
          cpi->twopass.frame_mb_stats_buf[mb_index] |= FPMB_DCINTRA_MASK;
          cpi->twopass.frame_mb_stats_buf[mb_index] |= FPMB_MOTION_ZERO_MASK;
          if (this_error > FPMB_ERROR_LARGE_TH) {
            cpi->twopass.frame_mb_stats_buf[mb_index] |= FPMB_ERROR_LARGE_MASK;
          } else if (this_error < FPMB_ERROR_SMALL_TH) {
            cpi->twopass.frame_mb_stats_buf[mb_index] |= FPMB_ERROR_SMALL_MASK;
          }
        }
#endif

        if (motion_error <= this_error) {
          // Keep a count of cases where the inter and intra were very close
          // and very low. This helps with scene cut detection for example in
          // cropped clips with black bars at the sides or top and bottom.
          if (((this_error - intrapenalty) * 9 <= motion_error * 10) &&
              this_error < 2 * intrapenalty)
            ++neutral_count;

          mv.row *= 8;
          mv.col *= 8;
          this_error = motion_error;
          xd->mi[0]->mbmi.mode = NEWMV;
          xd->mi[0]->mbmi.mv[0].as_mv = mv;
          xd->mi[0]->mbmi.tx_size = TX_4X4;
          xd->mi[0]->mbmi.ref_frame[0] = LAST_FRAME;
          xd->mi[0]->mbmi.ref_frame[1] = NONE;
          vp9_build_inter_predictors_sby(xd, mb_row << 1, mb_col << 1, bsize);
          vp9_encode_sby_pass1(x, bsize);
          sum_mvr += mv.row;
          sum_mvr_abs += abs(mv.row);
          sum_mvc += mv.col;
          sum_mvc_abs += abs(mv.col);
          sum_mvrs += mv.row * mv.row;
          sum_mvcs += mv.col * mv.col;
          ++intercount;

          best_ref_mv = mv;

#if CONFIG_FP_MB_STATS
          if (cpi->use_fp_mb_stats) {
            // inter predication statistics
            cpi->twopass.frame_mb_stats_buf[mb_index] = 0;
            cpi->twopass.frame_mb_stats_buf[mb_index] &= ~FPMB_DCINTRA_MASK;
            cpi->twopass.frame_mb_stats_buf[mb_index] |= FPMB_MOTION_ZERO_MASK;
            if (this_error > FPMB_ERROR_LARGE_TH) {
              cpi->twopass.frame_mb_stats_buf[mb_index] |=
                  FPMB_ERROR_LARGE_MASK;
            } else if (this_error < FPMB_ERROR_SMALL_TH) {
              cpi->twopass.frame_mb_stats_buf[mb_index] |=
                  FPMB_ERROR_SMALL_MASK;
            }
          }
#endif

          if (!is_zero_mv(&mv)) {
            ++mvcount;

#if CONFIG_FP_MB_STATS
            if (cpi->use_fp_mb_stats) {
              cpi->twopass.frame_mb_stats_buf[mb_index] &=
                  ~FPMB_MOTION_ZERO_MASK;
              // check estimated motion direction
              if (mv.as_mv.col > 0 && mv.as_mv.col >= abs(mv.as_mv.row)) {
                // right direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_RIGHT_MASK;
              } else if (mv.as_mv.row < 0 &&
                         abs(mv.as_mv.row) >= abs(mv.as_mv.col)) {
                // up direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_UP_MASK;
              } else if (mv.as_mv.col < 0 &&
                         abs(mv.as_mv.col) >= abs(mv.as_mv.row)) {
                // left direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_LEFT_MASK;
              } else {
                // down direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_DOWN_MASK;
              }
            }
#endif

            // Non-zero vector, was it different from the last non zero vector?
            if (!is_equal_mv(&mv, &lastmv))
              ++new_mv_count;
            lastmv = mv;

            // Does the row vector point inwards or outwards?
            if (mb_row < cm->mb_rows / 2) {
              if (mv.row > 0)
                --sum_in_vectors;
              else if (mv.row < 0)
                ++sum_in_vectors;
            } else if (mb_row > cm->mb_rows / 2) {
              if (mv.row > 0)
                ++sum_in_vectors;
              else if (mv.row < 0)
                --sum_in_vectors;
            }

            // Does the col vector point inwards or outwards?
            if (mb_col < cm->mb_cols / 2) {
              if (mv.col > 0)
                --sum_in_vectors;
              else if (mv.col < 0)
                ++sum_in_vectors;
            } else if (mb_col > cm->mb_cols / 2) {
              if (mv.col > 0)
                ++sum_in_vectors;
              else if (mv.col < 0)
                --sum_in_vectors;
            }
          }
        }
      } else {
        sr_coded_error += (int64_t)this_error;
      }
      coded_error += (int64_t)this_error;

      // Adjust to the next column of MBs.
      x->plane[0].src.buf += 16;
      x->plane[1].src.buf += uv_mb_height;
      x->plane[2].src.buf += uv_mb_height;

      recon_yoffset += 16;
      recon_uvoffset += uv_mb_height;
    }

    // Adjust to the next row of MBs.
    x->plane[0].src.buf += 16 * x->plane[0].src.stride - 16 * cm->mb_cols;
    x->plane[1].src.buf += uv_mb_height * x->plane[1].src.stride -
                           uv_mb_height * cm->mb_cols;
    x->plane[2].src.buf += uv_mb_height * x->plane[1].src.stride -
                           uv_mb_height * cm->mb_cols;

    vp9_clear_system_state();
  }

  vp9_clear_system_state();
  {
    FIRSTPASS_STATS fps;

    fps.frame = cm->current_video_frame;
    fps.spatial_layer_id = cpi->svc.spatial_layer_id;
    fps.intra_error = (double)(intra_error >> 8);
    fps.coded_error = (double)(coded_error >> 8);
    fps.sr_coded_error = (double)(sr_coded_error >> 8);
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
    fps.duration = (double)(source->ts_end - source->ts_start);

    // Don't want to do output stats with a stack variable!
    twopass->this_frame_stats = fps;
    output_stats(&twopass->this_frame_stats, cpi->output_pkt_list);
    accumulate_stats(&twopass->total_stats, &fps);

#if CONFIG_FP_MB_STATS
    if (cpi->use_fp_mb_stats) {
      output_fpmb_stats(twopass->frame_mb_stats_buf, cm, cpi->output_pkt_list);
    }
#endif
  }

  // Copy the previous Last Frame back into gf and and arf buffers if
  // the prediction is good enough... but also don't allow it to lag too far.
  if ((twopass->sr_update_lag > 3) ||
      ((cm->current_video_frame > 0) &&
       (twopass->this_frame_stats.pcnt_inter > 0.20) &&
       ((twopass->this_frame_stats.intra_error /
         DOUBLE_DIVIDE_CHECK(twopass->this_frame_stats.coded_error)) > 2.0))) {
    if (gld_yv12 != NULL) {
      vp8_yv12_copy_frame(lst_yv12, gld_yv12);
    }
    twopass->sr_update_lag = 1;
  } else {
    ++twopass->sr_update_lag;
  }

  vp9_extend_frame_borders(new_yv12);

  if (lc != NULL) {
    vp9_update_reference_frames(cpi);
  } else {
    // Swap frame pointers so last frame refers to the frame we just compressed.
    swap_yv12(lst_yv12, new_yv12);
  }

  // Special case for the first frame. Copy into the GF buffer as a second
  // reference.
  if (cm->current_video_frame == 0 && gld_yv12 != NULL) {
    vp8_yv12_copy_frame(lst_yv12, gld_yv12);
  }

  // Use this to see what the first pass reconstruction looks like.
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

  ++cm->current_video_frame;
  if (cpi->use_svc)
    vp9_inc_frame_in_layer(cpi);
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

  // Calculate correction factor.
  if (power_term < 1.0)
    assert(error_term >= 0.0);

  return fclamp(pow(error_term, power_term), 0.05, 5.0);
}

static int get_twopass_worst_quality(const VP9_COMP *cpi,
                                     const FIRSTPASS_STATS *stats,
                                     int section_target_bandwidth) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;

  if (section_target_bandwidth <= 0) {
    return rc->worst_quality;  // Highest value allowed
  } else {
    const int num_mbs = cpi->common.MBs;
    const double section_err = stats->coded_error / stats->count;
    const double err_per_mb = section_err / num_mbs;
    const double speed_term = 1.0 + 0.04 * oxcf->speed;
    const int target_norm_bits_per_mb = ((uint64_t)section_target_bandwidth <<
                                         BPER_MB_NORMBITS) / num_mbs;
    int q;
    int is_svc_upper_layer = 0;
    if (is_two_pass_svc(cpi) && cpi->svc.spatial_layer_id > 0)
      is_svc_upper_layer = 1;

    // Try and pick a max Q that will be high enough to encode the
    // content at the given rate.
    for (q = rc->best_quality; q < rc->worst_quality; ++q) {
      const double factor =
          calc_correction_factor(err_per_mb, ERR_DIVISOR,
                                 is_svc_upper_layer ? SVC_FACTOR_PT_LOW :
                                 FACTOR_PT_LOW, FACTOR_PT_HIGH, q);
      const int bits_per_mb = vp9_rc_bits_per_mb(INTER_FRAME, q,
                                                 factor * speed_term);
      if (bits_per_mb <= target_norm_bits_per_mb)
        break;
    }

    // Restriction on active max q for constrained quality mode.
    if (cpi->oxcf.rc_mode == VPX_CQ)
      q = MAX(q, oxcf->cq_level);
    return q;
  }
}

extern void vp9_new_framerate(VP9_COMP *cpi, double framerate);

void vp9_init_second_pass(VP9_COMP *cpi) {
  SVC *const svc = &cpi->svc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const int is_two_pass_svc = (svc->number_spatial_layers > 1) ||
                              (svc->number_temporal_layers > 1);
  TWO_PASS *const twopass = is_two_pass_svc ?
      &svc->layer_context[svc->spatial_layer_id].twopass : &cpi->twopass;
  double frame_rate;
  FIRSTPASS_STATS *stats;

  zero_stats(&twopass->total_stats);
  zero_stats(&twopass->total_left_stats);

  if (!twopass->stats_in_end)
    return;

  stats = &twopass->total_stats;

  *stats = *twopass->stats_in_end;
  twopass->total_left_stats = *stats;

  frame_rate = 10000000.0 * stats->count / stats->duration;
  // Each frame can have a different duration, as the frame rate in the source
  // isn't guaranteed to be constant. The frame rate prior to the first frame
  // encoded in the second pass is a guess. However, the sum duration is not.
  // It is calculated based on the actual durations of all frames from the
  // first pass.

  if (is_two_pass_svc) {
    vp9_update_spatial_layer_framerate(cpi, frame_rate);
    twopass->bits_left = (int64_t)(stats->duration *
        svc->layer_context[svc->spatial_layer_id].target_bandwidth /
        10000000.0);
  } else {
    vp9_new_framerate(cpi, frame_rate);
    twopass->bits_left = (int64_t)(stats->duration * oxcf->target_bandwidth /
                             10000000.0);
  }

  // Calculate a minimum intra value to be used in determining the IIratio
  // scores used in the second pass. We have this minimum to make sure
  // that clips that are static but "low complexity" in the intra domain
  // are still boosted appropriately for KF/GF/ARF.
  if (!is_two_pass_svc) {
    // We don't know the number of MBs for each layer at this point.
    // So we will do it later.
    twopass->kf_intra_err_min = KF_MB_INTRA_MIN * cpi->common.MBs;
    twopass->gf_intra_err_min = GF_MB_INTRA_MIN * cpi->common.MBs;
  }

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  // Scan the first pass file and calculate a modified total error based upon
  // the bias/power function used to allocate bits.
  {
    const double avg_error = stats->coded_error /
                             DOUBLE_DIVIDE_CHECK(stats->count);
    const FIRSTPASS_STATS *s = twopass->stats_in;
    double modified_error_total = 0.0;
    twopass->modified_error_min = (avg_error *
                                      oxcf->two_pass_vbrmin_section) / 100;
    twopass->modified_error_max = (avg_error *
                                      oxcf->two_pass_vbrmax_section) / 100;
    while (s < twopass->stats_in_end) {
      modified_error_total += calculate_modified_err(twopass, oxcf, s);
      ++s;
    }
    twopass->modified_error_left = modified_error_total;
  }

  // Reset the vbr bits off target counter
  cpi->rc.vbr_bits_off_target = 0;
}

// This function gives an estimate of how badly we believe the prediction
// quality is decaying from frame to frame.
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

// This function gives an estimate of how badly we believe the prediction
// quality is decaying from frame to frame.
static double get_zero_motion_factor(const FIRSTPASS_STATS *frame) {
  const double sr_ratio = frame->coded_error /
                          DOUBLE_DIVIDE_CHECK(frame->sr_coded_error);
  const double zero_motion_pct = frame->pcnt_inter -
                                 frame->pcnt_motion;

  return MIN(sr_ratio, zero_motion_pct);
}


// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static int detect_transition_to_still(const TWO_PASS *twopass,
                                      int frame_interval, int still_interval,
                                      double loop_decay_rate,
                                      double last_decay_rate) {
  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > MIN_GF_INTERVAL &&
      loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int j;

    // Look ahead a few frames to see if static condition persists...
    for (j = 0; j < still_interval; ++j) {
      const FIRSTPASS_STATS *stats = &twopass->stats_in[j];
      if (stats >= twopass->stats_in_end)
        break;

      if (stats->pcnt_inter - stats->pcnt_motion < 0.999)
        break;
    }

    // Only if it does do we signal a transition to still.
    return j == still_interval;
  }

  return 0;
}

// This function detects a flash through the high relative pcnt_second_ref
// score in the frame following a flash frame. The offset passed in should
// reflect this.
static int detect_flash(const TWO_PASS *twopass, int offset) {
  const FIRSTPASS_STATS *const next_frame = read_frame_stats(twopass, offset);

  // What we are looking for here is a situation where there is a
  // brief break in prediction (such as a flash) but subsequent frames
  // are reasonably well predicted by an earlier (pre flash) frame.
  // The recovery after a flash is indicated by a high pcnt_second_ref
  // compared to pcnt_inter.
  return next_frame != NULL &&
         next_frame->pcnt_second_ref > next_frame->pcnt_inter &&
         next_frame->pcnt_second_ref >= 0.5;
}

// Update the motion related elements to the GF arf boost calculation.
static void accumulate_frame_motion_stats(const FIRSTPASS_STATS *stats,
                                          double *mv_in_out,
                                          double *mv_in_out_accumulator,
                                          double *abs_mv_in_out_accumulator,
                                          double *mv_ratio_accumulator) {
  const double pct = stats->pcnt_motion;

  // Accumulate Motion In/Out of frame stats.
  *mv_in_out = stats->mv_in_out_count * pct;
  *mv_in_out_accumulator += *mv_in_out;
  *abs_mv_in_out_accumulator += fabs(*mv_in_out);

  // Accumulate a measure of how uniform (or conversely how random) the motion
  // field is (a ratio of abs(mv) / mv).
  if (pct > 0.05) {
    const double mvr_ratio = fabs(stats->mvr_abs) /
                                 DOUBLE_DIVIDE_CHECK(fabs(stats->MVr));
    const double mvc_ratio = fabs(stats->mvc_abs) /
                                 DOUBLE_DIVIDE_CHECK(fabs(stats->MVc));

    *mv_ratio_accumulator += pct * (mvr_ratio < stats->mvr_abs ?
                                       mvr_ratio : stats->mvr_abs);
    *mv_ratio_accumulator += pct * (mvc_ratio < stats->mvc_abs ?
                                       mvc_ratio : stats->mvc_abs);
  }
}

// Calculate a baseline boost number for the current frame.
static double calc_frame_boost(const TWO_PASS *twopass,
                               const FIRSTPASS_STATS *this_frame,
                               double this_frame_mv_in_out) {
  double frame_boost;

  // Underlying boost factor is based on inter intra error ratio.
  if (this_frame->intra_error > twopass->gf_intra_err_min)
    frame_boost = (IIFACTOR * this_frame->intra_error /
                   DOUBLE_DIVIDE_CHECK(this_frame->coded_error));
  else
    frame_boost = (IIFACTOR * twopass->gf_intra_err_min /
                   DOUBLE_DIVIDE_CHECK(this_frame->coded_error));

  // Increase boost for frames where new data coming into frame (e.g. zoom out).
  // Slightly reduce boost if there is a net balance of motion out of the frame
  // (zoom in). The range for this_frame_mv_in_out is -1.0 to +1.0.
  if (this_frame_mv_in_out > 0.0)
    frame_boost += frame_boost * (this_frame_mv_in_out * 2.0);
  // In the extreme case the boost is halved.
  else
    frame_boost += frame_boost * (this_frame_mv_in_out / 2.0);

  return MIN(frame_boost, GF_RMAX);
}

static int calc_arf_boost(VP9_COMP *cpi, int offset,
                          int f_frames, int b_frames,
                          int *f_boost, int *b_boost) {
  TWO_PASS *const twopass = &cpi->twopass;
  int i;
  double boost_score = 0.0;
  double mv_ratio_accumulator = 0.0;
  double decay_accumulator = 1.0;
  double this_frame_mv_in_out = 0.0;
  double mv_in_out_accumulator = 0.0;
  double abs_mv_in_out_accumulator = 0.0;
  int arf_boost;
  int flash_detected = 0;

  // Search forward from the proposed arf/next gf position.
  for (i = 0; i < f_frames; ++i) {
    const FIRSTPASS_STATS *this_frame = read_frame_stats(twopass, i + offset);
    if (this_frame == NULL)
      break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // We want to discount the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(&cpi->common, this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                          ? MIN_DECAY_FACTOR : decay_accumulator;
    }

    boost_score += decay_accumulator * calc_frame_boost(twopass, this_frame,
                                                        this_frame_mv_in_out);
  }

  *f_boost = (int)boost_score;

  // Reset for backward looking loop.
  boost_score = 0.0;
  mv_ratio_accumulator = 0.0;
  decay_accumulator = 1.0;
  this_frame_mv_in_out = 0.0;
  mv_in_out_accumulator = 0.0;
  abs_mv_in_out_accumulator = 0.0;

  // Search backward towards last gf position.
  for (i = -1; i >= -b_frames; --i) {
    const FIRSTPASS_STATS *this_frame = read_frame_stats(twopass, i + offset);
    if (this_frame == NULL)
      break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // We want to discount the the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Cumulative effect of prediction quality decay.
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(&cpi->common, this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                              ? MIN_DECAY_FACTOR : decay_accumulator;
    }

    boost_score += decay_accumulator * calc_frame_boost(twopass, this_frame,
                                                        this_frame_mv_in_out);
  }
  *b_boost = (int)boost_score;

  arf_boost = (*f_boost + *b_boost);
  if (arf_boost < ((b_frames + f_frames) * 20))
    arf_boost = ((b_frames + f_frames) * 20);

  return arf_boost;
}

// Calculate a section intra ratio used in setting max loop filter.
static int calculate_section_intra_ratio(const FIRSTPASS_STATS *begin,
                                         const FIRSTPASS_STATS *end,
                                         int section_length) {
  const FIRSTPASS_STATS *s = begin;
  double intra_error = 0.0;
  double coded_error = 0.0;
  int i = 0;

  while (s < end && i < section_length) {
    intra_error += s->intra_error;
    coded_error += s->coded_error;
    ++s;
    ++i;
  }

  return (int)(intra_error / DOUBLE_DIVIDE_CHECK(coded_error));
}

// Calculate the total bits to allocate in this GF/ARF group.
static int64_t calculate_total_gf_group_bits(VP9_COMP *cpi,
                                             double gf_group_err) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const TWO_PASS *const twopass = &cpi->twopass;
  const int max_bits = frame_max_bits(rc, &cpi->oxcf);
  int64_t total_group_bits;

  // Calculate the bits to be allocated to the group as a whole.
  if ((twopass->kf_group_bits > 0) && (twopass->kf_group_error_left > 0)) {
    total_group_bits = (int64_t)(twopass->kf_group_bits *
                                 (gf_group_err / twopass->kf_group_error_left));
  } else {
    total_group_bits = 0;
  }

  // Clamp odd edge cases.
  total_group_bits = (total_group_bits < 0) ?
     0 : (total_group_bits > twopass->kf_group_bits) ?
     twopass->kf_group_bits : total_group_bits;

  // Clip based on user supplied data rate variability limit.
  if (total_group_bits > (int64_t)max_bits * rc->baseline_gf_interval)
    total_group_bits = (int64_t)max_bits * rc->baseline_gf_interval;

  return total_group_bits;
}

// Calculate the number bits extra to assign to boosted frames in a group.
static int calculate_boost_bits(int frame_count,
                                int boost, int64_t total_group_bits) {
  int allocation_chunks;

  // return 0 for invalid inputs (could arise e.g. through rounding errors)
  if (!boost || (total_group_bits <= 0) || (frame_count <= 0) )
    return 0;

  allocation_chunks = (frame_count * 100) + boost;

  // Prevent overflow.
  if (boost > 1023) {
    int divisor = boost >> 10;
    boost /= divisor;
    allocation_chunks /= divisor;
  }

  // Calculate the number of extra bits for use in the boosted frame or frames.
  return MAX((int)(((int64_t)boost * total_group_bits) / allocation_chunks), 0);
}

// Current limit on maximum number of active arfs in a GF/ARF group.
#define MAX_ACTIVE_ARFS 2
#define ARF_SLOT1 2
#define ARF_SLOT2 3
// This function indirects the choice of buffers for arfs.
// At the moment the values are fixed but this may change as part of
// the integration process with other codec features that swap buffers around.
static void get_arf_buffer_indices(unsigned char *arf_buffer_indices) {
  arf_buffer_indices[0] = ARF_SLOT1;
  arf_buffer_indices[1] = ARF_SLOT2;
}

static void allocate_gf_group_bits(VP9_COMP *cpi, int64_t gf_group_bits,
                                   double group_error, int gf_arf_bits) {
  RATE_CONTROL *const rc = &cpi->rc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  FIRSTPASS_STATS frame_stats;
  int i;
  int frame_index = 1;
  int target_frame_size;
  int key_frame;
  const int max_bits = frame_max_bits(&cpi->rc, &cpi->oxcf);
  int64_t total_group_bits = gf_group_bits;
  double modified_err = 0.0;
  double err_fraction;
  int mid_boost_bits = 0;
  int mid_frame_idx;
  unsigned char arf_buffer_indices[MAX_ACTIVE_ARFS];
  int alt_frame_index = frame_index;
  int has_temporal_layers = is_two_pass_svc(cpi) &&
                            cpi->svc.number_temporal_layers > 1;

  // Only encode alt reference frame in temporal base layer.
  if (has_temporal_layers)
    alt_frame_index = cpi->svc.number_temporal_layers;

  key_frame = cpi->common.frame_type == KEY_FRAME ||
              vp9_is_upper_layer_key_frame(cpi);

  get_arf_buffer_indices(arf_buffer_indices);

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  if (!key_frame) {
    if (rc->source_alt_ref_active) {
      gf_group->update_type[0] = OVERLAY_UPDATE;
      gf_group->rf_level[0] = INTER_NORMAL;
      gf_group->bit_allocation[0] = 0;
      gf_group->arf_update_idx[0] = arf_buffer_indices[0];
      gf_group->arf_ref_idx[0] = arf_buffer_indices[0];
    } else {
      gf_group->update_type[0] = GF_UPDATE;
      gf_group->rf_level[0] = GF_ARF_STD;
      gf_group->bit_allocation[0] = gf_arf_bits;
      gf_group->arf_update_idx[0] = arf_buffer_indices[0];
      gf_group->arf_ref_idx[0] = arf_buffer_indices[0];
    }

    // Step over the golden frame / overlay frame
    if (EOF == input_stats(twopass, &frame_stats))
      return;
  }

  // Deduct the boost bits for arf (or gf if it is not a key frame)
  // from the group total.
  if (rc->source_alt_ref_pending || !key_frame)
    total_group_bits -= gf_arf_bits;

  // Store the bits to spend on the ARF if there is one.
  if (rc->source_alt_ref_pending) {
    gf_group->update_type[alt_frame_index] = ARF_UPDATE;
    gf_group->rf_level[alt_frame_index] = GF_ARF_STD;
    gf_group->bit_allocation[alt_frame_index] = gf_arf_bits;

    if (has_temporal_layers)
      gf_group->arf_src_offset[alt_frame_index] =
          (unsigned char)(rc->baseline_gf_interval -
                          cpi->svc.number_temporal_layers);
    else
      gf_group->arf_src_offset[alt_frame_index] =
          (unsigned char)(rc->baseline_gf_interval - 1);

    gf_group->arf_update_idx[alt_frame_index] = arf_buffer_indices[0];
    gf_group->arf_ref_idx[alt_frame_index] =
      arf_buffer_indices[cpi->multi_arf_last_grp_enabled &&
                         rc->source_alt_ref_active];
    if (!has_temporal_layers)
      ++frame_index;

    if (cpi->multi_arf_enabled) {
      // Set aside a slot for a level 1 arf.
      gf_group->update_type[frame_index] = ARF_UPDATE;
      gf_group->rf_level[frame_index] = GF_ARF_LOW;
      gf_group->arf_src_offset[frame_index] =
        (unsigned char)((rc->baseline_gf_interval >> 1) - 1);
      gf_group->arf_update_idx[frame_index] = arf_buffer_indices[1];
      gf_group->arf_ref_idx[frame_index] = arf_buffer_indices[0];
      ++frame_index;
    }
  }

  // Define middle frame
  mid_frame_idx = frame_index + (rc->baseline_gf_interval >> 1) - 1;

  // Allocate bits to the other frames in the group.
  for (i = 0; i < rc->baseline_gf_interval - 1; ++i) {
    int arf_idx = 0;
    if (EOF == input_stats(twopass, &frame_stats))
      break;

    if (has_temporal_layers && frame_index == alt_frame_index) {
      ++frame_index;
    }

    modified_err = calculate_modified_err(twopass, oxcf, &frame_stats);

    if (group_error > 0)
      err_fraction = modified_err / DOUBLE_DIVIDE_CHECK(group_error);
    else
      err_fraction = 0.0;

    target_frame_size = (int)((double)total_group_bits * err_fraction);

    if (rc->source_alt_ref_pending && cpi->multi_arf_enabled) {
      mid_boost_bits += (target_frame_size >> 4);
      target_frame_size -= (target_frame_size >> 4);

      if (frame_index <= mid_frame_idx)
        arf_idx = 1;
    }
    gf_group->arf_update_idx[frame_index] = arf_buffer_indices[arf_idx];
    gf_group->arf_ref_idx[frame_index] = arf_buffer_indices[arf_idx];

    target_frame_size = clamp(target_frame_size, 0,
                              MIN(max_bits, (int)total_group_bits));

    gf_group->update_type[frame_index] = LF_UPDATE;
    gf_group->rf_level[frame_index] = INTER_NORMAL;

    gf_group->bit_allocation[frame_index] = target_frame_size;
    ++frame_index;
  }

  // Note:
  // We need to configure the frame at the end of the sequence + 1 that will be
  // the start frame for the next group. Otherwise prior to the call to
  // vp9_rc_get_second_pass_params() the data will be undefined.
  gf_group->arf_update_idx[frame_index] = arf_buffer_indices[0];
  gf_group->arf_ref_idx[frame_index] = arf_buffer_indices[0];

  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->rf_level[frame_index] = INTER_NORMAL;

    // Final setup for second arf and its overlay.
    if (cpi->multi_arf_enabled) {
      gf_group->bit_allocation[2] =
          gf_group->bit_allocation[mid_frame_idx] + mid_boost_bits;
      gf_group->update_type[mid_frame_idx] = OVERLAY_UPDATE;
      gf_group->bit_allocation[mid_frame_idx] = 0;
    }
  } else {
    gf_group->update_type[frame_index] = GF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
  }

  // Note whether multi-arf was enabled this group for next time.
  cpi->multi_arf_last_grp_enabled = cpi->multi_arf_enabled;
}

// Analyse and define a gf/arf group.
static void define_gf_group(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  RATE_CONTROL *const rc = &cpi->rc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = twopass->stats_in;
  int i;

  double boost_score = 0.0;
  double old_boost_score = 0.0;
  double gf_group_err = 0.0;
  double gf_first_frame_err = 0.0;
  double mod_frame_err = 0.0;

  double mv_ratio_accumulator = 0.0;
  double decay_accumulator = 1.0;
  double zero_motion_accumulator = 1.0;

  double loop_decay_rate = 1.00;
  double last_loop_decay_rate = 1.00;

  double this_frame_mv_in_out = 0.0;
  double mv_in_out_accumulator = 0.0;
  double abs_mv_in_out_accumulator = 0.0;
  double mv_ratio_accumulator_thresh;
  unsigned int allow_alt_ref = is_altref_enabled(cpi);

  int f_boost = 0;
  int b_boost = 0;
  int flash_detected;
  int active_max_gf_interval;
  int64_t gf_group_bits;
  double gf_group_error_left;
  int gf_arf_bits;

  // Reset the GF group data structures unless this is a key
  // frame in which case it will already have been done.
  if (cpi->common.frame_type != KEY_FRAME) {
    vp9_zero(twopass->gf_group);
  }

  vp9_clear_system_state();
  vp9_zero(next_frame);

  // Load stats for the current frame.
  mod_frame_err = calculate_modified_err(twopass, oxcf, this_frame);

  // Note the error of the frame at the start of the group. This will be
  // the GF frame error if we code a normal gf.
  gf_first_frame_err = mod_frame_err;

  // If this is a key frame or the overlay from a previous arf then
  // the error score / cost of this frame has already been accounted for.
  if (cpi->common.frame_type == KEY_FRAME || rc->source_alt_ref_active)
    gf_group_err -= gf_first_frame_err;

  // Motion breakout threshold for loop below depends on image size.
  mv_ratio_accumulator_thresh = (cpi->common.width + cpi->common.height) / 10.0;

  // Work out a maximum interval for the GF group.
  // If the image appears almost completely static we can extend beyond this.
  if (cpi->multi_arf_allowed) {
    active_max_gf_interval = rc->max_gf_interval;
  } else {
   // The value chosen depends on the active Q range. At low Q we have
   // bits to spare and are better with a smaller interval and smaller boost.
   // At high Q when there are few bits to spare we are better with a longer
   // interval to spread the cost of the GF.
   active_max_gf_interval =
     12 + ((int)vp9_convert_qindex_to_q(rc->last_q[INTER_FRAME]) >> 5);

   if (active_max_gf_interval > rc->max_gf_interval)
     active_max_gf_interval = rc->max_gf_interval;
  }

  i = 0;
  while (i < rc->static_scene_max_gf_interval && i < rc->frames_to_key) {
    ++i;

    // Accumulate error score of frames in this gf group.
    mod_frame_err = calculate_modified_err(twopass, oxcf, this_frame);
    gf_group_err += mod_frame_err;

    if (EOF == input_stats(twopass, &next_frame))
      break;

    // Test for the case where there is a brief flash but the prediction
    // quality back to an earlier frame is then restored.
    flash_detected = detect_flash(twopass, 0);

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(&next_frame,
                                  &this_frame_mv_in_out, &mv_in_out_accumulator,
                                  &abs_mv_in_out_accumulator,
                                  &mv_ratio_accumulator);

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      last_loop_decay_rate = loop_decay_rate;
      loop_decay_rate = get_prediction_decay_rate(&cpi->common, &next_frame);
      decay_accumulator = decay_accumulator * loop_decay_rate;

      // Monitor for static sections.
      zero_motion_accumulator = MIN(zero_motion_accumulator,
                                    get_zero_motion_factor(&next_frame));

      // Break clause to detect very still sections after motion. For example,
      // a static image after a fade or other transition.
      if (detect_transition_to_still(twopass, i, 5, loop_decay_rate,
                                     last_loop_decay_rate)) {
        allow_alt_ref = 0;
        break;
      }
    }

    // Calculate a boost number for this frame.
    boost_score += decay_accumulator * calc_frame_boost(twopass, &next_frame,
                                                        this_frame_mv_in_out);

    // Break out conditions.
    if (
      // Break at active_max_gf_interval unless almost totally static.
      (i >= active_max_gf_interval && (zero_motion_accumulator < 0.995)) ||
      (
        // Don't break out with a very short interval.
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

  // Don't allow a gf too near the next kf.
  if ((rc->frames_to_key - i) < MIN_GF_INTERVAL) {
    while (i < (rc->frames_to_key + !rc->next_key_frame_forced)) {
      ++i;

      if (EOF == input_stats(twopass, this_frame))
        break;

      if (i < rc->frames_to_key) {
        mod_frame_err = calculate_modified_err(twopass, oxcf, this_frame);
        gf_group_err += mod_frame_err;
      }
    }
  }

  // Set the interval until the next gf.
  if (cpi->common.frame_type == KEY_FRAME || rc->source_alt_ref_active)
    rc->baseline_gf_interval = i - 1;
  else
    rc->baseline_gf_interval = i;

  // Only encode alt reference frame in temporal base layer. So
  // baseline_gf_interval should be multiple of a temporal layer group
  // (typically the frame distance between two base layer frames)
  if (is_two_pass_svc(cpi) && cpi->svc.number_temporal_layers > 1) {
    int count = (1 << (cpi->svc.number_temporal_layers - 1)) - 1;
    int new_gf_interval = (rc->baseline_gf_interval + count) & (~count);
    int j;
    for (j = 0; j < new_gf_interval - rc->baseline_gf_interval; ++j) {
      if (EOF == input_stats(twopass, this_frame))
        break;
      gf_group_err += calculate_modified_err(twopass, oxcf, this_frame);
    }
    rc->baseline_gf_interval = new_gf_interval;
  }

  rc->frames_till_gf_update_due = rc->baseline_gf_interval;

  // Should we use the alternate reference frame.
  if (allow_alt_ref &&
      (i < cpi->oxcf.lag_in_frames) &&
      (i >= MIN_GF_INTERVAL) &&
      // For real scene cuts (not forced kfs) don't allow arf very near kf.
      (rc->next_key_frame_forced ||
      (i <= (rc->frames_to_key - MIN_GF_INTERVAL)))) {
    // Calculate the boost for alt ref.
    rc->gfu_boost = calc_arf_boost(cpi, 0, (i - 1), (i - 1), &f_boost,
                                   &b_boost);
    rc->source_alt_ref_pending = 1;

    // Test to see if multi arf is appropriate.
    cpi->multi_arf_enabled =
      (cpi->multi_arf_allowed && (rc->baseline_gf_interval >= 6) &&
      (zero_motion_accumulator < 0.995)) ? 1 : 0;
  } else {
    rc->gfu_boost = (int)boost_score;
    rc->source_alt_ref_pending = 0;
  }

  // Reset the file position.
  reset_fpf_position(twopass, start_pos);

  // Calculate the bits to be allocated to the gf/arf group as a whole
  gf_group_bits = calculate_total_gf_group_bits(cpi, gf_group_err);

  // Calculate the extra bits to be used for boosted frame(s)
  {
    int q = rc->last_q[INTER_FRAME];
    int boost = (rc->gfu_boost * gfboost_qadjust(q)) / 100;

    // Set max and minimum boost and hence minimum allocation.
    boost = clamp(boost, 125, (rc->baseline_gf_interval + 1) * 200);

    // Calculate the extra bits to be used for boosted frame(s)
    gf_arf_bits = calculate_boost_bits(rc->baseline_gf_interval,
                                       boost, gf_group_bits);
  }

  // Adjust KF group bits and error remaining.
  twopass->kf_group_error_left -= (int64_t)gf_group_err;

  // If this is an arf update we want to remove the score for the overlay
  // frame at the end which will usually be very cheap to code.
  // The overlay frame has already, in effect, been coded so we want to spread
  // the remaining bits among the other frames.
  // For normal GFs remove the score for the GF itself unless this is
  // also a key frame in which case it has already been accounted for.
  if (rc->source_alt_ref_pending) {
    gf_group_error_left = gf_group_err - mod_frame_err;
  } else if (cpi->common.frame_type != KEY_FRAME) {
    gf_group_error_left = gf_group_err - gf_first_frame_err;
  } else {
    gf_group_error_left = gf_group_err;
  }

  // Allocate bits to each of the frames in the GF group.
  allocate_gf_group_bits(cpi, gf_group_bits, gf_group_error_left, gf_arf_bits);

  // Reset the file position.
  reset_fpf_position(twopass, start_pos);

  // Calculate a section intra ratio used in setting max loop filter.
  if (cpi->common.frame_type != KEY_FRAME) {
    twopass->section_intra_rating =
        calculate_section_intra_ratio(start_pos, twopass->stats_in_end,
                                      rc->baseline_gf_interval);
  }
}

static int test_candidate_kf(TWO_PASS *twopass,
                             const FIRSTPASS_STATS *last_frame,
                             const FIRSTPASS_STATS *this_frame,
                             const FIRSTPASS_STATS *next_frame) {
  int is_viable_kf = 0;

  // Does the frame satisfy the primary criteria of a key frame?
  // If so, then examine how well it predicts subsequent frames.
  if ((this_frame->pcnt_second_ref < 0.10) &&
      (next_frame->pcnt_second_ref < 0.10) &&
      ((this_frame->pcnt_inter < 0.05) ||
       (((this_frame->pcnt_inter - this_frame->pcnt_neutral) < 0.35) &&
        ((this_frame->intra_error /
          DOUBLE_DIVIDE_CHECK(this_frame->coded_error)) < 2.5) &&
        ((fabs(last_frame->coded_error - this_frame->coded_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->coded_error) > 0.40) ||
         (fabs(last_frame->intra_error - this_frame->intra_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->intra_error) > 0.40) ||
         ((next_frame->intra_error /
           DOUBLE_DIVIDE_CHECK(next_frame->coded_error)) > 3.5))))) {
    int i;
    const FIRSTPASS_STATS *start_pos = twopass->stats_in;
    FIRSTPASS_STATS local_next_frame = *next_frame;
    double boost_score = 0.0;
    double old_boost_score = 0.0;
    double decay_accumulator = 1.0;

    // Examine how well the key frame predicts subsequent frames.
    for (i = 0; i < 16; ++i) {
      double next_iiratio = (IIKFACTOR1 * local_next_frame.intra_error /
                             DOUBLE_DIVIDE_CHECK(local_next_frame.coded_error));

      if (next_iiratio > RMAX)
        next_iiratio = RMAX;

      // Cumulative effect of decay in prediction quality.
      if (local_next_frame.pcnt_inter > 0.85)
        decay_accumulator *= local_next_frame.pcnt_inter;
      else
        decay_accumulator *= (0.85 + local_next_frame.pcnt_inter) / 2.0;

      // Keep a running total.
      boost_score += (decay_accumulator * next_iiratio);

      // Test various breakout clauses.
      if ((local_next_frame.pcnt_inter < 0.05) ||
          (next_iiratio < 1.5) ||
          (((local_next_frame.pcnt_inter -
             local_next_frame.pcnt_neutral) < 0.20) &&
           (next_iiratio < 3.0)) ||
          ((boost_score - old_boost_score) < 3.0) ||
          (local_next_frame.intra_error < 200)) {
        break;
      }

      old_boost_score = boost_score;

      // Get the next frame details
      if (EOF == input_stats(twopass, &local_next_frame))
        break;
    }

    // If there is tolerable prediction for at least the next 3 frames then
    // break out else discard this potential key frame and move on
    if (boost_score > 30.0 && (i > 3)) {
      is_viable_kf = 1;
    } else {
      // Reset the file position
      reset_fpf_position(twopass, start_pos);

      is_viable_kf = 0;
    }
  }

  return is_viable_kf;
}

static void find_next_key_frame(VP9_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  int i, j;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  const FIRSTPASS_STATS first_frame = *this_frame;
  const FIRSTPASS_STATS *const start_position = twopass->stats_in;
  FIRSTPASS_STATS next_frame;
  FIRSTPASS_STATS last_frame;
  int kf_bits = 0;
  double decay_accumulator = 1.0;
  double zero_motion_accumulator = 1.0;
  double boost_score = 0.0;
  double kf_mod_err = 0.0;
  double kf_group_err = 0.0;
  double recent_loop_decay[8] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

  vp9_zero(next_frame);

  cpi->common.frame_type = KEY_FRAME;

  // Reset the GF group data structures.
  vp9_zero(*gf_group);

  // Is this a forced key frame by interval.
  rc->this_key_frame_forced = rc->next_key_frame_forced;

  // Clear the alt ref active flag and last group multi arf flags as they
  // can never be set for a key frame.
  rc->source_alt_ref_active = 0;
  cpi->multi_arf_last_grp_enabled = 0;

  // KF is always a GF so clear frames till next gf counter.
  rc->frames_till_gf_update_due = 0;

  rc->frames_to_key = 1;

  twopass->kf_group_bits = 0;        // Total bits available to kf group
  twopass->kf_group_error_left = 0;  // Group modified error score.

  kf_mod_err = calculate_modified_err(twopass, oxcf, this_frame);

  // Find the next keyframe.
  i = 0;
  while (twopass->stats_in < twopass->stats_in_end &&
         rc->frames_to_key < cpi->oxcf.key_freq) {
    // Accumulate kf group error.
    kf_group_err += calculate_modified_err(twopass, oxcf, this_frame);

    // Load the next frame's stats.
    last_frame = *this_frame;
    input_stats(twopass, this_frame);

    // Provided that we are not at the end of the file...
    if (cpi->oxcf.auto_key && twopass->stats_in < twopass->stats_in_end) {
      double loop_decay_rate;

      // Check for a scene cut.
      if (test_candidate_kf(twopass, &last_frame, this_frame,
                            twopass->stats_in))
        break;

      // How fast is the prediction quality decaying?
      loop_decay_rate = get_prediction_decay_rate(&cpi->common,
                                                  twopass->stats_in);

      // We want to know something about the recent past... rather than
      // as used elsewhere where we are concerned with decay in prediction
      // quality since the last GF or KF.
      recent_loop_decay[i % 8] = loop_decay_rate;
      decay_accumulator = 1.0;
      for (j = 0; j < 8; ++j)
        decay_accumulator *= recent_loop_decay[j];

      // Special check for transition or high motion followed by a
      // static scene.
      if (detect_transition_to_still(twopass, i, cpi->oxcf.key_freq - i,
                                     loop_decay_rate, decay_accumulator))
        break;

      // Step on to the next frame.
      ++rc->frames_to_key;

      // If we don't have a real key frame within the next two
      // key_freq intervals then break out of the loop.
      if (rc->frames_to_key >= 2 * cpi->oxcf.key_freq)
        break;
    } else {
      ++rc->frames_to_key;
    }
    ++i;
  }

  // If there is a max kf interval set by the user we must obey it.
  // We already breakout of the loop above at 2x max.
  // This code centers the extra kf if the actual natural interval
  // is between 1x and 2x.
  if (cpi->oxcf.auto_key &&
      rc->frames_to_key > cpi->oxcf.key_freq) {
    FIRSTPASS_STATS tmp_frame = first_frame;

    rc->frames_to_key /= 2;

    // Reset to the start of the group.
    reset_fpf_position(twopass, start_position);

    kf_group_err = 0;

    // Rescan to get the correct error data for the forced kf group.
    for (i = 0; i < rc->frames_to_key; ++i) {
      kf_group_err += calculate_modified_err(twopass, oxcf, &tmp_frame);
      input_stats(twopass, &tmp_frame);
    }
    rc->next_key_frame_forced = 1;
  } else if (twopass->stats_in == twopass->stats_in_end ||
             rc->frames_to_key >= cpi->oxcf.key_freq) {
    rc->next_key_frame_forced = 1;
  } else {
    rc->next_key_frame_forced = 0;
  }

  if (is_two_pass_svc(cpi) && cpi->svc.number_temporal_layers > 1) {
    int count = (1 << (cpi->svc.number_temporal_layers - 1)) - 1;
    int new_frame_to_key = (rc->frames_to_key + count) & (~count);
    int j;
    for (j = 0; j < new_frame_to_key - rc->frames_to_key; ++j) {
      if (EOF == input_stats(twopass, this_frame))
        break;
      kf_group_err += calculate_modified_err(twopass, oxcf, this_frame);
    }
    rc->frames_to_key = new_frame_to_key;
  }

  // Special case for the last key frame of the file.
  if (twopass->stats_in >= twopass->stats_in_end) {
    // Accumulate kf group error.
    kf_group_err += calculate_modified_err(twopass, oxcf, this_frame);
  }

  // Calculate the number of bits that should be assigned to the kf group.
  if (twopass->bits_left > 0 && twopass->modified_error_left > 0.0) {
    // Maximum number of bits for a single normal frame (not key frame).
    const int max_bits = frame_max_bits(rc, &cpi->oxcf);

    // Maximum number of bits allocated to the key frame group.
    int64_t max_grp_bits;

    // Default allocation based on bits left and relative
    // complexity of the section.
    twopass->kf_group_bits = (int64_t)(twopass->bits_left *
       (kf_group_err / twopass->modified_error_left));

    // Clip based on maximum per frame rate defined by the user.
    max_grp_bits = (int64_t)max_bits * (int64_t)rc->frames_to_key;
    if (twopass->kf_group_bits > max_grp_bits)
      twopass->kf_group_bits = max_grp_bits;
  } else {
    twopass->kf_group_bits = 0;
  }
  twopass->kf_group_bits = MAX(0, twopass->kf_group_bits);

  // Reset the first pass file position.
  reset_fpf_position(twopass, start_position);

  // Scan through the kf group collating various stats used to deteermine
  // how many bits to spend on it.
  decay_accumulator = 1.0;
  boost_score = 0.0;
  for (i = 0; i < rc->frames_to_key; ++i) {
    if (EOF == input_stats(twopass, &next_frame))
      break;

    // Monitor for static sections.
    zero_motion_accumulator =MIN(zero_motion_accumulator,
                                 get_zero_motion_factor(&next_frame));

    // For the first few frames collect data to decide kf boost.
    if (i <= (rc->max_gf_interval * 2)) {
      double r;
      if (next_frame.intra_error > twopass->kf_intra_err_min)
        r = (IIKFACTOR2 * next_frame.intra_error /
             DOUBLE_DIVIDE_CHECK(next_frame.coded_error));
      else
        r = (IIKFACTOR2 * twopass->kf_intra_err_min /
             DOUBLE_DIVIDE_CHECK(next_frame.coded_error));

      if (r > RMAX)
        r = RMAX;

      // How fast is prediction quality decaying.
      if (!detect_flash(twopass, 0)) {
        const double loop_decay_rate = get_prediction_decay_rate(&cpi->common,
                                                                 &next_frame);
        decay_accumulator *= loop_decay_rate;
        decay_accumulator = MAX(decay_accumulator, MIN_DECAY_FACTOR);
      }

      boost_score += (decay_accumulator * r);
    }
  }

  reset_fpf_position(twopass, start_position);

  // Store the zero motion percentage
  twopass->kf_zeromotion_pct = (int)(zero_motion_accumulator * 100.0);

  // Calculate a section intra ratio used in setting max loop filter.
  twopass->section_intra_rating =
      calculate_section_intra_ratio(start_position, twopass->stats_in_end,
                                    rc->frames_to_key);

  // Work out how many bits to allocate for the key frame itself.
  rc->kf_boost = (int)boost_score;

  if (rc->kf_boost  < (rc->frames_to_key * 3))
    rc->kf_boost  = (rc->frames_to_key * 3);
  if (rc->kf_boost   < MIN_KF_BOOST)
    rc->kf_boost = MIN_KF_BOOST;

  kf_bits = calculate_boost_bits((rc->frames_to_key - 1),
                                  rc->kf_boost, twopass->kf_group_bits);

  twopass->kf_group_bits -= kf_bits;

  // Save the bits to spend on the key frame.
  gf_group->bit_allocation[0] = kf_bits;
  gf_group->update_type[0] = KF_UPDATE;
  gf_group->rf_level[0] = KF_STD;

  // Note the total error score of the kf group minus the key frame itself.
  twopass->kf_group_error_left = (int)(kf_group_err - kf_mod_err);

  // Adjust the count of total modified error left.
  // The count of bits left is adjusted elsewhere based on real coded frame
  // sizes.
  twopass->modified_error_left -= kf_group_err;
}

// For VBR...adjustment to the frame target based on error from previous frames
void vbr_rate_correction(int * this_frame_target,
                         const int64_t vbr_bits_off_target) {
  int max_delta = (*this_frame_target * 15) / 100;

  // vbr_bits_off_target > 0 means we have extra bits to spend
  if (vbr_bits_off_target > 0) {
    *this_frame_target +=
      (vbr_bits_off_target > max_delta) ? max_delta
                                        : (int)vbr_bits_off_target;
  } else {
    *this_frame_target -=
      (vbr_bits_off_target < -max_delta) ? max_delta
                                         : (int)-vbr_bits_off_target;
  }
}

// Define the reference buffers that will be updated post encode.
void configure_buffer_updates(VP9_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;

  cpi->rc.is_src_frame_alt_ref = 0;
  switch (twopass->gf_group.update_type[twopass->gf_group.index]) {
    case KF_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_alt_ref_frame = 1;
      break;
    case LF_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;
    case GF_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_alt_ref_frame = 0;
      break;
    case OVERLAY_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_alt_ref_frame = 0;
      cpi->rc.is_src_frame_alt_ref = 1;
      break;
    case ARF_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      break;
    default:
      assert(0);
      break;
  }
  if (is_two_pass_svc(cpi)) {
    if (cpi->svc.layer_context[cpi->svc.spatial_layer_id].gold_ref_idx < 0)
      cpi->refresh_golden_frame = 0;
    if (cpi->alt_ref_source == NULL)
      cpi->refresh_alt_ref_frame = 0;
  }
}


void vp9_rc_get_second_pass_params(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  int frames_left;
  FIRSTPASS_STATS this_frame;
  FIRSTPASS_STATS this_frame_copy;

  int target_rate;
  LAYER_CONTEXT *const lc = is_two_pass_svc(cpi) ?
        &cpi->svc.layer_context[cpi->svc.spatial_layer_id] : 0;

  if (lc != NULL) {
    frames_left = (int)(twopass->total_stats.count -
                  lc->current_video_frame_in_layer);
  } else {
    frames_left = (int)(twopass->total_stats.count -
                  cm->current_video_frame);
  }

  if (!twopass->stats_in)
    return;

  // If this is an arf frame then we dont want to read the stats file or
  // advance the input pointer as we already have what we need.
  if (gf_group->update_type[gf_group->index] == ARF_UPDATE) {
    int target_rate;
    configure_buffer_updates(cpi);
    target_rate = gf_group->bit_allocation[gf_group->index];
    target_rate = vp9_rc_clamp_pframe_target_size(cpi, target_rate);
    rc->base_frame_target = target_rate;

    // Correction to rate target based on prior over or under shoot.
    if (cpi->oxcf.rc_mode == VPX_VBR)
      vbr_rate_correction(&target_rate, rc->vbr_bits_off_target);

    vp9_rc_set_frame_target(cpi, target_rate);
    cm->frame_type = INTER_FRAME;

    if (lc != NULL) {
      if (cpi->svc.spatial_layer_id == 0) {
        lc->is_key_frame = 0;
      } else {
        lc->is_key_frame = cpi->svc.layer_context[0].is_key_frame;

        if (lc->is_key_frame)
          cpi->ref_frame_flags &= (~VP9_LAST_FLAG);
      }
    }

    return;
  }

  vp9_clear_system_state();

  if (lc != NULL && twopass->kf_intra_err_min == 0) {
    twopass->kf_intra_err_min = KF_MB_INTRA_MIN * cpi->common.MBs;
    twopass->gf_intra_err_min = GF_MB_INTRA_MIN * cpi->common.MBs;
  }

  if (cpi->oxcf.rc_mode == VPX_Q) {
    twopass->active_worst_quality = cpi->oxcf.cq_level;
  } else if (cm->current_video_frame == 0 ||
             (lc != NULL && lc->current_video_frame_in_layer == 0)) {
    // Special case code for first frame.
    const int section_target_bandwidth = (int)(twopass->bits_left /
                                               frames_left);
    const int tmp_q = get_twopass_worst_quality(cpi, &twopass->total_left_stats,
                                                section_target_bandwidth);
    twopass->active_worst_quality = tmp_q;
    rc->ni_av_qi = tmp_q;
    rc->avg_q = vp9_convert_qindex_to_q(tmp_q);
  }
  vp9_zero(this_frame);
  if (EOF == input_stats(twopass, &this_frame))
    return;

  // Local copy of the current frame's first pass stats.
  this_frame_copy = this_frame;

  // Keyframe and section processing.
  if (rc->frames_to_key == 0 ||
      (cpi->frame_flags & FRAMEFLAGS_KEY)) {
    // Define next KF group and assign bits to it.
    find_next_key_frame(cpi, &this_frame_copy);
  } else {
    cm->frame_type = INTER_FRAME;
  }

  if (lc != NULL) {
    if (cpi->svc.spatial_layer_id == 0) {
      lc->is_key_frame = (cm->frame_type == KEY_FRAME);
      if (lc->is_key_frame) {
        cpi->ref_frame_flags &=
            (~VP9_LAST_FLAG & ~VP9_GOLD_FLAG & ~VP9_ALT_FLAG);
        lc->frames_from_key_frame = 0;
      }
    } else {
      cm->frame_type = INTER_FRAME;
      lc->is_key_frame = cpi->svc.layer_context[0].is_key_frame;

      if (lc->is_key_frame) {
        cpi->ref_frame_flags &= (~VP9_LAST_FLAG);
        lc->frames_from_key_frame = 0;
      }
    }
  }

  // Define a new GF/ARF group. (Should always enter here for key frames).
  if (rc->frames_till_gf_update_due == 0) {
    define_gf_group(cpi, &this_frame_copy);

    if (twopass->gf_zeromotion_pct > 995) {
      // As long as max_thresh for encode breakout is small enough, it is ok
      // to enable it for show frame, i.e. set allow_encode_breakout to
      // ENCODE_BREAKOUT_LIMITED.
      if (!cm->show_frame)
        cpi->allow_encode_breakout = ENCODE_BREAKOUT_DISABLED;
      else
        cpi->allow_encode_breakout = ENCODE_BREAKOUT_LIMITED;
    }

    rc->frames_till_gf_update_due = rc->baseline_gf_interval;
    if (lc != NULL)
      cpi->refresh_golden_frame = 1;
  }

  configure_buffer_updates(cpi);

  target_rate = gf_group->bit_allocation[gf_group->index];
  if (cpi->common.frame_type == KEY_FRAME)
    target_rate = vp9_rc_clamp_iframe_target_size(cpi, target_rate);
  else
    target_rate = vp9_rc_clamp_pframe_target_size(cpi, target_rate);

  rc->base_frame_target = target_rate;

  // Correction to rate target based on prior over or under shoot.
  if (cpi->oxcf.rc_mode == VPX_VBR)
    vbr_rate_correction(&target_rate, rc->vbr_bits_off_target);

  vp9_rc_set_frame_target(cpi, target_rate);

  // Update the total stats remaining structure.
  subtract_stats(&twopass->total_left_stats, &this_frame);
}

void vp9_twopass_postencode_update(VP9_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;

  // VBR correction is done through rc->vbr_bits_off_target. Based on the
  // sign of this value, a limited % adjustment is made to the target rate
  // of subsequent frames, to try and push it back towards 0. This method
  // is designed to prevent extreme behaviour at the end of a clip
  // or group of frames.
  const int bits_used = rc->base_frame_target;
  rc->vbr_bits_off_target += rc->base_frame_target - rc->projected_frame_size;

  twopass->bits_left = MAX(twopass->bits_left - bits_used, 0);

  if (cpi->common.frame_type != KEY_FRAME &&
      !vp9_is_upper_layer_key_frame(cpi)) {
    twopass->kf_group_bits -= bits_used;
  }
  twopass->kf_group_bits = MAX(twopass->kf_group_bits, 0);

  // Increment the gf group index ready for the next frame.
  ++twopass->gf_group.index;
}
