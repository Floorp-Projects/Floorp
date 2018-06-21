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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "av1/common/alloccommon.h"
#include "av1/common/cdef.h"
#include "av1/common/filter.h"
#include "av1/common/idct.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/resize.h"
#include "av1/common/tile_common.h"

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/hash_motion.h"
#include "av1/encoder/mbgraph.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/temporal_filter.h"

#include "aom_dsp/psnr.h"
#if CONFIG_INTERNAL_STATS
#include "aom_dsp/ssim.h"
#endif
#include "av1/encoder/grain_test_vectors.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"
#if CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG || CONFIG_MISMATCH_DEBUG

#define DEFAULT_EXPLICIT_ORDER_HINT_BITS 7

// av1 uses 10,000,000 ticks/second as time stamp
#define TICKS_PER_SEC 10000000LL

#if CONFIG_ENTROPY_STATS
FRAME_COUNTS aggregate_fc;
#endif  // CONFIG_ENTROPY_STATS

#define AM_SEGMENT_ID_INACTIVE 7
#define AM_SEGMENT_ID_ACTIVE 0

// Whether to use high precision mv for altref computation.
#define ALTREF_HIGH_PRECISION_MV 1

// Q threshold for high precision mv. Choose a very high value for now so that
// HIGH_PRECISION is always chosen.
#define HIGH_PRECISION_MV_QTHRESH 200

// #define OUTPUT_YUV_REC
#ifdef OUTPUT_YUV_SKINMAP
FILE *yuv_skinmap_file = NULL;
#endif
#ifdef OUTPUT_YUV_REC
FILE *yuv_rec_file;
#define FILE_NAME_LEN 100
#endif

static INLINE void Scale2Ratio(AOM_SCALING mode, int *hr, int *hs) {
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

// Mark all inactive blocks as active. Other segmentation features may be set
// so memset cannot be used, instead only inactive blocks should be reset.
static void suppress_active_map(AV1_COMP *cpi) {
  unsigned char *const seg_map = cpi->segmentation_map;
  int i;
  if (cpi->active_map.enabled || cpi->active_map.update)
    for (i = 0; i < cpi->common.mi_rows * cpi->common.mi_cols; ++i)
      if (seg_map[i] == AM_SEGMENT_ID_INACTIVE)
        seg_map[i] = AM_SEGMENT_ID_ACTIVE;
}

static void apply_active_map(AV1_COMP *cpi) {
  struct segmentation *const seg = &cpi->common.seg;
  unsigned char *const seg_map = cpi->segmentation_map;
  const unsigned char *const active_map = cpi->active_map.map;
  int i;

  assert(AM_SEGMENT_ID_ACTIVE == CR_SEGMENT_ID_BASE);

  if (frame_is_intra_only(&cpi->common)) {
    cpi->active_map.enabled = 0;
    cpi->active_map.update = 1;
  }

  if (cpi->active_map.update) {
    if (cpi->active_map.enabled) {
      for (i = 0; i < cpi->common.mi_rows * cpi->common.mi_cols; ++i)
        if (seg_map[i] == AM_SEGMENT_ID_ACTIVE) seg_map[i] = active_map[i];
      av1_enable_segmentation(seg);
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_SKIP);
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_H);
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_V);
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_U);
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_V);

      av1_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_H,
                      -MAX_LOOP_FILTER);
      av1_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_V,
                      -MAX_LOOP_FILTER);
      av1_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_U,
                      -MAX_LOOP_FILTER);
      av1_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_V,
                      -MAX_LOOP_FILTER);
    } else {
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_SKIP);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_H);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_V);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_U);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_V);
      if (seg->enabled) {
        seg->update_data = 1;
        seg->update_map = 1;
      }
    }
    cpi->active_map.update = 0;
  }
}

int av1_set_active_map(AV1_COMP *cpi, unsigned char *new_map_16x16, int rows,
                       int cols) {
  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols) {
    unsigned char *const active_map_8x8 = cpi->active_map.map;
    const int mi_rows = cpi->common.mi_rows;
    const int mi_cols = cpi->common.mi_cols;
    const int row_scale = mi_size_high[BLOCK_16X16] == 2 ? 1 : 2;
    const int col_scale = mi_size_wide[BLOCK_16X16] == 2 ? 1 : 2;
    cpi->active_map.update = 1;
    if (new_map_16x16) {
      int r, c;
      for (r = 0; r < mi_rows; ++r) {
        for (c = 0; c < mi_cols; ++c) {
          active_map_8x8[r * mi_cols + c] =
              new_map_16x16[(r >> row_scale) * cols + (c >> col_scale)]
                  ? AM_SEGMENT_ID_ACTIVE
                  : AM_SEGMENT_ID_INACTIVE;
        }
      }
      cpi->active_map.enabled = 1;
    } else {
      cpi->active_map.enabled = 0;
    }
    return 0;
  } else {
    return -1;
  }
}

int av1_get_active_map(AV1_COMP *cpi, unsigned char *new_map_16x16, int rows,
                       int cols) {
  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols &&
      new_map_16x16) {
    unsigned char *const seg_map_8x8 = cpi->segmentation_map;
    const int mi_rows = cpi->common.mi_rows;
    const int mi_cols = cpi->common.mi_cols;
    const int row_scale = mi_size_high[BLOCK_16X16] == 2 ? 1 : 2;
    const int col_scale = mi_size_wide[BLOCK_16X16] == 2 ? 1 : 2;

    memset(new_map_16x16, !cpi->active_map.enabled, rows * cols);
    if (cpi->active_map.enabled) {
      int r, c;
      for (r = 0; r < mi_rows; ++r) {
        for (c = 0; c < mi_cols; ++c) {
          // Cyclic refresh segments are considered active despite not having
          // AM_SEGMENT_ID_ACTIVE
          new_map_16x16[(r >> row_scale) * cols + (c >> col_scale)] |=
              seg_map_8x8[r * mi_cols + c] != AM_SEGMENT_ID_INACTIVE;
        }
      }
    }
    return 0;
  } else {
    return -1;
  }
}

static void set_high_precision_mv(AV1_COMP *cpi, int allow_high_precision_mv,
                                  int cur_frame_force_integer_mv) {
  MACROBLOCK *const mb = &cpi->td.mb;
  cpi->common.allow_high_precision_mv =
      allow_high_precision_mv && cur_frame_force_integer_mv == 0;
  const int copy_hp =
      cpi->common.allow_high_precision_mv && cur_frame_force_integer_mv == 0;
  int *(*src)[2] = copy_hp ? &mb->nmvcost_hp : &mb->nmvcost;
  mb->mv_cost_stack = *src;
}

static BLOCK_SIZE select_sb_size(const AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;

  if (cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_64X64)
    return BLOCK_64X64;
#if CONFIG_FILEOPTIONS
  if (cm->options && cm->options->ext_partition)
#endif
    if (cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_128X128)
      return BLOCK_128X128;

  assert(cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_DYNAMIC);

// TODO(any): Possibly could improve this with a heuristic.
#if CONFIG_FILEOPTIONS
  if (cm->options && !cm->options->ext_partition) return BLOCK_64X64;
#endif

  // When superres / resize is on, 'cm->width / height' can change between
  // calls, so we don't apply this heuristic there. Also, this heuristic gives
  // compression gain for speed >= 2 only.
  if (cpi->oxcf.superres_mode == SUPERRES_NONE &&
      cpi->oxcf.resize_mode == RESIZE_NONE && cpi->oxcf.speed >= 2) {
    return (cm->width >= 480 && cm->height >= 360) ? BLOCK_128X128
                                                   : BLOCK_64X64;
  }

  return BLOCK_128X128;
}

static void setup_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  // Set up entropy context depending on frame type. The decoder mandates
  // the use of the default context, index 0, for keyframes and inter
  // frames where the error_resilient_mode or intra_only flag is set. For
  // other inter-frames the encoder currently uses only two contexts;
  // context 1 for ALTREF frames and context 0 for the others.

  cm->primary_ref_frame = PRIMARY_REF_NONE;
  if (frame_is_intra_only(cm) || cm->error_resilient_mode ||
      cm->force_primary_ref_none) {
    av1_setup_past_independence(cm);
    for (int i = 0; i < REF_FRAMES; i++) {
      cm->fb_of_context_type[i] = -1;
    }
    cm->fb_of_context_type[REGULAR_FRAME] =
        get_ref_frame_map_idx(cpi, GOLDEN_FRAME);
    cm->frame_context_idx = REGULAR_FRAME;
  } else {
    const GF_GROUP *gf_group = &cpi->twopass.gf_group;
    if (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE)
      cm->frame_context_idx = EXT_ARF_FRAME;
    else if (cpi->refresh_alt_ref_frame)
      cm->frame_context_idx = ARF_FRAME;
    else if (cpi->rc.is_src_frame_alt_ref)
      cm->frame_context_idx = OVERLAY_FRAME;
    else if (cpi->refresh_golden_frame)
      cm->frame_context_idx = GLD_FRAME;
    else if (cpi->refresh_bwd_ref_frame)
      cm->frame_context_idx = BRF_FRAME;
    else
      cm->frame_context_idx = REGULAR_FRAME;
    int wanted_fb = cm->fb_of_context_type[cm->frame_context_idx];
    for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
      int fb = get_ref_frame_map_idx(cpi, ref_frame);
      if (fb == wanted_fb) {
        cm->primary_ref_frame = ref_frame - LAST_FRAME;
      }
    }
  }

  if (cm->frame_type == KEY_FRAME) {
    cpi->refresh_golden_frame = 1;
    cpi->refresh_alt_ref_frame = 1;
    av1_zero(cpi->interp_filter_selected);
    set_sb_size(&cm->seq_params, select_sb_size(cpi));
    set_use_reference_buffer(cm, 0);
  } else if (frame_is_sframe(cm)) {
    cpi->refresh_golden_frame = 1;
    cpi->refresh_alt_ref_frame = 1;
    av1_zero(cpi->interp_filter_selected);
    set_sb_size(&cm->seq_params, select_sb_size(cpi));
  } else {
    if (cm->primary_ref_frame == PRIMARY_REF_NONE ||
        cm->frame_refs[cm->primary_ref_frame].idx < 0) {
      av1_setup_past_independence(cm);
      cm->seg.update_map = 1;
      cm->seg.update_data = 1;
    } else {
      *cm->fc = cm->frame_contexts[cm->frame_refs[cm->primary_ref_frame].idx];
    }
    av1_zero(cpi->interp_filter_selected[0]);
  }

  cm->prev_frame = get_prev_frame(cm);
  cpi->vaq_refresh = 0;
}

static void enc_setup_mi(AV1_COMMON *cm) {
  int i;
  cm->mi = cm->mip;
  memset(cm->mip, 0, cm->mi_stride * cm->mi_rows * sizeof(*cm->mip));
  cm->prev_mi = cm->prev_mip;
  // Clear top border row
  memset(cm->prev_mip, 0, sizeof(*cm->prev_mip) * cm->mi_stride);
  // Clear left border column
  for (i = 0; i < cm->mi_rows; ++i)
    memset(&cm->prev_mip[i * cm->mi_stride], 0, sizeof(*cm->prev_mip));
  cm->mi_grid_visible = cm->mi_grid_base;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base;

  memset(cm->mi_grid_base, 0,
         cm->mi_stride * cm->mi_rows * sizeof(*cm->mi_grid_base));
}

static int enc_alloc_mi(AV1_COMMON *cm, int mi_size) {
  cm->mip = aom_calloc(mi_size, sizeof(*cm->mip));
  if (!cm->mip) return 1;
  cm->prev_mip = aom_calloc(mi_size, sizeof(*cm->prev_mip));
  if (!cm->prev_mip) return 1;
  cm->mi_alloc_size = mi_size;

  cm->mi_grid_base =
      (MB_MODE_INFO **)aom_calloc(mi_size, sizeof(MB_MODE_INFO *));
  if (!cm->mi_grid_base) return 1;
  cm->prev_mi_grid_base =
      (MB_MODE_INFO **)aom_calloc(mi_size, sizeof(MB_MODE_INFO *));
  if (!cm->prev_mi_grid_base) return 1;

  return 0;
}

static void enc_free_mi(AV1_COMMON *cm) {
  aom_free(cm->mip);
  cm->mip = NULL;
  aom_free(cm->prev_mip);
  cm->prev_mip = NULL;
  aom_free(cm->mi_grid_base);
  cm->mi_grid_base = NULL;
  aom_free(cm->prev_mi_grid_base);
  cm->prev_mi_grid_base = NULL;
  cm->mi_alloc_size = 0;
}

static void swap_mi_and_prev_mi(AV1_COMMON *cm) {
  // Current mip will be the prev_mip for the next frame.
  MB_MODE_INFO **temp_base = cm->prev_mi_grid_base;
  MB_MODE_INFO *temp = cm->prev_mip;
  cm->prev_mip = cm->mip;
  cm->mip = temp;

  // Update the upper left visible macroblock ptrs.
  cm->mi = cm->mip;
  cm->prev_mi = cm->prev_mip;

  cm->prev_mi_grid_base = cm->mi_grid_base;
  cm->mi_grid_base = temp_base;
  cm->mi_grid_visible = cm->mi_grid_base;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base;
}

void av1_initialize_enc(void) {
  static volatile int init_done = 0;

  if (!init_done) {
    av1_rtcd();
    aom_dsp_rtcd();
    aom_scale_rtcd();
    av1_init_intra_predictors();
    av1_init_me_luts();
    av1_rc_init_minq_luts();
    av1_init_wedge_masks();
    init_done = 1;
  }
}

static void dealloc_context_buffers_ext(AV1_COMP *cpi) {
  if (cpi->mbmi_ext_base) {
    aom_free(cpi->mbmi_ext_base);
    cpi->mbmi_ext_base = NULL;
  }
}

static void alloc_context_buffers_ext(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  int mi_size = cm->mi_cols * cm->mi_rows;

  dealloc_context_buffers_ext(cpi);
  CHECK_MEM_ERROR(cm, cpi->mbmi_ext_base,
                  aom_calloc(mi_size, sizeof(*cpi->mbmi_ext_base)));
}

static void update_film_grain_parameters(struct AV1_COMP *cpi,
                                         const AV1EncoderConfig *oxcf) {
  AV1_COMMON *const cm = &cpi->common;
  cpi->oxcf = *oxcf;

  if (cm->film_grain_table) {
    aom_film_grain_table_free(cm->film_grain_table);
    aom_free(cm->film_grain_table);
  }
  cm->film_grain_table = 0;

  if (oxcf->film_grain_test_vector) {
    cm->film_grain_params_present = 1;
    if (cm->frame_type == KEY_FRAME) {
      memcpy(&cm->film_grain_params,
             film_grain_test_vectors + oxcf->film_grain_test_vector - 1,
             sizeof(cm->film_grain_params));

      cm->film_grain_params.bit_depth = cm->bit_depth;
      if (cm->color_range == AOM_CR_FULL_RANGE) {
        cm->film_grain_params.clip_to_restricted_range = 0;
      }
    }
  } else if (oxcf->film_grain_table_filename) {
    cm->film_grain_table = aom_malloc(sizeof(*cm->film_grain_table));
    memset(cm->film_grain_table, 0, sizeof(aom_film_grain_table_t));

    aom_film_grain_table_read(cm->film_grain_table,
                              oxcf->film_grain_table_filename, &cm->error);
  } else {
    cm->film_grain_params_present = 0;
    memset(&cm->film_grain_params, 0, sizeof(cm->film_grain_params));
  }
}

static void dealloc_compressor_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);

  dealloc_context_buffers_ext(cpi);

  aom_free(cpi->tile_data);
  cpi->tile_data = NULL;

  // Delete sementation map
  aom_free(cpi->segmentation_map);
  cpi->segmentation_map = NULL;

  av1_cyclic_refresh_free(cpi->cyclic_refresh);
  cpi->cyclic_refresh = NULL;

  aom_free(cpi->active_map.map);
  cpi->active_map.map = NULL;

  aom_free(cpi->td.mb.above_pred_buf);
  cpi->td.mb.above_pred_buf = NULL;

  aom_free(cpi->td.mb.left_pred_buf);
  cpi->td.mb.left_pred_buf = NULL;

  aom_free(cpi->td.mb.wsrc_buf);
  cpi->td.mb.wsrc_buf = NULL;

  aom_free(cpi->td.mb.mask_buf);
  cpi->td.mb.mask_buf = NULL;

  aom_free(cm->tpl_mvs);
  cm->tpl_mvs = NULL;

  av1_free_ref_frame_buffers(cm->buffer_pool);
  av1_free_txb_buf(cpi);
  av1_free_context_buffers(cm);

  aom_free_frame_buffer(&cpi->last_frame_uf);
  av1_free_restoration_buffers(cm);
  aom_free_frame_buffer(&cpi->trial_frame_rst);
  aom_free_frame_buffer(&cpi->scaled_source);
  aom_free_frame_buffer(&cpi->scaled_last_source);
  aom_free_frame_buffer(&cpi->alt_ref_buffer);
  av1_lookahead_destroy(cpi->lookahead);

  aom_free(cpi->tile_tok[0][0]);
  cpi->tile_tok[0][0] = 0;

  av1_free_pc_tree(&cpi->td, num_planes);

  aom_free(cpi->td.mb.palette_buffer);
}

static void save_coding_context(AV1_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  AV1_COMMON *cm = &cpi->common;

  // Stores a snapshot of key state variables which can subsequently be
  // restored with a call to av1_restore_coding_context. These functions are
  // intended for use in a re-code loop in av1_compress_frame where the
  // quantizer value is adjusted between loop iterations.
  av1_copy(cc->nmv_vec_cost, cpi->td.mb.nmv_vec_cost);
  av1_copy(cc->nmv_costs, cpi->nmv_costs);
  av1_copy(cc->nmv_costs_hp, cpi->nmv_costs_hp);

  cc->fc = *cm->fc;
}

static void restore_coding_context(AV1_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  AV1_COMMON *cm = &cpi->common;

  // Restore key state variables to the snapshot state stored in the
  // previous call to av1_save_coding_context.
  av1_copy(cpi->td.mb.nmv_vec_cost, cc->nmv_vec_cost);
  av1_copy(cpi->nmv_costs, cc->nmv_costs);
  av1_copy(cpi->nmv_costs_hp, cc->nmv_costs_hp);

  *cm->fc = cc->fc;
}

static void configure_static_seg_features(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  struct segmentation *const seg = &cm->seg;

  int high_q = (int)(rc->avg_q > 48.0);
  int qi_delta;

  // Disable and clear down for KF
  if (cm->frame_type == KEY_FRAME) {
    // Clear down the global segmentation map
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation
    av1_disable_segmentation(seg);

    // Clear down the segment features.
    av1_clearall_segfeatures(seg);
  } else if (cpi->refresh_alt_ref_frame) {
    // If this is an alt ref frame
    // Clear down the global segmentation map
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation and individual segment features by default
    av1_disable_segmentation(seg);
    av1_clearall_segfeatures(seg);

    // Scan frames from current to arf frame.
    // This function re-enables segmentation if appropriate.
    av1_update_mbgraph_stats(cpi);

    // If segmentation was enabled set those features needed for the
    // arf itself.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;

      qi_delta =
          av1_compute_qdelta(rc, rc->avg_q, rc->avg_q * 0.875, cm->bit_depth);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta - 2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_H, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_V, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_U, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_V, -2);

      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_H);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_V);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_U);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_V);

      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);
    }
  } else if (seg->enabled) {
    // All other frames if segmentation has been enabled

    // First normal frame in a valid gf or alt ref group
    if (rc->frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (rc->source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;

        qi_delta =
            av1_compute_qdelta(rc, rc->avg_q, rc->avg_q * 1.125, cm->bit_depth);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta + 2);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_H, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_V, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_U, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_V, -2);

        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_H);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_V);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_U);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_V);

        // Segment coding disabled for compred testing
        if (high_q || (cpi->static_mb_pct == 100)) {
          av1_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);
          av1_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);
          av1_enable_segfeature(seg, 1, SEG_LVL_SKIP);
        }
      } else {
        // Disable segmentation and clear down features if alt ref
        // is not active for this group

        av1_disable_segmentation(seg);

        memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

        seg->update_map = 0;
        seg->update_data = 0;

        av1_clearall_segfeatures(seg);
      }
    } else if (rc->is_src_frame_alt_ref) {
      // Special case where we are coding over the top of a previous
      // alt ref frame.
      // Segment coding disabled for compred testing

      // Enable ref frame features for segment 0 as well
      av1_enable_segfeature(seg, 0, SEG_LVL_REF_FRAME);
      av1_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);

      // All mbs should use ALTREF_FRAME
      av1_clear_segdata(seg, 0, SEG_LVL_REF_FRAME);
      av1_set_segdata(seg, 0, SEG_LVL_REF_FRAME, ALTREF_FRAME);
      av1_clear_segdata(seg, 1, SEG_LVL_REF_FRAME);
      av1_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);

      // Skip all MBs if high Q (0,0 mv and skip coeffs)
      if (high_q) {
        av1_enable_segfeature(seg, 0, SEG_LVL_SKIP);
        av1_enable_segfeature(seg, 1, SEG_LVL_SKIP);
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

static void update_reference_segmentation_map(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MB_MODE_INFO **mi_4x4_ptr = cm->mi_grid_visible;
  uint8_t *cache_ptr = cm->current_frame_seg_map;
  int row, col;

  for (row = 0; row < cm->mi_rows; row++) {
    MB_MODE_INFO **mi_4x4 = mi_4x4_ptr;
    uint8_t *cache = cache_ptr;
    for (col = 0; col < cm->mi_cols; col++, mi_4x4++, cache++)
      cache[0] = mi_4x4[0]->segment_id;
    mi_4x4_ptr += cm->mi_stride;
    cache_ptr += cm->mi_cols;
  }
}

static void alloc_raw_frame_buffers(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;

  if (!cpi->lookahead)
    cpi->lookahead = av1_lookahead_init(
        oxcf->width, oxcf->height, cm->subsampling_x, cm->subsampling_y,
        cm->use_highbitdepth, oxcf->lag_in_frames);
  if (!cpi->lookahead)
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  // TODO(agrange) Check if ARF is enabled and skip allocation if not.
  if (aom_realloc_frame_buffer(&cpi->alt_ref_buffer, oxcf->width, oxcf->height,
                               cm->subsampling_x, cm->subsampling_y,
                               cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                               cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

static void alloc_util_frame_buffers(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (aom_realloc_frame_buffer(&cpi->last_frame_uf, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                               cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

  if (aom_realloc_frame_buffer(
          &cpi->trial_frame_rst, cm->superres_upscaled_width,
          cm->superres_upscaled_height, cm->subsampling_x, cm->subsampling_y,
          cm->use_highbitdepth, AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
          NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate trial restored frame buffer");

  if (aom_realloc_frame_buffer(&cpi->scaled_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                               cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate scaled source buffer");

  if (aom_realloc_frame_buffer(&cpi->scaled_last_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                               cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate scaled last source buffer");
}

static void alloc_compressor_data(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);

  av1_alloc_context_buffers(cm, cm->width, cm->height);

  av1_alloc_txb_buf(cpi);

  alloc_context_buffers_ext(cpi);

  aom_free(cpi->tile_tok[0][0]);

  {
    unsigned int tokens =
        get_token_alloc(cm->mb_rows, cm->mb_cols, MAX_SB_SIZE_LOG2, num_planes);
    CHECK_MEM_ERROR(cm, cpi->tile_tok[0][0],
                    aom_calloc(tokens, sizeof(*cpi->tile_tok[0][0])));
  }

  av1_setup_pc_tree(&cpi->common, &cpi->td);
}

void av1_new_framerate(AV1_COMP *cpi, double framerate) {
  cpi->framerate = framerate < 0.1 ? 30 : framerate;
  av1_rc_update_framerate(cpi, cpi->common.width, cpi->common.height);
}

static void set_tile_info(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int i, start_sb;

  av1_get_tile_limits(cm);

  // configure tile columns
  if (cpi->oxcf.tile_width_count == 0 || cpi->oxcf.tile_height_count == 0) {
    cm->uniform_tile_spacing_flag = 1;
    cm->log2_tile_cols = AOMMAX(cpi->oxcf.tile_columns, cm->min_log2_tile_cols);
    cm->log2_tile_cols = AOMMIN(cm->log2_tile_cols, cm->max_log2_tile_cols);
  } else {
    int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, cm->seq_params.mib_size_log2);
    int sb_cols = mi_cols >> cm->seq_params.mib_size_log2;
    int size_sb, j = 0;
    cm->uniform_tile_spacing_flag = 0;
    for (i = 0, start_sb = 0; start_sb < sb_cols && i < MAX_TILE_COLS; i++) {
      cm->tile_col_start_sb[i] = start_sb;
      size_sb = cpi->oxcf.tile_widths[j++];
      if (j >= cpi->oxcf.tile_width_count) j = 0;
      start_sb += AOMMIN(size_sb, cm->max_tile_width_sb);
    }
    cm->tile_cols = i;
    cm->tile_col_start_sb[i] = sb_cols;
  }
  av1_calculate_tile_cols(cm);

  // configure tile rows
  if (cm->uniform_tile_spacing_flag) {
    cm->log2_tile_rows = AOMMAX(cpi->oxcf.tile_rows, cm->min_log2_tile_rows);
    cm->log2_tile_rows = AOMMIN(cm->log2_tile_rows, cm->max_log2_tile_rows);
  } else {
    int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, cm->seq_params.mib_size_log2);
    int sb_rows = mi_rows >> cm->seq_params.mib_size_log2;
    int size_sb, j = 0;
    for (i = 0, start_sb = 0; start_sb < sb_rows && i < MAX_TILE_ROWS; i++) {
      cm->tile_row_start_sb[i] = start_sb;
      size_sb = cpi->oxcf.tile_heights[j++];
      if (j >= cpi->oxcf.tile_height_count) j = 0;
      start_sb += AOMMIN(size_sb, cm->max_tile_height_sb);
    }
    cm->tile_rows = i;
    cm->tile_row_start_sb[i] = sb_rows;
  }
  av1_calculate_tile_rows(cm);
}

static void update_frame_size(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  av1_set_mb_mi(cm, cm->width, cm->height);
  av1_init_context_buffers(cm);
  av1_init_macroblockd(cm, xd, NULL);
  memset(cpi->mbmi_ext_base, 0,
         cm->mi_rows * cm->mi_cols * sizeof(*cpi->mbmi_ext_base));
  set_tile_info(cpi);
}

static void init_buffer_indices(AV1_COMP *cpi) {
  int fb_idx;
  for (fb_idx = 0; fb_idx < REF_FRAMES; ++fb_idx)
    cpi->ref_fb_idx[fb_idx] = fb_idx;
  for (fb_idx = 0; fb_idx < MAX_EXT_ARFS + 1; ++fb_idx)
    cpi->arf_map[fb_idx] = LAST_REF_FRAMES + 2 + fb_idx;
  cpi->rate_index = 0;
  cpi->rate_size = 0;
  cpi->cur_poc = -1;
}

static INLINE int does_level_match(int width, int height, double fps,
                                   int lvl_width, int lvl_height,
                                   double lvl_fps, int lvl_dim_mult) {
  const int64_t lvl_luma_pels = lvl_width * lvl_height;
  const double lvl_display_sample_rate = lvl_luma_pels * lvl_fps;
  const int64_t luma_pels = width * height;
  const double display_sample_rate = luma_pels * fps;
  return luma_pels <= lvl_luma_pels &&
         display_sample_rate <= lvl_display_sample_rate &&
         width <= lvl_width * lvl_dim_mult &&
         height <= lvl_height * lvl_dim_mult;
}

static void set_bitstream_level_tier(SequenceHeader *seq, AV1_COMMON *cm,
                                     const AV1EncoderConfig *oxcf) {
  // TODO(any): This is a placeholder function that only addresses dimensions
  // and max display sample rates.
  // Need to add checks for max bit rate, max decoded luma sample rate, header
  // rate, etc. that are not covered by this function.
  (void)oxcf;
  BitstreamLevel bl = { 9, 3 };
  if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate, 512,
                       288, 30.0, 4)) {
    bl.major = 2;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              704, 396, 30.0, 4)) {
    bl.major = 2;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              1088, 612, 30.0, 4)) {
    bl.major = 3;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              1376, 774, 30.0, 4)) {
    bl.major = 3;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              2048, 1152, 30.0, 3)) {
    bl.major = 4;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              2048, 1152, 60.0, 3)) {
    bl.major = 4;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              4096, 2176, 30.0, 2)) {
    bl.major = 5;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              4096, 2176, 60.0, 2)) {
    bl.major = 5;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              4096, 2176, 120.0, 2)) {
    bl.major = 5;
    bl.minor = 2;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              8192, 4352, 30.0, 2)) {
    bl.major = 6;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              8192, 4352, 60.0, 2)) {
    bl.major = 6;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              8192, 4352, 120.0, 2)) {
    bl.major = 6;
    bl.minor = 2;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              16384, 8704, 30.0, 2)) {
    bl.major = 7;
    bl.minor = 0;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              16384, 8704, 60.0, 2)) {
    bl.major = 7;
    bl.minor = 1;
  } else if (does_level_match(oxcf->width, oxcf->height, oxcf->init_framerate,
                              16384, 8704, 120.0, 2)) {
    bl.major = 7;
    bl.minor = 2;
  }
  for (int i = 0; i < MAX_NUM_OPERATING_POINTS; ++i) {
    seq->level[i] = bl;
    seq->tier[i] = 0;  // setting main tier by default
    // Set the maximum parameters for bitrate and buffer size for this profile,
    // level, and tier
    cm->op_params[i].bitrate = max_level_bitrate(
        cm->profile, major_minor_to_seq_level_idx(seq->level[i]), seq->tier[i]);
    // Level with seq_level_idx = 31 returns a high "dummy" bitrate to pass the
    // check
    if (cm->op_params[i].bitrate == 0)
      aom_internal_error(
          &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
          "AV1 does not support this combination of profile, level, and tier.");
    // Buffer size in bits/s is bitrate in bits/s * 1 s
    cm->op_params[i].buffer_size = cm->op_params[i].bitrate;
  }
}

