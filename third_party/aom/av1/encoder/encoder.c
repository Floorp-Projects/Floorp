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

#include "./aom_config.h"

#include "av1/common/alloccommon.h"
#if CONFIG_CDEF
#include "av1/common/cdef.h"
#endif  // CONFIG_CDEF
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
#if CONFIG_BGSPRITE
#include "av1/encoder/bgsprite.h"
#endif  // CONFIG_BGSPRITE
#if CONFIG_ANS
#include "aom_dsp/buf_ans.h"
#endif
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.h"
#endif
#include "av1/encoder/ethread.h"
#include "av1/encoder/firstpass.h"
#if CONFIG_HASH_ME
#include "av1/encoder/hash_motion.h"
#endif
#include "av1/encoder/mbgraph.h"
#if CONFIG_NCOBMC_ADAPT_WEIGHT
#include "av1/common/ncobmc_kernels.h"
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT
#include "av1/encoder/picklpf.h"
#if CONFIG_LOOP_RESTORATION
#include "av1/encoder/pickrst.h"
#endif  // CONFIG_LOOP_RESTORATION
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/temporal_filter.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "aom_dsp/psnr.h"
#if CONFIG_INTERNAL_STATS
#include "aom_dsp/ssim.h"
#endif
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"
#if CONFIG_BITSTREAM_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#if CONFIG_ENTROPY_STATS
FRAME_COUNTS aggregate_fc;
// Aggregate frame counts per frame context type
FRAME_COUNTS aggregate_fc_per_type[FRAME_CONTEXTS];
#endif  // CONFIG_ENTROPY_STATS

#define AM_SEGMENT_ID_INACTIVE 7
#define AM_SEGMENT_ID_ACTIVE 0

#define SHARP_FILTER_QTHRESH 0 /* Q threshold for 8-tap sharp filter */

#define ALTREF_HIGH_PRECISION_MV 1     // Whether to use high precision mv
                                       //  for altref computation.
#define HIGH_PRECISION_MV_QTHRESH 200  // Q threshold for high precision
                                       // mv. Choose a very high value for
                                       // now so that HIGH_PRECISION is always
                                       // chosen.

// #define OUTPUT_YUV_REC
#ifdef OUTPUT_YUV_DENOISED
FILE *yuv_denoised_file = NULL;
#endif
#ifdef OUTPUT_YUV_SKINMAP
FILE *yuv_skinmap_file = NULL;
#endif
#ifdef OUTPUT_YUV_REC
FILE *yuv_rec_file;
#define FILE_NAME_LEN 100
#endif

#if 0
FILE *framepsnr;
FILE *kf_list;
FILE *keyfile;
#endif

#if CONFIG_CFL
CFL_CTX NULL_CFL;
#endif

#if CONFIG_INTERNAL_STATS
typedef enum { Y, U, V, ALL } STAT_TYPE;
#endif  // CONFIG_INTERNAL_STATS

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
#if CONFIG_LOOPFILTER_LEVEL
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
#else
      av1_enable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF);
      // Setting the data to -MAX_LOOP_FILTER will result in the computed loop
      // filter level being zero regardless of the value of seg->abs_delta.
      av1_set_segdata(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF,
                      -MAX_LOOP_FILTER);
#endif  // CONFIG_LOOPFILTER_LEVEL
    } else {
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_SKIP);
#if CONFIG_LOOPFILTER_LEVEL
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_H);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_Y_V);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_U);
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF_V);
#else
      av1_disable_segfeature(seg, AM_SEGMENT_ID_INACTIVE, SEG_LVL_ALT_LF);
#endif  // CONFIG_LOOPFILTER_LEVEL
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

static void set_high_precision_mv(AV1_COMP *cpi, int allow_high_precision_mv
#if CONFIG_AMVR
                                  ,
                                  int cur_frame_mv_precision_level
#endif
                                  ) {
  MACROBLOCK *const mb = &cpi->td.mb;
  cpi->common.allow_high_precision_mv = allow_high_precision_mv;

#if CONFIG_AMVR
  if (cpi->common.allow_high_precision_mv &&
      cur_frame_mv_precision_level == 0) {
#else
  if (cpi->common.allow_high_precision_mv) {
#endif
    int i;
    for (i = 0; i < NMV_CONTEXTS; ++i) {
      mb->mv_cost_stack[i] = mb->nmvcost_hp[i];
    }
  } else {
    int i;
    for (i = 0; i < NMV_CONTEXTS; ++i) {
      mb->mv_cost_stack[i] = mb->nmvcost[i];
    }
  }
}

static BLOCK_SIZE select_sb_size(const AV1_COMP *const cpi) {
#if CONFIG_EXT_PARTITION
  if (cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_64X64)
    return BLOCK_64X64;

  if (cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_128X128)
    return BLOCK_128X128;

  assert(cpi->oxcf.superblock_size == AOM_SUPERBLOCK_SIZE_DYNAMIC);

  assert(IMPLIES(cpi->common.tile_cols > 1,
                 cpi->common.tile_width % MAX_MIB_SIZE == 0));
  assert(IMPLIES(cpi->common.tile_rows > 1,
                 cpi->common.tile_height % MAX_MIB_SIZE == 0));

  // TODO(any): Possibly could improve this with a heuristic.
  return BLOCK_128X128;
#else
  (void)cpi;
  return BLOCK_64X64;
#endif  //  CONFIG_EXT_PARTITION
}

static void setup_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  // Set up entropy context depending on frame type. The decoder mandates
  // the use of the default context, index 0, for keyframes and inter
  // frames where the error_resilient_mode or intra_only flag is set. For
  // other inter-frames the encoder currently uses only two contexts;
  // context 1 for ALTREF frames and context 0 for the others.
  if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
    av1_setup_past_independence(cm);
  } else {
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
// Just use frame context from first signaled reference frame.
// This will always be LAST_FRAME for now.
#else
#if CONFIG_EXT_REFS
    const GF_GROUP *gf_group = &cpi->twopass.gf_group;
    if (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE)
      cm->frame_context_idx = EXT_ARF_FRAME;
    else if (cpi->refresh_alt_ref_frame)
      cm->frame_context_idx = ARF_FRAME;
#else   // !CONFIG_EXT_REFS
    if (cpi->refresh_alt_ref_frame) cm->frame_context_idx = ARF_FRAME;
#endif  // CONFIG_EXT_REFS
    else if (cpi->rc.is_src_frame_alt_ref)
      cm->frame_context_idx = OVERLAY_FRAME;
    else if (cpi->refresh_golden_frame)
      cm->frame_context_idx = GLD_FRAME;
#if CONFIG_EXT_REFS
    else if (cpi->refresh_bwd_ref_frame)
      cm->frame_context_idx = BRF_FRAME;
#endif  // CONFIG_EXT_REFS
    else
      cm->frame_context_idx = REGULAR_FRAME;
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING
  }

  if (cm->frame_type == KEY_FRAME) {
    cpi->refresh_golden_frame = 1;
    cpi->refresh_alt_ref_frame = 1;
    av1_zero(cpi->interp_filter_selected);
    set_sb_size(cm, select_sb_size(cpi));
#if CONFIG_REFERENCE_BUFFER
    set_use_reference_buffer(cm, 0);
#endif  // CONFIG_REFERENCE_BUFFER
  } else {
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
    if (frame_is_intra_only(cm) || cm->error_resilient_mode ||
        cm->frame_refs[0].idx < 0) {
      *cm->fc = cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
    } else {
      *cm->fc = cm->frame_contexts[cm->frame_refs[0].idx];
    }
#else
    *cm->fc = cm->frame_contexts[cm->frame_context_idx];
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING
    av1_zero(cpi->interp_filter_selected[0]);
  }
#if CONFIG_EXT_REFS
#if CONFIG_ONE_SIDED_COMPOUND && \
    !CONFIG_EXT_COMP_REFS  // No change to bitstream
  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    cpi->refresh_bwd_ref_frame = cpi->refresh_last_frame;
    cpi->rc.is_bipred_frame = 1;
  }
#endif  // CONFIG_ONE_SIDED_COMPOUND && !CONFIG_EXT_COMP_REFS
#endif  // CONFIG_EXT_REFS
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
  if (frame_is_intra_only(cm) || cm->error_resilient_mode ||
      cm->frame_refs[0].idx < 0) {
    // use default frame context values
    cm->pre_fc = &cm->frame_contexts[FRAME_CONTEXT_DEFAULTS];
  } else {
    *cm->fc = cm->frame_contexts[cm->frame_refs[0].idx];
    cm->pre_fc = &cm->frame_contexts[cm->frame_refs[0].idx];
  }
#else
  cm->pre_fc = &cm->frame_contexts[cm->frame_context_idx];
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING

  cpi->vaq_refresh = 0;
}

static void enc_setup_mi(AV1_COMMON *cm) {
  int i;
  cm->mi = cm->mip + cm->mi_stride + 1;
  memset(cm->mip, 0, cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mip));
  cm->prev_mi = cm->prev_mip + cm->mi_stride + 1;
  // Clear top border row
  memset(cm->prev_mip, 0, sizeof(*cm->prev_mip) * cm->mi_stride);
  // Clear left border column
  for (i = 1; i < cm->mi_rows + 1; ++i)
    memset(&cm->prev_mip[i * cm->mi_stride], 0, sizeof(*cm->prev_mip));
  cm->mi_grid_visible = cm->mi_grid_base + cm->mi_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mi_stride + 1;

  memset(cm->mi_grid_base, 0,
         cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->mi_grid_base));
}

static int enc_alloc_mi(AV1_COMMON *cm, int mi_size) {
  cm->mip = aom_calloc(mi_size, sizeof(*cm->mip));
  if (!cm->mip) return 1;
  cm->prev_mip = aom_calloc(mi_size, sizeof(*cm->prev_mip));
  if (!cm->prev_mip) return 1;
  cm->mi_alloc_size = mi_size;

  cm->mi_grid_base = (MODE_INFO **)aom_calloc(mi_size, sizeof(MODE_INFO *));
  if (!cm->mi_grid_base) return 1;
  cm->prev_mi_grid_base =
      (MODE_INFO **)aom_calloc(mi_size, sizeof(MODE_INFO *));
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
  MODE_INFO **temp_base = cm->prev_mi_grid_base;
  MODE_INFO *temp = cm->prev_mip;
  cm->prev_mip = cm->mip;
  cm->mip = temp;

  // Update the upper left visible macroblock ptrs.
  cm->mi = cm->mip + cm->mi_stride + 1;
  cm->prev_mi = cm->prev_mip + cm->mi_stride + 1;

  cm->prev_mi_grid_base = cm->mi_grid_base;
  cm->mi_grid_base = temp_base;
  cm->mi_grid_visible = cm->mi_grid_base + cm->mi_stride + 1;
  cm->prev_mi_grid_visible = cm->prev_mi_grid_base + cm->mi_stride + 1;
}

void av1_initialize_enc(void) {
  static volatile int init_done = 0;

  if (!init_done) {
    av1_rtcd();
    aom_dsp_rtcd();
    aom_scale_rtcd();
    av1_init_intra_predictors();
    av1_init_me_luts();
#if !CONFIG_XIPHRC
    av1_rc_init_minq_luts();
#endif
    av1_entropy_mv_init();
    av1_encode_token_init();
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

static void dealloc_compressor_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  dealloc_context_buffers_ext(cpi);

#if CONFIG_PVQ
  if (cpi->oxcf.pass != 1) {
    const int tile_cols = cm->tile_cols;
    const int tile_rows = cm->tile_rows;
    int tile_col, tile_row;

    for (tile_row = 0; tile_row < tile_rows; ++tile_row)
      for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
        TileDataEnc *tile_data =
            &cpi->tile_data[tile_row * tile_cols + tile_col];
        aom_free(tile_data->pvq_q.buf);
      }
  }
#endif
  aom_free(cpi->tile_data);
  cpi->tile_data = NULL;

  // Delete sementation map
  aom_free(cpi->segmentation_map);
  cpi->segmentation_map = NULL;

  av1_cyclic_refresh_free(cpi->cyclic_refresh);
  cpi->cyclic_refresh = NULL;

  aom_free(cpi->active_map.map);
  cpi->active_map.map = NULL;

#if CONFIG_MOTION_VAR
  aom_free(cpi->td.mb.above_pred_buf);
  cpi->td.mb.above_pred_buf = NULL;

  aom_free(cpi->td.mb.left_pred_buf);
  cpi->td.mb.left_pred_buf = NULL;

  aom_free(cpi->td.mb.wsrc_buf);
  cpi->td.mb.wsrc_buf = NULL;

  aom_free(cpi->td.mb.mask_buf);
  cpi->td.mb.mask_buf = NULL;
#endif

  av1_free_ref_frame_buffers(cm->buffer_pool);
#if CONFIG_LV_MAP
  av1_free_txb_buf(cpi);
#endif
  av1_free_context_buffers(cm);

  aom_free_frame_buffer(&cpi->last_frame_uf);
#if CONFIG_LOOP_RESTORATION
  av1_free_restoration_buffers(cm);
  aom_free_frame_buffer(&cpi->last_frame_db);
  aom_free_frame_buffer(&cpi->trial_frame_rst);
  aom_free(cpi->extra_rstbuf);
  {
    int i;
    for (i = 0; i < MAX_MB_PLANE; ++i)
      av1_free_restoration_struct(&cpi->rst_search[i]);
  }
#endif  // CONFIG_LOOP_RESTORATION
  aom_free_frame_buffer(&cpi->scaled_source);
  aom_free_frame_buffer(&cpi->scaled_last_source);
  aom_free_frame_buffer(&cpi->alt_ref_buffer);
  av1_lookahead_destroy(cpi->lookahead);

  aom_free(cpi->tile_tok[0][0]);
  cpi->tile_tok[0][0] = 0;

  av1_free_pc_tree(&cpi->td);

  aom_free(cpi->td.mb.palette_buffer);

#if CONFIG_ANS
  aom_buf_ans_free(&cpi->buf_ans);
#endif  // CONFIG_ANS
}

static void save_coding_context(AV1_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  AV1_COMMON *cm = &cpi->common;
  int i;

  // Stores a snapshot of key state variables which can subsequently be
  // restored with a call to av1_restore_coding_context. These functions are
  // intended for use in a re-code loop in av1_compress_frame where the
  // quantizer value is adjusted between loop iterations.
  for (i = 0; i < NMV_CONTEXTS; ++i) {
    av1_copy(cc->nmv_vec_cost[i], cpi->td.mb.nmv_vec_cost[i]);
    av1_copy(cc->nmv_costs, cpi->nmv_costs);
    av1_copy(cc->nmv_costs_hp, cpi->nmv_costs_hp);
  }

  av1_copy(cc->last_ref_lf_deltas, cm->lf.last_ref_deltas);
  av1_copy(cc->last_mode_lf_deltas, cm->lf.last_mode_deltas);

  cc->fc = *cm->fc;
}

static void restore_coding_context(AV1_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  AV1_COMMON *cm = &cpi->common;
  int i;

  // Restore key state variables to the snapshot state stored in the
  // previous call to av1_save_coding_context.
  for (i = 0; i < NMV_CONTEXTS; ++i) {
    av1_copy(cpi->td.mb.nmv_vec_cost[i], cc->nmv_vec_cost[i]);
    av1_copy(cpi->nmv_costs, cc->nmv_costs);
    av1_copy(cpi->nmv_costs_hp, cc->nmv_costs_hp);
  }

  av1_copy(cm->lf.last_ref_deltas, cc->last_ref_lf_deltas);
  av1_copy(cm->lf.last_mode_deltas, cc->last_mode_lf_deltas);

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
#if CONFIG_LOOPFILTER_LEVEL
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_H, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_V, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_U, -2);
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_V, -2);

      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_H);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_V);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_U);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_V);
#else
      av1_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);
      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);
#endif  // CONFIG_LOOPFILTER_LEVEL

      av1_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

      // Where relevant assume segment data is delta data
      seg->abs_delta = SEGMENT_DELTADATA;
    }
  } else if (seg->enabled) {
    // All other frames if segmentation has been enabled

    // First normal frame in a valid gf or alt ref group
    if (rc->frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (rc->source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;
        seg->abs_delta = SEGMENT_DELTADATA;

        qi_delta =
            av1_compute_qdelta(rc, rc->avg_q, rc->avg_q * 1.125, cm->bit_depth);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta + 2);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

#if CONFIG_LOOPFILTER_LEVEL
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_H, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_Y_V, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_U, -2);
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF_V, -2);

        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_H);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_Y_V);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_U);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF_V);
#else
        av1_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);
        av1_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);
#endif  // CONFIG_LOOPFILTER_LEVEL

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
  MODE_INFO **mi_8x8_ptr = cm->mi_grid_visible;
  uint8_t *cache_ptr = cm->last_frame_seg_map;
  int row, col;

  for (row = 0; row < cm->mi_rows; row++) {
    MODE_INFO **mi_8x8 = mi_8x8_ptr;
    uint8_t *cache = cache_ptr;
    for (col = 0; col < cm->mi_cols; col++, mi_8x8++, cache++)
      cache[0] = mi_8x8[0]->mbmi.segment_id;
    mi_8x8_ptr += cm->mi_stride;
    cache_ptr += cm->mi_cols;
  }
}

