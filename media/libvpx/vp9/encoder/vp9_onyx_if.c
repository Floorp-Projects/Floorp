/*
 * Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <limits.h>

#include "./vpx_config.h"
#include "./vpx_scale_rtcd.h"

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_idct.h"
#if CONFIG_VP9_POSTPROC
#include "vp9/common/vp9_postproc.h"
#endif
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_tile_common.h"

#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mbgraph.h"
#include "vp9/encoder/vp9_onyx_int.h"
#include "vp9/encoder/vp9_picklpf.h"
#include "vp9/encoder/vp9_psnr.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_temporal_filter.h"
#include "vp9/encoder/vp9_vaq.h"
#include "vp9/encoder/vp9_resize.h"

#include "vpx_ports/vpx_timer.h"

void vp9_entropy_mode_init();
void vp9_coef_tree_initialize();

#define DEFAULT_INTERP_FILTER SWITCHABLE

#define SHARP_FILTER_QTHRESH 0          /* Q threshold for 8-tap sharp filter */

#define ALTREF_HIGH_PRECISION_MV 1      // Whether to use high precision mv
                                         //  for altref computation.
#define HIGH_PRECISION_MV_QTHRESH 200   // Q threshold for high precision
                                         // mv. Choose a very high value for
                                         // now so that HIGH_PRECISION is always
                                         // chosen.

// Masks for partially or completely disabling split mode
#define DISABLE_ALL_SPLIT         0x3F
#define DISABLE_ALL_INTER_SPLIT   0x1F
#define DISABLE_COMPOUND_SPLIT    0x18
#define LAST_AND_INTRA_SPLIT_ONLY 0x1E

// Max rate target for 1080P and below encodes under normal circumstances
// (1920 * 1080 / (16 * 16)) * MAX_MB_RATE bits per MB
#define MAX_MB_RATE 250
#define MAXRATE_1080P 2025000

#if CONFIG_INTERNAL_STATS
extern double vp9_calc_ssim(YV12_BUFFER_CONFIG *source,
                            YV12_BUFFER_CONFIG *dest, int lumamask,
                            double *weight);


extern double vp9_calc_ssimg(YV12_BUFFER_CONFIG *source,
                             YV12_BUFFER_CONFIG *dest, double *ssim_y,
                             double *ssim_u, double *ssim_v);


#endif

// #define OUTPUT_YUV_REC

#ifdef OUTPUT_YUV_SRC
FILE *yuv_file;
#endif
#ifdef OUTPUT_YUV_REC
FILE *yuv_rec_file;
#endif

#if 0
FILE *framepsnr;
FILE *kf_list;
FILE *keyfile;
#endif

void vp9_init_quantizer(VP9_COMP *cpi);

static const double in_frame_q_adj_ratio[MAX_SEGMENTS] =
  {1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

static INLINE void Scale2Ratio(int mode, int *hr, int *hs) {
  switch (mode) {
    case NORMAL:
      *hr = 1;
      *hs = 1;
      break;
    case FOURFIVE:
      *hr = 4;
      *hs = 5;
      break;
    case THREEFIVE:
      *hr = 3;
      *hs = 5;
    break;
    case ONETWO:
      *hr = 1;
      *hs = 2;
    break;
    default:
      *hr = 1;
      *hs = 1;
       assert(0);
      break;
  }
}

static void set_high_precision_mv(VP9_COMP *cpi, int allow_high_precision_mv) {
  MACROBLOCK *const mb = &cpi->mb;
  cpi->common.allow_high_precision_mv = allow_high_precision_mv;
  if (cpi->common.allow_high_precision_mv) {
    mb->mvcost = mb->nmvcost_hp;
    mb->mvsadcost = mb->nmvsadcost_hp;
  } else {
    mb->mvcost = mb->nmvcost;
    mb->mvsadcost = mb->nmvsadcost;
  }
}

void vp9_initialize_enc() {
  static int init_done = 0;

  if (!init_done) {
    vp9_initialize_common();
    vp9_coef_tree_initialize();
    vp9_tokenize_initialize();
    vp9_init_quant_tables();
    vp9_init_me_luts();
    vp9_rc_init_minq_luts();
    // init_base_skip_probs();
    vp9_entropy_mv_init();
    vp9_entropy_mode_init();
    init_done = 1;
  }
}

static void dealloc_compressor_data(VP9_COMP *cpi) {
  // Delete sementation map
  vpx_free(cpi->segmentation_map);
  cpi->segmentation_map = 0;
  vpx_free(cpi->common.last_frame_seg_map);
  cpi->common.last_frame_seg_map = 0;
  vpx_free(cpi->coding_context.last_frame_seg_map_copy);
  cpi->coding_context.last_frame_seg_map_copy = 0;

  vpx_free(cpi->complexity_map);
  cpi->complexity_map = 0;
  vpx_free(cpi->active_map);
  cpi->active_map = 0;

  vp9_free_frame_buffers(&cpi->common);

  vp9_free_frame_buffer(&cpi->last_frame_uf);
  vp9_free_frame_buffer(&cpi->scaled_source);
  vp9_free_frame_buffer(&cpi->alt_ref_buffer);
  vp9_lookahead_destroy(cpi->lookahead);

  vpx_free(cpi->tok);
  cpi->tok = 0;

  // Activity mask based per mb zbin adjustments
  vpx_free(cpi->mb_activity_map);
  cpi->mb_activity_map = 0;
  vpx_free(cpi->mb_norm_activity_map);
  cpi->mb_norm_activity_map = 0;

  vpx_free(cpi->above_context[0]);
  cpi->above_context[0] = NULL;

  vpx_free(cpi->above_seg_context);
  cpi->above_seg_context = NULL;
}

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a target value
// target q value
int vp9_compute_qdelta(const VP9_COMP *cpi, double qstart, double qtarget) {
  int i;
  int start_index = cpi->rc.worst_quality;
  int target_index = cpi->rc.worst_quality;

  // Convert the average q value to an index.
  for (i = cpi->rc.best_quality; i < cpi->rc.worst_quality; i++) {
    start_index = i;
    if (vp9_convert_qindex_to_q(i) >= qstart)
      break;
  }

  // Convert the q target to an index
  for (i = cpi->rc.best_quality; i < cpi->rc.worst_quality; i++) {
    target_index = i;
    if (vp9_convert_qindex_to_q(i) >= qtarget)
      break;
  }

  return target_index - start_index;
}

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a value that should equate to thegiven rate ratio.

int vp9_compute_qdelta_by_rate(VP9_COMP *cpi,
                               double base_q_index, double rate_target_ratio) {
  int i;
  int base_bits_per_mb;
  int target_bits_per_mb;
  int target_index = cpi->rc.worst_quality;

  // Make SURE use of floating point in this function is safe.
  vp9_clear_system_state();

  // Look up the current projected bits per block for the base index
  base_bits_per_mb = vp9_rc_bits_per_mb(cpi->common.frame_type,
                                        base_q_index, 1.0);

  // Find the target bits per mb based on the base value and given ratio.
  target_bits_per_mb = rate_target_ratio * base_bits_per_mb;

  // Convert the q target to an index
  for (i = cpi->rc.best_quality; i < cpi->rc.worst_quality; i++) {
    target_index = i;
    if (vp9_rc_bits_per_mb(cpi->common.frame_type,
                           i, 1.0) <= target_bits_per_mb )
      break;
  }

  return target_index - base_q_index;
}

// This function sets up a set of segments with delta Q values around
// the baseline frame quantizer.
static void setup_in_frame_q_adj(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;
  // double q_ratio;
  int segment;
  int qindex_delta;

  // Make SURE use of floating point in this function is safe.
  vp9_clear_system_state();

  if (cm->frame_type == KEY_FRAME ||
      cpi->refresh_alt_ref_frame ||
      (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
    // Clear down the segment map
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

    // Clear down the complexity map used for rd
    vpx_memset(cpi->complexity_map, 0, cm->mi_rows * cm->mi_cols);

    vp9_enable_segmentation((VP9_PTR)cpi);
    vp9_clearall_segfeatures(seg);

    // Select delta coding method
    seg->abs_delta = SEGMENT_DELTADATA;

    // Segment 0 "Q" feature is disabled so it defaults to the baseline Q
    vp9_disable_segfeature(seg, 0, SEG_LVL_ALT_Q);

    // Use some of the segments for in frame Q adjustment
    for (segment = 1; segment < 2; segment++) {
      qindex_delta =
        vp9_compute_qdelta_by_rate(cpi, cm->base_qindex,
                                   in_frame_q_adj_ratio[segment]);
      vp9_enable_segfeature(seg, segment, SEG_LVL_ALT_Q);
      vp9_set_segdata(seg, segment, SEG_LVL_ALT_Q, qindex_delta);
    }
  }
}
static void configure_static_seg_features(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;

  int high_q = (int)(cpi->rc.avg_q > 48.0);
  int qi_delta;

  // Disable and clear down for KF
  if (cm->frame_type == KEY_FRAME) {
    // Clear down the global segmentation map
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation
    vp9_disable_segmentation((VP9_PTR)cpi);

    // Clear down the segment features.
    vp9_clearall_segfeatures(seg);
  } else if (cpi->refresh_alt_ref_frame) {
    // If this is an alt ref frame
    // Clear down the global segmentation map
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation and individual segment features by default
    vp9_disable_segmentation((VP9_PTR)cpi);
    vp9_clearall_segfeatures(seg);

    // Scan frames from current to arf frame.
    // This function re-enables segmentation if appropriate.
    vp9_update_mbgraph_stats(cpi);

    // If segmentation was enabled set those features needed for the
    // arf itself.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;

      qi_delta = vp9_compute_qdelta(
          cpi, cpi->rc.avg_q, (cpi->rc.avg_q * 0.875));
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, (qi_delta - 2));
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);

      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);
      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

      // Where relevant assume segment data is delta data
      seg->abs_delta = SEGMENT_DELTADATA;
    }
  } else if (seg->enabled) {
    // All other frames if segmentation has been enabled

    // First normal frame in a valid gf or alt ref group
    if (cpi->rc.frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (cpi->rc.source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;
        seg->abs_delta = SEGMENT_DELTADATA;

        qi_delta = vp9_compute_qdelta(cpi, cpi->rc.avg_q,
                                      (cpi->rc.avg_q * 1.125));
        vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, (qi_delta + 2));
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

        vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

        // Segment coding disabled for compred testing
        if (high_q || (cpi->static_mb_pct == 100)) {
          vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
        }
      } else {
        // Disable segmentation and clear down features if alt ref
        // is not active for this group

        vp9_disable_segmentation((VP9_PTR)cpi);

        vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

        seg->update_map = 0;
        seg->update_data = 0;

        vp9_clearall_segfeatures(seg);
      }
    } else if (cpi->rc.is_src_frame_alt_ref) {
      // Special case where we are coding over the top of a previous
      // alt ref frame.
      // Segment coding disabled for compred testing

      // Enable ref frame features for segment 0 as well
      vp9_enable_segfeature(seg, 0, SEG_LVL_REF_FRAME);
      vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);

      // All mbs should use ALTREF_FRAME
      vp9_clear_segdata(seg, 0, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 0, SEG_LVL_REF_FRAME, ALTREF_FRAME);
      vp9_clear_segdata(seg, 1, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);

      // Skip all MBs if high Q (0,0 mv and skip coeffs)
      if (high_q) {
        vp9_enable_segfeature(seg, 0, SEG_LVL_SKIP);
        vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
      }
      // Enable data update
      seg->update_data = 1;
    } else {
      // All other frames.

      // No updates.. leave things as they are.
      seg->update_map = 0;
      seg->update_data = 0;
    }
  }
}

// DEBUG: Print out the segment id of each MB in the current frame.
static void print_seg_map(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int row, col;
  int map_index = 0;
  FILE *statsfile = fopen("segmap.stt", "a");

  fprintf(statsfile, "%10d\n", cm->current_video_frame);

  for (row = 0; row < cpi->common.mi_rows; row++) {
    for (col = 0; col < cpi->common.mi_cols; col++) {
      fprintf(statsfile, "%10d", cpi->segmentation_map[map_index]);
      map_index++;
    }
    fprintf(statsfile, "\n");
  }
  fprintf(statsfile, "\n");

  fclose(statsfile);
}

static void update_reference_segmentation_map(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  int row, col;
  MODE_INFO **mi_8x8, **mi_8x8_ptr = cm->mi_grid_visible;
  uint8_t *cache_ptr = cm->last_frame_seg_map, *cache;

  for (row = 0; row < cm->mi_rows; row++) {
    mi_8x8 = mi_8x8_ptr;
    cache = cache_ptr;
    for (col = 0; col < cm->mi_cols; col++, mi_8x8++, cache++)
      cache[0] = mi_8x8[0]->mbmi.segment_id;
    mi_8x8_ptr += cm->mode_info_stride;
    cache_ptr += cm->mi_cols;
  }
}
static int is_slowest_mode(int mode) {
  return (mode == MODE_SECONDPASS_BEST || mode == MODE_BESTQUALITY);
}

static void set_rd_speed_thresholds(VP9_COMP *cpi) {
  SPEED_FEATURES *sf = &cpi->sf;
  int i;

  // Set baseline threshold values
  for (i = 0; i < MAX_MODES; ++i)
    sf->thresh_mult[i] = is_slowest_mode(cpi->oxcf.mode) ? -500 : 0;

  sf->thresh_mult[THR_NEARESTMV] = 0;
  sf->thresh_mult[THR_NEARESTG] = 0;
  sf->thresh_mult[THR_NEARESTA] = 0;

  sf->thresh_mult[THR_DC] += 1000;

  sf->thresh_mult[THR_NEWMV] += 1000;
  sf->thresh_mult[THR_NEWA] += 1000;
  sf->thresh_mult[THR_NEWG] += 1000;

  sf->thresh_mult[THR_NEARMV] += 1000;
  sf->thresh_mult[THR_NEARA] += 1000;
  sf->thresh_mult[THR_COMP_NEARESTLA] += 1000;
  sf->thresh_mult[THR_COMP_NEARESTGA] += 1000;

  sf->thresh_mult[THR_TM] += 1000;

  sf->thresh_mult[THR_COMP_NEARLA] += 1500;
  sf->thresh_mult[THR_COMP_NEWLA] += 2000;
  sf->thresh_mult[THR_NEARG] += 1000;
  sf->thresh_mult[THR_COMP_NEARGA] += 1500;
  sf->thresh_mult[THR_COMP_NEWGA] += 2000;

  sf->thresh_mult[THR_ZEROMV] += 2000;
  sf->thresh_mult[THR_ZEROG] += 2000;
  sf->thresh_mult[THR_ZEROA] += 2000;
  sf->thresh_mult[THR_COMP_ZEROLA] += 2500;
  sf->thresh_mult[THR_COMP_ZEROGA] += 2500;

  sf->thresh_mult[THR_H_PRED] += 2000;
  sf->thresh_mult[THR_V_PRED] += 2000;
  sf->thresh_mult[THR_D45_PRED ] += 2500;
  sf->thresh_mult[THR_D135_PRED] += 2500;
  sf->thresh_mult[THR_D117_PRED] += 2500;
  sf->thresh_mult[THR_D153_PRED] += 2500;
  sf->thresh_mult[THR_D207_PRED] += 2500;
  sf->thresh_mult[THR_D63_PRED] += 2500;

  /* disable frame modes if flags not set */
  if (!(cpi->ref_frame_flags & VP9_LAST_FLAG)) {
    sf->thresh_mult[THR_NEWMV    ] = INT_MAX;
    sf->thresh_mult[THR_NEARESTMV] = INT_MAX;
    sf->thresh_mult[THR_ZEROMV   ] = INT_MAX;
    sf->thresh_mult[THR_NEARMV   ] = INT_MAX;
  }
  if (!(cpi->ref_frame_flags & VP9_GOLD_FLAG)) {
    sf->thresh_mult[THR_NEARESTG ] = INT_MAX;
    sf->thresh_mult[THR_ZEROG    ] = INT_MAX;
    sf->thresh_mult[THR_NEARG    ] = INT_MAX;
    sf->thresh_mult[THR_NEWG     ] = INT_MAX;
  }
  if (!(cpi->ref_frame_flags & VP9_ALT_FLAG)) {
    sf->thresh_mult[THR_NEARESTA ] = INT_MAX;
    sf->thresh_mult[THR_ZEROA    ] = INT_MAX;
    sf->thresh_mult[THR_NEARA    ] = INT_MAX;
    sf->thresh_mult[THR_NEWA     ] = INT_MAX;
  }

  if ((cpi->ref_frame_flags & (VP9_LAST_FLAG | VP9_ALT_FLAG)) !=
      (VP9_LAST_FLAG | VP9_ALT_FLAG)) {
    sf->thresh_mult[THR_COMP_ZEROLA   ] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEARESTLA] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEARLA   ] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEWLA    ] = INT_MAX;
  }
  if ((cpi->ref_frame_flags & (VP9_GOLD_FLAG | VP9_ALT_FLAG)) !=
      (VP9_GOLD_FLAG | VP9_ALT_FLAG)) {
    sf->thresh_mult[THR_COMP_ZEROGA   ] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEARESTGA] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEARGA   ] = INT_MAX;
    sf->thresh_mult[THR_COMP_NEWGA    ] = INT_MAX;
  }
}