static void init_seq_coding_tools(SequenceHeader *seq, AV1_COMMON *cm,
                                  const AV1EncoderConfig *oxcf) {
  seq->still_picture = (oxcf->limit == 1);
  seq->reduced_still_picture_hdr = seq->still_picture;
  seq->reduced_still_picture_hdr &= !oxcf->full_still_picture_hdr;
  seq->force_screen_content_tools = 2;
  seq->force_integer_mv = 2;
  seq->enable_order_hint = oxcf->enable_order_hint;
  seq->frame_id_numbers_present_flag = oxcf->large_scale_tile;
  if (seq->still_picture && seq->reduced_still_picture_hdr) {
    seq->enable_order_hint = 0;
    seq->frame_id_numbers_present_flag = 0;
    seq->force_screen_content_tools = 2;
    seq->force_integer_mv = 2;
  }
  seq->order_hint_bits_minus_1 =
      seq->enable_order_hint ? DEFAULT_EXPLICIT_ORDER_HINT_BITS - 1 : -1;

  seq->enable_dual_filter = oxcf->enable_dual_filter;
  seq->enable_jnt_comp = oxcf->enable_jnt_comp;
  seq->enable_jnt_comp &= seq->enable_order_hint;
  seq->enable_ref_frame_mvs = oxcf->enable_ref_frame_mvs;
  seq->enable_ref_frame_mvs &= seq->enable_order_hint;
  seq->enable_superres = oxcf->enable_superres;
  seq->enable_cdef = oxcf->enable_cdef;
  seq->enable_restoration = oxcf->enable_restoration;
  seq->enable_warped_motion = oxcf->enable_warped_motion;
  seq->enable_interintra_compound = 1;
  seq->enable_masked_compound = 1;
  seq->enable_intra_edge_filter = 1;
  seq->enable_filter_intra = 1;

  set_bitstream_level_tier(seq, cm, oxcf);

  if (seq->operating_points_cnt_minus_1 == 0) {
    seq->operating_point_idc[0] = 0;
  } else {
    // Set operating_point_idc[] such that for the i-th operating point the
    // first (operating_points_cnt-i) spatial layers and the first temporal
    // layer are decoded Note that highest quality operating point should come
    // first
    for (int i = 0; i < seq->operating_points_cnt_minus_1 + 1; i++)
      seq->operating_point_idc[i] =
          (~(~0u << (seq->operating_points_cnt_minus_1 + 1 - i)) << 8) | 1;
  }
}

static void init_config(struct AV1_COMP *cpi, AV1EncoderConfig *oxcf) {
  AV1_COMMON *const cm = &cpi->common;

  cpi->oxcf = *oxcf;
  cpi->framerate = oxcf->init_framerate;

  cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
  cm->use_highbitdepth = oxcf->use_highbitdepth;
  cm->color_primaries = oxcf->color_primaries;
  cm->transfer_characteristics = oxcf->transfer_characteristics;
  cm->matrix_coefficients = oxcf->matrix_coefficients;
  cm->seq_params.monochrome = oxcf->monochrome;
  cm->chroma_sample_position = oxcf->chroma_sample_position;
  cm->color_range = oxcf->color_range;
  cm->timing_info_present = oxcf->timing_info_present;
  cm->timing_info.num_units_in_display_tick =
      oxcf->timing_info.num_units_in_display_tick;
  cm->timing_info.time_scale = oxcf->timing_info.time_scale;
  cm->timing_info.equal_picture_interval =
      oxcf->timing_info.equal_picture_interval;
  cm->timing_info.num_ticks_per_picture =
      oxcf->timing_info.num_ticks_per_picture;

  cm->seq_params.display_model_info_present_flag =
      oxcf->display_model_info_present_flag;
  cm->seq_params.decoder_model_info_present_flag =
      oxcf->decoder_model_info_present_flag;
  if (oxcf->decoder_model_info_present_flag) {
    // set the decoder model parameters in schedule mode
    cm->buffer_model.num_units_in_decoding_tick =
        oxcf->buffer_model.num_units_in_decoding_tick;
    cm->buffer_removal_delay_present = 1;
    set_aom_dec_model_info(&cm->buffer_model);
    set_dec_model_op_parameters(&cm->op_params[0]);
  } else if (cm->timing_info_present &&
             cm->timing_info.equal_picture_interval &&
             !cm->seq_params.decoder_model_info_present_flag) {
    // set the decoder model parameters in resource availability mode
    set_resource_availability_parameters(&cm->op_params[0]);
  } else {
    cm->op_params[0].initial_display_delay =
        10;  // Default value (not signaled)
  }

  cm->width = oxcf->width;
  cm->height = oxcf->height;
  set_sb_size(&cm->seq_params,
              select_sb_size(cpi));  // set sb size before allocations
  alloc_compressor_data(cpi);

  update_film_grain_parameters(cpi, oxcf);

  // Single thread case: use counts in common.
  cpi->td.counts = &cpi->counts;

  // change includes all joint functionality
  av1_change_config(cpi, oxcf);

  cpi->static_mb_pct = 0;
  cpi->ref_frame_flags = 0;

  // Reset resize pending flags
  cpi->resize_pending_width = 0;
  cpi->resize_pending_height = 0;

  init_buffer_indices(cpi);
}

static void set_rc_buffer_sizes(RATE_CONTROL *rc,
                                const AV1EncoderConfig *oxcf) {
  const int64_t bandwidth = oxcf->target_bandwidth;
  const int64_t starting = oxcf->starting_buffer_level_ms;
  const int64_t optimal = oxcf->optimal_buffer_level_ms;
  const int64_t maximum = oxcf->maximum_buffer_size_ms;

  rc->starting_buffer_level = starting * bandwidth / 1000;
  rc->optimal_buffer_level =
      (optimal == 0) ? bandwidth / 8 : optimal * bandwidth / 1000;
  rc->maximum_buffer_size =
      (maximum == 0) ? bandwidth / 8 : maximum * bandwidth / 1000;
}

#define HIGHBD_BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX4DF, JSDAF, JSVAF) \
  cpi->fn_ptr[BT].sdf = SDF;                                           \
  cpi->fn_ptr[BT].sdaf = SDAF;                                         \
  cpi->fn_ptr[BT].vf = VF;                                             \
  cpi->fn_ptr[BT].svf = SVF;                                           \
  cpi->fn_ptr[BT].svaf = SVAF;                                         \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;                                     \
  cpi->fn_ptr[BT].jsdaf = JSDAF;                                       \
  cpi->fn_ptr[BT].jsvaf = JSVAF;

#define MAKE_BFP_SAD_WRAPPER(fnname)                                           \
  static unsigned int fnname##_bits8(const uint8_t *src_ptr,                   \
                                     int source_stride,                        \
                                     const uint8_t *ref_ptr, int ref_stride) { \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride);                \
  }                                                                            \
  static unsigned int fnname##_bits10(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride) {                                                        \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride) >> 2;           \
  }                                                                            \
  static unsigned int fnname##_bits12(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride) {                                                        \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride) >> 4;           \
  }

#define MAKE_BFP_SADAVG_WRAPPER(fnname)                                        \
  static unsigned int fnname##_bits8(                                          \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred);   \
  }                                                                            \
  static unsigned int fnname##_bits10(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred) >> \
           2;                                                                  \
  }                                                                            \
  static unsigned int fnname##_bits12(                                         \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,       \
      int ref_stride, const uint8_t *second_pred) {                            \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred) >> \
           4;                                                                  \
  }

#define MAKE_BFP_SAD4D_WRAPPER(fnname)                                        \
  static void fnname##_bits8(const uint8_t *src_ptr, int source_stride,       \
                             const uint8_t *const ref_ptr[], int ref_stride,  \
                             unsigned int *sad_array) {                       \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
  }                                                                           \
  static void fnname##_bits10(const uint8_t *src_ptr, int source_stride,      \
                              const uint8_t *const ref_ptr[], int ref_stride, \
                              unsigned int *sad_array) {                      \
    int i;                                                                    \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
    for (i = 0; i < 4; i++) sad_array[i] >>= 2;                               \
  }                                                                           \
  static void fnname##_bits12(const uint8_t *src_ptr, int source_stride,      \
                              const uint8_t *const ref_ptr[], int ref_stride, \
                              unsigned int *sad_array) {                      \
    int i;                                                                    \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);           \
    for (i = 0; i < 4; i++) sad_array[i] >>= 4;                               \
  }

#define MAKE_BFP_JSADAVG_WRAPPER(fnname)                                    \
  static unsigned int fnname##_bits8(                                       \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,    \
      int ref_stride, const uint8_t *second_pred,                           \
      const JNT_COMP_PARAMS *jcp_param) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred, \
                  jcp_param);                                               \
  }                                                                         \
  static unsigned int fnname##_bits10(                                      \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,    \
      int ref_stride, const uint8_t *second_pred,                           \
      const JNT_COMP_PARAMS *jcp_param) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred, \
                  jcp_param) >>                                             \
           2;                                                               \
  }                                                                         \
  static unsigned int fnname##_bits12(                                      \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr,    \
      int ref_stride, const uint8_t *second_pred,                           \
      const JNT_COMP_PARAMS *jcp_param) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride, second_pred, \
                  jcp_param) >>                                             \
           4;                                                               \
  }

MAKE_BFP_SAD_WRAPPER(aom_highbd_sad128x128)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad128x128_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad128x128x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad128x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad128x64_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad128x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x128)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x128_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x128x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad32x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad32x16_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x32)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x32_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x32x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x32)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x32_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x32x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad32x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad32x64_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad32x32)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad32x32_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x32x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x64_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x16_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x8_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x16_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x8_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x4)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x4_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x4x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad4x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad4x8_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad4x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad4x4)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad4x4_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad4x4x4d)

MAKE_BFP_SAD_WRAPPER(aom_highbd_sad4x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad4x16_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad4x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x4)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x4_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x4x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x32)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x32_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x32x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad32x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad32x8_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x64_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x16_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x16x4d)

MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad128x128_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad128x64_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad64x128_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad32x16_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad16x32_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad64x32_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad32x64_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad32x32_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad64x64_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad16x16_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad16x8_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad8x16_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad8x8_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad8x4_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad4x8_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad4x4_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad4x16_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad16x4_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad8x32_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad32x8_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad16x64_avg)
MAKE_BFP_JSADAVG_WRAPPER(aom_highbd_jnt_sad64x16_avg)

#define HIGHBD_MBFP(BT, MCSDF, MCSVF) \
  cpi->fn_ptr[BT].msdf = MCSDF;       \
  cpi->fn_ptr[BT].msvf = MCSVF;

#define MAKE_MBFP_COMPOUND_SAD_WRAPPER(fnname)                           \
  static unsigned int fnname##_bits8(                                    \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, \
      int ref_stride, const uint8_t *second_pred_ptr, const uint8_t *m,  \
      int m_stride, int invert_mask) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride,           \
                  second_pred_ptr, m, m_stride, invert_mask);            \
  }                                                                      \
  static unsigned int fnname##_bits10(                                   \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, \
      int ref_stride, const uint8_t *second_pred_ptr, const uint8_t *m,  \
      int m_stride, int invert_mask) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride,           \
                  second_pred_ptr, m, m_stride, invert_mask) >>          \
           2;                                                            \
  }                                                                      \
  static unsigned int fnname##_bits12(                                   \
      const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, \
      int ref_stride, const uint8_t *second_pred_ptr, const uint8_t *m,  \
      int m_stride, int invert_mask) {                                   \
    return fnname(src_ptr, source_stride, ref_ptr, ref_stride,           \
                  second_pred_ptr, m, m_stride, invert_mask) >>          \
           4;                                                            \
  }

MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad128x128)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad128x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x128)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x32)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x32)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x16)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x32)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x16)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x8)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad8x16)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad8x8)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad8x4)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad4x8)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad4x4)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad4x16)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x4)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad8x32)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x8)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x16)

#define HIGHBD_OBFP(BT, OSDF, OVF, OSVF) \
  cpi->fn_ptr[BT].osdf = OSDF;           \
  cpi->fn_ptr[BT].ovf = OVF;             \
  cpi->fn_ptr[BT].osvf = OSVF;

#define MAKE_OBFP_SAD_WRAPPER(fnname)                                     \
  static unsigned int fnname##_bits8(const uint8_t *ref, int ref_stride,  \
                                     const int32_t *wsrc,                 \
                                     const int32_t *msk) {                \
    return fnname(ref, ref_stride, wsrc, msk);                            \
  }                                                                       \
  static unsigned int fnname##_bits10(const uint8_t *ref, int ref_stride, \
                                      const int32_t *wsrc,                \
                                      const int32_t *msk) {               \
    return fnname(ref, ref_stride, wsrc, msk) >> 2;                       \
  }                                                                       \
  static unsigned int fnname##_bits12(const uint8_t *ref, int ref_stride, \
                                      const int32_t *wsrc,                \
                                      const int32_t *msk) {               \
    return fnname(ref, ref_stride, wsrc, msk) >> 4;                       \
  }

MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad128x128)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad128x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x128)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x32)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x32)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x16)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x32)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x16)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x8)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad8x16)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad8x8)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad8x4)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad4x8)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad4x4)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad4x16)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x4)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad8x32)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x8)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x16)

