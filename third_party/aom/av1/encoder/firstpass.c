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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"
#include "aom_scale/yv12config.h"

#include "aom_dsp/variance.h"
#include "av1/common/entropymv.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"  // av1_setup_dst_planes()
#include "av1/common/txb_common.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/block.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/dwt.h"

#define OUTPUT_FPF 0
#define ARF_STATS_OUTPUT 0

#define GROUP_ADAPTIVE_MAXQ 1

#define BOOST_BREAKOUT 12.5
#define BOOST_FACTOR 12.5
#define FACTOR_PT_LOW 0.70
#define FACTOR_PT_HIGH 0.90
#define FIRST_PASS_Q 10.0
#define GF_MAX_BOOST 96.0
#define INTRA_MODE_PENALTY 1024
#define KF_MAX_BOOST 128.0
#define MIN_ARF_GF_BOOST 240
#define MIN_DECAY_FACTOR 0.01
#define MIN_KF_BOOST 300
#define NEW_MV_MODE_PENALTY 32
#define DARK_THRESH 64
#define DEFAULT_GRP_WEIGHT 1.0
#define RC_FACTOR_MIN 0.75
#define RC_FACTOR_MAX 1.75

#define NCOUNT_INTRA_THRESH 8192
#define NCOUNT_INTRA_FACTOR 3
#define NCOUNT_FRAME_II_THRESH 5.0

#define DOUBLE_DIVIDE_CHECK(x) ((x) < 0 ? (x)-0.000001 : (x) + 0.000001)

#if ARF_STATS_OUTPUT
unsigned int arf_count = 0;
#endif

// Resets the first pass file to the given position using a relative seek from
// the current position.
static void reset_fpf_position(TWO_PASS *p, const FIRSTPASS_STATS *position) {
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
  if (p->stats_in >= p->stats_in_end) return EOF;

  *fps = *p->stats_in;
  ++p->stats_in;
  return 1;
}

static void output_stats(FIRSTPASS_STATS *stats,
                         struct aom_codec_pkt_list *pktlist) {
  struct aom_codec_cx_pkt pkt;
  pkt.kind = AOM_CODEC_STATS_PKT;
  pkt.data.twopass_stats.buf = stats;
  pkt.data.twopass_stats.sz = sizeof(FIRSTPASS_STATS);
  aom_codec_pkt_list_add(pktlist, &pkt);

// TEMP debug code
#if OUTPUT_FPF
  {
    FILE *fpfile;
    fpfile = fopen("firstpass.stt", "a");

    fprintf(fpfile,
            "%12.0lf %12.4lf %12.0lf %12.0lf %12.0lf %12.4lf %12.4lf"
            "%12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf"
            "%12.4lf %12.4lf %12.0lf %12.0lf %12.0lf %12.4lf %12.4lf\n",
            stats->frame, stats->weight, stats->intra_error, stats->coded_error,
            stats->sr_coded_error, stats->pcnt_inter, stats->pcnt_motion,
            stats->pcnt_second_ref, stats->pcnt_neutral, stats->intra_skip_pct,
            stats->inactive_zone_rows, stats->inactive_zone_cols, stats->MVr,
            stats->mvr_abs, stats->MVc, stats->mvc_abs, stats->MVrv,
            stats->MVcv, stats->mv_in_out_count, stats->new_mv_count,
            stats->count, stats->duration);
    fclose(fpfile);
  }
#endif
}

#if CONFIG_FP_MB_STATS
static void output_fpmb_stats(uint8_t *this_frame_mb_stats, int stats_size,
                              struct aom_codec_pkt_list *pktlist) {
  struct aom_codec_cx_pkt pkt;
  pkt.kind = AOM_CODEC_FPMB_STATS_PKT;
  pkt.data.firstpass_mb_stats.buf = this_frame_mb_stats;
  pkt.data.firstpass_mb_stats.sz = stats_size * sizeof(*this_frame_mb_stats);
  aom_codec_pkt_list_add(pktlist, &pkt);
}
#endif

static void zero_stats(FIRSTPASS_STATS *section) {
  section->frame = 0.0;
  section->weight = 0.0;
  section->intra_error = 0.0;
  section->frame_avg_wavelet_energy = 0.0;
  section->coded_error = 0.0;
  section->sr_coded_error = 0.0;
  section->pcnt_inter = 0.0;
  section->pcnt_motion = 0.0;
  section->pcnt_second_ref = 0.0;
  section->pcnt_neutral = 0.0;
  section->intra_skip_pct = 0.0;
  section->inactive_zone_rows = 0.0;
  section->inactive_zone_cols = 0.0;
  section->MVr = 0.0;
  section->mvr_abs = 0.0;
  section->MVc = 0.0;
  section->mvc_abs = 0.0;
  section->MVrv = 0.0;
  section->MVcv = 0.0;
  section->mv_in_out_count = 0.0;
  section->new_mv_count = 0.0;
  section->count = 0.0;
  section->duration = 1.0;
}

static void accumulate_stats(FIRSTPASS_STATS *section,
                             const FIRSTPASS_STATS *frame) {
  section->frame += frame->frame;
  section->weight += frame->weight;
  section->intra_error += frame->intra_error;
  section->frame_avg_wavelet_energy += frame->frame_avg_wavelet_energy;
  section->coded_error += frame->coded_error;
  section->sr_coded_error += frame->sr_coded_error;
  section->pcnt_inter += frame->pcnt_inter;
  section->pcnt_motion += frame->pcnt_motion;
  section->pcnt_second_ref += frame->pcnt_second_ref;
  section->pcnt_neutral += frame->pcnt_neutral;
  section->intra_skip_pct += frame->intra_skip_pct;
  section->inactive_zone_rows += frame->inactive_zone_rows;
  section->inactive_zone_cols += frame->inactive_zone_cols;
  section->MVr += frame->MVr;
  section->mvr_abs += frame->mvr_abs;
  section->MVc += frame->MVc;
  section->mvc_abs += frame->mvc_abs;
  section->MVrv += frame->MVrv;
  section->MVcv += frame->MVcv;
  section->mv_in_out_count += frame->mv_in_out_count;
  section->new_mv_count += frame->new_mv_count;
  section->count += frame->count;
  section->duration += frame->duration;
}

static void subtract_stats(FIRSTPASS_STATS *section,
                           const FIRSTPASS_STATS *frame) {
  section->frame -= frame->frame;
  section->weight -= frame->weight;
  section->intra_error -= frame->intra_error;
  section->frame_avg_wavelet_energy -= frame->frame_avg_wavelet_energy;
  section->coded_error -= frame->coded_error;
  section->sr_coded_error -= frame->sr_coded_error;
  section->pcnt_inter -= frame->pcnt_inter;
  section->pcnt_motion -= frame->pcnt_motion;
  section->pcnt_second_ref -= frame->pcnt_second_ref;
  section->pcnt_neutral -= frame->pcnt_neutral;
  section->intra_skip_pct -= frame->intra_skip_pct;
  section->inactive_zone_rows -= frame->inactive_zone_rows;
  section->inactive_zone_cols -= frame->inactive_zone_cols;
  section->MVr -= frame->MVr;
  section->mvr_abs -= frame->mvr_abs;
  section->MVc -= frame->MVc;
  section->mvc_abs -= frame->mvc_abs;
  section->MVrv -= frame->MVrv;
  section->MVcv -= frame->MVcv;
  section->mv_in_out_count -= frame->mv_in_out_count;
  section->new_mv_count -= frame->new_mv_count;
  section->count -= frame->count;
  section->duration -= frame->duration;
}

// Calculate the linear size relative to a baseline of 1080P
#define BASE_SIZE 2073600.0  // 1920x1080
static double get_linear_size_factor(const AV1_COMP *cpi) {
  const double this_area = cpi->initial_width * cpi->initial_height;
  return pow(this_area / BASE_SIZE, 0.5);
}

// Calculate an active area of the image that discounts formatting
// bars and partially discounts other 0 energy areas.
#define MIN_ACTIVE_AREA 0.5
#define MAX_ACTIVE_AREA 1.0
static double calculate_active_area(const AV1_COMP *cpi,
                                    const FIRSTPASS_STATS *this_frame) {
  double active_pct;

  active_pct =
      1.0 -
      ((this_frame->intra_skip_pct / 2) +
       ((this_frame->inactive_zone_rows * 2) / (double)cpi->common.mb_rows));
  return fclamp(active_pct, MIN_ACTIVE_AREA, MAX_ACTIVE_AREA);
}

// Calculate a modified Error used in distributing bits between easier and
// harder frames.
#define ACT_AREA_CORRECTION 0.5
static double calculate_modified_err(const AV1_COMP *cpi,
                                     const TWO_PASS *twopass,
                                     const AV1EncoderConfig *oxcf,
                                     const FIRSTPASS_STATS *this_frame) {
  const FIRSTPASS_STATS *const stats = &twopass->total_stats;
  const double av_weight = stats->weight / stats->count;
  const double av_err = (stats->coded_error * av_weight) / stats->count;
  double modified_error =
      av_err * pow(this_frame->coded_error * this_frame->weight /
                       DOUBLE_DIVIDE_CHECK(av_err),
                   oxcf->two_pass_vbrbias / 100.0);

  // Correction for active area. Frames with a reduced active area
  // (eg due to formatting bars) have a higher error per mb for the
  // remaining active MBs. The correction here assumes that coding
  // 0.5N blocks of complexity 2X is a little easier than coding N
  // blocks of complexity X.
  modified_error *=
      pow(calculate_active_area(cpi, this_frame), ACT_AREA_CORRECTION);

  return fclamp(modified_error, twopass->modified_error_min,
                twopass->modified_error_max);
}

// This function returns the maximum target rate per frame.
static int frame_max_bits(const RATE_CONTROL *rc,
                          const AV1EncoderConfig *oxcf) {
  int64_t max_bits = ((int64_t)rc->avg_frame_bandwidth *
                      (int64_t)oxcf->two_pass_vbrmax_section) /
                     100;
  if (max_bits < 0)
    max_bits = 0;
  else if (max_bits > rc->max_frame_bandwidth)
    max_bits = rc->max_frame_bandwidth;

  return (int)max_bits;
}

void av1_init_first_pass(AV1_COMP *cpi) {
  zero_stats(&cpi->twopass.total_stats);
}

void av1_end_first_pass(AV1_COMP *cpi) {
  output_stats(&cpi->twopass.total_stats, cpi->output_pkt_list);
}

static aom_variance_fn_t get_block_variance_fn(BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_8X8: return aom_mse8x8;
    case BLOCK_16X8: return aom_mse16x8;
    case BLOCK_8X16: return aom_mse8x16;
    default: return aom_mse16x16;
  }
}

static unsigned int get_prediction_error(BLOCK_SIZE bsize,
                                         const struct buf_2d *src,
                                         const struct buf_2d *ref) {
  unsigned int sse;
  const aom_variance_fn_t fn = get_block_variance_fn(bsize);
  fn(src->buf, src->stride, ref->buf, ref->stride, &sse);
  return sse;
}

static aom_variance_fn_t highbd_get_block_variance_fn(BLOCK_SIZE bsize,
                                                      int bd) {
  switch (bd) {
    default:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_8_mse8x8;
        case BLOCK_16X8: return aom_highbd_8_mse16x8;
        case BLOCK_8X16: return aom_highbd_8_mse8x16;
        default: return aom_highbd_8_mse16x16;
      }
      break;
    case 10:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_10_mse8x8;
        case BLOCK_16X8: return aom_highbd_10_mse16x8;
        case BLOCK_8X16: return aom_highbd_10_mse8x16;
        default: return aom_highbd_10_mse16x16;
      }
      break;
    case 12:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_12_mse8x8;
        case BLOCK_16X8: return aom_highbd_12_mse16x8;
        case BLOCK_8X16: return aom_highbd_12_mse8x16;
        default: return aom_highbd_12_mse16x16;
      }
      break;
  }
}

static unsigned int highbd_get_prediction_error(BLOCK_SIZE bsize,
                                                const struct buf_2d *src,
                                                const struct buf_2d *ref,
                                                int bd) {
  unsigned int sse;
  const aom_variance_fn_t fn = highbd_get_block_variance_fn(bsize, bd);
  fn(src->buf, src->stride, ref->buf, ref->stride, &sse);
  return sse;
}

// Refine the motion search range according to the frame dimension
// for first pass test.
static int get_search_range(const AV1_COMP *cpi) {
  int sr = 0;
  const int dim = AOMMIN(cpi->initial_width, cpi->initial_height);

  while ((dim << sr) < MAX_FULL_PEL_VAL) ++sr;
  return sr;
}

static void first_pass_motion_search(AV1_COMP *cpi, MACROBLOCK *x,
                                     const MV *ref_mv, MV *best_mv,
                                     int *best_motion_err) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MV tmp_mv = kZeroMv;
  MV ref_mv_full = { ref_mv->row >> 3, ref_mv->col >> 3 };
  int num00, tmp_err, n;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  aom_variance_fn_ptr_t v_fn_ptr = cpi->fn_ptr[bsize];
  const int new_mv_mode_penalty = NEW_MV_MODE_PENALTY;

  int step_param = 3;
  int further_steps = (MAX_MVSEARCH_STEPS - 1) - step_param;
  const int sr = get_search_range(cpi);
  step_param += sr;
  further_steps -= sr;

  // Override the default variance function to use MSE.
  v_fn_ptr.vf = get_block_variance_fn(bsize);
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    v_fn_ptr.vf = highbd_get_block_variance_fn(bsize, xd->bd);
  }

  // Center the initial step/diamond search on best mv.
  tmp_err = cpi->diamond_search_sad(x, &cpi->ss_cfg, &ref_mv_full, &tmp_mv,
                                    step_param, x->sadperbit16, &num00,
                                    &v_fn_ptr, ref_mv);
  if (tmp_err < INT_MAX)
    tmp_err = av1_get_mvpred_var(x, &tmp_mv, ref_mv, &v_fn_ptr, 1);
  if (tmp_err < INT_MAX - new_mv_mode_penalty) tmp_err += new_mv_mode_penalty;

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
                                        step_param + n, x->sadperbit16, &num00,
                                        &v_fn_ptr, ref_mv);
      if (tmp_err < INT_MAX)
        tmp_err = av1_get_mvpred_var(x, &tmp_mv, ref_mv, &v_fn_ptr, 1);
      if (tmp_err < INT_MAX - new_mv_mode_penalty)
        tmp_err += new_mv_mode_penalty;

      if (tmp_err < *best_motion_err) {
        *best_motion_err = tmp_err;
        *best_mv = tmp_mv;
      }
    }
  }
}

