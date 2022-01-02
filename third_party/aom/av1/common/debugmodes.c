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

#include <stdio.h>

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"

static void log_frame_info(AV1_COMMON *cm, const char *str, FILE *f) {
  fprintf(f, "%s", str);
  fprintf(f, "(Frame %d, Show:%d, Q:%d): \n", cm->current_video_frame,
          cm->show_frame, cm->base_qindex);
}
/* This function dereferences a pointer to the mbmi structure
 * and uses the passed in member offset to print out the value of an integer
 * for each mbmi member value in the mi structure.
 */
static void print_mi_data(AV1_COMMON *cm, FILE *file, const char *descriptor,
                          size_t member_offset) {
  int mi_row, mi_col;
  MB_MODE_INFO **mi = cm->mi_grid_visible;
  int rows = cm->mi_rows;
  int cols = cm->mi_cols;
  char prefix = descriptor[0];

  log_frame_info(cm, descriptor, file);
  for (mi_row = 0; mi_row < rows; mi_row++) {
    fprintf(file, "%c ", prefix);
    for (mi_col = 0; mi_col < cols; mi_col++) {
      fprintf(file, "%2d ", *((char *)((char *)(mi[0]) + member_offset)));
      mi++;
    }
    fprintf(file, "\n");
    mi += MAX_MIB_SIZE;
  }
  fprintf(file, "\n");
}

void av1_print_modes_and_motion_vectors(AV1_COMMON *cm, const char *file) {
  int mi_row;
  int mi_col;
  FILE *mvs = fopen(file, "a");
  MB_MODE_INFO **mi = cm->mi_grid_visible;
  int rows = cm->mi_rows;
  int cols = cm->mi_cols;

  print_mi_data(cm, mvs, "Partitions:", offsetof(MB_MODE_INFO, sb_type));
  print_mi_data(cm, mvs, "Modes:", offsetof(MB_MODE_INFO, mode));
  print_mi_data(cm, mvs, "Ref frame:", offsetof(MB_MODE_INFO, ref_frame[0]));
  print_mi_data(cm, mvs, "Transform:", offsetof(MB_MODE_INFO, tx_size));
  print_mi_data(cm, mvs, "UV Modes:", offsetof(MB_MODE_INFO, uv_mode));

  // output skip infomation.
  log_frame_info(cm, "Skips:", mvs);
  for (mi_row = 0; mi_row < rows; mi_row++) {
    fprintf(mvs, "S ");
    for (mi_col = 0; mi_col < cols; mi_col++) {
      fprintf(mvs, "%2d ", mi[0]->skip);
      mi++;
    }
    fprintf(mvs, "\n");
    mi += MAX_MIB_SIZE;
  }
  fprintf(mvs, "\n");

  // output motion vectors.
  log_frame_info(cm, "Vectors ", mvs);
  mi = cm->mi_grid_visible;
  for (mi_row = 0; mi_row < rows; mi_row++) {
    fprintf(mvs, "V ");
    for (mi_col = 0; mi_col < cols; mi_col++) {
      fprintf(mvs, "%4d:%4d ", mi[0]->mv[0].as_mv.row, mi[0]->mv[0].as_mv.col);
      mi++;
    }
    fprintf(mvs, "\n");
    mi += MAX_MIB_SIZE;
  }
  fprintf(mvs, "\n");

  fclose(mvs);
}

void av1_print_uncompressed_frame_header(const uint8_t *data, int size,
                                         const char *filename) {
  FILE *hdrFile = fopen(filename, "w");
  fwrite(data, size, sizeof(uint8_t), hdrFile);
  fclose(hdrFile);
}

void av1_print_frame_contexts(const FRAME_CONTEXT *fc, const char *filename) {
  FILE *fcFile = fopen(filename, "w");
  const uint16_t *fcp = (uint16_t *)fc;
  const unsigned int n_contexts = sizeof(FRAME_CONTEXT) / sizeof(uint16_t);
  unsigned int i;

  for (i = 0; i < n_contexts; ++i) fprintf(fcFile, "%d ", *fcp++);
  fclose(fcFile);
}