static void highbd_set_var_fns(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (cm->use_highbitdepth) {
    switch (cm->bit_depth) {
      case AOM_BITS_8:
        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits8,
                   aom_highbd_sad64x16_avg_bits8, aom_highbd_8_variance64x16,
                   aom_highbd_8_sub_pixel_variance64x16,
                   aom_highbd_8_sub_pixel_avg_variance64x16,
                   aom_highbd_sad64x16x4d_bits8,
                   aom_highbd_jnt_sad64x16_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance64x16)

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits8,
                   aom_highbd_sad16x64_avg_bits8, aom_highbd_8_variance16x64,
                   aom_highbd_8_sub_pixel_variance16x64,
                   aom_highbd_8_sub_pixel_avg_variance16x64,
                   aom_highbd_sad16x64x4d_bits8,
                   aom_highbd_jnt_sad16x64_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance16x64)

        HIGHBD_BFP(
            BLOCK_32X8, aom_highbd_sad32x8_bits8, aom_highbd_sad32x8_avg_bits8,
            aom_highbd_8_variance32x8, aom_highbd_8_sub_pixel_variance32x8,
            aom_highbd_8_sub_pixel_avg_variance32x8,
            aom_highbd_sad32x8x4d_bits8, aom_highbd_jnt_sad32x8_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance32x8)

        HIGHBD_BFP(
            BLOCK_8X32, aom_highbd_sad8x32_bits8, aom_highbd_sad8x32_avg_bits8,
            aom_highbd_8_variance8x32, aom_highbd_8_sub_pixel_variance8x32,
            aom_highbd_8_sub_pixel_avg_variance8x32,
            aom_highbd_sad8x32x4d_bits8, aom_highbd_jnt_sad8x32_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance8x32)

        HIGHBD_BFP(
            BLOCK_16X4, aom_highbd_sad16x4_bits8, aom_highbd_sad16x4_avg_bits8,
            aom_highbd_8_variance16x4, aom_highbd_8_sub_pixel_variance16x4,
            aom_highbd_8_sub_pixel_avg_variance16x4,
            aom_highbd_sad16x4x4d_bits8, aom_highbd_jnt_sad16x4_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance16x4)

        HIGHBD_BFP(
            BLOCK_4X16, aom_highbd_sad4x16_bits8, aom_highbd_sad4x16_avg_bits8,
            aom_highbd_8_variance4x16, aom_highbd_8_sub_pixel_variance4x16,
            aom_highbd_8_sub_pixel_avg_variance4x16,
            aom_highbd_sad4x16x4d_bits8, aom_highbd_jnt_sad4x16_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance4x16)

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits8,
                   aom_highbd_sad32x16_avg_bits8, aom_highbd_8_variance32x16,
                   aom_highbd_8_sub_pixel_variance32x16,
                   aom_highbd_8_sub_pixel_avg_variance32x16,
                   aom_highbd_sad32x16x4d_bits8,
                   aom_highbd_jnt_sad32x16_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance32x16)

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits8,
                   aom_highbd_sad16x32_avg_bits8, aom_highbd_8_variance16x32,
                   aom_highbd_8_sub_pixel_variance16x32,
                   aom_highbd_8_sub_pixel_avg_variance16x32,
                   aom_highbd_sad16x32x4d_bits8,
                   aom_highbd_jnt_sad16x32_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance16x32)

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits8,
                   aom_highbd_sad64x32_avg_bits8, aom_highbd_8_variance64x32,
                   aom_highbd_8_sub_pixel_variance64x32,
                   aom_highbd_8_sub_pixel_avg_variance64x32,
                   aom_highbd_sad64x32x4d_bits8,
                   aom_highbd_jnt_sad64x32_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance64x32)

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits8,
                   aom_highbd_sad32x64_avg_bits8, aom_highbd_8_variance32x64,
                   aom_highbd_8_sub_pixel_variance32x64,
                   aom_highbd_8_sub_pixel_avg_variance32x64,
                   aom_highbd_sad32x64x4d_bits8,
                   aom_highbd_jnt_sad32x64_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance32x64)

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits8,
                   aom_highbd_sad32x32_avg_bits8, aom_highbd_8_variance32x32,
                   aom_highbd_8_sub_pixel_variance32x32,
                   aom_highbd_8_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x4d_bits8,
                   aom_highbd_jnt_sad32x32_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance32x32)

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits8,
                   aom_highbd_sad64x64_avg_bits8, aom_highbd_8_variance64x64,
                   aom_highbd_8_sub_pixel_variance64x64,
                   aom_highbd_8_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x4d_bits8,
                   aom_highbd_jnt_sad64x64_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance64x64)

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits8,
                   aom_highbd_sad16x16_avg_bits8, aom_highbd_8_variance16x16,
                   aom_highbd_8_sub_pixel_variance16x16,
                   aom_highbd_8_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x4d_bits8,
                   aom_highbd_jnt_sad16x16_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance16x16)

        HIGHBD_BFP(
            BLOCK_16X8, aom_highbd_sad16x8_bits8, aom_highbd_sad16x8_avg_bits8,
            aom_highbd_8_variance16x8, aom_highbd_8_sub_pixel_variance16x8,
            aom_highbd_8_sub_pixel_avg_variance16x8,
            aom_highbd_sad16x8x4d_bits8, aom_highbd_jnt_sad16x8_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance16x8)

        HIGHBD_BFP(
            BLOCK_8X16, aom_highbd_sad8x16_bits8, aom_highbd_sad8x16_avg_bits8,
            aom_highbd_8_variance8x16, aom_highbd_8_sub_pixel_variance8x16,
            aom_highbd_8_sub_pixel_avg_variance8x16,
            aom_highbd_sad8x16x4d_bits8, aom_highbd_jnt_sad8x16_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance8x16)

        HIGHBD_BFP(BLOCK_8X8, aom_highbd_sad8x8_bits8,
                   aom_highbd_sad8x8_avg_bits8, aom_highbd_8_variance8x8,
                   aom_highbd_8_sub_pixel_variance8x8,
                   aom_highbd_8_sub_pixel_avg_variance8x8,
                   aom_highbd_sad8x8x4d_bits8, aom_highbd_jnt_sad8x8_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance8x8)

        HIGHBD_BFP(BLOCK_8X4, aom_highbd_sad8x4_bits8,
                   aom_highbd_sad8x4_avg_bits8, aom_highbd_8_variance8x4,
                   aom_highbd_8_sub_pixel_variance8x4,
                   aom_highbd_8_sub_pixel_avg_variance8x4,
                   aom_highbd_sad8x4x4d_bits8, aom_highbd_jnt_sad8x4_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance8x4)

        HIGHBD_BFP(BLOCK_4X8, aom_highbd_sad4x8_bits8,
                   aom_highbd_sad4x8_avg_bits8, aom_highbd_8_variance4x8,
                   aom_highbd_8_sub_pixel_variance4x8,
                   aom_highbd_8_sub_pixel_avg_variance4x8,
                   aom_highbd_sad4x8x4d_bits8, aom_highbd_jnt_sad4x8_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance4x8)

        HIGHBD_BFP(BLOCK_4X4, aom_highbd_sad4x4_bits8,
                   aom_highbd_sad4x4_avg_bits8, aom_highbd_8_variance4x4,
                   aom_highbd_8_sub_pixel_variance4x4,
                   aom_highbd_8_sub_pixel_avg_variance4x4,
                   aom_highbd_sad4x4x4d_bits8, aom_highbd_jnt_sad4x4_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance4x4)

        HIGHBD_BFP(
            BLOCK_128X128, aom_highbd_sad128x128_bits8,
            aom_highbd_sad128x128_avg_bits8, aom_highbd_8_variance128x128,
            aom_highbd_8_sub_pixel_variance128x128,
            aom_highbd_8_sub_pixel_avg_variance128x128,
            aom_highbd_sad128x128x4d_bits8, aom_highbd_jnt_sad128x128_avg_bits8,
            aom_highbd_8_jnt_sub_pixel_avg_variance128x128)

        HIGHBD_BFP(BLOCK_128X64, aom_highbd_sad128x64_bits8,
                   aom_highbd_sad128x64_avg_bits8, aom_highbd_8_variance128x64,
                   aom_highbd_8_sub_pixel_variance128x64,
                   aom_highbd_8_sub_pixel_avg_variance128x64,
                   aom_highbd_sad128x64x4d_bits8,
                   aom_highbd_jnt_sad128x64_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance128x64)

        HIGHBD_BFP(BLOCK_64X128, aom_highbd_sad64x128_bits8,
                   aom_highbd_sad64x128_avg_bits8, aom_highbd_8_variance64x128,
                   aom_highbd_8_sub_pixel_variance64x128,
                   aom_highbd_8_sub_pixel_avg_variance64x128,
                   aom_highbd_sad64x128x4d_bits8,
                   aom_highbd_jnt_sad64x128_avg_bits8,
                   aom_highbd_8_jnt_sub_pixel_avg_variance64x128)

        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits8,
                    aom_highbd_8_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits8,
                    aom_highbd_8_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits8,
                    aom_highbd_8_masked_sub_pixel_variance64x128)
        HIGHBD_MBFP(BLOCK_64X64, aom_highbd_masked_sad64x64_bits8,
                    aom_highbd_8_masked_sub_pixel_variance64x64)
        HIGHBD_MBFP(BLOCK_64X32, aom_highbd_masked_sad64x32_bits8,
                    aom_highbd_8_masked_sub_pixel_variance64x32)
        HIGHBD_MBFP(BLOCK_32X64, aom_highbd_masked_sad32x64_bits8,
                    aom_highbd_8_masked_sub_pixel_variance32x64)
        HIGHBD_MBFP(BLOCK_32X32, aom_highbd_masked_sad32x32_bits8,
                    aom_highbd_8_masked_sub_pixel_variance32x32)
        HIGHBD_MBFP(BLOCK_32X16, aom_highbd_masked_sad32x16_bits8,
                    aom_highbd_8_masked_sub_pixel_variance32x16)
        HIGHBD_MBFP(BLOCK_16X32, aom_highbd_masked_sad16x32_bits8,
                    aom_highbd_8_masked_sub_pixel_variance16x32)
        HIGHBD_MBFP(BLOCK_16X16, aom_highbd_masked_sad16x16_bits8,
                    aom_highbd_8_masked_sub_pixel_variance16x16)
        HIGHBD_MBFP(BLOCK_8X16, aom_highbd_masked_sad8x16_bits8,
                    aom_highbd_8_masked_sub_pixel_variance8x16)
        HIGHBD_MBFP(BLOCK_16X8, aom_highbd_masked_sad16x8_bits8,
                    aom_highbd_8_masked_sub_pixel_variance16x8)
        HIGHBD_MBFP(BLOCK_8X8, aom_highbd_masked_sad8x8_bits8,
                    aom_highbd_8_masked_sub_pixel_variance8x8)
        HIGHBD_MBFP(BLOCK_4X8, aom_highbd_masked_sad4x8_bits8,
                    aom_highbd_8_masked_sub_pixel_variance4x8)
        HIGHBD_MBFP(BLOCK_8X4, aom_highbd_masked_sad8x4_bits8,
                    aom_highbd_8_masked_sub_pixel_variance8x4)
        HIGHBD_MBFP(BLOCK_4X4, aom_highbd_masked_sad4x4_bits8,
                    aom_highbd_8_masked_sub_pixel_variance4x4)
        HIGHBD_MBFP(BLOCK_64X16, aom_highbd_masked_sad64x16_bits8,
                    aom_highbd_8_masked_sub_pixel_variance64x16)
        HIGHBD_MBFP(BLOCK_16X64, aom_highbd_masked_sad16x64_bits8,
                    aom_highbd_8_masked_sub_pixel_variance16x64)
        HIGHBD_MBFP(BLOCK_32X8, aom_highbd_masked_sad32x8_bits8,
                    aom_highbd_8_masked_sub_pixel_variance32x8)
        HIGHBD_MBFP(BLOCK_8X32, aom_highbd_masked_sad8x32_bits8,
                    aom_highbd_8_masked_sub_pixel_variance8x32)
        HIGHBD_MBFP(BLOCK_16X4, aom_highbd_masked_sad16x4_bits8,
                    aom_highbd_8_masked_sub_pixel_variance16x4)
        HIGHBD_MBFP(BLOCK_4X16, aom_highbd_masked_sad4x16_bits8,
                    aom_highbd_8_masked_sub_pixel_variance4x16)
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits8,
                    aom_highbd_obmc_variance128x128,
                    aom_highbd_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits8,
                    aom_highbd_obmc_variance128x64,
                    aom_highbd_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits8,
                    aom_highbd_obmc_variance64x128,
                    aom_highbd_obmc_sub_pixel_variance64x128)
        HIGHBD_OBFP(BLOCK_64X64, aom_highbd_obmc_sad64x64_bits8,
                    aom_highbd_obmc_variance64x64,
                    aom_highbd_obmc_sub_pixel_variance64x64)
        HIGHBD_OBFP(BLOCK_64X32, aom_highbd_obmc_sad64x32_bits8,
                    aom_highbd_obmc_variance64x32,
                    aom_highbd_obmc_sub_pixel_variance64x32)
        HIGHBD_OBFP(BLOCK_32X64, aom_highbd_obmc_sad32x64_bits8,
                    aom_highbd_obmc_variance32x64,
                    aom_highbd_obmc_sub_pixel_variance32x64)
        HIGHBD_OBFP(BLOCK_32X32, aom_highbd_obmc_sad32x32_bits8,
                    aom_highbd_obmc_variance32x32,
                    aom_highbd_obmc_sub_pixel_variance32x32)
        HIGHBD_OBFP(BLOCK_32X16, aom_highbd_obmc_sad32x16_bits8,
                    aom_highbd_obmc_variance32x16,
                    aom_highbd_obmc_sub_pixel_variance32x16)
        HIGHBD_OBFP(BLOCK_16X32, aom_highbd_obmc_sad16x32_bits8,
                    aom_highbd_obmc_variance16x32,
                    aom_highbd_obmc_sub_pixel_variance16x32)
        HIGHBD_OBFP(BLOCK_16X16, aom_highbd_obmc_sad16x16_bits8,
                    aom_highbd_obmc_variance16x16,
                    aom_highbd_obmc_sub_pixel_variance16x16)
        HIGHBD_OBFP(BLOCK_8X16, aom_highbd_obmc_sad8x16_bits8,
                    aom_highbd_obmc_variance8x16,
                    aom_highbd_obmc_sub_pixel_variance8x16)
        HIGHBD_OBFP(BLOCK_16X8, aom_highbd_obmc_sad16x8_bits8,
                    aom_highbd_obmc_variance16x8,
                    aom_highbd_obmc_sub_pixel_variance16x8)
        HIGHBD_OBFP(BLOCK_8X8, aom_highbd_obmc_sad8x8_bits8,
                    aom_highbd_obmc_variance8x8,
                    aom_highbd_obmc_sub_pixel_variance8x8)
        HIGHBD_OBFP(BLOCK_4X8, aom_highbd_obmc_sad4x8_bits8,
                    aom_highbd_obmc_variance4x8,
                    aom_highbd_obmc_sub_pixel_variance4x8)
        HIGHBD_OBFP(BLOCK_8X4, aom_highbd_obmc_sad8x4_bits8,
                    aom_highbd_obmc_variance8x4,
                    aom_highbd_obmc_sub_pixel_variance8x4)
        HIGHBD_OBFP(BLOCK_4X4, aom_highbd_obmc_sad4x4_bits8,
                    aom_highbd_obmc_variance4x4,
                    aom_highbd_obmc_sub_pixel_variance4x4)
        HIGHBD_OBFP(BLOCK_64X16, aom_highbd_obmc_sad64x16_bits8,
                    aom_highbd_obmc_variance64x16,
                    aom_highbd_obmc_sub_pixel_variance64x16)
        HIGHBD_OBFP(BLOCK_16X64, aom_highbd_obmc_sad16x64_bits8,
                    aom_highbd_obmc_variance16x64,
                    aom_highbd_obmc_sub_pixel_variance16x64)
        HIGHBD_OBFP(BLOCK_32X8, aom_highbd_obmc_sad32x8_bits8,
                    aom_highbd_obmc_variance32x8,
                    aom_highbd_obmc_sub_pixel_variance32x8)
        HIGHBD_OBFP(BLOCK_8X32, aom_highbd_obmc_sad8x32_bits8,
                    aom_highbd_obmc_variance8x32,
                    aom_highbd_obmc_sub_pixel_variance8x32)
        HIGHBD_OBFP(BLOCK_16X4, aom_highbd_obmc_sad16x4_bits8,
                    aom_highbd_obmc_variance16x4,
                    aom_highbd_obmc_sub_pixel_variance16x4)
        HIGHBD_OBFP(BLOCK_4X16, aom_highbd_obmc_sad4x16_bits8,
                    aom_highbd_obmc_variance4x16,
                    aom_highbd_obmc_sub_pixel_variance4x16)
        break;

      case AOM_BITS_10:
        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits10,
                   aom_highbd_sad64x16_avg_bits10, aom_highbd_10_variance64x16,
                   aom_highbd_10_sub_pixel_variance64x16,
                   aom_highbd_10_sub_pixel_avg_variance64x16,
                   aom_highbd_sad64x16x4d_bits10,
                   aom_highbd_jnt_sad64x16_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance64x16);

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits10,
                   aom_highbd_sad16x64_avg_bits10, aom_highbd_10_variance16x64,
                   aom_highbd_10_sub_pixel_variance16x64,
                   aom_highbd_10_sub_pixel_avg_variance16x64,
                   aom_highbd_sad16x64x4d_bits10,
                   aom_highbd_jnt_sad16x64_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance16x64);

        HIGHBD_BFP(BLOCK_32X8, aom_highbd_sad32x8_bits10,
                   aom_highbd_sad32x8_avg_bits10, aom_highbd_10_variance32x8,
                   aom_highbd_10_sub_pixel_variance32x8,
                   aom_highbd_10_sub_pixel_avg_variance32x8,
                   aom_highbd_sad32x8x4d_bits10,
                   aom_highbd_jnt_sad32x8_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance32x8);

        HIGHBD_BFP(BLOCK_8X32, aom_highbd_sad8x32_bits10,
                   aom_highbd_sad8x32_avg_bits10, aom_highbd_10_variance8x32,
                   aom_highbd_10_sub_pixel_variance8x32,
                   aom_highbd_10_sub_pixel_avg_variance8x32,
                   aom_highbd_sad8x32x4d_bits10,
                   aom_highbd_jnt_sad8x32_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance8x32);

        HIGHBD_BFP(BLOCK_16X4, aom_highbd_sad16x4_bits10,
                   aom_highbd_sad16x4_avg_bits10, aom_highbd_10_variance16x4,
                   aom_highbd_10_sub_pixel_variance16x4,
                   aom_highbd_10_sub_pixel_avg_variance16x4,
                   aom_highbd_sad16x4x4d_bits10,
                   aom_highbd_jnt_sad16x4_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance16x4);

        HIGHBD_BFP(BLOCK_4X16, aom_highbd_sad4x16_bits10,
                   aom_highbd_sad4x16_avg_bits10, aom_highbd_10_variance4x16,
                   aom_highbd_10_sub_pixel_variance4x16,
                   aom_highbd_10_sub_pixel_avg_variance4x16,
                   aom_highbd_sad4x16x4d_bits10,
                   aom_highbd_jnt_sad4x16_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance4x16);

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits10,
                   aom_highbd_sad32x16_avg_bits10, aom_highbd_10_variance32x16,
                   aom_highbd_10_sub_pixel_variance32x16,
                   aom_highbd_10_sub_pixel_avg_variance32x16,
                   aom_highbd_sad32x16x4d_bits10,
                   aom_highbd_jnt_sad32x16_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance32x16);

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits10,
                   aom_highbd_sad16x32_avg_bits10, aom_highbd_10_variance16x32,
                   aom_highbd_10_sub_pixel_variance16x32,
                   aom_highbd_10_sub_pixel_avg_variance16x32,
                   aom_highbd_sad16x32x4d_bits10,
                   aom_highbd_jnt_sad16x32_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance16x32);

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits10,
                   aom_highbd_sad64x32_avg_bits10, aom_highbd_10_variance64x32,
                   aom_highbd_10_sub_pixel_variance64x32,
                   aom_highbd_10_sub_pixel_avg_variance64x32,
                   aom_highbd_sad64x32x4d_bits10,
                   aom_highbd_jnt_sad64x32_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance64x32);

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits10,
                   aom_highbd_sad32x64_avg_bits10, aom_highbd_10_variance32x64,
                   aom_highbd_10_sub_pixel_variance32x64,
                   aom_highbd_10_sub_pixel_avg_variance32x64,
                   aom_highbd_sad32x64x4d_bits10,
                   aom_highbd_jnt_sad32x64_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance32x64);

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits10,
                   aom_highbd_sad32x32_avg_bits10, aom_highbd_10_variance32x32,
                   aom_highbd_10_sub_pixel_variance32x32,
                   aom_highbd_10_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x4d_bits10,
                   aom_highbd_jnt_sad32x32_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance32x32);

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits10,
                   aom_highbd_sad64x64_avg_bits10, aom_highbd_10_variance64x64,
                   aom_highbd_10_sub_pixel_variance64x64,
                   aom_highbd_10_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x4d_bits10,
                   aom_highbd_jnt_sad64x64_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance64x64);

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits10,
                   aom_highbd_sad16x16_avg_bits10, aom_highbd_10_variance16x16,
                   aom_highbd_10_sub_pixel_variance16x16,
                   aom_highbd_10_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x4d_bits10,
                   aom_highbd_jnt_sad16x16_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance16x16);

        HIGHBD_BFP(BLOCK_16X8, aom_highbd_sad16x8_bits10,
                   aom_highbd_sad16x8_avg_bits10, aom_highbd_10_variance16x8,
                   aom_highbd_10_sub_pixel_variance16x8,
                   aom_highbd_10_sub_pixel_avg_variance16x8,
                   aom_highbd_sad16x8x4d_bits10,
                   aom_highbd_jnt_sad16x8_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance16x8);

        HIGHBD_BFP(BLOCK_8X16, aom_highbd_sad8x16_bits10,
                   aom_highbd_sad8x16_avg_bits10, aom_highbd_10_variance8x16,
                   aom_highbd_10_sub_pixel_variance8x16,
                   aom_highbd_10_sub_pixel_avg_variance8x16,
                   aom_highbd_sad8x16x4d_bits10,
                   aom_highbd_jnt_sad8x16_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance8x16);

        HIGHBD_BFP(
            BLOCK_8X8, aom_highbd_sad8x8_bits10, aom_highbd_sad8x8_avg_bits10,
            aom_highbd_10_variance8x8, aom_highbd_10_sub_pixel_variance8x8,
            aom_highbd_10_sub_pixel_avg_variance8x8,
            aom_highbd_sad8x8x4d_bits10, aom_highbd_jnt_sad8x8_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance8x8);

        HIGHBD_BFP(
            BLOCK_8X4, aom_highbd_sad8x4_bits10, aom_highbd_sad8x4_avg_bits10,
            aom_highbd_10_variance8x4, aom_highbd_10_sub_pixel_variance8x4,
            aom_highbd_10_sub_pixel_avg_variance8x4,
            aom_highbd_sad8x4x4d_bits10, aom_highbd_jnt_sad8x4_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance8x4);

        HIGHBD_BFP(
            BLOCK_4X8, aom_highbd_sad4x8_bits10, aom_highbd_sad4x8_avg_bits10,
            aom_highbd_10_variance4x8, aom_highbd_10_sub_pixel_variance4x8,
            aom_highbd_10_sub_pixel_avg_variance4x8,
            aom_highbd_sad4x8x4d_bits10, aom_highbd_jnt_sad4x8_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance4x8);

        HIGHBD_BFP(
            BLOCK_4X4, aom_highbd_sad4x4_bits10, aom_highbd_sad4x4_avg_bits10,
            aom_highbd_10_variance4x4, aom_highbd_10_sub_pixel_variance4x4,
            aom_highbd_10_sub_pixel_avg_variance4x4,
            aom_highbd_sad4x4x4d_bits10, aom_highbd_jnt_sad4x4_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance4x4);

        HIGHBD_BFP(BLOCK_128X128, aom_highbd_sad128x128_bits10,
                   aom_highbd_sad128x128_avg_bits10,
                   aom_highbd_10_variance128x128,
                   aom_highbd_10_sub_pixel_variance128x128,
                   aom_highbd_10_sub_pixel_avg_variance128x128,
                   aom_highbd_sad128x128x4d_bits10,
                   aom_highbd_jnt_sad128x128_avg_bits10,
                   aom_highbd_10_jnt_sub_pixel_avg_variance128x128);

        HIGHBD_BFP(
            BLOCK_128X64, aom_highbd_sad128x64_bits10,
            aom_highbd_sad128x64_avg_bits10, aom_highbd_10_variance128x64,
            aom_highbd_10_sub_pixel_variance128x64,
            aom_highbd_10_sub_pixel_avg_variance128x64,
            aom_highbd_sad128x64x4d_bits10, aom_highbd_jnt_sad128x64_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance128x64);

        HIGHBD_BFP(
            BLOCK_64X128, aom_highbd_sad64x128_bits10,
            aom_highbd_sad64x128_avg_bits10, aom_highbd_10_variance64x128,
            aom_highbd_10_sub_pixel_variance64x128,
            aom_highbd_10_sub_pixel_avg_variance64x128,
            aom_highbd_sad64x128x4d_bits10, aom_highbd_jnt_sad64x128_avg_bits10,
            aom_highbd_10_jnt_sub_pixel_avg_variance64x128);

        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits10,
                    aom_highbd_10_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits10,
                    aom_highbd_10_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits10,
                    aom_highbd_10_masked_sub_pixel_variance64x128)
        HIGHBD_MBFP(BLOCK_64X64, aom_highbd_masked_sad64x64_bits10,
                    aom_highbd_10_masked_sub_pixel_variance64x64)
        HIGHBD_MBFP(BLOCK_64X32, aom_highbd_masked_sad64x32_bits10,
                    aom_highbd_10_masked_sub_pixel_variance64x32)
        HIGHBD_MBFP(BLOCK_32X64, aom_highbd_masked_sad32x64_bits10,
                    aom_highbd_10_masked_sub_pixel_variance32x64)
        HIGHBD_MBFP(BLOCK_32X32, aom_highbd_masked_sad32x32_bits10,
                    aom_highbd_10_masked_sub_pixel_variance32x32)
        HIGHBD_MBFP(BLOCK_32X16, aom_highbd_masked_sad32x16_bits10,
                    aom_highbd_10_masked_sub_pixel_variance32x16)
        HIGHBD_MBFP(BLOCK_16X32, aom_highbd_masked_sad16x32_bits10,
                    aom_highbd_10_masked_sub_pixel_variance16x32)
        HIGHBD_MBFP(BLOCK_16X16, aom_highbd_masked_sad16x16_bits10,
                    aom_highbd_10_masked_sub_pixel_variance16x16)
        HIGHBD_MBFP(BLOCK_8X16, aom_highbd_masked_sad8x16_bits10,
                    aom_highbd_10_masked_sub_pixel_variance8x16)
        HIGHBD_MBFP(BLOCK_16X8, aom_highbd_masked_sad16x8_bits10,
                    aom_highbd_10_masked_sub_pixel_variance16x8)
        HIGHBD_MBFP(BLOCK_8X8, aom_highbd_masked_sad8x8_bits10,
                    aom_highbd_10_masked_sub_pixel_variance8x8)
        HIGHBD_MBFP(BLOCK_4X8, aom_highbd_masked_sad4x8_bits10,
                    aom_highbd_10_masked_sub_pixel_variance4x8)
        HIGHBD_MBFP(BLOCK_8X4, aom_highbd_masked_sad8x4_bits10,
                    aom_highbd_10_masked_sub_pixel_variance8x4)
        HIGHBD_MBFP(BLOCK_4X4, aom_highbd_masked_sad4x4_bits10,
                    aom_highbd_10_masked_sub_pixel_variance4x4)
        HIGHBD_MBFP(BLOCK_64X16, aom_highbd_masked_sad64x16_bits10,
                    aom_highbd_10_masked_sub_pixel_variance64x16)
        HIGHBD_MBFP(BLOCK_16X64, aom_highbd_masked_sad16x64_bits10,
                    aom_highbd_10_masked_sub_pixel_variance16x64)
        HIGHBD_MBFP(BLOCK_32X8, aom_highbd_masked_sad32x8_bits10,
                    aom_highbd_10_masked_sub_pixel_variance32x8)
        HIGHBD_MBFP(BLOCK_8X32, aom_highbd_masked_sad8x32_bits10,
                    aom_highbd_10_masked_sub_pixel_variance8x32)
        HIGHBD_MBFP(BLOCK_16X4, aom_highbd_masked_sad16x4_bits10,
                    aom_highbd_10_masked_sub_pixel_variance16x4)
        HIGHBD_MBFP(BLOCK_4X16, aom_highbd_masked_sad4x16_bits10,
                    aom_highbd_10_masked_sub_pixel_variance4x16)
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits10,
                    aom_highbd_10_obmc_variance128x128,
                    aom_highbd_10_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits10,
                    aom_highbd_10_obmc_variance128x64,
                    aom_highbd_10_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits10,
                    aom_highbd_10_obmc_variance64x128,
                    aom_highbd_10_obmc_sub_pixel_variance64x128)
        HIGHBD_OBFP(BLOCK_64X64, aom_highbd_obmc_sad64x64_bits10,
                    aom_highbd_10_obmc_variance64x64,
                    aom_highbd_10_obmc_sub_pixel_variance64x64)
        HIGHBD_OBFP(BLOCK_64X32, aom_highbd_obmc_sad64x32_bits10,
                    aom_highbd_10_obmc_variance64x32,
                    aom_highbd_10_obmc_sub_pixel_variance64x32)
        HIGHBD_OBFP(BLOCK_32X64, aom_highbd_obmc_sad32x64_bits10,
                    aom_highbd_10_obmc_variance32x64,
                    aom_highbd_10_obmc_sub_pixel_variance32x64)
        HIGHBD_OBFP(BLOCK_32X32, aom_highbd_obmc_sad32x32_bits10,
                    aom_highbd_10_obmc_variance32x32,
                    aom_highbd_10_obmc_sub_pixel_variance32x32)
        HIGHBD_OBFP(BLOCK_32X16, aom_highbd_obmc_sad32x16_bits10,
                    aom_highbd_10_obmc_variance32x16,
                    aom_highbd_10_obmc_sub_pixel_variance32x16)
        HIGHBD_OBFP(BLOCK_16X32, aom_highbd_obmc_sad16x32_bits10,
                    aom_highbd_10_obmc_variance16x32,
                    aom_highbd_10_obmc_sub_pixel_variance16x32)
        HIGHBD_OBFP(BLOCK_16X16, aom_highbd_obmc_sad16x16_bits10,
                    aom_highbd_10_obmc_variance16x16,
                    aom_highbd_10_obmc_sub_pixel_variance16x16)
        HIGHBD_OBFP(BLOCK_8X16, aom_highbd_obmc_sad8x16_bits10,
                    aom_highbd_10_obmc_variance8x16,
                    aom_highbd_10_obmc_sub_pixel_variance8x16)
        HIGHBD_OBFP(BLOCK_16X8, aom_highbd_obmc_sad16x8_bits10,
                    aom_highbd_10_obmc_variance16x8,
                    aom_highbd_10_obmc_sub_pixel_variance16x8)
        HIGHBD_OBFP(BLOCK_8X8, aom_highbd_obmc_sad8x8_bits10,
                    aom_highbd_10_obmc_variance8x8,
                    aom_highbd_10_obmc_sub_pixel_variance8x8)
        HIGHBD_OBFP(BLOCK_4X8, aom_highbd_obmc_sad4x8_bits10,
                    aom_highbd_10_obmc_variance4x8,
                    aom_highbd_10_obmc_sub_pixel_variance4x8)
        HIGHBD_OBFP(BLOCK_8X4, aom_highbd_obmc_sad8x4_bits10,
                    aom_highbd_10_obmc_variance8x4,
                    aom_highbd_10_obmc_sub_pixel_variance8x4)
        HIGHBD_OBFP(BLOCK_4X4, aom_highbd_obmc_sad4x4_bits10,
                    aom_highbd_10_obmc_variance4x4,
                    aom_highbd_10_obmc_sub_pixel_variance4x4)

        HIGHBD_OBFP(BLOCK_64X16, aom_highbd_obmc_sad64x16_bits10,
                    aom_highbd_10_obmc_variance64x16,
                    aom_highbd_10_obmc_sub_pixel_variance64x16)

        HIGHBD_OBFP(BLOCK_16X64, aom_highbd_obmc_sad16x64_bits10,
                    aom_highbd_10_obmc_variance16x64,
                    aom_highbd_10_obmc_sub_pixel_variance16x64)

        HIGHBD_OBFP(BLOCK_32X8, aom_highbd_obmc_sad32x8_bits10,
                    aom_highbd_10_obmc_variance32x8,
                    aom_highbd_10_obmc_sub_pixel_variance32x8)

        HIGHBD_OBFP(BLOCK_8X32, aom_highbd_obmc_sad8x32_bits10,
                    aom_highbd_10_obmc_variance8x32,
                    aom_highbd_10_obmc_sub_pixel_variance8x32)

        HIGHBD_OBFP(BLOCK_16X4, aom_highbd_obmc_sad16x4_bits10,
                    aom_highbd_10_obmc_variance16x4,
                    aom_highbd_10_obmc_sub_pixel_variance16x4)

        HIGHBD_OBFP(BLOCK_4X16, aom_highbd_obmc_sad4x16_bits10,
                    aom_highbd_10_obmc_variance4x16,
                    aom_highbd_10_obmc_sub_pixel_variance4x16)
        break;

      case AOM_BITS_12:
        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits12,
                   aom_highbd_sad64x16_avg_bits12, aom_highbd_12_variance64x16,
                   aom_highbd_12_sub_pixel_variance64x16,
                   aom_highbd_12_sub_pixel_avg_variance64x16,
                   aom_highbd_sad64x16x4d_bits12,
                   aom_highbd_jnt_sad64x16_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance64x16);

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits12,
                   aom_highbd_sad16x64_avg_bits12, aom_highbd_12_variance16x64,
                   aom_highbd_12_sub_pixel_variance16x64,
                   aom_highbd_12_sub_pixel_avg_variance16x64,
                   aom_highbd_sad16x64x4d_bits12,
                   aom_highbd_jnt_sad16x64_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance16x64);

        HIGHBD_BFP(BLOCK_32X8, aom_highbd_sad32x8_bits12,
                   aom_highbd_sad32x8_avg_bits12, aom_highbd_12_variance32x8,
                   aom_highbd_12_sub_pixel_variance32x8,
                   aom_highbd_12_sub_pixel_avg_variance32x8,
                   aom_highbd_sad32x8x4d_bits12,
                   aom_highbd_jnt_sad32x8_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance32x8);

        HIGHBD_BFP(BLOCK_8X32, aom_highbd_sad8x32_bits12,
                   aom_highbd_sad8x32_avg_bits12, aom_highbd_12_variance8x32,
                   aom_highbd_12_sub_pixel_variance8x32,
                   aom_highbd_12_sub_pixel_avg_variance8x32,
                   aom_highbd_sad8x32x4d_bits12,
                   aom_highbd_jnt_sad8x32_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance8x32);

        HIGHBD_BFP(BLOCK_16X4, aom_highbd_sad16x4_bits12,
                   aom_highbd_sad16x4_avg_bits12, aom_highbd_12_variance16x4,
                   aom_highbd_12_sub_pixel_variance16x4,
                   aom_highbd_12_sub_pixel_avg_variance16x4,
                   aom_highbd_sad16x4x4d_bits12,
                   aom_highbd_jnt_sad16x4_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance16x4);

        HIGHBD_BFP(BLOCK_4X16, aom_highbd_sad4x16_bits12,
                   aom_highbd_sad4x16_avg_bits12, aom_highbd_12_variance4x16,
                   aom_highbd_12_sub_pixel_variance4x16,
                   aom_highbd_12_sub_pixel_avg_variance4x16,
                   aom_highbd_sad4x16x4d_bits12,
                   aom_highbd_jnt_sad4x16_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance4x16);

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits12,
                   aom_highbd_sad32x16_avg_bits12, aom_highbd_12_variance32x16,
                   aom_highbd_12_sub_pixel_variance32x16,
                   aom_highbd_12_sub_pixel_avg_variance32x16,
                   aom_highbd_sad32x16x4d_bits12,
                   aom_highbd_jnt_sad32x16_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance32x16);

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits12,
                   aom_highbd_sad16x32_avg_bits12, aom_highbd_12_variance16x32,
                   aom_highbd_12_sub_pixel_variance16x32,
                   aom_highbd_12_sub_pixel_avg_variance16x32,
                   aom_highbd_sad16x32x4d_bits12,
                   aom_highbd_jnt_sad16x32_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance16x32);

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits12,
                   aom_highbd_sad64x32_avg_bits12, aom_highbd_12_variance64x32,
                   aom_highbd_12_sub_pixel_variance64x32,
                   aom_highbd_12_sub_pixel_avg_variance64x32,
                   aom_highbd_sad64x32x4d_bits12,
                   aom_highbd_jnt_sad64x32_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance64x32);

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits12,
                   aom_highbd_sad32x64_avg_bits12, aom_highbd_12_variance32x64,
                   aom_highbd_12_sub_pixel_variance32x64,
                   aom_highbd_12_sub_pixel_avg_variance32x64,
                   aom_highbd_sad32x64x4d_bits12,
                   aom_highbd_jnt_sad32x64_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance32x64);

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits12,
                   aom_highbd_sad32x32_avg_bits12, aom_highbd_12_variance32x32,
                   aom_highbd_12_sub_pixel_variance32x32,
                   aom_highbd_12_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x4d_bits12,
                   aom_highbd_jnt_sad32x32_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance32x32);

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits12,
                   aom_highbd_sad64x64_avg_bits12, aom_highbd_12_variance64x64,
                   aom_highbd_12_sub_pixel_variance64x64,
                   aom_highbd_12_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x4d_bits12,
                   aom_highbd_jnt_sad64x64_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance64x64);

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits12,
                   aom_highbd_sad16x16_avg_bits12, aom_highbd_12_variance16x16,
                   aom_highbd_12_sub_pixel_variance16x16,
                   aom_highbd_12_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x4d_bits12,
                   aom_highbd_jnt_sad16x16_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance16x16);

        HIGHBD_BFP(BLOCK_16X8, aom_highbd_sad16x8_bits12,
                   aom_highbd_sad16x8_avg_bits12, aom_highbd_12_variance16x8,
                   aom_highbd_12_sub_pixel_variance16x8,
                   aom_highbd_12_sub_pixel_avg_variance16x8,
                   aom_highbd_sad16x8x4d_bits12,
                   aom_highbd_jnt_sad16x8_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance16x8);

        HIGHBD_BFP(BLOCK_8X16, aom_highbd_sad8x16_bits12,
                   aom_highbd_sad8x16_avg_bits12, aom_highbd_12_variance8x16,
                   aom_highbd_12_sub_pixel_variance8x16,
                   aom_highbd_12_sub_pixel_avg_variance8x16,
                   aom_highbd_sad8x16x4d_bits12,
                   aom_highbd_jnt_sad8x16_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance8x16);

        HIGHBD_BFP(
            BLOCK_8X8, aom_highbd_sad8x8_bits12, aom_highbd_sad8x8_avg_bits12,
            aom_highbd_12_variance8x8, aom_highbd_12_sub_pixel_variance8x8,
            aom_highbd_12_sub_pixel_avg_variance8x8,
            aom_highbd_sad8x8x4d_bits12, aom_highbd_jnt_sad8x8_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance8x8);

        HIGHBD_BFP(
            BLOCK_8X4, aom_highbd_sad8x4_bits12, aom_highbd_sad8x4_avg_bits12,
            aom_highbd_12_variance8x4, aom_highbd_12_sub_pixel_variance8x4,
            aom_highbd_12_sub_pixel_avg_variance8x4,
            aom_highbd_sad8x4x4d_bits12, aom_highbd_jnt_sad8x4_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance8x4);

        HIGHBD_BFP(
            BLOCK_4X8, aom_highbd_sad4x8_bits12, aom_highbd_sad4x8_avg_bits12,
            aom_highbd_12_variance4x8, aom_highbd_12_sub_pixel_variance4x8,
            aom_highbd_12_sub_pixel_avg_variance4x8,
            aom_highbd_sad4x8x4d_bits12, aom_highbd_jnt_sad4x8_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance4x8);

        HIGHBD_BFP(
            BLOCK_4X4, aom_highbd_sad4x4_bits12, aom_highbd_sad4x4_avg_bits12,
            aom_highbd_12_variance4x4, aom_highbd_12_sub_pixel_variance4x4,
            aom_highbd_12_sub_pixel_avg_variance4x4,
            aom_highbd_sad4x4x4d_bits12, aom_highbd_jnt_sad4x4_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance4x4);

        HIGHBD_BFP(BLOCK_128X128, aom_highbd_sad128x128_bits12,
                   aom_highbd_sad128x128_avg_bits12,
                   aom_highbd_12_variance128x128,
                   aom_highbd_12_sub_pixel_variance128x128,
                   aom_highbd_12_sub_pixel_avg_variance128x128,
                   aom_highbd_sad128x128x4d_bits12,
                   aom_highbd_jnt_sad128x128_avg_bits12,
                   aom_highbd_12_jnt_sub_pixel_avg_variance128x128);

        HIGHBD_BFP(
            BLOCK_128X64, aom_highbd_sad128x64_bits12,
            aom_highbd_sad128x64_avg_bits12, aom_highbd_12_variance128x64,
            aom_highbd_12_sub_pixel_variance128x64,
            aom_highbd_12_sub_pixel_avg_variance128x64,
            aom_highbd_sad128x64x4d_bits12, aom_highbd_jnt_sad128x64_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance128x64);

        HIGHBD_BFP(
            BLOCK_64X128, aom_highbd_sad64x128_bits12,
            aom_highbd_sad64x128_avg_bits12, aom_highbd_12_variance64x128,
            aom_highbd_12_sub_pixel_variance64x128,
            aom_highbd_12_sub_pixel_avg_variance64x128,
            aom_highbd_sad64x128x4d_bits12, aom_highbd_jnt_sad64x128_avg_bits12,
            aom_highbd_12_jnt_sub_pixel_avg_variance64x128);

        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits12,
                    aom_highbd_12_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits12,
                    aom_highbd_12_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits12,
                    aom_highbd_12_masked_sub_pixel_variance64x128)
        HIGHBD_MBFP(BLOCK_64X64, aom_highbd_masked_sad64x64_bits12,
                    aom_highbd_12_masked_sub_pixel_variance64x64)
        HIGHBD_MBFP(BLOCK_64X32, aom_highbd_masked_sad64x32_bits12,
                    aom_highbd_12_masked_sub_pixel_variance64x32)
        HIGHBD_MBFP(BLOCK_32X64, aom_highbd_masked_sad32x64_bits12,
                    aom_highbd_12_masked_sub_pixel_variance32x64)
        HIGHBD_MBFP(BLOCK_32X32, aom_highbd_masked_sad32x32_bits12,
                    aom_highbd_12_masked_sub_pixel_variance32x32)
        HIGHBD_MBFP(BLOCK_32X16, aom_highbd_masked_sad32x16_bits12,
                    aom_highbd_12_masked_sub_pixel_variance32x16)
        HIGHBD_MBFP(BLOCK_16X32, aom_highbd_masked_sad16x32_bits12,
                    aom_highbd_12_masked_sub_pixel_variance16x32)
        HIGHBD_MBFP(BLOCK_16X16, aom_highbd_masked_sad16x16_bits12,
                    aom_highbd_12_masked_sub_pixel_variance16x16)
        HIGHBD_MBFP(BLOCK_8X16, aom_highbd_masked_sad8x16_bits12,
                    aom_highbd_12_masked_sub_pixel_variance8x16)
        HIGHBD_MBFP(BLOCK_16X8, aom_highbd_masked_sad16x8_bits12,
                    aom_highbd_12_masked_sub_pixel_variance16x8)
        HIGHBD_MBFP(BLOCK_8X8, aom_highbd_masked_sad8x8_bits12,
                    aom_highbd_12_masked_sub_pixel_variance8x8)
        HIGHBD_MBFP(BLOCK_4X8, aom_highbd_masked_sad4x8_bits12,
                    aom_highbd_12_masked_sub_pixel_variance4x8)
        HIGHBD_MBFP(BLOCK_8X4, aom_highbd_masked_sad8x4_bits12,
                    aom_highbd_12_masked_sub_pixel_variance8x4)
        HIGHBD_MBFP(BLOCK_4X4, aom_highbd_masked_sad4x4_bits12,
                    aom_highbd_12_masked_sub_pixel_variance4x4)
        HIGHBD_MBFP(BLOCK_64X16, aom_highbd_masked_sad64x16_bits12,
                    aom_highbd_12_masked_sub_pixel_variance64x16)
        HIGHBD_MBFP(BLOCK_16X64, aom_highbd_masked_sad16x64_bits12,
                    aom_highbd_12_masked_sub_pixel_variance16x64)
        HIGHBD_MBFP(BLOCK_32X8, aom_highbd_masked_sad32x8_bits12,
                    aom_highbd_12_masked_sub_pixel_variance32x8)
        HIGHBD_MBFP(BLOCK_8X32, aom_highbd_masked_sad8x32_bits12,
                    aom_highbd_12_masked_sub_pixel_variance8x32)
        HIGHBD_MBFP(BLOCK_16X4, aom_highbd_masked_sad16x4_bits12,
                    aom_highbd_12_masked_sub_pixel_variance16x4)
        HIGHBD_MBFP(BLOCK_4X16, aom_highbd_masked_sad4x16_bits12,
                    aom_highbd_12_masked_sub_pixel_variance4x16)
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits12,
                    aom_highbd_12_obmc_variance128x128,
                    aom_highbd_12_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits12,
                    aom_highbd_12_obmc_variance128x64,
                    aom_highbd_12_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits12,
                    aom_highbd_12_obmc_variance64x128,
                    aom_highbd_12_obmc_sub_pixel_variance64x128)
        HIGHBD_OBFP(BLOCK_64X64, aom_highbd_obmc_sad64x64_bits12,
                    aom_highbd_12_obmc_variance64x64,
                    aom_highbd_12_obmc_sub_pixel_variance64x64)
        HIGHBD_OBFP(BLOCK_64X32, aom_highbd_obmc_sad64x32_bits12,
                    aom_highbd_12_obmc_variance64x32,
                    aom_highbd_12_obmc_sub_pixel_variance64x32)
        HIGHBD_OBFP(BLOCK_32X64, aom_highbd_obmc_sad32x64_bits12,
                    aom_highbd_12_obmc_variance32x64,
                    aom_highbd_12_obmc_sub_pixel_variance32x64)
        HIGHBD_OBFP(BLOCK_32X32, aom_highbd_obmc_sad32x32_bits12,
                    aom_highbd_12_obmc_variance32x32,
                    aom_highbd_12_obmc_sub_pixel_variance32x32)
        HIGHBD_OBFP(BLOCK_32X16, aom_highbd_obmc_sad32x16_bits12,
                    aom_highbd_12_obmc_variance32x16,
                    aom_highbd_12_obmc_sub_pixel_variance32x16)
        HIGHBD_OBFP(BLOCK_16X32, aom_highbd_obmc_sad16x32_bits12,
                    aom_highbd_12_obmc_variance16x32,
                    aom_highbd_12_obmc_sub_pixel_variance16x32)
        HIGHBD_OBFP(BLOCK_16X16, aom_highbd_obmc_sad16x16_bits12,
                    aom_highbd_12_obmc_variance16x16,
                    aom_highbd_12_obmc_sub_pixel_variance16x16)
        HIGHBD_OBFP(BLOCK_8X16, aom_highbd_obmc_sad8x16_bits12,
                    aom_highbd_12_obmc_variance8x16,
                    aom_highbd_12_obmc_sub_pixel_variance8x16)
        HIGHBD_OBFP(BLOCK_16X8, aom_highbd_obmc_sad16x8_bits12,
                    aom_highbd_12_obmc_variance16x8,
                    aom_highbd_12_obmc_sub_pixel_variance16x8)
        HIGHBD_OBFP(BLOCK_8X8, aom_highbd_obmc_sad8x8_bits12,
                    aom_highbd_12_obmc_variance8x8,
                    aom_highbd_12_obmc_sub_pixel_variance8x8)
        HIGHBD_OBFP(BLOCK_4X8, aom_highbd_obmc_sad4x8_bits12,
                    aom_highbd_12_obmc_variance4x8,
                    aom_highbd_12_obmc_sub_pixel_variance4x8)
        HIGHBD_OBFP(BLOCK_8X4, aom_highbd_obmc_sad8x4_bits12,
                    aom_highbd_12_obmc_variance8x4,
                    aom_highbd_12_obmc_sub_pixel_variance8x4)
        HIGHBD_OBFP(BLOCK_4X4, aom_highbd_obmc_sad4x4_bits12,
                    aom_highbd_12_obmc_variance4x4,
                    aom_highbd_12_obmc_sub_pixel_variance4x4)
        HIGHBD_OBFP(BLOCK_64X16, aom_highbd_obmc_sad64x16_bits12,
                    aom_highbd_12_obmc_variance64x16,
                    aom_highbd_12_obmc_sub_pixel_variance64x16)
        HIGHBD_OBFP(BLOCK_16X64, aom_highbd_obmc_sad16x64_bits12,
                    aom_highbd_12_obmc_variance16x64,
                    aom_highbd_12_obmc_sub_pixel_variance16x64)
        HIGHBD_OBFP(BLOCK_32X8, aom_highbd_obmc_sad32x8_bits12,
                    aom_highbd_12_obmc_variance32x8,
                    aom_highbd_12_obmc_sub_pixel_variance32x8)
        HIGHBD_OBFP(BLOCK_8X32, aom_highbd_obmc_sad8x32_bits12,
                    aom_highbd_12_obmc_variance8x32,
                    aom_highbd_12_obmc_sub_pixel_variance8x32)
        HIGHBD_OBFP(BLOCK_16X4, aom_highbd_obmc_sad16x4_bits12,
                    aom_highbd_12_obmc_variance16x4,
                    aom_highbd_12_obmc_sub_pixel_variance16x4)
        HIGHBD_OBFP(BLOCK_4X16, aom_highbd_obmc_sad4x16_bits12,
                    aom_highbd_12_obmc_variance4x16,
                    aom_highbd_12_obmc_sub_pixel_variance4x16)
        break;

      default:
        assert(0 &&
               "cm->bit_depth should be AOM_BITS_8, "
               "AOM_BITS_10 or AOM_BITS_12");
    }
  }
}