static BLOCK_SIZE get_bsize(const AV1_COMMON *cm, int mb_row, int mb_col) {
  if (mi_size_wide[BLOCK_16X16] * mb_col + mi_size_wide[BLOCK_8X8] <
      cm->mi_cols) {
    return mi_size_wide[BLOCK_16X16] * mb_row + mi_size_wide[BLOCK_8X8] <
                   cm->mi_rows
               ? BLOCK_16X16
               : BLOCK_16X8;
  } else {
    return mi_size_wide[BLOCK_16X16] * mb_row + mi_size_wide[BLOCK_8X8] <
                   cm->mi_rows
               ? BLOCK_8X16
               : BLOCK_8X8;
  }
}

static int find_fp_qindex(aom_bit_depth_t bit_depth) {
  int i;

  for (i = 0; i < QINDEX_RANGE; ++i)
    if (av1_convert_qindex_to_q(i, bit_depth) >= FIRST_PASS_Q) break;

  if (i == QINDEX_RANGE) i--;

  return i;
}

static void set_first_pass_params(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (!cpi->refresh_alt_ref_frame &&
      (cm->current_video_frame == 0 || (cpi->frame_flags & FRAMEFLAGS_KEY))) {
    cm->frame_type = KEY_FRAME;
  } else {
    cm->frame_type = INTER_FRAME;
  }
  // Do not use periodic key frames.
  cpi->rc.frames_to_key = INT_MAX;
}

static double raw_motion_error_stdev(int *raw_motion_err_list,
                                     int raw_motion_err_counts) {
  int64_t sum_raw_err = 0;
  double raw_err_avg = 0;
  double raw_err_stdev = 0;
  if (raw_motion_err_counts == 0) return 0;

  int i;
  for (i = 0; i < raw_motion_err_counts; i++) {
    sum_raw_err += raw_motion_err_list[i];
  }
  raw_err_avg = (double)sum_raw_err / raw_motion_err_counts;
  for (i = 0; i < raw_motion_err_counts; i++) {
    raw_err_stdev += (raw_motion_err_list[i] - raw_err_avg) *
                     (raw_motion_err_list[i] - raw_err_avg);
  }
  // Calculate the standard deviation for the motion error of all the inter
  // blocks of the 0,0 motion using the last source
  // frame as the reference.
  raw_err_stdev = sqrt(raw_err_stdev / raw_motion_err_counts);
  return raw_err_stdev;
}

#define UL_INTRA_THRESH 50
#define INVALID_ROW -1
void av1_first_pass(AV1_COMP *cpi, const struct lookahead_entry *source) {
  int mb_row, mb_col;
  MACROBLOCK *const x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  TileInfo tile;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const PICK_MODE_CONTEXT *ctx =
      &cpi->td.pc_root[MAX_MIB_SIZE_LOG2 - MIN_MIB_SIZE_LOG2]->none;
  int i;

  int recon_yoffset, recon_uvoffset;
  int64_t intra_error = 0;
  int64_t frame_avg_wavelet_energy = 0;
  int64_t coded_error = 0;
  int64_t sr_coded_error = 0;

  int sum_mvr = 0, sum_mvc = 0;
  int sum_mvr_abs = 0, sum_mvc_abs = 0;
  int64_t sum_mvrs = 0, sum_mvcs = 0;
  int mvcount = 0;
  int intercount = 0;
  int second_ref_count = 0;
  const int intrapenalty = INTRA_MODE_PENALTY;
  double neutral_count;
  int intra_skip_count = 0;
  int image_data_start_row = INVALID_ROW;
  int new_mv_count = 0;
  int sum_in_vectors = 0;
  MV lastmv = kZeroMv;
  TWO_PASS *twopass = &cpi->twopass;
  int recon_y_stride, recon_uv_stride, uv_mb_height;

  YV12_BUFFER_CONFIG *const lst_yv12 = get_ref_frame_buffer(cpi, LAST_FRAME);
  YV12_BUFFER_CONFIG *gld_yv12 = get_ref_frame_buffer(cpi, GOLDEN_FRAME);
  YV12_BUFFER_CONFIG *const new_yv12 = get_frame_new_buffer(cm);
  const YV12_BUFFER_CONFIG *first_ref_buf = lst_yv12;
  double intra_factor;
  double brightness_factor;
  BufferPool *const pool = cm->buffer_pool;
  const int qindex = find_fp_qindex(cm->bit_depth);
  const int mb_scale = mi_size_wide[BLOCK_16X16];

  int *raw_motion_err_list;
  int raw_motion_err_counts = 0;
  CHECK_MEM_ERROR(
      cm, raw_motion_err_list,
      aom_calloc(cm->mb_rows * cm->mb_cols, sizeof(*raw_motion_err_list)));
  // First pass code requires valid last and new frame buffers.
  assert(new_yv12 != NULL);
  assert(frame_is_intra_only(cm) || (lst_yv12 != NULL));

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    av1_zero_array(cpi->twopass.frame_mb_stats_buf, cpi->initial_mbs);
  }
#endif

  aom_clear_system_state();

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;
  x->e_mbd.mi[0]->sb_type = BLOCK_16X16;

  intra_factor = 0.0;
  brightness_factor = 0.0;
  neutral_count = 0.0;

  set_first_pass_params(cpi);
  av1_set_quantizer(cm, qindex);

  av1_setup_block_planes(&x->e_mbd, cm->subsampling_x, cm->subsampling_y,
                         num_planes);

  av1_setup_src_planes(x, cpi->source, 0, 0, num_planes);
  av1_setup_dst_planes(xd->plane, cm->seq_params.sb_size, new_yv12, 0, 0, 0,
                       num_planes);

  if (!frame_is_intra_only(cm)) {
    av1_setup_pre_planes(xd, 0, first_ref_buf, 0, 0, NULL, num_planes);
  }

  xd->mi = cm->mi_grid_visible;
  xd->mi[0] = cm->mi;

  // Don't store luma on the fist pass since chroma is not computed
  xd->cfl.store_y = 0;
  av1_frame_init_quantizer(cpi);

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }

  av1_init_mv_probs(cm);
  av1_init_lv_map(cm);
  av1_initialize_rd_consts(cpi);

  // Tiling is ignored in the first pass.
  av1_tile_init(&tile, cm, 0, 0);

  recon_y_stride = new_yv12->y_stride;
  recon_uv_stride = new_yv12->uv_stride;
  uv_mb_height = 16 >> (new_yv12->y_height > new_yv12->uv_height);

  for (mb_row = 0; mb_row < cm->mb_rows; ++mb_row) {
    MV best_ref_mv = kZeroMv;

    // Reset above block coeffs.
    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
    recon_uvoffset = (mb_row * recon_uv_stride * uv_mb_height);

    // Set up limit values for motion vectors to prevent them extending
    // outside the UMV borders.
    x->mv_limits.row_min = -((mb_row * 16) + BORDER_MV_PIXELS_B16);
    x->mv_limits.row_max =
        ((cm->mb_rows - 1 - mb_row) * 16) + BORDER_MV_PIXELS_B16;

    for (mb_col = 0; mb_col < cm->mb_cols; ++mb_col) {
      int this_error;
      const int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);
      const BLOCK_SIZE bsize = get_bsize(cm, mb_row, mb_col);
      double log_intra;
      int level_sample;

#if CONFIG_FP_MB_STATS
      const int mb_index = mb_row * cm->mb_cols + mb_col;
#endif

      aom_clear_system_state();

      const int idx_str = xd->mi_stride * mb_row * mb_scale + mb_col * mb_scale;
      xd->mi = cm->mi_grid_visible + idx_str;
      xd->mi[0] = cm->mi + idx_str;
      xd->plane[0].dst.buf = new_yv12->y_buffer + recon_yoffset;
      xd->plane[1].dst.buf = new_yv12->u_buffer + recon_uvoffset;
      xd->plane[2].dst.buf = new_yv12->v_buffer + recon_uvoffset;
      xd->left_available = (mb_col != 0);
      xd->mi[0]->sb_type = bsize;
      xd->mi[0]->ref_frame[0] = INTRA_FRAME;
      set_mi_row_col(xd, &tile, mb_row * mb_scale, mi_size_high[bsize],
                     mb_col * mb_scale, mi_size_wide[bsize], cm->mi_rows,
                     cm->mi_cols);

      set_plane_n4(xd, mi_size_wide[bsize], mi_size_high[bsize], num_planes);

      // Do intra 16x16 prediction.
      xd->mi[0]->segment_id = 0;
      xd->lossless[xd->mi[0]->segment_id] = (qindex == 0);
      xd->mi[0]->mode = DC_PRED;
      xd->mi[0]->tx_size =
          use_dc_pred ? (bsize >= BLOCK_16X16 ? TX_16X16 : TX_8X8) : TX_4X4;
      av1_encode_intra_block_plane(cpi, x, bsize, 0, 0, mb_row * 2, mb_col * 2);
      this_error = aom_get_mb_ss(x->plane[0].src_diff);

      // Keep a record of blocks that have almost no intra error residual
      // (i.e. are in effect completely flat and untextured in the intra
      // domain). In natural videos this is uncommon, but it is much more
      // common in animations, graphics and screen content, so may be used
      // as a signal to detect these types of content.
      if (this_error < UL_INTRA_THRESH) {
        ++intra_skip_count;
      } else if ((mb_col > 0) && (image_data_start_row == INVALID_ROW)) {
        image_data_start_row = mb_row;
      }

      if (cm->use_highbitdepth) {
        switch (cm->bit_depth) {
          case AOM_BITS_8: break;
          case AOM_BITS_10: this_error >>= 4; break;
          case AOM_BITS_12: this_error >>= 8; break;
          default:
            assert(0 &&
                   "cm->bit_depth should be AOM_BITS_8, "
                   "AOM_BITS_10 or AOM_BITS_12");
            return;
        }
      }

      aom_clear_system_state();
      log_intra = log(this_error + 1.0);
      if (log_intra < 10.0)
        intra_factor += 1.0 + ((10.0 - log_intra) * 0.05);
      else
        intra_factor += 1.0;

      if (cm->use_highbitdepth)
        level_sample = CONVERT_TO_SHORTPTR(x->plane[0].src.buf)[0];
      else
        level_sample = x->plane[0].src.buf[0];
      if ((level_sample < DARK_THRESH) && (log_intra < 9.0))
        brightness_factor += 1.0 + (0.01 * (DARK_THRESH - level_sample));
      else
        brightness_factor += 1.0;

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

      int stride = x->plane[0].src.stride;
      uint8_t *buf = x->plane[0].src.buf;
      for (int r8 = 0; r8 < 2; ++r8)
        for (int c8 = 0; c8 < 2; ++c8) {
          int hbd = xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH;
          frame_avg_wavelet_energy += av1_haar_ac_sad_8x8_uint8_input(
              buf + c8 * 8 + r8 * 8 * stride, stride, hbd);
        }

#if CONFIG_FP_MB_STATS
      if (cpi->use_fp_mb_stats) {
        // initialization
        cpi->twopass.frame_mb_stats_buf[mb_index] = 0;
      }
#endif

      // Set up limit values for motion vectors to prevent them extending
      // outside the UMV borders.
      x->mv_limits.col_min = -((mb_col * 16) + BORDER_MV_PIXELS_B16);
      x->mv_limits.col_max =
          ((cm->mb_cols - 1 - mb_col) * 16) + BORDER_MV_PIXELS_B16;

      if (!frame_is_intra_only(cm)) {  // Do a motion search
        int tmp_err, motion_error, raw_motion_error;
        // Assume 0,0 motion with no mv overhead.
        MV mv = kZeroMv, tmp_mv = kZeroMv;
        struct buf_2d unscaled_last_source_buf_2d;

        xd->plane[0].pre[0].buf = first_ref_buf->y_buffer + recon_yoffset;
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          motion_error = highbd_get_prediction_error(
              bsize, &x->plane[0].src, &xd->plane[0].pre[0], xd->bd);
        } else {
          motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                              &xd->plane[0].pre[0]);
        }

        // Compute the motion error of the 0,0 motion using the last source
        // frame as the reference. Skip the further motion search on
        // reconstructed frame if this error is small.
        unscaled_last_source_buf_2d.buf =
            cpi->unscaled_last_source->y_buffer + recon_yoffset;
        unscaled_last_source_buf_2d.stride =
            cpi->unscaled_last_source->y_stride;
        if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
          raw_motion_error = highbd_get_prediction_error(
              bsize, &x->plane[0].src, &unscaled_last_source_buf_2d, xd->bd);
        } else {
          raw_motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                                  &unscaled_last_source_buf_2d);
        }

        // TODO(pengchong): Replace the hard-coded threshold
        if (raw_motion_error > 25) {
          // Test last reference frame using the previous best mv as the
          // starting point (best reference) for the search.
          first_pass_motion_search(cpi, x, &best_ref_mv, &mv, &motion_error);

          // If the current best reference mv is not centered on 0,0 then do a
          // 0,0 based search as well.
          if (!is_zero_mv(&best_ref_mv)) {
            tmp_err = INT_MAX;
            first_pass_motion_search(cpi, x, &kZeroMv, &tmp_mv, &tmp_err);

            if (tmp_err < motion_error) {
              motion_error = tmp_err;
              mv = tmp_mv;
            }
          }

          // Search in an older reference frame.
          if ((cm->current_video_frame > 1) && gld_yv12 != NULL) {
            // Assume 0,0 motion with no mv overhead.
            int gf_motion_error;

            xd->plane[0].pre[0].buf = gld_yv12->y_buffer + recon_yoffset;
            if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
              gf_motion_error = highbd_get_prediction_error(
                  bsize, &x->plane[0].src, &xd->plane[0].pre[0], xd->bd);
            } else {
              gf_motion_error = get_prediction_error(bsize, &x->plane[0].src,
                                                     &xd->plane[0].pre[0]);
            }

            first_pass_motion_search(cpi, x, &kZeroMv, &tmp_mv,
                                     &gf_motion_error);

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
          aom_clear_system_state();

          // Keep a count of cases where the inter and intra were very close
          // and very low. This helps with scene cut detection for example in
          // cropped clips with black bars at the sides or top and bottom.
          if (((this_error - intrapenalty) * 9 <= motion_error * 10) &&
              (this_error < (2 * intrapenalty))) {
            neutral_count += 1.0;
            // Also track cases where the intra is not much worse than the inter
            // and use this in limiting the GF/arf group length.
          } else if ((this_error > NCOUNT_INTRA_THRESH) &&
                     (this_error < (NCOUNT_INTRA_FACTOR * motion_error))) {
            neutral_count +=
                (double)motion_error / DOUBLE_DIVIDE_CHECK((double)this_error);
          }

          mv.row *= 8;
          mv.col *= 8;
          this_error = motion_error;
          xd->mi[0]->mode = NEWMV;
          xd->mi[0]->mv[0].as_mv = mv;
          xd->mi[0]->tx_size = TX_4X4;
          xd->mi[0]->ref_frame[0] = LAST_FRAME;
          xd->mi[0]->ref_frame[1] = NONE_FRAME;
          av1_build_inter_predictors_sby(cm, xd, mb_row * mb_scale,
                                         mb_col * mb_scale, NULL, bsize);
          av1_encode_sby_pass1(cm, x, bsize);
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
              if (mv.col > 0 && mv.col >= abs(mv.row)) {
                // right direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_RIGHT_MASK;
              } else if (mv.row < 0 && abs(mv.row) >= abs(mv.col)) {
                // up direction
                cpi->twopass.frame_mb_stats_buf[mb_index] |=
                    FPMB_MOTION_UP_MASK;
              } else if (mv.col < 0 && abs(mv.col) >= abs(mv.row)) {
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
            if (!is_equal_mv(&mv, &lastmv)) ++new_mv_count;
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
        raw_motion_err_list[raw_motion_err_counts++] = raw_motion_error;
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
    x->plane[1].src.buf +=
        uv_mb_height * x->plane[1].src.stride - uv_mb_height * cm->mb_cols;
    x->plane[2].src.buf +=
        uv_mb_height * x->plane[1].src.stride - uv_mb_height * cm->mb_cols;

    aom_clear_system_state();
  }
  const double raw_err_stdev =
      raw_motion_error_stdev(raw_motion_err_list, raw_motion_err_counts);
  aom_free(raw_motion_err_list);

  // Clamp the image start to rows/2. This number of rows is discarded top
  // and bottom as dead data so rows / 2 means the frame is blank.
  if ((image_data_start_row > cm->mb_rows / 2) ||
      (image_data_start_row == INVALID_ROW)) {
    image_data_start_row = cm->mb_rows / 2;
  }
  // Exclude any image dead zone
  if (image_data_start_row > 0) {
    intra_skip_count =
        AOMMAX(0, intra_skip_count - (image_data_start_row * cm->mb_cols * 2));
  }

  {
    FIRSTPASS_STATS fps;
    // The minimum error here insures some bit allocation to frames even
    // in static regions. The allocation per MB declines for larger formats
    // where the typical "real" energy per MB also falls.
    // Initial estimate here uses sqrt(mbs) to define the min_err, where the
    // number of mbs is proportional to the image area.
    const int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cpi->common.MBs;
    const double min_err = 200 * sqrt(num_mbs);

    intra_factor = intra_factor / (double)num_mbs;
    brightness_factor = brightness_factor / (double)num_mbs;
    fps.weight = intra_factor * brightness_factor;

    fps.frame = cm->current_video_frame;
    fps.coded_error = (double)(coded_error >> 8) + min_err;
    fps.sr_coded_error = (double)(sr_coded_error >> 8) + min_err;
    fps.intra_error = (double)(intra_error >> 8) + min_err;
    fps.frame_avg_wavelet_energy = (double)frame_avg_wavelet_energy;
    fps.count = 1.0;
    fps.pcnt_inter = (double)intercount / num_mbs;
    fps.pcnt_second_ref = (double)second_ref_count / num_mbs;
    fps.pcnt_neutral = (double)neutral_count / num_mbs;
    fps.intra_skip_pct = (double)intra_skip_count / num_mbs;
    fps.inactive_zone_rows = (double)image_data_start_row;
    fps.inactive_zone_cols = (double)0;  // TODO(paulwilkins): fix
    fps.raw_error_stdev = raw_err_stdev;

    if (mvcount > 0) {
      fps.MVr = (double)sum_mvr / mvcount;
      fps.mvr_abs = (double)sum_mvr_abs / mvcount;
      fps.MVc = (double)sum_mvc / mvcount;
      fps.mvc_abs = (double)sum_mvc_abs / mvcount;
      fps.MVrv =
          ((double)sum_mvrs - ((double)sum_mvr * sum_mvr / mvcount)) / mvcount;
      fps.MVcv =
          ((double)sum_mvcs - ((double)sum_mvc * sum_mvc / mvcount)) / mvcount;
      fps.mv_in_out_count = (double)sum_in_vectors / (mvcount * 2);
      fps.new_mv_count = new_mv_count;
      fps.pcnt_motion = (double)mvcount / num_mbs;
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
      output_fpmb_stats(twopass->frame_mb_stats_buf, cpi->initial_mbs,
                        cpi->output_pkt_list);
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
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]],
                 cm->ref_frame_map[cpi->ref_fb_idx[LAST_FRAME - 1]]);
    }
    twopass->sr_update_lag = 1;
  } else {
    ++twopass->sr_update_lag;
  }

  aom_extend_frame_borders(new_yv12, num_planes);

  // The frame we just compressed now becomes the last frame.
  ref_cnt_fb(pool->frame_bufs,
             &cm->ref_frame_map[cpi->ref_fb_idx[LAST_FRAME - 1]],
             cm->new_fb_idx);

  // Special case for the first frame. Copy into the GF buffer as a second
  // reference.
  if (cm->current_video_frame == 0 &&
      cpi->ref_fb_idx[GOLDEN_FRAME - 1] != INVALID_IDX) {
    ref_cnt_fb(pool->frame_bufs,
               &cm->ref_frame_map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]],
               cm->ref_frame_map[cpi->ref_fb_idx[LAST_FRAME - 1]]);
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
}