static void set_rd_speed_thresholds_sub8x8(VP9_COMP *cpi) {
  SPEED_FEATURES *sf = &cpi->sf;
  int i;

  for (i = 0; i < MAX_REFS; ++i)
    sf->thresh_mult_sub8x8[i] = is_slowest_mode(cpi->oxcf.mode)  ? -500 : 0;

  sf->thresh_mult_sub8x8[THR_LAST] += 2500;
  sf->thresh_mult_sub8x8[THR_GOLD] += 2500;
  sf->thresh_mult_sub8x8[THR_ALTR] += 2500;
  sf->thresh_mult_sub8x8[THR_INTRA] += 2500;
  sf->thresh_mult_sub8x8[THR_COMP_LA] += 4500;
  sf->thresh_mult_sub8x8[THR_COMP_GA] += 4500;

  // Check for masked out split cases.
  for (i = 0; i < MAX_REFS; i++) {
    if (sf->disable_split_mask & (1 << i))
      sf->thresh_mult_sub8x8[i] = INT_MAX;
  }

  // disable mode test if frame flag is not set
  if (!(cpi->ref_frame_flags & VP9_LAST_FLAG))
    sf->thresh_mult_sub8x8[THR_LAST] = INT_MAX;
  if (!(cpi->ref_frame_flags & VP9_GOLD_FLAG))
    sf->thresh_mult_sub8x8[THR_GOLD] = INT_MAX;
  if (!(cpi->ref_frame_flags & VP9_ALT_FLAG))
    sf->thresh_mult_sub8x8[THR_ALTR] = INT_MAX;
  if ((cpi->ref_frame_flags & (VP9_LAST_FLAG | VP9_ALT_FLAG)) !=
      (VP9_LAST_FLAG | VP9_ALT_FLAG))
    sf->thresh_mult_sub8x8[THR_COMP_LA] = INT_MAX;
  if ((cpi->ref_frame_flags & (VP9_GOLD_FLAG | VP9_ALT_FLAG)) !=
      (VP9_GOLD_FLAG | VP9_ALT_FLAG))
    sf->thresh_mult_sub8x8[THR_COMP_GA] = INT_MAX;
}

static void set_good_speed_feature(VP9_COMMON *cm,
                                   SPEED_FEATURES *sf,
                                   int speed) {
  int i;
  sf->adaptive_rd_thresh = 1;
  sf->recode_loop = ((speed < 1) ? ALLOW_RECODE : ALLOW_RECODE_KFMAXBW);
  if (speed == 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check  = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm)
      ? USE_FULL_RD : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ?
        DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 1;
    sf->auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }
  if (speed == 2) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check  = 1;
    sf->tx_size_search_method = frame_is_intra_only(cm)
      ? USE_FULL_RD : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ?
        DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_INTRA_LOWVAR;
    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 2;
    sf->reference_masking = 1;
    sf->auto_mv_step_size = 1;

    sf->disable_filter_search_var_thresh = 50;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;

    sf->adaptive_rd_thresh = 2;
    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->use_lp32x32fdct = 1;
    sf->mode_skip_start = 11;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
  }
  if (speed == 3) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
    else
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
      FLAG_SKIP_INTRA_BESTINTER |
      FLAG_SKIP_COMP_BESTINTRA |
      FLAG_SKIP_INTRA_LOWVAR;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 2;
    sf->reference_masking = 1;
    sf->auto_mv_step_size = 1;

    sf->disable_filter_search_var_thresh = 100;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;

    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->use_lp32x32fdct = 1;
    sf->subpel_iters_per_step = 1;
    sf->use_fast_coef_updates = 2;

    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
  }
  if (speed == 4) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
      FLAG_SKIP_INTRA_BESTINTER |
      FLAG_SKIP_COMP_BESTINTRA |
      FLAG_SKIP_COMP_REFMISMATCH |
      FLAG_SKIP_INTRA_LOWVAR |
      FLAG_EARLY_TERMINATE;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 2;
    sf->reference_masking = 1;
    sf->auto_mv_step_size = 1;

    sf->disable_filter_search_var_thresh = 200;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;

    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->use_lp32x32fdct = 1;
    sf->subpel_iters_per_step = 1;
    sf->use_fast_coef_updates = 2;

    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
  }
  if (speed == 5) {
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
    sf->use_one_partition_size_always = 1;
    sf->always_this_block_size = BLOCK_16X16;
    sf->tx_size_search_method = frame_is_intra_only(cm) ?
      USE_FULL_RD : USE_LARGESTALL;
    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                 FLAG_SKIP_INTRA_BESTINTER |
                                 FLAG_SKIP_COMP_BESTINTRA |
                                 FLAG_SKIP_COMP_REFMISMATCH |
                                 FLAG_SKIP_INTRA_LOWVAR |
                                 FLAG_EARLY_TERMINATE;
    sf->use_rd_breakout = 1;
    sf->use_lp32x32fdct = 1;
    sf->optimize_coefficients = 0;
    sf->auto_mv_step_size = 1;
    sf->reference_masking = 1;

    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->search_method = HEX;
    sf->subpel_iters_per_step = 1;
    sf->disable_split_var_thresh = 64;
    sf->disable_filter_search_var_thresh = 500;
    for (i = 0; i < TX_SIZES; i++) {
      sf->intra_y_mode_mask[i] = INTRA_DC_ONLY;
      sf->intra_uv_mode_mask[i] = INTRA_DC_ONLY;
    }
    sf->use_fast_coef_updates = 2;
    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
  }
}
static void set_rt_speed_feature(VP9_COMMON *cm,
                                 SPEED_FEATURES *sf,
                                 int speed) {
  sf->static_segmentation = 0;
  sf->adaptive_rd_thresh = 1;
  sf->recode_loop = ((speed < 1) ? ALLOW_RECODE : ALLOW_RECODE_KFMAXBW);
  sf->encode_breakout_thresh = 1;

  if (speed == 1) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method =
        frame_is_intra_only(cm) ? USE_FULL_RD : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ?
        DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 1;
    sf->auto_mv_step_size = 1;
    sf->adaptive_rd_thresh = 2;
    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->encode_breakout_thresh = 8;
  }
  if (speed >= 2) {
    sf->use_square_partition_only = !frame_is_intra_only(cm);
    sf->less_rectangular_check = 1;
    sf->tx_size_search_method =
        frame_is_intra_only(cm) ? USE_FULL_RD : USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = cm->show_frame ?
        DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
    else
      sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH
        | FLAG_SKIP_INTRA_BESTINTER | FLAG_SKIP_COMP_BESTINTRA
        | FLAG_SKIP_INTRA_LOWVAR;

    sf->use_rd_breakout = 1;
    sf->adaptive_motion_search = 1;
    sf->adaptive_pred_interp_filter = 2;
    sf->auto_mv_step_size = 1;
    sf->reference_masking = 1;

    sf->disable_filter_search_var_thresh = 50;
    sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

    sf->auto_min_max_partition_size = RELAXED_NEIGHBORING_MIN_MAX;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
    sf->adjust_partitioning_from_last_frame = 1;
    sf->last_partitioning_redo_frequency = 3;

    sf->adaptive_rd_thresh = 2;
    sf->recode_loop = ALLOW_RECODE_KFARFGF;
    sf->use_lp32x32fdct = 1;
    sf->mode_skip_start = 11;
    sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_y_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
    sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
    sf->encode_breakout_thresh = 200;
  }
  if (speed >= 3) {
    sf->use_square_partition_only = 1;
    sf->tx_size_search_method = USE_LARGESTALL;

    if (MIN(cm->width, cm->height) >= 720)
      sf->disable_split_mask = DISABLE_ALL_SPLIT;
    else
      sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;

    sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH
        | FLAG_SKIP_INTRA_BESTINTER | FLAG_SKIP_COMP_BESTINTRA
        | FLAG_SKIP_INTRA_LOWVAR;

    sf->disable_filter_search_var_thresh = 100;
    sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_ALL;
    sf->use_uv_intra_rd_estimate = 1;
    sf->skip_encode_sb = 1;
    sf->subpel_iters_per_step = 1;
    sf->use_fast_coef_updates = 2;
    sf->adaptive_rd_thresh = 4;
    sf->mode_skip_start = 6;
    sf->encode_breakout_thresh = 400;
  }
  if (speed >= 4) {
    sf->optimize_coefficients = 0;
    sf->disable_split_mask = DISABLE_ALL_SPLIT;
    sf->use_fast_lpf_pick = 2;
    sf->encode_breakout_thresh = 700;
  }
  if (speed >= 5) {
    int i;
    sf->adaptive_rd_thresh = 5;
    sf->auto_min_max_partition_size = frame_is_intra_only(cm) ?
        RELAXED_NEIGHBORING_MIN_MAX : STRICT_NEIGHBORING_MIN_MAX;
    sf->subpel_force_stop = 1;
    for (i = 0; i < TX_SIZES; i++) {
      sf->intra_y_mode_mask[i] = INTRA_DC_H_V;
      sf->intra_uv_mode_mask[i] = INTRA_DC_ONLY;
    }
    sf->frame_parameter_update = 0;
    sf->encode_breakout_thresh = 1000;
  }
  if (speed >= 6) {
    sf->always_this_block_size = BLOCK_16X16;
    sf->use_pick_mode = 1;
    sf->encode_breakout_thresh = 1000;
  }
}

void vp9_set_speed_features(VP9_COMP *cpi) {
  SPEED_FEATURES *sf = &cpi->sf;
  VP9_COMMON *cm = &cpi->common;
  int speed = cpi->speed;
  int i;

  // Convert negative speed to positive
  if (speed < 0)
    speed = -speed;

  for (i = 0; i < MAX_MODES; ++i)
    cpi->mode_chosen_counts[i] = 0;

  // best quality defaults
  sf->frame_parameter_update = 1;
  sf->search_method = NSTEP;
  sf->recode_loop = ALLOW_RECODE;
  sf->subpel_search_method = SUBPEL_TREE;
  sf->subpel_iters_per_step = 2;
  sf->subpel_force_stop = 0;
  sf->optimize_coefficients = !cpi->oxcf.lossless;
  sf->reduce_first_step_size = 0;
  sf->auto_mv_step_size = 0;
  sf->max_step_search_steps = MAX_MVSEARCH_STEPS;
  sf->comp_inter_joint_search_thresh = BLOCK_4X4;
  sf->adaptive_rd_thresh = 0;
  sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_OFF;
  sf->tx_size_search_method = USE_FULL_RD;
  sf->use_lp32x32fdct = 0;
  sf->adaptive_motion_search = 0;
  sf->adaptive_pred_interp_filter = 0;
  sf->reference_masking = 0;
  sf->use_one_partition_size_always = 0;
  sf->less_rectangular_check = 0;
  sf->use_square_partition_only = 0;
  sf->auto_min_max_partition_size = NOT_IN_USE;
  sf->max_partition_size = BLOCK_64X64;
  sf->min_partition_size = BLOCK_4X4;
  sf->adjust_partitioning_from_last_frame = 0;
  sf->last_partitioning_redo_frequency = 4;
  sf->disable_split_mask = 0;
  sf->mode_search_skip_flags = 0;
  sf->disable_split_var_thresh = 0;
  sf->disable_filter_search_var_thresh = 0;
  for (i = 0; i < TX_SIZES; i++) {
    sf->intra_y_mode_mask[i] = ALL_INTRA_MODES;
    sf->intra_uv_mode_mask[i] = ALL_INTRA_MODES;
  }
  sf->use_rd_breakout = 0;
  sf->skip_encode_sb = 0;
  sf->use_uv_intra_rd_estimate = 0;
  sf->use_fast_lpf_pick = 0;
  sf->use_fast_coef_updates = 0;
  sf->mode_skip_start = MAX_MODES;  // Mode index at which mode skip mask set
  sf->use_pick_mode = 0;
  sf->encode_breakout_thresh = 0;

  switch (cpi->oxcf.mode) {
    case MODE_BESTQUALITY:
    case MODE_SECONDPASS_BEST:  // This is the best quality mode.
      cpi->diamond_search_sad = vp9_full_range_search;
      break;
    case MODE_FIRSTPASS:
    case MODE_GOODQUALITY:
    case MODE_SECONDPASS:
      set_good_speed_feature(cm, sf, speed);
      break;
    case MODE_REALTIME:
      set_rt_speed_feature(cm, sf, speed);
      break;
  }; /* switch */

  // Set rd thresholds based on mode and speed setting
  set_rd_speed_thresholds(cpi);
  set_rd_speed_thresholds_sub8x8(cpi);

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (cpi->pass == 1) {
    sf->optimize_coefficients = 0;
  }

  // No recode for 1 pass.
  if (cpi->pass == 0) {
    sf->recode_loop = DISALLOW_RECODE;
    sf->optimize_coefficients = 0;
  }

  cpi->mb.fwd_txm4x4 = vp9_fdct4x4;
  if (cpi->oxcf.lossless || cpi->mb.e_mbd.lossless) {
    cpi->mb.fwd_txm4x4 = vp9_fwht4x4;
  }

  if (cpi->sf.subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree;
    cpi->find_fractional_mv_step_comp = vp9_find_best_sub_pixel_comp_tree;
  }

  cpi->mb.optimize = cpi->sf.optimize_coefficients == 1 && cpi->pass != 1;

  if (cpi->encode_breakout && cpi->oxcf.mode == MODE_REALTIME &&
      sf->encode_breakout_thresh > cpi->encode_breakout)
    cpi->encode_breakout = sf->encode_breakout_thresh;
}

