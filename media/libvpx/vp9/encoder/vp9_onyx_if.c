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

#include "vpx_ports/vpx_timer.h"


extern void print_tree_update_probs();

static void set_default_lf_deltas(struct loopfilter *lf);

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


#ifdef ENTROPY_STATS
extern int intra_mode_stats[INTRA_MODES]
                           [INTRA_MODES]
                           [INTRA_MODES];
#endif

#ifdef MODE_STATS
extern void init_tx_count_stats();
extern void write_tx_count_stats();
extern void init_switchable_interp_stats();
extern void write_switchable_interp_stats();
#endif

#ifdef SPEEDSTATS
unsigned int frames_at_speed[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0};
#endif

#if defined(SECTIONBITS_OUTPUT)
extern unsigned __int64 Sectionbits[500];
#endif

extern void vp9_init_quantizer(VP9_COMP *cpi);

// Tables relating active max Q to active min Q
static int kf_low_motion_minq[QINDEX_RANGE];
static int kf_high_motion_minq[QINDEX_RANGE];
static int gf_low_motion_minq[QINDEX_RANGE];
static int gf_high_motion_minq[QINDEX_RANGE];
static int inter_minq[QINDEX_RANGE];
static int afq_low_motion_minq[QINDEX_RANGE];
static int afq_high_motion_minq[QINDEX_RANGE];

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

// Functions to compute the active minq lookup table entries based on a
// formulaic approach to facilitate easier adjustment of the Q tables.
// The formulae were derived from computing a 3rd order polynomial best
// fit to the original data (after plotting real maxq vs minq (not q index))
static int calculate_minq_index(double maxq,
                                double x3, double x2, double x1, double c) {
  int i;
  const double minqtarget = MIN(((x3 * maxq + x2) * maxq + x1) * maxq + c,
                                maxq);

  // Special case handling to deal with the step from q2.0
  // down to lossless mode represented by q 1.0.
  if (minqtarget <= 2.0)
    return 0;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (minqtarget <= vp9_convert_qindex_to_q(i))
      return i;
  }

  return QINDEX_RANGE - 1;
}

static void init_minq_luts(void) {
  int i;

  for (i = 0; i < QINDEX_RANGE; i++) {
    const double maxq = vp9_convert_qindex_to_q(i);


    kf_low_motion_minq[i] = calculate_minq_index(maxq,
                                                 0.000001,
                                                 -0.0004,
                                                 0.15,
                                                 0.0);
    kf_high_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.000002,
                                                  -0.0012,
                                                  0.5,
                                                  0.0);

    gf_low_motion_minq[i] = calculate_minq_index(maxq,
                                                 0.0000015,
                                                 -0.0009,
                                                 0.32,
                                                 0.0);
    gf_high_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.0000021,
                                                  -0.00125,
                                                  0.50,
                                                  0.0);
    inter_minq[i] = calculate_minq_index(maxq,
                                         0.00000271,
                                         -0.00113,
                                         0.75,
                                         0.0);
    afq_low_motion_minq[i] = calculate_minq_index(maxq,
                                                  0.0000015,
                                                  -0.0009,
                                                  0.33,
                                                  0.0);
    afq_high_motion_minq[i] = calculate_minq_index(maxq,
                                                   0.0000021,
                                                   -0.00125,
                                                   0.55,
                                                   0.0);
  }
}

static int get_active_quality(int q,
                              int gfu_boost,
                              int low,
                              int high,
                              int *low_motion_minq,
                              int *high_motion_minq) {
  int active_best_quality;
  if (gfu_boost > high) {
    active_best_quality = low_motion_minq[q];
  } else if (gfu_boost < low) {
    active_best_quality = high_motion_minq[q];
  } else {
    const int gap = high - low;
    const int offset = high - gfu_boost;
    const int qdiff = high_motion_minq[q] - low_motion_minq[q];
    const int adjustment = ((offset * qdiff) + (gap >> 1)) / gap;
    active_best_quality = low_motion_minq[q] + adjustment;
  }
  return active_best_quality;
}

static void set_mvcost(VP9_COMP *cpi) {
  MACROBLOCK *const mb = &cpi->mb;
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
    vp9_tokenize_initialize();
    vp9_init_quant_tables();
    vp9_init_me_luts();
    init_minq_luts();
    // init_base_skip_probs();
    init_done = 1;
  }
}

static void setup_features(VP9_COMMON *cm) {
  struct loopfilter *const lf = &cm->lf;
  struct segmentation *const seg = &cm->seg;

  // Set up default state for MB feature flags
  seg->enabled = 0;

  seg->update_map = 0;
  seg->update_data = 0;
  vpx_memset(seg->tree_probs, 255, sizeof(seg->tree_probs));

  vp9_clearall_segfeatures(seg);

  lf->mode_ref_delta_enabled = 0;
  lf->mode_ref_delta_update = 0;
  vp9_zero(lf->ref_deltas);
  vp9_zero(lf->mode_deltas);
  vp9_zero(lf->last_ref_deltas);
  vp9_zero(lf->last_mode_deltas);

  set_default_lf_deltas(lf);
}

