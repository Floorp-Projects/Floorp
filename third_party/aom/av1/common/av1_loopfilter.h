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

#ifndef AV1_COMMON_LOOPFILTER_H_
#define AV1_COMMON_LOOPFILTER_H_

#include "aom_ports/mem.h"
#include "./aom_config.h"

#include "av1/common/blockd.h"
#include "av1/common/seg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOOP_FILTER 63
#define MAX_SHARPNESS 7

#define SIMD_WIDTH 16

#define MAX_MODE_LF_DELTAS 2

enum lf_path {
  LF_PATH_420,
  LF_PATH_444,
  LF_PATH_SLOW,
};

struct loopfilter {
#if CONFIG_LOOPFILTER_LEVEL
  int filter_level[2];
  int filter_level_u;
  int filter_level_v;
#else
  int filter_level;
#endif

  int sharpness_level;
  int last_sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, Last2+Last3(CONFIG_EXT_REFS),
  // GF, BRF(CONFIG_EXT_REFS), ARF2(CONFIG_EXT_REFS), ARF
  int8_t ref_deltas[TOTAL_REFS_PER_FRAME];
  int8_t last_ref_deltas[TOTAL_REFS_PER_FRAME];

  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];
  int8_t last_mode_deltas[MAX_MODE_LF_DELTAS];
};

// Need to align this structure so when it is declared and
// passed it can be loaded into vector registers.
typedef struct {
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, mblim[SIMD_WIDTH]);
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, lim[SIMD_WIDTH]);
  DECLARE_ALIGNED(SIMD_WIDTH, uint8_t, hev_thr[SIMD_WIDTH]);
} loop_filter_thresh;

typedef struct {
  loop_filter_thresh lfthr[MAX_LOOP_FILTER + 1];
#if CONFIG_LOOPFILTER_LEVEL
  uint8_t lvl[MAX_SEGMENTS][2][TOTAL_REFS_PER_FRAME][MAX_MODE_LF_DELTAS];
#else
  uint8_t lvl[MAX_SEGMENTS][TOTAL_REFS_PER_FRAME][MAX_MODE_LF_DELTAS];
#endif
} loop_filter_info_n;

// This structure holds bit masks for all 8x8 blocks in a 64x64 region.
// Each 1 bit represents a position in which we want to apply the loop filter.
// Left_ entries refer to whether we apply a filter on the border to the
// left of the block.   Above_ entries refer to whether or not to apply a
// filter on the above border.   Int_ entries refer to whether or not to
// apply borders on the 4x4 edges within the 8x8 block that each bit
// represents.
// Since each transform is accompanied by a potentially different type of
// loop filter there is a different entry in the array for each transform size.
typedef struct {
  uint64_t left_y[TX_SIZES];
  uint64_t above_y[TX_SIZES];
  uint64_t int_4x4_y;
  uint16_t left_uv[TX_SIZES];
  uint16_t above_uv[TX_SIZES];
  uint16_t left_int_4x4_uv;
  uint16_t above_int_4x4_uv;
  uint8_t lfl_y[MAX_MIB_SIZE][MAX_MIB_SIZE];
  uint8_t lfl_uv[MAX_MIB_SIZE / 2][MAX_MIB_SIZE / 2];
} LOOP_FILTER_MASK;

/* assorted loopfilter functions which get used elsewhere */
struct AV1Common;
struct macroblockd;
struct AV1LfSyncData;

// This function sets up the bit masks for the entire 64x64 region represented
// by mi_row, mi_col.
void av1_setup_mask(struct AV1Common *const cm, const int mi_row,
                    const int mi_col, MODE_INFO **mi_8x8,
                    const int mode_info_stride, LOOP_FILTER_MASK *lfm);

void av1_filter_block_plane_ss00_ver(struct AV1Common *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm);
void av1_filter_block_plane_ss00_hor(struct AV1Common *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm);
void av1_filter_block_plane_ss11_ver(struct AV1Common *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm);
void av1_filter_block_plane_ss11_hor(struct AV1Common *const cm,
                                     struct macroblockd_plane *const plane,
                                     int mi_row, LOOP_FILTER_MASK *lfm);

void av1_filter_block_plane_non420_ver(struct AV1Common *const cm,
                                       struct macroblockd_plane *plane,
                                       MODE_INFO **mi_8x8, int mi_row,
                                       int mi_col, int pl);
void av1_filter_block_plane_non420_hor(struct AV1Common *const cm,
                                       struct macroblockd_plane *plane,
                                       MODE_INFO **mi_8x8, int mi_row,
                                       int mi_col, int pl);

void av1_loop_filter_init(struct AV1Common *cm);

// Update the loop filter for the current frame.
// This should be called before av1_loop_filter_rows(),
// av1_loop_filter_frame()
// calls this function directly.
void av1_loop_filter_frame_init(struct AV1Common *cm, int default_filt_lvl,
                                int default_filt_lvl_r
#if CONFIG_LOOPFILTER_LEVEL
                                ,
                                int plane
#endif
                                );

#if CONFIG_LPF_SB
void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                           struct macroblockd *mbd, int filter_level,
                           int y_only, int partial_frame, int mi_row,
                           int mi_col);

// Apply the loop filter to [start, stop) macro block rows in frame_buffer.
void av1_loop_filter_rows(YV12_BUFFER_CONFIG *frame_buffer,
                          struct AV1Common *cm,
                          struct macroblockd_plane *planes, int start, int stop,
                          int col_start, int col_end, int y_only);

void av1_loop_filter_sb_level_init(struct AV1Common *cm, int mi_row, int mi_col,
                                   int lvl);
#else
void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                           struct macroblockd *mbd, int filter_level,
#if CONFIG_LOOPFILTER_LEVEL
                           int filter_level_r,
#endif
                           int y_only, int partial_frame);

// Apply the loop filter to [start, stop) macro block rows in frame_buffer.
void av1_loop_filter_rows(YV12_BUFFER_CONFIG *frame_buffer,
                          struct AV1Common *cm,
                          struct macroblockd_plane *planes, int start, int stop,
                          int y_only);
#endif  // CONFIG_LPF_SB

typedef struct LoopFilterWorkerData {
  YV12_BUFFER_CONFIG *frame_buffer;
  struct AV1Common *cm;
  struct macroblockd_plane planes[MAX_MB_PLANE];

  int start;
  int stop;
  int y_only;
} LFWorkerData;

void av1_loop_filter_data_reset(LFWorkerData *lf_data,
                                YV12_BUFFER_CONFIG *frame_buffer,
                                struct AV1Common *cm,
                                const struct macroblockd_plane *planes);

// Operates on the rows described by 'lf_data'.
int av1_loop_filter_worker(LFWorkerData *const lf_data, void *unused);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_LOOPFILTER_H_