static void alloc_raw_frame_buffers(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  cpi->lookahead = vp9_lookahead_init(cpi->oxcf.width, cpi->oxcf.height,
                                      cm->subsampling_x, cm->subsampling_y,
                                      cpi->oxcf.lag_in_frames);
  if (!cpi->lookahead)
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  if (vp9_realloc_frame_buffer(&cpi->alt_ref_buffer,
                               cpi->oxcf.width, cpi->oxcf.height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

void vp9_alloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  if (vp9_alloc_frame_buffers(cm, cm->width, cm->height))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffers");

  if (vp9_alloc_frame_buffer(&cpi->last_frame_uf,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9_ENC_BORDER_IN_PIXELS))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

  if (vp9_alloc_frame_buffer(&cpi->scaled_source,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9_ENC_BORDER_IN_PIXELS))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled source buffer");

  vpx_free(cpi->tok);

  {
    unsigned int tokens = get_token_alloc(cm->mb_rows, cm->mb_cols);

    CHECK_MEM_ERROR(cm, cpi->tok, vpx_calloc(tokens, sizeof(*cpi->tok)));
  }

  vpx_free(cpi->mb_activity_map);
  CHECK_MEM_ERROR(cm, cpi->mb_activity_map,
                  vpx_calloc(sizeof(unsigned int),
                             cm->mb_rows * cm->mb_cols));

  vpx_free(cpi->mb_norm_activity_map);
  CHECK_MEM_ERROR(cm, cpi->mb_norm_activity_map,
                  vpx_calloc(sizeof(unsigned int),
                             cm->mb_rows * cm->mb_cols));

  // 2 contexts per 'mi unit', so that we have one context per 4x4 txfm
  // block where mi unit size is 8x8.
  vpx_free(cpi->above_context[0]);
  CHECK_MEM_ERROR(cm, cpi->above_context[0],
                  vpx_calloc(2 * mi_cols_aligned_to_sb(cm->mi_cols) *
                             MAX_MB_PLANE,
                             sizeof(*cpi->above_context[0])));

  vpx_free(cpi->above_seg_context);
  CHECK_MEM_ERROR(cm, cpi->above_seg_context,
                  vpx_calloc(mi_cols_aligned_to_sb(cm->mi_cols),
                             sizeof(*cpi->above_seg_context)));
}


static void update_frame_size(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  vp9_update_frame_size(cm);

  // Update size of buffers local to this frame
  if (vp9_realloc_frame_buffer(&cpi->last_frame_uf,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate last frame buffer");

  if (vp9_realloc_frame_buffer(&cpi->scaled_source,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate scaled source buffer");

  {
    int y_stride = cpi->scaled_source.y_stride;

    if (cpi->sf.search_method == NSTEP) {
      vp9_init3smotion_compensation(&cpi->mb, y_stride);
    } else if (cpi->sf.search_method == DIAMOND) {
      vp9_init_dsmotion_compensation(&cpi->mb, y_stride);
    }
  }

  {
    int i;
    for (i = 1; i < MAX_MB_PLANE; ++i) {
      cpi->above_context[i] = cpi->above_context[0] +
                              i * sizeof(*cpi->above_context[0]) * 2 *
                              mi_cols_aligned_to_sb(cm->mi_cols);
    }
  }
}


// Table that converts 0-63 Q range values passed in outside to the Qindex
// range used internally.
static const int q_trans[] = {
  0,    4,   8,  12,  16,  20,  24,  28,
  32,   36,  40,  44,  48,  52,  56,  60,
  64,   68,  72,  76,  80,  84,  88,  92,
  96,  100, 104, 108, 112, 116, 120, 124,
  128, 132, 136, 140, 144, 148, 152, 156,
  160, 164, 168, 172, 176, 180, 184, 188,
  192, 196, 200, 204, 208, 212, 216, 220,
  224, 228, 232, 236, 240, 244, 249, 255,
};

int vp9_reverse_trans(int x) {
  int i;

  for (i = 0; i < 64; i++)
    if (q_trans[i] >= x)
      return i;

  return 63;
};

void vp9_new_framerate(VP9_COMP *cpi, double framerate) {
  VP9_COMMON *const cm = &cpi->common;
  int64_t vbr_max_bits;

  if (framerate < 0.1)
    framerate = 30;

  cpi->oxcf.framerate = framerate;
  cpi->output_framerate = cpi->oxcf.framerate;
  cpi->rc.av_per_frame_bandwidth = (int)(cpi->oxcf.target_bandwidth
                                         / cpi->output_framerate);
  cpi->rc.min_frame_bandwidth = (int)(cpi->rc.av_per_frame_bandwidth *
                                      cpi->oxcf.two_pass_vbrmin_section / 100);


  cpi->rc.min_frame_bandwidth = MAX(cpi->rc.min_frame_bandwidth,
                                    FRAME_OVERHEAD_BITS);

  // A maximum bitrate for a frame is defined.
  // The baseline for this aligns with HW implementations that
  // can support decode of 1080P content up to a bitrate of MAX_MB_RATE bits
  // per 16x16 MB (averaged over a frame). However this limit is extended if
  // a very high rate is given on the command line or the the rate cannnot
  // be acheived because of a user specificed max q (e.g. when the user
  // specifies lossless encode.
  //
  vbr_max_bits = ((int64_t)cpi->rc.av_per_frame_bandwidth *
                  (int64_t)cpi->oxcf.two_pass_vbrmax_section) / 100;
  cpi->rc.max_frame_bandwidth =
    MAX(MAX((cm->MBs * MAX_MB_RATE), MAXRATE_1080P), vbr_max_bits);

  // Set Maximum gf/arf interval
  cpi->rc.max_gf_interval = 16;

  // Extended interval for genuinely static scenes
  cpi->twopass.static_scene_max_gf_interval = cpi->key_frame_frequency >> 1;

  // Special conditions when alt ref frame enabled in lagged compress mode
  if (cpi->oxcf.play_alternate && cpi->oxcf.lag_in_frames) {
    if (cpi->rc.max_gf_interval > cpi->oxcf.lag_in_frames - 1)
      cpi->rc.max_gf_interval = cpi->oxcf.lag_in_frames - 1;

    if (cpi->twopass.static_scene_max_gf_interval > cpi->oxcf.lag_in_frames - 1)
      cpi->twopass.static_scene_max_gf_interval = cpi->oxcf.lag_in_frames - 1;
  }

  if (cpi->rc.max_gf_interval > cpi->twopass.static_scene_max_gf_interval)
    cpi->rc.max_gf_interval = cpi->twopass.static_scene_max_gf_interval;
}

static int64_t rescale(int val, int64_t num, int denom) {
  int64_t llnum = num;
  int64_t llden = denom;
  int64_t llval = val;

  return (llval * llnum / llden);
}

// Initialize layer context data from init_config().
static void init_layer_context(VP9_COMP *const cpi) {
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  int temporal_layer = 0;
  cpi->svc.spatial_layer_id = 0;
  cpi->svc.temporal_layer_id = 0;
  for (temporal_layer = 0; temporal_layer < cpi->svc.number_temporal_layers;
      ++temporal_layer) {
    LAYER_CONTEXT *const lc = &cpi->svc.layer_context[temporal_layer];
    RATE_CONTROL *const lrc = &lc->rc;
    lrc->avg_frame_qindex[INTER_FRAME] = q_trans[oxcf->worst_allowed_q];
    lrc->last_q[INTER_FRAME] = q_trans[oxcf->worst_allowed_q];
    lrc->ni_av_qi = q_trans[oxcf->worst_allowed_q];
    lrc->total_actual_bits = 0;
    lrc->total_target_vs_actual = 0;
    lrc->ni_tot_qi = 0;
    lrc->tot_q = 0.0;
    lrc->avg_q = 0.0;
    lrc->ni_frames = 0;
    lrc->decimation_count = 0;
    lrc->decimation_factor = 0;
    lrc->rate_correction_factor = 1.0;
    lrc->key_frame_rate_correction_factor = 1.0;
    lc->target_bandwidth = oxcf->ts_target_bitrate[temporal_layer] *
        1000;
    lrc->buffer_level = rescale((int)(oxcf->starting_buffer_level),
                               lc->target_bandwidth, 1000);
    lrc->bits_off_target = lrc->buffer_level;
  }
}

// Update the layer context from a change_config() call.
static void update_layer_context_change_config(VP9_COMP *const cpi,
                                               const int target_bandwidth) {
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  const RATE_CONTROL *const rc = &cpi->rc;
  int temporal_layer = 0;
  float bitrate_alloc = 1.0;
  for (temporal_layer = 0; temporal_layer < cpi->svc.number_temporal_layers;
      ++temporal_layer) {
    LAYER_CONTEXT *const lc = &cpi->svc.layer_context[temporal_layer];
    RATE_CONTROL *const lrc = &lc->rc;
    lc->target_bandwidth = oxcf->ts_target_bitrate[temporal_layer] * 1000;
    bitrate_alloc = (float)lc->target_bandwidth / (float)target_bandwidth;
    // Update buffer-related quantities.
    lc->starting_buffer_level = oxcf->starting_buffer_level * bitrate_alloc;
    lc->optimal_buffer_level = oxcf->optimal_buffer_level * bitrate_alloc;
    lc->maximum_buffer_size = oxcf->maximum_buffer_size * bitrate_alloc;
    lrc->bits_off_target = MIN(lrc->bits_off_target, lc->maximum_buffer_size);
    lrc->buffer_level = MIN(lrc->buffer_level, lc->maximum_buffer_size);
    // Update framerate-related quantities.
    lc->framerate = oxcf->framerate / oxcf->ts_rate_decimator[temporal_layer];
    lrc->av_per_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
    lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
    // Update qp-related quantities.
    lrc->worst_quality = rc->worst_quality;
    lrc->best_quality = rc->best_quality;
  }
}

// Prior to encoding the frame, update framerate-related quantities
// for the current layer.
static void update_layer_framerate(VP9_COMP *const cpi) {
  int temporal_layer = cpi->svc.temporal_layer_id;
  const VP9_CONFIG *const oxcf = &cpi->oxcf;
  LAYER_CONTEXT *const lc = &cpi->svc.layer_context[temporal_layer];
  RATE_CONTROL *const lrc = &lc->rc;
  lc->framerate = oxcf->framerate / oxcf->ts_rate_decimator[temporal_layer];
  lrc->av_per_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (temporal_layer == 0) {
    lc->avg_frame_size = lrc->av_per_frame_bandwidth;
  } else {
    double prev_layer_framerate = oxcf->framerate /
        oxcf->ts_rate_decimator[temporal_layer - 1];
    int prev_layer_target_bandwidth =
        oxcf->ts_target_bitrate[temporal_layer - 1] * 1000;
    lc->avg_frame_size =
        (int)(lc->target_bandwidth - prev_layer_target_bandwidth) /
        (lc->framerate - prev_layer_framerate);
  }
}

// Prior to encoding the frame, set the layer context, for the current layer
// to be encoded, to the cpi struct.
static void restore_layer_context(VP9_COMP *const cpi) {
  int temporal_layer = cpi->svc.temporal_layer_id;
  LAYER_CONTEXT *lc = &cpi->svc.layer_context[temporal_layer];
  int frame_since_key = cpi->rc.frames_since_key;
  int frame_to_key = cpi->rc.frames_to_key;
  cpi->rc = lc->rc;
  cpi->oxcf.target_bandwidth = lc->target_bandwidth;
  cpi->oxcf.starting_buffer_level = lc->starting_buffer_level;
  cpi->oxcf.optimal_buffer_level = lc->optimal_buffer_level;
  cpi->oxcf.maximum_buffer_size = lc->maximum_buffer_size;
  cpi->output_framerate = lc->framerate;
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  cpi->rc.frames_since_key = frame_since_key;
  cpi->rc.frames_to_key = frame_to_key;
}

// Save the layer context after encoding the frame.
static void save_layer_context(VP9_COMP *const cpi) {
  int temporal_layer = cpi->svc.temporal_layer_id;
  LAYER_CONTEXT *lc = &cpi->svc.layer_context[temporal_layer];
  lc->rc = cpi->rc;
  lc->target_bandwidth = cpi->oxcf.target_bandwidth;
  lc->starting_buffer_level = cpi->oxcf.starting_buffer_level;
  lc->optimal_buffer_level = cpi->oxcf.optimal_buffer_level;
  lc->maximum_buffer_size = cpi->oxcf.maximum_buffer_size;
  lc->framerate = cpi->output_framerate;
}

static void set_tile_limits(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;

  int min_log2_tile_cols, max_log2_tile_cols;
  vp9_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  cm->log2_tile_cols = clamp(cpi->oxcf.tile_columns,
                             min_log2_tile_cols, max_log2_tile_cols);
  cm->log2_tile_rows = cpi->oxcf.tile_rows;
}

static void init_config(VP9_PTR ptr, VP9_CONFIG *oxcf) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);
  VP9_COMMON *const cm = &cpi->common;
  int i;

  cpi->oxcf = *oxcf;

  cm->version = oxcf->version;

  cm->width = oxcf->width;
  cm->height = oxcf->height;
  cm->subsampling_x = 0;
  cm->subsampling_y = 0;
  vp9_alloc_compressor_data(cpi);

  // Spatial scalability.
  cpi->svc.number_spatial_layers = oxcf->ss_number_layers;
  // Temporal scalability.
  cpi->svc.number_temporal_layers = oxcf->ts_number_layers;

  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    init_layer_context(cpi);
  }

  // change includes all joint functionality
  vp9_change_config(ptr, oxcf);

  // Initialize active best and worst q and average q values.
  if (cpi->pass == 0 && cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    cpi->rc.avg_frame_qindex[0] = cpi->oxcf.worst_allowed_q;
    cpi->rc.avg_frame_qindex[1] = cpi->oxcf.worst_allowed_q;
    cpi->rc.avg_frame_qindex[2] = cpi->oxcf.worst_allowed_q;
  } else {
    cpi->rc.avg_frame_qindex[0] = (cpi->oxcf.worst_allowed_q +
                                   cpi->oxcf.best_allowed_q) / 2;
    cpi->rc.avg_frame_qindex[1] = (cpi->oxcf.worst_allowed_q +
                                   cpi->oxcf.best_allowed_q) / 2;
    cpi->rc.avg_frame_qindex[2] = (cpi->oxcf.worst_allowed_q +
                                   cpi->oxcf.best_allowed_q) / 2;
  }
  cpi->rc.last_q[0]                 = cpi->oxcf.best_allowed_q;
  cpi->rc.last_q[1]                 = cpi->oxcf.best_allowed_q;
  cpi->rc.last_q[2]                 = cpi->oxcf.best_allowed_q;

  // Initialise the starting buffer levels
  cpi->rc.buffer_level              = cpi->oxcf.starting_buffer_level;
  cpi->rc.bits_off_target           = cpi->oxcf.starting_buffer_level;

  cpi->rc.rolling_target_bits       = cpi->rc.av_per_frame_bandwidth;
  cpi->rc.rolling_actual_bits       = cpi->rc.av_per_frame_bandwidth;
  cpi->rc.long_rolling_target_bits  = cpi->rc.av_per_frame_bandwidth;
  cpi->rc.long_rolling_actual_bits  = cpi->rc.av_per_frame_bandwidth;

  cpi->rc.total_actual_bits         = 0;
  cpi->rc.total_target_vs_actual    = 0;

  cpi->static_mb_pct = 0;

  cpi->lst_fb_idx = 0;
  cpi->gld_fb_idx = 1;
  cpi->alt_fb_idx = 2;

  set_tile_limits(cpi);

  cpi->fixed_divide[0] = 0;
  for (i = 1; i < 512; i++)
    cpi->fixed_divide[i] = 0x80000 / i;
}

void vp9_change_config(VP9_PTR ptr, VP9_CONFIG *oxcf) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);
  VP9_COMMON *const cm = &cpi->common;

  if (!cpi || !oxcf)
    return;

  if (cm->version != oxcf->version) {
    cm->version = oxcf->version;
  }

  cpi->oxcf = *oxcf;

  switch (cpi->oxcf.mode) {
      // Real time and one pass deprecated in test code base
    case MODE_GOODQUALITY:
      cpi->pass = 0;
      cpi->oxcf.cpu_used = clamp(cpi->oxcf.cpu_used, -5, 5);
      break;

    case MODE_FIRSTPASS:
      cpi->pass = 1;
      break;

    case MODE_SECONDPASS:
      cpi->pass = 2;
      cpi->oxcf.cpu_used = clamp(cpi->oxcf.cpu_used, -5, 5);
      break;

    case MODE_SECONDPASS_BEST:
      cpi->pass = 2;
      break;

    case MODE_REALTIME:
      cpi->pass = 0;
      break;
  }

  cpi->oxcf.worst_allowed_q = q_trans[oxcf->worst_allowed_q];
  cpi->oxcf.best_allowed_q = q_trans[oxcf->best_allowed_q];
  cpi->oxcf.cq_level = q_trans[cpi->oxcf.cq_level];

  cpi->oxcf.lossless = oxcf->lossless;
  cpi->mb.e_mbd.itxm_add = cpi->oxcf.lossless ? vp9_iwht4x4_add
                                              : vp9_idct4x4_add;
  cpi->rc.baseline_gf_interval = DEFAULT_GF_INTERVAL;

  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  cpi->refresh_golden_frame = 0;
  cpi->refresh_last_frame = 1;
  cm->refresh_frame_context = 1;
  cm->reset_frame_context = 0;

  vp9_reset_segment_features(&cm->seg);
  set_high_precision_mv(cpi, 0);

  {
    int i;

    for (i = 0; i < MAX_SEGMENTS; i++)
      cpi->segment_encode_breakout[i] = cpi->oxcf.encode_breakout;
  }
  cpi->encode_breakout = cpi->oxcf.encode_breakout;

  // local file playback mode == really big buffer
  if (cpi->oxcf.end_usage == USAGE_LOCAL_FILE_PLAYBACK) {
    cpi->oxcf.starting_buffer_level   = 60000;
    cpi->oxcf.optimal_buffer_level    = 60000;
    cpi->oxcf.maximum_buffer_size     = 240000;
  }

  // Convert target bandwidth from Kbit/s to Bit/s
  cpi->oxcf.target_bandwidth       *= 1000;

  cpi->oxcf.starting_buffer_level = rescale(cpi->oxcf.starting_buffer_level,
                                            cpi->oxcf.target_bandwidth, 1000);

  // Set or reset optimal and maximum buffer levels.
  if (cpi->oxcf.optimal_buffer_level == 0)
    cpi->oxcf.optimal_buffer_level = cpi->oxcf.target_bandwidth / 8;
  else
    cpi->oxcf.optimal_buffer_level = rescale(cpi->oxcf.optimal_buffer_level,
                                             cpi->oxcf.target_bandwidth, 1000);

  if (cpi->oxcf.maximum_buffer_size == 0)
    cpi->oxcf.maximum_buffer_size = cpi->oxcf.target_bandwidth / 8;
  else
    cpi->oxcf.maximum_buffer_size = rescale(cpi->oxcf.maximum_buffer_size,
                                            cpi->oxcf.target_bandwidth, 1000);
  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  cpi->rc.bits_off_target = MIN(cpi->rc.bits_off_target,
                                cpi->oxcf.maximum_buffer_size);
  cpi->rc.buffer_level = MIN(cpi->rc.buffer_level,
                             cpi->oxcf.maximum_buffer_size);

  // Set up frame rate and related parameters rate control values.
  vp9_new_framerate(cpi, cpi->oxcf.framerate);

  // Set absolute upper and lower quality limits
  cpi->rc.worst_quality = cpi->oxcf.worst_allowed_q;
  cpi->rc.best_quality = cpi->oxcf.best_allowed_q;

  // active values should only be modified if out of new range

  cpi->cq_target_quality = cpi->oxcf.cq_level;

  cm->interp_filter = DEFAULT_INTERP_FILTER;

  cm->display_width = cpi->oxcf.width;
  cm->display_height = cpi->oxcf.height;

  // VP8 sharpness level mapping 0-7 (vs 0-10 in general VPx dialogs)
  cpi->oxcf.sharpness = MIN(7, cpi->oxcf.sharpness);

  cpi->common.lf.sharpness_level = cpi->oxcf.sharpness;

  if (cpi->initial_width) {
    // Increasing the size of the frame beyond the first seen frame, or some
    // otherwise signaled maximum size, is not supported.
    // TODO(jkoleszar): exit gracefully.
    assert(cm->width <= cpi->initial_width);
    assert(cm->height <= cpi->initial_height);
  }
  update_frame_size(cpi);

  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    update_layer_context_change_config(cpi, cpi->oxcf.target_bandwidth);
  }

  cpi->speed = cpi->oxcf.cpu_used;

  if (cpi->oxcf.lag_in_frames == 0) {
    // Force allow_lag to 0 if lag_in_frames is 0.
    cpi->oxcf.allow_lag = 0;
  } else if (cpi->oxcf.lag_in_frames > MAX_LAG_BUFFERS) {
     // Limit on lag buffers as these are not currently dynamically allocated.
    cpi->oxcf.lag_in_frames = MAX_LAG_BUFFERS;
  }

#if CONFIG_MULTIPLE_ARF
  vp9_zero(cpi->alt_ref_source);
#else
  cpi->alt_ref_source = NULL;
#endif
  cpi->rc.is_src_frame_alt_ref = 0;

#if 0
  // Experimental RD Code
  cpi->frame_distortion = 0;
  cpi->last_frame_distortion = 0;
#endif

  set_tile_limits(cpi);

  cpi->ext_refresh_frame_flags_pending = 0;
  cpi->ext_refresh_frame_context_pending = 0;
}