static void realloc_segmentation_maps(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  // Create the encoder segmentation map and set all entries to 0
  aom_free(cpi->segmentation_map);
  CHECK_MEM_ERROR(cm, cpi->segmentation_map,
                  aom_calloc(cm->mi_rows * cm->mi_cols, 1));

  // Create a map used for cyclic background refresh.
  if (cpi->cyclic_refresh) av1_cyclic_refresh_free(cpi->cyclic_refresh);
  CHECK_MEM_ERROR(cm, cpi->cyclic_refresh,
                  av1_cyclic_refresh_alloc(cm->mi_rows, cm->mi_cols));

  // Create a map used to mark inactive areas.
  aom_free(cpi->active_map.map);
  CHECK_MEM_ERROR(cm, cpi->active_map.map,
                  aom_calloc(cm->mi_rows * cm->mi_cols, 1));
}

void av1_change_config(struct AV1_COMP *cpi, const AV1EncoderConfig *oxcf) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  RATE_CONTROL *const rc = &cpi->rc;
  MACROBLOCK *const x = &cpi->td.mb;

  if (cm->profile != oxcf->profile) cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
  cm->color_primaries = oxcf->color_primaries;
  cm->transfer_characteristics = oxcf->transfer_characteristics;
  cm->matrix_coefficients = oxcf->matrix_coefficients;
  cm->seq_params.monochrome = oxcf->monochrome;
  cm->chroma_sample_position = oxcf->chroma_sample_position;
  cm->color_range = oxcf->color_range;

  assert(IMPLIES(cm->profile <= PROFILE_1, cm->bit_depth <= AOM_BITS_10));

  cm->timing_info_present = oxcf->timing_info_present;
  cm->timing_info.num_units_in_display_tick =
      oxcf->timing_info.num_units_in_display_tick;
  cm->timing_info.time_scale = oxcf->timing_info.time_scale;
  cm->timing_info.equal_picture_interval =
      oxcf->timing_info.equal_picture_interval;
  cm->timing_info.num_ticks_per_picture =
      oxcf->timing_info.num_ticks_per_picture;

  cm->seq_params.display_model_info_present_flag =
      oxcf->display_model_info_present_flag;
  cm->seq_params.decoder_model_info_present_flag =
      oxcf->decoder_model_info_present_flag;
  if (oxcf->decoder_model_info_present_flag) {
    // set the decoder model parameters in schedule mode
    cm->buffer_model.num_units_in_decoding_tick =
        oxcf->buffer_model.num_units_in_decoding_tick;
    cm->buffer_removal_delay_present = 1;
    set_aom_dec_model_info(&cm->buffer_model);
    set_dec_model_op_parameters(&cm->op_params[0]);
  } else if (cm->timing_info_present &&
             cm->timing_info.equal_picture_interval &&
             !cm->seq_params.decoder_model_info_present_flag) {
    // set the decoder model parameters in resource availability mode
    set_resource_availability_parameters(&cm->op_params[0]);
  } else {
    cm->op_params[0].initial_display_delay =
        10;  // Default value (not signaled)
  }

  update_film_grain_parameters(cpi, oxcf);

  cpi->oxcf = *oxcf;
  cpi->common.options = oxcf->cfg;
  x->e_mbd.bd = (int)cm->bit_depth;
  x->e_mbd.global_motion = cm->global_motion;

  if ((oxcf->pass == 0) && (oxcf->rc_mode == AOM_Q)) {
    rc->baseline_gf_interval = FIXED_GF_INTERVAL;
  } else {
    rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
  }

  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
  cpi->refresh_bwd_ref_frame = 0;
  cpi->refresh_alt2_ref_frame = 0;

  cm->refresh_frame_context = (oxcf->frame_parallel_decoding_mode)
                                  ? REFRESH_FRAME_CONTEXT_DISABLED
                                  : REFRESH_FRAME_CONTEXT_BACKWARD;
  if (oxcf->large_scale_tile)
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

  if (x->palette_buffer == NULL) {
    CHECK_MEM_ERROR(cm, x->palette_buffer,
                    aom_memalign(16, sizeof(*x->palette_buffer)));
  }
  av1_reset_segment_features(cm);
  set_high_precision_mv(cpi, 1, 0);

  set_rc_buffer_sizes(rc, &cpi->oxcf);

  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = AOMMIN(rc->bits_off_target, rc->maximum_buffer_size);
  rc->buffer_level = AOMMIN(rc->buffer_level, rc->maximum_buffer_size);

  // Set up frame rate and related parameters rate control values.
  av1_new_framerate(cpi, cpi->framerate);

  // Set absolute upper and lower quality limits
  rc->worst_quality = cpi->oxcf.worst_allowed_q;
  rc->best_quality = cpi->oxcf.best_allowed_q;

  if (!oxcf->large_scale_tile)
    cm->interp_filter = cpi->sf.default_interp_filter;
  else
    cm->interp_filter = EIGHTTAP_REGULAR;

  cm->switchable_motion_mode = 1;

  if (cpi->oxcf.render_width > 0 && cpi->oxcf.render_height > 0) {
    cm->render_width = cpi->oxcf.render_width;
    cm->render_height = cpi->oxcf.render_height;
  } else {
    cm->render_width = cpi->oxcf.width;
    cm->render_height = cpi->oxcf.height;
  }
  cm->width = cpi->oxcf.width;
  cm->height = cpi->oxcf.height;

  int sb_size = cm->seq_params.sb_size;
  // Superblock size should not be updated after the first key frame.
  if (!cpi->seq_params_locked) {
    set_sb_size(&cm->seq_params, select_sb_size(cpi));
  }

  if (cpi->initial_width || sb_size != cm->seq_params.sb_size) {
    if (cm->width > cpi->initial_width || cm->height > cpi->initial_height ||
        cm->seq_params.sb_size != sb_size) {
      av1_free_context_buffers(cm);
      av1_free_pc_tree(&cpi->td, num_planes);
      alloc_compressor_data(cpi);
      realloc_segmentation_maps(cpi);
      cpi->initial_width = cpi->initial_height = 0;
    }
  }
  update_frame_size(cpi);

  cpi->alt_ref_source = NULL;
  rc->is_src_frame_alt_ref = 0;

  rc->is_bwd_ref_frame = 0;
  rc->is_last_bipred_frame = 0;
  rc->is_bipred_frame = 0;

  set_tile_info(cpi);

  cpi->ext_refresh_frame_flags_pending = 0;
  cpi->ext_refresh_frame_context_pending = 0;

  highbd_set_var_fns(cpi);

  // Init sequence level coding tools
  // This should not be called after the first key frame.
  if (!cpi->seq_params_locked) {
    cm->seq_params.operating_points_cnt_minus_1 =
        cm->number_spatial_layers > 1 ? cm->number_spatial_layers - 1 : 0;
    init_seq_coding_tools(&cm->seq_params, cm, oxcf);
  }
}

AV1_COMP *av1_create_compressor(AV1EncoderConfig *oxcf,
                                BufferPool *const pool) {
  unsigned int i;
  AV1_COMP *volatile const cpi = aom_memalign(32, sizeof(AV1_COMP));
  AV1_COMMON *volatile const cm = cpi != NULL ? &cpi->common : NULL;

  if (!cm) return NULL;

  av1_zero(*cpi);

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;
    av1_remove_compressor(cpi);
    return 0;
  }

  cm->error.setjmp = 1;
  cm->alloc_mi = enc_alloc_mi;
  cm->free_mi = enc_free_mi;
  cm->setup_mi = enc_setup_mi;

  CHECK_MEM_ERROR(cm, cm->fc,
                  (FRAME_CONTEXT *)aom_memalign(32, sizeof(*cm->fc)));
  CHECK_MEM_ERROR(cm, cm->frame_contexts,
                  (FRAME_CONTEXT *)aom_memalign(
                      32, FRAME_CONTEXTS * sizeof(*cm->frame_contexts)));
  memset(cm->fc, 0, sizeof(*cm->fc));
  memset(cm->frame_contexts, 0, FRAME_CONTEXTS * sizeof(*cm->frame_contexts));

  cpi->resize_state = 0;
  cpi->resize_avg_qp = 0;
  cpi->resize_buffer_underflow = 0;

  cpi->common.buffer_pool = pool;

  init_config(cpi, oxcf);
  av1_rc_init(&cpi->oxcf, oxcf->pass, &cpi->rc);

  cm->current_video_frame = 0;
  cpi->seq_params_locked = 0;
  cpi->partition_search_skippable_frame = 0;
  cpi->tile_data = NULL;
  cpi->last_show_frame_buf_idx = INVALID_IDX;

  realloc_segmentation_maps(cpi);

  memset(cpi->nmv_costs, 0, sizeof(cpi->nmv_costs));
  memset(cpi->nmv_costs_hp, 0, sizeof(cpi->nmv_costs_hp));

  for (i = 0; i < (sizeof(cpi->mbgraph_stats) / sizeof(cpi->mbgraph_stats[0]));
       i++) {
    CHECK_MEM_ERROR(
        cm, cpi->mbgraph_stats[i].mb_stats,
        aom_calloc(cm->MBs * sizeof(*cpi->mbgraph_stats[i].mb_stats), 1));
  }

#if CONFIG_FP_MB_STATS
  cpi->use_fp_mb_stats = 0;
  if (cpi->use_fp_mb_stats) {
    // a place holder used to store the first pass mb stats in the first pass
    CHECK_MEM_ERROR(cm, cpi->twopass.frame_mb_stats_buf,
                    aom_calloc(cm->MBs * sizeof(uint8_t), 1));
  } else {
    cpi->twopass.frame_mb_stats_buf = NULL;
  }
#endif

  cpi->refresh_alt_ref_frame = 0;

  cpi->b_calculate_psnr = CONFIG_INTERNAL_STATS;
#if CONFIG_INTERNAL_STATS
  cpi->b_calculate_blockiness = 1;
  cpi->b_calculate_consistency = 1;
  cpi->total_inconsistency = 0;
  cpi->psnr.worst = 100.0;
  cpi->worst_ssim = 100.0;

  cpi->count = 0;
  cpi->bytes = 0;

  if (cpi->b_calculate_psnr) {
    cpi->total_sq_error = 0;
    cpi->total_samples = 0;
    cpi->tot_recode_hits = 0;
    cpi->summed_quality = 0;
    cpi->summed_weights = 0;
  }

  cpi->fastssim.worst = 100.0;
  cpi->psnrhvs.worst = 100.0;

  if (cpi->b_calculate_blockiness) {
    cpi->total_blockiness = 0;
    cpi->worst_blockiness = 0.0;
  }

  if (cpi->b_calculate_consistency) {
    CHECK_MEM_ERROR(cm, cpi->ssim_vars,
                    aom_malloc(sizeof(*cpi->ssim_vars) * 4 *
                               cpi->common.mi_rows * cpi->common.mi_cols));
    cpi->worst_consistency = 100.0;
  }
#endif
#if CONFIG_ENTROPY_STATS
  av1_zero(aggregate_fc);
#endif  // CONFIG_ENTROPY_STATS

  cpi->first_time_stamp_ever = INT64_MAX;

  cpi->td.mb.nmvcost[0] = &cpi->nmv_costs[0][MV_MAX];
  cpi->td.mb.nmvcost[1] = &cpi->nmv_costs[1][MV_MAX];
  cpi->td.mb.nmvcost_hp[0] = &cpi->nmv_costs_hp[0][MV_MAX];
  cpi->td.mb.nmvcost_hp[1] = &cpi->nmv_costs_hp[1][MV_MAX];

#ifdef OUTPUT_YUV_SKINMAP
  yuv_skinmap_file = fopen("skinmap.yuv", "ab");
#endif
#ifdef OUTPUT_YUV_REC
  yuv_rec_file = fopen("rec.yuv", "wb");
#endif

  if (oxcf->pass == 1) {
    av1_init_first_pass(cpi);
  } else if (oxcf->pass == 2) {
    const size_t packet_sz = sizeof(FIRSTPASS_STATS);
    const int packets = (int)(oxcf->two_pass_stats_in.sz / packet_sz);

#if CONFIG_FP_MB_STATS
    if (cpi->use_fp_mb_stats) {
      const size_t psz = cpi->common.MBs * sizeof(uint8_t);
      const int ps = (int)(oxcf->firstpass_mb_stats_in.sz / psz);

      cpi->twopass.firstpass_mb_stats.mb_stats_start =
          oxcf->firstpass_mb_stats_in.buf;
      cpi->twopass.firstpass_mb_stats.mb_stats_end =
          cpi->twopass.firstpass_mb_stats.mb_stats_start +
          (ps - 1) * cpi->common.MBs * sizeof(uint8_t);
    }
#endif

    cpi->twopass.stats_in_start = oxcf->two_pass_stats_in.buf;
    cpi->twopass.stats_in = cpi->twopass.stats_in_start;
    cpi->twopass.stats_in_end = &cpi->twopass.stats_in[packets - 1];

    av1_init_second_pass(cpi);
  }

  CHECK_MEM_ERROR(
      cm, cpi->td.mb.above_pred_buf,
      (uint8_t *)aom_memalign(16, MAX_MB_PLANE * MAX_SB_SQUARE *
                                      sizeof(*cpi->td.mb.above_pred_buf)));
  CHECK_MEM_ERROR(
      cm, cpi->td.mb.left_pred_buf,
      (uint8_t *)aom_memalign(16, MAX_MB_PLANE * MAX_SB_SQUARE *
                                      sizeof(*cpi->td.mb.left_pred_buf)));

  CHECK_MEM_ERROR(cm, cpi->td.mb.wsrc_buf,
                  (int32_t *)aom_memalign(
                      16, MAX_SB_SQUARE * sizeof(*cpi->td.mb.wsrc_buf)));

  CHECK_MEM_ERROR(cm, cpi->td.mb.mask_buf,
                  (int32_t *)aom_memalign(
                      16, MAX_SB_SQUARE * sizeof(*cpi->td.mb.mask_buf)));

  av1_set_speed_features_framesize_independent(cpi);
  av1_set_speed_features_framesize_dependent(cpi);

#define BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX4DF, JSDAF, JSVAF) \
  cpi->fn_ptr[BT].sdf = SDF;                                    \
  cpi->fn_ptr[BT].sdaf = SDAF;                                  \
  cpi->fn_ptr[BT].vf = VF;                                      \
  cpi->fn_ptr[BT].svf = SVF;                                    \
  cpi->fn_ptr[BT].svaf = SVAF;                                  \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;                              \
  cpi->fn_ptr[BT].jsdaf = JSDAF;                                \
  cpi->fn_ptr[BT].jsvaf = JSVAF;

  BFP(BLOCK_4X16, aom_sad4x16, aom_sad4x16_avg, aom_variance4x16,
      aom_sub_pixel_variance4x16, aom_sub_pixel_avg_variance4x16,
      aom_sad4x16x4d, aom_jnt_sad4x16_avg, aom_jnt_sub_pixel_avg_variance4x16)

  BFP(BLOCK_16X4, aom_sad16x4, aom_sad16x4_avg, aom_variance16x4,
      aom_sub_pixel_variance16x4, aom_sub_pixel_avg_variance16x4,
      aom_sad16x4x4d, aom_jnt_sad16x4_avg, aom_jnt_sub_pixel_avg_variance16x4)

  BFP(BLOCK_8X32, aom_sad8x32, aom_sad8x32_avg, aom_variance8x32,
      aom_sub_pixel_variance8x32, aom_sub_pixel_avg_variance8x32,
      aom_sad8x32x4d, aom_jnt_sad8x32_avg, aom_jnt_sub_pixel_avg_variance8x32)

  BFP(BLOCK_32X8, aom_sad32x8, aom_sad32x8_avg, aom_variance32x8,
      aom_sub_pixel_variance32x8, aom_sub_pixel_avg_variance32x8,
      aom_sad32x8x4d, aom_jnt_sad32x8_avg, aom_jnt_sub_pixel_avg_variance32x8)

  BFP(BLOCK_16X64, aom_sad16x64, aom_sad16x64_avg, aom_variance16x64,
      aom_sub_pixel_variance16x64, aom_sub_pixel_avg_variance16x64,
      aom_sad16x64x4d, aom_jnt_sad16x64_avg,
      aom_jnt_sub_pixel_avg_variance16x64)

  BFP(BLOCK_64X16, aom_sad64x16, aom_sad64x16_avg, aom_variance64x16,
      aom_sub_pixel_variance64x16, aom_sub_pixel_avg_variance64x16,
      aom_sad64x16x4d, aom_jnt_sad64x16_avg,
      aom_jnt_sub_pixel_avg_variance64x16)

  BFP(BLOCK_128X128, aom_sad128x128, aom_sad128x128_avg, aom_variance128x128,
      aom_sub_pixel_variance128x128, aom_sub_pixel_avg_variance128x128,
      aom_sad128x128x4d, aom_jnt_sad128x128_avg,
      aom_jnt_sub_pixel_avg_variance128x128)

  BFP(BLOCK_128X64, aom_sad128x64, aom_sad128x64_avg, aom_variance128x64,
      aom_sub_pixel_variance128x64, aom_sub_pixel_avg_variance128x64,
      aom_sad128x64x4d, aom_jnt_sad128x64_avg,
      aom_jnt_sub_pixel_avg_variance128x64)

  BFP(BLOCK_64X128, aom_sad64x128, aom_sad64x128_avg, aom_variance64x128,
      aom_sub_pixel_variance64x128, aom_sub_pixel_avg_variance64x128,
      aom_sad64x128x4d, aom_jnt_sad64x128_avg,
      aom_jnt_sub_pixel_avg_variance64x128)

  BFP(BLOCK_32X16, aom_sad32x16, aom_sad32x16_avg, aom_variance32x16,
      aom_sub_pixel_variance32x16, aom_sub_pixel_avg_variance32x16,
      aom_sad32x16x4d, aom_jnt_sad32x16_avg,
      aom_jnt_sub_pixel_avg_variance32x16)

  BFP(BLOCK_16X32, aom_sad16x32, aom_sad16x32_avg, aom_variance16x32,
      aom_sub_pixel_variance16x32, aom_sub_pixel_avg_variance16x32,
      aom_sad16x32x4d, aom_jnt_sad16x32_avg,
      aom_jnt_sub_pixel_avg_variance16x32)

  BFP(BLOCK_64X32, aom_sad64x32, aom_sad64x32_avg, aom_variance64x32,
      aom_sub_pixel_variance64x32, aom_sub_pixel_avg_variance64x32,
      aom_sad64x32x4d, aom_jnt_sad64x32_avg,
      aom_jnt_sub_pixel_avg_variance64x32)

  BFP(BLOCK_32X64, aom_sad32x64, aom_sad32x64_avg, aom_variance32x64,
      aom_sub_pixel_variance32x64, aom_sub_pixel_avg_variance32x64,
      aom_sad32x64x4d, aom_jnt_sad32x64_avg,
      aom_jnt_sub_pixel_avg_variance32x64)

  BFP(BLOCK_32X32, aom_sad32x32, aom_sad32x32_avg, aom_variance32x32,
      aom_sub_pixel_variance32x32, aom_sub_pixel_avg_variance32x32,
      aom_sad32x32x4d, aom_jnt_sad32x32_avg,
      aom_jnt_sub_pixel_avg_variance32x32)

  BFP(BLOCK_64X64, aom_sad64x64, aom_sad64x64_avg, aom_variance64x64,
      aom_sub_pixel_variance64x64, aom_sub_pixel_avg_variance64x64,
      aom_sad64x64x4d, aom_jnt_sad64x64_avg,
      aom_jnt_sub_pixel_avg_variance64x64)

  BFP(BLOCK_16X16, aom_sad16x16, aom_sad16x16_avg, aom_variance16x16,
      aom_sub_pixel_variance16x16, aom_sub_pixel_avg_variance16x16,
      aom_sad16x16x4d, aom_jnt_sad16x16_avg,
      aom_jnt_sub_pixel_avg_variance16x16)

  BFP(BLOCK_16X8, aom_sad16x8, aom_sad16x8_avg, aom_variance16x8,
      aom_sub_pixel_variance16x8, aom_sub_pixel_avg_variance16x8,
      aom_sad16x8x4d, aom_jnt_sad16x8_avg, aom_jnt_sub_pixel_avg_variance16x8)

  BFP(BLOCK_8X16, aom_sad8x16, aom_sad8x16_avg, aom_variance8x16,
      aom_sub_pixel_variance8x16, aom_sub_pixel_avg_variance8x16,
      aom_sad8x16x4d, aom_jnt_sad8x16_avg, aom_jnt_sub_pixel_avg_variance8x16)

  BFP(BLOCK_8X8, aom_sad8x8, aom_sad8x8_avg, aom_variance8x8,
      aom_sub_pixel_variance8x8, aom_sub_pixel_avg_variance8x8, aom_sad8x8x4d,
      aom_jnt_sad8x8_avg, aom_jnt_sub_pixel_avg_variance8x8)

  BFP(BLOCK_8X4, aom_sad8x4, aom_sad8x4_avg, aom_variance8x4,
      aom_sub_pixel_variance8x4, aom_sub_pixel_avg_variance8x4, aom_sad8x4x4d,
      aom_jnt_sad8x4_avg, aom_jnt_sub_pixel_avg_variance8x4)

  BFP(BLOCK_4X8, aom_sad4x8, aom_sad4x8_avg, aom_variance4x8,
      aom_sub_pixel_variance4x8, aom_sub_pixel_avg_variance4x8, aom_sad4x8x4d,
      aom_jnt_sad4x8_avg, aom_jnt_sub_pixel_avg_variance4x8)

  BFP(BLOCK_4X4, aom_sad4x4, aom_sad4x4_avg, aom_variance4x4,
      aom_sub_pixel_variance4x4, aom_sub_pixel_avg_variance4x4, aom_sad4x4x4d,
      aom_jnt_sad4x4_avg, aom_jnt_sub_pixel_avg_variance4x4)

#define OBFP(BT, OSDF, OVF, OSVF) \
  cpi->fn_ptr[BT].osdf = OSDF;    \
  cpi->fn_ptr[BT].ovf = OVF;      \
  cpi->fn_ptr[BT].osvf = OSVF;

  OBFP(BLOCK_128X128, aom_obmc_sad128x128, aom_obmc_variance128x128,
       aom_obmc_sub_pixel_variance128x128)
  OBFP(BLOCK_128X64, aom_obmc_sad128x64, aom_obmc_variance128x64,
       aom_obmc_sub_pixel_variance128x64)
  OBFP(BLOCK_64X128, aom_obmc_sad64x128, aom_obmc_variance64x128,
       aom_obmc_sub_pixel_variance64x128)
  OBFP(BLOCK_64X64, aom_obmc_sad64x64, aom_obmc_variance64x64,
       aom_obmc_sub_pixel_variance64x64)
  OBFP(BLOCK_64X32, aom_obmc_sad64x32, aom_obmc_variance64x32,
       aom_obmc_sub_pixel_variance64x32)
  OBFP(BLOCK_32X64, aom_obmc_sad32x64, aom_obmc_variance32x64,
       aom_obmc_sub_pixel_variance32x64)
  OBFP(BLOCK_32X32, aom_obmc_sad32x32, aom_obmc_variance32x32,
       aom_obmc_sub_pixel_variance32x32)
  OBFP(BLOCK_32X16, aom_obmc_sad32x16, aom_obmc_variance32x16,
       aom_obmc_sub_pixel_variance32x16)
  OBFP(BLOCK_16X32, aom_obmc_sad16x32, aom_obmc_variance16x32,
       aom_obmc_sub_pixel_variance16x32)
  OBFP(BLOCK_16X16, aom_obmc_sad16x16, aom_obmc_variance16x16,
       aom_obmc_sub_pixel_variance16x16)
  OBFP(BLOCK_16X8, aom_obmc_sad16x8, aom_obmc_variance16x8,
       aom_obmc_sub_pixel_variance16x8)
  OBFP(BLOCK_8X16, aom_obmc_sad8x16, aom_obmc_variance8x16,
       aom_obmc_sub_pixel_variance8x16)
  OBFP(BLOCK_8X8, aom_obmc_sad8x8, aom_obmc_variance8x8,
       aom_obmc_sub_pixel_variance8x8)
  OBFP(BLOCK_4X8, aom_obmc_sad4x8, aom_obmc_variance4x8,
       aom_obmc_sub_pixel_variance4x8)
  OBFP(BLOCK_8X4, aom_obmc_sad8x4, aom_obmc_variance8x4,
       aom_obmc_sub_pixel_variance8x4)
  OBFP(BLOCK_4X4, aom_obmc_sad4x4, aom_obmc_variance4x4,
       aom_obmc_sub_pixel_variance4x4)
  OBFP(BLOCK_4X16, aom_obmc_sad4x16, aom_obmc_variance4x16,
       aom_obmc_sub_pixel_variance4x16)
  OBFP(BLOCK_16X4, aom_obmc_sad16x4, aom_obmc_variance16x4,
       aom_obmc_sub_pixel_variance16x4)
  OBFP(BLOCK_8X32, aom_obmc_sad8x32, aom_obmc_variance8x32,
       aom_obmc_sub_pixel_variance8x32)
  OBFP(BLOCK_32X8, aom_obmc_sad32x8, aom_obmc_variance32x8,
       aom_obmc_sub_pixel_variance32x8)
  OBFP(BLOCK_16X64, aom_obmc_sad16x64, aom_obmc_variance16x64,
       aom_obmc_sub_pixel_variance16x64)
  OBFP(BLOCK_64X16, aom_obmc_sad64x16, aom_obmc_variance64x16,
       aom_obmc_sub_pixel_variance64x16)

#define MBFP(BT, MCSDF, MCSVF)  \
  cpi->fn_ptr[BT].msdf = MCSDF; \
  cpi->fn_ptr[BT].msvf = MCSVF;

  MBFP(BLOCK_128X128, aom_masked_sad128x128,
       aom_masked_sub_pixel_variance128x128)
  MBFP(BLOCK_128X64, aom_masked_sad128x64, aom_masked_sub_pixel_variance128x64)
  MBFP(BLOCK_64X128, aom_masked_sad64x128, aom_masked_sub_pixel_variance64x128)
  MBFP(BLOCK_64X64, aom_masked_sad64x64, aom_masked_sub_pixel_variance64x64)
  MBFP(BLOCK_64X32, aom_masked_sad64x32, aom_masked_sub_pixel_variance64x32)
  MBFP(BLOCK_32X64, aom_masked_sad32x64, aom_masked_sub_pixel_variance32x64)
  MBFP(BLOCK_32X32, aom_masked_sad32x32, aom_masked_sub_pixel_variance32x32)
  MBFP(BLOCK_32X16, aom_masked_sad32x16, aom_masked_sub_pixel_variance32x16)
  MBFP(BLOCK_16X32, aom_masked_sad16x32, aom_masked_sub_pixel_variance16x32)
  MBFP(BLOCK_16X16, aom_masked_sad16x16, aom_masked_sub_pixel_variance16x16)
  MBFP(BLOCK_16X8, aom_masked_sad16x8, aom_masked_sub_pixel_variance16x8)
  MBFP(BLOCK_8X16, aom_masked_sad8x16, aom_masked_sub_pixel_variance8x16)
  MBFP(BLOCK_8X8, aom_masked_sad8x8, aom_masked_sub_pixel_variance8x8)
  MBFP(BLOCK_4X8, aom_masked_sad4x8, aom_masked_sub_pixel_variance4x8)
  MBFP(BLOCK_8X4, aom_masked_sad8x4, aom_masked_sub_pixel_variance8x4)
  MBFP(BLOCK_4X4, aom_masked_sad4x4, aom_masked_sub_pixel_variance4x4)

  MBFP(BLOCK_4X16, aom_masked_sad4x16, aom_masked_sub_pixel_variance4x16)

  MBFP(BLOCK_16X4, aom_masked_sad16x4, aom_masked_sub_pixel_variance16x4)

  MBFP(BLOCK_8X32, aom_masked_sad8x32, aom_masked_sub_pixel_variance8x32)

  MBFP(BLOCK_32X8, aom_masked_sad32x8, aom_masked_sub_pixel_variance32x8)

  MBFP(BLOCK_16X64, aom_masked_sad16x64, aom_masked_sub_pixel_variance16x64)

  MBFP(BLOCK_64X16, aom_masked_sad64x16, aom_masked_sub_pixel_variance64x16)

  highbd_set_var_fns(cpi);

  /* av1_init_quantizer() is first called here. Add check in
   * av1_frame_init_quantizer() so that av1_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * av1_init_quantizer() for every frame.
   */
  av1_init_quantizer(cpi);
  av1_qm_init(cm);

  av1_loop_filter_init(cm);
  cm->superres_scale_denominator = SCALE_NUMERATOR;
  cm->superres_upscaled_width = oxcf->width;
  cm->superres_upscaled_height = oxcf->height;
  av1_loop_restoration_precal();

  cm->error.setjmp = 0;

  return cpi;
}

#if CONFIG_INTERNAL_STATS
#define SNPRINT(H, T) snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T))

#define SNPRINT2(H, T, V) \
  snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T), (V))
#endif  // CONFIG_INTERNAL_STATS

