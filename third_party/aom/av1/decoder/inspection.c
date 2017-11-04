/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "av1/decoder/decoder.h"
#include "av1/decoder/inspection.h"
#include "av1/common/enums.h"
#if CONFIG_CDEF
#include "av1/common/cdef.h"
#endif
#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

static void ifd_init_mi_rc(insp_frame_data *fd, int mi_cols, int mi_rows) {
  fd->mi_cols = mi_cols;
  fd->mi_rows = mi_rows;
  fd->mi_grid = (insp_mi_data *)aom_malloc(sizeof(insp_mi_data) * fd->mi_rows *
                                           fd->mi_cols);
}

void ifd_init(insp_frame_data *fd, int frame_width, int frame_height) {
  int mi_cols = ALIGN_POWER_OF_TWO(frame_width, 3) >> MI_SIZE_LOG2;
  int mi_rows = ALIGN_POWER_OF_TWO(frame_height, 3) >> MI_SIZE_LOG2;
  ifd_init_mi_rc(fd, mi_cols, mi_rows);
}

void ifd_clear(insp_frame_data *fd) {
  aom_free(fd->mi_grid);
  fd->mi_grid = NULL;
}

/* TODO(negge) This function may be called by more than one thread when using
               a multi-threaded decoder and this may cause a data race. */
int ifd_inspect(insp_frame_data *fd, void *decoder) {
  struct AV1Decoder *pbi = (struct AV1Decoder *)decoder;
  AV1_COMMON *const cm = &pbi->common;
  if (fd->mi_rows != cm->mi_rows || fd->mi_cols != cm->mi_cols) {
    ifd_clear(fd);
    ifd_init_mi_rc(fd, cm->mi_rows, cm->mi_cols);
  }
  fd->show_frame = cm->show_frame;
  fd->frame_type = cm->frame_type;
  fd->base_qindex = cm->base_qindex;
  fd->tile_mi_cols = cm->tile_width;
  fd->tile_mi_rows = cm->tile_height;
#if CONFIG_ACCOUNTING
  fd->accounting = &pbi->accounting;
#endif
#if CONFIG_CDEF
// TODO(negge): copy per frame CDEF data
#endif
  int i, j;
  for (i = 0; i < MAX_SEGMENTS; i++) {
    for (j = 0; j < 2; j++) {
      fd->y_dequant[i][j] = cm->y_dequant[i][j];
      fd->uv_dequant[i][j] = cm->uv_dequant[i][j];
    }
  }
  for (j = 0; j < cm->mi_rows; j++) {
    for (i = 0; i < cm->mi_cols; i++) {
      const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[j * cm->mi_stride + i]->mbmi;
      insp_mi_data *mi = &fd->mi_grid[j * cm->mi_cols + i];
      // Segment
      mi->segment_id = mbmi->segment_id;
      // Motion Vectors
      mi->mv[0].row = mbmi->mv[0].as_mv.row;
      mi->mv[0].col = mbmi->mv[0].as_mv.col;
      mi->mv[1].row = mbmi->mv[1].as_mv.row;
      mi->mv[1].col = mbmi->mv[1].as_mv.col;
      // Reference Frames
      mi->ref_frame[0] = mbmi->ref_frame[0];
      mi->ref_frame[1] = mbmi->ref_frame[1];
      // Prediction Mode
      mi->mode = mbmi->mode;
      // Prediction Mode for Chromatic planes
      if (mi->mode < INTRA_MODES) {
        mi->uv_mode = mbmi->uv_mode;
      } else {
        mi->uv_mode = UV_MODE_INVALID;
      }
      // Block Size
      mi->sb_type = mbmi->sb_type;
      // Skip Flag
      mi->skip = mbmi->skip;
#if CONFIG_DUAL_FILTER
      mi->filter[0] = av1_extract_interp_filter(mbmi->interp_filters, 0);
      mi->filter[1] = av1_extract_interp_filter(mbmi->interp_filters, 1);
#else
      mi->filter = av1_extract_interp_filter(mbmi->interp_filters, 0);
#endif
      // Transform
      mi->tx_type = mbmi->tx_type;
      mi->tx_size = mbmi->tx_size;

#if CONFIG_CDEF
      mi->cdef_level =
          cm->cdef_strengths[mbmi->cdef_strength] / CDEF_SEC_STRENGTHS;
      mi->cdef_strength =
          cm->cdef_strengths[mbmi->cdef_strength] % CDEF_SEC_STRENGTHS;
      mi->cdef_strength += mi->cdef_strength == 3;
#endif
#if CONFIG_CFL
      if (mbmi->uv_mode == UV_CFL_PRED) {
        mi->cfl_alpha_idx = mbmi->cfl_alpha_idx;
        mi->cfl_alpha_sign = mbmi->cfl_alpha_signs;
      } else {
        mi->cfl_alpha_idx = 0;
        mi->cfl_alpha_sign = 0;
      }
#endif
    }
  }
  return 1;
}