#define M_LOG2_E 0.693147180559945309417
#define log2f(x) (log (x) / (float) M_LOG2_E)

static void cal_nmvjointsadcost(int *mvjointsadcost) {
  mvjointsadcost[0] = 600;
  mvjointsadcost[1] = 300;
  mvjointsadcost[2] = 300;
  mvjointsadcost[0] = 300;
}

static void cal_nmvsadcosts(int *mvsadcost[2]) {
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}

static void cal_nmvsadcosts_hp(int *mvsadcost[2]) {
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}

static void alloc_mode_context(VP9_COMMON *cm, int num_4x4_blk,
                               PICK_MODE_CONTEXT *ctx) {
  int num_pix = num_4x4_blk << 4;
  int i, k;
  ctx->num_4x4_blk = num_4x4_blk;
  CHECK_MEM_ERROR(cm, ctx->zcoeff_blk,
                  vpx_calloc(num_4x4_blk, sizeof(uint8_t)));
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    for (k = 0; k < 3; ++k) {
      CHECK_MEM_ERROR(cm, ctx->coeff[i][k],
                      vpx_memalign(16, num_pix * sizeof(int16_t)));
      CHECK_MEM_ERROR(cm, ctx->qcoeff[i][k],
                      vpx_memalign(16, num_pix * sizeof(int16_t)));
      CHECK_MEM_ERROR(cm, ctx->dqcoeff[i][k],
                      vpx_memalign(16, num_pix * sizeof(int16_t)));
      CHECK_MEM_ERROR(cm, ctx->eobs[i][k],
                      vpx_memalign(16, num_pix * sizeof(uint16_t)));
      ctx->coeff_pbuf[i][k]   = ctx->coeff[i][k];
      ctx->qcoeff_pbuf[i][k]  = ctx->qcoeff[i][k];
      ctx->dqcoeff_pbuf[i][k] = ctx->dqcoeff[i][k];
      ctx->eobs_pbuf[i][k]    = ctx->eobs[i][k];
    }
  }
}

static void free_mode_context(PICK_MODE_CONTEXT *ctx) {
  int i, k;
  vpx_free(ctx->zcoeff_blk);
  ctx->zcoeff_blk = 0;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    for (k = 0; k < 3; ++k) {
      vpx_free(ctx->coeff[i][k]);
      ctx->coeff[i][k] = 0;
      vpx_free(ctx->qcoeff[i][k]);
      ctx->qcoeff[i][k] = 0;
      vpx_free(ctx->dqcoeff[i][k]);
      ctx->dqcoeff[i][k] = 0;
      vpx_free(ctx->eobs[i][k]);
      ctx->eobs[i][k] = 0;
    }
  }
}

static void init_pick_mode_context(VP9_COMP *cpi) {
  int i;
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x  = &cpi->mb;


  for (i = 0; i < BLOCK_SIZES; ++i) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[i];
    const int num_4x4_h = num_4x4_blocks_high_lookup[i];
    const int num_4x4_blk = MAX(4, num_4x4_w * num_4x4_h);
    if (i < BLOCK_16X16) {
      for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index) {
        for (x->mb_index = 0; x->mb_index < 4; ++x->mb_index) {
          for (x->b_index = 0; x->b_index < 16 / num_4x4_blk; ++x->b_index) {
            PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
            alloc_mode_context(cm, num_4x4_blk, ctx);
          }
        }
      }
    } else if (i < BLOCK_32X32) {
      for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index) {
        for (x->mb_index = 0; x->mb_index < 64 / num_4x4_blk; ++x->mb_index) {
          PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
          ctx->num_4x4_blk = num_4x4_blk;
          alloc_mode_context(cm, num_4x4_blk, ctx);
        }
      }
    } else if (i < BLOCK_64X64) {
      for (x->sb_index = 0; x->sb_index < 256 / num_4x4_blk; ++x->sb_index) {
        PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
        ctx->num_4x4_blk = num_4x4_blk;
        alloc_mode_context(cm, num_4x4_blk, ctx);
      }
    } else {
      PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
      ctx->num_4x4_blk = num_4x4_blk;
      alloc_mode_context(cm, num_4x4_blk, ctx);
    }
  }
}

static void free_pick_mode_context(MACROBLOCK *x) {
  int i;

  for (i = 0; i < BLOCK_SIZES; ++i) {
    const int num_4x4_w = num_4x4_blocks_wide_lookup[i];
    const int num_4x4_h = num_4x4_blocks_high_lookup[i];
    const int num_4x4_blk = MAX(4, num_4x4_w * num_4x4_h);
    if (i < BLOCK_16X16) {
      for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index) {
        for (x->mb_index = 0; x->mb_index < 4; ++x->mb_index) {
          for (x->b_index = 0; x->b_index < 16 / num_4x4_blk; ++x->b_index) {
            PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
            free_mode_context(ctx);
          }
        }
      }
    } else if (i < BLOCK_32X32) {
      for (x->sb_index = 0; x->sb_index < 4; ++x->sb_index) {
        for (x->mb_index = 0; x->mb_index < 64 / num_4x4_blk; ++x->mb_index) {
          PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
          free_mode_context(ctx);
        }
      }
    } else if (i < BLOCK_64X64) {
      for (x->sb_index = 0; x->sb_index < 256 / num_4x4_blk; ++x->sb_index) {
        PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
        free_mode_context(ctx);
      }
    } else {
      PICK_MODE_CONTEXT *ctx = get_block_context(x, i);
      free_mode_context(ctx);
    }
  }
}

VP9_PTR vp9_create_compressor(VP9_CONFIG *oxcf) {
  int i, j;
  volatile union {
    VP9_COMP *cpi;
    VP9_PTR   ptr;
  } ctx;

  VP9_COMP *cpi;
  VP9_COMMON *cm;

  cpi = ctx.cpi = vpx_memalign(32, sizeof(VP9_COMP));
  // Check that the CPI instance is valid
  if (!cpi)
    return 0;

  cm = &cpi->common;

  vp9_zero(*cpi);

  if (setjmp(cm->error.jmp)) {
    VP9_PTR ptr = ctx.ptr;

    ctx.cpi->common.error.setjmp = 0;
    vp9_remove_compressor(&ptr);
    return 0;
  }

  cm->error.setjmp = 1;

  CHECK_MEM_ERROR(cm, cpi->mb.ss, vpx_calloc(sizeof(search_site),
                                             (MAX_MVSEARCH_STEPS * 8) + 1));

  vp9_create_common(cm);

  cpi->use_svc = 0;

  init_config((VP9_PTR)cpi, oxcf);

  init_pick_mode_context(cpi);

  cm->current_video_frame   = 0;

  // Set reference frame sign bias for ALTREF frame to 1 (for now)
  cm->ref_frame_sign_bias[ALTREF_FRAME] = 1;

  cpi->rc.baseline_gf_interval = DEFAULT_GF_INTERVAL;

  cpi->gold_is_last = 0;
  cpi->alt_is_last  = 0;
  cpi->gold_is_alt  = 0;

  // Create the encoder segmentation map and set all entries to 0
  CHECK_MEM_ERROR(cm, cpi->segmentation_map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  // Create a complexity map used for rd adjustment
  CHECK_MEM_ERROR(cm, cpi->complexity_map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));


  // And a place holder structure is the coding context
  // for use if we want to save and restore it
  CHECK_MEM_ERROR(cm, cpi->coding_context.last_frame_seg_map_copy,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  CHECK_MEM_ERROR(cm, cpi->active_map, vpx_calloc(cm->MBs, 1));
  vpx_memset(cpi->active_map, 1, cm->MBs);
  cpi->active_map_enabled = 0;

  for (i = 0; i < (sizeof(cpi->mbgraph_stats) /
                   sizeof(cpi->mbgraph_stats[0])); i++) {
    CHECK_MEM_ERROR(cm, cpi->mbgraph_stats[i].mb_stats,
                    vpx_calloc(cm->MBs *
                               sizeof(*cpi->mbgraph_stats[i].mb_stats), 1));
  }

  /*Initialize the feed-forward activity masking.*/
  cpi->activity_avg = 90 << 12;
  cpi->key_frame_frequency = cpi->oxcf.key_freq;

  cpi->rc.frames_since_key = 8;  // Sensible default for first frame.
  cpi->rc.this_key_frame_forced = 0;
  cpi->rc.next_key_frame_forced = 0;

  cpi->rc.source_alt_ref_pending = 0;
  cpi->rc.source_alt_ref_active = 0;
  cpi->refresh_alt_ref_frame = 0;

#if CONFIG_MULTIPLE_ARF
  // Turn multiple ARF usage on/off. This is a quick hack for the initial test
  // version. It should eventually be set via the codec API.
  cpi->multi_arf_enabled = 1;

  if (cpi->multi_arf_enabled) {
    cpi->sequence_number = 0;
    cpi->frame_coding_order_period = 0;
    vp9_zero(cpi->frame_coding_order);
    vp9_zero(cpi->arf_buffer_idx);
  }
#endif

  cpi->b_calculate_psnr = CONFIG_INTERNAL_STATS;
#if CONFIG_INTERNAL_STATS
  cpi->b_calculate_ssimg = 0;

  cpi->count = 0;
  cpi->bytes = 0;

  if (cpi->b_calculate_psnr) {
    cpi->total_y = 0.0;
    cpi->total_u = 0.0;
    cpi->total_v = 0.0;
    cpi->total = 0.0;
    cpi->total_sq_error = 0;
    cpi->total_samples = 0;

    cpi->totalp_y = 0.0;
    cpi->totalp_u = 0.0;
    cpi->totalp_v = 0.0;
    cpi->totalp = 0.0;
    cpi->totalp_sq_error = 0;
    cpi->totalp_samples = 0;

    cpi->tot_recode_hits = 0;
    cpi->summed_quality = 0;
    cpi->summed_weights = 0;
    cpi->summedp_quality = 0;
    cpi->summedp_weights = 0;
  }

  if (cpi->b_calculate_ssimg) {
    cpi->total_ssimg_y = 0;
    cpi->total_ssimg_u = 0;
    cpi->total_ssimg_v = 0;
    cpi->total_ssimg_all = 0;
  }

#endif

  cpi->first_time_stamp_ever = INT64_MAX;

  cpi->rc.frames_till_gf_update_due      = 0;

  cpi->rc.ni_av_qi                     = cpi->oxcf.worst_allowed_q;
  cpi->rc.ni_tot_qi                    = 0;
  cpi->rc.ni_frames                   = 0;
  cpi->rc.tot_q = 0.0;
  cpi->rc.avg_q = vp9_convert_qindex_to_q(cpi->oxcf.worst_allowed_q);

  cpi->rc.rate_correction_factor         = 1.0;
  cpi->rc.key_frame_rate_correction_factor = 1.0;
  cpi->rc.gf_rate_correction_factor  = 1.0;

  cal_nmvjointsadcost(cpi->mb.nmvjointsadcost);
  cpi->mb.nmvcost[0] = &cpi->mb.nmvcosts[0][MV_MAX];
  cpi->mb.nmvcost[1] = &cpi->mb.nmvcosts[1][MV_MAX];
  cpi->mb.nmvsadcost[0] = &cpi->mb.nmvsadcosts[0][MV_MAX];
  cpi->mb.nmvsadcost[1] = &cpi->mb.nmvsadcosts[1][MV_MAX];
  cal_nmvsadcosts(cpi->mb.nmvsadcost);

  cpi->mb.nmvcost_hp[0] = &cpi->mb.nmvcosts_hp[0][MV_MAX];
  cpi->mb.nmvcost_hp[1] = &cpi->mb.nmvcosts_hp[1][MV_MAX];
  cpi->mb.nmvsadcost_hp[0] = &cpi->mb.nmvsadcosts_hp[0][MV_MAX];
  cpi->mb.nmvsadcost_hp[1] = &cpi->mb.nmvsadcosts_hp[1][MV_MAX];
  cal_nmvsadcosts_hp(cpi->mb.nmvsadcost_hp);

#ifdef OUTPUT_YUV_SRC
  yuv_file = fopen("bd.yuv", "ab");
#endif
#ifdef OUTPUT_YUV_REC
  yuv_rec_file = fopen("rec.yuv", "wb");
#endif

#if 0
  framepsnr = fopen("framepsnr.stt", "a");
  kf_list = fopen("kf_list.stt", "w");
#endif

  cpi->output_pkt_list = oxcf->output_pkt_list;

  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  if (cpi->pass == 1) {
    vp9_init_first_pass(cpi);
  } else if (cpi->pass == 2) {
    size_t packet_sz = sizeof(FIRSTPASS_STATS);
    int packets = (int)(oxcf->two_pass_stats_in.sz / packet_sz);

    cpi->twopass.stats_in_start = oxcf->two_pass_stats_in.buf;
    cpi->twopass.stats_in = cpi->twopass.stats_in_start;
    cpi->twopass.stats_in_end = (void *)((char *)cpi->twopass.stats_in
                                         + (packets - 1) * packet_sz);
    vp9_init_second_pass(cpi);
  }

  vp9_set_speed_features(cpi);

  // Default rd threshold factors for mode selection
  for (i = 0; i < BLOCK_SIZES; ++i) {
    for (j = 0; j < MAX_MODES; ++j)
      cpi->rd_thresh_freq_fact[i][j] = 32;
    for (j = 0; j < MAX_REFS; ++j)
      cpi->rd_thresh_freq_sub8x8[i][j] = 32;
  }

#define BFP(BT, SDF, SDAF, VF, SVF, SVAF, SVFHH, SVFHV, SVFHHV, \
            SDX3F, SDX8F, SDX4DF)\
    cpi->fn_ptr[BT].sdf            = SDF; \
    cpi->fn_ptr[BT].sdaf           = SDAF; \
    cpi->fn_ptr[BT].vf             = VF; \
    cpi->fn_ptr[BT].svf            = SVF; \
    cpi->fn_ptr[BT].svaf           = SVAF; \
    cpi->fn_ptr[BT].svf_halfpix_h  = SVFHH; \
    cpi->fn_ptr[BT].svf_halfpix_v  = SVFHV; \
    cpi->fn_ptr[BT].svf_halfpix_hv = SVFHHV; \
    cpi->fn_ptr[BT].sdx3f          = SDX3F; \
    cpi->fn_ptr[BT].sdx8f          = SDX8F; \
    cpi->fn_ptr[BT].sdx4df         = SDX4DF;

  BFP(BLOCK_32X16, vp9_sad32x16, vp9_sad32x16_avg,
      vp9_variance32x16, vp9_sub_pixel_variance32x16,
      vp9_sub_pixel_avg_variance32x16, NULL, NULL,
      NULL, NULL, NULL,
      vp9_sad32x16x4d)

  BFP(BLOCK_16X32, vp9_sad16x32, vp9_sad16x32_avg,
      vp9_variance16x32, vp9_sub_pixel_variance16x32,
      vp9_sub_pixel_avg_variance16x32, NULL, NULL,
      NULL, NULL, NULL,
      vp9_sad16x32x4d)

  BFP(BLOCK_64X32, vp9_sad64x32, vp9_sad64x32_avg,
      vp9_variance64x32, vp9_sub_pixel_variance64x32,
      vp9_sub_pixel_avg_variance64x32, NULL, NULL,
      NULL, NULL, NULL,
      vp9_sad64x32x4d)

  BFP(BLOCK_32X64, vp9_sad32x64, vp9_sad32x64_avg,
      vp9_variance32x64, vp9_sub_pixel_variance32x64,
      vp9_sub_pixel_avg_variance32x64, NULL, NULL,
      NULL, NULL, NULL,
      vp9_sad32x64x4d)

  BFP(BLOCK_32X32, vp9_sad32x32, vp9_sad32x32_avg,
      vp9_variance32x32, vp9_sub_pixel_variance32x32,
      vp9_sub_pixel_avg_variance32x32, vp9_variance_halfpixvar32x32_h,
      vp9_variance_halfpixvar32x32_v,
      vp9_variance_halfpixvar32x32_hv, vp9_sad32x32x3, vp9_sad32x32x8,
      vp9_sad32x32x4d)

  BFP(BLOCK_64X64, vp9_sad64x64, vp9_sad64x64_avg,
      vp9_variance64x64, vp9_sub_pixel_variance64x64,
      vp9_sub_pixel_avg_variance64x64, vp9_variance_halfpixvar64x64_h,
      vp9_variance_halfpixvar64x64_v,
      vp9_variance_halfpixvar64x64_hv, vp9_sad64x64x3, vp9_sad64x64x8,
      vp9_sad64x64x4d)

  BFP(BLOCK_16X16, vp9_sad16x16, vp9_sad16x16_avg,
      vp9_variance16x16, vp9_sub_pixel_variance16x16,
      vp9_sub_pixel_avg_variance16x16, vp9_variance_halfpixvar16x16_h,
      vp9_variance_halfpixvar16x16_v,
      vp9_variance_halfpixvar16x16_hv, vp9_sad16x16x3, vp9_sad16x16x8,
      vp9_sad16x16x4d)

  BFP(BLOCK_16X8, vp9_sad16x8, vp9_sad16x8_avg,
      vp9_variance16x8, vp9_sub_pixel_variance16x8,
      vp9_sub_pixel_avg_variance16x8, NULL, NULL, NULL,
      vp9_sad16x8x3, vp9_sad16x8x8, vp9_sad16x8x4d)

  BFP(BLOCK_8X16, vp9_sad8x16, vp9_sad8x16_avg,
      vp9_variance8x16, vp9_sub_pixel_variance8x16,
      vp9_sub_pixel_avg_variance8x16, NULL, NULL, NULL,
      vp9_sad8x16x3, vp9_sad8x16x8, vp9_sad8x16x4d)

  BFP(BLOCK_8X8, vp9_sad8x8, vp9_sad8x8_avg,
      vp9_variance8x8, vp9_sub_pixel_variance8x8,
      vp9_sub_pixel_avg_variance8x8, NULL, NULL, NULL,
      vp9_sad8x8x3, vp9_sad8x8x8, vp9_sad8x8x4d)

  BFP(BLOCK_8X4, vp9_sad8x4, vp9_sad8x4_avg,
      vp9_variance8x4, vp9_sub_pixel_variance8x4,
      vp9_sub_pixel_avg_variance8x4, NULL, NULL,
      NULL, NULL, vp9_sad8x4x8,
      vp9_sad8x4x4d)

  BFP(BLOCK_4X8, vp9_sad4x8, vp9_sad4x8_avg,
      vp9_variance4x8, vp9_sub_pixel_variance4x8,
      vp9_sub_pixel_avg_variance4x8, NULL, NULL,
      NULL, NULL, vp9_sad4x8x8,
      vp9_sad4x8x4d)

  BFP(BLOCK_4X4, vp9_sad4x4, vp9_sad4x4_avg,
      vp9_variance4x4, vp9_sub_pixel_variance4x4,
      vp9_sub_pixel_avg_variance4x4, NULL, NULL, NULL,
      vp9_sad4x4x3, vp9_sad4x4x8, vp9_sad4x4x4d)

  cpi->full_search_sad = vp9_full_search_sad;
  cpi->diamond_search_sad = vp9_diamond_search_sad;
  cpi->refining_search_sad = vp9_refining_search_sad;

  /* vp9_init_quantizer() is first called here. Add check in
   * vp9_frame_init_quantizer() so that vp9_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * vp9_init_quantizer() for every frame.
   */
  vp9_init_quantizer(cpi);

  vp9_loop_filter_init(cm);

  cm->error.setjmp = 0;

  vp9_zero(cpi->common.counts.uv_mode);

#ifdef MODE_TEST_HIT_STATS
  vp9_zero(cpi->mode_test_hits);
#endif

  return (VP9_PTR) cpi;
}