static double calc_correction_factor(double err_per_mb, double err_divisor,
                                     double pt_low, double pt_high, int q,
                                     aom_bit_depth_t bit_depth) {
  const double error_term = err_per_mb / err_divisor;

  // Adjustment based on actual quantizer to power term.
  const double power_term =
      AOMMIN(av1_convert_qindex_to_q(q, bit_depth) * 0.01 + pt_low, pt_high);

  // Calculate correction factor.
  if (power_term < 1.0) assert(error_term >= 0.0);

  return fclamp(pow(error_term, power_term), 0.05, 5.0);
}

#define ERR_DIVISOR 100.0
static int get_twopass_worst_quality(const AV1_COMP *cpi,
                                     const double section_err,
                                     double inactive_zone,
                                     int section_target_bandwidth,
                                     double group_weight_factor) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  inactive_zone = fclamp(inactive_zone, 0.0, 1.0);

  if (section_target_bandwidth <= 0) {
    return rc->worst_quality;  // Highest value allowed
  } else {
    const int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cpi->common.MBs;
    const int active_mbs = AOMMAX(1, num_mbs - (int)(num_mbs * inactive_zone));
    const double av_err_per_mb = section_err / active_mbs;
    const double speed_term = 1.0;
    double ediv_size_correction;
    const int target_norm_bits_per_mb =
        (int)((uint64_t)section_target_bandwidth << BPER_MB_NORMBITS) /
        active_mbs;
    int q;

    // Larger image formats are expected to be a little harder to code
    // relatively given the same prediction error score. This in part at
    // least relates to the increased size and hence coding overheads of
    // motion vectors. Some account of this is made through adjustment of
    // the error divisor.
    ediv_size_correction =
        AOMMAX(0.2, AOMMIN(5.0, get_linear_size_factor(cpi)));
    if (ediv_size_correction < 1.0)
      ediv_size_correction = -(1.0 / ediv_size_correction);
    ediv_size_correction *= 4.0;

    // Try and pick a max Q that will be high enough to encode the
    // content at the given rate.
    for (q = rc->best_quality; q < rc->worst_quality; ++q) {
      const double factor = calc_correction_factor(
          av_err_per_mb, ERR_DIVISOR - ediv_size_correction, FACTOR_PT_LOW,
          FACTOR_PT_HIGH, q, cpi->common.bit_depth);
      const int bits_per_mb = av1_rc_bits_per_mb(
          INTER_FRAME, q, factor * speed_term * group_weight_factor,
          cpi->common.bit_depth);
      if (bits_per_mb <= target_norm_bits_per_mb) break;
    }

    // Restriction on active max q for constrained quality mode.
    if (cpi->oxcf.rc_mode == AOM_CQ) q = AOMMAX(q, oxcf->cq_level);
    return q;
  }
}

static void setup_rf_level_maxq(AV1_COMP *cpi) {
  int i;
  RATE_CONTROL *const rc = &cpi->rc;
  for (i = INTER_NORMAL; i < RATE_FACTOR_LEVELS; ++i) {
    int qdelta = av1_frame_type_qdelta(cpi, i, rc->worst_quality);
    rc->rf_level_maxq[i] = AOMMAX(rc->worst_quality + qdelta, rc->best_quality);
  }
}

void av1_init_second_pass(AV1_COMP *cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  double frame_rate;
  FIRSTPASS_STATS *stats;

  zero_stats(&twopass->total_stats);
  zero_stats(&twopass->total_left_stats);

  if (!twopass->stats_in_end) return;

  stats = &twopass->total_stats;

  *stats = *twopass->stats_in_end;
  twopass->total_left_stats = *stats;

  frame_rate = 10000000.0 * stats->count / stats->duration;
  // Each frame can have a different duration, as the frame rate in the source
  // isn't guaranteed to be constant. The frame rate prior to the first frame
  // encoded in the second pass is a guess. However, the sum duration is not.
  // It is calculated based on the actual durations of all frames from the
  // first pass.
  av1_new_framerate(cpi, frame_rate);
  twopass->bits_left =
      (int64_t)(stats->duration * oxcf->target_bandwidth / 10000000.0);

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  // Scan the first pass file and calculate a modified total error based upon
  // the bias/power function used to allocate bits.
  {
    const double avg_error =
        stats->coded_error / DOUBLE_DIVIDE_CHECK(stats->count);
    const FIRSTPASS_STATS *s = twopass->stats_in;
    double modified_error_total = 0.0;
    twopass->modified_error_min =
        (avg_error * oxcf->two_pass_vbrmin_section) / 100;
    twopass->modified_error_max =
        (avg_error * oxcf->two_pass_vbrmax_section) / 100;
    while (s < twopass->stats_in_end) {
      modified_error_total += calculate_modified_err(cpi, twopass, oxcf, s);
      ++s;
    }
    twopass->modified_error_left = modified_error_total;
  }

  // Reset the vbr bits off target counters
  cpi->rc.vbr_bits_off_target = 0;
  cpi->rc.vbr_bits_off_target_fast = 0;

  cpi->rc.rate_error_estimate = 0;

  // Static sequence monitor variables.
  twopass->kf_zeromotion_pct = 100;
  twopass->last_kfgroup_zeromotion_pct = 100;

  if (oxcf->resize_mode != RESIZE_NONE) {
    setup_rf_level_maxq(cpi);
  }
}

#define SR_DIFF_PART 0.0015
#define MOTION_AMP_PART 0.003
#define INTRA_PART 0.005
#define DEFAULT_DECAY_LIMIT 0.75
#define LOW_SR_DIFF_TRHESH 0.1
#define SR_DIFF_MAX 128.0

static double get_sr_decay_rate(const AV1_COMP *cpi,
                                const FIRSTPASS_STATS *frame) {
  const int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE) ? cpi->initial_mbs
                                                             : cpi->common.MBs;
  double sr_diff = (frame->sr_coded_error - frame->coded_error) / num_mbs;
  double sr_decay = 1.0;
  double modified_pct_inter;
  double modified_pcnt_intra;
  const double motion_amplitude_factor =
      frame->pcnt_motion * ((frame->mvc_abs + frame->mvr_abs) / 2);

  modified_pct_inter = frame->pcnt_inter;
  if ((frame->intra_error / DOUBLE_DIVIDE_CHECK(frame->coded_error)) <
      (double)NCOUNT_FRAME_II_THRESH) {
    modified_pct_inter = frame->pcnt_inter - frame->pcnt_neutral;
  }
  modified_pcnt_intra = 100 * (1.0 - modified_pct_inter);

  if ((sr_diff > LOW_SR_DIFF_TRHESH)) {
    sr_diff = AOMMIN(sr_diff, SR_DIFF_MAX);
    sr_decay = 1.0 - (SR_DIFF_PART * sr_diff) -
               (MOTION_AMP_PART * motion_amplitude_factor) -
               (INTRA_PART * modified_pcnt_intra);
  }
  return AOMMAX(sr_decay, AOMMIN(DEFAULT_DECAY_LIMIT, modified_pct_inter));
}

// This function gives an estimate of how badly we believe the prediction
// quality is decaying from frame to frame.
static double get_zero_motion_factor(const AV1_COMP *cpi,
                                     const FIRSTPASS_STATS *frame) {
  const double zero_motion_pct = frame->pcnt_inter - frame->pcnt_motion;
  double sr_decay = get_sr_decay_rate(cpi, frame);
  return AOMMIN(sr_decay, zero_motion_pct);
}

#define ZM_POWER_FACTOR 0.75

static double get_prediction_decay_rate(const AV1_COMP *cpi,
                                        const FIRSTPASS_STATS *next_frame) {
  const double sr_decay_rate = get_sr_decay_rate(cpi, next_frame);
  const double zero_motion_factor =
      (0.95 * pow((next_frame->pcnt_inter - next_frame->pcnt_motion),
                  ZM_POWER_FACTOR));

  return AOMMAX(zero_motion_factor,
                (sr_decay_rate + ((1.0 - sr_decay_rate) * zero_motion_factor)));
}

// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static int detect_transition_to_still(AV1_COMP *cpi, int frame_interval,
                                      int still_interval,
                                      double loop_decay_rate,
                                      double last_decay_rate) {
  TWO_PASS *const twopass = &cpi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;

  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > rc->min_gf_interval && loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int j;

    // Look ahead a few frames to see if static condition persists...
    for (j = 0; j < still_interval; ++j) {
      const FIRSTPASS_STATS *stats = &twopass->stats_in[j];
      if (stats >= twopass->stats_in_end) break;

      if (stats->pcnt_inter - stats->pcnt_motion < 0.999) break;
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
    const double mvr_ratio =
        fabs(stats->mvr_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVr));
    const double mvc_ratio =
        fabs(stats->mvc_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVc));

    *mv_ratio_accumulator +=
        pct * (mvr_ratio < stats->mvr_abs ? mvr_ratio : stats->mvr_abs);
    *mv_ratio_accumulator +=
        pct * (mvc_ratio < stats->mvc_abs ? mvc_ratio : stats->mvc_abs);
  }
}