static void alloc_raw_frame_buffers(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;

  if (!cpi->lookahead)
    cpi->lookahead = av1_lookahead_init(oxcf->width, oxcf->height,
                                        cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                                        cm->use_highbitdepth,
#endif
                                        oxcf->lag_in_frames);
  if (!cpi->lookahead)
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  // TODO(agrange) Check if ARF is enabled and skip allocation if not.
  if (aom_realloc_frame_buffer(&cpi->alt_ref_buffer, oxcf->width, oxcf->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

static void alloc_util_frame_buffers(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (aom_realloc_frame_buffer(&cpi->last_frame_uf, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

#if CONFIG_LOOP_RESTORATION
  if (aom_realloc_frame_buffer(&cpi->last_frame_db, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate last frame deblocked buffer");
  if (aom_realloc_frame_buffer(
          &cpi->trial_frame_rst,
#if CONFIG_FRAME_SUPERRES
          cm->superres_upscaled_width, cm->superres_upscaled_height,
#else
          cm->width, cm->height,
#endif  // CONFIG_FRAME_SUPERRES
          cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
          cm->use_highbitdepth,
#endif
          AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate trial restored frame buffer");
  int extra_rstbuf_sz = RESTORATION_EXTBUF_SIZE;
  if (extra_rstbuf_sz > 0) {
    aom_free(cpi->extra_rstbuf);
    CHECK_MEM_ERROR(cm, cpi->extra_rstbuf,
                    (uint8_t *)aom_malloc(extra_rstbuf_sz));
  } else {
    cpi->extra_rstbuf = NULL;
  }
#endif  // CONFIG_LOOP_RESTORATION

  if (aom_realloc_frame_buffer(&cpi->scaled_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate scaled source buffer");

  if (aom_realloc_frame_buffer(&cpi->scaled_last_source, cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate scaled last source buffer");
}

static void alloc_compressor_data(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  av1_alloc_context_buffers(cm, cm->width, cm->height);

#if CONFIG_LV_MAP
  av1_alloc_txb_buf(cpi);
#endif

  alloc_context_buffers_ext(cpi);

  aom_free(cpi->tile_tok[0][0]);

  {
    unsigned int tokens = get_token_alloc(cm->mb_rows, cm->mb_cols);
    CHECK_MEM_ERROR(cm, cpi->tile_tok[0][0],
                    aom_calloc(tokens, sizeof(*cpi->tile_tok[0][0])));
  }

  av1_setup_pc_tree(&cpi->common, &cpi->td);
}

void av1_new_framerate(AV1_COMP *cpi, double framerate) {
  cpi->framerate = framerate < 0.1 ? 30 : framerate;
#if CONFIG_XIPHRC
  if (!cpi->od_rc.cur_frame) return;
  cpi->od_rc.framerate = cpi->framerate;
  od_enc_rc_resize(&cpi->od_rc);
#else
  av1_rc_update_framerate(cpi, cpi->common.width, cpi->common.height);
#endif
}

#if CONFIG_MAX_TILE

static void set_tile_info_max_tile(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int i, start_sb;

  av1_get_tile_limits(cm);

  // configure tile columns
  if (cpi->oxcf.tile_width_count == 0 || cpi->oxcf.tile_height_count == 0) {
    cm->uniform_tile_spacing_flag = 1;
    cm->log2_tile_cols = AOMMAX(cpi->oxcf.tile_columns, cm->min_log2_tile_cols);
    cm->log2_tile_cols = AOMMIN(cm->log2_tile_cols, cm->max_log2_tile_cols);
  } else {
    int mi_cols = ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
    int sb_cols = mi_cols >> MAX_MIB_SIZE_LOG2;
    int size_sb, j = 0;
    cm->uniform_tile_spacing_flag = 0;
    for (i = 0, start_sb = 0; start_sb < sb_cols && i < MAX_TILE_COLS; i++) {
      cm->tile_col_start_sb[i] = start_sb;
      size_sb = cpi->oxcf.tile_widths[j++];
      if (j >= cpi->oxcf.tile_width_count) j = 0;
      start_sb += AOMMIN(size_sb, MAX_TILE_WIDTH_SB);
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
    int mi_rows = ALIGN_POWER_OF_TWO(cm->mi_rows, MAX_MIB_SIZE_LOG2);
    int sb_rows = mi_rows >> MAX_MIB_SIZE_LOG2;
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

#endif

static void set_tile_info(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
#if CONFIG_DEPENDENT_HORZTILES
  int tile_row, tile_col, num_tiles_in_tg;
  int tg_row_start, tg_col_start;
#endif
#if CONFIG_EXT_TILE
  if (cpi->oxcf.large_scale_tile) {
#if CONFIG_EXT_PARTITION
    if (cpi->oxcf.superblock_size != AOM_SUPERBLOCK_SIZE_64X64) {
      cm->tile_width = clamp(cpi->oxcf.tile_columns, 1, 32);
      cm->tile_height = clamp(cpi->oxcf.tile_rows, 1, 32);
      cm->tile_width <<= MAX_MIB_SIZE_LOG2;
      cm->tile_height <<= MAX_MIB_SIZE_LOG2;
    } else {
      cm->tile_width = clamp(cpi->oxcf.tile_columns, 1, 64);
      cm->tile_height = clamp(cpi->oxcf.tile_rows, 1, 64);
      cm->tile_width <<= MAX_MIB_SIZE_LOG2 - 1;
      cm->tile_height <<= MAX_MIB_SIZE_LOG2 - 1;
    }
#else
    cm->tile_width = clamp(cpi->oxcf.tile_columns, 1, 64);
    cm->tile_height = clamp(cpi->oxcf.tile_rows, 1, 64);
    cm->tile_width <<= MAX_MIB_SIZE_LOG2;
    cm->tile_height <<= MAX_MIB_SIZE_LOG2;
#endif  // CONFIG_EXT_PARTITION

    cm->tile_width = AOMMIN(cm->tile_width, cm->mi_cols);
    cm->tile_height = AOMMIN(cm->tile_height, cm->mi_rows);

    assert(cm->tile_width >> MAX_MIB_SIZE <= 32);
    assert(cm->tile_height >> MAX_MIB_SIZE <= 32);

    // Get the number of tiles
    cm->tile_cols = 1;
    while (cm->tile_cols * cm->tile_width < cm->mi_cols) ++cm->tile_cols;

    cm->tile_rows = 1;
    while (cm->tile_rows * cm->tile_height < cm->mi_rows) ++cm->tile_rows;
  } else {
#endif  // CONFIG_EXT_TILE

#if CONFIG_MAX_TILE
    set_tile_info_max_tile(cpi);
#else
  int min_log2_tile_cols, max_log2_tile_cols;
  av1_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  cm->log2_tile_cols =
      clamp(cpi->oxcf.tile_columns, min_log2_tile_cols, max_log2_tile_cols);
  cm->log2_tile_rows = cpi->oxcf.tile_rows;

  cm->tile_width =
      get_tile_size(cm->mi_cols, cm->log2_tile_cols, &cm->tile_cols);
  cm->tile_height =
      get_tile_size(cm->mi_rows, cm->log2_tile_rows, &cm->tile_rows);
#endif  // CONFIG_MAX_TILE
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

#if CONFIG_DEPENDENT_HORZTILES
  cm->dependent_horz_tiles = cpi->oxcf.dependent_horz_tiles;
#if CONFIG_EXT_TILE
  if (cm->large_scale_tile) {
    // May not needed since cpi->oxcf.dependent_horz_tiles is already adjusted.
    cm->dependent_horz_tiles = 0;
  } else {
#endif  // CONFIG_EXT_TILE
    if (cm->log2_tile_rows == 0) cm->dependent_horz_tiles = 0;
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE

#if CONFIG_EXT_TILE
  if (!cm->large_scale_tile) {
#endif  // CONFIG_EXT_TILE
    if (cpi->oxcf.mtu == 0) {
      cm->num_tg = cpi->oxcf.num_tile_groups;
    } else {
      // Use a default value for the purposes of weighting costs in probability
      // updates
      cm->num_tg = DEFAULT_MAX_NUM_TG;
    }
    num_tiles_in_tg =
        (cm->tile_cols * cm->tile_rows + cm->num_tg - 1) / cm->num_tg;
    tg_row_start = 0;
    tg_col_start = 0;
    for (tile_row = 0; tile_row < cm->tile_rows; ++tile_row) {
      for (tile_col = 0; tile_col < cm->tile_cols; ++tile_col) {
        if ((tile_row * cm->tile_cols + tile_col) % num_tiles_in_tg == 0) {
          tg_row_start = tile_row;
          tg_col_start = tile_col;
        }
        cm->tile_group_start_row[tile_row][tile_col] = tg_row_start;
        cm->tile_group_start_col[tile_row][tile_col] = tg_col_start;
      }
    }
#if CONFIG_EXT_TILE
  }
#endif  // CONFIG_EXT_TILE
#endif

#if CONFIG_LOOPFILTERING_ACROSS_TILES
  cm->loop_filter_across_tiles_enabled =
      cpi->oxcf.loop_filter_across_tiles_enabled;
#endif  // CONFIG_LOOPFILTERING_ACROSS_TILES
}

static void update_frame_size(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;

  av1_set_mb_mi(cm, cm->width, cm->height);
  av1_init_context_buffers(cm);
  av1_init_macroblockd(cm, xd,
#if CONFIG_PVQ
                       NULL,
#endif
#if CONFIG_CFL
                       &NULL_CFL,
#endif
                       NULL);
  memset(cpi->mbmi_ext_base, 0,
         cm->mi_rows * cm->mi_cols * sizeof(*cpi->mbmi_ext_base));
  set_tile_info(cpi);
}

static void init_buffer_indices(AV1_COMP *cpi) {
#if CONFIG_EXT_REFS
  int fb_idx;
  for (fb_idx = 0; fb_idx < LAST_REF_FRAMES; ++fb_idx)
    cpi->lst_fb_idxes[fb_idx] = fb_idx;
  cpi->gld_fb_idx = LAST_REF_FRAMES;
  cpi->bwd_fb_idx = LAST_REF_FRAMES + 1;
  cpi->alt2_fb_idx = LAST_REF_FRAMES + 2;
  cpi->alt_fb_idx = LAST_REF_FRAMES + 3;
  cpi->ext_fb_idx = LAST_REF_FRAMES + 4;
  for (fb_idx = 0; fb_idx < MAX_EXT_ARFS + 1; ++fb_idx)
    cpi->arf_map[fb_idx] = LAST_REF_FRAMES + 2 + fb_idx;
#else   // !CONFIG_EXT_REFS
  cpi->lst_fb_idx = 0;
  cpi->gld_fb_idx = 1;
  cpi->alt_fb_idx = 2;
#endif  // CONFIG_EXT_REFS
#if CONFIG_AMVR
  cpi->rate_index = 0;
  cpi->rate_size = 0;
  cpi->cur_poc = -1;
#endif
}

static void init_config(struct AV1_COMP *cpi, AV1EncoderConfig *oxcf) {
  AV1_COMMON *const cm = &cpi->common;

  cpi->oxcf = *oxcf;
  cpi->framerate = oxcf->init_framerate;

  cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
#if CONFIG_HIGHBITDEPTH
  cm->use_highbitdepth = oxcf->use_highbitdepth;
#endif
  cm->color_space = oxcf->color_space;
#if CONFIG_COLORSPACE_HEADERS
  cm->transfer_function = oxcf->transfer_function;
  cm->chroma_sample_position = oxcf->chroma_sample_position;
#endif
  cm->color_range = oxcf->color_range;

  cm->width = oxcf->width;
  cm->height = oxcf->height;
  alloc_compressor_data(cpi);

  // Single thread case: use counts in common.
  cpi->td.counts = &cm->counts;

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

#if CONFIG_HIGHBITDEPTH
#define HIGHBD_BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX3F, SDX8F, SDX4DF) \
  cpi->fn_ptr[BT].sdf = SDF;                                           \
  cpi->fn_ptr[BT].sdaf = SDAF;                                         \
  cpi->fn_ptr[BT].vf = VF;                                             \
  cpi->fn_ptr[BT].svf = SVF;                                           \
  cpi->fn_ptr[BT].svaf = SVAF;                                         \
  cpi->fn_ptr[BT].sdx3f = SDX3F;                                       \
  cpi->fn_ptr[BT].sdx8f = SDX8F;                                       \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;

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

#define MAKE_BFP_SAD3_WRAPPER(fnname)                                    \
  static void fnname##_bits8(const uint8_t *src_ptr, int source_stride,  \
                             const uint8_t *ref_ptr, int ref_stride,     \
                             unsigned int *sad_array) {                  \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
  }                                                                      \
  static void fnname##_bits10(const uint8_t *src_ptr, int source_stride, \
                              const uint8_t *ref_ptr, int ref_stride,    \
                              unsigned int *sad_array) {                 \
    int i;                                                               \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
    for (i = 0; i < 3; i++) sad_array[i] >>= 2;                          \
  }                                                                      \
  static void fnname##_bits12(const uint8_t *src_ptr, int source_stride, \
                              const uint8_t *ref_ptr, int ref_stride,    \
                              unsigned int *sad_array) {                 \
    int i;                                                               \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
    for (i = 0; i < 3; i++) sad_array[i] >>= 4;                          \
  }

#define MAKE_BFP_SAD8_WRAPPER(fnname)                                    \
  static void fnname##_bits8(const uint8_t *src_ptr, int source_stride,  \
                             const uint8_t *ref_ptr, int ref_stride,     \
                             unsigned int *sad_array) {                  \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
  }                                                                      \
  static void fnname##_bits10(const uint8_t *src_ptr, int source_stride, \
                              const uint8_t *ref_ptr, int ref_stride,    \
                              unsigned int *sad_array) {                 \
    int i;                                                               \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
    for (i = 0; i < 8; i++) sad_array[i] >>= 2;                          \
  }                                                                      \
  static void fnname##_bits12(const uint8_t *src_ptr, int source_stride, \
                              const uint8_t *ref_ptr, int ref_stride,    \
                              unsigned int *sad_array) {                 \
    int i;                                                               \
    fnname(src_ptr, source_stride, ref_ptr, ref_stride, sad_array);      \
    for (i = 0; i < 8; i++) sad_array[i] >>= 4;                          \
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

#if CONFIG_EXT_PARTITION
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad128x128)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad128x128_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad128x128x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad128x128x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad128x128x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad128x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad128x64_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad128x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x128)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x128_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x128x4d)
#endif  // CONFIG_EXT_PARTITION
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
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad32x32x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad32x32x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x32x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad64x64)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad64x64_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad64x64x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad64x64x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad64x64x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x16_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad16x16x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad16x16x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad16x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad16x8_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad16x8x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad16x8x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad16x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x16)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x16_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad8x16x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad8x16x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x16x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x8_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad8x8x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad8x8x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad8x4)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad8x4_avg)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad8x4x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad8x4x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad4x8)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad4x8_avg)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad4x8x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad4x8x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad4x4)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad4x4_avg)
MAKE_BFP_SAD3_WRAPPER(aom_highbd_sad4x4x3)
MAKE_BFP_SAD8_WRAPPER(aom_highbd_sad4x4x8)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad4x4x4d)

#if CONFIG_EXT_PARTITION_TYPES
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
#if CONFIG_EXT_PARTITION
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad32x128)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad32x128_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad32x128x4d)
MAKE_BFP_SAD_WRAPPER(aom_highbd_sad128x32)
MAKE_BFP_SADAVG_WRAPPER(aom_highbd_sad128x32_avg)
MAKE_BFP_SAD4D_WRAPPER(aom_highbd_sad128x32x4d)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES

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

#if CONFIG_EXT_PARTITION
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad128x128)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad128x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x128)
#endif  // CONFIG_EXT_PARTITION
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

#if CONFIG_EXT_PARTITION_TYPES
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad4x16)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x4)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad8x32)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x8)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad16x64)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad64x16)
#if CONFIG_EXT_PARTITION
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad32x128)
MAKE_MBFP_COMPOUND_SAD_WRAPPER(aom_highbd_masked_sad128x32)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_MOTION_VAR
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

#if CONFIG_EXT_PARTITION
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad128x128)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad128x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x128)
#endif  // CONFIG_EXT_PARTITION
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

#if CONFIG_EXT_PARTITION_TYPES
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad4x16)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x4)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad8x32)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x8)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad16x64)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad64x16)
#if CONFIG_EXT_PARTITION
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad32x128)
MAKE_OBFP_SAD_WRAPPER(aom_highbd_obmc_sad128x32)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES
#endif  // CONFIG_MOTION_VAR

static void highbd_set_var_fns(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  if (cm->use_highbitdepth) {
    switch (cm->bit_depth) {
      case AOM_BITS_8:
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(BLOCK_128X32, aom_highbd_sad128x32_bits8,
                   aom_highbd_sad128x32_avg_bits8, aom_highbd_8_variance128x32,
                   aom_highbd_8_sub_pixel_variance128x32,
                   aom_highbd_8_sub_pixel_avg_variance128x32, NULL, NULL,
                   aom_highbd_sad128x32x4d_bits8)

        HIGHBD_BFP(BLOCK_32X128, aom_highbd_sad32x128_bits8,
                   aom_highbd_sad32x128_avg_bits8, aom_highbd_8_variance32x128,
                   aom_highbd_8_sub_pixel_variance32x128,
                   aom_highbd_8_sub_pixel_avg_variance32x128, NULL, NULL,
                   aom_highbd_sad32x128x4d_bits8)
#endif  // CONFIG_EXT_PARTITION

        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits8,
                   aom_highbd_sad64x16_avg_bits8, aom_highbd_8_variance64x16,
                   aom_highbd_8_sub_pixel_variance64x16,
                   aom_highbd_8_sub_pixel_avg_variance64x16, NULL, NULL,
                   aom_highbd_sad64x16x4d_bits8)

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits8,
                   aom_highbd_sad16x64_avg_bits8, aom_highbd_8_variance16x64,
                   aom_highbd_8_sub_pixel_variance16x64,
                   aom_highbd_8_sub_pixel_avg_variance16x64, NULL, NULL,
                   aom_highbd_sad16x64x4d_bits8)

        HIGHBD_BFP(BLOCK_32X8, aom_highbd_sad32x8_bits8,
                   aom_highbd_sad32x8_avg_bits8, aom_highbd_8_variance32x8,
                   aom_highbd_8_sub_pixel_variance32x8,
                   aom_highbd_8_sub_pixel_avg_variance32x8, NULL, NULL,
                   aom_highbd_sad32x8x4d_bits8)

        HIGHBD_BFP(BLOCK_8X32, aom_highbd_sad8x32_bits8,
                   aom_highbd_sad8x32_avg_bits8, aom_highbd_8_variance8x32,
                   aom_highbd_8_sub_pixel_variance8x32,
                   aom_highbd_8_sub_pixel_avg_variance8x32, NULL, NULL,
                   aom_highbd_sad8x32x4d_bits8)

        HIGHBD_BFP(BLOCK_16X4, aom_highbd_sad16x4_bits8,
                   aom_highbd_sad16x4_avg_bits8, aom_highbd_8_variance16x4,
                   aom_highbd_8_sub_pixel_variance16x4,
                   aom_highbd_8_sub_pixel_avg_variance16x4, NULL, NULL,
                   aom_highbd_sad16x4x4d_bits8)

        HIGHBD_BFP(BLOCK_4X16, aom_highbd_sad4x16_bits8,
                   aom_highbd_sad4x16_avg_bits8, aom_highbd_8_variance4x16,
                   aom_highbd_8_sub_pixel_variance4x16,
                   aom_highbd_8_sub_pixel_avg_variance4x16, NULL, NULL,
                   aom_highbd_sad4x16x4d_bits8)
#endif

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits8,
                   aom_highbd_sad32x16_avg_bits8, aom_highbd_8_variance32x16,
                   aom_highbd_8_sub_pixel_variance32x16,
                   aom_highbd_8_sub_pixel_avg_variance32x16, NULL, NULL,
                   aom_highbd_sad32x16x4d_bits8)

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits8,
                   aom_highbd_sad16x32_avg_bits8, aom_highbd_8_variance16x32,
                   aom_highbd_8_sub_pixel_variance16x32,
                   aom_highbd_8_sub_pixel_avg_variance16x32, NULL, NULL,
                   aom_highbd_sad16x32x4d_bits8)

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits8,
                   aom_highbd_sad64x32_avg_bits8, aom_highbd_8_variance64x32,
                   aom_highbd_8_sub_pixel_variance64x32,
                   aom_highbd_8_sub_pixel_avg_variance64x32, NULL, NULL,
                   aom_highbd_sad64x32x4d_bits8)

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits8,
                   aom_highbd_sad32x64_avg_bits8, aom_highbd_8_variance32x64,
                   aom_highbd_8_sub_pixel_variance32x64,
                   aom_highbd_8_sub_pixel_avg_variance32x64, NULL, NULL,
                   aom_highbd_sad32x64x4d_bits8)

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits8,
                   aom_highbd_sad32x32_avg_bits8, aom_highbd_8_variance32x32,
                   aom_highbd_8_sub_pixel_variance32x32,
                   aom_highbd_8_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x3_bits8, aom_highbd_sad32x32x8_bits8,
                   aom_highbd_sad32x32x4d_bits8)

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits8,
                   aom_highbd_sad64x64_avg_bits8, aom_highbd_8_variance64x64,
                   aom_highbd_8_sub_pixel_variance64x64,
                   aom_highbd_8_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x3_bits8, aom_highbd_sad64x64x8_bits8,
                   aom_highbd_sad64x64x4d_bits8)

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits8,
                   aom_highbd_sad16x16_avg_bits8, aom_highbd_8_variance16x16,
                   aom_highbd_8_sub_pixel_variance16x16,
                   aom_highbd_8_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x3_bits8, aom_highbd_sad16x16x8_bits8,
                   aom_highbd_sad16x16x4d_bits8)

        HIGHBD_BFP(
            BLOCK_16X8, aom_highbd_sad16x8_bits8, aom_highbd_sad16x8_avg_bits8,
            aom_highbd_8_variance16x8, aom_highbd_8_sub_pixel_variance16x8,
            aom_highbd_8_sub_pixel_avg_variance16x8, aom_highbd_sad16x8x3_bits8,
            aom_highbd_sad16x8x8_bits8, aom_highbd_sad16x8x4d_bits8)

        HIGHBD_BFP(
            BLOCK_8X16, aom_highbd_sad8x16_bits8, aom_highbd_sad8x16_avg_bits8,
            aom_highbd_8_variance8x16, aom_highbd_8_sub_pixel_variance8x16,
            aom_highbd_8_sub_pixel_avg_variance8x16, aom_highbd_sad8x16x3_bits8,
            aom_highbd_sad8x16x8_bits8, aom_highbd_sad8x16x4d_bits8)

        HIGHBD_BFP(
            BLOCK_8X8, aom_highbd_sad8x8_bits8, aom_highbd_sad8x8_avg_bits8,
            aom_highbd_8_variance8x8, aom_highbd_8_sub_pixel_variance8x8,
            aom_highbd_8_sub_pixel_avg_variance8x8, aom_highbd_sad8x8x3_bits8,
            aom_highbd_sad8x8x8_bits8, aom_highbd_sad8x8x4d_bits8)

        HIGHBD_BFP(BLOCK_8X4, aom_highbd_sad8x4_bits8,
                   aom_highbd_sad8x4_avg_bits8, aom_highbd_8_variance8x4,
                   aom_highbd_8_sub_pixel_variance8x4,
                   aom_highbd_8_sub_pixel_avg_variance8x4, NULL,
                   aom_highbd_sad8x4x8_bits8, aom_highbd_sad8x4x4d_bits8)

        HIGHBD_BFP(BLOCK_4X8, aom_highbd_sad4x8_bits8,
                   aom_highbd_sad4x8_avg_bits8, aom_highbd_8_variance4x8,
                   aom_highbd_8_sub_pixel_variance4x8,
                   aom_highbd_8_sub_pixel_avg_variance4x8, NULL,
                   aom_highbd_sad4x8x8_bits8, aom_highbd_sad4x8x4d_bits8)

        HIGHBD_BFP(
            BLOCK_4X4, aom_highbd_sad4x4_bits8, aom_highbd_sad4x4_avg_bits8,
            aom_highbd_8_variance4x4, aom_highbd_8_sub_pixel_variance4x4,
            aom_highbd_8_sub_pixel_avg_variance4x4, aom_highbd_sad4x4x3_bits8,
            aom_highbd_sad4x4x8_bits8, aom_highbd_sad4x4x4d_bits8)

#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
        HIGHBD_BFP(BLOCK_2X2, NULL, NULL, aom_highbd_8_variance2x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_4X2, NULL, NULL, aom_highbd_8_variance4x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_2X4, NULL, NULL, aom_highbd_8_variance2x4, NULL, NULL,
                   NULL, NULL, NULL)
#endif