void vp9_remove_compressor(VP9_PTR *ptr) {
  VP9_COMP *cpi = (VP9_COMP *)(*ptr);
  int i;

  if (!cpi)
    return;

  if (cpi && (cpi->common.current_video_frame > 0)) {
    if (cpi->pass == 2) {
      vp9_end_second_pass(cpi);
    }

#if CONFIG_INTERNAL_STATS

    vp9_clear_system_state();

    // printf("\n8x8-4x4:%d-%d\n", cpi->t8x8_count, cpi->t4x4_count);
    if (cpi->pass != 1) {
      FILE *f = fopen("opsnr.stt", "a");
      double time_encoded = (cpi->last_end_time_stamp_seen
                             - cpi->first_time_stamp_ever) / 10000000.000;
      double total_encode_time = (cpi->time_receive_data +
                                  cpi->time_compress_data)   / 1000.000;
      double dr = (double)cpi->bytes * (double) 8 / (double)1000
                  / time_encoded;

      if (cpi->b_calculate_psnr) {
        const double total_psnr = vp9_mse2psnr(cpi->total_samples, 255.0,
                                               cpi->total_sq_error);
        const double totalp_psnr = vp9_mse2psnr(cpi->totalp_samples, 255.0,
                                                cpi->totalp_sq_error);
        const double total_ssim = 100 * pow(cpi->summed_quality /
                                                cpi->summed_weights, 8.0);
        const double totalp_ssim = 100 * pow(cpi->summedp_quality /
                                                cpi->summedp_weights, 8.0);

        fprintf(f, "Bitrate\tAVGPsnr\tGLBPsnr\tAVPsnrP\tGLPsnrP\t"
                "VPXSSIM\tVPSSIMP\t  Time(ms)\n");
        fprintf(f, "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%8.0f\n",
                dr, cpi->total / cpi->count, total_psnr,
                cpi->totalp / cpi->count, totalp_psnr, total_ssim, totalp_ssim,
                total_encode_time);
      }

      if (cpi->b_calculate_ssimg) {
        fprintf(f, "BitRate\tSSIM_Y\tSSIM_U\tSSIM_V\tSSIM_A\t  Time(ms)\n");
        fprintf(f, "%7.2f\t%6.4f\t%6.4f\t%6.4f\t%6.4f\t%8.0f\n", dr,
                cpi->total_ssimg_y / cpi->count,
                cpi->total_ssimg_u / cpi->count,
                cpi->total_ssimg_v / cpi->count,
                cpi->total_ssimg_all / cpi->count, total_encode_time);
      }

      fclose(f);
    }

#endif

#ifdef MODE_TEST_HIT_STATS
    if (cpi->pass != 1) {
      double norm_per_pixel_mode_tests = 0;
      double norm_counts[BLOCK_SIZES];
      int i;
      int sb64_per_frame;
      int norm_factors[BLOCK_SIZES] =
        {256, 128, 128, 64, 32, 32, 16, 8, 8, 4, 2, 2, 1};
      FILE *f = fopen("mode_hit_stats.stt", "a");

      // On average, how many mode tests do we do
      for (i = 0; i < BLOCK_SIZES; ++i) {
        norm_counts[i] = (double)cpi->mode_test_hits[i] /
                         (double)norm_factors[i];
        norm_per_pixel_mode_tests += norm_counts[i];
      }
      // Convert to a number per 64x64 and per frame
      sb64_per_frame = ((cpi->common.height + 63) / 64) *
                       ((cpi->common.width + 63) / 64);
      norm_per_pixel_mode_tests =
        norm_per_pixel_mode_tests /
        (double)(cpi->common.current_video_frame * sb64_per_frame);

      fprintf(f, "%6.4f\n", norm_per_pixel_mode_tests);
      fclose(f);
    }
#endif

#if 0
    {
      printf("\n_pick_loop_filter_level:%d\n", cpi->time_pick_lpf / 1000);
      printf("\n_frames recive_data encod_mb_row compress_frame  Total\n");
      printf("%6d %10ld %10ld %10ld %10ld\n", cpi->common.current_video_frame,
             cpi->time_receive_data / 1000, cpi->time_encode_sb_row / 1000,
             cpi->time_compress_data / 1000,
             (cpi->time_receive_data + cpi->time_compress_data) / 1000);
    }
#endif
  }

  free_pick_mode_context(&cpi->mb);
  dealloc_compressor_data(cpi);
  vpx_free(cpi->mb.ss);
  vpx_free(cpi->tok);

  for (i = 0; i < sizeof(cpi->mbgraph_stats) /
                  sizeof(cpi->mbgraph_stats[0]); ++i) {
    vpx_free(cpi->mbgraph_stats[i].mb_stats);
  }

  vp9_remove_common(&cpi->common);
  vpx_free(cpi);
  *ptr = 0;

#ifdef OUTPUT_YUV_SRC
  fclose(yuv_file);
#endif
#ifdef OUTPUT_YUV_REC
  fclose(yuv_rec_file);
#endif

#if 0

  if (keyfile)
    fclose(keyfile);

  if (framepsnr)
    fclose(framepsnr);

  if (kf_list)
    fclose(kf_list);

#endif
}


static uint64_t calc_plane_error(const uint8_t *orig, int orig_stride,
                                 const uint8_t *recon, int recon_stride,
                                 unsigned int cols, unsigned int rows) {
  unsigned int row, col;
  uint64_t total_sse = 0;
  int diff;

  for (row = 0; row + 16 <= rows; row += 16) {
    for (col = 0; col + 16 <= cols; col += 16) {
      unsigned int sse;

      vp9_mse16x16(orig + col, orig_stride, recon + col, recon_stride, &sse);
      total_sse += sse;
    }

    /* Handle odd-sized width */
    if (col < cols) {
      unsigned int border_row, border_col;
      const uint8_t *border_orig = orig;
      const uint8_t *border_recon = recon;

      for (border_row = 0; border_row < 16; border_row++) {
        for (border_col = col; border_col < cols; border_col++) {
          diff = border_orig[border_col] - border_recon[border_col];
          total_sse += diff * diff;
        }

        border_orig += orig_stride;
        border_recon += recon_stride;
      }
    }

    orig += orig_stride * 16;
    recon += recon_stride * 16;
  }

  /* Handle odd-sized height */
  for (; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      diff = orig[col] - recon[col];
      total_sse += diff * diff;
    }

    orig += orig_stride;
    recon += recon_stride;
  }

  return total_sse;
}

typedef struct {
  double psnr[4];       // total/y/u/v
  uint64_t sse[4];      // total/y/u/v
  uint32_t samples[4];  // total/y/u/v
} PSNR_STATS;

static void calc_psnr(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b,
                      PSNR_STATS *psnr) {
  const int widths[3]        = {a->y_width,  a->uv_width,  a->uv_width };
  const int heights[3]       = {a->y_height, a->uv_height, a->uv_height};
  const uint8_t *a_planes[3] = {a->y_buffer, a->u_buffer,  a->v_buffer };
  const int a_strides[3]     = {a->y_stride, a->uv_stride, a->uv_stride};
  const uint8_t *b_planes[3] = {b->y_buffer, b->u_buffer,  b->v_buffer };
  const int b_strides[3]     = {b->y_stride, b->uv_stride, b->uv_stride};
  int i;
  uint64_t total_sse = 0;
  uint32_t total_samples = 0;

  for (i = 0; i < 3; ++i) {
    const int w = widths[i];
    const int h = heights[i];
    const uint32_t samples = w * h;
    const double sse = calc_plane_error(a_planes[i], a_strides[i],
                                        b_planes[i], b_strides[i],
                                        w, h);
    psnr->sse[1 + i] = sse;
    psnr->samples[1 + i] = samples;
    psnr->psnr[1 + i] = vp9_mse2psnr(samples, 255.0, sse);

    total_sse += sse;
    total_samples += samples;
  }

  psnr->sse[0] = total_sse;
  psnr->samples[0] = total_samples;
  psnr->psnr[0] = vp9_mse2psnr(total_samples, 255.0, total_sse);
}

static void generate_psnr_packet(VP9_COMP *cpi) {
  struct vpx_codec_cx_pkt pkt;
  int i;
  PSNR_STATS psnr;
  calc_psnr(cpi->Source, cpi->common.frame_to_show, &psnr);
  for (i = 0; i < 4; ++i) {
    pkt.data.psnr.samples[i] = psnr.samples[i];
    pkt.data.psnr.sse[i] = psnr.sse[i];
    pkt.data.psnr.psnr[i] = psnr.psnr[i];
  }
  pkt.kind = VPX_CODEC_PSNR_PKT;
  vpx_codec_pkt_list_add(cpi->output_pkt_list, &pkt);
}

int vp9_use_as_reference(VP9_PTR ptr, int ref_frame_flags) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);

  if (ref_frame_flags > 7)
    return -1;

  cpi->ref_frame_flags = ref_frame_flags;
  return 0;
}

int vp9_update_reference(VP9_PTR ptr, int ref_frame_flags) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);

  if (ref_frame_flags > 7)
    return -1;

  cpi->ext_refresh_golden_frame = 0;
  cpi->ext_refresh_alt_ref_frame = 0;
  cpi->ext_refresh_last_frame   = 0;

  if (ref_frame_flags & VP9_LAST_FLAG)
    cpi->ext_refresh_last_frame = 1;

  if (ref_frame_flags & VP9_GOLD_FLAG)
    cpi->ext_refresh_golden_frame = 1;

  if (ref_frame_flags & VP9_ALT_FLAG)
    cpi->ext_refresh_alt_ref_frame = 1;

  cpi->ext_refresh_frame_flags_pending = 1;
  return 0;
}

static YV12_BUFFER_CONFIG *get_vp9_ref_frame_buffer(VP9_COMP *cpi,
                                VP9_REFFRAME ref_frame_flag) {
  MV_REFERENCE_FRAME ref_frame = NONE;
  if (ref_frame_flag == VP9_LAST_FLAG)
    ref_frame = LAST_FRAME;
  else if (ref_frame_flag == VP9_GOLD_FLAG)
    ref_frame = GOLDEN_FRAME;
  else if (ref_frame_flag == VP9_ALT_FLAG)
    ref_frame = ALTREF_FRAME;

  return ref_frame == NONE ? NULL : get_ref_frame_buffer(cpi, ref_frame);
}

int vp9_copy_reference_enc(VP9_PTR ptr, VP9_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd) {
  VP9_COMP *const cpi = (VP9_COMP *)ptr;
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vp8_yv12_copy_frame(cfg, sd);
    return 0;
  } else {
    return -1;
  }
}

int vp9_get_reference_enc(VP9_PTR ptr, int index, YV12_BUFFER_CONFIG **fb) {
  VP9_COMP *cpi = (VP9_COMP *)ptr;
  VP9_COMMON *cm = &cpi->common;

  if (index < 0 || index >= REF_FRAMES)
    return -1;

  *fb = &cm->frame_bufs[cm->ref_frame_map[index]].buf;
  return 0;
}

int vp9_set_reference_enc(VP9_PTR ptr, VP9_REFFRAME ref_frame_flag,
                          YV12_BUFFER_CONFIG *sd) {
  VP9_COMP *cpi = (VP9_COMP *)ptr;
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vp8_yv12_copy_frame(sd, cfg);
    return 0;
  } else {
    return -1;
  }
}

int vp9_update_entropy(VP9_PTR comp, int update) {
  ((VP9_COMP *)comp)->ext_refresh_frame_context = update;
  ((VP9_COMP *)comp)->ext_refresh_frame_context_pending = 1;
  return 0;
}