#define BASELINE_ERR_PER_MB 1000.0
static double calc_frame_boost(AV1_COMP *cpi, const FIRSTPASS_STATS *this_frame,
                               double this_frame_mv_in_out, double max_boost) {
  double frame_boost;
  const double lq = av1_convert_qindex_to_q(
      cpi->rc.avg_frame_qindex[INTER_FRAME], cpi->common.bit_depth);
  const double boost_q_correction = AOMMIN((0.5 + (lq * 0.015)), 1.5);
  int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE) ? cpi->initial_mbs
                                                       : cpi->common.MBs;

  // Correct for any inactive region in the image
  num_mbs = (int)AOMMAX(1, num_mbs * calculate_active_area(cpi, this_frame));

  // Underlying boost factor is based on inter error ratio.
  frame_boost = (BASELINE_ERR_PER_MB * num_mbs) /
                DOUBLE_DIVIDE_CHECK(this_frame->coded_error);
  frame_boost = frame_boost * BOOST_FACTOR * boost_q_correction;

  // Increase boost for frames where new data coming into frame (e.g. zoom out).
  // Slightly reduce boost if there is a net balance of motion out of the frame
  // (zoom in). The range for this_frame_mv_in_out is -1.0 to +1.0.
  if (this_frame_mv_in_out > 0.0)
    frame_boost += frame_boost * (this_frame_mv_in_out * 2.0);
  // In the extreme case the boost is halved.
  else
    frame_boost += frame_boost * (this_frame_mv_in_out / 2.0);

  return AOMMIN(frame_boost, max_boost * boost_q_correction);
}

static int calc_arf_boost(AV1_COMP *cpi, int offset, int f_frames, int b_frames,
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
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(
        this_frame, &this_frame_mv_in_out, &mv_in_out_accumulator,
        &abs_mv_in_out_accumulator, &mv_ratio_accumulator);

    // We want to discount the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(cpi, this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                              ? MIN_DECAY_FACTOR
                              : decay_accumulator;
    }

    boost_score +=
        decay_accumulator *
        calc_frame_boost(cpi, this_frame, this_frame_mv_in_out, GF_MAX_BOOST);
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
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(
        this_frame, &this_frame_mv_in_out, &mv_in_out_accumulator,
        &abs_mv_in_out_accumulator, &mv_ratio_accumulator);

    // We want to discount the the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, i + offset) ||
                     detect_flash(twopass, i + offset + 1);

    // Cumulative effect of prediction quality decay.
    if (!flash_detected) {
      decay_accumulator *= get_prediction_decay_rate(cpi, this_frame);
      decay_accumulator = decay_accumulator < MIN_DECAY_FACTOR
                              ? MIN_DECAY_FACTOR
                              : decay_accumulator;
    }

    boost_score +=
        decay_accumulator *
        calc_frame_boost(cpi, this_frame, this_frame_mv_in_out, GF_MAX_BOOST);
  }
  *b_boost = (int)boost_score;

  arf_boost = (*f_boost + *b_boost);
  if (arf_boost < ((b_frames + f_frames) * 20))
    arf_boost = ((b_frames + f_frames) * 20);
  arf_boost = AOMMAX(arf_boost, MIN_ARF_GF_BOOST);

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
static int64_t calculate_total_gf_group_bits(AV1_COMP *cpi,
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
  total_group_bits = (total_group_bits < 0)
                         ? 0
                         : (total_group_bits > twopass->kf_group_bits)
                               ? twopass->kf_group_bits
                               : total_group_bits;

  // Clip based on user supplied data rate variability limit.
  if (total_group_bits > (int64_t)max_bits * rc->baseline_gf_interval)
    total_group_bits = (int64_t)max_bits * rc->baseline_gf_interval;

  return total_group_bits;
}

// Calculate the number bits extra to assign to boosted frames in a group.
static int calculate_boost_bits(int frame_count, int boost,
                                int64_t total_group_bits) {
  int allocation_chunks;

  // return 0 for invalid inputs (could arise e.g. through rounding errors)
  if (!boost || (total_group_bits <= 0) || (frame_count <= 0)) return 0;

  allocation_chunks = (frame_count * 100) + boost;

  // Prevent overflow.
  if (boost > 1023) {
    int divisor = boost >> 10;
    boost /= divisor;
    allocation_chunks /= divisor;
  }

  // Calculate the number of extra bits for use in the boosted frame or frames.
  return AOMMAX((int)(((int64_t)boost * total_group_bits) / allocation_chunks),
                0);
}

#if USE_GF16_MULTI_LAYER
// === GF Group of 16 ===
#define GF_INTERVAL_16 16
#define GF_FRAME_PARAMS (REF_FRAMES + 5)