void av1_remove_compressor(AV1_COMP *cpi) {
  AV1_COMMON *cm;
  unsigned int i;
  int t;

  if (!cpi) return;

  cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);

  if (cm->current_video_frame > 0) {
#if CONFIG_ENTROPY_STATS
    if (cpi->oxcf.pass != 1) {
      fprintf(stderr, "Writing counts.stt\n");
      FILE *f = fopen("counts.stt", "wb");
      fwrite(&aggregate_fc, sizeof(aggregate_fc), 1, f);
      fclose(f);
    }
#endif  // CONFIG_ENTROPY_STATS
#if CONFIG_INTERNAL_STATS
    aom_clear_system_state();

    if (cpi->oxcf.pass != 1) {
      char headings[512] = { 0 };
      char results[512] = { 0 };
      FILE *f = fopen("opsnr.stt", "a");
      double time_encoded =
          (cpi->last_end_time_stamp_seen - cpi->first_time_stamp_ever) /
          10000000.000;
      double total_encode_time =
          (cpi->time_receive_data + cpi->time_compress_data) / 1000.000;
      const double dr =
          (double)cpi->bytes * (double)8 / (double)1000 / time_encoded;
      const double peak = (double)((1 << cpi->oxcf.input_bit_depth) - 1);
      const double target_rate = (double)cpi->oxcf.target_bandwidth / 1000;
      const double rate_err = ((100.0 * (dr - target_rate)) / target_rate);

      if (cpi->b_calculate_psnr) {
        const double total_psnr = aom_sse_to_psnr(
            (double)cpi->total_samples, peak, (double)cpi->total_sq_error);
        const double total_ssim =
            100 * pow(cpi->summed_quality / cpi->summed_weights, 8.0);
        snprintf(headings, sizeof(headings),
                 "Bitrate\tAVGPsnr\tGLBPsnr\tAVPsnrP\tGLPsnrP\t"
                 "AOMSSIM\tVPSSIMP\tFASTSIM\tPSNRHVS\t"
                 "WstPsnr\tWstSsim\tWstFast\tWstHVS\t"
                 "AVPsrnY\tAPsnrCb\tAPsnrCr");
        snprintf(results, sizeof(results),
                 "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f",
                 dr, cpi->psnr.stat[STAT_ALL] / cpi->count, total_psnr,
                 cpi->psnr.stat[STAT_ALL] / cpi->count, total_psnr, total_ssim,
                 total_ssim, cpi->fastssim.stat[STAT_ALL] / cpi->count,
                 cpi->psnrhvs.stat[STAT_ALL] / cpi->count, cpi->psnr.worst,
                 cpi->worst_ssim, cpi->fastssim.worst, cpi->psnrhvs.worst,
                 cpi->psnr.stat[STAT_Y] / cpi->count,
                 cpi->psnr.stat[STAT_U] / cpi->count,
                 cpi->psnr.stat[STAT_V] / cpi->count);

        if (cpi->b_calculate_blockiness) {
          SNPRINT(headings, "\t  Block\tWstBlck");
          SNPRINT2(results, "\t%7.3f", cpi->total_blockiness / cpi->count);
          SNPRINT2(results, "\t%7.3f", cpi->worst_blockiness);
        }

        if (cpi->b_calculate_consistency) {
          double consistency =
              aom_sse_to_psnr((double)cpi->total_samples, peak,
                              (double)cpi->total_inconsistency);

          SNPRINT(headings, "\tConsist\tWstCons");
          SNPRINT2(results, "\t%7.3f", consistency);
          SNPRINT2(results, "\t%7.3f", cpi->worst_consistency);
        }
        fprintf(f, "%s\t    Time\tRcErr\tAbsErr\n", headings);
        fprintf(f, "%s\t%8.0f\t%7.2f\t%7.2f\n", results, total_encode_time,
                rate_err, fabs(rate_err));
      }

      fclose(f);
    }
#endif  // CONFIG_INTERNAL_STATS
  }

  for (t = 0; t < cpi->num_workers; ++t) {
    AVxWorker *const worker = &cpi->workers[t];
    EncWorkerData *const thread_data = &cpi->tile_thr_data[t];

    // Deallocate allocated threads.
    aom_get_worker_interface()->end(worker);

    // Deallocate allocated thread data.
    if (t < cpi->num_workers - 1) {
      aom_free(thread_data->td->palette_buffer);
      aom_free(thread_data->td->above_pred_buf);
      aom_free(thread_data->td->left_pred_buf);
      aom_free(thread_data->td->wsrc_buf);
      aom_free(thread_data->td->mask_buf);
      aom_free(thread_data->td->counts);
      av1_free_pc_tree(thread_data->td, num_planes);
      aom_free(thread_data->td);
    }
  }
  aom_free(cpi->tile_thr_data);
  aom_free(cpi->workers);

  if (cpi->num_workers > 1) {
    av1_loop_filter_dealloc(&cpi->lf_row_sync);
    av1_loop_restoration_dealloc(&cpi->lr_row_sync, cpi->num_workers);
  }

  dealloc_compressor_data(cpi);

  for (i = 0; i < sizeof(cpi->mbgraph_stats) / sizeof(cpi->mbgraph_stats[0]);
       ++i) {
    aom_free(cpi->mbgraph_stats[i].mb_stats);
  }

#if CONFIG_FP_MB_STATS
  if (cpi->use_fp_mb_stats) {
    aom_free(cpi->twopass.frame_mb_stats_buf);
    cpi->twopass.frame_mb_stats_buf = NULL;
  }
#endif
#if CONFIG_INTERNAL_STATS
  aom_free(cpi->ssim_vars);
  cpi->ssim_vars = NULL;
#endif  // CONFIG_INTERNAL_STATS

  av1_remove_common(cm);
  for (i = 0; i < FRAME_BUFFERS; ++i) {
    av1_hash_table_destroy(&cm->buffer_pool->frame_bufs[i].hash_table);
  }
  if (cpi->sf.use_hash_based_trellis) hbt_destroy();
  av1_free_ref_frame_buffers(cm->buffer_pool);
  aom_free(cpi);

#ifdef OUTPUT_YUV_SKINMAP
  fclose(yuv_skinmap_file);
#endif
#ifdef OUTPUT_YUV_REC
  fclose(yuv_rec_file);
#endif
}

static void generate_psnr_packet(AV1_COMP *cpi) {
  struct aom_codec_cx_pkt pkt;
  int i;
  PSNR_STATS psnr;
  aom_calc_highbd_psnr(cpi->source, cpi->common.frame_to_show, &psnr,
                       cpi->td.mb.e_mbd.bd, cpi->oxcf.input_bit_depth);

  for (i = 0; i < 4; ++i) {
    pkt.data.psnr.samples[i] = psnr.samples[i];
    pkt.data.psnr.sse[i] = psnr.sse[i];
    pkt.data.psnr.psnr[i] = psnr.psnr[i];
  }
  pkt.kind = AOM_CODEC_PSNR_PKT;
  aom_codec_pkt_list_add(cpi->output_pkt_list, &pkt);
}

int av1_use_as_reference(AV1_COMP *cpi, int ref_frame_flags) {
  if (ref_frame_flags > ((1 << INTER_REFS_PER_FRAME) - 1)) return -1;

  cpi->ext_ref_frame_flags = ref_frame_flags;
  return 0;
}

void av1_update_reference(AV1_COMP *cpi, int ref_frame_upd_flags) {
  cpi->ext_refresh_last_frame = (ref_frame_upd_flags & AOM_LAST_FLAG) != 0;
  cpi->ext_refresh_golden_frame = (ref_frame_upd_flags & AOM_GOLD_FLAG) != 0;
  cpi->ext_refresh_alt_ref_frame = (ref_frame_upd_flags & AOM_ALT_FLAG) != 0;
  cpi->ext_refresh_bwd_ref_frame = (ref_frame_upd_flags & AOM_BWD_FLAG) != 0;
  cpi->ext_refresh_alt2_ref_frame = (ref_frame_upd_flags & AOM_ALT2_FLAG) != 0;
  cpi->ext_refresh_frame_flags_pending = 1;
}

int av1_copy_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  YV12_BUFFER_CONFIG *cfg = get_ref_frame(cm, idx);
  if (cfg) {
    aom_yv12_copy_frame(cfg, sd, num_planes);
    return 0;
  } else {
    return -1;
  }
}

int av1_set_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  YV12_BUFFER_CONFIG *cfg = get_ref_frame(cm, idx);
  if (cfg) {
    aom_yv12_copy_frame(sd, cfg, num_planes);
    return 0;
  } else {
    return -1;
  }
}

int av1_update_entropy(AV1_COMP *cpi, int update) {
  cpi->ext_refresh_frame_context = update;
  cpi->ext_refresh_frame_context_pending = 1;
  return 0;
}

#if defined(OUTPUT_YUV_DENOISED) || defined(OUTPUT_YUV_SKINMAP)
// The denoiser buffer is allocated as a YUV 440 buffer. This function writes it
// as YUV 420. We simply use the top-left pixels of the UV buffers, since we do
// not denoise the UV channels at this time. If ever we implement UV channel
// denoising we will have to modify this.
void aom_write_yuv_frame_420(YV12_BUFFER_CONFIG *s, FILE *f) {
  uint8_t *src = s->y_buffer;
  int h = s->y_height;

  do {
    fwrite(src, s->y_width, 1, f);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, f);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, f);
    src += s->uv_stride;
  } while (--h);
}
#endif

#if USE_GF16_MULTI_LAYER
static void check_show_existing_frame_gf16(AV1_COMP *cpi) {
  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  AV1_COMMON *const cm = &cpi->common;
  const FRAME_UPDATE_TYPE next_frame_update_type =
      gf_group->update_type[gf_group->index];

  if (cm->show_existing_frame == 1) {
    cm->show_existing_frame = 0;
  } else if (cpi->rc.is_last_bipred_frame) {
    cpi->rc.is_last_bipred_frame = 0;
    cm->show_existing_frame = 1;
    cpi->existing_fb_idx_to_show = cpi->ref_fb_idx[BWDREF_FRAME - 1];
  } else if (next_frame_update_type == OVERLAY_UPDATE ||
             next_frame_update_type == INTNL_OVERLAY_UPDATE) {
    // Check the temporal filtering status for the next OVERLAY frame
    const int num_arfs_in_gf = cpi->num_extra_arfs + 1;
    int which_arf = 0, arf_idx;
    // Identify the index to the next overlay frame.
    for (arf_idx = 0; arf_idx < num_arfs_in_gf; arf_idx++) {
      if (gf_group->index == cpi->arf_pos_for_ovrly[arf_idx]) {
        which_arf = arf_idx;
        break;
      }
    }
    assert(arf_idx < num_arfs_in_gf);
    if (cpi->is_arf_filter_off[which_arf]) {
      cm->show_existing_frame = 1;
      cpi->rc.is_src_frame_alt_ref = 1;
      cpi->existing_fb_idx_to_show = (next_frame_update_type == OVERLAY_UPDATE)
                                         ? cpi->ref_fb_idx[ALTREF_FRAME - 1]
                                         : cpi->ref_fb_idx[BWDREF_FRAME - 1];
      cpi->is_arf_filter_off[which_arf] = 0;
    }
  }
  cpi->rc.is_src_frame_ext_arf = 0;
}
#endif  // USE_GF16_MULTI_LAYER

static void check_show_existing_frame(AV1_COMP *cpi) {
#if USE_GF16_MULTI_LAYER
  if (cpi->rc.baseline_gf_interval == 16) {
    check_show_existing_frame_gf16(cpi);
    return;
  }
#endif  // USE_GF16_MULTI_LAYER

  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  AV1_COMMON *const cm = &cpi->common;
  const FRAME_UPDATE_TYPE next_frame_update_type =
      gf_group->update_type[gf_group->index];
  const int which_arf = gf_group->arf_update_idx[gf_group->index];

  if (cm->show_existing_frame == 1) {
    cm->show_existing_frame = 0;
  } else if (cpi->rc.is_last_bipred_frame) {
    // NOTE: If the current frame is a last bi-predictive frame, it is
    //       needed next to show the BWDREF_FRAME, which is pointed by
    //       the last_fb_idxes[0] after reference frame buffer update
    cpi->rc.is_last_bipred_frame = 0;
    cm->show_existing_frame = 1;
    cpi->existing_fb_idx_to_show = cpi->ref_fb_idx[0];
  } else if (cpi->is_arf_filter_off[which_arf] &&
             (next_frame_update_type == OVERLAY_UPDATE ||
              next_frame_update_type == INTNL_OVERLAY_UPDATE)) {
    // Other parameters related to OVERLAY_UPDATE will be taken care of
    // in av1_rc_get_second_pass_params(cpi)
    cm->show_existing_frame = 1;
    cpi->rc.is_src_frame_alt_ref = 1;
    cpi->existing_fb_idx_to_show = (next_frame_update_type == OVERLAY_UPDATE)
                                       ? cpi->ref_fb_idx[ALTREF_FRAME - 1]
                                       : cpi->ref_fb_idx[ALTREF2_FRAME - 1];
    cpi->is_arf_filter_off[which_arf] = 0;
  }
  cpi->rc.is_src_frame_ext_arf = 0;
}

#ifdef OUTPUT_YUV_REC
void aom_write_one_yuv_frame(AV1_COMMON *cm, YV12_BUFFER_CONFIG *s) {
  uint8_t *src = s->y_buffer;
  int h = cm->height;
  if (yuv_rec_file == NULL) return;
  if (s->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t *src16 = CONVERT_TO_SHORTPTR(s->y_buffer);

    do {
      fwrite(src16, s->y_width, 2, yuv_rec_file);
      src16 += s->y_stride;
    } while (--h);

    src16 = CONVERT_TO_SHORTPTR(s->u_buffer);
    h = s->uv_height;

    do {
      fwrite(src16, s->uv_width, 2, yuv_rec_file);
      src16 += s->uv_stride;
    } while (--h);

    src16 = CONVERT_TO_SHORTPTR(s->v_buffer);
    h = s->uv_height;

    do {
      fwrite(src16, s->uv_width, 2, yuv_rec_file);
      src16 += s->uv_stride;
    } while (--h);

    fflush(yuv_rec_file);
    return;
  }

  do {
    fwrite(src, s->y_width, 1, yuv_rec_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  fflush(yuv_rec_file);
}
#endif  // OUTPUT_YUV_REC

#define GM_RECODE_LOOP_NUM4X4_FACTOR 192
static int recode_loop_test_global_motion(AV1_COMP *cpi) {
  int i;
  int recode = 0;
  RD_COUNTS *const rdc = &cpi->td.rd_counts;
  AV1_COMMON *const cm = &cpi->common;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    if (cm->global_motion[i].wmtype != IDENTITY &&
        rdc->global_motion_used[i] * GM_RECODE_LOOP_NUM4X4_FACTOR <
            cpi->gmparams_cost[i]) {
      cm->global_motion[i] = default_warp_params;
      assert(cm->global_motion[i].wmtype == IDENTITY);
      cpi->gmparams_cost[i] = 0;
      recode = 1;
      // TODO(sarahparker): The earlier condition for recoding here was:
      // "recode |= (rdc->global_motion_used[i] > 0);". Can we bring something
      // similar to that back to speed up global motion?
    }
  }
  return recode;
}

// Function to test for conditions that indicate we should loop
// back and recode a frame.
static int recode_loop_test(AV1_COMP *cpi, int high_limit, int low_limit, int q,
                            int maxq, int minq) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const int frame_is_kfgfarf = frame_is_kf_gf_arf(cpi);
  int force_recode = 0;

  if ((rc->projected_frame_size >= rc->max_frame_bandwidth) ||
      (cpi->sf.recode_loop == ALLOW_RECODE) ||
      (frame_is_kfgfarf && (cpi->sf.recode_loop == ALLOW_RECODE_KFARFGF))) {
    // TODO(agrange) high_limit could be greater than the scale-down threshold.
    if ((rc->projected_frame_size > high_limit && q < maxq) ||
        (rc->projected_frame_size < low_limit && q > minq)) {
      force_recode = 1;
    } else if (cpi->oxcf.rc_mode == AOM_CQ) {
      // Deal with frame undershoot and whether or not we are
      // below the automatically set cq level.
      if (q > oxcf->cq_level &&
          rc->projected_frame_size < ((rc->this_frame_target * 7) >> 3)) {
        force_recode = 1;
      }
    }
  }
  return force_recode;
}

#define DUMP_REF_FRAME_IMAGES 0

#if DUMP_REF_FRAME_IMAGES == 1
static int dump_one_image(AV1_COMMON *cm,
                          const YV12_BUFFER_CONFIG *const ref_buf,
                          char *file_name) {
  int h;
  FILE *f_ref = NULL;

  if (ref_buf == NULL) {
    printf("Frame data buffer is NULL.\n");
    return AOM_CODEC_MEM_ERROR;
  }

  if ((f_ref = fopen(file_name, "wb")) == NULL) {
    printf("Unable to open file %s to write.\n", file_name);
    return AOM_CODEC_MEM_ERROR;
  }

  // --- Y ---
  for (h = 0; h < cm->height; ++h) {
    fwrite(&ref_buf->y_buffer[h * ref_buf->y_stride], 1, cm->width, f_ref);
  }
  // --- U ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&ref_buf->u_buffer[h * ref_buf->uv_stride], 1, (cm->width >> 1),
           f_ref);
  }
  // --- V ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&ref_buf->v_buffer[h * ref_buf->uv_stride], 1, (cm->width >> 1),
           f_ref);
  }

  fclose(f_ref);

  return AOM_CODEC_OK;
}

static void dump_ref_frame_images(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    char file_name[256] = "";
    snprintf(file_name, sizeof(file_name), "/tmp/enc_F%d_ref_%d.yuv",
             cm->current_video_frame, ref_frame);
    dump_one_image(cm, get_ref_frame_buffer(cpi, ref_frame), file_name);
  }
}
#endif  // DUMP_REF_FRAME_IMAGES == 1

// This function is used to shift the virtual indices of last reference frames
// as follows:
// LAST_FRAME -> LAST2_FRAME -> LAST3_FRAME
// when the LAST_FRAME is updated.
static INLINE void shift_last_ref_frames(AV1_COMP *cpi) {
  // TODO(isbs): shift the scaled indices as well
  int ref_frame;
  for (ref_frame = LAST_REF_FRAMES - 1; ref_frame > 0; --ref_frame) {
    cpi->ref_fb_idx[ref_frame] = cpi->ref_fb_idx[ref_frame - 1];

    // [0] is allocated to the current coded frame. The statistics for the
    // reference frames start at [LAST_FRAME], i.e. [1].
    if (!cpi->rc.is_src_frame_alt_ref) {
      memcpy(cpi->interp_filter_selected[ref_frame + LAST_FRAME],
             cpi->interp_filter_selected[ref_frame - 1 + LAST_FRAME],
             sizeof(cpi->interp_filter_selected[ref_frame - 1 + LAST_FRAME]));
    }
  }
}

#if USE_GF16_MULTI_LAYER
static void update_reference_frames_gf16(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (cm->frame_type == KEY_FRAME) {
    for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[ref_frame]],
                 cm->new_fb_idx);
    }
  } else {
    if (cpi->refresh_last_frame || cpi->refresh_golden_frame ||
        cpi->refresh_bwd_ref_frame || cpi->refresh_alt2_ref_frame ||
        cpi->refresh_alt_ref_frame) {
      assert(cpi->refresh_fb_idx >= 0 && cpi->refresh_fb_idx < REF_FRAMES);
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->refresh_fb_idx],
                 cm->new_fb_idx);
    }

    // TODO(zoeliu): To handle cpi->interp_filter_selected[].

    // For GF of 16, an additional ref frame index mapping needs to be handled
    // if this is the last frame to encode in the current GF group.
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    if (gf_group->update_type[gf_group->index + 1] == OVERLAY_UPDATE)
      av1_ref_frame_map_idx_updates(cpi, gf_group->index + 1);
  }

#if DUMP_REF_FRAME_IMAGES == 1
  // Dump out all reference frame images.
  dump_ref_frame_images(cpi);
#endif  // DUMP_REF_FRAME_IMAGES
}
#endif  // USE_GF16_MULTI_LAYER

static void update_reference_frames(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  // NOTE: Save the new show frame buffer index for --test-code=warn, i.e.,
  //       for the purpose to verify no mismatch between encoder and decoder.
  if (cm->show_frame) cpi->last_show_frame_buf_idx = cm->new_fb_idx;

#if USE_GF16_MULTI_LAYER
  if (cpi->rc.baseline_gf_interval == 16) {
    update_reference_frames_gf16(cpi);
    return;
  }
#endif  // USE_GF16_MULTI_LAYER

  BufferPool *const pool = cm->buffer_pool;

  // At this point the new frame has been encoded.
  // If any buffer copy / swapping is signaled it should be done here.

  if (cm->frame_type == KEY_FRAME || frame_is_sframe(cm)) {
    for (int ref_frame = 0; ref_frame < REF_FRAMES; ++ref_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[ref_frame]],
                 cm->new_fb_idx);
    }
    return;
  }

  if (av1_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term in function
    // av1_bitstream.c::get_refresh_mask() we left it in the GF slot and, if
    // we're updating the GF with the current decoded frame, we save it to the
    // ARF slot instead.
    // We now have to update the ARF with the current frame and swap gld_fb_idx
    // and alt_fb_idx so that, overall, we've stored the old GF in the new ARF
    // slot and, if we're updating the GF, the current frame becomes the new GF.
    int tmp;

    ref_cnt_fb(pool->frame_bufs,
               &cm->ref_frame_map[cpi->ref_fb_idx[ALTREF_FRAME - 1]],
               cm->new_fb_idx);
    tmp = cpi->ref_fb_idx[ALTREF_FRAME - 1];
    cpi->ref_fb_idx[ALTREF_FRAME - 1] = cpi->ref_fb_idx[GOLDEN_FRAME - 1];
    cpi->ref_fb_idx[GOLDEN_FRAME - 1] = tmp;

    // We need to modify the mapping accordingly
    cpi->arf_map[0] = cpi->ref_fb_idx[ALTREF_FRAME - 1];
    // TODO(zoeliu): Do we need to copy cpi->interp_filter_selected[0] over to
    // cpi->interp_filter_selected[GOLDEN_FRAME]?
  } else if (cpi->rc.is_src_frame_ext_arf && cm->show_existing_frame) {
    // Deal with the special case for showing existing internal ALTREF_FRAME
    // Refresh the LAST_FRAME with the ALTREF_FRAME and retire the LAST3_FRAME
    // by updating the virtual indices.
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const int which_arf = gf_group->arf_ref_idx[gf_group->index];
    assert(gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE);

    const int tmp = cpi->ref_fb_idx[LAST_REF_FRAMES - 1];
    shift_last_ref_frames(cpi);

    cpi->ref_fb_idx[LAST_FRAME - 1] = cpi->ref_fb_idx[ALTREF2_FRAME - 1];
    cpi->ref_fb_idx[ALTREF2_FRAME - 1] = tmp;
    // We need to modify the mapping accordingly
    cpi->arf_map[which_arf] = cpi->ref_fb_idx[ALTREF2_FRAME - 1];

    memcpy(cpi->interp_filter_selected[LAST_FRAME],
           cpi->interp_filter_selected[ALTREF2_FRAME],
           sizeof(cpi->interp_filter_selected[ALTREF2_FRAME]));
  } else { /* For non key/golden frames */
    // === ALTREF_FRAME ===
    if (cpi->refresh_alt_ref_frame) {
      int arf_idx = cpi->ref_fb_idx[ALTREF_FRAME - 1];
      int which_arf = 0;
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[arf_idx], cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[ALTREF_FRAME + which_arf],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }

    // === GOLDEN_FRAME ===
    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]],
                 cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[GOLDEN_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }

    // === BWDREF_FRAME ===
    if (cpi->refresh_bwd_ref_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[BWDREF_FRAME - 1]],
                 cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[BWDREF_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }

    // === ALTREF2_FRAME ===
    if (cpi->refresh_alt2_ref_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]],
                 cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[ALTREF2_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }
  }

  if (cpi->refresh_last_frame) {
    // NOTE(zoeliu): We have two layers of mapping (1) from the per-frame
    // reference to the reference frame buffer virtual index; and then (2) from
    // the virtual index to the reference frame buffer physical index:
    //
    // LAST_FRAME,      ..., LAST3_FRAME,     ..., ALTREF_FRAME
    //      |                     |                     |
    //      v                     v                     v
    // ref_fb_idx[0],   ..., ref_fb_idx[2],   ..., ref_fb_idx[ALTREF_FRAME-1]
    //      |                     |                     |
    //      v                     v                     v
    // ref_frame_map[], ..., ref_frame_map[], ..., ref_frame_map[]
    //
    // When refresh_last_frame is set, it is intended to retire LAST3_FRAME,
    // have the other 2 LAST reference frames shifted as follows:
    // LAST_FRAME -> LAST2_FRAME -> LAST3_FRAME
    // , and then have LAST_FRAME refreshed by the newly coded frame.
    //
    // To fulfill it, the decoder will be notified to execute following 2 steps:
    //
    // (a) To change ref_frame_map[] and have the virtual index of LAST3_FRAME
    //     to point to the newly coded frame, i.e.
    //     ref_frame_map[lst_fb_idexes[2]] => new_fb_idx;
    //
    // (b) To change the 1st layer mapping to have LAST_FRAME mapped to the
    //     original virtual index of LAST3_FRAME and have the other mappings
    //     shifted as follows:
    // LAST_FRAME,      LAST2_FRAME,     LAST3_FRAME
    //      |                |                |
    //      v                v                v
    // ref_fb_idx[2],   ref_fb_idx[0],   ref_fb_idx[1]
    int tmp;

    ref_cnt_fb(pool->frame_bufs,
               &cm->ref_frame_map[cpi->ref_fb_idx[LAST_REF_FRAMES - 1]],
               cm->new_fb_idx);

    tmp = cpi->ref_fb_idx[LAST_REF_FRAMES - 1];

    shift_last_ref_frames(cpi);
    cpi->ref_fb_idx[0] = tmp;

    assert(cm->show_existing_frame == 0);
    memcpy(cpi->interp_filter_selected[LAST_FRAME],
           cpi->interp_filter_selected[0],
           sizeof(cpi->interp_filter_selected[0]));

    if (cpi->rc.is_last_bipred_frame) {
      // Refresh the LAST_FRAME with the BWDREF_FRAME and retire the
      // LAST3_FRAME by updating the virtual indices.
      //
      // NOTE: The source frame for BWDREF does not have a holding position as
      //       the OVERLAY frame for ALTREF's. Hence, to resolve the reference
      //       virtual index reshuffling for BWDREF, the encoder always
      //       specifies a LAST_BIPRED right before BWDREF and completes the
      //       reshuffling job accordingly.
      tmp = cpi->ref_fb_idx[LAST_REF_FRAMES - 1];

      shift_last_ref_frames(cpi);
      cpi->ref_fb_idx[0] = cpi->ref_fb_idx[BWDREF_FRAME - 1];
      cpi->ref_fb_idx[BWDREF_FRAME - 1] = tmp;

      memcpy(cpi->interp_filter_selected[LAST_FRAME],
             cpi->interp_filter_selected[BWDREF_FRAME],
             sizeof(cpi->interp_filter_selected[BWDREF_FRAME]));
    }
  }

#if DUMP_REF_FRAME_IMAGES == 1
  // Dump out all reference frame images.
  dump_ref_frame_images(cpi);
#endif  // DUMP_REF_FRAME_IMAGES
}

static INLINE void alloc_frame_mvs(AV1_COMMON *const cm, int buffer_idx) {
  assert(buffer_idx != INVALID_IDX);
  RefCntBuffer *const new_fb_ptr = &cm->buffer_pool->frame_bufs[buffer_idx];
  ensure_mv_buffer(new_fb_ptr, cm);
  new_fb_ptr->width = cm->width;
  new_fb_ptr->height = cm->height;
}

static void scale_references(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MV_REFERENCE_FRAME ref_frame;
  const AOM_REFFRAME ref_mask[INTER_REFS_PER_FRAME] = {
    AOM_LAST_FLAG, AOM_LAST2_FLAG, AOM_LAST3_FLAG, AOM_GOLD_FLAG,
    AOM_BWD_FLAG,  AOM_ALT2_FLAG,  AOM_ALT_FLAG
  };

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    // Need to convert from AOM_REFFRAME to index into ref_mask (subtract 1).
    if (cpi->ref_frame_flags & ref_mask[ref_frame - 1]) {
      BufferPool *const pool = cm->buffer_pool;
      const YV12_BUFFER_CONFIG *const ref =
          get_ref_frame_buffer(cpi, ref_frame);

      if (ref == NULL) {
        cpi->scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
        continue;
      }

      if (ref->y_crop_width != cm->width || ref->y_crop_height != cm->height) {
        RefCntBuffer *new_fb_ptr = NULL;
        int force_scaling = 0;
        int new_fb = cpi->scaled_ref_idx[ref_frame - 1];
        if (new_fb == INVALID_IDX) {
          new_fb = get_free_fb(cm);
          force_scaling = 1;
        }
        if (new_fb == INVALID_IDX) return;
        new_fb_ptr = &pool->frame_bufs[new_fb];
        if (force_scaling || new_fb_ptr->buf.y_crop_width != cm->width ||
            new_fb_ptr->buf.y_crop_height != cm->height) {
          if (aom_realloc_frame_buffer(
                  &new_fb_ptr->buf, cm->width, cm->height, cm->subsampling_x,
                  cm->subsampling_y, cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                  cm->byte_alignment, NULL, NULL, NULL))
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          av1_resize_and_extend_frame(ref, &new_fb_ptr->buf, (int)cm->bit_depth,
                                      num_planes);
          cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
          alloc_frame_mvs(cm, new_fb);
        }
      } else {
        const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
        RefCntBuffer *const buf = &pool->frame_bufs[buf_idx];
        buf->buf.y_crop_width = ref->y_crop_width;
        buf->buf.y_crop_height = ref->y_crop_height;
        cpi->scaled_ref_idx[ref_frame - 1] = buf_idx;
        ++buf->ref_count;
      }
    } else {
      if (cpi->oxcf.pass != 0) cpi->scaled_ref_idx[ref_frame - 1] = INVALID_IDX;
    }
  }
}

static void release_scaled_references(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  int i;
  // TODO(isbs): only refresh the necessary frames, rather than all of them
  for (i = 0; i < REF_FRAMES; ++i) {
    const int idx = cpi->scaled_ref_idx[i];
    RefCntBuffer *const buf =
        idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[idx] : NULL;
    if (buf != NULL) {
      --buf->ref_count;
      cpi->scaled_ref_idx[i] = INVALID_IDX;
    }
  }
}

static void set_mv_search_params(AV1_COMP *cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  const unsigned int max_mv_def = AOMMIN(cm->width, cm->height);

  // Default based on max resolution.
  cpi->mv_step_param = av1_init_search_range(max_mv_def);

  if (cpi->sf.mv.auto_mv_step_size) {
    if (frame_is_intra_only(cm)) {
      // Initialize max_mv_magnitude for use in the first INTER frame
      // after a key/intra-only frame.
      cpi->max_mv_magnitude = max_mv_def;
    } else {
      if (cm->show_frame) {
        // Allow mv_steps to correspond to twice the max mv magnitude found
        // in the previous frame, capped by the default max_mv_magnitude based
        // on resolution.
        cpi->mv_step_param = av1_init_search_range(
            AOMMIN(max_mv_def, 2 * cpi->max_mv_magnitude));
      }
      cpi->max_mv_magnitude = 0;
    }
  }
}

static void set_size_independent_vars(AV1_COMP *cpi) {
  int i;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    cpi->common.global_motion[i] = default_warp_params;
  }
  cpi->global_motion_search_done = 0;
  av1_set_speed_features_framesize_independent(cpi);
  av1_set_rd_speed_thresholds(cpi);
  av1_set_rd_speed_thresholds_sub8x8(cpi);
  cpi->common.interp_filter = cpi->sf.default_interp_filter;
  cpi->common.switchable_motion_mode = 1;
}