#ifdef OUTPUT_YUV_SRC
void vp9_write_yuv_frame(YV12_BUFFER_CONFIG *s) {
  uint8_t *src = s->y_buffer;
  int h = s->y_height;

  do {
    fwrite(src, s->y_width, 1,  yuv_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1,  yuv_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);
}
#endif

#ifdef OUTPUT_YUV_REC
void vp9_write_yuv_rec_frame(VP9_COMMON *cm) {
  YV12_BUFFER_CONFIG *s = cm->frame_to_show;
  uint8_t *src = s->y_buffer;
  int h = cm->height;

  do {
    fwrite(src, s->y_width, 1,  yuv_rec_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1,  yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

#if CONFIG_ALPHA
  if (s->alpha_buffer) {
    src = s->alpha_buffer;
    h = s->alpha_height;
    do {
      fwrite(src, s->alpha_width, 1,  yuv_rec_file);
      src += s->alpha_stride;
    } while (--h);
  }
#endif

  fflush(yuv_rec_file);
}
#endif

static void scale_and_extend_frame_nonnormative(YV12_BUFFER_CONFIG *src_fb,
                                                YV12_BUFFER_CONFIG *dst_fb) {
  const int in_w = src_fb->y_crop_width;
  const int in_h = src_fb->y_crop_height;
  const int out_w = dst_fb->y_crop_width;
  const int out_h = dst_fb->y_crop_height;
  const int in_w_uv = src_fb->uv_crop_width;
  const int in_h_uv = src_fb->uv_crop_height;
  const int out_w_uv = dst_fb->uv_crop_width;
  const int out_h_uv = dst_fb->uv_crop_height;
  int i;

  uint8_t *srcs[4] = {src_fb->y_buffer, src_fb->u_buffer, src_fb->v_buffer,
    src_fb->alpha_buffer};
  int src_strides[4] = {src_fb->y_stride, src_fb->uv_stride, src_fb->uv_stride,
    src_fb->alpha_stride};

  uint8_t *dsts[4] = {dst_fb->y_buffer, dst_fb->u_buffer, dst_fb->v_buffer,
    dst_fb->alpha_buffer};
  int dst_strides[4] = {dst_fb->y_stride, dst_fb->uv_stride, dst_fb->uv_stride,
    dst_fb->alpha_stride};

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    if (i == 0 || i == 3) {
      // Y and alpha planes
      vp9_resize_plane(srcs[i], in_h, in_w, src_strides[i],
                       dsts[i], out_h, out_w, dst_strides[i]);
    } else {
      // Chroma planes
      vp9_resize_plane(srcs[i], in_h_uv, in_w_uv, src_strides[i],
                       dsts[i], out_h_uv, out_w_uv, dst_strides[i]);
    }
  }
  vp8_yv12_extend_frame_borders(dst_fb);
}

static void scale_and_extend_frame(YV12_BUFFER_CONFIG *src_fb,
                                   YV12_BUFFER_CONFIG *dst_fb) {
  const int in_w = src_fb->y_crop_width;
  const int in_h = src_fb->y_crop_height;
  const int out_w = dst_fb->y_crop_width;
  const int out_h = dst_fb->y_crop_height;
  int x, y, i;

  uint8_t *srcs[4] = {src_fb->y_buffer, src_fb->u_buffer, src_fb->v_buffer,
                      src_fb->alpha_buffer};
  int src_strides[4] = {src_fb->y_stride, src_fb->uv_stride, src_fb->uv_stride,
                        src_fb->alpha_stride};

  uint8_t *dsts[4] = {dst_fb->y_buffer, dst_fb->u_buffer, dst_fb->v_buffer,
                      dst_fb->alpha_buffer};
  int dst_strides[4] = {dst_fb->y_stride, dst_fb->uv_stride, dst_fb->uv_stride,
                        dst_fb->alpha_stride};

  for (y = 0; y < out_h; y += 16) {
    for (x = 0; x < out_w; x += 16) {
      for (i = 0; i < MAX_MB_PLANE; ++i) {
        const int factor = (i == 0 || i == 3 ? 1 : 2);
        const int x_q4 = x * (16 / factor) * in_w / out_w;
        const int y_q4 = y * (16 / factor) * in_h / out_h;
        const int src_stride = src_strides[i];
        const int dst_stride = dst_strides[i];
        uint8_t *src = srcs[i] + y / factor * in_h / out_h * src_stride +
                                 x / factor * in_w / out_w;
        uint8_t *dst = dsts[i] + y / factor * dst_stride + x / factor;

        vp9_convolve8(src, src_stride, dst, dst_stride,
                      vp9_sub_pel_filters_8[x_q4 & 0xf], 16 * in_w / out_w,
                      vp9_sub_pel_filters_8[y_q4 & 0xf], 16 * in_h / out_h,
                      16 / factor, 16 / factor);
      }
    }
  }

  vp8_yv12_extend_frame_borders(dst_fb);
}

static int find_fp_qindex() {
  int i;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (vp9_convert_qindex_to_q(i) >= 30.0) {
      break;
    }
  }

  if (i == QINDEX_RANGE)
    i--;

  return i;
}

#define WRITE_RECON_BUFFER 0
#if WRITE_RECON_BUFFER
void write_cx_frame_to_file(YV12_BUFFER_CONFIG *frame, int this_frame) {
  FILE *yframe;
  int i;
  char filename[255];

  snprintf(filename, sizeof(filename), "cx\\y%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->y_height; i++)
    fwrite(frame->y_buffer + i * frame->y_stride,
           frame->y_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename), "cx\\u%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->u_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename), "cx\\v%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->v_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
}
#endif

static double compute_edge_pixel_proportion(YV12_BUFFER_CONFIG *frame) {
#define EDGE_THRESH 128
  int i, j;
  int num_edge_pels = 0;
  int num_pels = (frame->y_height - 2) * (frame->y_width - 2);
  uint8_t *prev = frame->y_buffer + 1;
  uint8_t *curr = frame->y_buffer + 1 + frame->y_stride;
  uint8_t *next = frame->y_buffer + 1 + 2 * frame->y_stride;
  for (i = 1; i < frame->y_height - 1; i++) {
    for (j = 1; j < frame->y_width - 1; j++) {
      /* Sobel hor and ver gradients */
      int v = 2 * (curr[1] - curr[-1]) + (prev[1] - prev[-1]) +
              (next[1] - next[-1]);
      int h = 2 * (prev[0] - next[0]) + (prev[1] - next[1]) +
              (prev[-1] - next[-1]);
      h = (h < 0 ? -h : h);
      v = (v < 0 ? -v : v);
      if (h > EDGE_THRESH || v > EDGE_THRESH)
        num_edge_pels++;
      curr++;
      prev++;
      next++;
    }
    curr += frame->y_stride - frame->y_width + 2;
    prev += frame->y_stride - frame->y_width + 2;
    next += frame->y_stride - frame->y_width + 2;
  }
  return (double)num_edge_pels / num_pels;
}

// Function to test for conditions that indicate we should loop
// back and recode a frame.
static int recode_loop_test(const VP9_COMP *cpi,
                            int high_limit, int low_limit,
                            int q, int maxq, int minq) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  int force_recode = 0;

  // Special case trap if maximum allowed frame size exceeded.
  if (rc->projected_frame_size > rc->max_frame_bandwidth) {
    force_recode = 1;

  // Is frame recode allowed.
  // Yes if either recode mode 1 is selected or mode 2 is selected
  // and the frame is a key frame, golden frame or alt_ref_frame
  } else if ((cpi->sf.recode_loop == ALLOW_RECODE) ||
             ((cpi->sf.recode_loop == ALLOW_RECODE_KFARFGF) &&
              (cm->frame_type == KEY_FRAME ||
               cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame))) {
    // General over and under shoot tests
    if ((rc->projected_frame_size > high_limit && q < maxq) ||
        (rc->projected_frame_size < low_limit && q > minq)) {
      force_recode = 1;
    } else if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY) {
      // Deal with frame undershoot and whether or not we are
      // below the automatically set cq level.
      if (q > cpi->cq_target_quality &&
          rc->projected_frame_size < ((rc->this_frame_target * 7) >> 3)) {
        force_recode = 1;
      }
    }
  }
  return force_recode;
}

static void update_reference_frames(VP9_COMP * const cpi) {
  VP9_COMMON * const cm = &cpi->common;

  // At this point the new frame has been encoded.
  // If any buffer copy / swapping is signaled it should be done here.
  if (cm->frame_type == KEY_FRAME) {
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->alt_fb_idx], cm->new_fb_idx);
  }
#if CONFIG_MULTIPLE_ARF
  else if (!cpi->multi_arf_enabled && cpi->refresh_golden_frame &&
      !cpi->refresh_alt_ref_frame) {
#else
  else if (cpi->refresh_golden_frame && !cpi->refresh_alt_ref_frame &&
           !cpi->use_svc) {
#endif
    /* Preserve the previously existing golden frame and update the frame in
     * the alt ref slot instead. This is highly specific to the current use of
     * alt-ref as a forward reference, and this needs to be generalized as
     * other uses are implemented (like RTC/temporal scaling)
     *
     * The update to the buffer in the alt ref slot was signaled in
     * vp9_pack_bitstream(), now swap the buffer pointers so that it's treated
     * as the golden frame next time.
     */
    int tmp;

    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->alt_fb_idx], cm->new_fb_idx);

    tmp = cpi->alt_fb_idx;
    cpi->alt_fb_idx = cpi->gld_fb_idx;
    cpi->gld_fb_idx = tmp;
  }  else { /* For non key/golden frames */
    if (cpi->refresh_alt_ref_frame) {
      int arf_idx = cpi->alt_fb_idx;
#if CONFIG_MULTIPLE_ARF
      if (cpi->multi_arf_enabled) {
        arf_idx = cpi->arf_buffer_idx[cpi->sequence_number + 1];
      }
#endif
      ref_cnt_fb(cm->frame_bufs,
                 &cm->ref_frame_map[arf_idx], cm->new_fb_idx);
    }

    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(cm->frame_bufs,
                 &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    }
  }

  if (cpi->refresh_last_frame) {
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->lst_fb_idx], cm->new_fb_idx);
  }
}

static void loopfilter_frame(VP9_COMP *cpi, VP9_COMMON *cm) {
  MACROBLOCKD *xd = &cpi->mb.e_mbd;
  struct loopfilter *lf = &cm->lf;
  if (xd->lossless) {
      lf->filter_level = 0;
  } else {
    struct vpx_usec_timer timer;

    vp9_clear_system_state();

    vpx_usec_timer_start(&timer);

    vp9_pick_filter_level(cpi->Source, cpi, cpi->sf.use_fast_lpf_pick);

    vpx_usec_timer_mark(&timer);
    cpi->time_pick_lpf += vpx_usec_timer_elapsed(&timer);
  }

  if (lf->filter_level > 0) {
    vp9_set_alt_lf_level(cpi, lf->filter_level);
    vp9_loop_filter_frame(cm, xd, lf->filter_level, 0, 0);
  }

  vp9_extend_frame_inner_borders(cm->frame_to_show,
                                 cm->subsampling_x, cm->subsampling_y);
}

static void scale_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int idx = cm->ref_frame_map[get_ref_frame_idx(cpi, ref_frame)];
    YV12_BUFFER_CONFIG *const ref = &cm->frame_bufs[idx].buf;

    if (ref->y_crop_width != cm->width ||
        ref->y_crop_height != cm->height) {
      const int new_fb = get_free_fb(cm);
      vp9_realloc_frame_buffer(&cm->frame_bufs[new_fb].buf,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL);
      scale_and_extend_frame(ref, &cm->frame_bufs[new_fb].buf);
      cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
    } else {
      cpi->scaled_ref_idx[ref_frame - 1] = idx;
      cm->frame_bufs[idx].ref_count++;
    }
  }
}

static void release_scaled_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int i;

  for (i = 0; i < 3; i++)
    cm->frame_bufs[cpi->scaled_ref_idx[i]].ref_count--;
}

static void full_to_model_count(unsigned int *model_count,
                                unsigned int *full_count) {
  int n;
  model_count[ZERO_TOKEN] = full_count[ZERO_TOKEN];
  model_count[ONE_TOKEN] = full_count[ONE_TOKEN];
  model_count[TWO_TOKEN] = full_count[TWO_TOKEN];
  for (n = THREE_TOKEN; n < EOB_TOKEN; ++n)
    model_count[TWO_TOKEN] += full_count[n];
  model_count[EOB_MODEL_TOKEN] = full_count[EOB_TOKEN];
}

static void full_to_model_counts(vp9_coeff_count_model *model_count,
                                 vp9_coeff_count *full_count) {
  int i, j, k, l;

  for (i = 0; i < PLANE_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k)
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l)
          full_to_model_count(model_count[i][j][k][l], full_count[i][j][k][l]);
}

#if 0 && CONFIG_INTERNAL_STATS
static void output_frame_level_debug_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  FILE *const f = fopen("tmp.stt", cm->current_video_frame ? "a" : "w");
  int recon_err;

  vp9_clear_system_state();  // __asm emms;

  recon_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));

  if (cpi->twopass.total_left_stats.coded_error != 0.0)
    fprintf(f, "%10u %10d %10d %10d %10d %10d "
        "%10"PRId64" %10"PRId64" %10d "
        "%7.2lf %7.2lf %7.2lf %7.2lf %7.2lf"
        "%6d %6d %5d %5d %5d "
        "%10"PRId64" %10.3lf"
        "%10lf %8u %10d %10d %10d\n",
        cpi->common.current_video_frame, cpi->rc.this_frame_target,
        cpi->rc.projected_frame_size,
        cpi->rc.projected_frame_size / cpi->common.MBs,
        (cpi->rc.projected_frame_size - cpi->rc.this_frame_target),
        cpi->rc.total_target_vs_actual,
        (cpi->oxcf.starting_buffer_level - cpi->rc.bits_off_target),
        cpi->rc.total_actual_bits, cm->base_qindex,
        vp9_convert_qindex_to_q(cm->base_qindex),
        (double)vp9_dc_quant(cm->base_qindex, 0) / 4.0,
        cpi->rc.avg_q,
        vp9_convert_qindex_to_q(cpi->rc.ni_av_qi),
        vp9_convert_qindex_to_q(cpi->cq_target_quality),
        cpi->refresh_last_frame, cpi->refresh_golden_frame,
        cpi->refresh_alt_ref_frame, cm->frame_type, cpi->rc.gfu_boost,
        cpi->twopass.bits_left,
        cpi->twopass.total_left_stats.coded_error,
        cpi->twopass.bits_left /
            (1 + cpi->twopass.total_left_stats.coded_error),
        cpi->tot_recode_hits, recon_err, cpi->rc.kf_boost,
        cpi->twopass.kf_zeromotion_pct);

  fclose(f);

  if (0) {
    FILE *const fmodes = fopen("Modes.stt", "a");
    int i;

    fprintf(fmodes, "%6d:%1d:%1d:%1d ", cpi->common.current_video_frame,
            cm->frame_type, cpi->refresh_golden_frame,
            cpi->refresh_alt_ref_frame);

    for (i = 0; i < MAX_MODES; ++i)
      fprintf(fmodes, "%5d ", cpi->mode_chosen_counts[i]);
    for (i = 0; i < MAX_REFS; ++i)
      fprintf(fmodes, "%5d ", cpi->sub8x8_mode_chosen_counts[i]);

    fprintf(fmodes, "\n");

    fclose(fmodes);
  }
}
#endif

static void encode_without_recode_loop(VP9_COMP *cpi,
                                       size_t *size,
                                       uint8_t *dest,
                                       int q) {
  VP9_COMMON *const cm = &cpi->common;
  vp9_clear_system_state();  // __asm emms;
  vp9_set_quantizer(cpi, q);

  // Set up entropy context depending on frame type. The decoder mandates
  // the use of the default context, index 0, for keyframes and inter
  // frames where the error_resilient_mode or intra_only flag is set. For
  // other inter-frames the encoder currently uses only two contexts;
  // context 1 for ALTREF frames and context 0 for the others.
  if (cm->frame_type == KEY_FRAME) {
    vp9_setup_key_frame(cpi);
  } else {
    if (!cm->intra_only && !cm->error_resilient_mode) {
      cpi->common.frame_context_idx = cpi->refresh_alt_ref_frame;
    }
    vp9_setup_inter_frame(cpi);
  }
  // Variance adaptive and in frame q adjustment experiments are mutually
  // exclusive.
  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_vaq_frame_setup(cpi);
  } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    setup_in_frame_q_adj(cpi);
  }
  // transform / motion compensation build reconstruction frame
  vp9_encode_frame(cpi);

  // Update the skip mb flag probabilities based on the distribution
  // seen in the last encoder iteration.
  // update_base_skip_probs(cpi);
  vp9_clear_system_state();  // __asm emms;
}

static void encode_with_recode_loop(VP9_COMP *cpi,
                                    size_t *size,
                                    uint8_t *dest,
                                    int q,
                                    int bottom_index,
                                    int top_index) {
  VP9_COMMON *const cm = &cpi->common;
  int loop_count = 0;
  int loop = 0;
  int overshoot_seen = 0;
  int undershoot_seen = 0;
  int q_low = bottom_index, q_high = top_index;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;

  // Decide frame size bounds
  vp9_rc_compute_frame_size_bounds(cpi, cpi->rc.this_frame_target,
                                   &frame_under_shoot_limit,
                                   &frame_over_shoot_limit);

  do {
    vp9_clear_system_state();  // __asm emms;

    vp9_set_quantizer(cpi, q);

    if (loop_count == 0) {
      // Set up entropy context depending on frame type. The decoder mandates
      // the use of the default context, index 0, for keyframes and inter
      // frames where the error_resilient_mode or intra_only flag is set. For
      // other inter-frames the encoder currently uses only two contexts;
      // context 1 for ALTREF frames and context 0 for the others.
      if (cm->frame_type == KEY_FRAME) {
        vp9_setup_key_frame(cpi);
      } else {
        if (!cm->intra_only && !cm->error_resilient_mode) {
          cpi->common.frame_context_idx = cpi->refresh_alt_ref_frame;
        }
        vp9_setup_inter_frame(cpi);
      }
    }

    // Variance adaptive and in frame q adjustment experiments are mutually
    // exclusive.
    if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
      vp9_vaq_frame_setup(cpi);
    } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      setup_in_frame_q_adj(cpi);
    }

    // transform / motion compensation build reconstruction frame
    vp9_encode_frame(cpi);

    // Update the skip mb flag probabilities based on the distribution
    // seen in the last encoder iteration.
    // update_base_skip_probs(cpi);

    vp9_clear_system_state();  // __asm emms;

    // Dummy pack of the bitstream using up to date stats to get an
    // accurate estimate of output frame size to determine if we need
    // to recode.
    if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
      vp9_save_coding_context(cpi);
      cpi->dummy_packing = 1;
      if (!cpi->sf.use_pick_mode)
        vp9_pack_bitstream(cpi, dest, size);

      cpi->rc.projected_frame_size = (*size) << 3;
      vp9_restore_coding_context(cpi);

      if (frame_over_shoot_limit == 0)
        frame_over_shoot_limit = 1;
    }

    if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
      loop = 0;
    } else {
      if ((cm->frame_type == KEY_FRAME) &&
           cpi->rc.this_key_frame_forced &&
           (cpi->rc.projected_frame_size < cpi->rc.max_frame_bandwidth)) {
        int last_q = q;
        int kf_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));

        int high_err_target = cpi->ambient_err;
        int low_err_target = cpi->ambient_err >> 1;

        // Prevent possible divide by zero error below for perfect KF
        kf_err += !kf_err;

        // The key frame is not good enough or we can afford
        // to make it better without undue risk of popping.
        if ((kf_err > high_err_target &&
             cpi->rc.projected_frame_size <= frame_over_shoot_limit) ||
            (kf_err > low_err_target &&
             cpi->rc.projected_frame_size <= frame_under_shoot_limit)) {
          // Lower q_high
          q_high = q > q_low ? q - 1 : q_low;

          // Adjust Q
          q = (q * high_err_target) / kf_err;
          q = MIN(q, (q_high + q_low) >> 1);
        } else if (kf_err < low_err_target &&
                   cpi->rc.projected_frame_size >= frame_under_shoot_limit) {
          // The key frame is much better than the previous frame
          // Raise q_low
          q_low = q < q_high ? q + 1 : q_high;

          // Adjust Q
          q = (q * low_err_target) / kf_err;
          q = MIN(q, (q_high + q_low + 1) >> 1);
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else if (recode_loop_test(
          cpi, frame_over_shoot_limit, frame_under_shoot_limit,
          q, MAX(q_high, top_index), bottom_index)) {
        // Is the projected frame size out of range and are we allowed
        // to attempt to recode.
        int last_q = q;
        int retries = 0;

        // Frame size out of permitted range:
        // Update correction factor & compute new Q to try...

        // Frame is too large
        if (cpi->rc.projected_frame_size > cpi->rc.this_frame_target) {
          // Special case if the projected size is > the max allowed.
          if (cpi->rc.projected_frame_size >= cpi->rc.max_frame_bandwidth)
            q_high = cpi->rc.worst_quality;

          // Raise Qlow as to at least the current value
          q_low = q < q_high ? q + 1 : q_high;

          if (undershoot_seen || loop_count > 1) {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi, 1);

            q = (q_high + q_low + 1) / 2;
          } else {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi, 0);

            q = vp9_rc_regulate_q(cpi, cpi->rc.this_frame_target,
                                   bottom_index, MAX(q_high, top_index));

            while (q < q_low && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi, 0);
              q = vp9_rc_regulate_q(cpi, cpi->rc.this_frame_target,
                                     bottom_index, MAX(q_high, top_index));
              retries++;
            }
          }

          overshoot_seen = 1;
        } else {
          // Frame is too small
          q_high = q > q_low ? q - 1 : q_low;

          if (overshoot_seen || loop_count > 1) {
            vp9_rc_update_rate_correction_factors(cpi, 1);
            q = (q_high + q_low) / 2;
          } else {
            vp9_rc_update_rate_correction_factors(cpi, 0);
            q = vp9_rc_regulate_q(cpi, cpi->rc.this_frame_target,
                                   bottom_index, top_index);
            // Special case reset for qlow for constrained quality.
            // This should only trigger where there is very substantial
            // undershoot on a frame and the auto cq level is above
            // the user passsed in value.
            if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY &&
                q < q_low) {
              q_low = q;
            }

            while (q > q_high && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi, 0);
              q = vp9_rc_regulate_q(cpi, cpi->rc.this_frame_target,
                                     bottom_index, top_index);
              retries++;
            }
          }

          undershoot_seen = 1;
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else {
        loop = 0;
      }
    }

    // Special case for overlay frame.
    if (cpi->rc.is_src_frame_alt_ref &&
        (cpi->rc.projected_frame_size < cpi->rc.max_frame_bandwidth))
      loop = 0;

    if (loop) {
      loop_count++;

#if CONFIG_INTERNAL_STATS
      cpi->tot_recode_hits++;
#endif
    }
  } while (loop);
}