// GF Group of 16: multi-layer hierarchical coding structure
//   1st Layer: Frame 0 and Frame 16 (ALTREF)
//   2nd Layer: Frame 8 (ALTREF2)
//   3rd Layer: Frame 4 and 12 (ALTREF2)
//   4th Layer: Frame 2, 6, 10, and 14 (BWDREF)
//   5th Layer: Frame 1, 3, 5, 7, 9, 11, 13, and 15
static const unsigned char gf16_multi_layer_params[][GF_FRAME_PARAMS] = {
  // gf_group->index: coding order
  // (Frame #)      : display order
  {
      // gf_group->index == 0 (Frame 0)
      OVERLAY_UPDATE,  // update_type
      0,               // arf_src_offset
      0,               // brf_src_offset
      // References (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF_FRAME,  // Index (current) of reference to get updated
      GOLDEN_FRAME   // cpi->refresh_golden_frame = 1
  },
  {
      // gf_group->index == 1 (Frame 16)
      ARF_UPDATE,          // update_type
      GF_INTERVAL_16 - 1,  // arf_src_offset
      0,                   // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      ALTREF_FRAME,   // cpi->alt_fb_idx ===> cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      GOLDEN_FRAME,   // cpi->gld_fb_idx ===> cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF_FRAME,  // Index (current) of reference to get updated
      ALTREF_FRAME   // cpi->refresh_alt_ref_frame = 1
  },
  {
      // gf_group->index == 2 (Frame 8)
      INTNL_ARF_UPDATE,           // update_type
      (GF_INTERVAL_16 >> 1) - 1,  // arf_src_offset
      0,                          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF2_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME   // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 3 (Frame 4)
      INTNL_ARF_UPDATE,           // update_type
      (GF_INTERVAL_16 >> 2) - 1,  // arf_src_offset
      0,                          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx
                      // (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx
                      // (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF2_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME   // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 4 (Frame 2)
      BRF_UPDATE,  // update_type
      0,           // arf_src_offset
      1,           // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx
                      // (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx
                      // (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      REF_FRAMES,   // Index (current) of reference to get updated
      BWDREF_FRAME  // cpi->refresh_bwd_ref_frame = 1
  },
  {
      // gf_group->index == 5 (Frame 1)
      LAST_BIPRED_UPDATE,  // update_type
      0,                   // arf_src_offset
      0,                   // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->alt_fb_idx (ALTREF_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx ===> cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 6 (Frame 3)
      LF_UPDATE,  // update_type
      0,          // arf_src_offset
      0,          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx ===> cpi->alt2_fb_idx (ALTREF2_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx ===> cpi->alt_fb_idx (ALTREF_FRAME)
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 7 (Frame 4 - OVERLAY)
      INTNL_OVERLAY_UPDATE,  // update_type
      0,                     // arf_src_offset
      0,                     // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      BWDREF_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME  // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 8 (Frame 6)
      BRF_UPDATE,  // update_type
      0,           // arf_src_offset
      1,           // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx -> cpi->bwd_fb_idx (BWDREF_FRAME)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF2_FRAME,  // Index (current) of reference to get updated
      BWDREF_FRAME    // cpi->refresh_bwd_frame = 1
  },
  {
      // gf_group->index == 9 (Frame 5)
      LAST_BIPRED_UPDATE,  // update_type
      0,                   // arf_src_offset
      0,                   // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 10 (Frame 7)
      LF_UPDATE,  // update_type
      0,          // arf_src_offset
      0,          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 11 (Frame 8 - OVERLAY)
      INTNL_OVERLAY_UPDATE,  // update_type
      0,                     // arf_src_offset
      0,                     // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      BWDREF_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME  // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 12 (Frame 12)
      INTNL_ARF_UPDATE,           // update_type
      (GF_INTERVAL_16 >> 2) - 1,  // arf_src_offset
      0,                          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    //  cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      //  cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF2_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME   // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 13 (Frame 10)
      BRF_UPDATE,  // update_type
      0,           // arf_src_offset
      1,           // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF2_FRAME,  // Index (current) of reference to get updated
      BWDREF_FRAME    // cpi->refresh_bwd_frame = 1
  },
  {
      // gf_group->index == 14 (Frame 9)
      LAST_BIPRED_UPDATE,  // update_type
      0,                   // arf_src_offset
      0,                   // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 15 (Frame 11)
      LF_UPDATE,  // update_type
      0,          // arf_src_offset
      0,          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx ===> cpi->bwd_fb_idx (BWDREF_FRAME)
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 16 (Frame 12 - OVERLAY)
      INTNL_OVERLAY_UPDATE,  // update_type
      0,                     // arf_src_offset
      0,                     // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      BWDREF_FRAME,  // Index (current) of reference to get updated
      ALTREF2_FRAME  // cpi->refresh_alt2_ref_frame = 1
  },
  {
      // gf_group->index == 17 (Frame 14)
      BRF_UPDATE,  // update_type
      0,           // arf_src_offset
      1,           // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      BWDREF_FRAME,  // Index (current) of reference to get updated
      BWDREF_FRAME   // cpi->refresh_bwd_frame = 1
  },
  {
      // gf_group->index == 18 (Frame 13)
      LAST_BIPRED_UPDATE,  // update_type
      0,                   // arf_src_offset
      0,                   // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 19 (Frame 15)
      LF_UPDATE,  // update_type
      0,          // arf_src_offset
      0,          // brf_src_offset
      // Reference frame indexes (previous ===> current)
      BWDREF_FRAME,   // cpi->bwd_fb_idx ===> cpi->lst_fb_idxes[LAST_FRAME -
                      // LAST_FRAME]
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      LAST3_FRAME,  // Index (current) of reference to get updated
      LAST_FRAME    // cpi->refresh_last_frame = 1
  },
  {
      // gf_group->index == 20 (Frame 16 - OVERLAY: Belonging to the next GF
      // group)
      OVERLAY_UPDATE,  // update_type
      0,               // arf_src_offset
      0,               // brf_src_offset
      // Reference frame indexes (previous ===> current)
      LAST3_FRAME,    // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME]
      LAST_FRAME,     // cpi->lst_fb_idxes[LAST_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME]
      LAST2_FRAME,    // cpi->lst_fb_idxes[LAST2_FRAME - LAST_FRAME] ===>
                      // cpi->lst_fb_idxes[LAST3_FRAME - LAST_FRAME]
      GOLDEN_FRAME,   // cpi->gld_fb_idx (GOLDEN_FRAME)
      BWDREF_FRAME,   // cpi->bwd_fb_idx (BWDREF_FRAME)
      ALTREF2_FRAME,  // cpi->alt2_fb_idx (ALTREF2_FRAME)
      ALTREF_FRAME,   // cpi->alt_fb_idx (ALTREF_FRAME)
      REF_FRAMES,     // cpi->ext_fb_idx (extra ref frame)
      // Refreshment (index, flag)
      ALTREF_FRAME,  // Index (current) of reference to get updated
      GOLDEN_FRAME   // cpi->refresh_golden_frame = 1
  }
};

// === GF Group of 16 ===
static void define_gf_group_structure_16(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  const int key_frame = cpi->common.frame_type == KEY_FRAME;

  assert(rc->baseline_gf_interval == GF_INTERVAL_16);

  // Total number of frames to consider for GF group of 16:
  //   = GF group interval + number of OVERLAY's
  //   = rc->baseline_gf_interval + MAX_EXT_ARFS + 1 + 1
  // NOTE: The OVERLAY frame for the next GF group also needs to consider to
  //       prepare for the reference frame index mapping.

  const int gf_update_frames = rc->baseline_gf_interval + MAX_EXT_ARFS + 2;

  for (int frame_index = 0; frame_index < gf_update_frames; ++frame_index) {
    int param_idx = 0;

    // Treat KEY_FRAME differently
    if (frame_index == 0 && key_frame) {
      gf_group->update_type[frame_index] = KF_UPDATE;

      gf_group->rf_level[frame_index] = KF_STD;
      gf_group->arf_src_offset[frame_index] = 0;
      gf_group->brf_src_offset[frame_index] = 0;
      gf_group->bidir_pred_enabled[frame_index] = 0;
      for (int ref_idx = 0; ref_idx < REF_FRAMES; ++ref_idx)
        gf_group->ref_fb_idx_map[frame_index][ref_idx] = ref_idx;
      gf_group->refresh_idx[frame_index] = cpi->ref_fb_idx[LAST_FRAME - 1];
      gf_group->refresh_flag[frame_index] = cpi->ref_fb_idx[LAST_FRAME - 1];

      continue;
    }

    // == update_type ==
    gf_group->update_type[frame_index] =
        gf16_multi_layer_params[frame_index][param_idx++];

    // == rf_level ==
    // Derive rf_level from update_type
    switch (gf_group->update_type[frame_index]) {
      case LF_UPDATE: gf_group->rf_level[frame_index] = INTER_NORMAL; break;
      case ARF_UPDATE: gf_group->rf_level[frame_index] = GF_ARF_LOW; break;
      case OVERLAY_UPDATE:
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        break;
      case BRF_UPDATE: gf_group->rf_level[frame_index] = GF_ARF_LOW; break;
      case LAST_BIPRED_UPDATE:
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        break;
      case BIPRED_UPDATE: gf_group->rf_level[frame_index] = INTER_NORMAL; break;
      case INTNL_ARF_UPDATE:
        gf_group->rf_level[frame_index] = GF_ARF_LOW;
        break;
      case INTNL_OVERLAY_UPDATE:
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        break;
      default: gf_group->rf_level[frame_index] = INTER_NORMAL; break;
    }

    // == arf_src_offset ==
    gf_group->arf_src_offset[frame_index] =
        gf16_multi_layer_params[frame_index][param_idx++];

    // == brf_src_offset ==
    gf_group->brf_src_offset[frame_index] =
        gf16_multi_layer_params[frame_index][param_idx++];

    // == bidir_pred_enabled ==
    // Derive bidir_pred_enabled from bidir_src_offset
    gf_group->bidir_pred_enabled[frame_index] =
        gf_group->brf_src_offset[frame_index] ? 1 : 0;

    // == ref_fb_idx_map ==
    for (int ref_idx = 0; ref_idx < REF_FRAMES; ++ref_idx)
      gf_group->ref_fb_idx_map[frame_index][ref_idx] =
          gf16_multi_layer_params[frame_index][param_idx++];

    // == refresh_idx ==
    gf_group->refresh_idx[frame_index] =
        gf16_multi_layer_params[frame_index][param_idx++];

    // == refresh_flag ==
    gf_group->refresh_flag[frame_index] =
        gf16_multi_layer_params[frame_index][param_idx];
  }

  // Mark the ARF_UPDATE / INTNL_ARF_UPDATE and OVERLAY_UPDATE /
  // INTNL_OVERLAY_UPDATE for rate allocation
  // NOTE: Indexes are designed in the display order backward:
  //       ALT[3] .. ALT[2] .. ALT[1] .. ALT[0],
  //       but their coding order is as follows:
  // ALT0-ALT2-ALT3 .. OVERLAY3 .. OVERLAY2-ALT1 .. OVERLAY1 .. OVERLAY0

  const int num_arfs_in_gf = cpi->num_extra_arfs + 1;
  const int sub_arf_interval = rc->baseline_gf_interval / num_arfs_in_gf;

  // == arf_pos_for_ovrly ==: Position for OVERLAY
  for (int arf_idx = 0; arf_idx < num_arfs_in_gf; arf_idx++) {
    const int prior_num_arfs =
        (arf_idx <= 1) ? num_arfs_in_gf : (num_arfs_in_gf - 1);
    cpi->arf_pos_for_ovrly[arf_idx] =
        sub_arf_interval * (num_arfs_in_gf - arf_idx) + prior_num_arfs;
  }

  // == arf_pos_in_gf ==: Position for ALTREF
  cpi->arf_pos_in_gf[0] = 1;
  cpi->arf_pos_in_gf[1] = cpi->arf_pos_for_ovrly[2] + 1;
  cpi->arf_pos_in_gf[2] = 2;
  cpi->arf_pos_in_gf[3] = 3;

  // == arf_update_idx ==
  // == arf_ref_idx ==
  // NOTE: Due to the hierarchical nature of GF16, these two parameters only
  //       relect the index to the nearest future overlay.
  int start_frame_index = 0;
  for (int arf_idx = (num_arfs_in_gf - 1); arf_idx >= 0; --arf_idx) {
    const int end_frame_index = cpi->arf_pos_for_ovrly[arf_idx];
    for (int frame_index = start_frame_index; frame_index <= end_frame_index;
         ++frame_index) {
      gf_group->arf_update_idx[frame_index] = arf_idx;
      gf_group->arf_ref_idx[frame_index] = arf_idx;
    }
    start_frame_index = end_frame_index + 1;
  }
}
#endif  // USE_GF16_MULTI_LAYER

static void define_gf_group_structure(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;

#if USE_GF16_MULTI_LAYER
  if (rc->baseline_gf_interval == 16) {
    define_gf_group_structure_16(cpi);
    return;
  }
#endif  // USE_GF16_MULTI_LAYER

  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  int i;
  int frame_index = 0;
  const int key_frame = cpi->common.frame_type == KEY_FRAME;

  // The use of bi-predictive frames are only enabled when following 3
  // conditions are met:
  // (1) ALTREF is enabled;
  // (2) The bi-predictive group interval is at least 2; and
  // (3) The bi-predictive group interval is strictly smaller than the
  //     golden group interval.
  const int is_bipred_enabled =
      cpi->extra_arf_allowed && rc->source_alt_ref_pending &&
      rc->bipred_group_interval &&
      rc->bipred_group_interval <=
          (rc->baseline_gf_interval - rc->source_alt_ref_pending);
  int bipred_group_end = 0;
  int bipred_frame_index = 0;

  const unsigned char ext_arf_interval =
      (unsigned char)(rc->baseline_gf_interval / (cpi->num_extra_arfs + 1) - 1);
  int which_arf = cpi->num_extra_arfs;
  int subgroup_interval[MAX_EXT_ARFS + 1];
  int is_sg_bipred_enabled = is_bipred_enabled;
  int accumulative_subgroup_interval = 0;

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  // === [frame_index == 0] ===
  if (!key_frame) {
    if (rc->source_alt_ref_active) {
      gf_group->update_type[frame_index] = OVERLAY_UPDATE;
      gf_group->rf_level[frame_index] = INTER_NORMAL;
    } else {
      gf_group->update_type[frame_index] = GF_UPDATE;
      gf_group->rf_level[frame_index] = GF_ARF_STD;
    }
    gf_group->arf_update_idx[frame_index] = 0;
    gf_group->arf_ref_idx[frame_index] = 0;
  }

  gf_group->bidir_pred_enabled[frame_index] = 0;
  gf_group->brf_src_offset[frame_index] = 0;

  frame_index++;

  bipred_frame_index++;

  // === [frame_index == 1] ===
  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = ARF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
    gf_group->arf_src_offset[frame_index] =
        (unsigned char)(rc->baseline_gf_interval - 1);

    gf_group->arf_update_idx[frame_index] = 0;
    gf_group->arf_ref_idx[frame_index] = 0;

    gf_group->bidir_pred_enabled[frame_index] = 0;
    gf_group->brf_src_offset[frame_index] = 0;
    // NOTE: "bidir_pred_frame_index" stays unchanged for ARF_UPDATE frames.

    // Work out the ARFs' positions in this gf group
    // NOTE(weitinglin): ALT_REFs' are indexed inversely, but coded in display
    // order (except for the original ARF). In the example of three ALT_REF's,
    // We index ALTREF's as: KEY ----- ALT2 ----- ALT1 ----- ALT0
    // but code them in the following order:
    // KEY-ALT0-ALT2 ----- OVERLAY2-ALT1 ----- OVERLAY1 ----- OVERLAY0
    //
    // arf_pos_for_ovrly[]: Position for OVERLAY
    // arf_pos_in_gf[]:     Position for ALTREF
    cpi->arf_pos_for_ovrly[0] = frame_index + cpi->num_extra_arfs +
                                gf_group->arf_src_offset[frame_index] + 1;
    for (i = 0; i < cpi->num_extra_arfs; ++i) {
      cpi->arf_pos_for_ovrly[i + 1] =
          frame_index + (cpi->num_extra_arfs - i) * (ext_arf_interval + 2);
      subgroup_interval[i] = cpi->arf_pos_for_ovrly[i] -
                             cpi->arf_pos_for_ovrly[i + 1] - (i == 0 ? 1 : 2);
    }
    subgroup_interval[cpi->num_extra_arfs] =
        cpi->arf_pos_for_ovrly[cpi->num_extra_arfs] - frame_index -
        (cpi->num_extra_arfs == 0 ? 1 : 2);

    ++frame_index;

    // Insert an extra ARF
    // === [frame_index == 2] ===
    if (cpi->num_extra_arfs) {
      gf_group->update_type[frame_index] = INTNL_ARF_UPDATE;
      gf_group->rf_level[frame_index] = GF_ARF_LOW;
      gf_group->arf_src_offset[frame_index] = ext_arf_interval;

      gf_group->arf_update_idx[frame_index] = which_arf;
      gf_group->arf_ref_idx[frame_index] = 0;
      ++frame_index;
    }
    accumulative_subgroup_interval += subgroup_interval[cpi->num_extra_arfs];
  }

  for (i = 0; i < rc->baseline_gf_interval - rc->source_alt_ref_pending; ++i) {
    gf_group->arf_update_idx[frame_index] = which_arf;
    gf_group->arf_ref_idx[frame_index] = which_arf;

    // If we are going to have ARFs, check whether we can have BWDREF in this
    // subgroup, and further, whether we can have ARF subgroup which contains
    // the BWDREF subgroup but contained within the GF group:
    //
    // GF group --> ARF subgroup --> BWDREF subgroup
    if (rc->source_alt_ref_pending) {
      is_sg_bipred_enabled =
          is_bipred_enabled &&
          (subgroup_interval[which_arf] > rc->bipred_group_interval);
    }

    // NOTE: BIDIR_PRED is only enabled when the length of the bi-predictive
    //       frame group interval is strictly smaller than that of the GOLDEN
    //       FRAME group interval.
    // TODO(zoeliu): Currently BIDIR_PRED is only enabled when alt-ref is on.
    if (is_sg_bipred_enabled && !bipred_group_end) {
      const int cur_brf_src_offset = rc->bipred_group_interval - 1;

      if (bipred_frame_index == 1) {
        // --- BRF_UPDATE ---
        gf_group->update_type[frame_index] = BRF_UPDATE;
        gf_group->rf_level[frame_index] = GF_ARF_LOW;
        gf_group->brf_src_offset[frame_index] = cur_brf_src_offset;
      } else if (bipred_frame_index == rc->bipred_group_interval) {
        // --- LAST_BIPRED_UPDATE ---
        gf_group->update_type[frame_index] = LAST_BIPRED_UPDATE;
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        gf_group->brf_src_offset[frame_index] = 0;

        // Reset the bi-predictive frame index.
        bipred_frame_index = 0;
      } else {
        // --- BIPRED_UPDATE ---
        gf_group->update_type[frame_index] = BIPRED_UPDATE;
        gf_group->rf_level[frame_index] = INTER_NORMAL;
        gf_group->brf_src_offset[frame_index] = 0;
      }
      gf_group->bidir_pred_enabled[frame_index] = 1;

      bipred_frame_index++;
      // Check whether the next bi-predictive frame group would entirely be
      // included within the current golden frame group.
      // In addition, we need to avoid coding a BRF right before an ARF.
      if (bipred_frame_index == 1 &&
          (i + 2 + cur_brf_src_offset) >= accumulative_subgroup_interval) {
        bipred_group_end = 1;
      }
    } else {
      gf_group->update_type[frame_index] = LF_UPDATE;
      gf_group->rf_level[frame_index] = INTER_NORMAL;
      gf_group->bidir_pred_enabled[frame_index] = 0;
      gf_group->brf_src_offset[frame_index] = 0;
    }

    ++frame_index;

    // Check if we need to update the ARF.
    if (is_sg_bipred_enabled && cpi->num_extra_arfs && which_arf > 0 &&
        frame_index > cpi->arf_pos_for_ovrly[which_arf]) {
      --which_arf;
      accumulative_subgroup_interval += subgroup_interval[which_arf] + 1;

      // Meet the new subgroup; Reset the bipred_group_end flag.
      bipred_group_end = 0;
      // Insert another extra ARF after the overlay frame
      if (which_arf) {
        gf_group->update_type[frame_index] = INTNL_ARF_UPDATE;
        gf_group->rf_level[frame_index] = GF_ARF_LOW;
        gf_group->arf_src_offset[frame_index] = ext_arf_interval;

        gf_group->arf_update_idx[frame_index] = which_arf;
        gf_group->arf_ref_idx[frame_index] = 0;
        ++frame_index;
      }
    }
  }

  // NOTE: We need to configure the frame at the end of the sequence + 1 that
  // will
  //       be the start frame for the next group. Otherwise prior to the call to
  //       av1_rc_get_second_pass_params() the data will be undefined.
  gf_group->arf_update_idx[frame_index] = 0;
  gf_group->arf_ref_idx[frame_index] = 0;

  if (rc->source_alt_ref_pending) {
    gf_group->update_type[frame_index] = OVERLAY_UPDATE;
    gf_group->rf_level[frame_index] = INTER_NORMAL;

    cpi->arf_pos_in_gf[0] = 1;
    if (cpi->num_extra_arfs) {
      // Overwrite the update_type for extra-ARF's corresponding internal
      // OVERLAY's: Change from LF_UPDATE to INTNL_OVERLAY_UPDATE.
      for (i = cpi->num_extra_arfs; i > 0; --i) {
        cpi->arf_pos_in_gf[i] =
            (i == cpi->num_extra_arfs ? 2 : cpi->arf_pos_for_ovrly[i + 1] + 1);

        gf_group->update_type[cpi->arf_pos_for_ovrly[i]] = INTNL_OVERLAY_UPDATE;
        gf_group->rf_level[cpi->arf_pos_for_ovrly[i]] = INTER_NORMAL;
      }
    }
  } else {
    gf_group->update_type[frame_index] = GF_UPDATE;
    gf_group->rf_level[frame_index] = GF_ARF_STD;
  }

  gf_group->bidir_pred_enabled[frame_index] = 0;
  gf_group->brf_src_offset[frame_index] = 0;
}