static void set_size_dependent_vars(AV1_COMP *cpi, int *q, int *bottom_index,
                                    int *top_index) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  // Setup variables that depend on the dimensions of the frame.
  av1_set_speed_features_framesize_dependent(cpi);

  // Decide q and q bounds.
  *q = av1_rc_pick_q_and_bounds(cpi, cm->width, cm->height, bottom_index,
                                top_index);

  if (!frame_is_intra_only(cm)) {
    set_high_precision_mv(cpi, (*q) < HIGH_PRECISION_MV_QTHRESH,
                          cpi->common.cur_frame_force_integer_mv);
  }

  // Configure experimental use of segmentation for enhanced coding of
  // static regions if indicated.
  // Only allowed in the second pass of a two pass encode, as it requires
  // lagged coding, and if the relevant speed feature flag is set.
  if (oxcf->pass == 2 && cpi->sf.static_segmentation)
    configure_static_seg_features(cpi);
}

static void init_motion_estimation(AV1_COMP *cpi) {
  int y_stride = cpi->scaled_source.y_stride;

  if (cpi->sf.mv.search_method == NSTEP) {
    av1_init3smotion_compensation(&cpi->ss_cfg, y_stride);
  } else if (cpi->sf.mv.search_method == DIAMOND) {
    av1_init_dsmotion_compensation(&cpi->ss_cfg, y_stride);
  }
}

#define COUPLED_CHROMA_FROM_LUMA_RESTORATION 0
static void set_restoration_unit_size(int width, int height, int sx, int sy,
                                      RestorationInfo *rst) {
  (void)width;
  (void)height;
  (void)sx;
  (void)sy;
#if COUPLED_CHROMA_FROM_LUMA_RESTORATION
  int s = AOMMIN(sx, sy);
#else
  int s = 0;
#endif  // !COUPLED_CHROMA_FROM_LUMA_RESTORATION

  if (width * height > 352 * 288)
    rst[0].restoration_unit_size = RESTORATION_UNITSIZE_MAX;
  else
    rst[0].restoration_unit_size = (RESTORATION_UNITSIZE_MAX >> 1);
  rst[1].restoration_unit_size = rst[0].restoration_unit_size >> s;
  rst[2].restoration_unit_size = rst[1].restoration_unit_size;
}

static void init_ref_frame_bufs(AV1_COMMON *cm) {
  int i;
  BufferPool *const pool = cm->buffer_pool;
  cm->new_fb_idx = INVALID_IDX;
  for (i = 0; i < REF_FRAMES; ++i) {
    cm->ref_frame_map[i] = INVALID_IDX;
    pool->frame_bufs[i].ref_count = 0;
  }
  if (cm->seq_params.force_screen_content_tools) {
    for (i = 0; i < FRAME_BUFFERS; ++i) {
      av1_hash_table_init(&pool->frame_bufs[i].hash_table);
    }
  }
}

static void check_initial_width(AV1_COMP *cpi, int use_highbitdepth,
                                int subsampling_x, int subsampling_y) {
  AV1_COMMON *const cm = &cpi->common;

  if (!cpi->initial_width || cm->use_highbitdepth != use_highbitdepth ||
      cm->subsampling_x != subsampling_x ||
      cm->subsampling_y != subsampling_y) {
    cm->subsampling_x = subsampling_x;
    cm->subsampling_y = subsampling_y;
    cm->use_highbitdepth = use_highbitdepth;

    alloc_raw_frame_buffers(cpi);
    init_ref_frame_bufs(cm);
    alloc_util_frame_buffers(cpi);

    init_motion_estimation(cpi);  // TODO(agrange) This can be removed.

    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
    cpi->initial_mbs = cm->MBs;
  }
}

// Returns 1 if the assigned width or height was <= 0.
static int set_size_literal(AV1_COMP *cpi, int width, int height) {
  AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  check_initial_width(cpi, cm->use_highbitdepth, cm->subsampling_x,
                      cm->subsampling_y);

  if (width <= 0 || height <= 0) return 1;

  cm->width = width;
  cm->height = height;

  if (cpi->initial_width && cpi->initial_height &&
      (cm->width > cpi->initial_width || cm->height > cpi->initial_height)) {
    av1_free_context_buffers(cm);
    av1_free_pc_tree(&cpi->td, num_planes);
    alloc_compressor_data(cpi);
    realloc_segmentation_maps(cpi);
    cpi->initial_width = cpi->initial_height = 0;
  }
  update_frame_size(cpi);

  return 0;
}

static void set_frame_size(AV1_COMP *cpi, int width, int height) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int ref_frame;

  if (width != cm->width || height != cm->height) {
    // There has been a change in the encoded frame size
    set_size_literal(cpi, width, height);
    set_mv_search_params(cpi);
    // Recalculate 'all_lossless' in case super-resolution was (un)selected.
    cm->all_lossless = cm->coded_lossless && !av1_superres_scaled(cm);
  }

  if (cpi->oxcf.pass == 2) {
    av1_set_target_rate(cpi, cm->width, cm->height);
  }

  alloc_frame_mvs(cm, cm->new_fb_idx);

  // Allocate above context buffers
  if (cm->num_allocated_above_context_planes < av1_num_planes(cm) ||
      cm->num_allocated_above_context_mi_col < cm->mi_cols ||
      cm->num_allocated_above_contexts < cm->tile_rows) {
    av1_free_above_context_buffers(cm, cm->num_allocated_above_contexts);
    if (av1_alloc_above_context_buffers(cm, cm->tile_rows))
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate context buffers");
  }

  // Reset the frame pointers to the current frame size.
  if (aom_realloc_frame_buffer(get_frame_new_buffer(cm), cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               cm->use_highbitdepth, AOM_BORDER_IN_PIXELS,
                               cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");

  const int frame_width = cm->superres_upscaled_width;
  const int frame_height = cm->superres_upscaled_height;
  set_restoration_unit_size(frame_width, frame_height, cm->subsampling_x,
                            cm->subsampling_y, cm->rst_info);
  for (int i = 0; i < num_planes; ++i)
    cm->rst_info[i].frame_restoration_type = RESTORE_NONE;

  av1_alloc_restoration_buffers(cm);
  alloc_util_frame_buffers(cpi);  // TODO(afergs): Remove? Gets called anyways.
  init_motion_estimation(cpi);

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - LAST_FRAME];
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);

    ref_buf->idx = buf_idx;

    if (buf_idx != INVALID_IDX) {
      YV12_BUFFER_CONFIG *const buf = &cm->buffer_pool->frame_bufs[buf_idx].buf;
      ref_buf->buf = buf;
      av1_setup_scale_factors_for_frame(&ref_buf->sf, buf->y_crop_width,
                                        buf->y_crop_height, cm->width,
                                        cm->height);
      if (av1_is_scaled(&ref_buf->sf))
        aom_extend_frame_borders(buf, num_planes);
    } else {
      ref_buf->buf = NULL;
    }
  }

  av1_setup_scale_factors_for_frame(&cm->sf_identity, cm->width, cm->height,
                                    cm->width, cm->height);

  set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);
}

static uint8_t calculate_next_resize_scale(const AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 56789;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->pass == 1) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;

  if (cpi->common.seq_params.reduced_still_picture_hdr) return SCALE_NUMERATOR;
  switch (oxcf->resize_mode) {
    case RESIZE_NONE: new_denom = SCALE_NUMERATOR; break;
    case RESIZE_FIXED:
      if (cpi->common.frame_type == KEY_FRAME)
        new_denom = oxcf->resize_kf_scale_denominator;
      else
        new_denom = oxcf->resize_scale_denominator;
      break;
    case RESIZE_RANDOM: new_denom = lcg_rand16(&seed) % 9 + 8; break;
    default: assert(0);
  }
  return new_denom;
}

static uint8_t calculate_next_superres_scale(AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 34567;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->pass == 1) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;

  // Make sure that superres mode of the frame is consistent with the
  // sequence-level flag.
  assert(IMPLIES(oxcf->superres_mode != SUPERRES_NONE,
                 cpi->common.seq_params.enable_superres));
  assert(IMPLIES(!cpi->common.seq_params.enable_superres,
                 oxcf->superres_mode == SUPERRES_NONE));

  switch (oxcf->superres_mode) {
    case SUPERRES_NONE: new_denom = SCALE_NUMERATOR; break;
    case SUPERRES_FIXED:
      if (cpi->common.frame_type == KEY_FRAME)
        new_denom = oxcf->superres_kf_scale_denominator;
      else
        new_denom = oxcf->superres_scale_denominator;
      break;
    case SUPERRES_RANDOM: new_denom = lcg_rand16(&seed) % 9 + 8; break;
    case SUPERRES_QTHRESH: {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      const RATE_FACTOR_LEVEL rf_level = gf_group->rf_level[gf_group->index];
      const double rate_factor_delta = rate_factor_deltas[rf_level];
      const int qthresh = (rate_factor_delta <= 1.0)
                              ? oxcf->superres_qthresh
                              : oxcf->superres_kf_qthresh;
      av1_set_target_rate(cpi, cpi->oxcf.width, cpi->oxcf.height);
      int bottom_index, top_index;
      const int q = av1_rc_pick_q_and_bounds(
          cpi, cpi->oxcf.width, cpi->oxcf.height, &bottom_index, &top_index);
      if (q < qthresh) {
        new_denom = SCALE_NUMERATOR;
      } else {
        const uint8_t min_denom = SCALE_NUMERATOR + 1;
        const uint8_t denom_step = (MAXQ - qthresh + 1) >> 3;

        if (q == qthresh) {
          new_denom = min_denom;
        } else if (denom_step == 0) {
          new_denom = SCALE_NUMERATOR << 1;
        } else {
          const uint8_t additional_denom = (q - qthresh) / denom_step;
          new_denom =
              AOMMIN(min_denom + additional_denom, SCALE_NUMERATOR << 1);
        }
      }
      break;
    }
    default: assert(0);
  }
  return new_denom;
}

static int dimension_is_ok(int orig_dim, int resized_dim, int denom) {
  return (resized_dim * SCALE_NUMERATOR >= orig_dim * denom / 2);
}

static int dimensions_are_ok(int owidth, int oheight, size_params_type *rsz) {
  // Only need to check the width, as scaling is horizontal only.
  (void)oheight;
  return dimension_is_ok(owidth, rsz->resize_width, rsz->superres_denom);
}

static int validate_size_scales(RESIZE_MODE resize_mode,
                                SUPERRES_MODE superres_mode, int owidth,
                                int oheight, size_params_type *rsz) {
  if (dimensions_are_ok(owidth, oheight, rsz)) {  // Nothing to do.
    return 1;
  }

  // Calculate current resize scale.
  int resize_denom =
      AOMMAX(DIVIDE_AND_ROUND(owidth * SCALE_NUMERATOR, rsz->resize_width),
             DIVIDE_AND_ROUND(oheight * SCALE_NUMERATOR, rsz->resize_height));

  if (resize_mode != RESIZE_RANDOM && superres_mode == SUPERRES_RANDOM) {
    // Alter superres scale as needed to enforce conformity.
    rsz->superres_denom =
        (2 * SCALE_NUMERATOR * SCALE_NUMERATOR) / resize_denom;
    if (!dimensions_are_ok(owidth, oheight, rsz)) {
      if (rsz->superres_denom > SCALE_NUMERATOR) --rsz->superres_denom;
    }
  } else if (resize_mode == RESIZE_RANDOM && superres_mode != SUPERRES_RANDOM) {
    // Alter resize scale as needed to enforce conformity.
    resize_denom =
        (2 * SCALE_NUMERATOR * SCALE_NUMERATOR) / rsz->superres_denom;
    rsz->resize_width = owidth;
    rsz->resize_height = oheight;
    av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                              resize_denom);
    if (!dimensions_are_ok(owidth, oheight, rsz)) {
      if (resize_denom > SCALE_NUMERATOR) {
        --resize_denom;
        rsz->resize_width = owidth;
        rsz->resize_height = oheight;
        av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                                  resize_denom);
      }
    }
  } else if (resize_mode == RESIZE_RANDOM && superres_mode == SUPERRES_RANDOM) {
    // Alter both resize and superres scales as needed to enforce conformity.
    do {
      if (resize_denom > rsz->superres_denom)
        --resize_denom;
      else
        --rsz->superres_denom;
      rsz->resize_width = owidth;
      rsz->resize_height = oheight;
      av1_calculate_scaled_size(&rsz->resize_width, &rsz->resize_height,
                                resize_denom);
    } while (!dimensions_are_ok(owidth, oheight, rsz) &&
             (resize_denom > SCALE_NUMERATOR ||
              rsz->superres_denom > SCALE_NUMERATOR));
  } else {  // We are allowed to alter neither resize scale nor superres
            // scale.
    return 0;
  }
  return dimensions_are_ok(owidth, oheight, rsz);
}

// Calculates resize and superres params for next frame
size_params_type av1_calculate_next_size_params(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  size_params_type rsz = { oxcf->width, oxcf->height, SCALE_NUMERATOR };
  int resize_denom;
  if (oxcf->pass == 1) return rsz;
  if (cpi->resize_pending_width && cpi->resize_pending_height) {
    rsz.resize_width = cpi->resize_pending_width;
    rsz.resize_height = cpi->resize_pending_height;
    cpi->resize_pending_width = cpi->resize_pending_height = 0;
  } else {
    resize_denom = calculate_next_resize_scale(cpi);
    rsz.resize_width = cpi->oxcf.width;
    rsz.resize_height = cpi->oxcf.height;
    av1_calculate_scaled_size(&rsz.resize_width, &rsz.resize_height,
                              resize_denom);
  }
  rsz.superres_denom = calculate_next_superres_scale(cpi);
  if (!validate_size_scales(oxcf->resize_mode, oxcf->superres_mode, oxcf->width,
                            oxcf->height, &rsz))
    assert(0 && "Invalid scale parameters");
  return rsz;
}

static void setup_frame_size_from_params(AV1_COMP *cpi, size_params_type *rsz) {
  int encode_width = rsz->resize_width;
  int encode_height = rsz->resize_height;

  AV1_COMMON *cm = &cpi->common;
  cm->superres_upscaled_width = encode_width;
  cm->superres_upscaled_height = encode_height;
  cm->superres_scale_denominator = rsz->superres_denom;
  av1_calculate_scaled_superres_size(&encode_width, &encode_height,
                                     rsz->superres_denom);
  set_frame_size(cpi, encode_width, encode_height);
}

static void setup_frame_size(AV1_COMP *cpi) {
  size_params_type rsz = av1_calculate_next_size_params(cpi);
  setup_frame_size_from_params(cpi, &rsz);
}

static void superres_post_encode(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);

  if (!av1_superres_scaled(cm)) return;

  assert(cpi->oxcf.enable_superres);
  assert(!is_lossless_requested(&cpi->oxcf));
  assert(!cm->all_lossless);

  av1_superres_upscale(cm, NULL);

  // If regular resizing is occurring the source will need to be downscaled to
  // match the upscaled superres resolution. Otherwise the original source is
  // used.
  if (!av1_resize_scaled(cm)) {
    cpi->source = cpi->unscaled_source;
    if (cpi->last_source != NULL) cpi->last_source = cpi->unscaled_last_source;
  } else {
    assert(cpi->unscaled_source->y_crop_width != cm->superres_upscaled_width);
    assert(cpi->unscaled_source->y_crop_height != cm->superres_upscaled_height);
    // Do downscale. cm->(width|height) has been updated by
    // av1_superres_upscale
    if (aom_realloc_frame_buffer(
            &cpi->scaled_source, cm->superres_upscaled_width,
            cm->superres_upscaled_height, cm->subsampling_x, cm->subsampling_y,
            cm->use_highbitdepth, AOM_BORDER_IN_PIXELS, cm->byte_alignment,
            NULL, NULL, NULL))
      aom_internal_error(
          &cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to reallocate scaled source buffer for superres");
    assert(cpi->scaled_source.y_crop_width == cm->superres_upscaled_width);
    assert(cpi->scaled_source.y_crop_height == cm->superres_upscaled_height);
    av1_resize_and_extend_frame(cpi->unscaled_source, &cpi->scaled_source,
                                (int)cm->bit_depth, num_planes);
    cpi->source = &cpi->scaled_source;
  }
}

static void loopfilter_frame(AV1_COMP *cpi, AV1_COMMON *cm) {
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;

  assert(IMPLIES(is_lossless_requested(&cpi->oxcf),
                 cm->coded_lossless && cm->all_lossless));

  const int no_loopfilter = cm->coded_lossless || cm->large_scale_tile;
  const int no_cdef =
      !cm->seq_params.enable_cdef || cm->coded_lossless || cm->large_scale_tile;
  const int no_restoration = !cm->seq_params.enable_restoration ||
                             cm->all_lossless || cm->large_scale_tile;

  struct loopfilter *lf = &cm->lf;

  if (no_loopfilter) {
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
  } else {
    struct aom_usec_timer timer;

    aom_clear_system_state();

    aom_usec_timer_start(&timer);

    av1_pick_filter_level(cpi->source, cpi, cpi->sf.lpf_pick);

    aom_usec_timer_mark(&timer);
    cpi->time_pick_lpf += aom_usec_timer_elapsed(&timer);
  }

  if (lf->filter_level[0] || lf->filter_level[1]) {
#if LOOP_FILTER_BITMASK
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, 0, num_planes, 0);
#else
    if (cpi->num_workers > 1)
      av1_loop_filter_frame_mt(cm->frame_to_show, cm, xd, 0, num_planes, 0,
                               cpi->workers, cpi->num_workers,
                               &cpi->lf_row_sync);
    else
      av1_loop_filter_frame(cm->frame_to_show, cm, xd, 0, num_planes, 0);
#endif
  }

  if (!no_restoration)
    av1_loop_restoration_save_boundary_lines(cm->frame_to_show, cm, 0);

  if (no_cdef) {
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->nb_cdef_strengths = 1;
    cm->cdef_uv_strengths[0] = 0;
  } else {
    // Find CDEF parameters
    av1_cdef_search(cm->frame_to_show, cpi->source, cm, xd,
                    cpi->sf.fast_cdef_search);

    // Apply the filter
    av1_cdef_frame(cm->frame_to_show, cm, xd);
  }

  superres_post_encode(cpi);

  if (no_restoration) {
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  } else {
    av1_loop_restoration_save_boundary_lines(cm->frame_to_show, cm, 1);
    av1_pick_filter_restoration(cpi->source, cpi);
    if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
        cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
      if (cpi->num_workers > 1)
        av1_loop_restoration_filter_frame_mt(cm->frame_to_show, cm, 0,
                                             cpi->workers, cpi->num_workers,
                                             &cpi->lr_row_sync, &cpi->lr_ctxt);
      else
        av1_loop_restoration_filter_frame(cm->frame_to_show, cm, 0,
                                          &cpi->lr_ctxt);
    }
  }
}

static int encode_without_recode_loop(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int q = 0, bottom_index = 0, top_index = 0;  // Dummy variables.

  aom_clear_system_state();

  set_size_independent_vars(cpi);

  setup_frame_size(cpi);

  assert(cm->width == cpi->scaled_source.y_crop_width);
  assert(cm->height == cpi->scaled_source.y_crop_height);

  set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

  cpi->source =
      av1_scale_if_required(cm, cpi->unscaled_source, &cpi->scaled_source);
  if (cpi->unscaled_last_source != NULL)
    cpi->last_source = av1_scale_if_required(cm, cpi->unscaled_last_source,
                                             &cpi->scaled_last_source);
  cpi->source->buf_8bit_valid = 0;
  if (frame_is_intra_only(cm) == 0) {
    scale_references(cpi);
  }

  av1_set_quantizer(cm, q);
  setup_frame(cpi);
  suppress_active_map(cpi);

  // Variance adaptive and in frame q adjustment experiments are mutually
  // exclusive.
  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    av1_vaq_frame_setup(cpi);
  } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    av1_setup_in_frame_q_adj(cpi);
  } else if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
    av1_cyclic_refresh_setup(cpi);
  }
  apply_active_map(cpi);
  if (cm->seg.enabled) {
    if (!cm->seg.update_data && cm->prev_frame) {
      segfeatures_copy(&cm->seg, &cm->prev_frame->seg);
    } else {
      calculate_segdata(&cm->seg);
    }
  } else {
    memset(&cm->seg, 0, sizeof(cm->seg));
  }
  segfeatures_copy(&cm->cur_frame->seg, &cm->seg);

  // transform / motion compensation build reconstruction frame
  av1_encode_frame(cpi);

  // Update some stats from cyclic refresh, and check if we should not update
  // golden reference, for 1 pass CBR.
  if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ && cm->frame_type != KEY_FRAME &&
      (cpi->oxcf.pass == 0 && cpi->oxcf.rc_mode == AOM_CBR))
    av1_cyclic_refresh_check_golden_update(cpi);

  // Update the skip mb flag probabilities based on the distribution
  // seen in the last encoder iteration.
  // update_base_skip_probs(cpi);
  aom_clear_system_state();
  return AOM_CODEC_OK;
}

static int encode_with_recode_loop(AV1_COMP *cpi, size_t *size, uint8_t *dest) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int bottom_index, top_index;
  int loop_count = 0;
  int loop_at_this_size = 0;
  int loop = 0;
  int overshoot_seen = 0;
  int undershoot_seen = 0;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;
  int q = 0, q_low = 0, q_high = 0;

  set_size_independent_vars(cpi);

  cpi->source->buf_8bit_valid = 0;

  aom_clear_system_state();
  setup_frame_size(cpi);
  set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

  do {
    aom_clear_system_state();

    if (loop_count == 0) {
      // TODO(agrange) Scale cpi->max_mv_magnitude if frame-size has changed.
      set_mv_search_params(cpi);

      // Reset the loop state for new frame size.
      overshoot_seen = 0;
      undershoot_seen = 0;

      q_low = bottom_index;
      q_high = top_index;

      loop_at_this_size = 0;

      // Decide frame size bounds first time through.
      av1_rc_compute_frame_size_bounds(cpi, rc->this_frame_target,
                                       &frame_under_shoot_limit,
                                       &frame_over_shoot_limit);
    }

    // if frame was scaled calculate global_motion_search again if already
    // done
    if (loop_count > 0 && cpi->source && cpi->global_motion_search_done)
      if (cpi->source->y_crop_width != cm->width ||
          cpi->source->y_crop_height != cm->height)
        cpi->global_motion_search_done = 0;
    cpi->source =
        av1_scale_if_required(cm, cpi->unscaled_source, &cpi->scaled_source);
    if (cpi->unscaled_last_source != NULL)
      cpi->last_source = av1_scale_if_required(cm, cpi->unscaled_last_source,
                                               &cpi->scaled_last_source);

    if (frame_is_intra_only(cm) == 0) {
      if (loop_count > 0) {
        release_scaled_references(cpi);
      }
      scale_references(cpi);
    }
    av1_set_quantizer(cm, q);
    // printf("Frame %d/%d: q = %d, frame_type = %d\n", cm->current_video_frame,
    //        cm->show_frame, q, cm->frame_type);

    if (loop_count == 0) setup_frame(cpi);

    // Base q-index may have changed, so we need to assign proper default coef
    // probs before every iteration.
    if (cm->primary_ref_frame == PRIMARY_REF_NONE ||
        cm->frame_refs[cm->primary_ref_frame].idx < 0) {
      av1_default_coef_probs(cm);
      av1_setup_frame_contexts(cm);
    }

    // Variance adaptive and in frame q adjustment experiments are mutually
    // exclusive.
    if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
      av1_vaq_frame_setup(cpi);
    } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      av1_setup_in_frame_q_adj(cpi);
    }
    if (cm->seg.enabled) {
      if (!cm->seg.update_data && cm->prev_frame) {
        segfeatures_copy(&cm->seg, &cm->prev_frame->seg);
      } else {
        calculate_segdata(&cm->seg);
      }
    } else {
      memset(&cm->seg, 0, sizeof(cm->seg));
    }
    segfeatures_copy(&cm->cur_frame->seg, &cm->seg);

    // transform / motion compensation build reconstruction frame
    save_coding_context(cpi);
    av1_encode_frame(cpi);

    // Update the skip mb flag probabilities based on the distribution
    // seen in the last encoder iteration.
    // update_base_skip_probs(cpi);

    aom_clear_system_state();

    // Dummy pack of the bitstream using up to date stats to get an
    // accurate estimate of output frame size to determine if we need
    // to recode.
    if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
      restore_coding_context(cpi);

      if (av1_pack_bitstream(cpi, dest, size) != AOM_CODEC_OK)
        return AOM_CODEC_ERROR;

      rc->projected_frame_size = (int)(*size) << 3;
      restore_coding_context(cpi);

      if (frame_over_shoot_limit == 0) frame_over_shoot_limit = 1;
    }

    if (cpi->oxcf.rc_mode == AOM_Q) {
      loop = 0;
    } else {
      if ((cm->frame_type == KEY_FRAME) && rc->this_key_frame_forced &&
          (rc->projected_frame_size < rc->max_frame_bandwidth)) {
        int last_q = q;
        int64_t kf_err;

        int64_t high_err_target = cpi->ambient_err;
        int64_t low_err_target = cpi->ambient_err >> 1;

        if (cm->use_highbitdepth) {
          kf_err = aom_highbd_get_y_sse(cpi->source, get_frame_new_buffer(cm));
        } else {
          kf_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
        }
        // Prevent possible divide by zero error below for perfect KF
        kf_err += !kf_err;

        // The key frame is not good enough or we can afford
        // to make it better without undue risk of popping.
        if ((kf_err > high_err_target &&
             rc->projected_frame_size <= frame_over_shoot_limit) ||
            (kf_err > low_err_target &&
             rc->projected_frame_size <= frame_under_shoot_limit)) {
          // Lower q_high
          q_high = q > q_low ? q - 1 : q_low;

          // Adjust Q
          q = (int)((q * high_err_target) / kf_err);
          q = AOMMIN(q, (q_high + q_low) >> 1);
        } else if (kf_err < low_err_target &&
                   rc->projected_frame_size >= frame_under_shoot_limit) {
          // The key frame is much better than the previous frame
          // Raise q_low
          q_low = q < q_high ? q + 1 : q_high;

          // Adjust Q
          q = (int)((q * low_err_target) / kf_err);
          q = AOMMIN(q, (q_high + q_low + 1) >> 1);
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else if (recode_loop_test(cpi, frame_over_shoot_limit,
                                  frame_under_shoot_limit, q,
                                  AOMMAX(q_high, top_index), bottom_index)) {
        // Is the projected frame size out of range and are we allowed
        // to attempt to recode.
        int last_q = q;
        int retries = 0;

        // Frame size out of permitted range:
        // Update correction factor & compute new Q to try...
        // Frame is too large
        if (rc->projected_frame_size > rc->this_frame_target) {
          // Special case if the projected size is > the max allowed.
          if (rc->projected_frame_size >= rc->max_frame_bandwidth)
            q_high = rc->worst_quality;

          // Raise Qlow as to at least the current value
          q_low = q < q_high ? q + 1 : q_high;

          if (undershoot_seen || loop_at_this_size > 1) {
            // Update rate_correction_factor unless
            av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);

            q = (q_high + q_low + 1) / 2;
          } else {
            // Update rate_correction_factor unless
            av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);

            q = av1_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                  AOMMAX(q_high, top_index), cm->width,
                                  cm->height);

            while (q < q_low && retries < 10) {
              av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);
              q = av1_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                    AOMMAX(q_high, top_index), cm->width,
                                    cm->height);
              retries++;
            }
          }

          overshoot_seen = 1;
        } else {
          // Frame is too small
          q_high = q > q_low ? q - 1 : q_low;

          if (overshoot_seen || loop_at_this_size > 1) {
            av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);
            q = (q_high + q_low) / 2;
          } else {
            av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);
            q = av1_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                  top_index, cm->width, cm->height);
            // Special case reset for qlow for constrained quality.
            // This should only trigger where there is very substantial
            // undershoot on a frame and the auto cq level is above
            // the user passsed in value.
            if (cpi->oxcf.rc_mode == AOM_CQ && q < q_low) {
              q_low = q;
            }

            while (q > q_high && retries < 10) {
              av1_rc_update_rate_correction_factors(cpi, cm->width, cm->height);
              q = av1_rc_regulate_q(cpi, rc->this_frame_target, bottom_index,
                                    top_index, cm->width, cm->height);
              retries++;
            }
          }

          undershoot_seen = 1;
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = (q != last_q);
      } else {
        loop = 0;
      }
    }

    // Special case for overlay frame.
    if (rc->is_src_frame_alt_ref &&
        rc->projected_frame_size < rc->max_frame_bandwidth)
      loop = 0;

    if (recode_loop_test_global_motion(cpi)) {
      loop = 1;
    }

    if (loop) {
      ++loop_count;
      ++loop_at_this_size;

#if CONFIG_INTERNAL_STATS
      ++cpi->tot_recode_hits;
#endif
    }
  } while (loop);

  return AOM_CODEC_OK;
}

static int get_ref_frame_flags(const AV1_COMP *cpi) {
  const int *const map = cpi->common.ref_frame_map;

  // No.1 Priority: LAST_FRAME
  const int last2_is_last = map[cpi->ref_fb_idx[1]] == map[cpi->ref_fb_idx[0]];
  const int last3_is_last = map[cpi->ref_fb_idx[2]] == map[cpi->ref_fb_idx[0]];
  const int gld_is_last =
      map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]] == map[cpi->ref_fb_idx[0]];
  const int bwd_is_last =
      map[cpi->ref_fb_idx[BWDREF_FRAME - 1]] == map[cpi->ref_fb_idx[0]];
  const int alt2_is_last =
      map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] == map[cpi->ref_fb_idx[0]];
  const int alt_is_last =
      map[cpi->ref_fb_idx[ALTREF_FRAME - 1]] == map[cpi->ref_fb_idx[0]];

  // No.2 Priority: ALTREF_FRAME
  const int last2_is_alt =
      map[cpi->ref_fb_idx[1]] == map[cpi->ref_fb_idx[ALTREF_FRAME - 1]];
  const int last3_is_alt =
      map[cpi->ref_fb_idx[2]] == map[cpi->ref_fb_idx[ALTREF_FRAME - 1]];
  const int gld_is_alt = map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]] ==
                         map[cpi->ref_fb_idx[ALTREF_FRAME - 1]];
  const int bwd_is_alt = map[cpi->ref_fb_idx[BWDREF_FRAME - 1]] ==
                         map[cpi->ref_fb_idx[ALTREF_FRAME - 1]];
  const int alt2_is_alt = map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] ==
                          map[cpi->ref_fb_idx[ALTREF_FRAME - 1]];

  // No.3 Priority: LAST2_FRAME
  const int last3_is_last2 = map[cpi->ref_fb_idx[2]] == map[cpi->ref_fb_idx[1]];
  const int gld_is_last2 =
      map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]] == map[cpi->ref_fb_idx[1]];
  const int bwd_is_last2 =
      map[cpi->ref_fb_idx[BWDREF_FRAME - 1]] == map[cpi->ref_fb_idx[1]];
  const int alt2_is_last2 =
      map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] == map[cpi->ref_fb_idx[1]];

  // No.4 Priority: LAST3_FRAME
  const int gld_is_last3 =
      map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]] == map[cpi->ref_fb_idx[2]];
  const int bwd_is_last3 =
      map[cpi->ref_fb_idx[BWDREF_FRAME - 1]] == map[cpi->ref_fb_idx[2]];
  const int alt2_is_last3 =
      map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] == map[cpi->ref_fb_idx[2]];

  // No.5 Priority: GOLDEN_FRAME
  const int bwd_is_gld = map[cpi->ref_fb_idx[BWDREF_FRAME - 1]] ==
                         map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]];
  const int alt2_is_gld = map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] ==
                          map[cpi->ref_fb_idx[GOLDEN_FRAME - 1]];

  // No.6 Priority: BWDREF_FRAME
  const int alt2_is_bwd = map[cpi->ref_fb_idx[ALTREF2_FRAME - 1]] ==
                          map[cpi->ref_fb_idx[BWDREF_FRAME - 1]];

  // No.7 Priority: ALTREF2_FRAME

  // After av1_apply_encoding_flags() is called, cpi->ref_frame_flags might be
  // adjusted according to external encoder flags.
  int flags = cpi->ext_ref_frame_flags;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX) flags &= ~AOM_GOLD_FLAG;

  if (alt_is_last) flags &= ~AOM_ALT_FLAG;

  if (last2_is_last || last2_is_alt) flags &= ~AOM_LAST2_FLAG;

  if (last3_is_last || last3_is_alt || last3_is_last2) flags &= ~AOM_LAST3_FLAG;

  if (gld_is_last || gld_is_alt || gld_is_last2 || gld_is_last3)
    flags &= ~AOM_GOLD_FLAG;

  if ((bwd_is_last || bwd_is_alt || bwd_is_last2 || bwd_is_last3 ||
       bwd_is_gld) &&
      (flags & AOM_BWD_FLAG))
    flags &= ~AOM_BWD_FLAG;

  if ((alt2_is_last || alt2_is_alt || alt2_is_last2 || alt2_is_last3 ||
       alt2_is_gld || alt2_is_bwd) &&
      (flags & AOM_ALT2_FLAG))
    flags &= ~AOM_ALT2_FLAG;

  return flags;
}