#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(BLOCK_128X128, aom_highbd_sad128x128_bits8,
                   aom_highbd_sad128x128_avg_bits8,
                   aom_highbd_8_variance128x128,
                   aom_highbd_8_sub_pixel_variance128x128,
                   aom_highbd_8_sub_pixel_avg_variance128x128,
                   aom_highbd_sad128x128x3_bits8, aom_highbd_sad128x128x8_bits8,
                   aom_highbd_sad128x128x4d_bits8)

        HIGHBD_BFP(BLOCK_128X64, aom_highbd_sad128x64_bits8,
                   aom_highbd_sad128x64_avg_bits8, aom_highbd_8_variance128x64,
                   aom_highbd_8_sub_pixel_variance128x64,
                   aom_highbd_8_sub_pixel_avg_variance128x64, NULL, NULL,
                   aom_highbd_sad128x64x4d_bits8)

        HIGHBD_BFP(BLOCK_64X128, aom_highbd_sad64x128_bits8,
                   aom_highbd_sad64x128_avg_bits8, aom_highbd_8_variance64x128,
                   aom_highbd_8_sub_pixel_variance64x128,
                   aom_highbd_8_sub_pixel_avg_variance64x128, NULL, NULL,
                   aom_highbd_sad64x128x4d_bits8)
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits8,
                    aom_highbd_8_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits8,
                    aom_highbd_8_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits8,
                    aom_highbd_8_masked_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X32, aom_highbd_masked_sad128x32_bits8,
                    aom_highbd_8_masked_sub_pixel_variance128x32)

        HIGHBD_MBFP(BLOCK_32X128, aom_highbd_masked_sad32x128_bits8,
                    aom_highbd_8_masked_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif
#if CONFIG_MOTION_VAR
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits8,
                    aom_highbd_obmc_variance128x128,
                    aom_highbd_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits8,
                    aom_highbd_obmc_variance128x64,
                    aom_highbd_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits8,
                    aom_highbd_obmc_variance64x128,
                    aom_highbd_obmc_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X32, aom_highbd_obmc_sad128x32_bits8,
                    aom_highbd_obmc_variance128x32,
                    aom_highbd_obmc_sub_pixel_variance128x32)

        HIGHBD_OBFP(BLOCK_32X128, aom_highbd_obmc_sad32x128_bits8,
                    aom_highbd_obmc_variance32x128,
                    aom_highbd_obmc_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif
#endif  // CONFIG_MOTION_VAR
        break;

      case AOM_BITS_10:
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(BLOCK_128X32, aom_highbd_sad128x32_bits10,
                   aom_highbd_sad128x32_avg_bits10,
                   aom_highbd_10_variance128x32,
                   aom_highbd_10_sub_pixel_variance128x32,
                   aom_highbd_10_sub_pixel_avg_variance128x32, NULL, NULL,
                   aom_highbd_sad128x32x4d_bits10)

        HIGHBD_BFP(BLOCK_32X128, aom_highbd_sad32x128_bits10,
                   aom_highbd_sad32x128_avg_bits10,
                   aom_highbd_10_variance32x128,
                   aom_highbd_10_sub_pixel_variance32x128,
                   aom_highbd_10_sub_pixel_avg_variance32x128, NULL, NULL,
                   aom_highbd_sad32x128x4d_bits10)
#endif  // CONFIG_EXT_PARTITION

        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits10,
                   aom_highbd_sad64x16_avg_bits10, aom_highbd_10_variance64x16,
                   aom_highbd_10_sub_pixel_variance64x16,
                   aom_highbd_10_sub_pixel_avg_variance64x16, NULL, NULL,
                   aom_highbd_sad64x16x4d_bits10)

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits10,
                   aom_highbd_sad16x64_avg_bits10, aom_highbd_10_variance16x64,
                   aom_highbd_10_sub_pixel_variance16x64,
                   aom_highbd_10_sub_pixel_avg_variance16x64, NULL, NULL,
                   aom_highbd_sad16x64x4d_bits10)

        HIGHBD_BFP(BLOCK_32X8, aom_highbd_sad32x8_bits10,
                   aom_highbd_sad32x8_avg_bits10, aom_highbd_10_variance32x8,
                   aom_highbd_10_sub_pixel_variance32x8,
                   aom_highbd_10_sub_pixel_avg_variance32x8, NULL, NULL,
                   aom_highbd_sad32x8x4d_bits10)

        HIGHBD_BFP(BLOCK_8X32, aom_highbd_sad8x32_bits10,
                   aom_highbd_sad8x32_avg_bits10, aom_highbd_10_variance8x32,
                   aom_highbd_10_sub_pixel_variance8x32,
                   aom_highbd_10_sub_pixel_avg_variance8x32, NULL, NULL,
                   aom_highbd_sad8x32x4d_bits10)

        HIGHBD_BFP(BLOCK_16X4, aom_highbd_sad16x4_bits10,
                   aom_highbd_sad16x4_avg_bits10, aom_highbd_10_variance16x4,
                   aom_highbd_10_sub_pixel_variance16x4,
                   aom_highbd_10_sub_pixel_avg_variance16x4, NULL, NULL,
                   aom_highbd_sad16x4x4d_bits10)

        HIGHBD_BFP(BLOCK_4X16, aom_highbd_sad4x16_bits10,
                   aom_highbd_sad4x16_avg_bits10, aom_highbd_10_variance4x16,
                   aom_highbd_10_sub_pixel_variance4x16,
                   aom_highbd_10_sub_pixel_avg_variance4x16, NULL, NULL,
                   aom_highbd_sad4x16x4d_bits10)
#endif

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits10,
                   aom_highbd_sad32x16_avg_bits10, aom_highbd_10_variance32x16,
                   aom_highbd_10_sub_pixel_variance32x16,
                   aom_highbd_10_sub_pixel_avg_variance32x16, NULL, NULL,
                   aom_highbd_sad32x16x4d_bits10)

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits10,
                   aom_highbd_sad16x32_avg_bits10, aom_highbd_10_variance16x32,
                   aom_highbd_10_sub_pixel_variance16x32,
                   aom_highbd_10_sub_pixel_avg_variance16x32, NULL, NULL,
                   aom_highbd_sad16x32x4d_bits10)

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits10,
                   aom_highbd_sad64x32_avg_bits10, aom_highbd_10_variance64x32,
                   aom_highbd_10_sub_pixel_variance64x32,
                   aom_highbd_10_sub_pixel_avg_variance64x32, NULL, NULL,
                   aom_highbd_sad64x32x4d_bits10)

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits10,
                   aom_highbd_sad32x64_avg_bits10, aom_highbd_10_variance32x64,
                   aom_highbd_10_sub_pixel_variance32x64,
                   aom_highbd_10_sub_pixel_avg_variance32x64, NULL, NULL,
                   aom_highbd_sad32x64x4d_bits10)

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits10,
                   aom_highbd_sad32x32_avg_bits10, aom_highbd_10_variance32x32,
                   aom_highbd_10_sub_pixel_variance32x32,
                   aom_highbd_10_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x3_bits10, aom_highbd_sad32x32x8_bits10,
                   aom_highbd_sad32x32x4d_bits10)

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits10,
                   aom_highbd_sad64x64_avg_bits10, aom_highbd_10_variance64x64,
                   aom_highbd_10_sub_pixel_variance64x64,
                   aom_highbd_10_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x3_bits10, aom_highbd_sad64x64x8_bits10,
                   aom_highbd_sad64x64x4d_bits10)

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits10,
                   aom_highbd_sad16x16_avg_bits10, aom_highbd_10_variance16x16,
                   aom_highbd_10_sub_pixel_variance16x16,
                   aom_highbd_10_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x3_bits10, aom_highbd_sad16x16x8_bits10,
                   aom_highbd_sad16x16x4d_bits10)

        HIGHBD_BFP(BLOCK_16X8, aom_highbd_sad16x8_bits10,
                   aom_highbd_sad16x8_avg_bits10, aom_highbd_10_variance16x8,
                   aom_highbd_10_sub_pixel_variance16x8,
                   aom_highbd_10_sub_pixel_avg_variance16x8,
                   aom_highbd_sad16x8x3_bits10, aom_highbd_sad16x8x8_bits10,
                   aom_highbd_sad16x8x4d_bits10)

        HIGHBD_BFP(BLOCK_8X16, aom_highbd_sad8x16_bits10,
                   aom_highbd_sad8x16_avg_bits10, aom_highbd_10_variance8x16,
                   aom_highbd_10_sub_pixel_variance8x16,
                   aom_highbd_10_sub_pixel_avg_variance8x16,
                   aom_highbd_sad8x16x3_bits10, aom_highbd_sad8x16x8_bits10,
                   aom_highbd_sad8x16x4d_bits10)

        HIGHBD_BFP(
            BLOCK_8X8, aom_highbd_sad8x8_bits10, aom_highbd_sad8x8_avg_bits10,
            aom_highbd_10_variance8x8, aom_highbd_10_sub_pixel_variance8x8,
            aom_highbd_10_sub_pixel_avg_variance8x8, aom_highbd_sad8x8x3_bits10,
            aom_highbd_sad8x8x8_bits10, aom_highbd_sad8x8x4d_bits10)

        HIGHBD_BFP(BLOCK_8X4, aom_highbd_sad8x4_bits10,
                   aom_highbd_sad8x4_avg_bits10, aom_highbd_10_variance8x4,
                   aom_highbd_10_sub_pixel_variance8x4,
                   aom_highbd_10_sub_pixel_avg_variance8x4, NULL,
                   aom_highbd_sad8x4x8_bits10, aom_highbd_sad8x4x4d_bits10)

        HIGHBD_BFP(BLOCK_4X8, aom_highbd_sad4x8_bits10,
                   aom_highbd_sad4x8_avg_bits10, aom_highbd_10_variance4x8,
                   aom_highbd_10_sub_pixel_variance4x8,
                   aom_highbd_10_sub_pixel_avg_variance4x8, NULL,
                   aom_highbd_sad4x8x8_bits10, aom_highbd_sad4x8x4d_bits10)

        HIGHBD_BFP(
            BLOCK_4X4, aom_highbd_sad4x4_bits10, aom_highbd_sad4x4_avg_bits10,
            aom_highbd_10_variance4x4, aom_highbd_10_sub_pixel_variance4x4,
            aom_highbd_10_sub_pixel_avg_variance4x4, aom_highbd_sad4x4x3_bits10,
            aom_highbd_sad4x4x8_bits10, aom_highbd_sad4x4x4d_bits10)

#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
        HIGHBD_BFP(BLOCK_2X2, NULL, NULL, aom_highbd_10_variance2x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_4X2, NULL, NULL, aom_highbd_10_variance4x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_2X4, NULL, NULL, aom_highbd_10_variance2x4, NULL, NULL,
                   NULL, NULL, NULL)
#endif