static void allocate_gf_group_bits(AV1_COMP *cpi, int64_t gf_group_bits,
                                   double group_error, int gf_arf_bits) {
  RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  FIRSTPASS_STATS frame_stats;
  int i;
  int frame_index = 0;
  int target_frame_size;
  int key_frame;
  const int max_bits = frame_max_bits(&cpi->rc, &cpi->oxcf);
  int64_t total_group_bits = gf_group_bits;
  double modified_err = 0.0;
  double err_fraction;
  int ext_arf_boost[MAX_EXT_ARFS];

  define_gf_group_structure(cpi);

  av1_zero_array(ext_arf_boost, MAX_EXT_ARFS);

  key_frame = cpi->common.frame_type == KEY_FRAME;

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  // === [frame_index == 0] ===
  if (!key_frame) {
    if (rc->source_alt_ref_active)
      gf_group->bit_allocation[frame_index] = 0;
    else
      gf_group->bit_allocation[frame_index] = gf_arf_bits;

    // Step over the golden frame / overlay frame
    if (EOF == input_stats(twopass, &frame_stats)) return;
  }

  // Deduct the boost bits for arf (or gf if it is not a key frame)
  // from the group total.
  if (rc->source_alt_ref_pending || !key_frame) total_group_bits -= gf_arf_bits;

  frame_index++;

  // Store the bits to spend on the ARF if there is one.
  // === [frame_index == 1] ===
  if (rc->source_alt_ref_pending) {
    gf_group->bit_allocation[frame_index] = gf_arf_bits;

    ++frame_index;

    // Skip all the extra-ARF's right after ARF at the starting segment of
    // the current GF group.
    if (cpi->num_extra_arfs) {
      while (gf_group->update_type[frame_index] == INTNL_ARF_UPDATE)
        ++frame_index;
    }
  }

  // Allocate bits to the other frames in the group.
  for (i = 0; i < rc->baseline_gf_interval - rc->source_alt_ref_pending; ++i) {
    if (EOF == input_stats(twopass, &frame_stats)) break;

    modified_err = calculate_modified_err(cpi, twopass, oxcf, &frame_stats);

    if (group_error > 0)
      err_fraction = modified_err / DOUBLE_DIVIDE_CHECK(group_error);
    else
      err_fraction = 0.0;

    target_frame_size = (int)((double)total_group_bits * err_fraction);

    target_frame_size =
        clamp(target_frame_size, 0, AOMMIN(max_bits, (int)total_group_bits));

    if (gf_group->update_type[frame_index] == BRF_UPDATE) {
      // Boost up the allocated bits on BWDREF_FRAME
      gf_group->bit_allocation[frame_index] =
          target_frame_size + (target_frame_size >> 2);
    } else if (gf_group->update_type[frame_index] == LAST_BIPRED_UPDATE) {
      // Press down the allocated bits on LAST_BIPRED_UPDATE frames
      gf_group->bit_allocation[frame_index] =
          target_frame_size - (target_frame_size >> 1);
    } else if (gf_group->update_type[frame_index] == BIPRED_UPDATE) {
      // TODO(zoeliu): To investigate whether the allocated bits on
      // BIPRED_UPDATE frames need to be further adjusted.
      gf_group->bit_allocation[frame_index] = target_frame_size;
    } else {
      assert(gf_group->update_type[frame_index] == LF_UPDATE ||
             gf_group->update_type[frame_index] == INTNL_OVERLAY_UPDATE);
      gf_group->bit_allocation[frame_index] = target_frame_size;
    }

    ++frame_index;

    // Skip all the extra-ARF's.
    if (cpi->num_extra_arfs) {
      while (gf_group->update_type[frame_index] == INTNL_ARF_UPDATE)
        ++frame_index;
    }
  }

  // NOTE: We need to configure the frame at the end of the sequence + 1 that
  //       will be the start frame for the next group. Otherwise prior to the
  //       call to av1_rc_get_second_pass_params() the data will be undefined.
  if (rc->source_alt_ref_pending) {
    if (cpi->num_extra_arfs) {
      // NOTE: For bit allocation, move the allocated bits associated with
      //       INTNL_OVERLAY_UPDATE to the corresponding INTNL_ARF_UPDATE.
      //       i > 0 for extra-ARF's and i == 0 for ARF:
      //         arf_pos_for_ovrly[i]: Position for INTNL_OVERLAY_UPDATE
      //         arf_pos_in_gf[i]: Position for INTNL_ARF_UPDATE
      for (i = cpi->num_extra_arfs; i > 0; --i) {
        assert(gf_group->update_type[cpi->arf_pos_for_ovrly[i]] ==
               INTNL_OVERLAY_UPDATE);

        // Encoder's choice:
        //   Set show_existing_frame == 1 for all extra-ARF's, and hence
        //   allocate zero bit for both all internal OVERLAY frames.
        gf_group->bit_allocation[cpi->arf_pos_in_gf[i]] =
            gf_group->bit_allocation[cpi->arf_pos_for_ovrly[i]];
        gf_group->bit_allocation[cpi->arf_pos_for_ovrly[i]] = 0;
      }
    }
  }
}

// Analyse and define a gf/arf group.
static void define_gf_group(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = twopass->stats_in;
  int i;

  double boost_score = 0.0;
  double old_boost_score = 0.0;
  double gf_group_err = 0.0;
#if GROUP_ADAPTIVE_MAXQ
  double gf_group_raw_error = 0.0;
#endif
  double gf_group_skip_pct = 0.0;
  double gf_group_inactive_zone_rows = 0.0;
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
  int active_min_gf_interval;
  int64_t gf_group_bits;
  double gf_group_error_left;
  int gf_arf_bits;
  const int is_key_frame = frame_is_intra_only(cm);
  const int arf_active_or_kf = is_key_frame || rc->source_alt_ref_active;

  cpi->extra_arf_allowed = 1;

  // Reset the GF group data structures unless this is a key
  // frame in which case it will already have been done.
  if (is_key_frame == 0) {
    av1_zero(twopass->gf_group);
  }

  aom_clear_system_state();
  av1_zero(next_frame);

  // Load stats for the current frame.
  mod_frame_err = calculate_modified_err(cpi, twopass, oxcf, this_frame);

  // Note the error of the frame at the start of the group. This will be
  // the GF frame error if we code a normal gf.
  gf_first_frame_err = mod_frame_err;

  // If this is a key frame or the overlay from a previous arf then
  // the error score / cost of this frame has already been accounted for.
  if (arf_active_or_kf) {
    gf_group_err -= gf_first_frame_err;
#if GROUP_ADAPTIVE_MAXQ
    gf_group_raw_error -= this_frame->coded_error;
#endif
    gf_group_skip_pct -= this_frame->intra_skip_pct;
    gf_group_inactive_zone_rows -= this_frame->inactive_zone_rows;
  }

  // Motion breakout threshold for loop below depends on image size.
  mv_ratio_accumulator_thresh =
      (cpi->initial_height + cpi->initial_width) / 4.0;

  // Set a maximum and minimum interval for the GF group.
  // If the image appears almost completely static we can extend beyond this.
  {
    int int_max_q = (int)(av1_convert_qindex_to_q(twopass->active_worst_quality,
                                                  cpi->common.bit_depth));
    int int_lbq = (int)(av1_convert_qindex_to_q(rc->last_boosted_qindex,
                                                cpi->common.bit_depth));

    active_min_gf_interval = rc->min_gf_interval + AOMMIN(2, int_max_q / 200);
    if (active_min_gf_interval > rc->max_gf_interval)
      active_min_gf_interval = rc->max_gf_interval;

    if (cpi->multi_arf_allowed) {
      active_max_gf_interval = rc->max_gf_interval;
    } else {
      // The value chosen depends on the active Q range. At low Q we have
      // bits to spare and are better with a smaller interval and smaller boost.
      // At high Q when there are few bits to spare we are better with a longer
      // interval to spread the cost of the GF.
      active_max_gf_interval = 12 + AOMMIN(4, (int_lbq / 6));

      // We have: active_min_gf_interval <= rc->max_gf_interval
      if (active_max_gf_interval < active_min_gf_interval)
        active_max_gf_interval = active_min_gf_interval;
      else if (active_max_gf_interval > rc->max_gf_interval)
        active_max_gf_interval = rc->max_gf_interval;
    }
  }

  double avg_sr_coded_error = 0;
  double avg_raw_err_stdev = 0;
  int non_zero_stdev_count = 0;

  i = 0;
  while (i < rc->static_scene_max_gf_interval && i < rc->frames_to_key) {
    ++i;

    // Accumulate error score of frames in this gf group.
    mod_frame_err = calculate_modified_err(cpi, twopass, oxcf, this_frame);
    gf_group_err += mod_frame_err;
#if GROUP_ADAPTIVE_MAXQ
    gf_group_raw_error += this_frame->coded_error;
#endif
    gf_group_skip_pct += this_frame->intra_skip_pct;
    gf_group_inactive_zone_rows += this_frame->inactive_zone_rows;

    if (EOF == input_stats(twopass, &next_frame)) break;

    // Test for the case where there is a brief flash but the prediction
    // quality back to an earlier frame is then restored.
    flash_detected = detect_flash(twopass, 0);

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(
        &next_frame, &this_frame_mv_in_out, &mv_in_out_accumulator,
        &abs_mv_in_out_accumulator, &mv_ratio_accumulator);
    // sum up the metric values of current gf group
    avg_sr_coded_error += next_frame.sr_coded_error;
    if (fabs(next_frame.raw_error_stdev) > 0.000001) {
      non_zero_stdev_count++;
      avg_raw_err_stdev += next_frame.raw_error_stdev;
    }

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      last_loop_decay_rate = loop_decay_rate;
      loop_decay_rate = get_prediction_decay_rate(cpi, &next_frame);

      decay_accumulator = decay_accumulator * loop_decay_rate;

      // Monitor for static sections.
      zero_motion_accumulator = AOMMIN(
          zero_motion_accumulator, get_zero_motion_factor(cpi, &next_frame));

      // Break clause to detect very still sections after motion. For example,
      // a static image after a fade or other transition.
      if (detect_transition_to_still(cpi, i, 5, loop_decay_rate,
                                     last_loop_decay_rate)) {
        allow_alt_ref = 0;
        break;
      }
    }

    // Calculate a boost number for this frame.
    boost_score +=
        decay_accumulator *
        calc_frame_boost(cpi, &next_frame, this_frame_mv_in_out, GF_MAX_BOOST);

    // Break out conditions.
    if (
        // Break at active_max_gf_interval unless almost totally static.
        (i >= (active_max_gf_interval + arf_active_or_kf) &&
         zero_motion_accumulator < 0.995) ||
        (
            // Don't break out with a very short interval.
            (i >= active_min_gf_interval + arf_active_or_kf) &&
            (!flash_detected) &&
            ((mv_ratio_accumulator > mv_ratio_accumulator_thresh) ||
             (abs_mv_in_out_accumulator > 3.0) ||
             (mv_in_out_accumulator < -2.0) ||
             ((boost_score - old_boost_score) < BOOST_BREAKOUT)))) {
      // If GF group interval is < 12, we force it to be 8. Otherwise,
      // if it is >= 12, we keep it as is.
      // NOTE: 'i' is 1 more than the GF group interval candidate that is being
      //       checked.
      if (i == (8 + 1) || i >= (12 + 1)) {
        boost_score = old_boost_score;
        break;
      }
    }

    *this_frame = next_frame;
    old_boost_score = boost_score;
  }
  twopass->gf_zeromotion_pct = (int)(zero_motion_accumulator * 1000.0);

  // Was the group length constrained by the requirement for a new KF?
  rc->constrained_gf_group = (i >= rc->frames_to_key) ? 1 : 0;

  const int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE) ? cpi->initial_mbs
                                                             : cpi->common.MBs;
  assert(num_mbs > 0);
  if (i) avg_sr_coded_error /= i;

  // Should we use the alternate reference frame.
  if (allow_alt_ref && (i < cpi->oxcf.lag_in_frames) &&
      (i >= rc->min_gf_interval)) {
    // Calculate the boost for alt ref.
    rc->gfu_boost =
        calc_arf_boost(cpi, 0, (i - 1), (i - 1), &f_boost, &b_boost);
    rc->source_alt_ref_pending = 1;
  } else {
    rc->gfu_boost = AOMMAX((int)boost_score, MIN_ARF_GF_BOOST);
    rc->source_alt_ref_pending = 0;
  }

  // Set the interval until the next gf.
  rc->baseline_gf_interval = i - (is_key_frame || rc->source_alt_ref_pending);
  if (non_zero_stdev_count) avg_raw_err_stdev /= non_zero_stdev_count;

  // Disable extra altrefs and backward refs for "still" gf group:
  //   zero_motion_accumulator: minimum percentage of (0,0) motion;
  //   avg_sr_coded_error:      average of the SSE per pixel of each frame;
  //   avg_raw_err_stdev:       average of the standard deviation of (0,0)
  //                            motion error per block of each frame.
  const int disable_bwd_extarf =
      (zero_motion_accumulator > MIN_ZERO_MOTION &&
       avg_sr_coded_error / num_mbs < MAX_SR_CODED_ERROR &&
       avg_raw_err_stdev < MAX_RAW_ERR_VAR);

  if (disable_bwd_extarf) cpi->extra_arf_allowed = 0;

  if (!cpi->extra_arf_allowed) {
    cpi->num_extra_arfs = 0;
  } else {
    // Compute how many extra alt_refs we can have
    cpi->num_extra_arfs = get_number_of_extra_arfs(rc->baseline_gf_interval,
                                                   rc->source_alt_ref_pending);
  }
  // Currently at maximum two extra ARFs' are allowed
  assert(cpi->num_extra_arfs <= MAX_EXT_ARFS);

  rc->frames_till_gf_update_due = rc->baseline_gf_interval;

  rc->bipred_group_interval = BFG_INTERVAL;
  // The minimum bi-predictive frame group interval is 2.
  if (rc->bipred_group_interval < 2) rc->bipred_group_interval = 0;

  // Reset the file position.
  reset_fpf_position(twopass, start_pos);

  // Calculate the bits to be allocated to the gf/arf group as a whole
  gf_group_bits = calculate_total_gf_group_bits(cpi, gf_group_err);

#if GROUP_ADAPTIVE_MAXQ
  // Calculate an estimate of the maxq needed for the group.
  // We are more agressive about correcting for sections
  // where there could be significant overshoot than for easier
  // sections where we do not wish to risk creating an overshoot
  // of the allocated bit budget.
  if ((cpi->oxcf.rc_mode != AOM_Q) && (rc->baseline_gf_interval > 1)) {
    const int vbr_group_bits_per_frame =
        (int)(gf_group_bits / rc->baseline_gf_interval);
    const double group_av_err = gf_group_raw_error / rc->baseline_gf_interval;
    const double group_av_skip_pct =
        gf_group_skip_pct / rc->baseline_gf_interval;
    const double group_av_inactive_zone =
        ((gf_group_inactive_zone_rows * 2) /
         (rc->baseline_gf_interval * (double)cm->mb_rows));

    int tmp_q;
    // rc factor is a weight factor that corrects for local rate control drift.
    double rc_factor = 1.0;
    if (rc->rate_error_estimate > 0) {
      rc_factor = AOMMAX(RC_FACTOR_MIN,
                         (double)(100 - rc->rate_error_estimate) / 100.0);
    } else {
      rc_factor = AOMMIN(RC_FACTOR_MAX,
                         (double)(100 - rc->rate_error_estimate) / 100.0);
    }
    tmp_q = get_twopass_worst_quality(
        cpi, group_av_err, (group_av_skip_pct + group_av_inactive_zone),
        vbr_group_bits_per_frame, twopass->kfgroup_inter_fraction * rc_factor);
    twopass->active_worst_quality =
        AOMMAX(tmp_q, twopass->active_worst_quality >> 1);
  }