static void get_ref_frame_flags(VP9_COMP *cpi) {
  if (cpi->refresh_last_frame & cpi->refresh_golden_frame)
    cpi->gold_is_last = 1;
  else if (cpi->refresh_last_frame ^ cpi->refresh_golden_frame)
    cpi->gold_is_last = 0;

  if (cpi->refresh_last_frame & cpi->refresh_alt_ref_frame)
    cpi->alt_is_last = 1;
  else if (cpi->refresh_last_frame ^ cpi->refresh_alt_ref_frame)
    cpi->alt_is_last = 0;

  if (cpi->refresh_alt_ref_frame & cpi->refresh_golden_frame)
    cpi->gold_is_alt = 1;
  else if (cpi->refresh_alt_ref_frame ^ cpi->refresh_golden_frame)
    cpi->gold_is_alt = 0;

  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  if (cpi->gold_is_last)
    cpi->ref_frame_flags &= ~VP9_GOLD_FLAG;

  if (cpi->alt_is_last)
    cpi->ref_frame_flags &= ~VP9_ALT_FLAG;

  if (cpi->gold_is_alt)
    cpi->ref_frame_flags &= ~VP9_ALT_FLAG;
}

static void set_ext_overrides(VP9_COMP *cpi) {
  // Overrides the defaults with the externally supplied values with
  // vp9_update_reference() and vp9_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to encode_frame_to_data_rate() function
  if (cpi->ext_refresh_frame_context_pending) {
    cpi->common.refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  if (cpi->ext_refresh_frame_flags_pending) {
    cpi->refresh_last_frame = cpi->ext_refresh_last_frame;
    cpi->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    cpi->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
    cpi->ext_refresh_frame_flags_pending = 0;
  }
}

static void encode_frame_to_data_rate(VP9_COMP *cpi,
                                      size_t *size,
                                      uint8_t *dest,
                                      unsigned int *frame_flags) {
  VP9_COMMON *const cm = &cpi->common;
  TX_SIZE t;
  int q;
  int top_index;
  int bottom_index;

  SPEED_FEATURES *const sf = &cpi->sf;
  unsigned int max_mv_def = MIN(cm->width, cm->height);
  struct segmentation *const seg = &cm->seg;

  set_ext_overrides(cpi);

  /* Scale the source buffer, if required. */
  if (cm->mi_cols * MI_SIZE != cpi->un_scaled_source->y_width ||
      cm->mi_rows * MI_SIZE != cpi->un_scaled_source->y_height) {
    scale_and_extend_frame_nonnormative(cpi->un_scaled_source,
                                        &cpi->scaled_source);
    cpi->Source = &cpi->scaled_source;
  } else {
    cpi->Source = cpi->un_scaled_source;
  }
  scale_references(cpi);

  vp9_clear_system_state();

  // Enable or disable mode based tweaking of the zbin.
  // For 2 pass only used where GF/ARF prediction quality
  // is above a threshold.
  cpi->zbin_mode_boost = 0;
  cpi->zbin_mode_boost_enabled = 0;

  // Current default encoder behavior for the altref sign bias.
  cm->ref_frame_sign_bias[ALTREF_FRAME] = cpi->rc.source_alt_ref_active;

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  // Initialize cpi->mv_step_param to default based on max resolution.
  cpi->mv_step_param = vp9_init_search_range(cpi, max_mv_def);
  // Initialize cpi->max_mv_magnitude and cpi->mv_step_param if appropriate.
  if (sf->auto_mv_step_size) {
    if (frame_is_intra_only(cm)) {
      // Initialize max_mv_magnitude for use in the first INTER frame
      // after a key/intra-only frame.
      cpi->max_mv_magnitude = max_mv_def;
    } else {
      if (cm->show_frame)
        // Allow mv_steps to correspond to twice the max mv magnitude found
        // in the previous frame, capped by the default max_mv_magnitude based
        // on resolution.
        cpi->mv_step_param = vp9_init_search_range(cpi, MIN(max_mv_def, 2 *
                                 cpi->max_mv_magnitude));
      cpi->max_mv_magnitude = 0;
    }
  }

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm)) {
    vp9_setup_key_frame(cpi);
    // Reset the loop filter deltas and segmentation map.
    vp9_reset_segment_features(&cm->seg);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->rc.source_alt_ref_active = 0;

    cm->error_resilient_mode = (cpi->oxcf.error_resilient_mode != 0);
    cm->frame_parallel_decoding_mode =
      (cpi->oxcf.frame_parallel_decoding_mode != 0);
    if (cm->error_resilient_mode) {
      cm->frame_parallel_decoding_mode = 1;
      cm->reset_frame_context = 0;
      cm->refresh_frame_context = 0;
    } else if (cm->intra_only) {
      // Only reset the current context.
      cm->reset_frame_context = 2;
    }
  }

  // Configure experimental use of segmentation for enhanced coding of
  // static regions if indicated.
  // Only allowed in second pass of two pass (as requires lagged coding)
  // and if the relevant speed feature flag is set.
  if (cpi->pass == 2 && cpi->sf.static_segmentation)
    configure_static_seg_features(cpi);

  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame.
  if (cpi->pass == 0 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER &&
      cm->frame_type != KEY_FRAME) {
    if (vp9_rc_drop_frame(cpi)) {
      vp9_rc_postencode_update_drop_frame(cpi);
      ++cm->current_video_frame;
      return;
    }
  }

  vp9_clear_system_state();

  vp9_zero(cpi->rd_tx_select_threshes);

#if CONFIG_VP9_POSTPROC
  if (cpi->oxcf.noise_sensitivity > 0) {
    int l = 0;
    switch (cpi->oxcf.noise_sensitivity) {
      case 1:
        l = 20;
        break;
      case 2:
        l = 40;
        break;
      case 3:
        l = 60;
        break;
      case 4:
      case 5:
        l = 100;
        break;
      case 6:
        l = 150;
        break;
    }
    vp9_denoise(cpi->Source, cpi->Source, l);
  }
#endif

#ifdef OUTPUT_YUV_SRC
  vp9_write_yuv_frame(cpi->Source);
#endif

  // Decide q and q bounds.
  q = vp9_rc_pick_q_and_bounds(cpi, &bottom_index, &top_index);

  if (!frame_is_intra_only(cm)) {
    cm->interp_filter = DEFAULT_INTERP_FILTER;
    /* TODO: Decide this more intelligently */
    set_high_precision_mv(cpi, (q < HIGH_PRECISION_MV_QTHRESH));
  }

  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    encode_without_recode_loop(cpi, size, dest, q);
  } else {
    encode_with_recode_loop(cpi, size, dest, q, bottom_index, top_index);
  }

  // Special case code to reduce pulsing when key frames are forced at a
  // fixed interval. Note the reconstruction error if it is the frame before
  // the force key frame
  if (cpi->rc.next_key_frame_forced && cpi->rc.frames_to_key == 1) {
    cpi->ambient_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));
  }

  // If the encoder forced a KEY_FRAME decision
  if (cm->frame_type == KEY_FRAME)
    cpi->refresh_last_frame = 1;

  cm->frame_to_show = get_frame_new_buffer(cm);

#if WRITE_RECON_BUFFER
  if (cm->show_frame)
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame);
  else
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 1000);
#endif

  // Pick the loop filter level for the frame.
  loopfilter_frame(cpi, cm);

#if WRITE_RECON_BUFFER
  if (cm->show_frame)
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 2000);
  else
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 3000);
#endif

  // build the bitstream
  cpi->dummy_packing = 0;
  vp9_pack_bitstream(cpi, dest, size);

  if (cm->seg.update_map)
    update_reference_segmentation_map(cpi);

  release_scaled_references(cpi);
  update_reference_frames(cpi);

  for (t = TX_4X4; t <= TX_32X32; t++)
    full_to_model_counts(cm->counts.coef[t], cpi->coef_counts[t]);

  if (!cm->error_resilient_mode && !cm->frame_parallel_decoding_mode)
    vp9_adapt_coef_probs(cm);

  if (!frame_is_intra_only(cm)) {
    if (!cm->error_resilient_mode && !cm->frame_parallel_decoding_mode) {
      vp9_adapt_mode_probs(cm);
      vp9_adapt_mv_probs(cm, cm->allow_high_precision_mv);
    }
  }

#if 0
  output_frame_level_debug_stats(cpi);
#endif
  if (cpi->refresh_golden_frame == 1)
    cm->frame_flags |= FRAMEFLAGS_GOLDEN;
  else
    cm->frame_flags &= ~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cm->frame_flags |= FRAMEFLAGS_ALTREF;
  else
    cm->frame_flags &= ~FRAMEFLAGS_ALTREF;

  get_ref_frame_flags(cpi);

  vp9_rc_postencode_update(cpi, *size);

  if (cm->frame_type == KEY_FRAME) {
    // Tell the caller that the frame was coded as a key frame
    *frame_flags = cm->frame_flags | FRAMEFLAGS_KEY;

#if CONFIG_MULTIPLE_ARF
    // Reset the sequence number.
    if (cpi->multi_arf_enabled) {
      cpi->sequence_number = 0;
      cpi->frame_coding_order_period = cpi->new_frame_coding_order_period;
      cpi->new_frame_coding_order_period = -1;
    }
#endif
  } else {
    *frame_flags = cm->frame_flags&~FRAMEFLAGS_KEY;

#if CONFIG_MULTIPLE_ARF
    /* Increment position in the coded frame sequence. */
    if (cpi->multi_arf_enabled) {
      ++cpi->sequence_number;
      if (cpi->sequence_number >= cpi->frame_coding_order_period) {
        cpi->sequence_number = 0;
        cpi->frame_coding_order_period = cpi->new_frame_coding_order_period;
        cpi->new_frame_coding_order_period = -1;
      }
      cpi->this_frame_weight = cpi->arf_weight[cpi->sequence_number];
      assert(cpi->this_frame_weight >= 0);
    }
#endif
  }

  // Clear the one shot update flags for segmentation map and mode/ref loop
  // filter deltas.
  cm->seg.update_map = 0;
  cm->seg.update_data = 0;
  cm->lf.mode_ref_delta_update = 0;

  // keep track of the last coded dimensions
  cm->last_width = cm->width;
  cm->last_height = cm->height;

  // reset to normal state now that we are done.
  if (!cm->show_existing_frame)
    cm->last_show_frame = cm->show_frame;

  if (cm->show_frame) {
    // current mip will be the prev_mip for the next frame
    MODE_INFO *temp = cm->prev_mip;
    MODE_INFO **temp2 = cm->prev_mi_grid_base;
    cm->prev_mip = cm->mip;
    cm->mip = temp;
    cm->prev_mi_grid_base = cm->mi_grid_base;
    cm->mi_grid_base = temp2;

    // update the upper left visible macroblock ptrs
    cm->mi = cm->mip + cm->mode_info_stride + 1;
    cm->mi_grid_visible = cm->mi_grid_base + cm->mode_info_stride + 1;

    cpi->mb.e_mbd.mi_8x8 = cm->mi_grid_visible;
    cpi->mb.e_mbd.mi_8x8[0] = cm->mi;

    // Don't increment frame counters if this was an altref buffer
    // update not a real frame
    ++cm->current_video_frame;
  }

  // restore prev_mi
  cm->prev_mi = cm->prev_mip + cm->mode_info_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mode_info_stride + 1;
}

static void SvcEncode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                      unsigned int *frame_flags) {
  vp9_rc_get_svc_params(cpi);
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);
}

static void Pass0Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  if (cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    vp9_rc_get_one_pass_cbr_params(cpi);
  } else {
    vp9_rc_get_one_pass_vbr_params(cpi);
  }
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);
}

static void Pass1Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  (void) size;
  (void) dest;
  (void) frame_flags;

  vp9_rc_get_first_pass_params(cpi);
  vp9_set_quantizer(cpi, find_fp_qindex());
  vp9_first_pass(cpi);
}

static void Pass2Encode(VP9_COMP *cpi, size_t *size,
                        uint8_t *dest, unsigned int *frame_flags) {
  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  vp9_rc_get_second_pass_params(cpi);
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);

  vp9_twopass_postencode_update(cpi, *size);
}

static void check_initial_width(VP9_COMP *cpi, int subsampling_x,
                                int subsampling_y) {
  VP9_COMMON *const cm = &cpi->common;
  if (!cpi->initial_width) {
    cm->subsampling_x = subsampling_x;
    cm->subsampling_y = subsampling_y;
    alloc_raw_frame_buffers(cpi);
    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
  }
}


int vp9_receive_raw_frame(VP9_PTR ptr, unsigned int frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  VP9_COMP              *cpi = (VP9_COMP *) ptr;
  VP9_COMMON             *cm = &cpi->common;
  struct vpx_usec_timer  timer;
  int                    res = 0;
  const int    subsampling_x = sd->uv_width  < sd->y_width;
  const int    subsampling_y = sd->uv_height < sd->y_height;

  check_initial_width(cpi, subsampling_x, subsampling_y);
  vpx_usec_timer_start(&timer);
  if (vp9_lookahead_push(cpi->lookahead, sd, time_stamp, end_time, frame_flags,
                         cpi->active_map_enabled ? cpi->active_map : NULL))
    res = -1;
  vpx_usec_timer_mark(&timer);
  cpi->time_receive_data += vpx_usec_timer_elapsed(&timer);

  if (cm->version == 0 && (subsampling_x != 1 || subsampling_y != 1)) {
    vpx_internal_error(&cm->error, VPX_CODEC_INVALID_PARAM,
                       "Non-4:2:0 color space requires profile >= 1");
    res = -1;
  }

  return res;
}


static int frame_is_reference(const VP9_COMP *cpi) {
  const VP9_COMMON *cm = &cpi->common;

  return cm->frame_type == KEY_FRAME ||
         cpi->refresh_last_frame ||
         cpi->refresh_golden_frame ||
         cpi->refresh_alt_ref_frame ||
         cm->refresh_frame_context ||
         cm->lf.mode_ref_delta_update ||
         cm->seg.update_map ||
         cm->seg.update_data;
}

#if CONFIG_MULTIPLE_ARF
int is_next_frame_arf(VP9_COMP *cpi) {
  // Negative entry in frame_coding_order indicates an ARF at this position.
  return cpi->frame_coding_order[cpi->sequence_number + 1] < 0 ? 1 : 0;
}
#endif

void adjust_frame_rate(VP9_COMP *cpi) {
  int64_t this_duration;
  int step = 0;

  if (cpi->source->ts_start == cpi->first_time_stamp_ever) {
    this_duration = cpi->source->ts_end - cpi->source->ts_start;
    step = 1;
  } else {
    int64_t last_duration = cpi->last_end_time_stamp_seen
        - cpi->last_time_stamp_seen;

    this_duration = cpi->source->ts_end - cpi->last_end_time_stamp_seen;

    // do a step update if the duration changes by 10%
    if (last_duration)
      step = (int)((this_duration - last_duration) * 10 / last_duration);
  }

  if (this_duration) {
    if (step) {
      vp9_new_framerate(cpi, 10000000.0 / this_duration);
    } else {
      // Average this frame's rate into the last second's average
      // frame rate. If we haven't seen 1 second yet, then average
      // over the whole interval seen.
      const double interval = MIN((double)(cpi->source->ts_end
                                   - cpi->first_time_stamp_ever), 10000000.0);
      double avg_duration = 10000000.0 / cpi->oxcf.framerate;
      avg_duration *= (interval - avg_duration + this_duration);
      avg_duration /= interval;

      vp9_new_framerate(cpi, 10000000.0 / avg_duration);
    }
  }
  cpi->last_time_stamp_seen = cpi->source->ts_start;
  cpi->last_end_time_stamp_seen = cpi->source->ts_end;
}

