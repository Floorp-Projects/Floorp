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

#include "./aom_config.h"
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

#if CONFIG_CB4X4
  const int mb_cols = (mi_cols + 2) >> 2;
  const int mb_rows = (mi_rows + 2) >> 2;
#else
  const int mb_cols = (mi_cols + 1) >> 1;
  const int mb_rows = (mi_rows + 1) >> 1;
#endif
  return mb_rows * mb_cols;
}

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

#if CONFIG_CB4X4
  cm->mb_cols = (cm->mi_cols + 2) >> 2;
  cm->mb_rows = (cm->mi_rows + 2) >> 2;
#else
  cm->mb_cols = (cm->mi_cols + 1) >> 1;
  cm->mb_rows = (cm->mi_rows + 1) >> 1;
#endif
  cm->MBs = cm->mb_rows * cm->mb_cols;
}

static int alloc_seg_map(AV1_COMMON *cm, int seg_map_size) {
  int i;

  for (i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
    cm->seg_map_array[i] = (uint8_t *)aom_calloc(seg_map_size, 1);
    if (cm->seg_map_array[i] == NULL) return 1;
  }
  cm->seg_map_alloc_size = seg_map_size;

  // Init the index.
  cm->seg_map_idx = 0;
  cm->prev_seg_map_idx = 1;

  cm->current_frame_seg_map = cm->seg_map_array[cm->seg_map_idx];
  if (!cm->frame_parallel_decode)
    cm->last_frame_seg_map = cm->seg_map_array[cm->prev_seg_map_idx];

  return 0;
}

static void free_seg_map(AV1_COMMON *cm) {
  int i;

  for (i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
    aom_free(cm->seg_map_array[i]);
    cm->seg_map_array[i] = NULL;
  }

  cm->current_frame_seg_map = NULL;

  if (!cm->frame_parallel_decode) {
    cm->last_frame_seg_map = NULL;
  }
  cm->seg_map_alloc_size = 0;
}

static void free_scratch_buffers(AV1_COMMON *cm) {
  (void)cm;
#if CONFIG_NCOBMC && CONFIG_NCOBMC_ADAPT_WEIGHT
  for (int i = 0; i < 4; ++i) {
    if (cm->ncobmcaw_buf[i]) {
      aom_free(cm->ncobmcaw_buf[i]);
      cm->ncobmcaw_buf[i] = NULL;
    }
  }
#endif  // CONFIG_NCOBMC && CONFIG_NCOBMC_ADAPT_WEIGHT
}

static int alloc_scratch_buffers(AV1_COMMON *cm) {
  (void)cm;
#if CONFIG_NCOBMC && CONFIG_NCOBMC_ADAPT_WEIGHT
  // If not allocated already, allocate
  if (!cm->ncobmcaw_buf[0] && !cm->ncobmcaw_buf[1] && !cm->ncobmcaw_buf[2] &&
      !cm->ncobmcaw_buf[3]) {
    for (int i = 0; i < 4; ++i) {
      CHECK_MEM_ERROR(
          cm, cm->ncobmcaw_buf[i],
          (uint8_t *)aom_memalign(
              16, (1 + CONFIG_HIGHBITDEPTH) * MAX_MB_PLANE * MAX_SB_SQUARE));
    }
  }
#endif  // CONFIG_NCOBMC && CONFIG_NCOBMC_ADAPT_WEIGHT
  return 0;
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
#if CONFIG_MFMV
    aom_free(pool->frame_bufs[i].tpl_mvs);
    pool->frame_bufs[i].tpl_mvs = NULL;
#endif
    aom_free_frame_buffer(&pool->frame_bufs[i].buf);
#if CONFIG_HASH_ME
    av1_hash_table_destroy(&pool->frame_bufs[i].hash_table);
#endif
  }
}

#if CONFIG_LOOP_RESTORATION
// Assumes cm->rst_info[p].restoration_tilesize is already initialized
void av1_alloc_restoration_buffers(AV1_COMMON *cm) {
  int p;
#if CONFIG_FRAME_SUPERRES
  int width = cm->superres_upscaled_width;
  int height = cm->superres_upscaled_height;
#else
  int width = cm->width;
  int height = cm->height;
#endif  // CONFIG_FRAME_SUPERRES
  av1_alloc_restoration_struct(cm, &cm->rst_info[0], width, height);
  for (p = 1; p < MAX_MB_PLANE; ++p)
    av1_alloc_restoration_struct(cm, &cm->rst_info[p],
                                 ROUND_POWER_OF_TWO(width, cm->subsampling_x),
                                 ROUND_POWER_OF_TWO(height, cm->subsampling_y));
  aom_free(cm->rst_internal.tmpbuf);
  CHECK_MEM_ERROR(cm, cm->rst_internal.tmpbuf,
                  (int32_t *)aom_memalign(16, RESTORATION_TMPBUF_SIZE));

#if CONFIG_STRIPED_LOOP_RESTORATION
  // Allocate internal storage for the loop restoration stripe boundary lines
  for (p = 0; p < MAX_MB_PLANE; ++p) {
    int w = p == 0 ? width : ROUND_POWER_OF_TWO(width, cm->subsampling_x);
    int align_bits = 5;  // align for efficiency
    int stride = ALIGN_POWER_OF_TWO(w, align_bits);
    int num_stripes = (height + 63) / 64;
    // for each processing stripe: 2 lines above, 2 below
    int buf_size = num_stripes * 2 * stride;
    uint8_t *above_buf, *below_buf;

    aom_free(cm->rst_internal.stripe_boundary_above[p]);
    aom_free(cm->rst_internal.stripe_boundary_below[p]);

#if CONFIG_HIGHBITDEPTH
    if (cm->use_highbitdepth) buf_size = buf_size * 2;
#endif
    CHECK_MEM_ERROR(cm, above_buf,
                    (uint8_t *)aom_memalign(1 << align_bits, buf_size));
    CHECK_MEM_ERROR(cm, below_buf,
                    (uint8_t *)aom_memalign(1 << align_bits, buf_size));
    cm->rst_internal.stripe_boundary_above[p] = above_buf;
    cm->rst_internal.stripe_boundary_below[p] = below_buf;
    cm->rst_internal.stripe_boundary_stride[p] = stride;
  }
#endif  // CONFIG_STRIPED_LOOP_RESTORATION
}