#endif

  // Calculate the extra bits to be used for boosted frame(s)
  gf_arf_bits = calculate_boost_bits(rc->baseline_gf_interval, rc->gfu_boost,
                                     gf_group_bits);

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
  } else if (is_key_frame == 0) {
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
    twopass->section_intra_rating = calculate_section_intra_ratio(
        start_pos, twopass->stats_in_end, rc->baseline_gf_interval);
  }
}

// Threshold for use of the lagging second reference frame. High second ref
// usage may point to a transient event like a flash or occlusion rather than
// a real scene cut.
#define SECOND_REF_USEAGE_THRESH 0.1
// Minimum % intra coding observed in first pass (1.0 = 100%)
#define MIN_INTRA_LEVEL 0.25
// Minimum ratio between the % of intra coding and inter coding in the first
// pass after discounting neutral blocks (discounting neutral blocks in this
// way helps catch scene cuts in clips with very flat areas or letter box
// format clips with image padding.
#define INTRA_VS_INTER_THRESH 2.0
// Hard threshold where the first pass chooses intra for almost all blocks.
// In such a case even if the frame is not a scene cut coding a key frame
// may be a good option.
#define VERY_LOW_INTER_THRESH 0.05
// Maximum threshold for the relative ratio of intra error score vs best
// inter error score.
#define KF_II_ERR_THRESHOLD 2.5
// In real scene cuts there is almost always a sharp change in the intra
// or inter error score.
#define ERR_CHANGE_THRESHOLD 0.4
// For real scene cuts we expect an improvment in the intra inter error
// ratio in the next frame.
#define II_IMPROVEMENT_THRESHOLD 3.5
#define KF_II_MAX 128.0

static int test_candidate_kf(TWO_PASS *twopass,
                             const FIRSTPASS_STATS *last_frame,
                             const FIRSTPASS_STATS *this_frame,
                             const FIRSTPASS_STATS *next_frame) {
  int is_viable_kf = 0;
  double pcnt_intra = 1.0 - this_frame->pcnt_inter;
  double modified_pcnt_inter =
      this_frame->pcnt_inter - this_frame->pcnt_neutral;

  // Does the frame satisfy the primary criteria of a key frame?
  // See above for an explanation of the test criteria.
  // If so, then examine how well it predicts subsequent frames.
  if ((this_frame->pcnt_second_ref < SECOND_REF_USEAGE_THRESH) &&
      (next_frame->pcnt_second_ref < SECOND_REF_USEAGE_THRESH) &&
      ((this_frame->pcnt_inter < VERY_LOW_INTER_THRESH) ||
       ((pcnt_intra > MIN_INTRA_LEVEL) &&
        (pcnt_intra > (INTRA_VS_INTER_THRESH * modified_pcnt_inter)) &&
        ((this_frame->intra_error /
          DOUBLE_DIVIDE_CHECK(this_frame->coded_error)) <
         KF_II_ERR_THRESHOLD) &&
        ((fabs(last_frame->coded_error - this_frame->coded_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->coded_error) >
          ERR_CHANGE_THRESHOLD) ||
         (fabs(last_frame->intra_error - this_frame->intra_error) /
              DOUBLE_DIVIDE_CHECK(this_frame->intra_error) >
          ERR_CHANGE_THRESHOLD) ||
         ((next_frame->intra_error /
           DOUBLE_DIVIDE_CHECK(next_frame->coded_error)) >
          II_IMPROVEMENT_THRESHOLD))))) {
    int i;
    const FIRSTPASS_STATS *start_pos = twopass->stats_in;
    FIRSTPASS_STATS local_next_frame = *next_frame;
    double boost_score = 0.0;
    double old_boost_score = 0.0;
    double decay_accumulator = 1.0;

    // Examine how well the key frame predicts subsequent frames.
    for (i = 0; i < 16; ++i) {
      double next_iiratio = (BOOST_FACTOR * local_next_frame.intra_error /
                             DOUBLE_DIVIDE_CHECK(local_next_frame.coded_error));

      if (next_iiratio > KF_II_MAX) next_iiratio = KF_II_MAX;

      // Cumulative effect of decay in prediction quality.
      if (local_next_frame.pcnt_inter > 0.85)
        decay_accumulator *= local_next_frame.pcnt_inter;
      else
        decay_accumulator *= (0.85 + local_next_frame.pcnt_inter) / 2.0;

      // Keep a running total.
      boost_score += (decay_accumulator * next_iiratio);

      // Test various breakout clauses.
      if ((local_next_frame.pcnt_inter < 0.05) || (next_iiratio < 1.5) ||
          (((local_next_frame.pcnt_inter - local_next_frame.pcnt_neutral) <
            0.20) &&
           (next_iiratio < 3.0)) ||
          ((boost_score - old_boost_score) < 3.0) ||
          (local_next_frame.intra_error < 200)) {
        break;
      }

      old_boost_score = boost_score;

      // Get the next frame details
      if (EOF == input_stats(twopass, &local_next_frame)) break;
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

#define FRAMES_TO_CHECK_DECAY 8

static void find_next_key_frame(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  int i, j;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const FIRSTPASS_STATS first_frame = *this_frame;
  const FIRSTPASS_STATS *const start_position = twopass->stats_in;
  FIRSTPASS_STATS next_frame;
  FIRSTPASS_STATS last_frame;
  int kf_bits = 0;
  int loop_decay_counter = 0;
  double decay_accumulator = 1.0;
  double av_decay_accumulator = 0.0;
  double zero_motion_accumulator = 1.0;
  double boost_score = 0.0;
  double kf_mod_err = 0.0;
  double kf_group_err = 0.0;
  double recent_loop_decay[FRAMES_TO_CHECK_DECAY];

  av1_zero(next_frame);

  cpi->common.frame_type = KEY_FRAME;

  // Reset the GF group data structures.
  av1_zero(*gf_group);

  // Is this a forced key frame by interval.
  rc->this_key_frame_forced = rc->next_key_frame_forced;

  // Clear the alt ref active flag and last group multi arf flags as they
  // can never be set for a key frame.
  rc->source_alt_ref_active = 0;

  // KF is always a GF so clear frames till next gf counter.
  rc->frames_till_gf_update_due = 0;

  rc->frames_to_key = 1;

  twopass->kf_group_bits = 0;        // Total bits available to kf group
  twopass->kf_group_error_left = 0;  // Group modified error score.

  kf_mod_err = calculate_modified_err(cpi, twopass, oxcf, this_frame);

  // Initialize the decay rates for the recent frames to check
  for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j) recent_loop_decay[j] = 1.0;

  // Find the next keyframe.
  i = 0;
  while (twopass->stats_in < twopass->stats_in_end &&
         rc->frames_to_key < cpi->oxcf.key_freq) {
    // Accumulate kf group error.
    kf_group_err += calculate_modified_err(cpi, twopass, oxcf, this_frame);

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
      loop_decay_rate = get_prediction_decay_rate(cpi, twopass->stats_in);

      // We want to know something about the recent past... rather than
      // as used elsewhere where we are concerned with decay in prediction
      // quality since the last GF or KF.
      recent_loop_decay[i % FRAMES_TO_CHECK_DECAY] = loop_decay_rate;
      decay_accumulator = 1.0;
      for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j)
        decay_accumulator *= recent_loop_decay[j];

      // Special check for transition or high motion followed by a
      // static scene.
      if (detect_transition_to_still(cpi, i, cpi->oxcf.key_freq - i,
                                     loop_decay_rate, decay_accumulator))
        break;

      // Step on to the next frame.
      ++rc->frames_to_key;

      // If we don't have a real key frame within the next two
      // key_freq intervals then break out of the loop.
      if (rc->frames_to_key >= 2 * cpi->oxcf.key_freq) break;
    } else {
      ++rc->frames_to_key;
    }
    ++i;
  }

  // If there is a max kf interval set by the user we must obey it.
  // We already breakout of the loop above at 2x max.
  // This code centers the extra kf if the actual natural interval
  // is between 1x and 2x.
  if (cpi->oxcf.auto_key && rc->frames_to_key > cpi->oxcf.key_freq) {
    FIRSTPASS_STATS tmp_frame = first_frame;

    rc->frames_to_key /= 2;

    // Reset to the start of the group.
    reset_fpf_position(twopass, start_position);

    kf_group_err = 0.0;

    // Rescan to get the correct error data for the forced kf group.
    for (i = 0; i < rc->frames_to_key; ++i) {
      kf_group_err += calculate_modified_err(cpi, twopass, oxcf, &tmp_frame);
      input_stats(twopass, &tmp_frame);
    }
    rc->next_key_frame_forced = 1;
  } else if (twopass->stats_in == twopass->stats_in_end ||
             rc->frames_to_key >= cpi->oxcf.key_freq) {
    rc->next_key_frame_forced = 1;
  } else {
    rc->next_key_frame_forced = 0;
  }

  // Special case for the last key frame of the file.
  if (twopass->stats_in >= twopass->stats_in_end) {
    // Accumulate kf group error.
    kf_group_err += calculate_modified_err(cpi, twopass, oxcf, this_frame);
  }

  // Calculate the number of bits that should be assigned to the kf group.
  if (twopass->bits_left > 0 && twopass->modified_error_left > 0.0) {
    // Maximum number of bits for a single normal frame (not key frame).
    const int max_bits = frame_max_bits(rc, &cpi->oxcf);

    // Maximum number of bits allocated to the key frame group.
    int64_t max_grp_bits;

    // Default allocation based on bits left and relative
    // complexity of the section.
    twopass->kf_group_bits = (int64_t)(
        twopass->bits_left * (kf_group_err / twopass->modified_error_left));

    // Clip based on maximum per frame rate defined by the user.
    max_grp_bits = (int64_t)max_bits * (int64_t)rc->frames_to_key;
    if (twopass->kf_group_bits > max_grp_bits)
      twopass->kf_group_bits = max_grp_bits;
  } else {
    twopass->kf_group_bits = 0;
  }
  twopass->kf_group_bits = AOMMAX(0, twopass->kf_group_bits);

  // Reset the first pass file position.
  reset_fpf_position(twopass, start_position);

  // Scan through the kf group collating various stats used to determine
  // how many bits to spend on it.
  decay_accumulator = 1.0;
  boost_score = 0.0;
  for (i = 0; i < (rc->frames_to_key - 1); ++i) {
    if (EOF == input_stats(twopass, &next_frame)) break;

    // Monitor for static sections.
    zero_motion_accumulator = AOMMIN(zero_motion_accumulator,
                                     get_zero_motion_factor(cpi, &next_frame));

    // Not all frames in the group are necessarily used in calculating boost.
    if ((i <= rc->max_gf_interval) ||
        ((i <= (rc->max_gf_interval * 4)) && (decay_accumulator > 0.5))) {
      const double frame_boost =
          calc_frame_boost(cpi, this_frame, 0, KF_MAX_BOOST);

      // How fast is prediction quality decaying.
      if (!detect_flash(twopass, 0)) {
        const double loop_decay_rate =
            get_prediction_decay_rate(cpi, &next_frame);
        decay_accumulator *= loop_decay_rate;
        decay_accumulator = AOMMAX(decay_accumulator, MIN_DECAY_FACTOR);
        av_decay_accumulator += decay_accumulator;
        ++loop_decay_counter;
      }
      boost_score += (decay_accumulator * frame_boost);
    }
  }
  if (loop_decay_counter > 0)
    av_decay_accumulator /= (double)loop_decay_counter;

  reset_fpf_position(twopass, start_position);

  // Store the zero motion percentage
  twopass->kf_zeromotion_pct = (int)(zero_motion_accumulator * 100.0);

  // Calculate a section intra ratio used in setting max loop filter.
  twopass->section_intra_rating = calculate_section_intra_ratio(
      start_position, twopass->stats_in_end, rc->frames_to_key);

  // Apply various clamps for min and max boost
  rc->kf_boost = (int)(av_decay_accumulator * boost_score);
  rc->kf_boost = AOMMAX(rc->kf_boost, (rc->frames_to_key * 3));
  rc->kf_boost = AOMMAX(rc->kf_boost, MIN_KF_BOOST);

  // Work out how many bits to allocate for the key frame itself.
  kf_bits = calculate_boost_bits((rc->frames_to_key - 1), rc->kf_boost,
                                 twopass->kf_group_bits);
  // printf("kf boost = %d kf_bits = %d kf_zeromotion_pct = %d\n", rc->kf_boost,
  //        kf_bits, twopass->kf_zeromotion_pct);

  // Work out the fraction of the kf group bits reserved for the inter frames
  // within the group after discounting the bits for the kf itself.
  if (twopass->kf_group_bits) {
    twopass->kfgroup_inter_fraction =
        (double)(twopass->kf_group_bits - kf_bits) /
        (double)twopass->kf_group_bits;
  } else {
    twopass->kfgroup_inter_fraction = 1.0;
  }

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

#if USE_GF16_MULTI_LAYER
// === GF Group of 16 ===
void av1_ref_frame_map_idx_updates(AV1_COMP *cpi, int gf_frame_index) {
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;

  int ref_fb_idx_prev[REF_FRAMES];
  int ref_fb_idx_curr[REF_FRAMES];

  for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame) {
    ref_fb_idx_prev[ref_frame] = cpi->ref_fb_idx[ref_frame];
  }

  // Update map index for each reference frame
  for (int ref_idx = 0; ref_idx < REF_FRAMES; ++ref_idx) {
    int ref_frame = gf_group->ref_fb_idx_map[gf_frame_index][ref_idx];
    ref_fb_idx_curr[ref_idx] = ref_fb_idx_prev[ref_frame - LAST_FRAME];
  }

  for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame) {
    cpi->ref_fb_idx[ref_frame] = ref_fb_idx_curr[ref_frame];
  }
}