static void set_ext_overrides(AV1_COMP *cpi) {
  // Overrides the defaults with the externally supplied values with
  // av1_update_reference() and av1_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to encode_frame_to_data_rate() function
  if (cpi->ext_use_s_frame) cpi->common.frame_type = S_FRAME;
  cpi->common.force_primary_ref_none = cpi->ext_use_primary_ref_none;

  if (cpi->ext_refresh_frame_context_pending) {
    cpi->common.refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  if (cpi->ext_refresh_frame_flags_pending) {
    cpi->refresh_last_frame = cpi->ext_refresh_last_frame;
    cpi->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    cpi->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
    cpi->refresh_bwd_ref_frame = cpi->ext_refresh_bwd_ref_frame;
    cpi->refresh_alt2_ref_frame = cpi->ext_refresh_alt2_ref_frame;
    cpi->ext_refresh_frame_flags_pending = 0;
  }
  cpi->common.allow_ref_frame_mvs = cpi->ext_use_ref_frame_mvs;
  cpi->common.error_resilient_mode = cpi->ext_use_error_resilient;
}

static int setup_interp_filter_search_mask(AV1_COMP *cpi) {
  InterpFilter ifilter;
  int ref_total[REF_FRAMES] = { 0 };
  MV_REFERENCE_FRAME ref;
  int mask = 0;
  int arf_idx = ALTREF_FRAME;

  if (cpi->common.last_frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame ||
      cpi->refresh_alt2_ref_frame)
    return mask;

  for (ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref)
    for (ifilter = EIGHTTAP_REGULAR; ifilter < SWITCHABLE_FILTERS; ++ifilter)
      ref_total[ref] += cpi->interp_filter_selected[ref][ifilter];

  for (ifilter = EIGHTTAP_REGULAR; ifilter < SWITCHABLE_FILTERS; ++ifilter) {
    if ((ref_total[LAST_FRAME] &&
         cpi->interp_filter_selected[LAST_FRAME][ifilter] == 0) &&
        (ref_total[LAST2_FRAME] == 0 ||
         cpi->interp_filter_selected[LAST2_FRAME][ifilter] * 50 <
             ref_total[LAST2_FRAME]) &&
        (ref_total[LAST3_FRAME] == 0 ||
         cpi->interp_filter_selected[LAST3_FRAME][ifilter] * 50 <
             ref_total[LAST3_FRAME]) &&
        (ref_total[GOLDEN_FRAME] == 0 ||
         cpi->interp_filter_selected[GOLDEN_FRAME][ifilter] * 50 <
             ref_total[GOLDEN_FRAME]) &&
        (ref_total[BWDREF_FRAME] == 0 ||
         cpi->interp_filter_selected[BWDREF_FRAME][ifilter] * 50 <
             ref_total[BWDREF_FRAME]) &&
        (ref_total[ALTREF2_FRAME] == 0 ||
         cpi->interp_filter_selected[ALTREF2_FRAME][ifilter] * 50 <
             ref_total[ALTREF2_FRAME]) &&
        (ref_total[ALTREF_FRAME] == 0 ||
         cpi->interp_filter_selected[arf_idx][ifilter] * 50 <
             ref_total[ALTREF_FRAME]))
      mask |= 1 << ifilter;
  }
  return mask;
}

#define DUMP_RECON_FRAMES 0

#if DUMP_RECON_FRAMES == 1
// NOTE(zoeliu): For debug - Output the filtered reconstructed video.
static void dump_filtered_recon_frames(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const YV12_BUFFER_CONFIG *recon_buf = cm->frame_to_show;

  if (recon_buf == NULL) {
    printf("Frame %d is not ready.\n", cm->current_video_frame);
    return;
  }

  static const int flag_list[REF_FRAMES] = { 0,
                                             AOM_LAST_FLAG,
                                             AOM_LAST2_FLAG,
                                             AOM_LAST3_FLAG,
                                             AOM_GOLD_FLAG,
                                             AOM_BWD_FLAG,
                                             AOM_ALT2_FLAG,
                                             AOM_ALT_FLAG };
  printf(
      "\n***Frame=%d (frame_offset=%d, show_frame=%d, "
      "show_existing_frame=%d) "
      "[LAST LAST2 LAST3 GOLDEN BWD ALT2 ALT]=[",
      cm->current_video_frame, cm->frame_offset, cm->show_frame,
      cm->show_existing_frame);
  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int buf_idx = cm->frame_refs[ref_frame - LAST_FRAME].idx;
    const int ref_offset =
        (buf_idx >= 0)
            ? (int)cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset
            : -1;
    printf(
        " %d(%c-%d-%4.2f)", ref_offset,
        (cpi->ref_frame_flags & flag_list[ref_frame]) ? 'Y' : 'N',
        (buf_idx >= 0) ? (int)cpi->frame_rf_level[buf_idx] : -1,
        (buf_idx >= 0) ? rate_factor_deltas[cpi->frame_rf_level[buf_idx]] : -1);
  }
  printf(" ]\n");

  if (!cm->show_frame) {
    printf("Frame %d is a no show frame, so no image dump.\n",
           cm->current_video_frame);
    return;
  }

  int h;
  char file_name[256] = "/tmp/enc_filtered_recon.yuv";
  FILE *f_recon = NULL;

  if (cm->current_video_frame == 0) {
    if ((f_recon = fopen(file_name, "wb")) == NULL) {
      printf("Unable to open file %s to write.\n", file_name);
      return;
    }
  } else {
    if ((f_recon = fopen(file_name, "ab")) == NULL) {
      printf("Unable to open file %s to append.\n", file_name);
      return;
    }
  }
  printf(
      "\nFrame=%5d, encode_update_type[%5d]=%1d, frame_offset=%d, "
      "show_frame=%d, show_existing_frame=%d, source_alt_ref_active=%d, "
      "refresh_alt_ref_frame=%d, rf_level=%d, "
      "y_stride=%4d, uv_stride=%4d, cm->width=%4d, cm->height=%4d\n\n",
      cm->current_video_frame, cpi->twopass.gf_group.index,
      cpi->twopass.gf_group.update_type[cpi->twopass.gf_group.index],
      cm->frame_offset, cm->show_frame, cm->show_existing_frame,
      cpi->rc.source_alt_ref_active, cpi->refresh_alt_ref_frame,
      cpi->twopass.gf_group.rf_level[cpi->twopass.gf_group.index],
      recon_buf->y_stride, recon_buf->uv_stride, cm->width, cm->height);
#if 0
  int ref_frame;
  printf("get_ref_frame_map_idx: [");
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame)
    printf(" %d", get_ref_frame_map_idx(cpi, ref_frame));
  printf(" ]\n");
  printf("cm->new_fb_idx = %d\n", cm->new_fb_idx);
  printf("cm->ref_frame_map = [");
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    printf(" %d", cm->ref_frame_map[ref_frame - LAST_FRAME]);
  }
  printf(" ]\n");
#endif  // 0

  // --- Y ---
  for (h = 0; h < cm->height; ++h) {
    fwrite(&recon_buf->y_buffer[h * recon_buf->y_stride], 1, cm->width,
           f_recon);
  }
  // --- U ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&recon_buf->u_buffer[h * recon_buf->uv_stride], 1, (cm->width >> 1),
           f_recon);
  }
  // --- V ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&recon_buf->v_buffer[h * recon_buf->uv_stride], 1, (cm->width >> 1),
           f_recon);
  }

  fclose(f_recon);
}
#endif  // DUMP_RECON_FRAMES

static int encode_frame_to_data_rate(AV1_COMP *cpi, size_t *size, uint8_t *dest,
                                     int skip_adapt,
                                     unsigned int *frame_flags) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  struct segmentation *const seg = &cm->seg;

  set_ext_overrides(cpi);
  aom_clear_system_state();

  // frame type has been decided outside of this function call
  cm->cur_frame->intra_only = frame_is_intra_only(cm);
  cm->cur_frame->frame_type = cm->frame_type;

  // S_FRAMEs are always error resilient
  cm->error_resilient_mode |= frame_is_sframe(cm);

  cm->large_scale_tile = cpi->oxcf.large_scale_tile;
  cm->single_tile_decoding = cpi->oxcf.single_tile_decoding;
  if (cm->large_scale_tile) cm->seq_params.frame_id_numbers_present_flag = 0;

  cm->allow_ref_frame_mvs &= frame_might_allow_ref_frame_mvs(cm);
  // cm->allow_ref_frame_mvs needs to be written into the frame header while
  // cm->large_scale_tile is 1, therefore, "cm->large_scale_tile=1" case is
  // separated from frame_might_allow_ref_frame_mvs().
  cm->allow_ref_frame_mvs &= !cm->large_scale_tile;

  cm->allow_warped_motion =
      cpi->oxcf.allow_warped_motion && frame_might_allow_warped_motion(cm);

  // Reset the frame packet stamp index.
  if (cm->frame_type == KEY_FRAME) cm->current_video_frame = 0;

  // NOTE:
  // (1) Move the setup of the ref_frame_flags upfront as it would be
  //     determined by the current frame properties;
  // (2) The setup of the ref_frame_flags applies to both
  // show_existing_frame's
  //     and the other cases.
  if (cm->current_video_frame > 0)
    cpi->ref_frame_flags = get_ref_frame_flags(cpi);

  if (cm->show_existing_frame) {
    // NOTE(zoeliu): In BIDIR_PRED, the existing frame to show is the current
    //               BWDREF_FRAME in the reference frame buffer.
    cm->frame_type = INTER_FRAME;
    cm->show_frame = 1;
    cpi->frame_flags = *frame_flags;

    // In the case of show_existing frame, we will not send fresh flag
    // to decoder. Any change in the reference frame buffer can be done by
    // switching the virtual indices.

    cpi->refresh_last_frame = 0;
    cpi->refresh_golden_frame = 0;
    cpi->refresh_bwd_ref_frame = 0;
    cpi->refresh_alt2_ref_frame = 0;
    cpi->refresh_alt_ref_frame = 0;

    cpi->rc.is_bwd_ref_frame = 0;
    cpi->rc.is_last_bipred_frame = 0;
    cpi->rc.is_bipred_frame = 0;

    restore_coding_context(cpi);

    // Build the bitstream
    if (av1_pack_bitstream(cpi, dest, size) != AOM_CODEC_OK)
      return AOM_CODEC_ERROR;

    cpi->seq_params_locked = 1;

    // Set up frame to show to get ready for stats collection.
    cm->frame_to_show = get_frame_new_buffer(cm);

    // Update current frame offset.
    cm->frame_offset =
        cm->buffer_pool->frame_bufs[cm->new_fb_idx].cur_frame_offset;

#if DUMP_RECON_FRAMES == 1
    // NOTE(zoeliu): For debug - Output the filtered reconstructed video.
    dump_filtered_recon_frames(cpi);
#endif  // DUMP_RECON_FRAMES

    // Update the LAST_FRAME in the reference frame buffer.
    // NOTE:
    // (1) For BWDREF_FRAME as the show_existing_frame, the reference frame
    //     update has been done previously when handling the LAST_BIPRED_FRAME
    //     right before BWDREF_FRAME (in the display order);
    // (2) For INTNL_OVERLAY as the show_existing_frame, the reference frame
    //     update will be done when the following is called, which will
    //     exchange
    //     the virtual indexes between LAST_FRAME and ALTREF2_FRAME, so that
    //     LAST3 will get retired, LAST2 becomes LAST3, LAST becomes LAST2,
    //     and
    //     ALTREF2_FRAME will serve as the new LAST_FRAME.
    update_reference_frames(cpi);

    // Update frame flags
    cpi->frame_flags &= ~FRAMEFLAGS_GOLDEN;
    cpi->frame_flags &= ~FRAMEFLAGS_BWDREF;
    cpi->frame_flags &= ~FRAMEFLAGS_ALTREF;

    *frame_flags = cpi->frame_flags & ~FRAMEFLAGS_KEY;

    // Update the frame type
    cm->last_frame_type = cm->frame_type;

    // Since we allocate a spot for the OVERLAY frame in the gf group, we need
    // to do post-encoding update accordingly.
    if (cpi->rc.is_src_frame_alt_ref) {
      av1_set_target_rate(cpi, cm->width, cm->height);
      av1_rc_postencode_update(cpi, *size);
    }

    ++cm->current_video_frame;

    return AOM_CODEC_OK;
  }

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  if (cpi->oxcf.pass == 2 && cpi->sf.adaptive_interp_filter_search)
    cpi->sf.interp_filter_search_mask = setup_interp_filter_search_mask(cpi);

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm) || frame_is_sframe(cm)) {
    // Reset the loop filter deltas and segmentation map.
    av1_reset_segment_features(cm);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->rc.source_alt_ref_active = 0;
  }
  if (cpi->oxcf.mtu == 0) {
    cm->num_tg = cpi->oxcf.num_tile_groups;
  } else {
    // Use a default value for the purposes of weighting costs in probability
    // updates
    cm->num_tg = DEFAULT_MAX_NUM_TG;
  }

  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame.
  if (oxcf->pass == 0 && oxcf->rc_mode == AOM_CBR &&
      cm->frame_type != KEY_FRAME) {
    if (av1_rc_drop_frame(cpi)) {
      av1_rc_postencode_update_drop_frame(cpi);
      return AOM_CODEC_OK;
    }
  }

  aom_clear_system_state();

#if CONFIG_INTERNAL_STATS
  memset(cpi->mode_chosen_counts, 0,
         MAX_MODES * sizeof(*cpi->mode_chosen_counts));
#endif

  if (cm->seq_params.frame_id_numbers_present_flag) {
    /* Non-normative definition of current_frame_id ("frame counter" with
     * wraparound) */
    const int frame_id_length = FRAME_ID_LENGTH;
    if (cm->current_frame_id == -1) {
      int lsb, msb;
      /* quasi-random initialization of current_frame_id for a key frame */
      if (cpi->source->flags & YV12_FLAG_HIGHBITDEPTH) {
        lsb = CONVERT_TO_SHORTPTR(cpi->source->y_buffer)[0] & 0xff;
        msb = CONVERT_TO_SHORTPTR(cpi->source->y_buffer)[1] & 0xff;
      } else {
        lsb = cpi->source->y_buffer[0] & 0xff;
        msb = cpi->source->y_buffer[1] & 0xff;
      }
      cm->current_frame_id = ((msb << 8) + lsb) % (1 << frame_id_length);

      // S_frame is meant for stitching different streams of different
      // resolutions together, so current_frame_id must be the
      // same across different streams of the same content current_frame_id
      // should be the same and not random. 0x37 is a chosen number as start
      // point
      if (cpi->oxcf.sframe_enabled) cm->current_frame_id = 0x37;
    } else {
      cm->current_frame_id =
          (cm->current_frame_id + 1 + (1 << frame_id_length)) %
          (1 << frame_id_length);
    }
  }

  switch (cpi->oxcf.cdf_update_mode) {
    case 0:  // No CDF update for any frames(4~6% compression loss).
      cm->disable_cdf_update = 1;
      break;
    case 1:  // Enable CDF update for all frames.
      cm->disable_cdf_update = 0;
      break;
    case 2:
      // Strategically determine at which frames to do CDF update.
      // Currently only enable CDF update for all-intra and no-show frames(1.5%
      // compression loss).
      // TODO(huisu@google.com): design schemes for various trade-offs between
      // compression quality and decoding speed.
      cm->disable_cdf_update =
          (frame_is_intra_only(cm) || !cm->show_frame) ? 0 : 1;
      break;
  }
  cm->timing_info_present &= !cm->seq_params.reduced_still_picture_hdr;

  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    if (encode_without_recode_loop(cpi) != AOM_CODEC_OK) return AOM_CODEC_ERROR;
  } else {
    if (encode_with_recode_loop(cpi, size, dest) != AOM_CODEC_OK)
      return AOM_CODEC_ERROR;
  }

  cm->last_tile_cols = cm->tile_cols;
  cm->last_tile_rows = cm->tile_rows;

#ifdef OUTPUT_YUV_SKINMAP
  if (cpi->common.current_video_frame > 1) {
    av1_compute_skin_map(cpi, yuv_skinmap_file);
  }
#endif  // OUTPUT_YUV_SKINMAP

  // Special case code to reduce pulsing when key frames are forced at a
  // fixed interval. Note the reconstruction error if it is the frame before
  // the force key frame
  if (cpi->rc.next_key_frame_forced && cpi->rc.frames_to_key == 1) {
    if (cm->use_highbitdepth) {
      cpi->ambient_err =
          aom_highbd_get_y_sse(cpi->source, get_frame_new_buffer(cm));
    } else {
      cpi->ambient_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
    }
  }

  // If the encoder forced a KEY_FRAME decision or if frame is an S_FRAME
  if (cm->frame_type == KEY_FRAME || frame_is_sframe(cm)) {
    cpi->refresh_last_frame = 1;
  }

  cm->frame_to_show = get_frame_new_buffer(cm);
  cm->frame_to_show->color_primaries = cm->color_primaries;
  cm->frame_to_show->transfer_characteristics = cm->transfer_characteristics;
  cm->frame_to_show->matrix_coefficients = cm->matrix_coefficients;
  cm->frame_to_show->monochrome = cm->seq_params.monochrome;
  cm->frame_to_show->chroma_sample_position = cm->chroma_sample_position;
  cm->frame_to_show->color_range = cm->color_range;
  cm->frame_to_show->render_width = cm->render_width;
  cm->frame_to_show->render_height = cm->render_height;

  // TODO(zoeliu): For non-ref frames, loop filtering may need to be turned
  // off.

  // Pick the loop filter level for the frame.
  if (!cm->allow_intrabc) {
    loopfilter_frame(cpi, cm);
  } else {
    cm->lf.filter_level[0] = 0;
    cm->lf.filter_level[1] = 0;
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->nb_cdef_strengths = 1;
    cm->cdef_uv_strengths[0] = 0;
    cm->rst_info[0].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[1].frame_restoration_type = RESTORE_NONE;
    cm->rst_info[2].frame_restoration_type = RESTORE_NONE;
  }

  // TODO(debargha): Fix mv search range on encoder side
  // aom_extend_frame_inner_borders(cm->frame_to_show, av1_num_planes(cm));
  aom_extend_frame_borders(cm->frame_to_show, av1_num_planes(cm));

#ifdef OUTPUT_YUV_REC
  aom_write_one_yuv_frame(cm, cm->frame_to_show);
#endif

  // Build the bitstream
  if (av1_pack_bitstream(cpi, dest, size) != AOM_CODEC_OK)
    return AOM_CODEC_ERROR;

  cpi->seq_params_locked = 1;

  if (skip_adapt) return AOM_CODEC_OK;

  if (cm->seq_params.frame_id_numbers_present_flag) {
    int i;
    // Update reference frame id values based on the value of refresh_frame_mask
    for (i = 0; i < REF_FRAMES; i++) {
      if ((cpi->refresh_frame_mask >> i) & 1) {
        cm->ref_frame_id[i] = cm->current_frame_id;
      }
    }
  }

#if DUMP_RECON_FRAMES == 1
  // NOTE(zoeliu): For debug - Output the filtered reconstructed video.
  dump_filtered_recon_frames(cpi);
#endif  // DUMP_RECON_FRAMES

  if (cm->seg.enabled) {
    if (cm->seg.update_map) {
      update_reference_segmentation_map(cpi);
    } else if (cm->last_frame_seg_map) {
      memcpy(cm->current_frame_seg_map, cm->last_frame_seg_map,
             cm->mi_cols * cm->mi_rows * sizeof(uint8_t));
    }
  }

  if (frame_is_intra_only(cm) == 0) {
    release_scaled_references(cpi);
  }

  update_reference_frames(cpi);

#if CONFIG_ENTROPY_STATS
  av1_accumulate_frame_counts(&aggregate_fc, &cpi->counts);
#endif  // CONFIG_ENTROPY_STATS

  if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
    *cm->fc = cpi->tile_data[cm->largest_tile_id].tctx;
    av1_reset_cdf_symbol_counters(cm->fc);
  }

  if (cpi->refresh_golden_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_GOLDEN;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_ALTREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_ALTREF;

  if (cpi->refresh_bwd_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_BWDREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_BWDREF;

  cm->last_frame_type = cm->frame_type;

  av1_rc_postencode_update(cpi, *size);

  if (cm->frame_type == KEY_FRAME) {
    // Tell the caller that the frame was coded as a key frame
    *frame_flags = cpi->frame_flags | FRAMEFLAGS_KEY;
  } else {
    *frame_flags = cpi->frame_flags & ~FRAMEFLAGS_KEY;
  }

  // Clear the one shot update flags for segmentation map and mode/ref loop
  // filter deltas.
  cm->seg.update_map = 0;
  cm->seg.update_data = 0;
  cm->lf.mode_ref_delta_update = 0;

  if (cm->show_frame) {
    // TODO(zoeliu): We may only swamp mi and prev_mi for those frames that
    // are
    // being used as reference.
    swap_mi_and_prev_mi(cm);
    // Don't increment frame counters if this was an altref buffer
    // update not a real frame
    ++cm->current_video_frame;
  }

  // NOTE: Shall not refer to any frame not used as reference.
  if (cm->is_reference_frame) {
    // keep track of the last coded dimensions
    cm->last_width = cm->width;
    cm->last_height = cm->height;

    // reset to normal state now that we are done.
    cm->last_show_frame = cm->show_frame;
  }

  return AOM_CODEC_OK;
}

static int Pass0Encode(AV1_COMP *cpi, size_t *size, uint8_t *dest,
                       int skip_adapt, unsigned int *frame_flags) {
  if (cpi->oxcf.rc_mode == AOM_CBR) {
    av1_rc_get_one_pass_cbr_params(cpi);
  } else {
    av1_rc_get_one_pass_vbr_params(cpi);
  }
  if (encode_frame_to_data_rate(cpi, size, dest, skip_adapt, frame_flags) !=
      AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }
  check_show_existing_frame(cpi);
  return AOM_CODEC_OK;
}

static int Pass2Encode(AV1_COMP *cpi, size_t *size, uint8_t *dest,
                       unsigned int *frame_flags) {
#if CONFIG_MISMATCH_DEBUG
  mismatch_move_frame_idx_w();
#endif
#if TXCOEFF_COST_TIMER
  AV1_COMMON *cm = &cpi->common;
  cm->txcoeff_cost_timer = 0;
  cm->txcoeff_cost_count = 0;
#endif

  if (encode_frame_to_data_rate(cpi, size, dest, 0, frame_flags) !=
      AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }

#if TXCOEFF_COST_TIMER
  cm->cum_txcoeff_cost_timer += cm->txcoeff_cost_timer;
  fprintf(stderr,
          "\ntxb coeff cost block number: %ld, frame time: %ld, cum time %ld "
          "in us\n",
          cm->txcoeff_cost_count, cm->txcoeff_cost_timer,
          cm->cum_txcoeff_cost_timer);
#endif

  // Do not do post-encoding update for those frames that do not have a spot
  // in
  // a gf group, but note that an OVERLAY frame always has a spot in a gf
  // group,
  // even when show_existing_frame is used.
  if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref) {
    av1_twopass_postencode_update(cpi);
  }
  check_show_existing_frame(cpi);
  return AOM_CODEC_OK;
}

int av1_receive_raw_frame(AV1_COMP *cpi, aom_enc_frame_flags_t frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  AV1_COMMON *const cm = &cpi->common;
  struct aom_usec_timer timer;
  int res = 0;
  const int subsampling_x = sd->subsampling_x;
  const int subsampling_y = sd->subsampling_y;
  const int use_highbitdepth = (sd->flags & YV12_FLAG_HIGHBITDEPTH) != 0;

  check_initial_width(cpi, use_highbitdepth, subsampling_x, subsampling_y);

  aom_usec_timer_start(&timer);

  if (av1_lookahead_push(cpi->lookahead, sd, time_stamp, end_time,
                         use_highbitdepth, frame_flags))
    res = -1;
  aom_usec_timer_mark(&timer);
  cpi->time_receive_data += aom_usec_timer_elapsed(&timer);

  if ((cm->profile == PROFILE_0) && !cm->seq_params.monochrome &&
      (subsampling_x != 1 || subsampling_y != 1)) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "Non-4:2:0 color format requires profile 1 or 2");
    res = -1;
  }
  if ((cm->profile == PROFILE_1) &&
      !(subsampling_x == 0 && subsampling_y == 0)) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "Profile 1 requires 4:4:4 color format");
    res = -1;
  }
  if ((cm->profile == PROFILE_2) && (cm->bit_depth <= AOM_BITS_10) &&
      !(subsampling_x == 1 && subsampling_y == 0)) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "Profile 2 bit-depth < 10 requires 4:2:2 color format");
    res = -1;
  }

  return res;
}

static int frame_is_reference(const AV1_COMP *cpi) {
  const AV1_COMMON *cm = &cpi->common;

  return cm->frame_type == KEY_FRAME || cpi->refresh_last_frame ||
         cpi->refresh_golden_frame || cpi->refresh_bwd_ref_frame ||
         cpi->refresh_alt2_ref_frame || cpi->refresh_alt_ref_frame ||
         !cm->error_resilient_mode || cm->lf.mode_ref_delta_update ||
         cm->seg.update_map || cm->seg.update_data;
}

static void adjust_frame_rate(AV1_COMP *cpi,
                              const struct lookahead_entry *source) {
  int64_t this_duration;
  int step = 0;

  if (source->ts_start == cpi->first_time_stamp_ever) {
    this_duration = source->ts_end - source->ts_start;
    step = 1;
  } else {
    int64_t last_duration =
        cpi->last_end_time_stamp_seen - cpi->last_time_stamp_seen;

    this_duration = source->ts_end - cpi->last_end_time_stamp_seen;

    // do a step update if the duration changes by 10%
    if (last_duration)
      step = (int)((this_duration - last_duration) * 10 / last_duration);
  }

  if (this_duration) {
    if (step) {
      av1_new_framerate(cpi, 10000000.0 / this_duration);
    } else {
      // Average this frame's rate into the last second's average
      // frame rate. If we haven't seen 1 second yet, then average
      // over the whole interval seen.
      const double interval = AOMMIN(
          (double)(source->ts_end - cpi->first_time_stamp_ever), 10000000.0);
      double avg_duration = 10000000.0 / cpi->framerate;
      avg_duration *= (interval - avg_duration + this_duration);
      avg_duration /= interval;

      av1_new_framerate(cpi, 10000000.0 / avg_duration);
    }
  }
  cpi->last_time_stamp_seen = source->ts_start;
  cpi->last_end_time_stamp_seen = source->ts_end;
}

// Returns 0 if this is not an alt ref else the offset of the source frame
// used as the arf midpoint.
static int get_arf_src_index(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  int arf_src_index = 0;
  if (is_altref_enabled(cpi)) {
    if (cpi->oxcf.pass == 2) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      if (gf_group->update_type[gf_group->index] == ARF_UPDATE) {
        arf_src_index = gf_group->arf_src_offset[gf_group->index];
      }
    } else if (rc->source_alt_ref_pending) {
      arf_src_index = rc->frames_till_gf_update_due;
    }
  }
  return arf_src_index;
}

static int get_brf_src_index(AV1_COMP *cpi) {
  int brf_src_index = 0;
  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;

  // TODO(zoeliu): We need to add the check on the -bwd_ref command line setup
  //               flag.
  if (gf_group->bidir_pred_enabled[gf_group->index]) {
    if (cpi->oxcf.pass == 2) {
      if (gf_group->update_type[gf_group->index] == BRF_UPDATE)
        brf_src_index = gf_group->brf_src_offset[gf_group->index];
    } else {
      // TODO(zoeliu): To re-visit the setup for this scenario
      brf_src_index = cpi->rc.bipred_group_interval - 1;
    }
  }

  return brf_src_index;
}

// Returns 0 if this is not an alt ref else the offset of the source frame
// used as the arf midpoint.
static int get_arf2_src_index(AV1_COMP *cpi) {
  int arf2_src_index = 0;
  if (is_altref_enabled(cpi) && cpi->num_extra_arfs) {
    if (cpi->oxcf.pass == 2) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      if (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE) {
        arf2_src_index = gf_group->arf_src_offset[gf_group->index];
      }
    }
  }
  return arf2_src_index;
}

static void check_src_altref(AV1_COMP *cpi,
                             const struct lookahead_entry *source) {
  RATE_CONTROL *const rc = &cpi->rc;

  // If pass == 2, the parameters set here will be reset in
  // av1_rc_get_second_pass_params()

  if (cpi->oxcf.pass == 2) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    rc->is_src_frame_alt_ref =
        (gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE) ||
        (gf_group->update_type[gf_group->index] == OVERLAY_UPDATE);
    rc->is_src_frame_ext_arf =
        gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE;
  } else {
    rc->is_src_frame_alt_ref =
        cpi->alt_ref_source && (source == cpi->alt_ref_source);
  }

  if (rc->is_src_frame_alt_ref) {
    // Current frame is an ARF overlay frame.
    cpi->alt_ref_source = NULL;

    if (rc->is_src_frame_ext_arf && !cpi->common.show_existing_frame) {
      // For INTNL_OVERLAY, when show_existing_frame == 0, they do need to
      // refresh the LAST_FRAME, i.e. LAST3 gets retired, LAST2 becomes LAST3,
      // LAST becomes LAST2, and INTNL_OVERLAY becomes LAST.
      cpi->refresh_last_frame = 1;
    } else {
      // Don't refresh the last buffer for an ARF overlay frame. It will
      // become the GF so preserve last as an alternative prediction option.
      cpi->refresh_last_frame = 0;
    }
  }
}