#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(
            BLOCK_128X128, aom_highbd_sad128x128_bits10,
            aom_highbd_sad128x128_avg_bits10, aom_highbd_10_variance128x128,
            aom_highbd_10_sub_pixel_variance128x128,
            aom_highbd_10_sub_pixel_avg_variance128x128,
            aom_highbd_sad128x128x3_bits10, aom_highbd_sad128x128x8_bits10,
            aom_highbd_sad128x128x4d_bits10)

        HIGHBD_BFP(BLOCK_128X64, aom_highbd_sad128x64_bits10,
                   aom_highbd_sad128x64_avg_bits10,
                   aom_highbd_10_variance128x64,
                   aom_highbd_10_sub_pixel_variance128x64,
                   aom_highbd_10_sub_pixel_avg_variance128x64, NULL, NULL,
                   aom_highbd_sad128x64x4d_bits10)

        HIGHBD_BFP(BLOCK_64X128, aom_highbd_sad64x128_bits10,
                   aom_highbd_sad64x128_avg_bits10,
                   aom_highbd_10_variance64x128,
                   aom_highbd_10_sub_pixel_variance64x128,
                   aom_highbd_10_sub_pixel_avg_variance64x128, NULL, NULL,
                   aom_highbd_sad64x128x4d_bits10)
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits10,
                    aom_highbd_10_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits10,
                    aom_highbd_10_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits10,
                    aom_highbd_10_masked_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X32, aom_highbd_masked_sad128x32_bits10,
                    aom_highbd_10_masked_sub_pixel_variance128x32)

        HIGHBD_MBFP(BLOCK_32X128, aom_highbd_masked_sad32x128_bits10,
                    aom_highbd_10_masked_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif
#if CONFIG_MOTION_VAR
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits10,
                    aom_highbd_10_obmc_variance128x128,
                    aom_highbd_10_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits10,
                    aom_highbd_10_obmc_variance128x64,
                    aom_highbd_10_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits10,
                    aom_highbd_10_obmc_variance64x128,
                    aom_highbd_10_obmc_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X32, aom_highbd_obmc_sad128x32_bits10,
                    aom_highbd_10_obmc_variance128x32,
                    aom_highbd_10_obmc_sub_pixel_variance128x32)

        HIGHBD_OBFP(BLOCK_32X128, aom_highbd_obmc_sad32x128_bits10,
                    aom_highbd_10_obmc_variance32x128,
                    aom_highbd_10_obmc_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif
#endif  // CONFIG_MOTION_VAR
        break;

      case AOM_BITS_12:
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(BLOCK_128X32, aom_highbd_sad128x32_bits12,
                   aom_highbd_sad128x32_avg_bits12,
                   aom_highbd_12_variance128x32,
                   aom_highbd_12_sub_pixel_variance128x32,
                   aom_highbd_12_sub_pixel_avg_variance128x32, NULL, NULL,
                   aom_highbd_sad128x32x4d_bits12)

        HIGHBD_BFP(BLOCK_32X128, aom_highbd_sad32x128_bits12,
                   aom_highbd_sad32x128_avg_bits12,
                   aom_highbd_12_variance32x128,
                   aom_highbd_12_sub_pixel_variance32x128,
                   aom_highbd_12_sub_pixel_avg_variance32x128, NULL, NULL,
                   aom_highbd_sad32x128x4d_bits12)
#endif  // CONFIG_EXT_PARTITION

        HIGHBD_BFP(BLOCK_64X16, aom_highbd_sad64x16_bits12,
                   aom_highbd_sad64x16_avg_bits12, aom_highbd_12_variance64x16,
                   aom_highbd_12_sub_pixel_variance64x16,
                   aom_highbd_12_sub_pixel_avg_variance64x16, NULL, NULL,
                   aom_highbd_sad64x16x4d_bits12)

        HIGHBD_BFP(BLOCK_16X64, aom_highbd_sad16x64_bits12,
                   aom_highbd_sad16x64_avg_bits12, aom_highbd_12_variance16x64,
                   aom_highbd_12_sub_pixel_variance16x64,
                   aom_highbd_12_sub_pixel_avg_variance16x64, NULL, NULL,
                   aom_highbd_sad16x64x4d_bits12)

        HIGHBD_BFP(BLOCK_32X8, aom_highbd_sad32x8_bits12,
                   aom_highbd_sad32x8_avg_bits12, aom_highbd_12_variance32x8,
                   aom_highbd_12_sub_pixel_variance32x8,
                   aom_highbd_12_sub_pixel_avg_variance32x8, NULL, NULL,
                   aom_highbd_sad32x8x4d_bits12)

        HIGHBD_BFP(BLOCK_8X32, aom_highbd_sad8x32_bits12,
                   aom_highbd_sad8x32_avg_bits12, aom_highbd_12_variance8x32,
                   aom_highbd_12_sub_pixel_variance8x32,
                   aom_highbd_12_sub_pixel_avg_variance8x32, NULL, NULL,
                   aom_highbd_sad8x32x4d_bits12)

        HIGHBD_BFP(BLOCK_16X4, aom_highbd_sad16x4_bits12,
                   aom_highbd_sad16x4_avg_bits12, aom_highbd_12_variance16x4,
                   aom_highbd_12_sub_pixel_variance16x4,
                   aom_highbd_12_sub_pixel_avg_variance16x4, NULL, NULL,
                   aom_highbd_sad16x4x4d_bits12)

        HIGHBD_BFP(BLOCK_4X16, aom_highbd_sad4x16_bits12,
                   aom_highbd_sad4x16_avg_bits12, aom_highbd_12_variance4x16,
                   aom_highbd_12_sub_pixel_variance4x16,
                   aom_highbd_12_sub_pixel_avg_variance4x16, NULL, NULL,
                   aom_highbd_sad4x16x4d_bits12)
#endif

        HIGHBD_BFP(BLOCK_32X16, aom_highbd_sad32x16_bits12,
                   aom_highbd_sad32x16_avg_bits12, aom_highbd_12_variance32x16,
                   aom_highbd_12_sub_pixel_variance32x16,
                   aom_highbd_12_sub_pixel_avg_variance32x16, NULL, NULL,
                   aom_highbd_sad32x16x4d_bits12)

        HIGHBD_BFP(BLOCK_16X32, aom_highbd_sad16x32_bits12,
                   aom_highbd_sad16x32_avg_bits12, aom_highbd_12_variance16x32,
                   aom_highbd_12_sub_pixel_variance16x32,
                   aom_highbd_12_sub_pixel_avg_variance16x32, NULL, NULL,
                   aom_highbd_sad16x32x4d_bits12)

        HIGHBD_BFP(BLOCK_64X32, aom_highbd_sad64x32_bits12,
                   aom_highbd_sad64x32_avg_bits12, aom_highbd_12_variance64x32,
                   aom_highbd_12_sub_pixel_variance64x32,
                   aom_highbd_12_sub_pixel_avg_variance64x32, NULL, NULL,
                   aom_highbd_sad64x32x4d_bits12)

        HIGHBD_BFP(BLOCK_32X64, aom_highbd_sad32x64_bits12,
                   aom_highbd_sad32x64_avg_bits12, aom_highbd_12_variance32x64,
                   aom_highbd_12_sub_pixel_variance32x64,
                   aom_highbd_12_sub_pixel_avg_variance32x64, NULL, NULL,
                   aom_highbd_sad32x64x4d_bits12)

        HIGHBD_BFP(BLOCK_32X32, aom_highbd_sad32x32_bits12,
                   aom_highbd_sad32x32_avg_bits12, aom_highbd_12_variance32x32,
                   aom_highbd_12_sub_pixel_variance32x32,
                   aom_highbd_12_sub_pixel_avg_variance32x32,
                   aom_highbd_sad32x32x3_bits12, aom_highbd_sad32x32x8_bits12,
                   aom_highbd_sad32x32x4d_bits12)

        HIGHBD_BFP(BLOCK_64X64, aom_highbd_sad64x64_bits12,
                   aom_highbd_sad64x64_avg_bits12, aom_highbd_12_variance64x64,
                   aom_highbd_12_sub_pixel_variance64x64,
                   aom_highbd_12_sub_pixel_avg_variance64x64,
                   aom_highbd_sad64x64x3_bits12, aom_highbd_sad64x64x8_bits12,
                   aom_highbd_sad64x64x4d_bits12)

        HIGHBD_BFP(BLOCK_16X16, aom_highbd_sad16x16_bits12,
                   aom_highbd_sad16x16_avg_bits12, aom_highbd_12_variance16x16,
                   aom_highbd_12_sub_pixel_variance16x16,
                   aom_highbd_12_sub_pixel_avg_variance16x16,
                   aom_highbd_sad16x16x3_bits12, aom_highbd_sad16x16x8_bits12,
                   aom_highbd_sad16x16x4d_bits12)

        HIGHBD_BFP(BLOCK_16X8, aom_highbd_sad16x8_bits12,
                   aom_highbd_sad16x8_avg_bits12, aom_highbd_12_variance16x8,
                   aom_highbd_12_sub_pixel_variance16x8,
                   aom_highbd_12_sub_pixel_avg_variance16x8,
                   aom_highbd_sad16x8x3_bits12, aom_highbd_sad16x8x8_bits12,
                   aom_highbd_sad16x8x4d_bits12)

        HIGHBD_BFP(BLOCK_8X16, aom_highbd_sad8x16_bits12,
                   aom_highbd_sad8x16_avg_bits12, aom_highbd_12_variance8x16,
                   aom_highbd_12_sub_pixel_variance8x16,
                   aom_highbd_12_sub_pixel_avg_variance8x16,
                   aom_highbd_sad8x16x3_bits12, aom_highbd_sad8x16x8_bits12,
                   aom_highbd_sad8x16x4d_bits12)

        HIGHBD_BFP(
            BLOCK_8X8, aom_highbd_sad8x8_bits12, aom_highbd_sad8x8_avg_bits12,
            aom_highbd_12_variance8x8, aom_highbd_12_sub_pixel_variance8x8,
            aom_highbd_12_sub_pixel_avg_variance8x8, aom_highbd_sad8x8x3_bits12,
            aom_highbd_sad8x8x8_bits12, aom_highbd_sad8x8x4d_bits12)

        HIGHBD_BFP(BLOCK_8X4, aom_highbd_sad8x4_bits12,
                   aom_highbd_sad8x4_avg_bits12, aom_highbd_12_variance8x4,
                   aom_highbd_12_sub_pixel_variance8x4,
                   aom_highbd_12_sub_pixel_avg_variance8x4, NULL,
                   aom_highbd_sad8x4x8_bits12, aom_highbd_sad8x4x4d_bits12)

        HIGHBD_BFP(BLOCK_4X8, aom_highbd_sad4x8_bits12,
                   aom_highbd_sad4x8_avg_bits12, aom_highbd_12_variance4x8,
                   aom_highbd_12_sub_pixel_variance4x8,
                   aom_highbd_12_sub_pixel_avg_variance4x8, NULL,
                   aom_highbd_sad4x8x8_bits12, aom_highbd_sad4x8x4d_bits12)

        HIGHBD_BFP(
            BLOCK_4X4, aom_highbd_sad4x4_bits12, aom_highbd_sad4x4_avg_bits12,
            aom_highbd_12_variance4x4, aom_highbd_12_sub_pixel_variance4x4,
            aom_highbd_12_sub_pixel_avg_variance4x4, aom_highbd_sad4x4x3_bits12,
            aom_highbd_sad4x4x8_bits12, aom_highbd_sad4x4x4d_bits12)

#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
        HIGHBD_BFP(BLOCK_2X2, NULL, NULL, aom_highbd_12_variance2x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_4X2, NULL, NULL, aom_highbd_12_variance4x2, NULL, NULL,
                   NULL, NULL, NULL)
        HIGHBD_BFP(BLOCK_2X4, NULL, NULL, aom_highbd_12_variance2x4, NULL, NULL,
                   NULL, NULL, NULL)
#endif

#if CONFIG_EXT_PARTITION
        HIGHBD_BFP(
            BLOCK_128X128, aom_highbd_sad128x128_bits12,
            aom_highbd_sad128x128_avg_bits12, aom_highbd_12_variance128x128,
            aom_highbd_12_sub_pixel_variance128x128,
            aom_highbd_12_sub_pixel_avg_variance128x128,
            aom_highbd_sad128x128x3_bits12, aom_highbd_sad128x128x8_bits12,
            aom_highbd_sad128x128x4d_bits12)

        HIGHBD_BFP(BLOCK_128X64, aom_highbd_sad128x64_bits12,
                   aom_highbd_sad128x64_avg_bits12,
                   aom_highbd_12_variance128x64,
                   aom_highbd_12_sub_pixel_variance128x64,
                   aom_highbd_12_sub_pixel_avg_variance128x64, NULL, NULL,
                   aom_highbd_sad128x64x4d_bits12)

        HIGHBD_BFP(BLOCK_64X128, aom_highbd_sad64x128_bits12,
                   aom_highbd_sad64x128_avg_bits12,
                   aom_highbd_12_variance64x128,
                   aom_highbd_12_sub_pixel_variance64x128,
                   aom_highbd_12_sub_pixel_avg_variance64x128, NULL, NULL,
                   aom_highbd_sad64x128x4d_bits12)
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X128, aom_highbd_masked_sad128x128_bits12,
                    aom_highbd_12_masked_sub_pixel_variance128x128)
        HIGHBD_MBFP(BLOCK_128X64, aom_highbd_masked_sad128x64_bits12,
                    aom_highbd_12_masked_sub_pixel_variance128x64)
        HIGHBD_MBFP(BLOCK_64X128, aom_highbd_masked_sad64x128_bits12,
                    aom_highbd_12_masked_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_MBFP(BLOCK_128X32, aom_highbd_masked_sad128x32_bits12,
                    aom_highbd_12_masked_sub_pixel_variance128x32)

        HIGHBD_MBFP(BLOCK_32X128, aom_highbd_masked_sad32x128_bits12,
                    aom_highbd_12_masked_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif

#if CONFIG_MOTION_VAR
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X128, aom_highbd_obmc_sad128x128_bits12,
                    aom_highbd_12_obmc_variance128x128,
                    aom_highbd_12_obmc_sub_pixel_variance128x128)
        HIGHBD_OBFP(BLOCK_128X64, aom_highbd_obmc_sad128x64_bits12,
                    aom_highbd_12_obmc_variance128x64,
                    aom_highbd_12_obmc_sub_pixel_variance128x64)
        HIGHBD_OBFP(BLOCK_64X128, aom_highbd_obmc_sad64x128_bits12,
                    aom_highbd_12_obmc_variance64x128,
                    aom_highbd_12_obmc_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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
#if CONFIG_EXT_PARTITION_TYPES
#if CONFIG_EXT_PARTITION
        HIGHBD_OBFP(BLOCK_128X32, aom_highbd_obmc_sad128x32_bits12,
                    aom_highbd_12_obmc_variance128x32,
                    aom_highbd_12_obmc_sub_pixel_variance128x32)

        HIGHBD_OBFP(BLOCK_32X128, aom_highbd_obmc_sad32x128_bits12,
                    aom_highbd_12_obmc_variance32x128,
                    aom_highbd_12_obmc_sub_pixel_variance32x128)
#endif  // CONFIG_EXT_PARTITION

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
#endif
#endif  // CONFIG_MOTION_VAR
        break;

      default:
        assert(0 &&
               "cm->bit_depth should be AOM_BITS_8, "
               "AOM_BITS_10 or AOM_BITS_12");
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

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

void set_compound_tools(AV1_COMMON *cm) {
  (void)cm;
#if CONFIG_INTERINTRA
  cm->allow_interintra_compound = 1;
#endif  // CONFIG_INTERINTRA
#if CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
  cm->allow_masked_compound = 1;
#endif  // CONFIG_WEDGE || CONFIG_COMPOUND_SEGMENT
}

void av1_change_config(struct AV1_COMP *cpi, const AV1EncoderConfig *oxcf) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  MACROBLOCK *const x = &cpi->td.mb;

  if (cm->profile != oxcf->profile) cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;
  cm->color_space = oxcf->color_space;
#if CONFIG_COLORSPACE_HEADERS
  cm->transfer_function = oxcf->transfer_function;
  cm->chroma_sample_position = oxcf->chroma_sample_position;
#endif
  cm->color_range = oxcf->color_range;

  if (cm->profile <= PROFILE_1)
    assert(cm->bit_depth == AOM_BITS_8);
  else
    assert(cm->bit_depth > AOM_BITS_8);

  cpi->oxcf = *oxcf;
  x->e_mbd.bd = (int)cm->bit_depth;
#if CONFIG_GLOBAL_MOTION
  x->e_mbd.global_motion = cm->global_motion;
#endif  // CONFIG_GLOBAL_MOTION

  if ((oxcf->pass == 0) && (oxcf->rc_mode == AOM_Q)) {
    rc->baseline_gf_interval = FIXED_GF_INTERVAL;
  } else {
    rc->baseline_gf_interval = (MIN_GF_INTERVAL + MAX_GF_INTERVAL) / 2;
  }

  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
#if CONFIG_EXT_REFS
  cpi->refresh_bwd_ref_frame = 0;
  cpi->refresh_alt2_ref_frame = 0;
#endif  // CONFIG_EXT_REFS

  cm->refresh_frame_context =
      (oxcf->error_resilient_mode || oxcf->frame_parallel_decoding_mode)
          ? REFRESH_FRAME_CONTEXT_FORWARD
          : REFRESH_FRAME_CONTEXT_BACKWARD;
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  cm->reset_frame_context = RESET_FRAME_CONTEXT_NONE;
#endif

  if (x->palette_buffer == NULL) {
    CHECK_MEM_ERROR(cm, x->palette_buffer,
                    aom_memalign(16, sizeof(*x->palette_buffer)));
  }
  set_compound_tools(cm);
  av1_reset_segment_features(cm);
#if CONFIG_AMVR
  set_high_precision_mv(cpi, 0, 0);
#else
  set_high_precision_mv(cpi, 0);
#endif

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

  cm->interp_filter = cpi->sf.default_interp_filter;

  if (cpi->oxcf.render_width > 0 && cpi->oxcf.render_height > 0) {
    cm->render_width = cpi->oxcf.render_width;
    cm->render_height = cpi->oxcf.render_height;
  } else {
    cm->render_width = cpi->oxcf.width;
    cm->render_height = cpi->oxcf.height;
  }
  cm->width = cpi->oxcf.width;
  cm->height = cpi->oxcf.height;

  if (cpi->initial_width) {
    if (cm->width > cpi->initial_width || cm->height > cpi->initial_height) {
      av1_free_context_buffers(cm);
      av1_free_pc_tree(&cpi->td);
      alloc_compressor_data(cpi);
      realloc_segmentation_maps(cpi);
      cpi->initial_width = cpi->initial_height = 0;
    }
  }
  update_frame_size(cpi);

  cpi->alt_ref_source = NULL;
  rc->is_src_frame_alt_ref = 0;

#if CONFIG_EXT_REFS
  rc->is_bwd_ref_frame = 0;
  rc->is_last_bipred_frame = 0;
  rc->is_bipred_frame = 0;
#endif  // CONFIG_EXT_REFS

#if 0
  // Experimental RD Code
  cpi->frame_distortion = 0;
  cpi->last_frame_distortion = 0;
#endif

  set_tile_info(cpi);

  cpi->ext_refresh_frame_flags_pending = 0;
  cpi->ext_refresh_frame_context_pending = 0;

#if CONFIG_HIGHBITDEPTH
  highbd_set_var_fns(cpi);
#endif
#if CONFIG_ANS && ANS_MAX_SYMBOLS
  cpi->common.ans_window_size_log2 = cpi->oxcf.ans_window_size_log2;
#endif  // CONFIG_ANS && ANS_MAX_SYMBOLS
#if CONFIG_AMVR
  cm->seq_mv_precision_level = 2;
#endif
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

#if CONFIG_NCOBMC_ADAPT_WEIGHT
  get_default_ncobmc_kernels(cm);
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

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
#if CONFIG_XIPHRC
  cpi->od_rc.framerate = cpi->framerate;
  cpi->od_rc.frame_width = cm->render_width;
  cpi->od_rc.frame_height = cm->render_height;
  cpi->od_rc.keyframe_rate = oxcf->key_freq;
  cpi->od_rc.goldenframe_rate = FIXED_GF_INTERVAL;
  cpi->od_rc.altref_rate = 25;
  cpi->od_rc.firstpass_quant = 1;
  cpi->od_rc.bit_depth = cm->bit_depth;
  cpi->od_rc.minq = oxcf->best_allowed_q;
  cpi->od_rc.maxq = oxcf->worst_allowed_q;
  if (cpi->oxcf.rc_mode == AOM_CQ) cpi->od_rc.minq = cpi->od_rc.quality;
  cpi->od_rc.quality = cpi->oxcf.rc_mode == AOM_Q ? oxcf->cq_level : -1;
  cpi->od_rc.periodic_boosts = oxcf->frame_periodic_boost;
  od_enc_rc_init(&cpi->od_rc,
                 cpi->oxcf.rc_mode == AOM_Q ? -1 : oxcf->target_bandwidth,
                 oxcf->maximum_buffer_size_ms);
#else
  av1_rc_init(&cpi->oxcf, oxcf->pass, &cpi->rc);
#endif

  cm->current_video_frame = 0;
  cpi->partition_search_skippable_frame = 0;
  cpi->tile_data = NULL;
  cpi->last_show_frame_buf_idx = INVALID_IDX;

  realloc_segmentation_maps(cpi);

  for (i = 0; i < NMV_CONTEXTS; ++i) {
    memset(cpi->nmv_costs, 0, sizeof(cpi->nmv_costs));
    memset(cpi->nmv_costs_hp, 0, sizeof(cpi->nmv_costs_hp));
  }

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
  cpi->multi_arf_last_grp_enabled = 0;

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
  av1_zero_array(aggregate_fc_per_type, FRAME_CONTEXTS);
#endif  // CONFIG_ENTROPY_STATS

  cpi->first_time_stamp_ever = INT64_MAX;

  for (i = 0; i < NMV_CONTEXTS; ++i) {
    cpi->td.mb.nmvcost[i][0] = &cpi->nmv_costs[i][0][MV_MAX];
    cpi->td.mb.nmvcost[i][1] = &cpi->nmv_costs[i][1][MV_MAX];
    cpi->td.mb.nmvcost_hp[i][0] = &cpi->nmv_costs_hp[i][0][MV_MAX];
    cpi->td.mb.nmvcost_hp[i][1] = &cpi->nmv_costs_hp[i][1][MV_MAX];
  }

#ifdef OUTPUT_YUV_SKINMAP
  yuv_skinmap_file = fopen("skinmap.yuv", "ab");
#endif
#ifdef OUTPUT_YUV_REC
  yuv_rec_file = fopen("rec.yuv", "wb");
#endif

#if 0
  framepsnr = fopen("framepsnr.stt", "a");
  kf_list = fopen("kf_list.stt", "w");
#endif

#if CONFIG_XIPHRC
  if (oxcf->pass == 2) {
    cpi->od_rc.twopass_allframes_buf = oxcf->two_pass_stats_in.buf;
    cpi->od_rc.twopass_allframes_buf_size = oxcf->two_pass_stats_in.sz;
  }
#else
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
#endif

#if CONFIG_MOTION_VAR
#if CONFIG_HIGHBITDEPTH
  int buf_scaler = 2;
#else
  int buf_scaler = 1;
#endif
  CHECK_MEM_ERROR(
      cm, cpi->td.mb.above_pred_buf,
      (uint8_t *)aom_memalign(16,
                              buf_scaler * MAX_MB_PLANE * MAX_SB_SQUARE *
                                  sizeof(*cpi->td.mb.above_pred_buf)));
  CHECK_MEM_ERROR(
      cm, cpi->td.mb.left_pred_buf,
      (uint8_t *)aom_memalign(16,
                              buf_scaler * MAX_MB_PLANE * MAX_SB_SQUARE *
                                  sizeof(*cpi->td.mb.left_pred_buf)));

  CHECK_MEM_ERROR(cm, cpi->td.mb.wsrc_buf,
                  (int32_t *)aom_memalign(
                      16, MAX_SB_SQUARE * sizeof(*cpi->td.mb.wsrc_buf)));

  CHECK_MEM_ERROR(cm, cpi->td.mb.mask_buf,
                  (int32_t *)aom_memalign(
                      16, MAX_SB_SQUARE * sizeof(*cpi->td.mb.mask_buf)));

#endif

  av1_set_speed_features_framesize_independent(cpi);
  av1_set_speed_features_framesize_dependent(cpi);

#define BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX3F, SDX8F, SDX4DF) \
  cpi->fn_ptr[BT].sdf = SDF;                                    \
  cpi->fn_ptr[BT].sdaf = SDAF;                                  \
  cpi->fn_ptr[BT].vf = VF;                                      \
  cpi->fn_ptr[BT].svf = SVF;                                    \
  cpi->fn_ptr[BT].svaf = SVAF;                                  \
  cpi->fn_ptr[BT].sdx3f = SDX3F;                                \
  cpi->fn_ptr[BT].sdx8f = SDX8F;                                \
  cpi->fn_ptr[BT].sdx4df = SDX4DF;

#if CONFIG_EXT_PARTITION_TYPES
  BFP(BLOCK_4X16, aom_sad4x16, aom_sad4x16_avg, aom_variance4x16,
      aom_sub_pixel_variance4x16, aom_sub_pixel_avg_variance4x16, NULL, NULL,
      aom_sad4x16x4d)

  BFP(BLOCK_16X4, aom_sad16x4, aom_sad16x4_avg, aom_variance16x4,
      aom_sub_pixel_variance16x4, aom_sub_pixel_avg_variance16x4, NULL, NULL,
      aom_sad16x4x4d)

  BFP(BLOCK_8X32, aom_sad8x32, aom_sad8x32_avg, aom_variance8x32,
      aom_sub_pixel_variance8x32, aom_sub_pixel_avg_variance8x32, NULL, NULL,
      aom_sad8x32x4d)

  BFP(BLOCK_32X8, aom_sad32x8, aom_sad32x8_avg, aom_variance32x8,
      aom_sub_pixel_variance32x8, aom_sub_pixel_avg_variance32x8, NULL, NULL,
      aom_sad32x8x4d)

  BFP(BLOCK_16X64, aom_sad16x64, aom_sad16x64_avg, aom_variance16x64,
      aom_sub_pixel_variance16x64, aom_sub_pixel_avg_variance16x64, NULL, NULL,
      aom_sad16x64x4d)

  BFP(BLOCK_64X16, aom_sad64x16, aom_sad64x16_avg, aom_variance64x16,
      aom_sub_pixel_variance64x16, aom_sub_pixel_avg_variance64x16, NULL, NULL,
      aom_sad64x16x4d)

#if CONFIG_EXT_PARTITION
  BFP(BLOCK_32X128, aom_sad32x128, aom_sad32x128_avg, aom_variance32x128,
      aom_sub_pixel_variance32x128, aom_sub_pixel_avg_variance32x128, NULL,
      NULL, aom_sad32x128x4d)

  BFP(BLOCK_128X32, aom_sad128x32, aom_sad128x32_avg, aom_variance128x32,
      aom_sub_pixel_variance128x32, aom_sub_pixel_avg_variance128x32, NULL,
      NULL, aom_sad128x32x4d)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_EXT_PARTITION
  BFP(BLOCK_128X128, aom_sad128x128, aom_sad128x128_avg, aom_variance128x128,
      aom_sub_pixel_variance128x128, aom_sub_pixel_avg_variance128x128,
      aom_sad128x128x3, aom_sad128x128x8, aom_sad128x128x4d)

  BFP(BLOCK_128X64, aom_sad128x64, aom_sad128x64_avg, aom_variance128x64,
      aom_sub_pixel_variance128x64, aom_sub_pixel_avg_variance128x64, NULL,
      NULL, aom_sad128x64x4d)

  BFP(BLOCK_64X128, aom_sad64x128, aom_sad64x128_avg, aom_variance64x128,
      aom_sub_pixel_variance64x128, aom_sub_pixel_avg_variance64x128, NULL,
      NULL, aom_sad64x128x4d)
#endif  // CONFIG_EXT_PARTITION

  BFP(BLOCK_32X16, aom_sad32x16, aom_sad32x16_avg, aom_variance32x16,
      aom_sub_pixel_variance32x16, aom_sub_pixel_avg_variance32x16, NULL, NULL,
      aom_sad32x16x4d)

  BFP(BLOCK_16X32, aom_sad16x32, aom_sad16x32_avg, aom_variance16x32,
      aom_sub_pixel_variance16x32, aom_sub_pixel_avg_variance16x32, NULL, NULL,
      aom_sad16x32x4d)

  BFP(BLOCK_64X32, aom_sad64x32, aom_sad64x32_avg, aom_variance64x32,
      aom_sub_pixel_variance64x32, aom_sub_pixel_avg_variance64x32, NULL, NULL,
      aom_sad64x32x4d)

  BFP(BLOCK_32X64, aom_sad32x64, aom_sad32x64_avg, aom_variance32x64,
      aom_sub_pixel_variance32x64, aom_sub_pixel_avg_variance32x64, NULL, NULL,
      aom_sad32x64x4d)

  BFP(BLOCK_32X32, aom_sad32x32, aom_sad32x32_avg, aom_variance32x32,
      aom_sub_pixel_variance32x32, aom_sub_pixel_avg_variance32x32,
      aom_sad32x32x3, aom_sad32x32x8, aom_sad32x32x4d)

  BFP(BLOCK_64X64, aom_sad64x64, aom_sad64x64_avg, aom_variance64x64,
      aom_sub_pixel_variance64x64, aom_sub_pixel_avg_variance64x64,
      aom_sad64x64x3, aom_sad64x64x8, aom_sad64x64x4d)

  BFP(BLOCK_16X16, aom_sad16x16, aom_sad16x16_avg, aom_variance16x16,
      aom_sub_pixel_variance16x16, aom_sub_pixel_avg_variance16x16,
      aom_sad16x16x3, aom_sad16x16x8, aom_sad16x16x4d)

  BFP(BLOCK_16X8, aom_sad16x8, aom_sad16x8_avg, aom_variance16x8,
      aom_sub_pixel_variance16x8, aom_sub_pixel_avg_variance16x8, aom_sad16x8x3,
      aom_sad16x8x8, aom_sad16x8x4d)

  BFP(BLOCK_8X16, aom_sad8x16, aom_sad8x16_avg, aom_variance8x16,
      aom_sub_pixel_variance8x16, aom_sub_pixel_avg_variance8x16, aom_sad8x16x3,
      aom_sad8x16x8, aom_sad8x16x4d)

  BFP(BLOCK_8X8, aom_sad8x8, aom_sad8x8_avg, aom_variance8x8,
      aom_sub_pixel_variance8x8, aom_sub_pixel_avg_variance8x8, aom_sad8x8x3,
      aom_sad8x8x8, aom_sad8x8x4d)

  BFP(BLOCK_8X4, aom_sad8x4, aom_sad8x4_avg, aom_variance8x4,
      aom_sub_pixel_variance8x4, aom_sub_pixel_avg_variance8x4, NULL,
      aom_sad8x4x8, aom_sad8x4x4d)

  BFP(BLOCK_4X8, aom_sad4x8, aom_sad4x8_avg, aom_variance4x8,
      aom_sub_pixel_variance4x8, aom_sub_pixel_avg_variance4x8, NULL,
      aom_sad4x8x8, aom_sad4x8x4d)

  BFP(BLOCK_4X4, aom_sad4x4, aom_sad4x4_avg, aom_variance4x4,
      aom_sub_pixel_variance4x4, aom_sub_pixel_avg_variance4x4, aom_sad4x4x3,
      aom_sad4x4x8, aom_sad4x4x4d)

#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  BFP(BLOCK_2X2, NULL, NULL, aom_variance2x2, NULL, NULL, NULL, NULL, NULL)
  BFP(BLOCK_2X4, NULL, NULL, aom_variance2x4, NULL, NULL, NULL, NULL, NULL)
  BFP(BLOCK_4X2, NULL, NULL, aom_variance4x2, NULL, NULL, NULL, NULL, NULL)
#endif

#if CONFIG_MOTION_VAR
#define OBFP(BT, OSDF, OVF, OSVF) \
  cpi->fn_ptr[BT].osdf = OSDF;    \
  cpi->fn_ptr[BT].ovf = OVF;      \
  cpi->fn_ptr[BT].osvf = OSVF;

#if CONFIG_EXT_PARTITION
  OBFP(BLOCK_128X128, aom_obmc_sad128x128, aom_obmc_variance128x128,
       aom_obmc_sub_pixel_variance128x128)
  OBFP(BLOCK_128X64, aom_obmc_sad128x64, aom_obmc_variance128x64,
       aom_obmc_sub_pixel_variance128x64)
  OBFP(BLOCK_64X128, aom_obmc_sad64x128, aom_obmc_variance64x128,
       aom_obmc_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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

#if CONFIG_EXT_PARTITION_TYPES
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

#if CONFIG_EXT_PARTITION
  OBFP(BLOCK_32X128, aom_obmc_sad32x128, aom_obmc_variance32x128,
       aom_obmc_sub_pixel_variance32x128)

  OBFP(BLOCK_128X32, aom_obmc_sad128x32, aom_obmc_variance128x32,
       aom_obmc_sub_pixel_variance128x32)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES
#endif  // CONFIG_MOTION_VAR

#define MBFP(BT, MCSDF, MCSVF)  \
  cpi->fn_ptr[BT].msdf = MCSDF; \
  cpi->fn_ptr[BT].msvf = MCSVF;

#if CONFIG_EXT_PARTITION
  MBFP(BLOCK_128X128, aom_masked_sad128x128,
       aom_masked_sub_pixel_variance128x128)
  MBFP(BLOCK_128X64, aom_masked_sad128x64, aom_masked_sub_pixel_variance128x64)
  MBFP(BLOCK_64X128, aom_masked_sad64x128, aom_masked_sub_pixel_variance64x128)
#endif  // CONFIG_EXT_PARTITION
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

#if CONFIG_EXT_PARTITION_TYPES
  MBFP(BLOCK_4X16, aom_masked_sad4x16, aom_masked_sub_pixel_variance4x16)

  MBFP(BLOCK_16X4, aom_masked_sad16x4, aom_masked_sub_pixel_variance16x4)

  MBFP(BLOCK_8X32, aom_masked_sad8x32, aom_masked_sub_pixel_variance8x32)

  MBFP(BLOCK_32X8, aom_masked_sad32x8, aom_masked_sub_pixel_variance32x8)

  MBFP(BLOCK_16X64, aom_masked_sad16x64, aom_masked_sub_pixel_variance16x64)

  MBFP(BLOCK_64X16, aom_masked_sad64x16, aom_masked_sub_pixel_variance64x16)

#if CONFIG_EXT_PARTITION
  MBFP(BLOCK_32X128, aom_masked_sad32x128, aom_masked_sub_pixel_variance32x128)

  MBFP(BLOCK_128X32, aom_masked_sad128x32, aom_masked_sub_pixel_variance128x32)
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_HIGHBITDEPTH
  highbd_set_var_fns(cpi);
#endif

  /* av1_init_quantizer() is first called here. Add check in
   * av1_frame_init_quantizer() so that av1_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * av1_init_quantizer() for every frame.
   */
  av1_init_quantizer(cpi);
#if CONFIG_AOM_QM
  aom_qm_init(cm);
#endif

  av1_loop_filter_init(cm);
#if CONFIG_FRAME_SUPERRES
  cm->superres_scale_denominator = SCALE_NUMERATOR;
  cm->superres_upscaled_width = oxcf->width;
  cm->superres_upscaled_height = oxcf->height;
#endif  // CONFIG_FRAME_SUPERRES
#if CONFIG_LOOP_RESTORATION
  av1_loop_restoration_precal();
#endif  // CONFIG_LOOP_RESTORATION

  cm->error.setjmp = 0;

  return cpi;
}

#define SNPRINT(H, T) snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T))

#define SNPRINT2(H, T, V) \
  snprintf((H) + strlen(H), sizeof(H) - strlen(H), (T), (V))

void av1_remove_compressor(AV1_COMP *cpi) {
  AV1_COMMON *cm;
  unsigned int i;
  int t;

  if (!cpi) return;

  cm = &cpi->common;
  if (cm->current_video_frame > 0) {
#if CONFIG_ENTROPY_STATS
    if (cpi->oxcf.pass != 1) {
      fprintf(stderr, "Writing counts.stt\n");
      FILE *f = fopen("counts.stt", "wb");
      fwrite(&aggregate_fc, sizeof(aggregate_fc), 1, f);
      fwrite(aggregate_fc_per_type, sizeof(aggregate_fc_per_type[0]),
             FRAME_CONTEXTS, f);
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
                 "WstPsnr\tWstSsim\tWstFast\tWstHVS");
        snprintf(results, sizeof(results),
                 "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f\t"
                 "%7.3f\t%7.3f\t%7.3f\t%7.3f",
                 dr, cpi->psnr.stat[ALL] / cpi->count, total_psnr,
                 cpi->psnr.stat[ALL] / cpi->count, total_psnr, total_ssim,
                 total_ssim, cpi->fastssim.stat[ALL] / cpi->count,
                 cpi->psnrhvs.stat[ALL] / cpi->count, cpi->psnr.worst,
                 cpi->worst_ssim, cpi->fastssim.worst, cpi->psnrhvs.worst);

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

  for (t = 0; t < cpi->num_workers; ++t) {
    AVxWorker *const worker = &cpi->workers[t];
    EncWorkerData *const thread_data = &cpi->tile_thr_data[t];

    // Deallocate allocated threads.
    aom_get_worker_interface()->end(worker);

    // Deallocate allocated thread data.
    if (t < cpi->num_workers - 1) {
      aom_free(thread_data->td->palette_buffer);
#if CONFIG_MOTION_VAR
      aom_free(thread_data->td->above_pred_buf);
      aom_free(thread_data->td->left_pred_buf);
      aom_free(thread_data->td->wsrc_buf);
      aom_free(thread_data->td->mask_buf);
#endif  // CONFIG_MOTION_VAR
      aom_free(thread_data->td->counts);
      av1_free_pc_tree(thread_data->td);
      aom_free(thread_data->td);
    }
  }
  aom_free(cpi->tile_thr_data);
  aom_free(cpi->workers);

  if (cpi->num_workers > 1) av1_loop_filter_dealloc(&cpi->lf_row_sync);

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
  av1_free_ref_frame_buffers(cm->buffer_pool);
  aom_free(cpi);

#ifdef OUTPUT_YUV_SKINMAP
  fclose(yuv_skinmap_file);
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

static void generate_psnr_packet(AV1_COMP *cpi) {
  struct aom_codec_cx_pkt pkt;
  int i;
  PSNR_STATS psnr;
#if CONFIG_HIGHBITDEPTH
  aom_calc_highbd_psnr(cpi->source, cpi->common.frame_to_show, &psnr,
                       cpi->td.mb.e_mbd.bd, cpi->oxcf.input_bit_depth);
#else
  aom_calc_psnr(cpi->source, cpi->common.frame_to_show, &psnr);
#endif

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

  cpi->ref_frame_flags = ref_frame_flags;
  return 0;
}

void av1_update_reference(AV1_COMP *cpi, int ref_frame_flags) {
  cpi->ext_refresh_golden_frame = (ref_frame_flags & AOM_GOLD_FLAG) != 0;
  cpi->ext_refresh_alt_ref_frame = (ref_frame_flags & AOM_ALT_FLAG) != 0;
  cpi->ext_refresh_last_frame = (ref_frame_flags & AOM_LAST_FLAG) != 0;
  cpi->ext_refresh_frame_flags_pending = 1;
}

int av1_copy_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *const cm = &cpi->common;
  YV12_BUFFER_CONFIG *cfg = get_ref_frame(cm, idx);
  if (cfg) {
    aom_yv12_copy_frame(cfg, sd);
    return 0;
  } else {
    return -1;
  }
}

int av1_set_reference_enc(AV1_COMP *cpi, int idx, YV12_BUFFER_CONFIG *sd) {
  AV1_COMMON *const cm = &cpi->common;
  YV12_BUFFER_CONFIG *cfg = get_ref_frame(cm, idx);
  if (cfg) {
    aom_yv12_copy_frame(sd, cfg);
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

#if CONFIG_EXT_REFS && !CONFIG_XIPHRC
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
    cpi->existing_fb_idx_to_show = cpi->bwd_fb_idx;
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
                                         ? cpi->alt_fb_idx
                                         : cpi->bwd_fb_idx;
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
    cpi->existing_fb_idx_to_show = cpi->lst_fb_idxes[0];
  } else if (cpi->is_arf_filter_off[which_arf] &&
             (next_frame_update_type == OVERLAY_UPDATE ||
              next_frame_update_type == INTNL_OVERLAY_UPDATE)) {
    // Other parameters related to OVERLAY_UPDATE will be taken care of
    // in av1_rc_get_second_pass_params(cpi)
    cm->show_existing_frame = 1;
    cpi->rc.is_src_frame_alt_ref = 1;
    cpi->existing_fb_idx_to_show = (next_frame_update_type == OVERLAY_UPDATE)
                                       ? cpi->alt_fb_idx
                                       : cpi->alt2_fb_idx;
    cpi->is_arf_filter_off[which_arf] = 0;
  }
  cpi->rc.is_src_frame_ext_arf = 0;
}
#endif  // CONFIG_EXT_REFS && !CONFIG_XIPHRC

#ifdef OUTPUT_YUV_REC
void aom_write_one_yuv_frame(AV1_COMMON *cm, YV12_BUFFER_CONFIG *s) {
  uint8_t *src = s->y_buffer;
  int h = cm->height;
  if (yuv_rec_file == NULL) return;
#if CONFIG_HIGHBITDEPTH
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
#endif  // CONFIG_HIGHBITDEPTH

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

#if CONFIG_GLOBAL_MOTION
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
      recode |= (rdc->global_motion_used[i] > 0);
    }
  }
  return recode;
}
#endif  // CONFIG_GLOBAL_MOTION

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

#if CONFIG_EXT_REFS
// This function is used to shift the virtual indices of last reference frames
// as follows:
// LAST_FRAME -> LAST2_FRAME -> LAST3_FRAME
// when the LAST_FRAME is updated.
static INLINE void shift_last_ref_frames(AV1_COMP *cpi) {
  int ref_frame;
  for (ref_frame = LAST_REF_FRAMES - 1; ref_frame > 0; --ref_frame) {
    cpi->lst_fb_idxes[ref_frame] = cpi->lst_fb_idxes[ref_frame - 1];

    // [0] is allocated to the current coded frame. The statistics for the
    // reference frames start at [LAST_FRAME], i.e. [1].
    if (!cpi->rc.is_src_frame_alt_ref) {
      memcpy(cpi->interp_filter_selected[ref_frame + LAST_FRAME],
             cpi->interp_filter_selected[ref_frame - 1 + LAST_FRAME],
             sizeof(cpi->interp_filter_selected[ref_frame - 1 + LAST_FRAME]));
    }
  }
}
#endif  // CONFIG_EXT_REFS

#if CONFIG_VAR_REFS
static void enc_check_valid_ref_frames(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;

  // TODO(zoeliu): To handle ALTREF_FRAME the same way as do with other
  //               reference frames. Current encoder invalid ALTREF when ALTREF
  //               is the same as LAST, but invalid all the other references
  //               when they are the same as ALTREF.
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    int ref_buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - LAST_FRAME];

    if (ref_buf_idx != INVALID_IDX) {
      ref_buf->is_valid = 1;

      MV_REFERENCE_FRAME ref;
      for (ref = LAST_FRAME; ref < ref_frame; ++ref) {
        int buf_idx = get_ref_frame_buf_idx(cpi, ref);
        RefBuffer *const buf = &cm->frame_refs[ref - LAST_FRAME];
        if (buf->is_valid && buf_idx == ref_buf_idx) {
          if (ref_frame != ALTREF_FRAME || ref == LAST_FRAME) {
            ref_buf->is_valid = 0;
            break;
          } else {
            buf->is_valid = 0;
          }
        }
      }
    } else {
      ref_buf->is_valid = 0;
    }
  }
}
#endif  // CONFIG_VAR_REFS