// Define the reference buffers that will be updated post encode.
static void configure_buffer_updates_16(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;

  if (gf_group->update_type[gf_group->index] == KF_UPDATE) {
    cpi->refresh_fb_idx = 0;

    cpi->refresh_last_frame = 1;
    cpi->refresh_golden_frame = 1;
    cpi->refresh_bwd_ref_frame = 1;
    cpi->refresh_alt2_ref_frame = 1;
    cpi->refresh_alt_ref_frame = 1;

    return;
  }

  // Update reference frame map indexes
  av1_ref_frame_map_idx_updates(cpi, gf_group->index);

  // Update refresh index
  switch (gf_group->refresh_idx[gf_group->index]) {
    case LAST_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[LAST_FRAME - LAST_FRAME];
      break;

    case LAST2_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[LAST2_FRAME - LAST_FRAME];
      break;

    case LAST3_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[LAST3_FRAME - LAST_FRAME];
      break;

    case GOLDEN_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[GOLDEN_FRAME - 1];
      break;

    case BWDREF_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[BWDREF_FRAME - 1];
      break;

    case ALTREF2_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[ALTREF2_FRAME - 1];
      break;

    case ALTREF_FRAME:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[ALTREF_FRAME - 1];
      break;

    case REF_FRAMES:
      cpi->refresh_fb_idx = cpi->ref_fb_idx[REF_FRAMES - 1];
      break;

    default: assert(0); break;
  }

  // Update refresh flags
  switch (gf_group->refresh_flag[gf_group->index]) {
    case LAST_FRAME:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case GOLDEN_FRAME:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case BWDREF_FRAME:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 1;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case ALTREF2_FRAME:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 1;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case ALTREF_FRAME:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      break;

    default: assert(0); break;
  }

  switch (gf_group->update_type[gf_group->index]) {
    case BRF_UPDATE: cpi->rc.is_bwd_ref_frame = 1; break;

    case LAST_BIPRED_UPDATE: cpi->rc.is_last_bipred_frame = 1; break;

    case BIPRED_UPDATE: cpi->rc.is_bipred_frame = 1; break;

    case INTNL_OVERLAY_UPDATE: cpi->rc.is_src_frame_ext_arf = 1;
    case OVERLAY_UPDATE: cpi->rc.is_src_frame_alt_ref = 1; break;

    default: break;
  }
}
#endif  // USE_GF16_MULTI_LAYER

// Define the reference buffers that will be updated post encode.
static void configure_buffer_updates(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;

  // NOTE(weitinglin): Should we define another function to take care of
  // cpi->rc.is_$Source_Type to make this function as it is in the comment?

  cpi->rc.is_src_frame_alt_ref = 0;
  cpi->rc.is_bwd_ref_frame = 0;
  cpi->rc.is_last_bipred_frame = 0;
  cpi->rc.is_bipred_frame = 0;
  cpi->rc.is_src_frame_ext_arf = 0;

#if USE_GF16_MULTI_LAYER
  RATE_CONTROL *const rc = &cpi->rc;
  if (rc->baseline_gf_interval == 16) {
    configure_buffer_updates_16(cpi);
    return;
  }
#endif  // USE_GF16_MULTI_LAYER

  switch (twopass->gf_group.update_type[twopass->gf_group.index]) {
    case KF_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_bwd_ref_frame = 1;
      cpi->refresh_alt2_ref_frame = 1;
      cpi->refresh_alt_ref_frame = 1;
      break;

    case LF_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case GF_UPDATE:
      // TODO(zoeliu): To further investigate whether 'refresh_last_frame' is
      //               needed.
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      break;

    case OVERLAY_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 1;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      cpi->rc.is_src_frame_alt_ref = 1;
      break;

    case ARF_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      // NOTE: BWDREF does not get updated along with ALTREF_FRAME.
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      break;

    case BRF_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 1;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      cpi->rc.is_bwd_ref_frame = 1;
      break;

    case LAST_BIPRED_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      cpi->rc.is_last_bipred_frame = 1;
      break;

    case BIPRED_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      cpi->rc.is_bipred_frame = 1;
      break;

    case INTNL_OVERLAY_UPDATE:
      cpi->refresh_last_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      cpi->rc.is_src_frame_alt_ref = 1;
      cpi->rc.is_src_frame_ext_arf = 1;
      break;

    case INTNL_ARF_UPDATE:
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 1;
      cpi->refresh_alt_ref_frame = 0;
      break;

    default: assert(0); break;
  }
}

static int is_skippable_frame(const AV1_COMP *cpi) {
  // If the current frame does not have non-zero motion vector detected in the
  // first  pass, and so do its previous and forward frames, then this frame
  // can be skipped for partition check, and the partition size is assigned
  // according to the variance
  const TWO_PASS *const twopass = &cpi->twopass;

  return (!frame_is_intra_only(&cpi->common) &&
          twopass->stats_in - 2 > twopass->stats_in_start &&
          twopass->stats_in < twopass->stats_in_end &&
          (twopass->stats_in - 1)->pcnt_inter -
                  (twopass->stats_in - 1)->pcnt_motion ==
              1 &&
          (twopass->stats_in - 2)->pcnt_inter -
                  (twopass->stats_in - 2)->pcnt_motion ==
              1 &&
          twopass->stats_in->pcnt_inter - twopass->stats_in->pcnt_motion == 1);
}

void av1_rc_get_second_pass_params(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->twopass;
  GF_GROUP *const gf_group = &twopass->gf_group;
  int frames_left;
  FIRSTPASS_STATS this_frame;

  int target_rate;

  frames_left = (int)(twopass->total_stats.count - cm->current_video_frame);

  if (!twopass->stats_in) return;

  // If this is an arf frame then we dont want to read the stats file or
  // advance the input pointer as we already have what we need.
  if (gf_group->update_type[gf_group->index] == ARF_UPDATE ||
      gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE) {
    configure_buffer_updates(cpi);
    target_rate = gf_group->bit_allocation[gf_group->index];
    target_rate = av1_rc_clamp_pframe_target_size(cpi, target_rate);
    rc->base_frame_target = target_rate;

    cm->frame_type = INTER_FRAME;

    // Do the firstpass stats indicate that this frame is skippable for the
    // partition search?
    if (cpi->sf.allow_partition_search_skip && cpi->oxcf.pass == 2) {
      cpi->partition_search_skippable_frame = is_skippable_frame(cpi);
    }

    return;
  }

  aom_clear_system_state();

  if (cpi->oxcf.rc_mode == AOM_Q) {
    twopass->active_worst_quality = cpi->oxcf.cq_level;
  } else if (cm->current_video_frame == 0) {
    // Special case code for first frame.
    const int section_target_bandwidth =
        (int)(twopass->bits_left / frames_left);
    const double section_length = twopass->total_left_stats.count;
    const double section_error =
        twopass->total_left_stats.coded_error / section_length;
    const double section_intra_skip =
        twopass->total_left_stats.intra_skip_pct / section_length;
    const double section_inactive_zone =
        (twopass->total_left_stats.inactive_zone_rows * 2) /
        ((double)cm->mb_rows * section_length);
    const int tmp_q = get_twopass_worst_quality(
        cpi, section_error, section_intra_skip + section_inactive_zone,
        section_target_bandwidth, DEFAULT_GRP_WEIGHT);

    twopass->active_worst_quality = tmp_q;
    twopass->baseline_active_worst_quality = tmp_q;
    rc->ni_av_qi = tmp_q;
    rc->last_q[INTER_FRAME] = tmp_q;
    rc->avg_q = av1_convert_qindex_to_q(tmp_q, cm->bit_depth);
    rc->avg_frame_qindex[INTER_FRAME] = tmp_q;
    rc->last_q[KEY_FRAME] = (tmp_q + cpi->oxcf.best_allowed_q) / 2;
    rc->avg_frame_qindex[KEY_FRAME] = rc->last_q[KEY_FRAME];
  }

  av1_zero(this_frame);
  if (EOF == input_stats(twopass, &this_frame)) return;

  // Set the frame content type flag.
  if (this_frame.intra_skip_pct >= FC_ANIMATION_THRESH)
    twopass->fr_content_type = FC_GRAPHICS_ANIMATION;
  else
    twopass->fr_content_type = FC_NORMAL;

  // Keyframe and section processing.
  if (rc->frames_to_key == 0 || (cpi->frame_flags & FRAMEFLAGS_KEY)) {
    FIRSTPASS_STATS this_frame_copy;
    this_frame_copy = this_frame;
    // Define next KF group and assign bits to it.
    find_next_key_frame(cpi, &this_frame);
    this_frame = this_frame_copy;
  } else {
    cm->frame_type = INTER_FRAME;
  }

  // Define a new GF/ARF group. (Should always enter here for key frames).
  if (rc->frames_till_gf_update_due == 0) {
    define_gf_group(cpi, &this_frame);

    rc->frames_till_gf_update_due = rc->baseline_gf_interval;

#if ARF_STATS_OUTPUT
    {
      FILE *fpfile;
      fpfile = fopen("arf.stt", "a");
      ++arf_count;
      fprintf(fpfile, "%10d %10d %10d %10d %10d\n", cm->current_video_frame,
              rc->frames_till_gf_update_due, rc->kf_boost, arf_count,
              rc->gfu_boost);

      fclose(fpfile);
    }
#endif
  }

  configure_buffer_updates(cpi);

  // Do the firstpass stats indicate that this frame is skippable for the
  // partition search?
  if (cpi->sf.allow_partition_search_skip && cpi->oxcf.pass == 2) {
    cpi->partition_search_skippable_frame = is_skippable_frame(cpi);
  }

  target_rate = gf_group->bit_allocation[gf_group->index];

  if (cpi->common.frame_type == KEY_FRAME)
    target_rate = av1_rc_clamp_iframe_target_size(cpi, target_rate);
  else
    target_rate = av1_rc_clamp_pframe_target_size(cpi, target_rate);

  rc->base_frame_target = target_rate;

  {
    const int num_mbs = (cpi->oxcf.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cpi->common.MBs;
    // The multiplication by 256 reverses a scaling factor of (>> 8)
    // applied when combining MB error values for the frame.
    twopass->mb_av_energy =
        log(((this_frame.intra_error * 256.0) / num_mbs) + 1.0);
    twopass->frame_avg_haar_energy =
        log((this_frame.frame_avg_wavelet_energy / num_mbs) + 1.0);
  }

  // Update the total stats remaining structure.
  subtract_stats(&twopass->total_left_stats, &this_frame);
}

#define MINQ_ADJ_LIMIT 48
#define MINQ_ADJ_LIMIT_CQ 20
#define HIGH_UNDERSHOOT_RATIO 2
void av1_twopass_postencode_update(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;
  const int bits_used = rc->base_frame_target;

  // VBR correction is done through rc->vbr_bits_off_target. Based on the
  // sign of this value, a limited % adjustment is made to the target rate
  // of subsequent frames, to try and push it back towards 0. This method
  // is designed to prevent extreme behaviour at the end of a clip
  // or group of frames.
  rc->vbr_bits_off_target += rc->base_frame_target - rc->projected_frame_size;
  twopass->bits_left = AOMMAX(twopass->bits_left - bits_used, 0);

  // Calculate the pct rc error.
  if (rc->total_actual_bits) {
    rc->rate_error_estimate =
        (int)((rc->vbr_bits_off_target * 100) / rc->total_actual_bits);
    rc->rate_error_estimate = clamp(rc->rate_error_estimate, -100, 100);
  } else {
    rc->rate_error_estimate = 0;
  }

  if (cpi->common.frame_type != KEY_FRAME) {
    twopass->kf_group_bits -= bits_used;
    twopass->last_kfgroup_zeromotion_pct = twopass->kf_zeromotion_pct;
  }
  twopass->kf_group_bits = AOMMAX(twopass->kf_group_bits, 0);

  // Increment the gf group index ready for the next frame.
  ++twopass->gf_group.index;

  // If the rate control is drifting consider adjustment to min or maxq.
  if ((cpi->oxcf.rc_mode != AOM_Q) &&
      (cpi->twopass.gf_zeromotion_pct < VLOW_MOTION_THRESHOLD) &&
      !cpi->rc.is_src_frame_alt_ref) {
    const int maxq_adj_limit =
        rc->worst_quality - twopass->active_worst_quality;
    const int minq_adj_limit =
        (cpi->oxcf.rc_mode == AOM_CQ ? MINQ_ADJ_LIMIT_CQ : MINQ_ADJ_LIMIT);

    // Undershoot.
    if (rc->rate_error_estimate > cpi->oxcf.under_shoot_pct) {
      --twopass->extend_maxq;
      if (rc->rolling_target_bits >= rc->rolling_actual_bits)
        ++twopass->extend_minq;
      // Overshoot.
    } else if (rc->rate_error_estimate < -cpi->oxcf.over_shoot_pct) {
      --twopass->extend_minq;
      if (rc->rolling_target_bits < rc->rolling_actual_bits)
        ++twopass->extend_maxq;
    } else {
      // Adjustment for extreme local overshoot.
      if (rc->projected_frame_size > (2 * rc->base_frame_target) &&
          rc->projected_frame_size > (2 * rc->avg_frame_bandwidth))
        ++twopass->extend_maxq;

      // Unwind undershoot or overshoot adjustment.
      if (rc->rolling_target_bits < rc->rolling_actual_bits)
        --twopass->extend_minq;
      else if (rc->rolling_target_bits > rc->rolling_actual_bits)
        --twopass->extend_maxq;
    }

    twopass->extend_minq = clamp(twopass->extend_minq, 0, minq_adj_limit);
    twopass->extend_maxq = clamp(twopass->extend_maxq, 0, maxq_adj_limit);

    // If there is a big and undexpected undershoot then feed the extra
    // bits back in quickly. One situation where this may happen is if a
    // frame is unexpectedly almost perfectly predicted by the ARF or GF
    // but not very well predcited by the previous frame.
    if (!frame_is_kf_gf_arf(cpi) && !cpi->rc.is_src_frame_alt_ref) {
      int fast_extra_thresh = rc->base_frame_target / HIGH_UNDERSHOOT_RATIO;
      if (rc->projected_frame_size < fast_extra_thresh) {
        rc->vbr_bits_off_target_fast +=
            fast_extra_thresh - rc->projected_frame_size;
        rc->vbr_bits_off_target_fast =
            AOMMIN(rc->vbr_bits_off_target_fast, (4 * rc->avg_frame_bandwidth));

        // Fast adaptation of minQ if necessary to use up the extra bits.
        if (rc->avg_frame_bandwidth) {
          twopass->extend_minq_fast =
              (int)(rc->vbr_bits_off_target_fast * 8 / rc->avg_frame_bandwidth);
        }
        twopass->extend_minq_fast = AOMMIN(
            twopass->extend_minq_fast, minq_adj_limit - twopass->extend_minq);
      } else if (rc->vbr_bits_off_target_fast) {
        twopass->extend_minq_fast = AOMMIN(
            twopass->extend_minq_fast, minq_adj_limit - twopass->extend_minq);
      } else {
        twopass->extend_minq_fast = 0;
      }
    }
  }
}
