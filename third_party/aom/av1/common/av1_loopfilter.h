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

#ifndef AOM_AV1_COMMON_AV1_LOOPFILTER_H_
#define AOM_AV1_COMMON_AV1_LOOPFILTER_H_

#include "config/aom_config.h"

#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/seg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOOP_FILTER 63
#define MAX_SHARPNESS 7

#define SIMD_WIDTH 16

enum lf_path {
  LF_PATH_420,
  LF_PATH_444,
  LF_PATH_SLOW,
};

#if LOOP_FILTER_BITMASK
typedef struct {
  uint64_t bits[4];
} FilterMask;

// This structure holds bit masks for all 4x4 blocks in a 64x64 region.
// Each 1 bit represents a position in which we want to apply the loop filter.
// For Y plane, 4x4 in 64x64 requires 16x16 = 256 bit, therefore we use 4
// uint64_t; For U, V plane, for 420 format, plane size is 32x32, thus we use
// a uint64_t to represent bitmask.
// Left_ entries refer to whether we apply a filter on the border to the
// left of the block.   Above_ entries refer to whether or not to apply a
// filter on the above border.
// Since each transform is accompanied by a potentially different type of
// loop filter there is a different entry in the array for each transform size.
typedef struct {
  FilterMask left_y[TX_SIZES];
  FilterMask above_y[TX_SIZES];
  FilterMask left_u[TX_SIZES];
  FilterMask above_u[TX_SIZES];
  FilterMask left_v[TX_SIZES];
  FilterMask above_v[TX_SIZES];

  // Y plane vertical edge and horizontal edge filter level
  uint8_t lfl_y_hor[MI_SIZE_64X64][MI_SIZE_64X64];
  uint8_t lfl_y_ver[MI_SIZE_64X64][MI_SIZE_64X64];

  // U plane filter level
  uint8_t lfl_u[MI_SIZE_64X64][MI_SIZE_64X64];

  // V plane filter level
  uint8_t lfl_v[MI_SIZE_64X64][MI_SIZE_64X64];

  // other info
  FilterMask skip;
  FilterMask is_vert_border;
  FilterMask is_horz_border;
  // Y or UV planes, 5 tx sizes: 4x4, 8x8, 16x16, 32x32, 64x64
  FilterMask tx_size_ver[2][5];
  FilterMask tx_size_hor[2][5];
} LoopFilterMask;
#endif  // LOOP_FILTER_BITMASK

struct loopfilter {
  int filter_level[2];
  int filter_level_u;
  int filter_level_v;

  int sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, Last2+Last3,
  // GF, BRF, ARF2, ARF
  int8_t ref_deltas[REF_FRAMES];

  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];

  int combine_vert_horz_lf;

#if LOOP_FILTER_BITMASK
  LoopFilterMask *lfm;
  size_t lfm_num;
  int lfm_stride;
#endif  // LOOP_FILTER_BITMASK
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
  uint8_t lvl[MAX_MB_PLANE][MAX_SEGMENTS][2][REF_FRAMES][MAX_MODE_LF_DELTAS];
} loop_filter_info_n;

/* assorted loopfilter functions which get used elsewhere */
struct AV1Common;
struct macroblockd;
struct AV1LfSyncData;

void av1_loop_filter_init(struct AV1Common *cm);

void av1_loop_filter_frame_init(struct AV1Common *cm, int plane_start,
                                int plane_end);

#if LOOP_FILTER_BITMASK
void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                           struct macroblockd *mbd, int is_decoding,
                           int plane_start, int plane_end, int partial_frame);
#else
void av1_loop_filter_frame(YV12_BUFFER_CONFIG *frame, struct AV1Common *cm,
                           struct macroblockd *mbd, int plane_start,
                           int plane_end, int partial_frame);
#endif

void av1_filter_block_plane_vert(const struct AV1Common *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col);

void av1_filter_block_plane_horz(const struct AV1Common *const cm,
                                 const MACROBLOCKD *const xd, const int plane,
                                 const MACROBLOCKD_PLANE *const plane_ptr,
                                 const uint32_t mi_row, const uint32_t mi_col);

typedef struct LoopFilterWorkerData {
  YV12_BUFFER_CONFIG *frame_buffer;
  struct AV1Common *cm;
  struct macroblockd_plane planes[MAX_MB_PLANE];
  // TODO(Ranjit): When the filter functions are modified to use xd->lossless
  // add lossless as a member here.
  MACROBLOCKD *xd;
} LFWorkerData;

uint8_t get_filter_level(const struct AV1Common *cm,
                         const loop_filter_info_n *lfi_n, const int dir_idx,
                         int plane, const MB_MODE_INFO *mbmi);
#if LOOP_FILTER_BITMASK
void av1_setup_bitmask(struct AV1Common *const cm, int mi_row, int mi_col,
                       int plane, int subsampling_x, int subsampling_y,
                       int row_end, int col_end);

void av1_filter_block_plane_ver(struct AV1Common *const cm,
                                struct macroblockd_plane *const plane_ptr,
                                int pl, int mi_row, int mi_col);

void av1_filter_block_plane_hor(struct AV1Common *const cm,
                                struct macroblockd_plane *const plane, int pl,
                                int mi_row, int mi_col);
LoopFilterMask *get_loop_filter_mask(const struct AV1Common *const cm,
                                     int mi_row, int mi_col);
int get_index_shift(int mi_col, int mi_row, int *index);

static const FilterMask left_txform_mask[TX_SIZES] = {
  { { 0x0000000000000001ULL,  // TX_4X4,
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0000000000010001ULL,  // TX_8X8,
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0001000100010001ULL,  // TX_16X16,
      0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0001000100010001ULL,  // TX_32X32,
      0x0001000100010001ULL, 0x0000000000000000ULL, 0x0000000000000000ULL } },

  { { 0x0001000100010001ULL,  // TX_64X64,
      0x0001000100010001ULL, 0x0001000100010001ULL, 0x0001000100010001ULL } },
};

static const uint64_t above_txform_mask[2][TX_SIZES] = {
  {
      0x0000000000000001ULL,  // TX_4X4
      0x0000000000000003ULL,  // TX_8X8
      0x000000000000000fULL,  // TX_16X16
      0x00000000000000ffULL,  // TX_32X32
      0x000000000000ffffULL,  // TX_64X64
  },
  {
      0x0000000000000001ULL,  // TX_4X4
      0x0000000000000005ULL,  // TX_8X8
      0x0000000000000055ULL,  // TX_16X16
      0x0000000000005555ULL,  // TX_32X32
      0x0000000055555555ULL,  // TX_64X64
  },
};

extern const int mask_id_table_tx_4x4[BLOCK_SIZES_ALL];

extern const int mask_id_table_tx_8x8[BLOCK_SIZES_ALL];

extern const int mask_id_table_tx_16x16[BLOCK_SIZES_ALL];

extern const int mask_id_table_tx_32x32[BLOCK_SIZES_ALL];

extern const FilterMask left_mask_univariant_reordered[67];

extern const FilterMask above_mask_univariant_reordered[67];
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_AV1_LOOPFILTER_H_