#if CONFIG_EXT_REFS
#if USE_GF16_MULTI_LAYER
static void update_reference_frames_gf16(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  BufferPool *const pool = cm->buffer_pool;

  if (cm->frame_type == KEY_FRAME) {
    for (int ref_frame = 0; ref_frame < LAST_REF_FRAMES; ++ref_frame) {
      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->lst_fb_idxes[ref_frame]],
                 cm->new_fb_idx);
    }
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->gld_fb_idx],
               cm->new_fb_idx);
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->bwd_fb_idx],
               cm->new_fb_idx);
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt2_fb_idx],
               cm->new_fb_idx);
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt_fb_idx],
               cm->new_fb_idx);
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
#endif  // CONFIG_EXT_REFS

static void update_reference_frames(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  // NOTE: Save the new show frame buffer index for --test-code=warn, i.e.,
  //       for the purpose to verify no mismatch between encoder and decoder.
  if (cm->show_frame) cpi->last_show_frame_buf_idx = cm->new_fb_idx;

#if CONFIG_EXT_REFS
#if USE_GF16_MULTI_LAYER
  if (cpi->rc.baseline_gf_interval == 16) {
    update_reference_frames_gf16(cpi);
    return;
  }
#endif  // USE_GF16_MULTI_LAYER
#endif  // CONFIG_EXT_REFS

  BufferPool *const pool = cm->buffer_pool;
  // At this point the new frame has been encoded.
  // If any buffer copy / swapping is signaled it should be done here.
  if (cm->frame_type == KEY_FRAME) {
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->gld_fb_idx],
               cm->new_fb_idx);
#if CONFIG_EXT_REFS
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->bwd_fb_idx],
               cm->new_fb_idx);
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt2_fb_idx],
               cm->new_fb_idx);
#endif  // CONFIG_EXT_REFS
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt_fb_idx],
               cm->new_fb_idx);
  } else if (av1_preserve_existing_gf(cpi)) {
    // We have decided to preserve the previously existing golden frame as our
    // new ARF frame. However, in the short term in function
    // av1_bitstream.c::get_refresh_mask() we left it in the GF slot and, if
    // we're updating the GF with the current decoded frame, we save it to the
    // ARF slot instead.
    // We now have to update the ARF with the current frame and swap gld_fb_idx
    // and alt_fb_idx so that, overall, we've stored the old GF in the new ARF
    // slot and, if we're updating the GF, the current frame becomes the new GF.
    int tmp;

    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt_fb_idx],
               cm->new_fb_idx);
    tmp = cpi->alt_fb_idx;
    cpi->alt_fb_idx = cpi->gld_fb_idx;
    cpi->gld_fb_idx = tmp;

#if CONFIG_EXT_REFS
    // We need to modify the mapping accordingly
    cpi->arf_map[0] = cpi->alt_fb_idx;
#endif  // CONFIG_EXT_REFS
// TODO(zoeliu): Do we need to copy cpi->interp_filter_selected[0] over to
// cpi->interp_filter_selected[GOLDEN_FRAME]?
#if CONFIG_EXT_REFS
  } else if (cpi->rc.is_src_frame_ext_arf && cm->show_existing_frame) {
    // Deal with the special case for showing existing internal ALTREF_FRAME
    // Refresh the LAST_FRAME with the ALTREF_FRAME and retire the LAST3_FRAME
    // by updating the virtual indices.
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    const int which_arf = gf_group->arf_ref_idx[gf_group->index];
    assert(gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE);

    const int tmp = cpi->lst_fb_idxes[LAST_REF_FRAMES - 1];
    shift_last_ref_frames(cpi);

    cpi->lst_fb_idxes[0] = cpi->alt2_fb_idx;
    cpi->alt2_fb_idx = tmp;
    // We need to modify the mapping accordingly
    cpi->arf_map[which_arf] = cpi->alt2_fb_idx;

    memcpy(cpi->interp_filter_selected[LAST_FRAME],
           cpi->interp_filter_selected[ALTREF2_FRAME],
           sizeof(cpi->interp_filter_selected[ALTREF2_FRAME]));
#endif     // CONFIG_EXT_REFS
  } else { /* For non key/golden frames */
    // === ALTREF_FRAME ===
    if (cpi->refresh_alt_ref_frame) {
      int arf_idx = cpi->alt_fb_idx;
      int which_arf = 0;
#if !CONFIG_EXT_REFS
      if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
        const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
        arf_idx = gf_group->arf_update_idx[gf_group->index];
      }
#endif  // !CONFIG_EXT_REFS
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[arf_idx], cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[ALTREF_FRAME + which_arf],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }

    // === GOLDEN_FRAME ===
    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->gld_fb_idx],
                 cm->new_fb_idx);

#if !CONFIG_EXT_REFS
      if (!cpi->rc.is_src_frame_alt_ref)
#endif  // !CONFIG_EXT_REFS
        memcpy(cpi->interp_filter_selected[GOLDEN_FRAME],
               cpi->interp_filter_selected[0],
               sizeof(cpi->interp_filter_selected[0]));
    }

#if CONFIG_EXT_REFS
    // === BWDREF_FRAME ===
    if (cpi->refresh_bwd_ref_frame) {
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->bwd_fb_idx],
                 cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[BWDREF_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }

    // === ALTREF2_FRAME ===
    if (cpi->refresh_alt2_ref_frame) {
      ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->alt2_fb_idx],
                 cm->new_fb_idx);

      memcpy(cpi->interp_filter_selected[ALTREF2_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }
#endif  // CONFIG_EXT_REFS
  }

  if (cpi->refresh_last_frame) {
#if CONFIG_EXT_REFS
    // NOTE(zoeliu): We have two layers of mapping (1) from the per-frame
    // reference to the reference frame buffer virtual index; and then (2) from
    // the virtual index to the reference frame buffer physical index:
    //
    // LAST_FRAME,      ..., LAST3_FRAME,     ..., ALTREF_FRAME
    //      |                     |                     |
    //      v                     v                     v
    // lst_fb_idxes[0], ..., lst_fb_idxes[2], ..., alt_fb_idx
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
    // lst_fb_idxes[2], lst_fb_idxes[0], lst_fb_idxes[1]
    int ref_frame;

    if (cm->frame_type == KEY_FRAME) {
      for (ref_frame = 0; ref_frame < LAST_REF_FRAMES; ++ref_frame) {
        ref_cnt_fb(pool->frame_bufs,
                   &cm->ref_frame_map[cpi->lst_fb_idxes[ref_frame]],
                   cm->new_fb_idx);
      }
    } else {
      int tmp;

      ref_cnt_fb(pool->frame_bufs,
                 &cm->ref_frame_map[cpi->lst_fb_idxes[LAST_REF_FRAMES - 1]],
                 cm->new_fb_idx);

      tmp = cpi->lst_fb_idxes[LAST_REF_FRAMES - 1];

      shift_last_ref_frames(cpi);
      cpi->lst_fb_idxes[0] = tmp;

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
        tmp = cpi->lst_fb_idxes[LAST_REF_FRAMES - 1];

        shift_last_ref_frames(cpi);
        cpi->lst_fb_idxes[0] = cpi->bwd_fb_idx;
        cpi->bwd_fb_idx = tmp;

        memcpy(cpi->interp_filter_selected[LAST_FRAME],
               cpi->interp_filter_selected[BWDREF_FRAME],
               sizeof(cpi->interp_filter_selected[BWDREF_FRAME]));
      }
    }
#else   // !CONFIG_EXT_REFS
    ref_cnt_fb(pool->frame_bufs, &cm->ref_frame_map[cpi->lst_fb_idx],
               cm->new_fb_idx);
    if (!cpi->rc.is_src_frame_alt_ref) {
      memcpy(cpi->interp_filter_selected[LAST_FRAME],
             cpi->interp_filter_selected[0],
             sizeof(cpi->interp_filter_selected[0]));
    }
#endif  // CONFIG_EXT_REFS
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
  MV_REFERENCE_FRAME ref_frame;
  const AOM_REFFRAME ref_mask[INTER_REFS_PER_FRAME] = {
    AOM_LAST_FLAG,
#if CONFIG_EXT_REFS
    AOM_LAST2_FLAG,
    AOM_LAST3_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_GOLD_FLAG,
#if CONFIG_EXT_REFS
    AOM_BWD_FLAG,
    AOM_ALT2_FLAG,
#endif  // CONFIG_EXT_REFS
    AOM_ALT_FLAG
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

#if CONFIG_HIGHBITDEPTH
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
          av1_resize_and_extend_frame(ref, &new_fb_ptr->buf,
                                      (int)cm->bit_depth);
          cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
          alloc_frame_mvs(cm, new_fb);
        }
#else
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
          if (aom_realloc_frame_buffer(&new_fb_ptr->buf, cm->width, cm->height,
                                       cm->subsampling_x, cm->subsampling_y,
                                       AOM_BORDER_IN_PIXELS, cm->byte_alignment,
                                       NULL, NULL, NULL))
            aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                               "Failed to allocate frame buffer");
          av1_resize_and_extend_frame(ref, &new_fb_ptr->buf);
          cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
          alloc_frame_mvs(cm, new_fb);
        }
#endif  // CONFIG_HIGHBITDEPTH
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
  if (cpi->oxcf.pass == 0) {
    // Only release scaled references under certain conditions:
    // if reference will be updated, or if scaled reference has same resolution.
    int refresh[INTER_REFS_PER_FRAME];
    refresh[0] = (cpi->refresh_last_frame) ? 1 : 0;
#if CONFIG_EXT_REFS
    refresh[1] = refresh[2] = 0;
    refresh[3] = (cpi->refresh_golden_frame) ? 1 : 0;
    refresh[4] = (cpi->refresh_bwd_ref_frame) ? 1 : 0;
    refresh[5] = (cpi->refresh_alt2_ref_frame) ? 1 : 0;
    refresh[6] = (cpi->refresh_alt_ref_frame) ? 1 : 0;
#else   // !CONFIG_EXT_REFS
    refresh[1] = (cpi->refresh_golden_frame) ? 1 : 0;
    refresh[2] = (cpi->refresh_alt_ref_frame) ? 1 : 0;
#endif  // CONFIG_EXT_REFS
    for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
      const int idx = cpi->scaled_ref_idx[i - 1];
      RefCntBuffer *const buf =
          idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[idx] : NULL;
      const YV12_BUFFER_CONFIG *const ref = get_ref_frame_buffer(cpi, i);
      if (buf != NULL &&
          (refresh[i - 1] || (buf->buf.y_crop_width == ref->y_crop_width &&
                              buf->buf.y_crop_height == ref->y_crop_height))) {
        --buf->ref_count;
        cpi->scaled_ref_idx[i - 1] = INVALID_IDX;
      }
    }
  } else {
    for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i) {
      const int idx = cpi->scaled_ref_idx[i];
      RefCntBuffer *const buf =
          idx != INVALID_IDX ? &cm->buffer_pool->frame_bufs[idx] : NULL;
      if (buf != NULL) {
        --buf->ref_count;
        cpi->scaled_ref_idx[i] = INVALID_IDX;
      }
    }
  }
}

#if 0 && CONFIG_INTERNAL_STATS
static void output_frame_level_debug_stats(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  FILE *const f = fopen("tmp.stt", cm->current_video_frame ? "a" : "w");
  int64_t recon_err;

  aom_clear_system_state();

  recon_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));

  if (cpi->twopass.total_left_stats.coded_error != 0.0)
    fprintf(f, "%10u %dx%d %d %d %10d %10d %10d %10d"
       "%10"PRId64" %10"PRId64" %5d %5d %10"PRId64" "
       "%10"PRId64" %10"PRId64" %10d "
       "%7.2lf %7.2lf %7.2lf %7.2lf %7.2lf"
        "%6d %6d %5d %5d %5d "
        "%10"PRId64" %10.3lf"
        "%10lf %8u %10"PRId64" %10d %10d %10d\n",
        cpi->common.current_video_frame,
        cm->width, cm->height,
        cpi->rc.source_alt_ref_pending,
        cpi->rc.source_alt_ref_active,
        cpi->rc.this_frame_target,
        cpi->rc.projected_frame_size,
        cpi->rc.projected_frame_size / cpi->common.MBs,
        (cpi->rc.projected_frame_size - cpi->rc.this_frame_target),
        cpi->rc.vbr_bits_off_target,
        cpi->rc.vbr_bits_off_target_fast,
        cpi->twopass.extend_minq,
        cpi->twopass.extend_minq_fast,
        cpi->rc.total_target_vs_actual,
        (cpi->rc.starting_buffer_level - cpi->rc.bits_off_target),
        cpi->rc.total_actual_bits, cm->base_qindex,
        av1_convert_qindex_to_q(cm->base_qindex, cm->bit_depth),
        (double)av1_dc_quant(cm->base_qindex, 0, cm->bit_depth) / 4.0,
        av1_convert_qindex_to_q(cpi->twopass.active_worst_quality,
                                cm->bit_depth),
        cpi->rc.avg_q,
        av1_convert_qindex_to_q(cpi->oxcf.cq_level, cm->bit_depth),
        cpi->refresh_last_frame, cpi->refresh_golden_frame,
        cpi->refresh_alt_ref_frame, cm->frame_type, cpi->rc.gfu_boost,
        cpi->twopass.bits_left,
        cpi->twopass.total_left_stats.coded_error,
        cpi->twopass.bits_left /
            (1 + cpi->twopass.total_left_stats.coded_error),
        cpi->tot_recode_hits, recon_err, cpi->rc.kf_boost,
        cpi->twopass.kf_zeromotion_pct,
        cpi->twopass.fr_content_type);

  fclose(f);

  if (0) {
    FILE *const fmodes = fopen("Modes.stt", "a");
    int i;

    fprintf(fmodes, "%6d:%1d:%1d:%1d ", cpi->common.current_video_frame,
            cm->frame_type, cpi->refresh_golden_frame,
            cpi->refresh_alt_ref_frame);

    for (i = 0; i < MAX_MODES; ++i)
      fprintf(fmodes, "%5d ", cpi->mode_chosen_counts[i]);

    fprintf(fmodes, "\n");

    fclose(fmodes);
  }
}
#endif

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
#if CONFIG_GLOBAL_MOTION
  int i;
  for (i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
    cpi->common.global_motion[i] = default_warp_params;
  }
  cpi->global_motion_search_done = 0;