int vp9_get_compressed_data(VP9_PTR ptr, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest,
                            int64_t *time_stamp, int64_t *time_end, int flush) {
  VP9_COMP *cpi = (VP9_COMP *) ptr;
  VP9_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &cpi->mb.e_mbd;
  struct vpx_usec_timer  cmptimer;
  YV12_BUFFER_CONFIG *force_src_buffer = NULL;
  MV_REFERENCE_FRAME ref_frame;

  if (!cpi)
    return -1;

  vpx_usec_timer_start(&cmptimer);

  cpi->source = NULL;

  set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV);

  // Normal defaults
  cm->reset_frame_context = 0;
  cm->refresh_frame_context = 1;
  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
  cpi->refresh_alt_ref_frame = 0;

  // Should we code an alternate reference frame.
  if (cpi->oxcf.play_alternate && cpi->rc.source_alt_ref_pending) {
    int frames_to_arf;

#if CONFIG_MULTIPLE_ARF
    assert(!cpi->multi_arf_enabled ||
           cpi->frame_coding_order[cpi->sequence_number] < 0);

    if (cpi->multi_arf_enabled && (cpi->pass == 2))
      frames_to_arf = (-cpi->frame_coding_order[cpi->sequence_number])
          - cpi->next_frame_in_order;
    else
#endif
      frames_to_arf = cpi->rc.frames_till_gf_update_due;

    assert(frames_to_arf <= cpi->rc.frames_to_key);

    if ((cpi->source = vp9_lookahead_peek(cpi->lookahead, frames_to_arf))) {
#if CONFIG_MULTIPLE_ARF
      cpi->alt_ref_source[cpi->arf_buffered] = cpi->source;
#else
      cpi->alt_ref_source = cpi->source;
#endif

      if (cpi->oxcf.arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        // TODO(agrange) merge these two functions.
        vp9_configure_arnr_filter(cpi, frames_to_arf, cpi->rc.gfu_boost);
        vp9_temporal_filter_prepare(cpi, frames_to_arf);
        vp9_extend_frame_borders(&cpi->alt_ref_buffer,
                                 cm->subsampling_x, cm->subsampling_y);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_last_frame = 0;
      cpi->rc.is_src_frame_alt_ref = 0;

#if CONFIG_MULTIPLE_ARF
      if (!cpi->multi_arf_enabled)
#endif
        cpi->rc.source_alt_ref_pending = 0;
    } else {
      cpi->rc.source_alt_ref_pending = 0;
    }
  }

  if (!cpi->source) {
#if CONFIG_MULTIPLE_ARF
    int i;
#endif
    if ((cpi->source = vp9_lookahead_pop(cpi->lookahead, flush))) {
      cm->show_frame = 1;
      cm->intra_only = 0;

#if CONFIG_MULTIPLE_ARF
      // Is this frame the ARF overlay.
      cpi->rc.is_src_frame_alt_ref = 0;
      for (i = 0; i < cpi->arf_buffered; ++i) {
        if (cpi->source == cpi->alt_ref_source[i]) {
          cpi->rc.is_src_frame_alt_ref = 1;
          cpi->refresh_golden_frame = 1;
          break;
        }
      }
#else
      cpi->rc.is_src_frame_alt_ref = cpi->alt_ref_source
          && (cpi->source == cpi->alt_ref_source);
#endif
      if (cpi->rc.is_src_frame_alt_ref) {
        // Current frame is an ARF overlay frame.
#if CONFIG_MULTIPLE_ARF
        cpi->alt_ref_source[i] = NULL;
#else
        cpi->alt_ref_source = NULL;
#endif
        // Don't refresh the last buffer for an ARF overlay frame. It will
        // become the GF so preserve last as an alternative prediction option.
        cpi->refresh_last_frame = 0;
      }
#if CONFIG_MULTIPLE_ARF
      ++cpi->next_frame_in_order;
#endif
    }
  }

  if (cpi->source) {
    cpi->un_scaled_source = cpi->Source = force_src_buffer ? force_src_buffer
                                                           : &cpi->source->img;
    *time_stamp = cpi->source->ts_start;
    *time_end = cpi->source->ts_end;
    *frame_flags = cpi->source->flags;

#if CONFIG_MULTIPLE_ARF
    if ((cm->frame_type != KEY_FRAME) && (cpi->pass == 2))
      cpi->rc.source_alt_ref_pending = is_next_frame_arf(cpi);
#endif
  } else {
    *size = 0;
    if (flush && cpi->pass == 1 && !cpi->twopass.first_pass_done) {
      vp9_end_first_pass(cpi);    /* get last stats packet */
      cpi->twopass.first_pass_done = 1;
    }
    return -1;
  }

  if (cpi->source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = cpi->source->ts_start;
    cpi->last_end_time_stamp_seen = cpi->source->ts_start;
  }

  // adjust frame rates based on timestamps given
  if (cm->show_frame) {
    adjust_frame_rate(cpi);
  }

  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    update_layer_framerate(cpi);
    restore_layer_context(cpi);
  }

  // start with a 0 size frame
  *size = 0;

  // Clear down mmx registers
  vp9_clear_system_state();  // __asm emms;

  /* find a free buffer for the new frame, releasing the reference previously
   * held.
   */
  cm->frame_bufs[cm->new_fb_idx].ref_count--;
  cm->new_fb_idx = get_free_fb(cm);

#if CONFIG_MULTIPLE_ARF
  /* Set up the correct ARF frame. */
  if (cpi->refresh_alt_ref_frame) {
    ++cpi->arf_buffered;
  }
  if (cpi->multi_arf_enabled && (cm->frame_type != KEY_FRAME) &&
      (cpi->pass == 2)) {
    cpi->alt_fb_idx = cpi->arf_buffer_idx[cpi->sequence_number];
  }
#endif

  cm->frame_flags = *frame_flags;

  // Reset the frame pointers to the current frame size
  vp9_realloc_frame_buffer(get_frame_new_buffer(cm),
                           cm->width, cm->height,
                           cm->subsampling_x, cm->subsampling_y,
                           VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL);

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int idx = cm->ref_frame_map[get_ref_frame_idx(cpi, ref_frame)];
    YV12_BUFFER_CONFIG *const buf = &cm->frame_bufs[idx].buf;
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - 1];
    ref_buf->buf = buf;
    ref_buf->idx = idx;
    vp9_setup_scale_factors_for_frame(&ref_buf->sf,
                                      buf->y_crop_width, buf->y_crop_height,
                                      cm->width, cm->height);

    if (vp9_is_scaled(&ref_buf->sf))
      vp9_extend_frame_borders(buf, cm->subsampling_x, cm->subsampling_y);
  }

  set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);
  xd->interp_kernel = vp9_get_interp_kernel(
      DEFAULT_INTERP_FILTER == SWITCHABLE ? EIGHTTAP : DEFAULT_INTERP_FILTER);

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_vaq_init();
  }

  if (cpi->use_svc) {
    SvcEncode(cpi, size, dest, frame_flags);
  } else if (cpi->pass == 1) {
    Pass1Encode(cpi, size, dest, frame_flags);
  } else if (cpi->pass == 2) {
    Pass2Encode(cpi, size, dest, frame_flags);
  } else {
    // One pass encode
    Pass0Encode(cpi, size, dest, frame_flags);
  }

  if (cm->refresh_frame_context)
    cm->frame_contexts[cm->frame_context_idx] = cm->fc;

  // Frame was dropped, release scaled references.
  if (*size == 0) {
    release_scaled_references(cpi);
  }

  if (*size > 0) {
    cpi->droppable = !frame_is_reference(cpi);
  }

  // Save layer specific state.
  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) {
    save_layer_context(cpi);
  }

  vpx_usec_timer_mark(&cmptimer);
  cpi->time_compress_data += vpx_usec_timer_elapsed(&cmptimer);

  if (cpi->b_calculate_psnr && cpi->pass != 1 && cm->show_frame)
    generate_psnr_packet(cpi);

#if CONFIG_INTERNAL_STATS

  if (cpi->pass != 1) {
    cpi->bytes += *size;

    if (cm->show_frame) {
      cpi->count++;

      if (cpi->b_calculate_psnr) {
        YV12_BUFFER_CONFIG *orig = cpi->Source;
        YV12_BUFFER_CONFIG *recon = cpi->common.frame_to_show;
        YV12_BUFFER_CONFIG *pp = &cm->post_proc_buffer;
        PSNR_STATS psnr;
        calc_psnr(orig, recon, &psnr);

        cpi->total += psnr.psnr[0];
        cpi->total_y += psnr.psnr[1];
        cpi->total_u += psnr.psnr[2];
        cpi->total_v += psnr.psnr[3];
        cpi->total_sq_error += psnr.sse[0];
        cpi->total_samples += psnr.samples[0];

        {
          PSNR_STATS psnr2;
          double frame_ssim2 = 0, weight = 0;
#if CONFIG_VP9_POSTPROC
          vp9_deblock(cm->frame_to_show, &cm->post_proc_buffer,
                      cm->lf.filter_level * 10 / 6);
#endif
          vp9_clear_system_state();

          calc_psnr(orig, pp, &psnr2);

          cpi->totalp += psnr2.psnr[0];
          cpi->totalp_y += psnr2.psnr[1];
          cpi->totalp_u += psnr2.psnr[2];
          cpi->totalp_v += psnr2.psnr[3];
          cpi->totalp_sq_error += psnr2.sse[0];
          cpi->totalp_samples += psnr2.samples[0];

          frame_ssim2 = vp9_calc_ssim(orig, recon, 1, &weight);

          cpi->summed_quality += frame_ssim2 * weight;
          cpi->summed_weights += weight;

          frame_ssim2 = vp9_calc_ssim(orig, &cm->post_proc_buffer, 1, &weight);

          cpi->summedp_quality += frame_ssim2 * weight;
          cpi->summedp_weights += weight;
#if 0
          {
            FILE *f = fopen("q_used.stt", "a");
            fprintf(f, "%5d : Y%f7.3:U%f7.3:V%f7.3:F%f7.3:S%7.3f\n",
                    cpi->common.current_video_frame, y2, u2, v2,
                    frame_psnr2, frame_ssim2);
            fclose(f);
          }
#endif
        }
      }

      if (cpi->b_calculate_ssimg) {
        double y, u, v, frame_all;
        frame_all = vp9_calc_ssimg(cpi->Source, cm->frame_to_show, &y, &u, &v);
        cpi->total_ssimg_y += y;
        cpi->total_ssimg_u += u;
        cpi->total_ssimg_v += v;
        cpi->total_ssimg_all += frame_all;
      }
    }
  }

#endif
  return 0;
}

int vp9_get_preview_raw_frame(VP9_PTR comp, YV12_BUFFER_CONFIG *dest,
                              vp9_ppflags_t *flags) {
  VP9_COMP *cpi = (VP9_COMP *) comp;

  if (!cpi->common.show_frame) {
    return -1;
  } else {
    int ret;
#if CONFIG_VP9_POSTPROC
    ret = vp9_post_proc_frame(&cpi->common, dest, flags);
#else

    if (cpi->common.frame_to_show) {
      *dest = *cpi->common.frame_to_show;
      dest->y_width = cpi->common.width;
      dest->y_height = cpi->common.height;
      dest->uv_width = cpi->common.width >> cpi->common.subsampling_x;
      dest->uv_height = cpi->common.height >> cpi->common.subsampling_y;
      ret = 0;
    } else {
      ret = -1;
    }

#endif  // !CONFIG_VP9_POSTPROC
    vp9_clear_system_state();
    return ret;
  }
}

int vp9_set_roimap(VP9_PTR comp, unsigned char *map, unsigned int rows,
                   unsigned int cols, int delta_q[MAX_SEGMENTS],
                   int delta_lf[MAX_SEGMENTS],
                   unsigned int threshold[MAX_SEGMENTS]) {
  VP9_COMP *cpi = (VP9_COMP *) comp;
  signed char feature_data[SEG_LVL_MAX][MAX_SEGMENTS];
  struct segmentation *seg = &cpi->common.seg;
  int i;

  if (cpi->common.mb_rows != rows || cpi->common.mb_cols != cols)
    return -1;

  if (!map) {
    vp9_disable_segmentation((VP9_PTR)cpi);
    return 0;
  }

  // Set the segmentation Map
  vp9_set_segmentation_map((VP9_PTR)cpi, map);

  // Activate segmentation.
  vp9_enable_segmentation((VP9_PTR)cpi);

  // Set up the quant, LF and breakout threshold segment data
  for (i = 0; i < MAX_SEGMENTS; i++) {
    feature_data[SEG_LVL_ALT_Q][i] = delta_q[i];
    feature_data[SEG_LVL_ALT_LF][i] = delta_lf[i];
    cpi->segment_encode_breakout[i] = threshold[i];
  }

  // Enable the loop and quant changes in the feature mask
  for (i = 0; i < MAX_SEGMENTS; i++) {
    if (delta_q[i])
      vp9_enable_segfeature(seg, i, SEG_LVL_ALT_Q);
    else
      vp9_disable_segfeature(seg, i, SEG_LVL_ALT_Q);

    if (delta_lf[i])
      vp9_enable_segfeature(seg, i, SEG_LVL_ALT_LF);
    else
      vp9_disable_segfeature(seg, i, SEG_LVL_ALT_LF);
  }

  // Initialize the feature data structure
  // SEGMENT_DELTADATA    0, SEGMENT_ABSDATA      1
  vp9_set_segment_data((VP9_PTR)cpi, &feature_data[0][0], SEGMENT_DELTADATA);

  return 0;
}

int vp9_set_active_map(VP9_PTR comp, unsigned char *map,
                       unsigned int rows, unsigned int cols) {
  VP9_COMP *cpi = (VP9_COMP *) comp;

  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols) {
    if (map) {
      vpx_memcpy(cpi->active_map, map, rows * cols);
      cpi->active_map_enabled = 1;
    } else {
      cpi->active_map_enabled = 0;
    }

    return 0;
  } else {
    // cpi->active_map_enabled = 0;
    return -1;
  }
}

int vp9_set_internal_size(VP9_PTR comp,
                          VPX_SCALING horiz_mode, VPX_SCALING vert_mode) {
  VP9_COMP *cpi = (VP9_COMP *) comp;
  VP9_COMMON *cm = &cpi->common;
  int hr = 0, hs = 0, vr = 0, vs = 0;

  if (horiz_mode > ONETWO || vert_mode > ONETWO)
    return -1;

  Scale2Ratio(horiz_mode, &hr, &hs);
  Scale2Ratio(vert_mode, &vr, &vs);

  // always go to the next whole number
  cm->width = (hs - 1 + cpi->oxcf.width * hr) / hs;
  cm->height = (vs - 1 + cpi->oxcf.height * vr) / vs;

  assert(cm->width <= cpi->initial_width);
  assert(cm->height <= cpi->initial_height);
  update_frame_size(cpi);
  return 0;
}

int vp9_set_size_literal(VP9_PTR comp, unsigned int width,
                         unsigned int height) {
  VP9_COMP *cpi = (VP9_COMP *)comp;
  VP9_COMMON *cm = &cpi->common;

  check_initial_width(cpi, 1, 1);

  if (width) {
    cm->width = width;
    if (cm->width * 5 < cpi->initial_width) {
      cm->width = cpi->initial_width / 5 + 1;
      printf("Warning: Desired width too small, changed to %d\n", cm->width);
    }
    if (cm->width > cpi->initial_width) {
      cm->width = cpi->initial_width;
      printf("Warning: Desired width too large, changed to %d\n", cm->width);
    }
  }

  if (height) {
    cm->height = height;
    if (cm->height * 5 < cpi->initial_height) {
      cm->height = cpi->initial_height / 5 + 1;
      printf("Warning: Desired height too small, changed to %d\n", cm->height);
    }
    if (cm->height > cpi->initial_height) {
      cm->height = cpi->initial_height;
      printf("Warning: Desired height too large, changed to %d\n", cm->height);
    }
  }

  assert(cm->width <= cpi->initial_width);
  assert(cm->height <= cpi->initial_height);
  update_frame_size(cpi);
  return 0;
}

void vp9_set_svc(VP9_PTR comp, int use_svc) {
  VP9_COMP *cpi = (VP9_COMP *)comp;
  cpi->use_svc = use_svc;
  return;
}

int vp9_calc_ss_err(const YV12_BUFFER_CONFIG *source,
                    const YV12_BUFFER_CONFIG *reference) {
  int i, j;
  int total = 0;

  const uint8_t *src = source->y_buffer;
  const uint8_t *ref = reference->y_buffer;

  // Loop through the Y plane raw and reconstruction data summing
  // (square differences)
  for (i = 0; i < source->y_height; i += 16) {
    for (j = 0; j < source->y_width; j += 16) {
      unsigned int sse;
      total += vp9_mse16x16(src + j, source->y_stride,
                            ref + j, reference->y_stride, &sse);
    }

    src += 16 * source->y_stride;
    ref += 16 * reference->y_stride;
  }

  return total;
}


int vp9_get_quantizer(VP9_PTR c) {
  return ((VP9_COMP *)c)->common.base_qindex;
}