void av1_free_restoration_buffers(AV1_COMMON *cm) {
  int p;
  for (p = 0; p < MAX_MB_PLANE; ++p)
    av1_free_restoration_struct(&cm->rst_info[p]);
  aom_free(cm->rst_internal.tmpbuf);
  cm->rst_internal.tmpbuf = NULL;
}
#endif  // CONFIG_LOOP_RESTORATION

void av1_free_context_buffers(AV1_COMMON *cm) {
  int i;
  cm->free_mi(cm);
  free_seg_map(cm);
  free_scratch_buffers(cm);
  for (i = 0; i < MAX_MB_PLANE; i++) {
    aom_free(cm->above_context[i]);
    cm->above_context[i] = NULL;
  }
  aom_free(cm->above_seg_context);
  cm->above_seg_context = NULL;
  cm->above_context_alloc_cols = 0;
#if CONFIG_VAR_TX
  aom_free(cm->above_txfm_context);
  cm->above_txfm_context = NULL;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cm->top_txfm_context[i]);
    cm->top_txfm_context[i] = NULL;
  }
#endif
}

int av1_alloc_context_buffers(AV1_COMMON *cm, int width, int height) {
  int new_mi_size;

  av1_set_mb_mi(cm, width, height);
  new_mi_size = cm->mi_stride * calc_mi_size(cm->mi_rows);
  if (cm->mi_alloc_size < new_mi_size) {
    cm->free_mi(cm);
    if (cm->alloc_mi(cm, new_mi_size)) goto fail;
  }

  if (cm->seg_map_alloc_size < cm->mi_rows * cm->mi_cols) {
    // Create the segmentation map structure and set to 0.
    free_seg_map(cm);
    if (alloc_seg_map(cm, cm->mi_rows * cm->mi_cols)) goto fail;
  }
  if (alloc_scratch_buffers(cm)) goto fail;

  if (cm->above_context_alloc_cols < cm->mi_cols) {
    // TODO(geza.lore): These are bigger than they need to be.
    // cm->tile_width would be enough but it complicates indexing a
    // little elsewhere.
    const int aligned_mi_cols =
        ALIGN_POWER_OF_TWO(cm->mi_cols, MAX_MIB_SIZE_LOG2);
    int i;

    for (i = 0; i < MAX_MB_PLANE; i++) {
      aom_free(cm->above_context[i]);
      cm->above_context[i] = (ENTROPY_CONTEXT *)aom_calloc(
          aligned_mi_cols << (MI_SIZE_LOG2 - tx_size_wide_log2[0]),
          sizeof(*cm->above_context[0]));
      if (!cm->above_context[i]) goto fail;
    }

    aom_free(cm->above_seg_context);
    cm->above_seg_context = (PARTITION_CONTEXT *)aom_calloc(
        aligned_mi_cols, sizeof(*cm->above_seg_context));
    if (!cm->above_seg_context) goto fail;

#if CONFIG_VAR_TX
    aom_free(cm->above_txfm_context);
    cm->above_txfm_context = (TXFM_CONTEXT *)aom_calloc(
        aligned_mi_cols << TX_UNIT_WIDE_LOG2, sizeof(*cm->above_txfm_context));
    if (!cm->above_txfm_context) goto fail;

    for (i = 0; i < MAX_MB_PLANE; ++i) {
      aom_free(cm->top_txfm_context[i]);
      cm->top_txfm_context[i] =
          (TXFM_CONTEXT *)aom_calloc(aligned_mi_cols << TX_UNIT_WIDE_LOG2,
                                     sizeof(*cm->top_txfm_context[0]));
      if (!cm->top_txfm_context[i]) goto fail;
    }
#endif

    cm->above_context_alloc_cols = aligned_mi_cols;
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

void av1_init_context_buffers(AV1_COMMON *cm) {
  cm->setup_mi(cm);
  if (cm->last_frame_seg_map && !cm->frame_parallel_decode)
    memset(cm->last_frame_seg_map, 0, cm->mi_rows * cm->mi_cols);
}

void av1_swap_current_and_last_seg_map(AV1_COMMON *cm) {
  // Swap indices.
  const int tmp = cm->seg_map_idx;
  cm->seg_map_idx = cm->prev_seg_map_idx;
  cm->prev_seg_map_idx = tmp;

  cm->current_frame_seg_map = cm->seg_map_array[cm->seg_map_idx];
  cm->last_frame_seg_map = cm->seg_map_array[cm->prev_seg_map_idx];
}