#endif  // CONFIG_GLOBAL_MOTION
  av1_set_speed_features_framesize_independent(cpi);
  av1_set_rd_speed_thresholds(cpi);
  av1_set_rd_speed_thresholds_sub8x8(cpi);
  cpi->common.interp_filter = cpi->sf.default_interp_filter;
  if (!frame_is_intra_only(&cpi->common)) set_compound_tools(&cpi->common);
}

static void set_size_dependent_vars(AV1_COMP *cpi, int *q, int *bottom_index,
                                    int *top_index) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  // Setup variables that depend on the dimensions of the frame.
  av1_set_speed_features_framesize_dependent(cpi);

// Decide q and q bounds.
#if CONFIG_XIPHRC
  int frame_type = cm->frame_type == KEY_FRAME ? OD_I_FRAME : OD_P_FRAME;
  *q = od_enc_rc_select_quantizers_and_lambdas(
      &cpi->od_rc, cpi->refresh_golden_frame, cpi->refresh_alt_ref_frame,
      frame_type, bottom_index, top_index);
#else
  *q = av1_rc_pick_q_and_bounds(cpi, cm->width, cm->height, bottom_index,
                                top_index);
#endif

  if (!frame_is_intra_only(cm)) {
#if CONFIG_AMVR
    set_high_precision_mv(cpi, (*q) < HIGH_PRECISION_MV_QTHRESH,
                          cpi->common.cur_frame_mv_precision_level);
#else
    set_high_precision_mv(cpi, (*q) < HIGH_PRECISION_MV_QTHRESH);
#endif
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

#if CONFIG_LOOP_RESTORATION
#define COUPLED_CHROMA_FROM_LUMA_RESTORATION 0
static void set_restoration_tilesize(int width, int height, int sx, int sy,
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

  rst[0].restoration_tilesize = (RESTORATION_TILESIZE_MAX >> 1);
  rst[1].restoration_tilesize = rst[0].restoration_tilesize >> s;
  rst[2].restoration_tilesize = rst[1].restoration_tilesize;

  rst[0].procunit_width = rst[0].procunit_height = RESTORATION_PROC_UNIT_SIZE;
  rst[1].procunit_width = rst[2].procunit_width =
      RESTORATION_PROC_UNIT_SIZE >> sx;
  rst[1].procunit_height = rst[2].procunit_height =
      RESTORATION_PROC_UNIT_SIZE >> sy;
}
#endif  // CONFIG_LOOP_RESTORATION

static void init_ref_frame_bufs(AV1_COMMON *cm) {
  int i;
  BufferPool *const pool = cm->buffer_pool;
  cm->new_fb_idx = INVALID_IDX;
  for (i = 0; i < REF_FRAMES; ++i) {
    cm->ref_frame_map[i] = INVALID_IDX;
    pool->frame_bufs[i].ref_count = 0;
  }
#if CONFIG_HASH_ME
  for (i = 0; i < FRAME_BUFFERS; ++i) {
    av1_hash_table_init(&pool->frame_bufs[i].hash_table);
  }
#endif
}

static void check_initial_width(AV1_COMP *cpi,
#if CONFIG_HIGHBITDEPTH
                                int use_highbitdepth,
#endif
                                int subsampling_x, int subsampling_y) {
  AV1_COMMON *const cm = &cpi->common;

  if (!cpi->initial_width ||
#if CONFIG_HIGHBITDEPTH
      cm->use_highbitdepth != use_highbitdepth ||
#endif
      cm->subsampling_x != subsampling_x ||
      cm->subsampling_y != subsampling_y) {
    cm->subsampling_x = subsampling_x;
    cm->subsampling_y = subsampling_y;
#if CONFIG_HIGHBITDEPTH
    cm->use_highbitdepth = use_highbitdepth;
#endif

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
#if CONFIG_HIGHBITDEPTH
  check_initial_width(cpi, cm->use_highbitdepth, cm->subsampling_x,
                      cm->subsampling_y);
#else
  check_initial_width(cpi, cm->subsampling_x, cm->subsampling_y);
#endif  // CONFIG_HIGHBITDEPTH

  if (width <= 0 || height <= 0) return 1;

  cm->width = width;
  cm->height = height;

  if (cpi->initial_width && cpi->initial_height &&
      (cm->width > cpi->initial_width || cm->height > cpi->initial_height)) {
    av1_free_context_buffers(cm);
    av1_free_pc_tree(&cpi->td);
    alloc_compressor_data(cpi);
    realloc_segmentation_maps(cpi);
    cpi->initial_width = cpi->initial_height = 0;
  }
  update_frame_size(cpi);

  return 0;
}

static void set_frame_size(AV1_COMP *cpi, int width, int height) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int ref_frame;

  if (width != cm->width || height != cm->height) {
    // There has been a change in the encoded frame size
    set_size_literal(cpi, width, height);
    set_mv_search_params(cpi);
  }

#if !CONFIG_XIPHRC
  if (cpi->oxcf.pass == 2) {
    av1_set_target_rate(cpi, cm->width, cm->height);
  }
#endif

  alloc_frame_mvs(cm, cm->new_fb_idx);

  // Reset the frame pointers to the current frame size.
  if (aom_realloc_frame_buffer(get_frame_new_buffer(cm), cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                               cm->use_highbitdepth,
#endif
                               AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                               NULL, NULL))
    aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");

#if CONFIG_LOOP_RESTORATION
  set_restoration_tilesize(
#if CONFIG_FRAME_SUPERRES
      cm->superres_upscaled_width, cm->superres_upscaled_height,
#else
      cm->width, cm->height,
#endif  // CONFIG_FRAME_SUPERRES
      cm->subsampling_x, cm->subsampling_y, cm->rst_info);
  for (int i = 0; i < MAX_MB_PLANE; ++i)
    cm->rst_info[i].frame_restoration_type = RESTORE_NONE;
  av1_alloc_restoration_buffers(cm);
  for (int i = 0; i < MAX_MB_PLANE; ++i) {
    cpi->rst_search[i].restoration_tilesize =
        cm->rst_info[i].restoration_tilesize;
    cpi->rst_search[i].procunit_width = cm->rst_info[i].procunit_width;
    cpi->rst_search[i].procunit_height = cm->rst_info[i].procunit_height;
    av1_alloc_restoration_struct(cm, &cpi->rst_search[i],
#if CONFIG_FRAME_SUPERRES
                                 cm->superres_upscaled_width,
                                 cm->superres_upscaled_height);
#else
                                 cm->width, cm->height);
#endif  // CONFIG_FRAME_SUPERRES
  }
#endif                            // CONFIG_LOOP_RESTORATION
  alloc_util_frame_buffers(cpi);  // TODO(afergs): Remove? Gets called anyways.
  init_motion_estimation(cpi);

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - LAST_FRAME];
    const int buf_idx = get_ref_frame_buf_idx(cpi, ref_frame);

    ref_buf->idx = buf_idx;

    if (buf_idx != INVALID_IDX) {
      YV12_BUFFER_CONFIG *const buf = &cm->buffer_pool->frame_bufs[buf_idx].buf;
      ref_buf->buf = buf;
#if CONFIG_HIGHBITDEPTH
      av1_setup_scale_factors_for_frame(
          &ref_buf->sf, buf->y_crop_width, buf->y_crop_height, cm->width,
          cm->height, (buf->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0);
#else
      av1_setup_scale_factors_for_frame(&ref_buf->sf, buf->y_crop_width,
                                        buf->y_crop_height, cm->width,
                                        cm->height);
#endif  // CONFIG_HIGHBITDEPTH
      if (av1_is_scaled(&ref_buf->sf)) aom_extend_frame_borders(buf);
    } else {
      ref_buf->buf = NULL;
    }
  }

#if CONFIG_VAR_REFS
  // Check duplicate reference frames
  enc_check_valid_ref_frames(cpi);
#endif  // CONFIG_VAR_REFS

#if CONFIG_INTRABC
#if CONFIG_HIGHBITDEPTH
  av1_setup_scale_factors_for_frame(&xd->sf_identity, cm->width, cm->height,
                                    cm->width, cm->height,
                                    cm->use_highbitdepth);
#else
  av1_setup_scale_factors_for_frame(&xd->sf_identity, cm->width, cm->height,
                                    cm->width, cm->height);
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_INTRABC

  set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);
}

static uint8_t calculate_next_resize_scale(const AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 56789;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->pass == 1) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;

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

#if CONFIG_FRAME_SUPERRES

static uint8_t calculate_next_superres_scale(AV1_COMP *cpi) {
  // Choose an arbitrary random number
  static unsigned int seed = 34567;
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  if (oxcf->pass == 1) return SCALE_NUMERATOR;
  uint8_t new_denom = SCALE_NUMERATOR;
  int bottom_index, top_index, q, qthresh;

  switch (oxcf->superres_mode) {
    case SUPERRES_NONE: new_denom = SCALE_NUMERATOR; break;
    case SUPERRES_FIXED:
      if (cpi->common.frame_type == KEY_FRAME)
        new_denom = oxcf->superres_kf_scale_denominator;
      else
        new_denom = oxcf->superres_scale_denominator;
      break;
    case SUPERRES_RANDOM: new_denom = lcg_rand16(&seed) % 9 + 8; break;
    case SUPERRES_QTHRESH:
      qthresh = (cpi->common.frame_type == KEY_FRAME ? oxcf->superres_kf_qthresh
                                                     : oxcf->superres_qthresh);
      av1_set_target_rate(cpi, cpi->oxcf.width, cpi->oxcf.height);
      q = av1_rc_pick_q_and_bounds(cpi, cpi->oxcf.width, cpi->oxcf.height,
                                   &bottom_index, &top_index);
      if (q < qthresh) {
        new_denom = SCALE_NUMERATOR;
      } else {
        new_denom = SCALE_NUMERATOR + 1 + ((q - qthresh) >> 3);
        new_denom = AOMMIN(SCALE_NUMERATOR << 1, new_denom);
        // printf("SUPERRES: q %d, qthresh %d: denom %d\n", q, qthresh,
        // new_denom);
      }
      break;
    default: assert(0);
  }
  return new_denom;
}

static int dimension_is_ok(int orig_dim, int resized_dim, int denom) {
  return (resized_dim * SCALE_NUMERATOR >= orig_dim * denom / 2);
}

// TODO(now): Fix?
static int dimensions_are_ok(int owidth, int oheight, size_params_type *rsz) {
  return dimension_is_ok(owidth, rsz->resize_width, rsz->superres_denom) &&
         (CONFIG_HORZONLY_FRAME_SUPERRES ||
          dimension_is_ok(oheight, rsz->resize_height, rsz->superres_denom));
}

#define DIVIDE_AND_ROUND(x, y) (((x) + ((y) >> 1)) / (y))

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
  } else {  // We are allowed to alter neither resize scale nor superres scale.
    return 0;
  }
  return dimensions_are_ok(owidth, oheight, rsz);
}
#undef DIVIDE_AND_ROUND
#endif  // CONFIG_FRAME_SUPERRES

// Calculates resize and superres params for next frame
size_params_type av1_calculate_next_size_params(AV1_COMP *cpi) {
  const AV1EncoderConfig *oxcf = &cpi->oxcf;
  size_params_type rsz = {
    oxcf->width,
    oxcf->height,
#if CONFIG_FRAME_SUPERRES
    SCALE_NUMERATOR
#endif  // CONFIG_FRAME_SUPERRES
  };
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
#if CONFIG_FRAME_SUPERRES
  rsz.superres_denom = calculate_next_superres_scale(cpi);
  if (!validate_size_scales(oxcf->resize_mode, oxcf->superres_mode, oxcf->width,
                            oxcf->height, &rsz))
    assert(0 && "Invalid scale parameters");
#endif  // CONFIG_FRAME_SUPERRES
  return rsz;
}

static void setup_frame_size_from_params(AV1_COMP *cpi, size_params_type *rsz) {
  int encode_width = rsz->resize_width;
  int encode_height = rsz->resize_height;

#if CONFIG_FRAME_SUPERRES
  AV1_COMMON *cm = &cpi->common;
  cm->superres_upscaled_width = encode_width;
  cm->superres_upscaled_height = encode_height;
  cm->superres_scale_denominator = rsz->superres_denom;
  av1_calculate_scaled_superres_size(&encode_width, &encode_height,
                                     rsz->superres_denom);
#endif  // CONFIG_FRAME_SUPERRES
  set_frame_size(cpi, encode_width, encode_height);
}

static void setup_frame_size(AV1_COMP *cpi) {
  size_params_type rsz = av1_calculate_next_size_params(cpi);
  setup_frame_size_from_params(cpi, &rsz);
}

#if CONFIG_FRAME_SUPERRES
static void superres_post_encode(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  if (av1_superres_unscaled(cm)) return;

  av1_superres_upscale(cm, NULL);

  // If regular resizing is occurring the source will need to be downscaled to
  // match the upscaled superres resolution. Otherwise the original source is
  // used.
  if (av1_resize_unscaled(cm)) {
    cpi->source = cpi->unscaled_source;
    if (cpi->last_source != NULL) cpi->last_source = cpi->unscaled_last_source;
  } else {
    assert(cpi->unscaled_source->y_crop_width != cm->superres_upscaled_width);
    assert(cpi->unscaled_source->y_crop_height != cm->superres_upscaled_height);
    // Do downscale. cm->(width|height) has been updated by av1_superres_upscale
    if (aom_realloc_frame_buffer(
            &cpi->scaled_source, cm->superres_upscaled_width,
            cm->superres_upscaled_height, cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
            cm->use_highbitdepth,
#endif  // CONFIG_HIGHBITDEPTH
            AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL, NULL, NULL))
      aom_internal_error(
          &cm->error, AOM_CODEC_MEM_ERROR,
          "Failed to reallocate scaled source buffer for superres");
    assert(cpi->scaled_source.y_crop_width == cm->superres_upscaled_width);
    assert(cpi->scaled_source.y_crop_height == cm->superres_upscaled_height);
#if CONFIG_HIGHBITDEPTH
    av1_resize_and_extend_frame(cpi->unscaled_source, &cpi->scaled_source,
                                (int)cm->bit_depth);
#else
    av1_resize_and_extend_frame(cpi->unscaled_source, &cpi->scaled_source);
#endif  // CONFIG_HIGHBITDEPTH
    cpi->source = &cpi->scaled_source;
  }
}
#endif  // CONFIG_FRAME_SUPERRES

static void loopfilter_frame(AV1_COMP *cpi, AV1_COMMON *cm) {
  MACROBLOCKD *xd = &cpi->td.mb.e_mbd;
  struct loopfilter *lf = &cm->lf;
  int no_loopfilter = 0;

  if (is_lossless_requested(&cpi->oxcf)) no_loopfilter = 1;

#if CONFIG_EXT_TILE
  // 0 loopfilter level is only necessary if individual tile
  // decoding is required.
  if (cm->single_tile_decoding) no_loopfilter = 1;
#endif  // CONFIG_EXT_TILE

  if (no_loopfilter) {
#if CONFIG_LOOPFILTER_LEVEL
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
#else
    lf->filter_level = 0;
#endif
  } else {
    struct aom_usec_timer timer;

    aom_clear_system_state();

    aom_usec_timer_start(&timer);

    av1_pick_filter_level(cpi->source, cpi, cpi->sf.lpf_pick);

    aom_usec_timer_mark(&timer);
    cpi->time_pick_lpf += aom_usec_timer_elapsed(&timer);
  }

#if !CONFIG_LPF_SB
#if CONFIG_LOOPFILTER_LEVEL
  if (lf->filter_level[0] || lf->filter_level[1])
#else
  if (lf->filter_level > 0)
#endif
#endif  // CONFIG_LPF_SB
  {
#if CONFIG_VAR_TX || CONFIG_EXT_PARTITION || CONFIG_CB4X4
#if CONFIG_LPF_SB
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level, 0, 0, 0,
                          0);
#else
#if CONFIG_LOOPFILTER_LEVEL
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level[0],
                          lf->filter_level[1], 0, 0);
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level_u,
                          lf->filter_level_u, 1, 0);
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level_v,
                          lf->filter_level_v, 2, 0);

#else
    av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level, 0, 0);
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif  // CONFIG_LPF_SB
#else
    if (cpi->num_workers > 1)
      av1_loop_filter_frame_mt(cm->frame_to_show, cm, xd->plane,
                               lf->filter_level, 0, 0, cpi->workers,
                               cpi->num_workers, &cpi->lf_row_sync);
    else
      av1_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level, 0, 0);
#endif
  }

#if CONFIG_STRIPED_LOOP_RESTORATION
  av1_loop_restoration_save_boundary_lines(cm->frame_to_show, cm);
#endif

#if CONFIG_CDEF
  if (is_lossless_requested(&cpi->oxcf)) {
    cm->cdef_bits = 0;
    cm->cdef_strengths[0] = 0;
    cm->nb_cdef_strengths = 1;
  } else {
    // Find CDEF parameters
    av1_cdef_search(cm->frame_to_show, cpi->source, cm, xd,
                    cpi->oxcf.speed > 0);

    // Apply the filter
    av1_cdef_frame(cm->frame_to_show, cm, xd);
  }
#endif

#if CONFIG_FRAME_SUPERRES
  superres_post_encode(cpi);
#endif  // CONFIG_FRAME_SUPERRES

#if CONFIG_LOOP_RESTORATION
  aom_extend_frame_borders(cm->frame_to_show);
  av1_pick_filter_restoration(cpi->source, cpi, cpi->sf.lpf_pick);
  if (cm->rst_info[0].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[1].frame_restoration_type != RESTORE_NONE ||
      cm->rst_info[2].frame_restoration_type != RESTORE_NONE) {
    av1_loop_restoration_frame(cm->frame_to_show, cm, cm->rst_info, 7, 0, NULL);
  }
#endif  // CONFIG_LOOP_RESTORATION
  // TODO(debargha): Fix mv search range on encoder side
  // aom_extend_frame_inner_borders(cm->frame_to_show);
  aom_extend_frame_borders(cm->frame_to_show);
}

static void encode_without_recode_loop(AV1_COMP *cpi) {
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
#if CONFIG_HIGHBITDEPTH && CONFIG_GLOBAL_MOTION
  cpi->source->buf_8bit_valid = 0;
#endif

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
}

static void encode_with_recode_loop(AV1_COMP *cpi, size_t *size,
                                    uint8_t *dest) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int bottom_index, top_index;
  int loop_count = 0;
  int loop_at_this_size = 0;
  int loop = 0;
#if !CONFIG_XIPHRC
  int overshoot_seen = 0;
  int undershoot_seen = 0;
#endif
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;
  int q = 0, q_low = 0, q_high = 0;

  set_size_independent_vars(cpi);

#if CONFIG_HIGHBITDEPTH && CONFIG_GLOBAL_MOTION
  cpi->source->buf_8bit_valid = 0;
#endif

  aom_clear_system_state();
  setup_frame_size(cpi);
  set_size_dependent_vars(cpi, &q, &bottom_index, &top_index);

  do {
    aom_clear_system_state();

    if (loop_count == 0) {
      // TODO(agrange) Scale cpi->max_mv_magnitude if frame-size has changed.
      set_mv_search_params(cpi);

#if !CONFIG_XIPHRC
      // Reset the loop state for new frame size.
      overshoot_seen = 0;
      undershoot_seen = 0;
#endif

      q_low = bottom_index;
      q_high = top_index;

      loop_at_this_size = 0;
    }

    // Decide frame size bounds first time through.
    if (loop_count == 0) {
      av1_rc_compute_frame_size_bounds(cpi, rc->this_frame_target,
                                       &frame_under_shoot_limit,
                                       &frame_over_shoot_limit);
    }

#if CONFIG_GLOBAL_MOTION
    // if frame was scaled calculate global_motion_search again if already done
    if (loop_count > 0 && cpi->source && cpi->global_motion_search_done)
      if (cpi->source->y_crop_width != cm->width ||
          cpi->source->y_crop_height != cm->height)
        cpi->global_motion_search_done = 0;
#endif  // CONFIG_GLOBAL_MOTION
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

    if (loop_count == 0) setup_frame(cpi);

#if CONFIG_Q_ADAPT_PROBS
    // Base q-index may have changed, so we need to assign proper default coef
    // probs before every iteration.
    if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
      int i;
      av1_default_coef_probs(cm);
      if (cm->frame_type == KEY_FRAME || cm->error_resilient_mode ||
          cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL) {
        for (i = 0; i < FRAME_CONTEXTS; ++i) cm->frame_contexts[i] = *cm->fc;
      } else if (cm->reset_frame_context == RESET_FRAME_CONTEXT_CURRENT) {
#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
        if (cm->frame_refs[0].idx >= 0) {
          cm->frame_contexts[cm->frame_refs[0].idx] = *cm->fc;
        }
#else
        cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
#endif
      }
    }
#endif  // CONFIG_Q_ADAPT_PROBS

    // Variance adaptive and in frame q adjustment experiments are mutually
    // exclusive.
    if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
      av1_vaq_frame_setup(cpi);
    } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      av1_setup_in_frame_q_adj(cpi);
    }

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
      av1_pack_bitstream(cpi, dest, size);

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

#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          kf_err = aom_highbd_get_y_sse(cpi->source, get_frame_new_buffer(cm));
        } else {
          kf_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
        }
#else
        kf_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
#endif  // CONFIG_HIGHBITDEPTH

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
#if !CONFIG_XIPHRC
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
#endif

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

#if CONFIG_GLOBAL_MOTION
    if (recode_loop_test_global_motion(cpi)) {
      loop = 1;
    }
#endif  // CONFIG_GLOBAL_MOTION

    if (loop) {
      ++loop_count;
      ++loop_at_this_size;

#if CONFIG_INTERNAL_STATS
      ++cpi->tot_recode_hits;
#endif
    }
  } while (loop);
}