#if CONFIG_INTERNAL_STATS
extern double av1_get_blockiness(const unsigned char *img1, int img1_pitch,
                                 const unsigned char *img2, int img2_pitch,
                                 int width, int height);

static void adjust_image_stat(double y, double u, double v, double all,
                              ImageStat *s) {
  s->stat[STAT_Y] += y;
  s->stat[STAT_U] += u;
  s->stat[STAT_V] += v;
  s->stat[STAT_ALL] += all;
  s->worst = AOMMIN(s->worst, all);
}

static void compute_internal_stats(AV1_COMP *cpi, int frame_bytes) {
  AV1_COMMON *const cm = &cpi->common;
  double samples = 0.0;
  uint32_t in_bit_depth = 8;
  uint32_t bit_depth = 8;

#if CONFIG_INTER_STATS_ONLY
  if (cm->frame_type == KEY_FRAME) return;  // skip key frame
#endif
  cpi->bytes += frame_bytes;

  if (cm->use_highbitdepth) {
    in_bit_depth = cpi->oxcf.input_bit_depth;
    bit_depth = cm->bit_depth;
  }
  if (cm->show_frame) {
    const YV12_BUFFER_CONFIG *orig = cpi->source;
    const YV12_BUFFER_CONFIG *recon = cpi->common.frame_to_show;
    double y, u, v, frame_all;

    cpi->count++;
    if (cpi->b_calculate_psnr) {
      PSNR_STATS psnr;
      double frame_ssim2 = 0.0, weight = 0.0;
      aom_clear_system_state();
      // TODO(yaowu): unify these two versions into one.
      aom_calc_highbd_psnr(orig, recon, &psnr, bit_depth, in_bit_depth);

      adjust_image_stat(psnr.psnr[1], psnr.psnr[2], psnr.psnr[3], psnr.psnr[0],
                        &cpi->psnr);
      cpi->total_sq_error += psnr.sse[0];
      cpi->total_samples += psnr.samples[0];
      samples = psnr.samples[0];
      // TODO(yaowu): unify these two versions into one.
      if (cm->use_highbitdepth)
        frame_ssim2 =
            aom_highbd_calc_ssim(orig, recon, &weight, bit_depth, in_bit_depth);
      else
        frame_ssim2 = aom_calc_ssim(orig, recon, &weight);

      cpi->worst_ssim = AOMMIN(cpi->worst_ssim, frame_ssim2);
      cpi->summed_quality += frame_ssim2 * weight;
      cpi->summed_weights += weight;

#if 0
      {
        FILE *f = fopen("q_used.stt", "a");
        double y2 = psnr.psnr[1];
        double u2 = psnr.psnr[2];
        double v2 = psnr.psnr[3];
        double frame_psnr2 = psnr.psnr[0];
        fprintf(f, "%5d : Y%f7.3:U%f7.3:V%f7.3:F%f7.3:S%7.3f\n",
                cm->current_video_frame, y2, u2, v2,
                frame_psnr2, frame_ssim2);
        fclose(f);
      }
#endif
    }
    if (cpi->b_calculate_blockiness) {
      if (!cm->use_highbitdepth) {
        const double frame_blockiness =
            av1_get_blockiness(orig->y_buffer, orig->y_stride, recon->y_buffer,
                               recon->y_stride, orig->y_width, orig->y_height);
        cpi->worst_blockiness = AOMMAX(cpi->worst_blockiness, frame_blockiness);
        cpi->total_blockiness += frame_blockiness;
      }

      if (cpi->b_calculate_consistency) {
        if (!cm->use_highbitdepth) {
          const double this_inconsistency = aom_get_ssim_metrics(
              orig->y_buffer, orig->y_stride, recon->y_buffer, recon->y_stride,
              orig->y_width, orig->y_height, cpi->ssim_vars, &cpi->metrics, 1);

          const double peak = (double)((1 << in_bit_depth) - 1);
          const double consistency =
              aom_sse_to_psnr(samples, peak, cpi->total_inconsistency);
          if (consistency > 0.0)
            cpi->worst_consistency =
                AOMMIN(cpi->worst_consistency, consistency);
          cpi->total_inconsistency += this_inconsistency;
        }
      }
    }

    frame_all =
        aom_calc_fastssim(orig, recon, &y, &u, &v, bit_depth, in_bit_depth);
    adjust_image_stat(y, u, v, frame_all, &cpi->fastssim);
    frame_all = aom_psnrhvs(orig, recon, &y, &u, &v, bit_depth, in_bit_depth);
    adjust_image_stat(y, u, v, frame_all, &cpi->psnrhvs);
  }
}
#endif  // CONFIG_INTERNAL_STATS

static int is_integer_mv(AV1_COMP *cpi, const YV12_BUFFER_CONFIG *cur_picture,
                         const YV12_BUFFER_CONFIG *last_picture,
                         hash_table *last_hash_table) {
  aom_clear_system_state();
  // check use hash ME
  int k;
  uint32_t hash_value_1;
  uint32_t hash_value_2;

  const int block_size = 8;
  const double threshold_current = 0.8;
  const double threshold_average = 0.95;
  const int max_history_size = 32;
  int T = 0;  // total block
  int C = 0;  // match with collocated block
  int S = 0;  // smooth region but not match with collocated block
  int M = 0;  // match with other block

  const int pic_width = cur_picture->y_width;
  const int pic_height = cur_picture->y_height;
  for (int i = 0; i + block_size <= pic_height; i += block_size) {
    for (int j = 0; j + block_size <= pic_width; j += block_size) {
      const int x_pos = j;
      const int y_pos = i;
      int match = 1;
      T++;

      // check whether collocated block match with current
      uint8_t *p_cur = cur_picture->y_buffer;
      uint8_t *p_ref = last_picture->y_buffer;
      int stride_cur = cur_picture->y_stride;
      int stride_ref = last_picture->y_stride;
      p_cur += (y_pos * stride_cur + x_pos);
      p_ref += (y_pos * stride_ref + x_pos);

      if (cur_picture->flags & YV12_FLAG_HIGHBITDEPTH) {
        uint16_t *p16_cur = CONVERT_TO_SHORTPTR(p_cur);
        uint16_t *p16_ref = CONVERT_TO_SHORTPTR(p_ref);
        for (int tmpY = 0; tmpY < block_size && match; tmpY++) {
          for (int tmpX = 0; tmpX < block_size && match; tmpX++) {
            if (p16_cur[tmpX] != p16_ref[tmpX]) {
              match = 0;
            }
          }
          p16_cur += stride_cur;
          p16_ref += stride_ref;
        }
      } else {
        for (int tmpY = 0; tmpY < block_size && match; tmpY++) {
          for (int tmpX = 0; tmpX < block_size && match; tmpX++) {
            if (p_cur[tmpX] != p_ref[tmpX]) {
              match = 0;
            }
          }
          p_cur += stride_cur;
          p_ref += stride_ref;
        }
      }

      if (match) {
        C++;
        continue;
      }

      if (av1_hash_is_horizontal_perfect(cur_picture, block_size, x_pos,
                                         y_pos) ||
          av1_hash_is_vertical_perfect(cur_picture, block_size, x_pos, y_pos)) {
        S++;
        continue;
      }

      av1_get_block_hash_value(
          cur_picture->y_buffer + y_pos * stride_cur + x_pos, stride_cur,
          block_size, &hash_value_1, &hash_value_2,
          (cur_picture->flags & YV12_FLAG_HIGHBITDEPTH));
      // Hashing does not work for highbitdepth currently.
      // TODO(Roger): Make it work for highbitdepth.
      if (av1_use_hash_me(&cpi->common)) {
        if (av1_has_exact_match(last_hash_table, hash_value_1, hash_value_2)) {
          M++;
        }
      }
    }
  }

  assert(T > 0);
  double csm_rate = ((double)(C + S + M)) / ((double)(T));
  double m_rate = ((double)(M)) / ((double)(T));

  cpi->csm_rate_array[cpi->rate_index] = csm_rate;
  cpi->m_rate_array[cpi->rate_index] = m_rate;

  cpi->rate_index = (cpi->rate_index + 1) % max_history_size;
  cpi->rate_size++;
  cpi->rate_size = AOMMIN(cpi->rate_size, max_history_size);

  if (csm_rate < threshold_current) {
    return 0;
  }

  if (C == T) {
    return 1;
  }

  double csm_average = 0.0;
  double m_average = 0.0;

  for (k = 0; k < cpi->rate_size; k++) {
    csm_average += cpi->csm_rate_array[k];
    m_average += cpi->m_rate_array[k];
  }
  csm_average /= cpi->rate_size;
  m_average /= cpi->rate_size;

  if (csm_average < threshold_average) {
    return 0;
  }

  if (M > (T - C - S) / 3) {
    return 1;
  }

  if (csm_rate > 0.99 && m_rate > 0.01) {
    return 1;
  }

  if (csm_average + m_average > 1.01) {
    return 1;
  }

  return 0;
}

int av1_get_compressed_data(AV1_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest, int64_t *time_stamp,
                            int64_t *time_end, int flush,
                            const aom_rational_t *timebase) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  BufferPool *const pool = cm->buffer_pool;
  RATE_CONTROL *const rc = &cpi->rc;
  struct aom_usec_timer cmptimer;
  YV12_BUFFER_CONFIG *force_src_buffer = NULL;
  struct lookahead_entry *last_source = NULL;
  struct lookahead_entry *source = NULL;
  int arf_src_index;
  int brf_src_index;
  int i;

#if CONFIG_BITSTREAM_DEBUG
  assert(cpi->oxcf.max_threads == 0 &&
         "bitstream debug tool does not support multithreading");
  bitstream_queue_record_write();
  bitstream_queue_set_frame_write(cm->current_video_frame * 2 + cm->show_frame);
#endif

  cm->showable_frame = 0;
  aom_usec_timer_start(&cmptimer);

  set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV, 0);

  // Is multi-arf enabled.
  // Note that at the moment multi_arf is only configured for 2 pass VBR
  if ((oxcf->pass == 2) && (cpi->oxcf.enable_auto_arf > 1))
    cpi->multi_arf_allowed = 1;
  else
    cpi->multi_arf_allowed = 0;

  // Normal defaults
  cm->refresh_frame_context = oxcf->frame_parallel_decoding_mode
                                  ? REFRESH_FRAME_CONTEXT_DISABLED
                                  : REFRESH_FRAME_CONTEXT_BACKWARD;
  if (oxcf->large_scale_tile)
    cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
  cpi->refresh_bwd_ref_frame = 0;
  cpi->refresh_alt2_ref_frame = 0;
  cpi->refresh_alt_ref_frame = 0;

  // TODO(zoeliu@gmail.com): To support forward-KEY_FRAME and set up the
  //                         following flag accordingly.
  cm->reset_decoder_state = 0;

  // Don't allow a show_existing_frame to coincide with an error resilient or
  // S-Frame
  struct lookahead_entry *lookahead_src = NULL;
  if (cm->current_video_frame > 0)
    lookahead_src = av1_lookahead_peek(cpi->lookahead, 0);
  if (lookahead_src != NULL &&
      ((cpi->oxcf.error_resilient_mode |
        ((lookahead_src->flags & AOM_EFLAG_ERROR_RESILIENT) != 0)) ||
       (cpi->oxcf.s_frame_mode |
        ((lookahead_src->flags & AOM_EFLAG_SET_S_FRAME) != 0)))) {
    cm->show_existing_frame = 0;
  }

  if (oxcf->pass == 2 && cm->show_existing_frame) {
    // Manage the source buffer and flush out the source frame that has been
    // coded already; Also get prepared for PSNR calculation if needed.
    if ((source = av1_lookahead_pop(cpi->lookahead, flush)) == NULL) {
      *size = 0;
      return -1;
    }
    av1_apply_encoding_flags(cpi, source->flags);
    cpi->source = &source->img;
    // TODO(zoeliu): To track down to determine whether it's needed to adjust
    // the frame rate.
    *time_stamp = source->ts_start;
    *time_end = source->ts_end;

    // We need to adjust frame rate for an overlay frame
    if (cpi->rc.is_src_frame_alt_ref) adjust_frame_rate(cpi, source);

    // Find a free buffer for the new frame, releasing the reference
    // previously
    // held.
    if (cm->new_fb_idx != INVALID_IDX) {
      --pool->frame_bufs[cm->new_fb_idx].ref_count;
    }
    cm->new_fb_idx = get_free_fb(cm);

    if (cm->new_fb_idx == INVALID_IDX) return -1;

    // Clear down mmx registers
    aom_clear_system_state();

    // Start with a 0 size frame.
    *size = 0;

    // We need to update the gf_group for show_existing overlay frame
    if (cpi->rc.is_src_frame_alt_ref) av1_rc_get_second_pass_params(cpi);

    if (Pass2Encode(cpi, size, dest, frame_flags) != AOM_CODEC_OK)
      return AOM_CODEC_ERROR;

    if (cpi->b_calculate_psnr) generate_psnr_packet(cpi);

#if CONFIG_INTERNAL_STATS
    compute_internal_stats(cpi, (int)(*size));
#endif  // CONFIG_INTERNAL_STATS

    // Clear down mmx registers
    aom_clear_system_state();

    cm->show_existing_frame = 0;
    return 0;
  }

  // Should we encode an arf frame.
  arf_src_index = get_arf_src_index(cpi);
  if (arf_src_index) {
    for (i = 0; i <= arf_src_index; ++i) {
      struct lookahead_entry *e = av1_lookahead_peek(cpi->lookahead, i);
      // Avoid creating an alt-ref if there's a forced keyframe pending.
      if (e == NULL) {
        break;
      } else if (e->flags == AOM_EFLAG_FORCE_KF) {
        arf_src_index = 0;
        flush = 1;
        break;
      }
    }
  }

  if (arf_src_index) {
    assert(arf_src_index <= rc->frames_to_key);

    if ((source = av1_lookahead_peek(cpi->lookahead, arf_src_index)) != NULL) {
      cm->showable_frame = 1;
      cpi->alt_ref_source = source;

      if (oxcf->arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        av1_temporal_filter(cpi, arf_src_index);
        aom_extend_frame_borders(&cpi->alt_ref_buffer, num_planes);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cm->intra_only = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      rc->is_src_frame_alt_ref = 0;
    }
    rc->source_alt_ref_pending = 0;
  }

  // Should we encode an arf2 frame.
  arf_src_index = get_arf2_src_index(cpi);
  if (arf_src_index) {
    for (i = 0; i <= arf_src_index; ++i) {
      struct lookahead_entry *e = av1_lookahead_peek(cpi->lookahead, i);
      // Avoid creating an alt-ref if there's a forced keyframe pending.
      if (e == NULL) {
        break;
      } else if (e->flags == AOM_EFLAG_FORCE_KF) {
        arf_src_index = 0;
        flush = 1;
        break;
      }
    }
  }

  if (arf_src_index) {
    assert(arf_src_index <= rc->frames_to_key);

    if ((source = av1_lookahead_peek(cpi->lookahead, arf_src_index)) != NULL) {
      cm->showable_frame = 1;
      cpi->alt_ref_source = source;

      if (oxcf->arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        av1_temporal_filter(cpi, arf_src_index);
        aom_extend_frame_borders(&cpi->alt_ref_buffer, num_planes);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cm->intra_only = 0;
      cpi->refresh_alt2_ref_frame = 1;
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;
      rc->is_src_frame_alt_ref = 0;
      rc->is_src_frame_ext_arf = 0;
    }
    rc->source_alt_ref_pending = 0;
  }

  rc->is_bwd_ref_frame = 0;
  brf_src_index = get_brf_src_index(cpi);
  if (brf_src_index) {
    assert(brf_src_index <= rc->frames_to_key);
    if ((source = av1_lookahead_peek(cpi->lookahead, brf_src_index)) != NULL) {
      cm->showable_frame = 1;
      cm->show_frame = 0;
      cm->intra_only = 0;

      cpi->refresh_bwd_ref_frame = 1;
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
      cpi->refresh_alt_ref_frame = 0;

      rc->is_bwd_ref_frame = 1;
    }
  }

  if (!source) {
    // Get last frame source.
    if (cm->current_video_frame > 0) {
      if ((last_source = av1_lookahead_peek(cpi->lookahead, -1)) == NULL)
        return -1;
    }
    if (cm->current_video_frame > 0) assert(last_source != NULL);
    // Read in the source frame.
    source = av1_lookahead_pop(cpi->lookahead, flush);

    if (source != NULL) {
      cm->show_frame = 1;
      cm->intra_only = 0;

      // Check to see if the frame should be encoded as an arf overlay.
      check_src_altref(cpi, source);
    }
  }
  if (source) {
    cpi->unscaled_source = cpi->source =
        force_src_buffer ? force_src_buffer : &source->img;
    cpi->unscaled_last_source = last_source != NULL ? &last_source->img : NULL;

    *time_stamp = source->ts_start;
    *time_end = source->ts_end;
    av1_apply_encoding_flags(cpi, source->flags);
    *frame_flags = (source->flags & AOM_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

  } else {
    *size = 0;
    if (flush && oxcf->pass == 1 && !cpi->twopass.first_pass_done) {
      av1_end_first_pass(cpi); /* get last stats packet */
      cpi->twopass.first_pass_done = 1;
    }
    return -1;
  }

  if (source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = source->ts_start;
    cpi->last_end_time_stamp_seen = source->ts_start;
  }

  // Clear down mmx registers
  aom_clear_system_state();

  // adjust frame rates based on timestamps given
  if (cm->show_frame) adjust_frame_rate(cpi, source);

  // Find a free buffer for the new frame, releasing the reference previously
  // held.
  if (cm->new_fb_idx != INVALID_IDX) {
    --pool->frame_bufs[cm->new_fb_idx].ref_count;
  }
  cm->new_fb_idx = get_free_fb(cm);

  if (cm->new_fb_idx == INVALID_IDX) return -1;

  // Retain the RF_LEVEL for the current newly coded frame.
  cpi->frame_rf_level[cm->new_fb_idx] =
      cpi->twopass.gf_group.rf_level[cpi->twopass.gf_group.index];

  cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];
  cm->cur_frame->buf.buf_8bit_valid = 0;

  if (cm->film_grain_table) {
    cm->film_grain_params_present = aom_film_grain_table_lookup(
        cm->film_grain_table, *time_stamp, *time_end, 0 /* erase */,
        &cm->film_grain_params);
  }
  cm->cur_frame->film_grain_params_present = cm->film_grain_params_present;

  // only one operating point supported now
  cpi->common.tu_presentation_delay =
      ticks_to_timebase_units(timebase, *time_stamp);

  // Start with a 0 size frame.
  *size = 0;

  cpi->frame_flags = *frame_flags;

  if (oxcf->pass == 2) {
    av1_rc_get_second_pass_params(cpi);
  } else if (oxcf->pass == 1) {
    setup_frame_size(cpi);
  }

  if (cpi->oxcf.pass != 0 || frame_is_intra_only(cm) == 1) {
    for (i = 0; i < REF_FRAMES; ++i) cpi->scaled_ref_idx[i] = INVALID_IDX;
  }

  cm->using_qmatrix = cpi->oxcf.using_qm;
  cm->min_qmlevel = cpi->oxcf.qm_minlevel;
  cm->max_qmlevel = cpi->oxcf.qm_maxlevel;

  if (cm->seq_params.frame_id_numbers_present_flag) {
    if (*time_stamp == 0) {
      cpi->common.current_frame_id = -1;
    }
  }

  cpi->cur_poc++;
  if (oxcf->pass != 1 && cpi->common.allow_screen_content_tools &&
      !frame_is_intra_only(cm)) {
    if (cpi->common.seq_params.force_integer_mv == 2) {
      struct lookahead_entry *previous_entry =
          av1_lookahead_peek(cpi->lookahead, cpi->previous_index);
      if (!previous_entry)
        cpi->common.cur_frame_force_integer_mv = 0;
      else
        cpi->common.cur_frame_force_integer_mv = is_integer_mv(
            cpi, cpi->source, &previous_entry->img, cpi->previous_hash_table);
    } else {
      cpi->common.cur_frame_force_integer_mv =
          cpi->common.seq_params.force_integer_mv;
    }
  } else {
    cpi->common.cur_frame_force_integer_mv = 0;
  }

  if (oxcf->pass == 1) {
    cpi->td.mb.e_mbd.lossless[0] = is_lossless_requested(oxcf);
    av1_first_pass(cpi, source);
  } else if (oxcf->pass == 2) {
    if (Pass2Encode(cpi, size, dest, frame_flags) != AOM_CODEC_OK)
      return AOM_CODEC_ERROR;
  } else {
    // One pass encode
    if (Pass0Encode(cpi, size, dest, 0, frame_flags) != AOM_CODEC_OK)
      return AOM_CODEC_ERROR;
  }
  if (oxcf->pass != 1 && cpi->common.allow_screen_content_tools) {
    cpi->previous_hash_table = &cm->cur_frame->hash_table;
    {
      int l;
      for (l = -MAX_PRE_FRAMES; l < cpi->lookahead->max_sz; l++) {
        if ((cpi->lookahead->buf + l) == source) {
          cpi->previous_index = l;
          break;
        }
      }

      if (l == cpi->lookahead->max_sz) {
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to find last frame original buffer");
      }
    }
  }

  if (!cm->large_scale_tile) {
    cm->frame_contexts[cm->new_fb_idx] = *cm->fc;
  }

#define EXT_TILE_DEBUG 0
#if EXT_TILE_DEBUG
  if (cm->large_scale_tile && oxcf->pass == 2) {
    char fn[20] = "./fc";
    fn[4] = cm->current_video_frame / 100 + '0';
    fn[5] = (cm->current_video_frame % 100) / 10 + '0';
    fn[6] = (cm->current_video_frame % 10) + '0';
    fn[7] = '\0';
    av1_print_frame_contexts(cm->fc, fn);
  }
#endif  // EXT_TILE_DEBUG
#undef EXT_TILE_DEBUG

  cm->showable_frame = !cm->show_frame && cm->showable_frame;

  // No frame encoded, or frame was dropped, release scaled references.
  if ((*size == 0) && (frame_is_intra_only(cm) == 0)) {
    release_scaled_references(cpi);
  }

  if (*size > 0) {
    cpi->droppable = !frame_is_reference(cpi);
  }

  aom_usec_timer_mark(&cmptimer);
  cpi->time_compress_data += aom_usec_timer_elapsed(&cmptimer);

  if (cpi->b_calculate_psnr && oxcf->pass != 1 && cm->show_frame)
    generate_psnr_packet(cpi);

#if CONFIG_INTERNAL_STATS
  if (oxcf->pass != 1) {
    compute_internal_stats(cpi, (int)(*size));
  }
#endif  // CONFIG_INTERNAL_STATS

  aom_clear_system_state();

  return 0;
}

int av1_get_preview_raw_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *dest) {
  AV1_COMMON *cm = &cpi->common;
  if (!cm->show_frame) {
    return -1;
  } else {
    int ret;
    if (cm->frame_to_show) {
      *dest = *cm->frame_to_show;
      dest->y_width = cm->width;
      dest->y_height = cm->height;
      dest->uv_width = cm->width >> cm->subsampling_x;
      dest->uv_height = cm->height >> cm->subsampling_y;
      ret = 0;
    } else {
      ret = -1;
    }
    aom_clear_system_state();
    return ret;
  }
}

int av1_get_last_show_frame(AV1_COMP *cpi, YV12_BUFFER_CONFIG *frame) {
  if (cpi->last_show_frame_buf_idx == INVALID_IDX) return -1;

  *frame =
      cpi->common.buffer_pool->frame_bufs[cpi->last_show_frame_buf_idx].buf;
  return 0;
}

static int equal_dimensions_and_border(const YV12_BUFFER_CONFIG *a,
                                       const YV12_BUFFER_CONFIG *b) {
  return a->y_height == b->y_height && a->y_width == b->y_width &&
         a->uv_height == b->uv_height && a->uv_width == b->uv_width &&
         a->y_stride == b->y_stride && a->uv_stride == b->uv_stride &&
         a->border == b->border &&
         (a->flags & YV12_FLAG_HIGHBITDEPTH) ==
             (b->flags & YV12_FLAG_HIGHBITDEPTH);
}

aom_codec_err_t av1_copy_new_frame_enc(AV1_COMMON *cm,
                                       YV12_BUFFER_CONFIG *new_frame,
                                       YV12_BUFFER_CONFIG *sd) {
  const int num_planes = av1_num_planes(cm);
  if (!equal_dimensions_and_border(new_frame, sd))
    aom_internal_error(&cm->error, AOM_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  else
    aom_yv12_copy_frame(new_frame, sd, num_planes);

  return cm->error.error_code;
}

int av1_set_internal_size(AV1_COMP *cpi, AOM_SCALING horiz_mode,
                          AOM_SCALING vert_mode) {
  int hr = 0, hs = 0, vr = 0, vs = 0;

  if (horiz_mode > ONETWO || vert_mode > ONETWO) return -1;

  Scale2Ratio(horiz_mode, &hr, &hs);
  Scale2Ratio(vert_mode, &vr, &vs);

  // always go to the next whole number
  cpi->resize_pending_width = (hs - 1 + cpi->oxcf.width * hr) / hs;
  cpi->resize_pending_height = (vs - 1 + cpi->oxcf.height * vr) / vs;

  return 0;
}

int av1_get_quantizer(AV1_COMP *cpi) { return cpi->common.base_qindex; }

int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t *frame_size) {
  size_t output_size = 0;
  size_t total_bytes_read = 0;
  size_t remaining_size = *frame_size;
  uint8_t *buff_ptr = buffer;

  // go through each OBUs
  while (total_bytes_read < *frame_size) {
    uint8_t saved_obu_header[2];
    uint64_t obu_payload_size;
    size_t length_of_payload_size;
    size_t length_of_obu_size;
    uint32_t obu_header_size = (buff_ptr[0] >> 2) & 0x1 ? 2 : 1;
    size_t obu_bytes_read = obu_header_size;  // bytes read for current obu

    // save the obu header (1 or 2 bytes)
    memmove(saved_obu_header, buff_ptr, obu_header_size);
    // clear the obu_has_size_field
    saved_obu_header[0] = saved_obu_header[0] & (~0x2);

    // get the payload_size and length of payload_size
    if (aom_uleb_decode(buff_ptr + obu_header_size, remaining_size,
                        &obu_payload_size, &length_of_payload_size) != 0) {
      return AOM_CODEC_ERROR;
    }
    obu_bytes_read += length_of_payload_size;

    // calculate the length of size of the obu header plus payload
    length_of_obu_size =
        aom_uleb_size_in_bytes((uint64_t)(obu_header_size + obu_payload_size));

    // move the rest of data to new location
    memmove(buff_ptr + length_of_obu_size + obu_header_size,
            buff_ptr + obu_bytes_read, remaining_size - obu_bytes_read);
    obu_bytes_read += (size_t)obu_payload_size;

    // write the new obu size
    const uint64_t obu_size = obu_header_size + obu_payload_size;
    size_t coded_obu_size;
    if (aom_uleb_encode(obu_size, sizeof(obu_size), buff_ptr,
                        &coded_obu_size) != 0) {
      return AOM_CODEC_ERROR;
    }

    // write the saved (modified) obu_header following obu size
    memmove(buff_ptr + length_of_obu_size, saved_obu_header, obu_header_size);

    total_bytes_read += obu_bytes_read;
    remaining_size -= obu_bytes_read;
    buff_ptr += length_of_obu_size + obu_size;
    output_size += length_of_obu_size + (size_t)obu_size;
  }

  *frame_size = output_size;
  return AOM_CODEC_OK;
}

void av1_apply_encoding_flags(AV1_COMP *cpi, aom_enc_frame_flags_t flags) {
  // TODO(yunqingwang): For what references to use, external encoding flags
  // should be consistent with internal reference frame selection. Need to
  // ensure that there is not conflict between the two. In AV1 encoder, the
  // priority rank for 7 reference frames are: LAST, ALTREF, LAST2, LAST3,
  // GOLDEN, BWDREF, ALTREF2. If only one reference frame is used, it must be
  // LAST.
  cpi->ext_ref_frame_flags = AOM_REFFRAME_ALL;
  if (flags &
      (AOM_EFLAG_NO_REF_LAST | AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
       AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF | AOM_EFLAG_NO_REF_BWD |
       AOM_EFLAG_NO_REF_ARF2)) {
    if (flags & AOM_EFLAG_NO_REF_LAST) {
      cpi->ext_ref_frame_flags = 0;
    } else {
      int ref = AOM_REFFRAME_ALL;

      if (flags & AOM_EFLAG_NO_REF_LAST2) ref ^= AOM_LAST2_FLAG;
      if (flags & AOM_EFLAG_NO_REF_LAST3) ref ^= AOM_LAST3_FLAG;

      if (flags & AOM_EFLAG_NO_REF_GF) ref ^= AOM_GOLD_FLAG;

      if (flags & AOM_EFLAG_NO_REF_ARF) {
        ref ^= AOM_ALT_FLAG;
        ref ^= AOM_BWD_FLAG;
        ref ^= AOM_ALT2_FLAG;
      } else {
        if (flags & AOM_EFLAG_NO_REF_BWD) ref ^= AOM_BWD_FLAG;
        if (flags & AOM_EFLAG_NO_REF_ARF2) ref ^= AOM_ALT2_FLAG;
      }

      av1_use_as_reference(cpi, ref);
    }
  }

  if (flags &
      (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF)) {
    int upd = AOM_REFFRAME_ALL;

    // Refreshing LAST/LAST2/LAST3 is handled by 1 common flag.
    if (flags & AOM_EFLAG_NO_UPD_LAST) upd ^= AOM_LAST_FLAG;

    if (flags & AOM_EFLAG_NO_UPD_GF) upd ^= AOM_GOLD_FLAG;

    if (flags & AOM_EFLAG_NO_UPD_ARF) {
      upd ^= AOM_ALT_FLAG;
      upd ^= AOM_BWD_FLAG;
      upd ^= AOM_ALT2_FLAG;
    }

    av1_update_reference(cpi, upd);
  }

  cpi->ext_use_ref_frame_mvs = cpi->oxcf.allow_ref_frame_mvs &
                               ((flags & AOM_EFLAG_NO_REF_FRAME_MVS) == 0);
  cpi->ext_use_error_resilient = cpi->oxcf.error_resilient_mode |
                                 ((flags & AOM_EFLAG_ERROR_RESILIENT) != 0);
  cpi->ext_use_s_frame =
      cpi->oxcf.s_frame_mode | ((flags & AOM_EFLAG_SET_S_FRAME) != 0);
  cpi->ext_use_primary_ref_none = (flags & AOM_EFLAG_SET_PRIMARY_REF_NONE) != 0;

  if (flags & AOM_EFLAG_NO_UPD_ENTROPY) {
    av1_update_entropy(cpi, 0);
  }
}

int64_t timebase_units_to_ticks(const aom_rational_t *timebase, int64_t n) {
  return n * TICKS_PER_SEC * timebase->num / timebase->den;
}

int64_t ticks_to_timebase_units(const aom_rational_t *timebase, int64_t n) {
  const int64_t round = TICKS_PER_SEC * timebase->num / 2 - 1;
  return (n * timebase->den + round) / timebase->num / TICKS_PER_SEC;
}