static void dealloc_compressor_data(VP9_COMP *cpi) {
  // Delete sementation map
  vpx_free(cpi->segmentation_map);
  cpi->segmentation_map = 0;
  vpx_free(cpi->common.last_frame_seg_map);
  cpi->common.last_frame_seg_map = 0;
  vpx_free(cpi->coding_context.last_frame_seg_map_copy);
  cpi->coding_context.last_frame_seg_map_copy = 0;

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
int vp9_compute_qdelta(VP9_COMP *cpi, double qstart, double qtarget) {
  int i;
  int start_index = cpi->worst_quality;
  int target_index = cpi->worst_quality;

  // Convert the average q value to an index.
  for (i = cpi->best_quality; i < cpi->worst_quality; i++) {
    start_index = i;
    if (vp9_convert_qindex_to_q(i) >= qstart)
      break;
  }

  // Convert the q target to an index
  for (i = cpi->best_quality; i < cpi->worst_quality; i++) {
    target_index = i;
    if (vp9_convert_qindex_to_q(i) >= qtarget)
      break;
  }

  return target_index - start_index;
}

static void configure_static_seg_features(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  struct segmentation *seg = &cm->seg;

  int high_q = (int)(cpi->avg_q > 48.0);
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

      qi_delta = vp9_compute_qdelta(cpi, cpi->avg_q, (cpi->avg_q * 0.875));
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
    if (cpi->frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (cpi->source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;
        seg->abs_delta = SEGMENT_DELTADATA;

        qi_delta = vp9_compute_qdelta(cpi, cpi->avg_q,
                                      (cpi->avg_q * 1.125));
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
    } else if (cpi->is_src_frame_alt_ref) {
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

#ifdef ENTROPY_STATS
void vp9_update_mode_context_stats(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int i, j;
  unsigned int (*inter_mode_counts)[INTER_MODES - 1][2] =
      cm->fc.inter_mode_counts;
  int64_t (*mv_ref_stats)[INTER_MODES - 1][2] = cpi->mv_ref_stats;
  FILE *f;

  // Read the past stats counters
  f = fopen("mode_context.bin",  "rb");
  if (!f) {
    vpx_memset(cpi->mv_ref_stats, 0, sizeof(cpi->mv_ref_stats));
  } else {
    fread(cpi->mv_ref_stats, sizeof(cpi->mv_ref_stats), 1, f);
    fclose(f);
  }

  // Add in the values for this frame
  for (i = 0; i < INTER_MODE_CONTEXTS; i++) {
    for (j = 0; j < INTER_MODES - 1; j++) {
      mv_ref_stats[i][j][0] += (int64_t)inter_mode_counts[i][j][0];
      mv_ref_stats[i][j][1] += (int64_t)inter_mode_counts[i][j][1];
    }
  }

  // Write back the accumulated stats
  f = fopen("mode_context.bin",  "wb");
  fwrite(cpi->mv_ref_stats, sizeof(cpi->mv_ref_stats), 1, f);
  fclose(f);
}

void print_mode_context(VP9_COMP *cpi) {
  FILE *f = fopen("vp9_modecont.c", "a");
  int i, j;

  fprintf(f, "#include \"vp9_entropy.h\"\n");
  fprintf(
      f,
      "const int inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1] =");
  fprintf(f, "{\n");
  for (j = 0; j < INTER_MODE_CONTEXTS; j++) {
    fprintf(f, "  {/* %d */ ", j);
    fprintf(f, "    ");
    for (i = 0; i < INTER_MODES - 1; i++) {
      int this_prob;
      int64_t count = cpi->mv_ref_stats[j][i][0] + cpi->mv_ref_stats[j][i][1];
      if (count)
        this_prob = ((cpi->mv_ref_stats[j][i][0] * 256) + (count >> 1)) / count;
      else
        this_prob = 128;

      // context probs
      fprintf(f, "%5d, ", this_prob);
    }
    fprintf(f, "  },\n");
  }

  fprintf(f, "};\n");
  fclose(f);
}
#endif  // ENTROPY_STATS

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

static void set_default_lf_deltas(struct loopfilter *lf) {
  lf->mode_ref_delta_enabled = 1;
  lf->mode_ref_delta_update = 1;

  vp9_zero(lf->ref_deltas);
  vp9_zero(lf->mode_deltas);

  // Test of ref frame deltas
  lf->ref_deltas[INTRA_FRAME] = 2;
  lf->ref_deltas[LAST_FRAME] = 0;
  lf->ref_deltas[GOLDEN_FRAME] = -2;
  lf->ref_deltas[ALTREF_FRAME] = -2;

  lf->mode_deltas[0] = 0;   // Zero
  lf->mode_deltas[1] = 0;   // New mv
}

static void set_rd_speed_thresholds(VP9_COMP *cpi, int mode) {
  SPEED_FEATURES *sf = &cpi->sf;
  int i;

  // Set baseline threshold values
  for (i = 0; i < MAX_MODES; ++i)
    sf->thresh_mult[i] = mode == 0 ? -500 : 0;

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

static void set_rd_speed_thresholds_sub8x8(VP9_COMP *cpi, int mode) {
  SPEED_FEATURES *sf = &cpi->sf;
  int i;

  for (i = 0; i < MAX_REFS; ++i)
    sf->thresh_mult_sub8x8[i] = mode == 0 ? -500 : 0;

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

void vp9_set_speed_features(VP9_COMP *cpi) {
  SPEED_FEATURES *sf = &cpi->sf;
  int mode = cpi->compressor_speed;
  int speed = cpi->speed;
  int i;

  // Only modes 0 and 1 supported for now in experimental code basae
  if (mode > 1)
    mode = 1;

  for (i = 0; i < MAX_MODES; ++i)
    cpi->mode_chosen_counts[i] = 0;

  // best quality defaults
  sf->RD = 1;
  sf->search_method = NSTEP;
  sf->auto_filter = 1;
  sf->recode_loop = 1;
  sf->subpel_search_method = SUBPEL_TREE;
  sf->subpel_iters_per_step = 2;
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
  sf->use_avoid_tested_higherror = 0;
  sf->reference_masking = 0;
  sf->use_one_partition_size_always = 0;
  sf->less_rectangular_check = 0;
  sf->use_square_partition_only = 0;
  sf->auto_min_max_partition_size = 0;
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
  sf->using_small_partition_info = 0;
  sf->mode_skip_start = MAX_MODES;  // Mode index at which mode skip mask set

#if CONFIG_MULTIPLE_ARF
  // Switch segmentation off.
  sf->static_segmentation = 0;
#else
  sf->static_segmentation = 0;
#endif

  switch (mode) {
    case 0:  // This is the best quality mode.
      break;

    case 1:
#if CONFIG_MULTIPLE_ARF
      // Switch segmentation off.
      sf->static_segmentation = 0;
#else
      sf->static_segmentation = 0;
#endif
      sf->use_avoid_tested_higherror = 1;
      sf->adaptive_rd_thresh = 1;
      sf->recode_loop = (speed < 1);

      if (speed == 1) {
        sf->use_square_partition_only = !frame_is_intra_only(&cpi->common);
        sf->less_rectangular_check  = 1;
        sf->tx_size_search_method = frame_is_intra_only(&cpi->common)
                                     ? USE_FULL_RD : USE_LARGESTALL;

        if (MIN(cpi->common.width, cpi->common.height) >= 720)
          sf->disable_split_mask = cpi->common.show_frame ?
              DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
        else
          sf->disable_split_mask = DISABLE_COMPOUND_SPLIT;

        sf->use_rd_breakout = 1;
        sf->adaptive_motion_search = 1;
        sf->auto_mv_step_size = 1;
        sf->adaptive_rd_thresh = 2;
        sf->recode_loop = 2;
        sf->intra_y_mode_mask[TX_32X32] = INTRA_DC_H_V;
        sf->intra_uv_mode_mask[TX_32X32] = INTRA_DC_H_V;
        sf->intra_uv_mode_mask[TX_16X16] = INTRA_DC_H_V;
      }
      if (speed == 2) {
        sf->use_square_partition_only = !frame_is_intra_only(&cpi->common);
        sf->less_rectangular_check  = 1;
        sf->tx_size_search_method = frame_is_intra_only(&cpi->common)
                                     ? USE_FULL_RD : USE_LARGESTALL;

        if (MIN(cpi->common.width, cpi->common.height) >= 720)
          sf->disable_split_mask = cpi->common.show_frame ?
              DISABLE_ALL_SPLIT : DISABLE_ALL_INTER_SPLIT;
        else
          sf->disable_split_mask = LAST_AND_INTRA_SPLIT_ONLY;


        sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                     FLAG_SKIP_INTRA_BESTINTER |
                                     FLAG_SKIP_COMP_BESTINTRA |
                                     FLAG_SKIP_INTRA_LOWVAR;

        sf->use_rd_breakout = 1;
        sf->adaptive_motion_search = 1;
        sf->auto_mv_step_size = 1;

        sf->disable_filter_search_var_thresh = 16;
        sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

        sf->auto_min_max_partition_size = 1;
        sf->use_lastframe_partitioning = LAST_FRAME_PARTITION_LOW_MOTION;
        sf->adjust_partitioning_from_last_frame = 1;
        sf->last_partitioning_redo_frequency = 3;

        sf->adaptive_rd_thresh = 2;
        sf->recode_loop = 2;
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

        if (MIN(cpi->common.width, cpi->common.height) >= 720)
          sf->disable_split_mask = DISABLE_ALL_SPLIT;
        else
          sf->disable_split_mask = DISABLE_ALL_INTER_SPLIT;

        sf->mode_search_skip_flags = FLAG_SKIP_INTRA_DIRMISMATCH |
                                     FLAG_SKIP_INTRA_BESTINTER |
                                     FLAG_SKIP_COMP_BESTINTRA |
                                     FLAG_SKIP_INTRA_LOWVAR;

        sf->use_rd_breakout = 1;
        sf->adaptive_motion_search = 1;
        sf->auto_mv_step_size = 1;

        sf->disable_filter_search_var_thresh = 16;
        sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

        sf->auto_min_max_partition_size = 1;
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
        sf->auto_mv_step_size = 1;

        sf->disable_filter_search_var_thresh = 16;
        sf->comp_inter_joint_search_thresh = BLOCK_SIZES;

        sf->auto_min_max_partition_size = 1;
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

        /* sf->intra_y_mode_mask = INTRA_DC_ONLY;
        sf->intra_uv_mode_mask = INTRA_DC_ONLY;
        sf->search_method = BIGDIA;
        sf->disable_split_var_thresh = 64;
        sf->disable_filter_search_var_thresh = 64; */
      }
      if (speed == 5) {
        sf->comp_inter_joint_search_thresh = BLOCK_SIZES;
        sf->use_one_partition_size_always = 1;
        sf->always_this_block_size = BLOCK_16X16;
        sf->tx_size_search_method = frame_is_intra_only(&cpi->common) ?
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
        // sf->reduce_first_step_size = 1;
        // sf->reference_masking = 1;

        sf->disable_split_mask = DISABLE_ALL_SPLIT;
        sf->search_method = HEX;
        sf->subpel_iters_per_step = 1;
        sf->disable_split_var_thresh = 64;
        sf->disable_filter_search_var_thresh = 96;
        for (i = 0; i < TX_SIZES; i++) {
          sf->intra_y_mode_mask[i] = INTRA_DC_ONLY;
          sf->intra_uv_mode_mask[i] = INTRA_DC_ONLY;
        }
        sf->use_fast_coef_updates = 2;
        sf->adaptive_rd_thresh = 4;
        sf->mode_skip_start = 6;
      }
      break;
  }; /* switch */

  // Set rd thresholds based on mode and speed setting
  set_rd_speed_thresholds(cpi, mode);
  set_rd_speed_thresholds_sub8x8(cpi, mode);

  // Slow quant, dct and trellis not worthwhile for first pass
  // so make sure they are always turned off.
  if (cpi->pass == 1) {
    sf->optimize_coefficients = 0;
  }

  // No recode for 1 pass.
  if (cpi->pass == 0) {
    sf->recode_loop = 0;
    sf->optimize_coefficients = 0;
  }

  cpi->mb.fwd_txm4x4 = vp9_fdct4x4;
  if (cpi->oxcf.lossless || cpi->mb.e_mbd.lossless) {
    cpi->mb.fwd_txm4x4 = vp9_fwht4x4;
  }

  if (cpi->sf.subpel_search_method == SUBPEL_ITERATIVE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_iterative;
    cpi->find_fractional_mv_step_comp = vp9_find_best_sub_pixel_comp_iterative;
  } else if (cpi->sf.subpel_search_method == SUBPEL_TREE) {
    cpi->find_fractional_mv_step = vp9_find_best_sub_pixel_tree;
    cpi->find_fractional_mv_step_comp = vp9_find_best_sub_pixel_comp_tree;
  }

  cpi->mb.optimize = cpi->sf.optimize_coefficients == 1 && cpi->pass != 1;

#ifdef SPEEDSTATS
  frames_at_speed[cpi->speed]++;
#endif
}

static void alloc_raw_frame_buffers(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  cpi->lookahead = vp9_lookahead_init(cpi->oxcf.width, cpi->oxcf.height,
                                      cm->subsampling_x, cm->subsampling_y,
                                      cpi->oxcf.lag_in_frames);
  if (!cpi->lookahead)
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  if (vp9_realloc_frame_buffer(&cpi->alt_ref_buffer,
                               cpi->oxcf.width, cpi->oxcf.height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9BORDERINPIXELS))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

void vp9_alloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  if (vp9_alloc_frame_buffers(cm, cm->width, cm->height))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffers");

  if (vp9_alloc_frame_buffer(&cpi->last_frame_uf,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9BORDERINPIXELS))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

  if (vp9_alloc_frame_buffer(&cpi->scaled_source,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9BORDERINPIXELS))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
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
                               VP9BORDERINPIXELS))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate last frame buffer");

  if (vp9_realloc_frame_buffer(&cpi->scaled_source,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9BORDERINPIXELS))
    vpx_internal_error(&cpi->common.error, VPX_CODEC_MEM_ERROR,
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
  if (framerate < 0.1)
    framerate = 30;

  cpi->oxcf.framerate = framerate;
  cpi->output_framerate = cpi->oxcf.framerate;
  cpi->per_frame_bandwidth = (int)(cpi->oxcf.target_bandwidth
                             / cpi->output_framerate);
  cpi->av_per_frame_bandwidth = (int)(cpi->oxcf.target_bandwidth
                                / cpi->output_framerate);
  cpi->min_frame_bandwidth = (int)(cpi->av_per_frame_bandwidth *
                                   cpi->oxcf.two_pass_vbrmin_section / 100);


  cpi->min_frame_bandwidth = MAX(cpi->min_frame_bandwidth, FRAME_OVERHEAD_BITS);

  // Set Maximum gf/arf interval
  cpi->max_gf_interval = 16;

  // Extended interval for genuinely static scenes
  cpi->twopass.static_scene_max_gf_interval = cpi->key_frame_frequency >> 1;

  // Special conditions when alt ref frame enabled in lagged compress mode
  if (cpi->oxcf.play_alternate && cpi->oxcf.lag_in_frames) {
    if (cpi->max_gf_interval > cpi->oxcf.lag_in_frames - 1)
      cpi->max_gf_interval = cpi->oxcf.lag_in_frames - 1;

    if (cpi->twopass.static_scene_max_gf_interval > cpi->oxcf.lag_in_frames - 1)
      cpi->twopass.static_scene_max_gf_interval = cpi->oxcf.lag_in_frames - 1;
  }

  if (cpi->max_gf_interval > cpi->twopass.static_scene_max_gf_interval)
    cpi->max_gf_interval = cpi->twopass.static_scene_max_gf_interval;
}

static int64_t rescale(int val, int64_t num, int denom) {
  int64_t llnum = num;
  int64_t llden = denom;
  int64_t llval = val;

  return (llval * llnum / llden);
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

  // change includes all joint functionality
  vp9_change_config(ptr, oxcf);

  // Initialize active best and worst q and average q values.
  cpi->active_worst_quality         = cpi->oxcf.worst_allowed_q;
  cpi->active_best_quality          = cpi->oxcf.best_allowed_q;
  cpi->avg_frame_qindex             = cpi->oxcf.worst_allowed_q;

  // Initialise the starting buffer levels
  cpi->buffer_level                 = cpi->oxcf.starting_buffer_level;
  cpi->bits_off_target              = cpi->oxcf.starting_buffer_level;

  cpi->rolling_target_bits          = cpi->av_per_frame_bandwidth;
  cpi->rolling_actual_bits          = cpi->av_per_frame_bandwidth;
  cpi->long_rolling_target_bits     = cpi->av_per_frame_bandwidth;
  cpi->long_rolling_actual_bits     = cpi->av_per_frame_bandwidth;

  cpi->total_actual_bits            = 0;
  cpi->total_target_vs_actual       = 0;

  cpi->static_mb_pct = 0;

  cpi->lst_fb_idx = 0;
  cpi->gld_fb_idx = 1;
  cpi->alt_fb_idx = 2;

  cpi->current_layer = 0;
  cpi->use_svc = 0;

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

  switch (cpi->oxcf.Mode) {
      // Real time and one pass deprecated in test code base
    case MODE_GOODQUALITY:
      cpi->pass = 0;
      cpi->compressor_speed = 2;
      cpi->oxcf.cpu_used = clamp(cpi->oxcf.cpu_used, -5, 5);
      break;

    case MODE_FIRSTPASS:
      cpi->pass = 1;
      cpi->compressor_speed = 1;
      break;

    case MODE_SECONDPASS:
      cpi->pass = 2;
      cpi->compressor_speed = 1;
      cpi->oxcf.cpu_used = clamp(cpi->oxcf.cpu_used, -5, 5);
      break;

    case MODE_SECONDPASS_BEST:
      cpi->pass = 2;
      cpi->compressor_speed = 0;
      break;
  }

  cpi->oxcf.worst_allowed_q = q_trans[oxcf->worst_allowed_q];
  cpi->oxcf.best_allowed_q = q_trans[oxcf->best_allowed_q];
  cpi->oxcf.cq_level = q_trans[cpi->oxcf.cq_level];

  cpi->oxcf.lossless = oxcf->lossless;
  cpi->mb.e_mbd.itxm_add = cpi->oxcf.lossless ? vp9_iwht4x4_add
                                              : vp9_idct4x4_add;
  cpi->baseline_gf_interval = DEFAULT_GF_INTERVAL;

  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  // cpi->use_golden_frame_only = 0;
  // cpi->use_last_frame_only = 0;
  cpi->refresh_golden_frame = 0;
  cpi->refresh_last_frame = 1;
  cm->refresh_frame_context = 1;
  cm->reset_frame_context = 0;

  setup_features(cm);
  cpi->common.allow_high_precision_mv = 0;  // Default mv precision
  set_mvcost(cpi);

  {
    int i;

    for (i = 0; i < MAX_SEGMENTS; i++)
      cpi->segment_encode_breakout[i] = cpi->oxcf.encode_breakout;
  }

  // At the moment the first order values may not be > MAXQ
  cpi->oxcf.fixed_q = MIN(cpi->oxcf.fixed_q, MAXQ);

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

  // Set up frame rate and related parameters rate control values.
  vp9_new_framerate(cpi, cpi->oxcf.framerate);

  // Set absolute upper and lower quality limits
  cpi->worst_quality = cpi->oxcf.worst_allowed_q;
  cpi->best_quality = cpi->oxcf.best_allowed_q;

  // active values should only be modified if out of new range
  cpi->active_worst_quality = clamp(cpi->active_worst_quality,
                                    cpi->oxcf.best_allowed_q,
                                    cpi->oxcf.worst_allowed_q);

  cpi->active_best_quality = clamp(cpi->active_best_quality,
                                   cpi->oxcf.best_allowed_q,
                                   cpi->oxcf.worst_allowed_q);

  cpi->buffered_mode = cpi->oxcf.optimal_buffer_level > 0;

  cpi->cq_target_quality = cpi->oxcf.cq_level;

  cm->mcomp_filter_type = DEFAULT_INTERP_FILTER;

  cpi->target_bandwidth = cpi->oxcf.target_bandwidth;

  cm->display_width = cpi->oxcf.width;
  cm->display_height = cpi->oxcf.height;

  // VP8 sharpness level mapping 0-7 (vs 0-10 in general VPx dialogs)
  cpi->oxcf.Sharpness = MIN(7, cpi->oxcf.Sharpness);

  cpi->common.lf.sharpness_level = cpi->oxcf.Sharpness;

  if (cpi->initial_width) {
    // Increasing the size of the frame beyond the first seen frame, or some
    // otherwise signalled maximum size, is not supported.
    // TODO(jkoleszar): exit gracefully.
    assert(cm->width <= cpi->initial_width);
    assert(cm->height <= cpi->initial_height);
  }
  update_frame_size(cpi);

  if (cpi->oxcf.fixed_q >= 0) {
    cpi->last_q[0] = cpi->oxcf.fixed_q;
    cpi->last_q[1] = cpi->oxcf.fixed_q;
    cpi->last_boosted_qindex = cpi->oxcf.fixed_q;
  }

  cpi->speed = cpi->oxcf.cpu_used;

  if (cpi->oxcf.lag_in_frames == 0) {
    // force to allowlag to 0 if lag_in_frames is 0;
    cpi->oxcf.allow_lag = 0;
  } else if (cpi->oxcf.lag_in_frames > MAX_LAG_BUFFERS) {
     // Limit on lag buffers as these are not currently dynamically allocated
    cpi->oxcf.lag_in_frames = MAX_LAG_BUFFERS;
  }

  // YX Temp
#if CONFIG_MULTIPLE_ARF
  vp9_zero(cpi->alt_ref_source);
#else
  cpi->alt_ref_source = NULL;
#endif
  cpi->is_src_frame_alt_ref = 0;

#if 0
  // Experimental RD Code
  cpi->frame_distortion = 0;
  cpi->last_frame_distortion = 0;
#endif

  set_tile_limits(cpi);
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

  init_config((VP9_PTR)cpi, oxcf);

  init_pick_mode_context(cpi);

  cm->current_video_frame   = 0;
  cpi->kf_overspend_bits            = 0;
  cpi->kf_bitrate_adjustment        = 0;
  cpi->frames_till_gf_update_due    = 0;
  cpi->gf_overspend_bits            = 0;
  cpi->non_gf_bitrate_adjustment    = 0;

  // Set reference frame sign bias for ALTREF frame to 1 (for now)
  cm->ref_frame_sign_bias[ALTREF_FRAME] = 1;

  cpi->baseline_gf_interval = DEFAULT_GF_INTERVAL;

  cpi->gold_is_last = 0;
  cpi->alt_is_last  = 0;
  cpi->gold_is_alt  = 0;

  // Spatial scalability
  cpi->number_spatial_layers = oxcf->ss_number_layers;

  // Create the encoder segmentation map and set all entries to 0
  CHECK_MEM_ERROR(cm, cpi->segmentation_map,
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

#ifdef ENTROPY_STATS
  if (cpi->pass != 1)
    init_context_counters();
#endif

#ifdef MODE_STATS
  init_tx_count_stats();
  init_switchable_interp_stats();
#endif

  /*Initialize the feed-forward activity masking.*/
  cpi->activity_avg = 90 << 12;

  cpi->frames_since_key = 8;  // Sensible default for first frame.
  cpi->key_frame_frequency = cpi->oxcf.key_freq;
  cpi->this_key_frame_forced = 0;
  cpi->next_key_frame_forced = 0;

  cpi->source_alt_ref_pending = 0;
  cpi->source_alt_ref_active = 0;
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
    cpi->total_sq_error = 0.0;
    cpi->total_sq_error2 = 0.0;
    cpi->total_y = 0.0;
    cpi->total_u = 0.0;
    cpi->total_v = 0.0;
    cpi->total = 0.0;
    cpi->totalp_y = 0.0;
    cpi->totalp_u = 0.0;
    cpi->totalp_v = 0.0;
    cpi->totalp = 0.0;
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

  cpi->frames_till_gf_update_due      = 0;
  cpi->key_frame_count              = 1;

  cpi->ni_av_qi                     = cpi->oxcf.worst_allowed_q;
  cpi->ni_tot_qi                    = 0;
  cpi->ni_frames                   = 0;
  cpi->tot_q = 0.0;
  cpi->avg_q = vp9_convert_qindex_to_q(cpi->oxcf.worst_allowed_q);
  cpi->total_byte_count             = 0;

  cpi->rate_correction_factor         = 1.0;
  cpi->key_frame_rate_correction_factor = 1.0;
  cpi->gf_rate_correction_factor  = 1.0;
  cpi->twopass.est_max_qcorrection_factor  = 1.0;

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

  for (i = 0; i < KEY_FRAME_CONTEXT; i++)
    cpi->prior_key_frame_distance[i] = (int)cpi->output_framerate;

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

  cpi->enable_encode_breakout = 1;

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

  // make sure frame 1 is okay
  cpi->error_bins[0] = cpi->common.MBs;

  /* vp9_init_quantizer() is first called here. Add check in
   * vp9_frame_init_quantizer() so that vp9_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * vp9_init_quantizer() for every frame.
   */
  vp9_init_quantizer(cpi);

  vp9_loop_filter_init(cm);

  cpi->common.error.setjmp = 0;

  vp9_zero(cpi->y_uv_mode_count);

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

#ifdef ENTROPY_STATS
    if (cpi->pass != 1) {
      print_context_counters();
      print_tree_update_probs();
      print_mode_context(cpi);
    }
#endif

#ifdef MODE_STATS
    if (cpi->pass != 1) {
      write_tx_count_stats();
      write_switchable_interp_stats();
    }
#endif

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
        YV12_BUFFER_CONFIG *lst_yv12 =
            &cpi->common.yv12_fb[cpi->common.ref_frame_map[cpi->lst_fb_idx]];
        double samples = 3.0 / 2 * cpi->count *
                         lst_yv12->y_width * lst_yv12->y_height;
        double total_psnr = vp9_mse2psnr(samples, 255.0, cpi->total_sq_error);
        double total_psnr2 = vp9_mse2psnr(samples, 255.0, cpi->total_sq_error2);
        double total_ssim = 100 * pow(cpi->summed_quality /
                                      cpi->summed_weights, 8.0);
        double total_ssimp = 100 * pow(cpi->summedp_quality /
                                       cpi->summedp_weights, 8.0);

        fprintf(f, "Bitrate\tAVGPsnr\tGLBPsnr\tAVPsnrP\tGLPsnrP\t"
                "VPXSSIM\tVPSSIMP\t  Time(ms)\n");
        fprintf(f, "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%8.0f\n",
                dr, cpi->total / cpi->count, total_psnr,
                cpi->totalp / cpi->count, total_psnr2, total_ssim, total_ssimp,
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

#ifdef ENTROPY_STATS
    {
      int i, j, k;
      FILE *fmode = fopen("vp9_modecontext.c", "w");

      fprintf(fmode, "\n#include \"vp9_entropymode.h\"\n\n");
      fprintf(fmode, "const unsigned int vp9_kf_default_bmode_counts ");
      fprintf(fmode, "[INTRA_MODES][INTRA_MODES]"
                     "[INTRA_MODES] =\n{\n");

      for (i = 0; i < INTRA_MODES; i++) {
        fprintf(fmode, "    { // Above Mode :  %d\n", i);

        for (j = 0; j < INTRA_MODES; j++) {
          fprintf(fmode, "        {");

          for (k = 0; k < INTRA_MODES; k++) {
            if (!intra_mode_stats[i][j][k])
              fprintf(fmode, " %5d, ", 1);
            else
              fprintf(fmode, " %5d, ", intra_mode_stats[i][j][k]);
          }

          fprintf(fmode, "}, // left_mode %d\n", j);
        }

        fprintf(fmode, "    },\n");
      }

      fprintf(fmode, "};\n");
      fclose(fmode);
    }
#endif


#if defined(SECTIONBITS_OUTPUT)

    if (0) {
      int i;
      FILE *f = fopen("tokenbits.stt", "a");

      for (i = 0; i < 28; i++)
        fprintf(f, "%8d", (int)(Sectionbits[i] / 256));

      fprintf(f, "\n");
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


static uint64_t calc_plane_error(uint8_t *orig, int orig_stride,
                                 uint8_t *recon, int recon_stride,
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
      uint8_t *border_orig = orig;
      uint8_t *border_recon = recon;

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


static void generate_psnr_packet(VP9_COMP *cpi) {
  YV12_BUFFER_CONFIG      *orig = cpi->Source;
  YV12_BUFFER_CONFIG      *recon = cpi->common.frame_to_show;
  struct vpx_codec_cx_pkt  pkt;
  uint64_t                 sse;
  int                      i;
  unsigned int             width = orig->y_crop_width;
  unsigned int             height = orig->y_crop_height;

  pkt.kind = VPX_CODEC_PSNR_PKT;
  sse = calc_plane_error(orig->y_buffer, orig->y_stride,
                         recon->y_buffer, recon->y_stride,
                         width, height);
  pkt.data.psnr.sse[0] = sse;
  pkt.data.psnr.sse[1] = sse;
  pkt.data.psnr.samples[0] = width * height;
  pkt.data.psnr.samples[1] = width * height;

  width = orig->uv_crop_width;
  height = orig->uv_crop_height;

  sse = calc_plane_error(orig->u_buffer, orig->uv_stride,
                         recon->u_buffer, recon->uv_stride,
                         width, height);
  pkt.data.psnr.sse[0] += sse;
  pkt.data.psnr.sse[2] = sse;
  pkt.data.psnr.samples[0] += width * height;
  pkt.data.psnr.samples[2] = width * height;

  sse = calc_plane_error(orig->v_buffer, orig->uv_stride,
                         recon->v_buffer, recon->uv_stride,
                         width, height);
  pkt.data.psnr.sse[0] += sse;
  pkt.data.psnr.sse[3] = sse;
  pkt.data.psnr.samples[0] += width * height;
  pkt.data.psnr.samples[3] = width * height;

  for (i = 0; i < 4; i++)
    pkt.data.psnr.psnr[i] = vp9_mse2psnr(pkt.data.psnr.samples[i], 255.0,
                                         (double)pkt.data.psnr.sse[i]);

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

  cpi->refresh_golden_frame = 0;
  cpi->refresh_alt_ref_frame = 0;
  cpi->refresh_last_frame   = 0;

  if (ref_frame_flags & VP9_LAST_FLAG)
    cpi->refresh_last_frame = 1;

  if (ref_frame_flags & VP9_GOLD_FLAG)
    cpi->refresh_golden_frame = 1;

  if (ref_frame_flags & VP9_ALT_FLAG)
    cpi->refresh_alt_ref_frame = 1;

  return 0;
}

int vp9_copy_reference_enc(VP9_PTR ptr, VP9_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);
  VP9_COMMON *cm = &cpi->common;
  int ref_fb_idx;

  if (ref_frame_flag == VP9_LAST_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->lst_fb_idx];
  else if (ref_frame_flag == VP9_GOLD_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->gld_fb_idx];
  else if (ref_frame_flag == VP9_ALT_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->alt_fb_idx];
  else
    return -1;

  vp8_yv12_copy_frame(&cm->yv12_fb[ref_fb_idx], sd);

  return 0;
}

int vp9_get_reference_enc(VP9_PTR ptr, int index, YV12_BUFFER_CONFIG **fb) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);
  VP9_COMMON *cm = &cpi->common;

  if (index < 0 || index >= NUM_REF_FRAMES)
    return -1;

  *fb = &cm->yv12_fb[cm->ref_frame_map[index]];
  return 0;
}

int vp9_set_reference_enc(VP9_PTR ptr, VP9_REFFRAME ref_frame_flag,
                          YV12_BUFFER_CONFIG *sd) {
  VP9_COMP *cpi = (VP9_COMP *)(ptr);
  VP9_COMMON *cm = &cpi->common;

  int ref_fb_idx;

  if (ref_frame_flag == VP9_LAST_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->lst_fb_idx];
  else if (ref_frame_flag == VP9_GOLD_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->gld_fb_idx];
  else if (ref_frame_flag == VP9_ALT_FLAG)
    ref_fb_idx = cm->ref_frame_map[cpi->alt_fb_idx];
  else
    return -1;

  vp8_yv12_copy_frame(sd, &cm->yv12_fb[ref_fb_idx]);

  return 0;
}
int vp9_update_entropy(VP9_PTR comp, int update) {
  ((VP9_COMP *)comp)->common.refresh_frame_context = update;
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
        const int factor = i == 0 ? 1 : 2;
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


static void update_alt_ref_frame_stats(VP9_COMP *cpi) {
  // this frame refreshes means next frames don't unless specified by user
  cpi->frames_since_golden = 0;

#if CONFIG_MULTIPLE_ARF
  if (!cpi->multi_arf_enabled)
#endif
    // Clear the alternate reference update pending flag.
    cpi->source_alt_ref_pending = 0;

  // Set the alternate reference frame active flag
  cpi->source_alt_ref_active = 1;
}
static void update_golden_frame_stats(VP9_COMP *cpi) {
  // Update the Golden frame usage counts.
  if (cpi->refresh_golden_frame) {
    // this frame refreshes means next frames don't unless specified by user
    cpi->refresh_golden_frame = 0;
    cpi->frames_since_golden = 0;

    // ******** Fixed Q test code only ************
    // If we are going to use the ALT reference for the next group of frames
    // set a flag to say so.
    if (cpi->oxcf.fixed_q >= 0 &&
        cpi->oxcf.play_alternate && !cpi->refresh_alt_ref_frame) {
      cpi->source_alt_ref_pending = 1;
      cpi->frames_till_gf_update_due = cpi->baseline_gf_interval;

      // TODO(ivan): For SVC encoder, GF automatic update is disabled by using
      // a large GF_interval.
      if (cpi->use_svc) {
        cpi->frames_till_gf_update_due = INT_MAX;
      }
    }

    if (!cpi->source_alt_ref_pending)
      cpi->source_alt_ref_active = 0;

    // Decrement count down till next gf
    if (cpi->frames_till_gf_update_due > 0)
      cpi->frames_till_gf_update_due--;

  } else if (!cpi->refresh_alt_ref_frame) {
    // Decrement count down till next gf
    if (cpi->frames_till_gf_update_due > 0)
      cpi->frames_till_gf_update_due--;

    if (cpi->frames_till_alt_ref_frame)
      cpi->frames_till_alt_ref_frame--;

    cpi->frames_since_golden++;
  }
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

static void Pass1Encode(VP9_COMP *cpi, unsigned long *size, unsigned char *dest,
                        unsigned int *frame_flags) {
  (void) size;
  (void) dest;
  (void) frame_flags;

  vp9_set_quantizer(cpi, find_fp_qindex());
  vp9_first_pass(cpi);
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
static int recode_loop_test(VP9_COMP *cpi,
                            int high_limit, int low_limit,
                            int q, int maxq, int minq) {
  int force_recode = 0;
  VP9_COMMON *cm = &cpi->common;

  // Is frame recode allowed at all
  // Yes if either recode mode 1 is selected or mode two is selected
  // and the frame is a key frame. golden frame or alt_ref_frame
  if ((cpi->sf.recode_loop == 1) ||
      ((cpi->sf.recode_loop == 2) &&
       ((cm->frame_type == KEY_FRAME) ||
        cpi->refresh_golden_frame ||
        cpi->refresh_alt_ref_frame))) {
    // General over and under shoot tests
    if (((cpi->projected_frame_size > high_limit) && (q < maxq)) ||
        ((cpi->projected_frame_size < low_limit) && (q > minq))) {
      force_recode = 1;
    } else if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY) {
      // Deal with frame undershoot and whether or not we are
      // below the automatically set cq level.
      if (q > cpi->cq_target_quality &&
          cpi->projected_frame_size < ((cpi->this_frame_target * 7) >> 3)) {
        force_recode = 1;
      } else if (q > cpi->oxcf.cq_level &&
                 cpi->projected_frame_size < cpi->min_frame_bandwidth &&
                 cpi->active_best_quality > cpi->oxcf.cq_level) {
        // Severe undershoot and between auto and user cq level
        force_recode = 1;
        cpi->active_best_quality = cpi->oxcf.cq_level;
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
    ref_cnt_fb(cm->fb_idx_ref_cnt,
               &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    ref_cnt_fb(cm->fb_idx_ref_cnt,
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

    ref_cnt_fb(cm->fb_idx_ref_cnt,
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
      ref_cnt_fb(cm->fb_idx_ref_cnt,
                 &cm->ref_frame_map[arf_idx], cm->new_fb_idx);
    }

    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(cm->fb_idx_ref_cnt,
                 &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    }
  }

  if (cpi->refresh_last_frame) {
    ref_cnt_fb(cm->fb_idx_ref_cnt,
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
  int i;
  int refs[ALLOWED_REFS_PER_FRAME] = {cpi->lst_fb_idx, cpi->gld_fb_idx,
                                      cpi->alt_fb_idx};

  for (i = 0; i < 3; i++) {
    YV12_BUFFER_CONFIG *ref = &cm->yv12_fb[cm->ref_frame_map[refs[i]]];

    if (ref->y_crop_width != cm->width ||
        ref->y_crop_height != cm->height) {
      int new_fb = get_free_fb(cm);

      vp9_realloc_frame_buffer(&cm->yv12_fb[new_fb],
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9BORDERINPIXELS);
      scale_and_extend_frame(ref, &cm->yv12_fb[new_fb]);
      cpi->scaled_ref_idx[i] = new_fb;
    } else {
      cpi->scaled_ref_idx[i] = cm->ref_frame_map[refs[i]];
      cm->fb_idx_ref_cnt[cm->ref_frame_map[refs[i]]]++;
    }
  }
}

static void release_scaled_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int i;

  for (i = 0; i < 3; i++)
    cm->fb_idx_ref_cnt[cpi->scaled_ref_idx[i]]--;
}

static void full_to_model_count(unsigned int *model_count,
                                unsigned int *full_count) {
  int n;
  model_count[ZERO_TOKEN] = full_count[ZERO_TOKEN];
  model_count[ONE_TOKEN] = full_count[ONE_TOKEN];
  model_count[TWO_TOKEN] = full_count[TWO_TOKEN];
  for (n = THREE_TOKEN; n < DCT_EOB_TOKEN; ++n)
    model_count[TWO_TOKEN] += full_count[n];
  model_count[DCT_EOB_MODEL_TOKEN] = full_count[DCT_EOB_TOKEN];
}

static void full_to_model_counts(
    vp9_coeff_count_model *model_count, vp9_coeff_count *full_count) {
  int i, j, k, l;
  for (i = 0; i < BLOCK_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k)
        for (l = 0; l < PREV_COEF_CONTEXTS; ++l) {
          if (l >= 3 && k == 0)
            continue;
          full_to_model_count(model_count[i][j][k][l], full_count[i][j][k][l]);
        }
}

#if 0 && CONFIG_INTERNAL_STATS
static void output_frame_level_debug_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  FILE *const f = fopen("tmp.stt", cm->current_video_frame ? "a" : "w");
  int recon_err;

  vp9_clear_system_state();  // __asm emms;

  recon_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));

  if (cpi->twopass.total_left_stats.coded_error != 0.0)
    fprintf(f, "%10d %10d %10d %10d %10d %10d %10d %10d %10d"
        "%7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f"
        "%6d %6d %5d %5d %5d %8.2f %10d %10.3f"
        "%10.3f %8d %10d %10d %10d\n",
        cpi->common.current_video_frame, cpi->this_frame_target,
        cpi->projected_frame_size, 0,
        (cpi->projected_frame_size - cpi->this_frame_target),
        (int)cpi->total_target_vs_actual,
        (int)(cpi->oxcf.starting_buffer_level - cpi->bits_off_target),
        (int)cpi->total_actual_bits, cm->base_qindex,
        vp9_convert_qindex_to_q(cm->base_qindex),
        (double)vp9_dc_quant(cm->base_qindex, 0) / 4.0,
        vp9_convert_qindex_to_q(cpi->active_best_quality),
        vp9_convert_qindex_to_q(cpi->active_worst_quality), cpi->avg_q,
        vp9_convert_qindex_to_q(cpi->ni_av_qi),
        vp9_convert_qindex_to_q(cpi->cq_target_quality),
        cpi->refresh_last_frame, cpi->refresh_golden_frame,
        cpi->refresh_alt_ref_frame, cm->frame_type, cpi->gfu_boost,
        cpi->twopass.est_max_qcorrection_factor, (int)cpi->twopass.bits_left,
        cpi->twopass.total_left_stats.coded_error,
        (double)cpi->twopass.bits_left /
            (1 + cpi->twopass.total_left_stats.coded_error),
        cpi->tot_recode_hits, recon_err, cpi->kf_boost, cpi->kf_zeromotion_pct);

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

static int pick_q_and_adjust_q_bounds(VP9_COMP *cpi,
                                      int * bottom_index, int * top_index) {
  // Set an active best quality and if necessary active worst quality
  int q = cpi->active_worst_quality;
  VP9_COMMON *const cm = &cpi->common;

  if (frame_is_intra_only(cm)) {
#if !CONFIG_MULTIPLE_ARF
    // Handle the special case for key frames forced when we have75 reached
    // the maximum key frame interval. Here force the Q to a range
    // based on the ambient Q to reduce the risk of popping.
    if (cpi->this_key_frame_forced) {
      int delta_qindex;
      int qindex = cpi->last_boosted_qindex;
      double last_boosted_q = vp9_convert_qindex_to_q(qindex);

      delta_qindex = vp9_compute_qdelta(cpi, last_boosted_q,
                                        (last_boosted_q * 0.75));

      cpi->active_best_quality = MAX(qindex + delta_qindex,
                                     cpi->best_quality);
    } else {
      int high = 5000;
      int low = 400;
      double q_adj_factor = 1.0;
      double q_val;

      // Baseline value derived from cpi->active_worst_quality and kf boost
      cpi->active_best_quality = get_active_quality(q, cpi->kf_boost,
                                                    low, high,
                                                    kf_low_motion_minq,
                                                    kf_high_motion_minq);

      // Allow somewhat lower kf minq with small image formats.
      if ((cm->width * cm->height) <= (352 * 288)) {
        q_adj_factor -= 0.25;
      }

      // Make a further adjustment based on the kf zero motion measure.
      q_adj_factor += 0.05 - (0.001 * (double)cpi->kf_zeromotion_pct);

      // Convert the adjustment factor to a qindex delta
      // on active_best_quality.
      q_val = vp9_convert_qindex_to_q(cpi->active_best_quality);
      cpi->active_best_quality +=
          vp9_compute_qdelta(cpi, q_val, (q_val * q_adj_factor));
    }
#else
    double current_q;
    // Force the KF quantizer to be 30% of the active_worst_quality.
    current_q = vp9_convert_qindex_to_q(cpi->active_worst_quality);
    cpi->active_best_quality = cpi->active_worst_quality
        + vp9_compute_qdelta(cpi, current_q, current_q * 0.3);
#endif
  } else if (!cpi->is_src_frame_alt_ref &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    int high = 2000;
    int low = 400;

    // Use the lower of cpi->active_worst_quality and recent
    // average Q as basis for GF/ARF best Q limit unless last frame was
    // a key frame.
    if (cpi->frames_since_key > 1 &&
        cpi->avg_frame_qindex < cpi->active_worst_quality) {
      q = cpi->avg_frame_qindex;
    }
    // For constrained quality dont allow Q less than the cq level
    if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY) {
      if (q < cpi->cq_target_quality)
        q = cpi->cq_target_quality;
      if (cpi->frames_since_key > 1) {
        cpi->active_best_quality = get_active_quality(q, cpi->gfu_boost,
                                                      low, high,
                                                      afq_low_motion_minq,
                                                      afq_high_motion_minq);
      } else {
        cpi->active_best_quality = get_active_quality(q, cpi->gfu_boost,
                                                      low, high,
                                                      gf_low_motion_minq,
                                                      gf_high_motion_minq);
      }
      // Constrained quality use slightly lower active best.
      cpi->active_best_quality = cpi->active_best_quality * 15 / 16;

    } else if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
      if (!cpi->refresh_alt_ref_frame) {
        cpi->active_best_quality = cpi->cq_target_quality;
      } else {
        if (cpi->frames_since_key > 1) {
          cpi->active_best_quality = get_active_quality(q, cpi->gfu_boost,
                                                        low, high,
                                                        afq_low_motion_minq,
                                                        afq_high_motion_minq);
        } else {
          cpi->active_best_quality = get_active_quality(q, cpi->gfu_boost,
                                                        low, high,
                                                        gf_low_motion_minq,
                                                        gf_high_motion_minq);
        }
      }
    } else {
        cpi->active_best_quality = get_active_quality(q, cpi->gfu_boost,
                                                      low, high,
                                                      gf_low_motion_minq,
                                                      gf_high_motion_minq);
    }
  } else {
    if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
      cpi->active_best_quality = cpi->cq_target_quality;
    } else {
      cpi->active_best_quality = inter_minq[q];
      // 1-pass: for now, use the average Q for the active_best, if its lower
      // than active_worst.
      if (cpi->pass == 0 && (cpi->avg_frame_qindex < q))
        cpi->active_best_quality = inter_minq[cpi->avg_frame_qindex];

      // For the constrained quality mode we don't want
      // q to fall below the cq level.
      if ((cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY) &&
          (cpi->active_best_quality < cpi->cq_target_quality)) {
        // If we are strongly undershooting the target rate in the last
        // frames then use the user passed in cq value not the auto
        // cq value.
        if (cpi->rolling_actual_bits < cpi->min_frame_bandwidth)
          cpi->active_best_quality = cpi->oxcf.cq_level;
        else
          cpi->active_best_quality = cpi->cq_target_quality;
      }
    }
  }

  // Clip the active best and worst quality values to limits
  if (cpi->active_worst_quality > cpi->worst_quality)
    cpi->active_worst_quality = cpi->worst_quality;

  if (cpi->active_best_quality < cpi->best_quality)
    cpi->active_best_quality = cpi->best_quality;

  if (cpi->active_best_quality > cpi->worst_quality)
    cpi->active_best_quality = cpi->worst_quality;

  if (cpi->active_worst_quality < cpi->active_best_quality)
    cpi->active_worst_quality = cpi->active_best_quality;

  // Limit Q range for the adaptive loop.
  if (cm->frame_type == KEY_FRAME && !cpi->this_key_frame_forced) {
    *top_index =
      (cpi->active_worst_quality + cpi->active_best_quality * 3) / 4;
    // If this is the first (key) frame in 1-pass, active best is the user
    // best-allowed, and leave the top_index to active_worst.
    if (cpi->pass == 0 && cpi->common.current_video_frame == 0) {
      cpi->active_best_quality = cpi->oxcf.best_allowed_q;
      *top_index = cpi->oxcf.worst_allowed_q;
    }
  } else if (!cpi->is_src_frame_alt_ref &&
             (cpi->oxcf.end_usage != USAGE_STREAM_FROM_SERVER) &&
             (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame)) {
    *top_index =
      (cpi->active_worst_quality + cpi->active_best_quality) / 2;
  } else {
    *top_index = cpi->active_worst_quality;
  }
  *bottom_index = cpi->active_best_quality;

  if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
    q = cpi->active_best_quality;
  // Special case code to try and match quality with forced key frames
  } else if ((cm->frame_type == KEY_FRAME) && cpi->this_key_frame_forced) {
    q = cpi->last_boosted_qindex;
  } else {
    // Determine initial Q to try.
    if (cpi->pass == 0) {
      // 1-pass: for now, use per-frame-bw for target size of frame, scaled
      // by |x| for key frame.
      int scale = (cm->frame_type == KEY_FRAME) ? 5 : 1;
      q = vp9_regulate_q(cpi, scale * cpi->av_per_frame_bandwidth);
    } else {
      q = vp9_regulate_q(cpi, cpi->this_frame_target);
    }
    if (q > *top_index)
      q = *top_index;
  }

  return q;
}
static void encode_frame_to_data_rate(VP9_COMP *cpi,
                                      unsigned long *size,
                                      unsigned char *dest,
                                      unsigned int *frame_flags) {
  VP9_COMMON *const cm = &cpi->common;
  TX_SIZE t;
  int q;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;

  int loop = 0;
  int loop_count;

  int q_low;
  int q_high;

  int top_index;
  int bottom_index;
  int active_worst_qchanged = 0;

  int overshoot_seen = 0;
  int undershoot_seen = 0;

  SPEED_FEATURES *const sf = &cpi->sf;
  unsigned int max_mv_def = MIN(cpi->common.width, cpi->common.height);
  struct segmentation *const seg = &cm->seg;

  /* Scale the source buffer, if required. */
  if (cm->mi_cols * 8 != cpi->un_scaled_source->y_width ||
      cm->mi_rows * 8 != cpi->un_scaled_source->y_height) {
    scale_and_extend_frame(cpi->un_scaled_source, &cpi->scaled_source);
    cpi->Source = &cpi->scaled_source;
  } else {
    cpi->Source = cpi->un_scaled_source;
  }
  scale_references(cpi);

  // Clear down mmx registers to allow floating point in what follows.
  vp9_clear_system_state();

  // For an alt ref frame in 2 pass we skip the call to the second
  // pass function that sets the target bandwidth so we must set it here.
  if (cpi->refresh_alt_ref_frame) {
    // Set a per frame bit target for the alt ref frame.
    cpi->per_frame_bandwidth = cpi->twopass.gf_bits;
    // Set a per second target bitrate.
    cpi->target_bandwidth = (int)(cpi->twopass.gf_bits * cpi->output_framerate);
  }

  // Clear zbin over-quant value and mode boost values.
  cpi->zbin_mode_boost = 0;

  // Enable or disable mode based tweaking of the zbin.
  // For 2 pass only used where GF/ARF prediction quality
  // is above a threshold.
  cpi->zbin_mode_boost = 0;
  cpi->zbin_mode_boost_enabled = 0;

  // Current default encoder behavior for the altref sign bias.
  cpi->common.ref_frame_sign_bias[ALTREF_FRAME] = cpi->source_alt_ref_active;

  // Check to see if a key frame is signaled.
  // For two pass with auto key frame enabled cm->frame_type may already be
  // set, but not for one pass.
  if ((cm->current_video_frame == 0) ||
      (cm->frame_flags & FRAMEFLAGS_KEY) ||
      (cpi->oxcf.auto_key && (cpi->frames_since_key %
                              cpi->key_frame_frequency == 0))) {
    // Set frame type to key frame for the force key frame, if we exceed the
    // maximum distance in an automatic keyframe selection or for the first
    // frame.
    cm->frame_type = KEY_FRAME;
  }

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  // Initialize cpi->mv_step_param to default based on max resolution.
  cpi->mv_step_param = vp9_init_search_range(cpi, max_mv_def);
  // Initialize cpi->max_mv_magnitude and cpi->mv_step_param if appropriate.
  if (sf->auto_mv_step_size) {
    if (frame_is_intra_only(&cpi->common)) {
      // Initialize max_mv_magnitude for use in the first INTER frame
      // after a key/intra-only frame.
      cpi->max_mv_magnitude = max_mv_def;
    } else {
      if (cm->show_frame)
        // Allow mv_steps to correspond to twice the max mv magnitude found
        // in the previous frame, capped by the default max_mv_magnitude based
        // on resolution.
        cpi->mv_step_param = vp9_init_search_range(
            cpi, MIN(max_mv_def, 2 * cpi->max_mv_magnitude));
      cpi->max_mv_magnitude = 0;
    }
  }

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm)) {
    vp9_setup_key_frame(cpi);
    // Reset the loop filter deltas and segmentation map.
    setup_features(cm);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->source_alt_ref_active = 0;

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
  if ((cpi->pass == 2) && (cpi->sf.static_segmentation)) {
    configure_static_seg_features(cpi);
  }

  // Decide how big to make the frame.
  vp9_pick_frame_size(cpi);

  vp9_clear_system_state();

  q = pick_q_and_adjust_q_bounds(cpi, &bottom_index, &top_index);

  q_high = top_index;
  q_low  = bottom_index;

  vp9_compute_frame_size_bounds(cpi, &frame_under_shoot_limit,
                                &frame_over_shoot_limit);

#if CONFIG_MULTIPLE_ARF
  // Force the quantizer determined by the coding order pattern.
  if (cpi->multi_arf_enabled && (cm->frame_type != KEY_FRAME) &&
      cpi->oxcf.end_usage != USAGE_CONSTANT_QUALITY) {
    double new_q;
    double current_q = vp9_convert_qindex_to_q(cpi->active_worst_quality);
    int level = cpi->this_frame_weight;
    assert(level >= 0);

    // Set quantizer steps at 10% increments.
    new_q = current_q * (1.0 - (0.2 * (cpi->max_arf_level - level)));
    q = cpi->active_worst_quality + vp9_compute_qdelta(cpi, current_q, new_q);

    bottom_index = q;
    top_index    = q;
    q_low  = q;
    q_high = q;

    printf("frame:%d q:%d\n", cm->current_video_frame, q);
  }
#endif

  loop_count = 0;
  vp9_zero(cpi->rd_tx_select_threshes);

  if (!frame_is_intra_only(cm)) {
    cm->mcomp_filter_type = DEFAULT_INTERP_FILTER;
    /* TODO: Decide this more intelligently */
    cm->allow_high_precision_mv = q < HIGH_PRECISION_MV_QTHRESH;
    set_mvcost(cpi);
  }

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

    if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
        vp9_vaq_frame_setup(cpi);
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
    vp9_save_coding_context(cpi);
    cpi->dummy_packing = 1;
    vp9_pack_bitstream(cpi, dest, size);
    cpi->projected_frame_size = (*size) << 3;
    vp9_restore_coding_context(cpi);

    if (frame_over_shoot_limit == 0)
      frame_over_shoot_limit = 1;
    active_worst_qchanged = 0;

    if (cpi->oxcf.end_usage == USAGE_CONSTANT_QUALITY) {
      loop = 0;
    } else {
      // Special case handling for forced key frames
      if ((cm->frame_type == KEY_FRAME) && cpi->this_key_frame_forced) {
        int last_q = q;
        int kf_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));

        int high_err_target = cpi->ambient_err;
        int low_err_target = cpi->ambient_err >> 1;

        // Prevent possible divide by zero error below for perfect KF
        kf_err += !kf_err;

        // The key frame is not good enough or we can afford
        // to make it better without undue risk of popping.
        if ((kf_err > high_err_target &&
             cpi->projected_frame_size <= frame_over_shoot_limit) ||
            (kf_err > low_err_target &&
             cpi->projected_frame_size <= frame_under_shoot_limit)) {
          // Lower q_high
          q_high = q > q_low ? q - 1 : q_low;

          // Adjust Q
          q = (q * high_err_target) / kf_err;
          q = MIN(q, (q_high + q_low) >> 1);
        } else if (kf_err < low_err_target &&
                   cpi->projected_frame_size >= frame_under_shoot_limit) {
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
          q, top_index, bottom_index)) {
        // Is the projected frame size out of range and are we allowed
        // to attempt to recode.
        int last_q = q;
        int retries = 0;

        // Frame size out of permitted range:
        // Update correction factor & compute new Q to try...

        // Frame is too large
        if (cpi->projected_frame_size > cpi->this_frame_target) {
          // Raise Qlow as to at least the current value
          q_low = q < q_high ? q + 1 : q_high;

          if (undershoot_seen || loop_count > 1) {
            // Update rate_correction_factor unless
            // cpi->active_worst_quality has changed.
            if (!active_worst_qchanged)
              vp9_update_rate_correction_factors(cpi, 1);

            q = (q_high + q_low + 1) / 2;
          } else {
            // Update rate_correction_factor unless
            // cpi->active_worst_quality has changed.
            if (!active_worst_qchanged)
              vp9_update_rate_correction_factors(cpi, 0);

            q = vp9_regulate_q(cpi, cpi->this_frame_target);

            while (q < q_low && retries < 10) {
              vp9_update_rate_correction_factors(cpi, 0);
              q = vp9_regulate_q(cpi, cpi->this_frame_target);
              retries++;
            }
          }

          overshoot_seen = 1;
        } else {
          // Frame is too small
          q_high = q > q_low ? q - 1 : q_low;

          if (overshoot_seen || loop_count > 1) {
            // Update rate_correction_factor unless
            // cpi->active_worst_quality has changed.
            if (!active_worst_qchanged)
              vp9_update_rate_correction_factors(cpi, 1);

            q = (q_high + q_low) / 2;
          } else {
            // Update rate_correction_factor unless
            // cpi->active_worst_quality has changed.
            if (!active_worst_qchanged)
              vp9_update_rate_correction_factors(cpi, 0);

            q = vp9_regulate_q(cpi, cpi->this_frame_target);

            // Special case reset for qlow for constrained quality.
            // This should only trigger where there is very substantial
            // undershoot on a frame and the auto cq level is above
            // the user passsed in value.
            if (cpi->oxcf.end_usage == USAGE_CONSTRAINED_QUALITY && q < q_low) {
              q_low = q;
            }

            while (q > q_high && retries < 10) {
              vp9_update_rate_correction_factors(cpi, 0);
              q = vp9_regulate_q(cpi, cpi->this_frame_target);
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

    if (cpi->is_src_frame_alt_ref)
      loop = 0;

    if (loop) {
      loop_count++;

#if CONFIG_INTERNAL_STATS
      cpi->tot_recode_hits++;
#endif
    }
  } while (loop);

  // Special case code to reduce pulsing when key frames are forced at a
  // fixed interval. Note the reconstruction error if it is the frame before
  // the force key frame
  if (cpi->next_key_frame_forced && (cpi->twopass.frames_to_key == 0)) {
    cpi->ambient_err = vp9_calc_ss_err(cpi->Source, get_frame_new_buffer(cm));
  }

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
    full_to_model_counts(cpi->common.counts.coef[t],
                         cpi->coef_counts[t]);
  if (!cpi->common.error_resilient_mode &&
      !cpi->common.frame_parallel_decoding_mode) {
    vp9_adapt_coef_probs(&cpi->common);
  }

  if (!frame_is_intra_only(&cpi->common)) {
    FRAME_COUNTS *counts = &cpi->common.counts;

    vp9_copy(counts->y_mode, cpi->y_mode_count);
    vp9_copy(counts->uv_mode, cpi->y_uv_mode_count);
    vp9_copy(counts->partition, cpi->partition_count);
    vp9_copy(counts->intra_inter, cpi->intra_inter_count);
    vp9_copy(counts->comp_inter, cpi->comp_inter_count);
    vp9_copy(counts->single_ref, cpi->single_ref_count);
    vp9_copy(counts->comp_ref, cpi->comp_ref_count);
    counts->mv = cpi->NMVcount;
    if (!cpi->common.error_resilient_mode &&
        !cpi->common.frame_parallel_decoding_mode) {
      vp9_adapt_mode_probs(&cpi->common);
      vp9_adapt_mv_probs(&cpi->common, cpi->common.allow_high_precision_mv);
    }
  }

#ifdef ENTROPY_STATS
  vp9_update_mode_context_stats(cpi);
#endif

  /* Move storing frame_type out of the above loop since it is also
   * needed in motion search besides loopfilter */
  cm->last_frame_type = cm->frame_type;

  // Update rate control heuristics
  cpi->total_byte_count += (*size);
  cpi->projected_frame_size = (*size) << 3;

  // Post encode loop adjustment of Q prediction.
  if (!active_worst_qchanged)
    vp9_update_rate_correction_factors(cpi, (cpi->sf.recode_loop ||
        cpi->oxcf.end_usage == USAGE_STREAM_FROM_SERVER) ? 2 : 0);


  cpi->last_q[cm->frame_type] = cm->base_qindex;

  // Keep record of last boosted (KF/KF/ARF) Q value.
  // If the current frame is coded at a lower Q then we also update it.
  // If all mbs in this group are skipped only update if the Q value is
  // better than that already stored.
  // This is used to help set quality in forced key frames to reduce popping
  if ((cm->base_qindex < cpi->last_boosted_qindex) ||
      ((cpi->static_mb_pct < 100) &&
       ((cm->frame_type == KEY_FRAME) ||
        cpi->refresh_alt_ref_frame ||
        (cpi->refresh_golden_frame && !cpi->is_src_frame_alt_ref)))) {
    cpi->last_boosted_qindex = cm->base_qindex;
  }

  if (cm->frame_type == KEY_FRAME) {
    vp9_adjust_key_frame_context(cpi);
  }

  // Keep a record of ambient average Q.
  if (cm->frame_type != KEY_FRAME)
    cpi->avg_frame_qindex = (2 + 3 * cpi->avg_frame_qindex +
                            cm->base_qindex) >> 2;

  // Keep a record from which we can calculate the average Q excluding GF
  // updates and key frames.
  if (cm->frame_type != KEY_FRAME &&
      !cpi->refresh_golden_frame &&
      !cpi->refresh_alt_ref_frame) {
    cpi->ni_frames++;
    cpi->tot_q += vp9_convert_qindex_to_q(q);
    cpi->avg_q = cpi->tot_q / (double)cpi->ni_frames;

    // Calculate the average Q for normal inter frames (not key or GFU frames).
    cpi->ni_tot_qi += q;
    cpi->ni_av_qi = cpi->ni_tot_qi / cpi->ni_frames;
  }

  // Update the buffer level variable.
  // Non-viewable frames are a special case and are treated as pure overhead.
  if (!cm->show_frame)
    cpi->bits_off_target -= cpi->projected_frame_size;
  else
    cpi->bits_off_target += cpi->av_per_frame_bandwidth -
                            cpi->projected_frame_size;

  // Clip the buffer level at the maximum buffer size
  if (cpi->bits_off_target > cpi->oxcf.maximum_buffer_size)
    cpi->bits_off_target = cpi->oxcf.maximum_buffer_size;

  // Rolling monitors of whether we are over or underspending used to help
  // regulate min and Max Q in two pass.
  if (cm->frame_type != KEY_FRAME) {
    cpi->rolling_target_bits =
      ((cpi->rolling_target_bits * 3) + cpi->this_frame_target + 2) / 4;
    cpi->rolling_actual_bits =
      ((cpi->rolling_actual_bits * 3) + cpi->projected_frame_size + 2) / 4;
    cpi->long_rolling_target_bits =
      ((cpi->long_rolling_target_bits * 31) + cpi->this_frame_target + 16) / 32;
    cpi->long_rolling_actual_bits =
      ((cpi->long_rolling_actual_bits * 31) +
       cpi->projected_frame_size + 16) / 32;
  }

  // Actual bits spent
  cpi->total_actual_bits += cpi->projected_frame_size;

  // Debug stats
  cpi->total_target_vs_actual += (cpi->this_frame_target -
                                  cpi->projected_frame_size);

  cpi->buffer_level = cpi->bits_off_target;

#ifndef DISABLE_RC_LONG_TERM_MEM
  // Update bits left to the kf and gf groups to account for overshoot or
  // undershoot on these frames
  if (cm->frame_type == KEY_FRAME) {
    cpi->twopass.kf_group_bits += cpi->this_frame_target -
                                  cpi->projected_frame_size;

    cpi->twopass.kf_group_bits = MAX(cpi->twopass.kf_group_bits, 0);
  } else if (cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame) {
    cpi->twopass.gf_group_bits += cpi->this_frame_target -
                                  cpi->projected_frame_size;

    cpi->twopass.gf_group_bits = MAX(cpi->twopass.gf_group_bits, 0);
  }
#endif

#if 0
  output_frame_level_debug_stats(cpi);
#endif
  if (cpi->refresh_golden_frame == 1)
    cm->frame_flags = cm->frame_flags | FRAMEFLAGS_GOLDEN;
  else
    cm->frame_flags = cm->frame_flags&~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cm->frame_flags = cm->frame_flags | FRAMEFLAGS_ALTREF;
  else
    cm->frame_flags = cm->frame_flags&~FRAMEFLAGS_ALTREF;


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

  if (cpi->oxcf.play_alternate && cpi->refresh_alt_ref_frame
      && (cm->frame_type != KEY_FRAME))
    // Update the alternate reference frame stats as appropriate.
    update_alt_ref_frame_stats(cpi);
  else
    // Update the Golden frame stats as appropriate.
    update_golden_frame_stats(cpi);

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

    // As this frame is a key frame the next defaults to an inter frame.
    cm->frame_type = INTER_FRAME;
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
    ++cpi->frames_since_key;
  }
  // restore prev_mi
  cm->prev_mi = cm->prev_mip + cm->mode_info_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mode_info_stride + 1;
}

static void Pass2Encode(VP9_COMP *cpi, unsigned long *size,
                        unsigned char *dest, unsigned int *frame_flags) {
  cpi->enable_encode_breakout = 1;

  if (!cpi->refresh_alt_ref_frame)
    vp9_second_pass(cpi);

  encode_frame_to_data_rate(cpi, size, dest, frame_flags);
  // vp9_print_modes_and_motion_vectors(&cpi->common, "encode.stt");
#ifdef DISABLE_RC_LONG_TERM_MEM
  cpi->twopass.bits_left -=  cpi->this_frame_target;
#else
  cpi->twopass.bits_left -= 8 * *size;
#endif

  if (!cpi->refresh_alt_ref_frame) {
    double lower_bounds_min_rate = FRAME_OVERHEAD_BITS * cpi->oxcf.framerate;
    double two_pass_min_rate = (double)(cpi->oxcf.target_bandwidth
                                        * cpi->oxcf.two_pass_vbrmin_section
                                        / 100);

    if (two_pass_min_rate < lower_bounds_min_rate)
      two_pass_min_rate = lower_bounds_min_rate;

    cpi->twopass.bits_left += (int64_t)(two_pass_min_rate
                              / cpi->oxcf.framerate);
  }
}

static void check_initial_width(VP9_COMP *cpi, YV12_BUFFER_CONFIG *sd) {
  VP9_COMMON            *cm = &cpi->common;
  if (!cpi->initial_width) {
    // TODO(jkoleszar): Support 1/4 subsampling?
    cm->subsampling_x = (sd != NULL) && sd->uv_width < sd->y_width;
    cm->subsampling_y = (sd != NULL) && sd->uv_height < sd->y_height;
    alloc_raw_frame_buffers(cpi);

    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
  }
}


int vp9_receive_raw_frame(VP9_PTR ptr, unsigned int frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  VP9_COMP              *cpi = (VP9_COMP *) ptr;
  struct vpx_usec_timer  timer;
  int                    res = 0;

  check_initial_width(cpi, sd);
  vpx_usec_timer_start(&timer);
  if (vp9_lookahead_push(cpi->lookahead, sd, time_stamp, end_time, frame_flags,
                         cpi->active_map_enabled ? cpi->active_map : NULL))
    res = -1;
  vpx_usec_timer_mark(&timer);
  cpi->time_receive_data += vpx_usec_timer_elapsed(&timer);

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

int vp9_get_compressed_data(VP9_PTR ptr, unsigned int *frame_flags,
                            unsigned long *size, unsigned char *dest,
                            int64_t *time_stamp, int64_t *time_end, int flush) {
  VP9_COMP *cpi = (VP9_COMP *) ptr;
  VP9_COMMON *cm = &cpi->common;
  struct vpx_usec_timer  cmptimer;
  YV12_BUFFER_CONFIG    *force_src_buffer = NULL;
  int i;
  // FILE *fp_out = fopen("enc_frame_type.txt", "a");

  if (!cpi)
    return -1;

  vpx_usec_timer_start(&cmptimer);

  cpi->source = NULL;

  cpi->common.allow_high_precision_mv = ALTREF_HIGH_PRECISION_MV;
  set_mvcost(cpi);

  // Should we code an alternate reference frame.
  if (cpi->oxcf.play_alternate && cpi->source_alt_ref_pending) {
    int frames_to_arf;

#if CONFIG_MULTIPLE_ARF
    assert(!cpi->multi_arf_enabled ||
           cpi->frame_coding_order[cpi->sequence_number] < 0);

    if (cpi->multi_arf_enabled && (cpi->pass == 2))
      frames_to_arf = (-cpi->frame_coding_order[cpi->sequence_number])
        - cpi->next_frame_in_order;
    else
#endif
      frames_to_arf = cpi->frames_till_gf_update_due;

    assert(frames_to_arf < cpi->twopass.frames_to_key);

    if ((cpi->source = vp9_lookahead_peek(cpi->lookahead, frames_to_arf))) {
#if CONFIG_MULTIPLE_ARF
      cpi->alt_ref_source[cpi->arf_buffered] = cpi->source;
#else
      cpi->alt_ref_source = cpi->source;
#endif

      if (cpi->oxcf.arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        // TODO(agrange) merge these two functions.
        configure_arnr_filter(cpi, cm->current_video_frame + frames_to_arf,
                              cpi->gfu_boost);
        vp9_temporal_filter_prepare(cpi, frames_to_arf);
        vp9_extend_frame_borders(&cpi->alt_ref_buffer,
                                 cm->subsampling_x, cm->subsampling_y);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_last_frame = 0;
      cpi->is_src_frame_alt_ref = 0;

      // TODO(agrange) This needs to vary depending on where the next ARF is.
      cpi->frames_till_alt_ref_frame = frames_to_arf;

#if CONFIG_MULTIPLE_ARF
      if (!cpi->multi_arf_enabled)
#endif
        cpi->source_alt_ref_pending = 0;   // Clear Pending altf Ref flag.
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
      cpi->is_src_frame_alt_ref = 0;
      for (i = 0; i < cpi->arf_buffered; ++i) {
        if (cpi->source == cpi->alt_ref_source[i]) {
          cpi->is_src_frame_alt_ref = 1;
          cpi->refresh_golden_frame = 1;
          break;
        }
      }
#else
      cpi->is_src_frame_alt_ref = cpi->alt_ref_source
                                  && (cpi->source == cpi->alt_ref_source);
#endif
      if (cpi->is_src_frame_alt_ref) {
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

    // fprintf(fp_out, "   Frame:%d", cm->current_video_frame);
#if CONFIG_MULTIPLE_ARF
    if (cpi->multi_arf_enabled) {
      // fprintf(fp_out, "   seq_no:%d  this_frame_weight:%d",
      //         cpi->sequence_number, cpi->this_frame_weight);
    } else {
      // fprintf(fp_out, "\n");
    }
#else
    // fprintf(fp_out, "\n");
#endif

#if CONFIG_MULTIPLE_ARF
    if ((cm->frame_type != KEY_FRAME) && (cpi->pass == 2))
      cpi->source_alt_ref_pending = is_next_frame_arf(cpi);
#endif
  } else {
    *size = 0;
    if (flush && cpi->pass == 1 && !cpi->twopass.first_pass_done) {
      vp9_end_first_pass(cpi);    /* get last stats packet */
      cpi->twopass.first_pass_done = 1;
    }

    // fclose(fp_out);
    return -1;
  }

  if (cpi->source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = cpi->source->ts_start;
    cpi->last_end_time_stamp_seen = cpi->source->ts_start;
  }

  // adjust frame rates based on timestamps given
  if (!cpi->refresh_alt_ref_frame) {
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

  // start with a 0 size frame
  *size = 0;

  // Clear down mmx registers
  vp9_clear_system_state();  // __asm emms;

  /* find a free buffer for the new frame, releasing the reference previously
   * held.
   */
  cm->fb_idx_ref_cnt[cm->new_fb_idx]--;
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

  /* Get the mapping of L/G/A to the reference buffer pool */
  cm->active_ref_idx[0] = cm->ref_frame_map[cpi->lst_fb_idx];
  cm->active_ref_idx[1] = cm->ref_frame_map[cpi->gld_fb_idx];
  cm->active_ref_idx[2] = cm->ref_frame_map[cpi->alt_fb_idx];

#if 0  // CONFIG_MULTIPLE_ARF
  if (cpi->multi_arf_enabled) {
    fprintf(fp_out, "      idx(%d, %d, %d, %d) active(%d, %d, %d)",
        cpi->lst_fb_idx, cpi->gld_fb_idx, cpi->alt_fb_idx, cm->new_fb_idx,
        cm->active_ref_idx[0], cm->active_ref_idx[1], cm->active_ref_idx[2]);
    if (cpi->refresh_alt_ref_frame)
      fprintf(fp_out, "  type:ARF");
    if (cpi->is_src_frame_alt_ref)
      fprintf(fp_out, "  type:OVERLAY[%d]", cpi->alt_fb_idx);
    fprintf(fp_out, "\n");
  }
#endif

  cm->frame_type = INTER_FRAME;
  cm->frame_flags = *frame_flags;

  // Reset the frame pointers to the current frame size
  vp9_realloc_frame_buffer(get_frame_new_buffer(cm),
                           cm->width, cm->height,
                           cm->subsampling_x, cm->subsampling_y,
                           VP9BORDERINPIXELS);

  // Calculate scaling factors for each of the 3 available references
  for (i = 0; i < ALLOWED_REFS_PER_FRAME; ++i)
    vp9_setup_scale_factors(cm, i);

  vp9_setup_interp_filters(&cpi->mb.e_mbd, DEFAULT_INTERP_FILTER, cm);

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
      vp9_vaq_init();
  }

  if (cpi->pass == 1) {
    Pass1Encode(cpi, size, dest, frame_flags);
  } else if (cpi->pass == 2) {
    Pass2Encode(cpi, size, dest, frame_flags);
  } else {
    encode_frame_to_data_rate(cpi, size, dest, frame_flags);
  }

  if (cm->refresh_frame_context)
    cm->frame_contexts[cm->frame_context_idx] = cm->fc;

  if (*size > 0) {
    // if its a dropped frame honor the requests on subsequent frames
    cpi->droppable = !frame_is_reference(cpi);

    // return to normal state
    cm->reset_frame_context = 0;
    cm->refresh_frame_context = 1;
    cpi->refresh_alt_ref_frame = 0;
    cpi->refresh_golden_frame = 0;
    cpi->refresh_last_frame = 1;
    cm->frame_type = INTER_FRAME;
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
        double ye, ue, ve;
        double frame_psnr;
        YV12_BUFFER_CONFIG      *orig = cpi->Source;
        YV12_BUFFER_CONFIG      *recon = cpi->common.frame_to_show;
        YV12_BUFFER_CONFIG      *pp = &cm->post_proc_buffer;
        int y_samples = orig->y_height * orig->y_width;
        int uv_samples = orig->uv_height * orig->uv_width;
        int t_samples = y_samples + 2 * uv_samples;
        double sq_error;

        ye = (double)calc_plane_error(orig->y_buffer, orig->y_stride,
                              recon->y_buffer, recon->y_stride,
                              orig->y_crop_width, orig->y_crop_height);

        ue = (double)calc_plane_error(orig->u_buffer, orig->uv_stride,
                              recon->u_buffer, recon->uv_stride,
                              orig->uv_crop_width, orig->uv_crop_height);

        ve = (double)calc_plane_error(orig->v_buffer, orig->uv_stride,
                              recon->v_buffer, recon->uv_stride,
                              orig->uv_crop_width, orig->uv_crop_height);

        sq_error = ye + ue + ve;

        frame_psnr = vp9_mse2psnr(t_samples, 255.0, sq_error);

        cpi->total_y += vp9_mse2psnr(y_samples, 255.0, ye);
        cpi->total_u += vp9_mse2psnr(uv_samples, 255.0, ue);
        cpi->total_v += vp9_mse2psnr(uv_samples, 255.0, ve);
        cpi->total_sq_error += sq_error;
        cpi->total  += frame_psnr;
        {
          double frame_psnr2, frame_ssim2 = 0;
          double weight = 0;
#if CONFIG_VP9_POSTPROC
          vp9_deblock(cm->frame_to_show, &cm->post_proc_buffer,
                      cm->lf.filter_level * 10 / 6);
#endif
          vp9_clear_system_state();

          ye = (double)calc_plane_error(orig->y_buffer, orig->y_stride,
                                pp->y_buffer, pp->y_stride,
                                orig->y_crop_width, orig->y_crop_height);

          ue = (double)calc_plane_error(orig->u_buffer, orig->uv_stride,
                                pp->u_buffer, pp->uv_stride,
                                orig->uv_crop_width, orig->uv_crop_height);

          ve = (double)calc_plane_error(orig->v_buffer, orig->uv_stride,
                                pp->v_buffer, pp->uv_stride,
                                orig->uv_crop_width, orig->uv_crop_height);

          sq_error = ye + ue + ve;

          frame_psnr2 = vp9_mse2psnr(t_samples, 255.0, sq_error);

          cpi->totalp_y += vp9_mse2psnr(y_samples, 255.0, ye);
          cpi->totalp_u += vp9_mse2psnr(uv_samples, 255.0, ue);
          cpi->totalp_v += vp9_mse2psnr(uv_samples, 255.0, ve);
          cpi->total_sq_error2 += sq_error;
          cpi->totalp  += frame_psnr2;

          frame_ssim2 = vp9_calc_ssim(cpi->Source,
                                      recon, 1, &weight);

          cpi->summed_quality += frame_ssim2 * weight;
          cpi->summed_weights += weight;

          frame_ssim2 = vp9_calc_ssim(cpi->Source,
                                      &cm->post_proc_buffer, 1, &weight);

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
        frame_all =  vp9_calc_ssimg(cpi->Source, cm->frame_to_show,
                                    &y, &u, &v);
        cpi->total_ssimg_y += y;
        cpi->total_ssimg_u += u;
        cpi->total_ssimg_v += v;
        cpi->total_ssimg_all += frame_all;
      }
    }
  }

#endif
  // fclose(fp_out);
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
      dest->uv_height = cpi->common.height / 2;
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

  check_initial_width(cpi, NULL);

  if (width) {
    cm->width = width;
    if (cm->width * 5 < cpi->initial_width) {
      cm->width = cpi->initial_width / 5 + 1;
      printf("Warning: Desired width too small, changed to %d \n", cm->width);
    }
    if (cm->width > cpi->initial_width) {
      cm->width = cpi->initial_width;
      printf("Warning: Desired width too large, changed to %d \n", cm->width);
    }
  }

  if (height) {
    cm->height = height;
    if (cm->height * 5 < cpi->initial_height) {
      cm->height = cpi->initial_height / 5 + 1;
      printf("Warning: Desired height too small, changed to %d \n", cm->height);
    }
    if (cm->height > cpi->initial_height) {
      cm->height = cpi->initial_height;
      printf("Warning: Desired height too large, changed to %d \n", cm->height);
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

int vp9_calc_ss_err(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest) {
  int i, j;
  int total = 0;

  uint8_t *src = source->y_buffer;
  uint8_t *dst = dest->y_buffer;

  // Loop through the Y plane raw and reconstruction data summing
  // (square differences)
  for (i = 0; i < source->y_height; i += 16) {
    for (j = 0; j < source->y_width; j += 16) {
      unsigned int sse;
      total += vp9_mse16x16(src + j, source->y_stride, dst + j, dest->y_stride,
                            &sse);
    }

    src += 16 * source->y_stride;
    dst += 16 * dest->y_stride;
  }

  return total;
}


int vp9_get_quantizer(VP9_PTR c) {
  return ((VP9_COMP *)c)->common.base_qindex;
}