static int get_ref_frame_flags(const AV1_COMP *cpi) {
  const int *const map = cpi->common.ref_frame_map;

#if CONFIG_EXT_REFS
  const int last2_is_last =
      map[cpi->lst_fb_idxes[1]] == map[cpi->lst_fb_idxes[0]];
  const int last3_is_last =
      map[cpi->lst_fb_idxes[2]] == map[cpi->lst_fb_idxes[0]];
  const int gld_is_last = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idxes[0]];
#if CONFIG_ONE_SIDED_COMPOUND && !CONFIG_EXT_COMP_REFS
  const int alt_is_last = map[cpi->alt_fb_idx] == map[cpi->lst_fb_idxes[0]];
  const int last3_is_last2 =
      map[cpi->lst_fb_idxes[2]] == map[cpi->lst_fb_idxes[1]];
  const int gld_is_last2 = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idxes[1]];
  const int gld_is_last3 = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idxes[2]];
#else   // !CONFIG_ONE_SIDED_COMPOUND || CONFIG_EXT_COMP_REFS
  const int bwd_is_last = map[cpi->bwd_fb_idx] == map[cpi->lst_fb_idxes[0]];
  const int alt_is_last = map[cpi->alt_fb_idx] == map[cpi->lst_fb_idxes[0]];

  const int last3_is_last2 =
      map[cpi->lst_fb_idxes[2]] == map[cpi->lst_fb_idxes[1]];
  const int gld_is_last2 = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idxes[1]];
  const int bwd_is_last2 = map[cpi->bwd_fb_idx] == map[cpi->lst_fb_idxes[1]];

  const int gld_is_last3 = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idxes[2]];
  const int bwd_is_last3 = map[cpi->bwd_fb_idx] == map[cpi->lst_fb_idxes[2]];

  const int bwd_is_gld = map[cpi->bwd_fb_idx] == map[cpi->gld_fb_idx];
#endif  // CONFIG_ONE_SIDED_COMPOUND && !CONFIG_EXT_COMP_REFS

  const int alt2_is_last = map[cpi->alt2_fb_idx] == map[cpi->lst_fb_idxes[0]];
  const int alt2_is_last2 = map[cpi->alt2_fb_idx] == map[cpi->lst_fb_idxes[1]];
  const int alt2_is_last3 = map[cpi->alt2_fb_idx] == map[cpi->lst_fb_idxes[2]];
  const int alt2_is_gld = map[cpi->alt2_fb_idx] == map[cpi->gld_fb_idx];
  const int alt2_is_bwd = map[cpi->alt2_fb_idx] == map[cpi->bwd_fb_idx];

  const int last2_is_alt = map[cpi->lst_fb_idxes[1]] == map[cpi->alt_fb_idx];
  const int last3_is_alt = map[cpi->lst_fb_idxes[2]] == map[cpi->alt_fb_idx];
  const int gld_is_alt = map[cpi->gld_fb_idx] == map[cpi->alt_fb_idx];
  const int bwd_is_alt = map[cpi->bwd_fb_idx] == map[cpi->alt_fb_idx];
  const int alt2_is_alt = map[cpi->alt2_fb_idx] == map[cpi->alt_fb_idx];
#else   // !CONFIG_EXT_REFS
  const int gld_is_last = map[cpi->gld_fb_idx] == map[cpi->lst_fb_idx];
  const int gld_is_alt = map[cpi->gld_fb_idx] == map[cpi->alt_fb_idx];
  const int alt_is_last = map[cpi->alt_fb_idx] == map[cpi->lst_fb_idx];
#endif  // CONFIG_EXT_REFS

  int flags = AOM_REFFRAME_ALL;

  if (gld_is_last || gld_is_alt) flags &= ~AOM_GOLD_FLAG;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX) flags &= ~AOM_GOLD_FLAG;

  if (alt_is_last) flags &= ~AOM_ALT_FLAG;

#if CONFIG_EXT_REFS
  if (last2_is_last || last2_is_alt) flags &= ~AOM_LAST2_FLAG;

  if (last3_is_last || last3_is_last2 || last3_is_alt) flags &= ~AOM_LAST3_FLAG;

  if (gld_is_last2 || gld_is_last3) flags &= ~AOM_GOLD_FLAG;

#if CONFIG_ONE_SIDED_COMPOUND && \
    !CONFIG_EXT_COMP_REFS  // Changes LL & HL bitstream
  /* Allow biprediction between two identical frames (e.g. bwd_is_last = 1) */
  if (bwd_is_alt && (flags & AOM_BWD_FLAG)) flags &= ~AOM_BWD_FLAG;
#else   // !CONFIG_ONE_SIDED_COMPOUND || CONFIG_EXT_COMP_REFS
  if ((bwd_is_last || bwd_is_last2 || bwd_is_last3 || bwd_is_gld ||
       bwd_is_alt) &&
      (flags & AOM_BWD_FLAG))
    flags &= ~AOM_BWD_FLAG;
#endif  // CONFIG_ONE_SIDED_COMPOUND && !CONFIG_EXT_COMP_REFS

  if ((alt2_is_last || alt2_is_last2 || alt2_is_last3 || alt2_is_gld ||
       alt2_is_bwd || alt2_is_alt) &&
      (flags & AOM_ALT2_FLAG))
    flags &= ~AOM_ALT2_FLAG;
#endif  // CONFIG_EXT_REFS

  return flags;
}

static void set_ext_overrides(AV1_COMP *cpi) {
  // Overrides the defaults with the externally supplied values with
  // av1_update_reference() and av1_update_entropy() calls
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

#if !CONFIG_FRAME_SIGN_BIAS
static void set_arf_sign_bias(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  int arf_sign_bias;
#if CONFIG_EXT_REFS
  const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
  // The arf_sign_bias will be one for internal ARFs'
  arf_sign_bias = cpi->rc.source_alt_ref_active &&
                  (!cpi->refresh_alt_ref_frame ||
                   gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE);
#else   // !CONFIG_EXT_REFS
  if ((cpi->oxcf.pass == 2) && cpi->multi_arf_allowed) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    arf_sign_bias = cpi->rc.source_alt_ref_active &&
                    (!cpi->refresh_alt_ref_frame ||
                     (gf_group->rf_level[gf_group->index] == GF_ARF_LOW));
  } else {
    arf_sign_bias =
        (cpi->rc.source_alt_ref_active && !cpi->refresh_alt_ref_frame);
  }
#endif  // CONFIG_EXT_REFS

  cm->ref_frame_sign_bias[ALTREF_FRAME] = arf_sign_bias;
#if CONFIG_EXT_REFS
  cm->ref_frame_sign_bias[BWDREF_FRAME] = cm->ref_frame_sign_bias[ALTREF_FRAME];
  cm->ref_frame_sign_bias[ALTREF2_FRAME] =
      cm->ref_frame_sign_bias[ALTREF_FRAME];
#endif  // CONFIG_EXT_REFS
}
#endif  // !CONFIG_FRAME_SIGN_BIAS

static int setup_interp_filter_search_mask(AV1_COMP *cpi) {
  InterpFilter ifilter;
  int ref_total[TOTAL_REFS_PER_FRAME] = { 0 };
  MV_REFERENCE_FRAME ref;
  int mask = 0;
  int arf_idx = ALTREF_FRAME;

#if CONFIG_EXT_REFS
  if (cpi->common.last_frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame ||
      cpi->refresh_alt2_ref_frame)
#else   // !CONFIG_EXT_REFS
  if (cpi->common.last_frame_type == KEY_FRAME || cpi->refresh_alt_ref_frame)
#endif  // CONFIG_EXT_REFS
    return mask;

  for (ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref)
    for (ifilter = EIGHTTAP_REGULAR; ifilter < SWITCHABLE_FILTERS; ++ifilter)
      ref_total[ref] += cpi->interp_filter_selected[ref][ifilter];

  for (ifilter = EIGHTTAP_REGULAR; ifilter < SWITCHABLE_FILTERS; ++ifilter) {
    if ((ref_total[LAST_FRAME] &&
         cpi->interp_filter_selected[LAST_FRAME][ifilter] == 0) &&
#if CONFIG_EXT_REFS
        (ref_total[LAST2_FRAME] == 0 ||
         cpi->interp_filter_selected[LAST2_FRAME][ifilter] * 50 <
             ref_total[LAST2_FRAME]) &&
        (ref_total[LAST3_FRAME] == 0 ||
         cpi->interp_filter_selected[LAST3_FRAME][ifilter] * 50 <
             ref_total[LAST3_FRAME]) &&
#endif  // CONFIG_EXT_REFS
        (ref_total[GOLDEN_FRAME] == 0 ||
         cpi->interp_filter_selected[GOLDEN_FRAME][ifilter] * 50 <
             ref_total[GOLDEN_FRAME]) &&
#if CONFIG_EXT_REFS
        (ref_total[BWDREF_FRAME] == 0 ||
         cpi->interp_filter_selected[BWDREF_FRAME][ifilter] * 50 <
             ref_total[BWDREF_FRAME]) &&
        (ref_total[ALTREF2_FRAME] == 0 ||
         cpi->interp_filter_selected[ALTREF2_FRAME][ifilter] * 50 <
             ref_total[ALTREF2_FRAME]) &&
#endif  // CONFIG_EXT_REFS
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
  int h;
  char file_name[256] = "/tmp/enc_filtered_recon.yuv";
  FILE *f_recon = NULL;

  if (recon_buf == NULL || !cm->show_frame) {
    printf("Frame %d is not ready or no show to dump.\n",
           cm->current_video_frame);
    return;
  }

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
      "\nFrame=%5d, encode_update_type[%5d]=%1d, show_existing_frame=%d, "
      "source_alt_ref_active=%d, refresh_alt_ref_frame=%d, rf_level=%d, "
      "y_stride=%4d, uv_stride=%4d, cm->width=%4d, cm->height=%4d\n",
      cm->current_video_frame, cpi->twopass.gf_group.index,
      cpi->twopass.gf_group.update_type[cpi->twopass.gf_group.index],
      cm->show_existing_frame, cpi->rc.source_alt_ref_active,
      cpi->refresh_alt_ref_frame,
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

static void make_update_tile_list_enc(AV1_COMP *cpi, const int tile_rows,
                                      const int tile_cols,
                                      FRAME_CONTEXT *ec_ctxs[]) {
  int i;
  for (i = 0; i < tile_rows * tile_cols; ++i)
    ec_ctxs[i] = &cpi->tile_data[i].tctx;
}

static void encode_frame_to_data_rate(AV1_COMP *cpi, size_t *size,
                                      uint8_t *dest, int skip_adapt,
                                      unsigned int *frame_flags) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  struct segmentation *const seg = &cm->seg;
  FRAME_CONTEXT **tile_ctxs = aom_malloc(cm->tile_rows * cm->tile_cols *
                                         sizeof(&cpi->tile_data[0].tctx));
  aom_cdf_prob **cdf_ptrs =
      aom_malloc(cm->tile_rows * cm->tile_cols *
                 sizeof(&cpi->tile_data[0].tctx.partition_cdf[0][0]));
#if CONFIG_XIPHRC
  int frame_type;
  int drop_this_frame = 0;
#endif  // CONFIG_XIPHRC
  set_ext_overrides(cpi);
  aom_clear_system_state();

#if !CONFIG_FRAME_SIGN_BIAS
  // Set the arf sign bias for this frame.
  set_arf_sign_bias(cpi);
#endif  // !CONFIG_FRAME_SIGN_BIAS

#if CONFIG_TEMPMV_SIGNALING
  // frame type has been decided outside of this function call
  cm->cur_frame->intra_only = cm->frame_type == KEY_FRAME || cm->intra_only;
  cm->use_prev_frame_mvs =
      !cpi->oxcf.disable_tempmv && !cm->cur_frame->intra_only;
#endif

#if CONFIG_EXT_REFS
  // NOTE:
  // (1) Move the setup of the ref_frame_flags upfront as it would be
  //     determined by the current frame properties;
  // (2) The setup of the ref_frame_flags applies to both show_existing_frame's
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
    av1_pack_bitstream(cpi, dest, size);

    // Set up frame to show to get ready for stats collection.
    cm->frame_to_show = get_frame_new_buffer(cm);

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
    //     update will be done when the following is called, which will exchange
    //     the virtual indexes between LAST_FRAME and ALTREF2_FRAME, so that
    //     LAST3 will get retired, LAST2 becomes LAST3, LAST becomes LAST2, and
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
#if CONFIG_XIPHRC
      frame_type = cm->frame_type == INTER_FRAME ? OD_P_FRAME : OD_I_FRAME;
      drop_this_frame = od_enc_rc_update_state(
          &cpi->od_rc, *size << 3, cpi->refresh_golden_frame,
          cpi->refresh_alt_ref_frame, frame_type, cpi->droppable);
#else
      av1_rc_postencode_update(cpi, *size);
#endif
    }

    ++cm->current_video_frame;

    aom_free(tile_ctxs);
    aom_free(cdf_ptrs);
    return;
  }
#endif  // CONFIG_EXT_REFS

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  if (cpi->oxcf.pass == 2 && cpi->sf.adaptive_interp_filter_search)
    cpi->sf.interp_filter_search_mask = setup_interp_filter_search_mask(cpi);

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm)) {
    // Reset the loop filter deltas and segmentation map.
    av1_reset_segment_features(cm);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->rc.source_alt_ref_active = 0;

    cm->error_resilient_mode = oxcf->error_resilient_mode;

#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
    // By default, encoder assumes decoder can use prev_mi.
    if (cm->error_resilient_mode) {
      cm->reset_frame_context = RESET_FRAME_CONTEXT_NONE;
      cm->refresh_frame_context = REFRESH_FRAME_CONTEXT_FORWARD;
    } else if (cm->intra_only) {
      // Only reset the current context.
      cm->reset_frame_context = RESET_FRAME_CONTEXT_CURRENT;
    }
#endif
  }
  if (cpi->oxcf.mtu == 0) {
    cm->num_tg = cpi->oxcf.num_tile_groups;
  } else {
    // Use a default value for the purposes of weighting costs in probability
    // updates
    cm->num_tg = DEFAULT_MAX_NUM_TG;
  }

#if CONFIG_EXT_TILE
  cm->large_scale_tile = cpi->oxcf.large_scale_tile;
  cm->single_tile_decoding = cpi->oxcf.single_tile_decoding;
#endif  // CONFIG_EXT_TILE

#if CONFIG_XIPHRC
  if (drop_this_frame) {
    av1_rc_postencode_update_drop_frame(cpi);
    ++cm->current_video_frame;
    aom_free(tile_ctxs);
    aom_free(cdf_ptrs);
    return;
  }
#else
  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame.
  if (oxcf->pass == 0 && oxcf->rc_mode == AOM_CBR &&
      cm->frame_type != KEY_FRAME) {
    if (av1_rc_drop_frame(cpi)) {
      av1_rc_postencode_update_drop_frame(cpi);
      ++cm->current_video_frame;
      aom_free(tile_ctxs);
      aom_free(cdf_ptrs);
      return;
    }
  }
#endif

  aom_clear_system_state();

#if CONFIG_INTERNAL_STATS
  memset(cpi->mode_chosen_counts, 0,
         MAX_MODES * sizeof(*cpi->mode_chosen_counts));
#endif

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    /* Non-normative definition of current_frame_id ("frame counter" with
    * wraparound) */
    const int frame_id_length = FRAME_ID_LENGTH_MINUS7 + 7;
    if (cm->current_frame_id == -1) {
      int lsb, msb;
/* quasi-random initialization of current_frame_id for a key frame */
#if CONFIG_HIGHBITDEPTH
      if (cpi->source->flags & YV12_FLAG_HIGHBITDEPTH) {
        lsb = CONVERT_TO_SHORTPTR(cpi->source->y_buffer)[0] & 0xff;
        msb = CONVERT_TO_SHORTPTR(cpi->source->y_buffer)[1] & 0xff;
      } else {
#endif
        lsb = cpi->source->y_buffer[0] & 0xff;
        msb = cpi->source->y_buffer[1] & 0xff;
#if CONFIG_HIGHBITDEPTH
      }
#endif
      cm->current_frame_id = ((msb << 8) + lsb) % (1 << frame_id_length);
    } else {
      cm->current_frame_id =
          (cm->current_frame_id + 1 + (1 << frame_id_length)) %
          (1 << frame_id_length);
    }
  }
#endif  // CONFIG_REFERENCE_BUFFER

#if CONFIG_EXT_DELTA_Q
  cm->delta_q_present_flag = cpi->oxcf.deltaq_mode != NO_DELTA_Q;
  cm->delta_lf_present_flag = cpi->oxcf.deltaq_mode == DELTA_Q_LF;
#if CONFIG_LOOPFILTER_LEVEL
  cm->delta_lf_multi = DEFAULT_DELTA_LF_MULTI;
#endif  // CONFIG_LOOPFILTER_LEVEL
#endif

  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    encode_without_recode_loop(cpi);
  } else {
    encode_with_recode_loop(cpi, size, dest);
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
#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth) {
      cpi->ambient_err =
          aom_highbd_get_y_sse(cpi->source, get_frame_new_buffer(cm));
    } else {
      cpi->ambient_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
    }
#else
    cpi->ambient_err = aom_get_y_sse(cpi->source, get_frame_new_buffer(cm));
#endif  // CONFIG_HIGHBITDEPTH
  }

  // If the encoder forced a KEY_FRAME decision
  if (cm->frame_type == KEY_FRAME) {
    cpi->refresh_last_frame = 1;
  }

  cm->frame_to_show = get_frame_new_buffer(cm);
  cm->frame_to_show->color_space = cm->color_space;
#if CONFIG_COLORSPACE_HEADERS
  cm->frame_to_show->transfer_function = cm->transfer_function;
  cm->frame_to_show->chroma_sample_position = cm->chroma_sample_position;
#endif
  cm->frame_to_show->color_range = cm->color_range;
  cm->frame_to_show->render_width = cm->render_width;
  cm->frame_to_show->render_height = cm->render_height;

#if CONFIG_EXT_REFS
// TODO(zoeliu): For non-ref frames, loop filtering may need to be turned
// off.
#endif  // CONFIG_EXT_REFS

  // Pick the loop filter level for the frame.
  loopfilter_frame(cpi, cm);

#ifdef OUTPUT_YUV_REC
  aom_write_one_yuv_frame(cm, cm->frame_to_show);
#endif

  // Build the bitstream
  av1_pack_bitstream(cpi, dest, size);

  if (skip_adapt) {
    aom_free(tile_ctxs);
    aom_free(cdf_ptrs);
    return;
  }

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    int i;
    /* Update reference frame id values based on the value of refresh_mask */
    for (i = 0; i < REF_FRAMES; i++) {
      if ((cm->refresh_mask >> i) & 1) {
        cm->ref_frame_id[i] = cm->current_frame_id;
      }
    }
  }
#endif  // CONFIG_REFERENCE_BUFFER

#if DUMP_RECON_FRAMES == 1
  // NOTE(zoeliu): For debug - Output the filtered reconstructed video.
  if (cm->show_frame) dump_filtered_recon_frames(cpi);
#endif  // DUMP_RECON_FRAMES

  if (cm->seg.update_map) update_reference_segmentation_map(cpi);

  if (frame_is_intra_only(cm) == 0) {
    release_scaled_references(cpi);
  }

  update_reference_frames(cpi);

#if CONFIG_ENTROPY_STATS
  av1_accumulate_frame_counts(&aggregate_fc, &cm->counts);
  assert(cm->frame_context_idx < FRAME_CONTEXTS);
  av1_accumulate_frame_counts(&aggregate_fc_per_type[cm->frame_context_idx],
                              &cm->counts);
#endif  // CONFIG_ENTROPY_STATS
  if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
#if CONFIG_LV_MAP
    av1_adapt_coef_probs(cm);
#endif  // CONFIG_LV_MAP
    av1_adapt_intra_frame_probs(cm);
    make_update_tile_list_enc(cpi, cm->tile_rows, cm->tile_cols, tile_ctxs);
    av1_average_tile_coef_cdfs(cpi->common.fc, tile_ctxs, cdf_ptrs,
                               cm->tile_rows * cm->tile_cols);
    av1_average_tile_intra_cdfs(cpi->common.fc, tile_ctxs, cdf_ptrs,
                                cm->tile_rows * cm->tile_cols);
#if CONFIG_PVQ
    av1_average_tile_pvq_cdfs(cpi->common.fc, tile_ctxs,
                              cm->tile_rows * cm->tile_cols);
#endif  // CONFIG_PVQ
#if CONFIG_ADAPT_SCAN
    av1_adapt_scan_order(cm);
#endif  // CONFIG_ADAPT_SCAN
  }

  if (!frame_is_intra_only(cm)) {
    if (cm->refresh_frame_context == REFRESH_FRAME_CONTEXT_BACKWARD) {
      av1_adapt_inter_frame_probs(cm);
      av1_adapt_mv_probs(cm, cm->allow_high_precision_mv);
      av1_average_tile_inter_cdfs(&cpi->common, cpi->common.fc, tile_ctxs,
                                  cdf_ptrs, cm->tile_rows * cm->tile_cols);
      av1_average_tile_mv_cdfs(cpi->common.fc, tile_ctxs, cdf_ptrs,
                               cm->tile_rows * cm->tile_cols);
    }
  }

  if (cpi->refresh_golden_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_GOLDEN;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_ALTREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_ALTREF;

#if CONFIG_EXT_REFS
  if (cpi->refresh_bwd_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_BWDREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_BWDREF;
#endif  // CONFIG_EXT_REFS

#if !CONFIG_EXT_REFS
  cpi->ref_frame_flags = get_ref_frame_flags(cpi);
#endif  // !CONFIG_EXT_REFS

  cm->last_frame_type = cm->frame_type;

#if CONFIG_XIPHRC
  frame_type = cm->frame_type == KEY_FRAME ? OD_I_FRAME : OD_P_FRAME;

  drop_this_frame =
      od_enc_rc_update_state(&cpi->od_rc, *size << 3, cpi->refresh_golden_frame,
                             cpi->refresh_alt_ref_frame, frame_type, 0);
  if (drop_this_frame) {
    av1_rc_postencode_update_drop_frame(cpi);
    ++cm->current_video_frame;
    aom_free(tile_ctxs);
    aom_free(cdf_ptrs);
    return;
  }
#else   // !CONFIG_XIPHRC
  av1_rc_postencode_update(cpi, *size);
#endif  // CONFIG_XIPHRC

#if 0
  output_frame_level_debug_stats(cpi);
#endif

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
#if CONFIG_EXT_REFS
// TODO(zoeliu): We may only swamp mi and prev_mi for those frames that are
// being used as reference.
#endif  // CONFIG_EXT_REFS
    swap_mi_and_prev_mi(cm);
    // Don't increment frame counters if this was an altref buffer
    // update not a real frame
    ++cm->current_video_frame;
  }

#if CONFIG_EXT_REFS
  // NOTE: Shall not refer to any frame not used as reference.
  if (cm->is_reference_frame) {
#endif  // CONFIG_EXT_REFS
    cm->prev_frame = cm->cur_frame;
    // keep track of the last coded dimensions
    cm->last_width = cm->width;
    cm->last_height = cm->height;

    // reset to normal state now that we are done.
    cm->last_show_frame = cm->show_frame;
#if CONFIG_EXT_REFS
  }
#endif  // CONFIG_EXT_REFS

  aom_free(tile_ctxs);
  aom_free(cdf_ptrs);
}

