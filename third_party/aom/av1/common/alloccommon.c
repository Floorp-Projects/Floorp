/*
 *
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "config/aom_config.h"

#include "aom_mem/aom_mem.h"

#include "av1/common/alloccommon.h"
#include "av1/common/blockd.h"
#include "av1/common/entropymode.h"
#include "av1/common/entropymv.h"
#include "av1/common/onyxc_int.h"

int av1_get_MBs(int width, int height) {
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);
  const int mi_cols = aligned_width >> MI_SIZE_LOG2;
  const int mi_rows = aligned_height >> MI_SIZE_LOG2;

  const int mb_cols = (mi_cols + 2) >> 2;
  const int mb_rows = (mi_rows + 2) >> 2;
  return mb_rows * mb_cols;
}

#if LOOP_FILTER_BITMASK
static int alloc_loop_filter_mask(AV1_COMMON *cm) {
  aom_free(cm->lf.lfm);
  cm->lf.lfm = NULL;

  // Each lfm holds bit masks for all the 4x4 blocks in a max
  // 64x64 (128x128 for ext_partitions) region.  The stride
  // and rows are rounded up / truncated to a multiple of 16
  // (32 for ext_partition).
  cm->lf.lfm_stride = (cm->mi_cols + (MI_SIZE_64X64 - 1)) >> MIN_MIB_SIZE_LOG2;
  cm->lf.lfm_num = ((cm->mi_rows + (MI_SIZE_64X64 - 1)) >> MIN_MIB_SIZE_LOG2) *
                   cm->lf.lfm_stride;
  cm->lf.lfm =
      (LoopFilterMask *)aom_calloc(cm->lf.lfm_num, sizeof(*cm->lf.lfm));
  if (!cm->lf.lfm) return 1;

  unsigned int i;
  for (i = 0; i < cm->lf.lfm_num; ++i) av1_zero(cm->lf.lfm[i]);

  return 0;
}

static void free_loop_filter_mask(AV1_COMMON *cm) {
  if (cm->lf.lfm == NULL) return;

  aom_free(cm->lf.lfm);
  cm->lf.lfm = NULL;
  cm->lf.lfm_num = 0;
  cm->lf.lfm_stride = 0;
}
#endif

void av1_set_mb_mi(AV1_COMMON *cm, int width, int height) {
  // Ensure that the decoded width and height are both multiples of
  // 8 luma pixels (note: this may only be a multiple of 4 chroma pixels if
  // subsampling is used).
  // This simplifies the implementation of various experiments,
  // eg. cdef, which operates on units of 8x8 luma pixels.
  const int aligned_width = ALIGN_POWER_OF_TWO(width, 3);
  const int aligned_height = ALIGN_POWER_OF_TWO(height, 3);

  cm->mi_cols = aligned_width >> MI_SIZE_LOG2;
  cm->mi_rows = aligned_height >> MI_SIZE_LOG2;
  cm->mi_stride = calc_mi_size(cm->mi_cols);

  cm->mb_cols = (cm->mi_cols + 2) >> 2;
  cm->mb_rows = (cm->mi_rows + 2) >> 2;
  cm->MBs = cm->mb_rows * cm->mb_cols;

#if LOOP_FILTER_BITMASK
  alloc_loop_filter_mask(cm);
#endif
}

void av1_free_ref_frame_buffers(BufferPool *pool) {
  int i;

  for (i = 0; i < FRAME_BUFFERS; ++i) {
    if (pool->frame_bufs[i].ref_count > 0 &&
        pool->frame_bufs[i].raw_frame_buffer.data != NULL) {
      pool->release_fb_cb(pool->cb_priv, &pool->frame_bufs[i].raw_frame_buffer);
      pool->frame_bufs[i].ref_count = 0;
    }
    aom_free(pool->frame_bufs[i].mvs);
    pool->frame_bufs[i].mvs = NULL;
    aom_free(pool->frame_bufs[i].seg_map);
    pool->frame_bufs[i].seg_map = NULL;
    aom_free_frame_buffer(&pool->frame_bufs[i].buf);
  }
}

// Assumes cm->rst_info[p].restoration_unit_size is already initialized
void av1_alloc_restoration_buffers(AV1_COMMON *cm) {
  const int num_planes = av1_num_planes(cm);
  for (int p = 0; p < num_planes; ++p)
    av1_alloc_restoration_struct(cm, &cm->rst_info[p], p > 0);

  if (cm->rst_tmpbuf == NULL) {
    CHECK_MEM_ERROR(cm, cm->rst_tmpbuf,
                    (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));
  }

  if (cm->rlbs == NULL) {
    CHECK_MEM_ERROR(cm, cm->rlbs, aom_malloc(sizeof(RestorationLineBuffers)));
  }

  // For striped loop restoration, we divide each row of tiles into "stripes",
  // of height 64 luma pixels but with an offset by RESTORATION_UNIT_OFFSET
  // luma pixels to match the output from CDEF. We will need to store 2 *
  // RESTORATION_CTX_VERT lines of data for each stripe, and also need to be
  // able to quickly answer the question "Where is the <n>'th stripe for tile
  // row <m>?" To make that efficient, we generate the rst_last_stripe array.
  int num_stripes = 0;
  for (int i = 0; i < cm->tile_rows; ++i) {
    TileInfo tile_info;
    av1_tile_set_row(&tile_info, cm, i);
    const int mi_h = tile_info.mi_row_end - tile_info.mi_row_start;
    const int ext_h = RESTORATION_UNIT_OFFSET + (mi_h << MI_SIZE_LOG2);
    const int tile_stripes = (ext_h + 63) / 64;
    num_stripes += tile_stripes;
    cm->rst_end_stripe[i] = num_stripes;
  }

  // Now we need to allocate enough space to store the line buffers for the
  // stripes
  const int frame_w = cm->superres_upscaled_width;
  const int use_highbd = cm->use_highbitdepth ? 1 : 0;

  for (int p = 0; p < num_planes; ++p) {
    const int is_uv = p > 0;
    const int ss_x = is_uv && cm->subsampling_x;
    const int plane_w = ((frame_w + ss_x) >> ss_x) + 2 * RESTORATION_EXTRA_HORZ;
    const int stride = ALIGN_POWER_OF_TWO(plane_w, 5);
    const int buf_size = num_stripes * stride * RESTORATION_CTX_VERT
                         << use_highbd;
    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;

    if (buf_size != boundaries->stripe_boundary_size ||
        boundaries->stripe_boundary_above == NULL ||
        boundaries->stripe_boundary_below == NULL) {
      aom_free(boundaries->stripe_boundary_above);
      aom_free(boundaries->stripe_boundary_below);

      CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_above,
                      (uint8_t *)aom_memalign(32, buf_size));
      CHECK_MEM_ERROR(cm, boundaries->stripe_boundary_below,
                      (uint8_t *)aom_memalign(32, buf_size));

      boundaries->stripe_boundary_size = buf_size;
    }
    boundaries->stripe_boundary_stride = stride;
  }
}

void av1_free_restoration_buffers(AV1_COMMON *cm) {
  int p;
  for (p = 0; p < MAX_MB_PLANE; ++p)
    av1_free_restoration_struct(&cm->rst_info[p]);
  aom_free(cm->rst_tmpbuf);
  cm->rst_tmpbuf = NULL;
  aom_free(cm->rlbs);
  cm->rlbs = NULL;
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;
    aom_free(boundaries->stripe_boundary_above);
    aom_free(boundaries->stripe_boundary_below);
    boundaries->stripe_boundary_above = NULL;
    boundaries->stripe_boundary_below = NULL;
  }

  aom_free_frame_buffer(&cm->rst_frame);
}

void av1_free_above_context_buffers(AV1_COMMON *cm,
                                    int num_free_above_contexts) {
  int i;
  const int num_planes = cm->num_allocated_above_context_planes;

  for (int tile_row = 0; tile_row < num_free_above_contexts; tile_row++) {
    for (i = 0; i < num_planes; i++) {
      aom_free(cm->above_context[i][tile_row]);
      cm->above_context[i][tile_row] = NULL;
    }
    aom_free(cm->above_seg_context[tile_row]);
    cm->above_seg_context[tile_row] = NULL;

    aom_free(cm->above_txfm_context[tile_row]);
    cm->above_txfm_context[tile_row] = NULL;
  }
  for (i = 0; i < num_planes; i++) {
    aom_free(cm->above_context[i]);
    cm->above_context[i] = NULL;
  }
  aom_free(cm->above_seg_context);
  cm->above_seg_context = NULL;

  aom_free(cm->above_txfm_context);
  cm->above_txfm_context = NULL;

  cm->num_allocated_above_contexts = 0;
  cm->num_allocated_above_context_mi_col = 0;
  cm->num_allocated_above_context_planes = 0;
}

void av1_free_context_buffers(AV1_COMMON *cm) {
  cm->free_mi(cm);

  av1_free_above_context_buffers(cm, cm->num_allocated_above_contexts);

#if LOOP_FILTER_BITMASK
  free_loop_filter_mask(cm);
#endif
}

int av1_alloc_above_context_buffers(AV1_COMMON *cm,
                                    int num_alloc_above_contexts) {
  const int num_planes = av1_num_planes(cm);
  int plane_idx;
  const int aligned_mi_cols =
      ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);

  // Allocate above context buffers
  cm->num_allocated_above_contexts = num_alloc_above_contexts;
  cm->num_allocated_above_context_mi_col = aligned_mi_cols;
  cm->num_allocated_above_context_planes = num_planes;
  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    cm->above_context[plane_idx] = (ENTROPY_CONTEXT **)aom_calloc(
        num_alloc_above_contexts, sizeof(cm->above_context[0]));
    if (!cm->above_context[plane_idx]) return 1;
  }

  cm->above_seg_context = (PARTITION_CONTEXT **)aom_calloc(
      num_alloc_above_contexts, sizeof(cm->above_seg_context));
  if (!cm->above_seg_context) return 1;

  cm->above_txfm_context = (TXFM_CONTEXT **)aom_calloc(
      num_alloc_above_contexts, sizeof(cm->above_txfm_context));
  if (!cm->above_txfm_context) return 1;

  for (int tile_row = 0; tile_row < num_alloc_above_contexts; tile_row++) {
    for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
      cm->above_context[plane_idx][tile_row] = (ENTROPY_CONTEXT *)aom_calloc(
          aligned_mi_cols, sizeof(*cm->above_context[0][tile_row]));
      if (!cm->above_context[plane_idx][tile_row]) return 1;
    }

    cm->above_seg_context[tile_row] = (PARTITION_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*cm->above_seg_context[tile_row]));
    if (!cm->above_seg_context[tile_row]) return 1;

    cm->above_txfm_context[tile_row] = (TXFM_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*cm->above_txfm_context[tile_row]));
    if (!cm->above_txfm_context[tile_row]) return 1;
  }

  return 0;
}

int av1_alloc_context_buffers(AV1_COMMON *cm, int width, int height) {
  int new_mi_size;

  av1_set_mb_mi(cm, width, height);
  new_mi_size = cm->mi_stride * calc_mi_size(cm->mi_rows);
  if (cm->mi_alloc_size < new_mi_size) {
    cm->free_mi(cm);
    if (cm->alloc_mi(cm, new_mi_size)) goto fail;
  }

  return 0;

fail:
  // clear the mi_* values to force a realloc on resync
  av1_set_mb_mi(cm, 0, 0);
  av1_free_context_buffers(cm);
  return 1;
}

void av1_remove_common(AV1_COMMON *cm) {
  av1_free_context_buffers(cm);

  aom_free(cm->fc);
  cm->fc = NULL;
  aom_free(cm->frame_contexts);
  cm->frame_contexts = NULL;
}

void av1_init_context_buffers(AV1_COMMON *cm) { cm->setup_mi(cm); }