static void Pass0Encode(AV1_COMP *cpi, size_t *size, uint8_t *dest,
                        int skip_adapt, unsigned int *frame_flags) {
#if CONFIG_XIPHRC
  int64_t ip_count;
  int frame_type, is_golden, is_altref;

  /* Not updated during init so update it here */
  if (cpi->oxcf.rc_mode == AOM_Q) cpi->od_rc.quality = cpi->oxcf.cq_level;

  frame_type = od_frame_type(&cpi->od_rc, cpi->od_rc.cur_frame, &is_golden,
                             &is_altref, &ip_count);

  if (frame_type == OD_I_FRAME) {
    frame_type = KEY_FRAME;
    cpi->frame_flags &= FRAMEFLAGS_KEY;
  } else if (frame_type == OD_P_FRAME) {
    frame_type = INTER_FRAME;
  }

  if (is_altref) {
    cpi->refresh_alt_ref_frame = 1;
    cpi->rc.source_alt_ref_active = 1;
  }

  cpi->refresh_golden_frame = is_golden;
  cpi->common.frame_type = frame_type;
  if (is_golden) cpi->frame_flags &= FRAMEFLAGS_GOLDEN;
#else
  if (cpi->oxcf.rc_mode == AOM_CBR) {
    av1_rc_get_one_pass_cbr_params(cpi);
  } else {
    av1_rc_get_one_pass_vbr_params(cpi);
  }
#endif
  encode_frame_to_data_rate(cpi, size, dest, skip_adapt, frame_flags);
}

#if !CONFIG_XIPHRC
static void Pass2Encode(AV1_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  encode_frame_to_data_rate(cpi, size, dest, 0, frame_flags);

#if CONFIG_EXT_REFS
  // Do not do post-encoding update for those frames that do not have a spot in
  // a gf group, but note that an OVERLAY frame always has a spot in a gf group,
  // even when show_existing_frame is used.
  if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref) {
    av1_twopass_postencode_update(cpi);
  }
  check_show_existing_frame(cpi);
#else
  av1_twopass_postencode_update(cpi);
#endif  // CONFIG_EXT_REFS
}
#endif

int av1_receive_raw_frame(AV1_COMP *cpi, aom_enc_frame_flags_t frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  AV1_COMMON *const cm = &cpi->common;
  struct aom_usec_timer timer;
  int res = 0;
  const int subsampling_x = sd->subsampling_x;
  const int subsampling_y = sd->subsampling_y;
#if CONFIG_HIGHBITDEPTH
  const int use_highbitdepth = (sd->flags & YV12_FLAG_HIGHBITDEPTH) != 0;
#endif

#if CONFIG_HIGHBITDEPTH
  check_initial_width(cpi, use_highbitdepth, subsampling_x, subsampling_y);
#else
  check_initial_width(cpi, subsampling_x, subsampling_y);
#endif  // CONFIG_HIGHBITDEPTH

  aom_usec_timer_start(&timer);

  if (av1_lookahead_push(cpi->lookahead, sd, time_stamp, end_time,
#if CONFIG_HIGHBITDEPTH
                         use_highbitdepth,
#endif  // CONFIG_HIGHBITDEPTH
                         frame_flags))
    res = -1;
  aom_usec_timer_mark(&timer);
  cpi->time_receive_data += aom_usec_timer_elapsed(&timer);

  if ((cm->profile == PROFILE_0 || cm->profile == PROFILE_2) &&
      (subsampling_x != 1 || subsampling_y != 1)) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "Non-4:2:0 color format requires profile 1 or 3");
    res = -1;
  }
  if ((cm->profile == PROFILE_1 || cm->profile == PROFILE_3) &&
      (subsampling_x == 1 && subsampling_y == 1)) {
    aom_internal_error(&cm->error, AOM_CODEC_INVALID_PARAM,
                       "4:2:0 color format requires profile 0 or 2");
    res = -1;
  }

  return res;
}

static int frame_is_reference(const AV1_COMP *cpi) {
  const AV1_COMMON *cm = &cpi->common;

  return cm->frame_type == KEY_FRAME || cpi->refresh_last_frame ||
         cpi->refresh_golden_frame ||
#if CONFIG_EXT_REFS
         cpi->refresh_bwd_ref_frame || cpi->refresh_alt2_ref_frame ||
#endif  // CONFIG_EXT_REFS
         cpi->refresh_alt_ref_frame || !cm->error_resilient_mode ||
         cm->lf.mode_ref_delta_update || cm->seg.update_map ||
         cm->seg.update_data;
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

#if CONFIG_EXT_REFS
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
#endif  // CONFIG_EXT_REFS

static void check_src_altref(AV1_COMP *cpi,
                             const struct lookahead_entry *source) {
  RATE_CONTROL *const rc = &cpi->rc;

  // If pass == 2, the parameters set here will be reset in
  // av1_rc_get_second_pass_params()

  if (cpi->oxcf.pass == 2) {
    const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
    rc->is_src_frame_alt_ref =
#if CONFIG_EXT_REFS
        (gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE) ||
#endif  // CONFIG_EXT_REFS
        (gf_group->update_type[gf_group->index] == OVERLAY_UPDATE);
#if CONFIG_EXT_REFS
    rc->is_src_frame_ext_arf =
        gf_group->update_type[gf_group->index] == INTNL_OVERLAY_UPDATE;
#endif  // CONFIG_EXT_REFS
  } else {
    rc->is_src_frame_alt_ref =
        cpi->alt_ref_source && (source == cpi->alt_ref_source);
  }

  if (rc->is_src_frame_alt_ref) {
    // Current frame is an ARF overlay frame.
    cpi->alt_ref_source = NULL;

#if CONFIG_EXT_REFS
    if (rc->is_src_frame_ext_arf && !cpi->common.show_existing_frame) {
      // For INTNL_OVERLAY, when show_existing_frame == 0, they do need to
      // refresh the LAST_FRAME, i.e. LAST3 gets retired, LAST2 becomes LAST3,
      // LAST becomes LAST2, and INTNL_OVERLAY becomes LAST.
      cpi->refresh_last_frame = 1;
    } else {
#endif  // CONFIG_EXT_REFS
      // Don't refresh the last buffer for an ARF overlay frame. It will
      // become the GF so preserve last as an alternative prediction option.
      cpi->refresh_last_frame = 0;
#if CONFIG_EXT_REFS
    }
#endif  // CONFIG_EXT_REFS
  }
}

#if CONFIG_INTERNAL_STATS
extern double av1_get_blockiness(const unsigned char *img1, int img1_pitch,
                                 const unsigned char *img2, int img2_pitch,
                                 int width, int height);

static void adjust_image_stat(double y, double u, double v, double all,
                              ImageStat *s) {
  s->stat[Y] += y;
  s->stat[U] += u;
  s->stat[V] += v;
  s->stat[ALL] += all;
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

#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    in_bit_depth = cpi->oxcf.input_bit_depth;
    bit_depth = cm->bit_depth;
  }
#endif
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
#if CONFIG_HIGHBITDEPTH
      aom_calc_highbd_psnr(orig, recon, &psnr, bit_depth, in_bit_depth);
#else
      aom_calc_psnr(orig, recon, &psnr);
#endif  // CONFIG_HIGHBITDEPTH

      adjust_image_stat(psnr.psnr[1], psnr.psnr[2], psnr.psnr[3], psnr.psnr[0],
                        &cpi->psnr);
      cpi->total_sq_error += psnr.sse[0];
      cpi->total_samples += psnr.samples[0];
      samples = psnr.samples[0];
// TODO(yaowu): unify these two versions into one.
#if CONFIG_HIGHBITDEPTH
      if (cm->use_highbitdepth)
        frame_ssim2 =
            aom_highbd_calc_ssim(orig, recon, &weight, bit_depth, in_bit_depth);
      else
        frame_ssim2 = aom_calc_ssim(orig, recon, &weight);
#else
      frame_ssim2 = aom_calc_ssim(orig, recon, &weight);
#endif  // CONFIG_HIGHBITDEPTH

      cpi->worst_ssim = AOMMIN(cpi->worst_ssim, frame_ssim2);
      cpi->summed_quality += frame_ssim2 * weight;
      cpi->summed_weights += weight;

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
    if (cpi->b_calculate_blockiness) {
#if CONFIG_HIGHBITDEPTH
      if (!cm->use_highbitdepth)
#endif
      {
        const double frame_blockiness =
            av1_get_blockiness(orig->y_buffer, orig->y_stride, recon->y_buffer,
                               recon->y_stride, orig->y_width, orig->y_height);
        cpi->worst_blockiness = AOMMAX(cpi->worst_blockiness, frame_blockiness);
        cpi->total_blockiness += frame_blockiness;
      }

      if (cpi->b_calculate_consistency) {
#if CONFIG_HIGHBITDEPTH
        if (!cm->use_highbitdepth)
#endif
        {
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

#if CONFIG_AMVR
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

      for (int tmpY = 0; tmpY < block_size && match; tmpY++) {
        for (int tmpX = 0; tmpX < block_size && match; tmpX++) {
          if (p_cur[tmpX] != p_ref[tmpX]) {
            match = 0;
          }
        }
        p_cur += stride_cur;
        p_ref += stride_ref;
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
          block_size, &hash_value_1, &hash_value_2);

      if (av1_has_exact_match(last_hash_table, hash_value_1, hash_value_2)) {
        M++;
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
#endif

int av1_get_compressed_data(AV1_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest, int64_t *time_stamp,
                            int64_t *time_end, int flush) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  AV1_COMMON *const cm = &cpi->common;
  BufferPool *const pool = cm->buffer_pool;
  RATE_CONTROL *const rc = &cpi->rc;
  struct aom_usec_timer cmptimer;
  YV12_BUFFER_CONFIG *force_src_buffer = NULL;
  struct lookahead_entry *last_source = NULL;
  struct lookahead_entry *source = NULL;
  int arf_src_index;
#if CONFIG_EXT_REFS
  int brf_src_index;
#endif  // CONFIG_EXT_REFS
  int i;

#if CONFIG_XIPHRC
  cpi->od_rc.end_of_input = flush;
#endif

#if CONFIG_BITSTREAM_DEBUG
  assert(cpi->oxcf.max_threads == 0 &&
         "bitstream debug tool does not support multithreading");
  bitstream_queue_record_write();
  bitstream_queue_set_frame_write(cm->current_video_frame * 2 + cm->show_frame);
#endif

  aom_usec_timer_start(&cmptimer);

#if CONFIG_AMVR
  set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV, 0);
#else
  set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV);
#endif

  // Is multi-arf enabled.
  // Note that at the moment multi_arf is only configured for 2 pass VBR
  if ((oxcf->pass == 2) && (cpi->oxcf.enable_auto_arf > 1))
    cpi->multi_arf_allowed = 1;
  else
    cpi->multi_arf_allowed = 0;

// Normal defaults
#if !CONFIG_NO_FRAME_CONTEXT_SIGNALING
  cm->reset_frame_context = RESET_FRAME_CONTEXT_NONE;
#endif
  cm->refresh_frame_context =
      (oxcf->error_resilient_mode || oxcf->frame_parallel_decoding_mode)
          ? REFRESH_FRAME_CONTEXT_FORWARD
          : REFRESH_FRAME_CONTEXT_BACKWARD;

  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
#if CONFIG_EXT_REFS
  cpi->refresh_bwd_ref_frame = 0;
  cpi->refresh_alt2_ref_frame = 0;
#endif  // CONFIG_EXT_REFS
  cpi->refresh_alt_ref_frame = 0;

#if CONFIG_EXT_REFS && !CONFIG_XIPHRC
  if (oxcf->pass == 2 && cm->show_existing_frame) {
    // Manage the source buffer and flush out the source frame that has been
    // coded already; Also get prepared for PSNR calculation if needed.
    if ((source = av1_lookahead_pop(cpi->lookahead, flush)) == NULL) {
      *size = 0;
      return -1;
    }
    cpi->source = &source->img;
    // TODO(zoeliu): To track down to determine whether it's needed to adjust
    // the frame rate.
    *time_stamp = source->ts_start;
    *time_end = source->ts_end;

    // We need to adjust frame rate for an overlay frame
    if (cpi->rc.is_src_frame_alt_ref) adjust_frame_rate(cpi, source);

    // Find a free buffer for the new frame, releasing the reference previously
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

    Pass2Encode(cpi, size, dest, frame_flags);

    if (cpi->b_calculate_psnr) generate_psnr_packet(cpi);

#if CONFIG_INTERNAL_STATS
    compute_internal_stats(cpi, (int)(*size));
#endif  // CONFIG_INTERNAL_STATS

    // Clear down mmx registers
    aom_clear_system_state();

    cm->show_existing_frame = 0;
    return 0;
  }
#endif  // CONFIG_EXT_REFS && !CONFIG_XIPHRC

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
      cpi->alt_ref_source = source;

      if (oxcf->arnr_max_frames > 0) {
// Produce the filtered ARF frame.
#if CONFIG_BGSPRITE
        int bgsprite_ret = av1_background_sprite(cpi, arf_src_index);
        // Do temporal filter if bgsprite not generated.
        if (bgsprite_ret != 0)
#endif  // CONFIG_BGSPRITE
          av1_temporal_filter(cpi,
#if CONFIG_BGSPRITE
                              NULL, &cpi->alt_ref_buffer,
#endif  // CONFIG_BGSPRITE
                              arf_src_index);
        aom_extend_frame_borders(&cpi->alt_ref_buffer);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cm->intra_only = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_last_frame = 0;
      cpi->refresh_golden_frame = 0;
#if CONFIG_EXT_REFS
      cpi->refresh_bwd_ref_frame = 0;
      cpi->refresh_alt2_ref_frame = 0;
#endif  // CONFIG_EXT_REFS
      rc->is_src_frame_alt_ref = 0;
    }
    rc->source_alt_ref_pending = 0;
  }

#if CONFIG_EXT_REFS
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
      cpi->alt_ref_source = source;

      if (oxcf->arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        av1_temporal_filter(cpi,
#if CONFIG_BGSPRITE
                            NULL, NULL,
#endif  // CONFIG_BGSPRITE
                            arf_src_index);
        aom_extend_frame_borders(&cpi->alt_ref_buffer);
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
#endif  // CONFIG_EXT_REFS

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
    *frame_flags = (source->flags & AOM_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

  } else {
    *size = 0;
    if (flush && oxcf->pass == 1 && !cpi->twopass.first_pass_done) {
#if CONFIG_XIPHRC
      od_enc_rc_2pass_out(&cpi->od_rc, cpi->output_pkt_list, 1);
#else
      av1_end_first_pass(cpi); /* get last stats packet */
#endif
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

  cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];
#if CONFIG_HIGHBITDEPTH && CONFIG_GLOBAL_MOTION
  cm->cur_frame->buf.buf_8bit_valid = 0;
#endif
#if !CONFIG_EXT_REFS
  if (cpi->multi_arf_allowed) {
    if (cm->frame_type == KEY_FRAME) {
      init_buffer_indices(cpi);
    } else if (oxcf->pass == 2) {
      const GF_GROUP *const gf_group = &cpi->twopass.gf_group;
      cpi->alt_fb_idx = gf_group->arf_ref_idx[gf_group->index];
    }
  }
#endif  // !CONFIG_EXT_REFS

  // Start with a 0 size frame.
  *size = 0;

  cpi->frame_flags = *frame_flags;

  if (oxcf->pass == 2) {
#if CONFIG_XIPHRC
    if (od_enc_rc_2pass_in(&cpi->od_rc) < 0) return -1;
  }
#else
    av1_rc_get_second_pass_params(cpi);
  } else if (oxcf->pass == 1) {
    setup_frame_size(cpi);
  }
#endif

  if (cpi->oxcf.pass != 0 || frame_is_intra_only(cm) == 1) {
    for (i = 0; i < TOTAL_REFS_PER_FRAME; ++i)
      cpi->scaled_ref_idx[i] = INVALID_IDX;
  }

#if CONFIG_AOM_QM
  cm->using_qmatrix = cpi->oxcf.using_qm;
  cm->min_qmlevel = cpi->oxcf.qm_minlevel;
  cm->max_qmlevel = cpi->oxcf.qm_maxlevel;
#endif

#if CONFIG_REFERENCE_BUFFER
  if (cm->seq_params.frame_id_numbers_present_flag) {
    if (*time_stamp == 0) {
      cpi->common.current_frame_id = -1;
    }
  }
#endif  // CONFIG_REFERENCE_BUFFER
#if CONFIG_AMVR
  cpi->cur_poc++;
  if (oxcf->pass != 1 && cpi->common.allow_screen_content_tools) {
    if (cpi->common.seq_mv_precision_level == 2) {
      struct lookahead_entry *previous_entry =
          cpi->lookahead->buf + cpi->previsous_index;
      cpi->common.cur_frame_mv_precision_level = is_integer_mv(
          cpi, cpi->source, &previous_entry->img, cpi->previsou_hash_table);
    } else {
      cpi->common.cur_frame_mv_precision_level =
          cpi->common.seq_mv_precision_level;
    }
  } else {
    cpi->common.cur_frame_mv_precision_level = 0;
  }
#endif

#if CONFIG_XIPHRC
  if (oxcf->pass == 1) {
    size_t tmp;
    if (cpi->od_rc.cur_frame == 0) Pass0Encode(cpi, &tmp, dest, 1, frame_flags);
    cpi->od_rc.firstpass_quant = cpi->od_rc.target_quantizer;
    Pass0Encode(cpi, &tmp, dest, 0, frame_flags);
    od_enc_rc_2pass_out(&cpi->od_rc, cpi->output_pkt_list, 0);
  } else if (oxcf->pass == 2) {
    Pass0Encode(cpi, size, dest, 0, frame_flags);
  } else {
    if (cpi->od_rc.cur_frame == 0) {
      size_t tmp;
      Pass0Encode(cpi, &tmp, dest, 1, frame_flags);
    }
    Pass0Encode(cpi, size, dest, 0, frame_flags);
  }
#else
  if (oxcf->pass == 1) {
    cpi->td.mb.e_mbd.lossless[0] = is_lossless_requested(oxcf);
    av1_first_pass(cpi, source);
  } else if (oxcf->pass == 2) {
    Pass2Encode(cpi, size, dest, frame_flags);
  } else {
    // One pass encode
    Pass0Encode(cpi, size, dest, 0, frame_flags);
  }
#endif
#if CONFIG_HASH_ME
  if (oxcf->pass != 1 && cpi->common.allow_screen_content_tools) {
#if CONFIG_AMVR
    cpi->previsou_hash_table = &cm->cur_frame->hash_table;
    {
      int l;
      for (l = -MAX_PRE_FRAMES; l < cpi->lookahead->max_sz; l++) {
        if ((cpi->lookahead->buf + l) == source) {
          cpi->previsous_index = l;
          break;
        }
      }

      if (l == cpi->lookahead->max_sz) {
        aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                           "Failed to find last frame original buffer");
      }
    }
#endif
  }

#endif

#if CONFIG_NO_FRAME_CONTEXT_SIGNALING
  cm->frame_contexts[cm->new_fb_idx] = *cm->fc;
#else
  if (!cm->error_resilient_mode)
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
#endif  // CONFIG_NO_FRAME_CONTEXT_SIGNALING

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

#if CONFIG_XIPHRC
  cpi->od_rc.cur_frame++;
#endif

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

void av1_apply_encoding_flags(AV1_COMP *cpi, aom_enc_frame_flags_t flags) {
  if (flags &
      (AOM_EFLAG_NO_REF_LAST | AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF)) {
    int ref = AOM_REFFRAME_ALL;

    if (flags & AOM_EFLAG_NO_REF_LAST) {
      ref ^= AOM_LAST_FLAG;
#if CONFIG_EXT_REFS
      ref ^= AOM_LAST2_FLAG;
      ref ^= AOM_LAST3_FLAG;
#endif  // CONFIG_EXT_REFS
    }

    if (flags & AOM_EFLAG_NO_REF_GF) ref ^= AOM_GOLD_FLAG;

    if (flags & AOM_EFLAG_NO_REF_ARF) ref ^= AOM_ALT_FLAG;

    av1_use_as_reference(cpi, ref);
  }

  if (flags &
      (AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
       AOM_EFLAG_FORCE_GF | AOM_EFLAG_FORCE_ARF)) {
    int upd = AOM_REFFRAME_ALL;

    if (flags & AOM_EFLAG_NO_UPD_LAST) {
      upd ^= AOM_LAST_FLAG;
#if CONFIG_EXT_REFS
      upd ^= AOM_LAST2_FLAG;
      upd ^= AOM_LAST3_FLAG;
#endif  // CONFIG_EXT_REFS
    }

    if (flags & AOM_EFLAG_NO_UPD_GF) upd ^= AOM_GOLD_FLAG;

    if (flags & AOM_EFLAG_NO_UPD_ARF) upd ^= AOM_ALT_FLAG;

    av1_update_reference(cpi, upd);
  }

  if (flags & AOM_EFLAG_NO_UPD_ENTROPY) {
    av1_update_entropy(cpi, 0);
  }
}
