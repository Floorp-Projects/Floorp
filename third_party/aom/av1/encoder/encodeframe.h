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

#ifndef AV1_ENCODER_ENCODEFRAME_H_
#define AV1_ENCODER_ENCODEFRAME_H_

#include "aom/aom_integer.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct macroblock;
struct yv12_buffer_config;
struct AV1_COMP;
struct ThreadData;

void av1_setup_src_planes(struct macroblock *x,
                          const struct yv12_buffer_config *src, int mi_row,
                          int mi_col);

void av1_encode_frame(struct AV1_COMP *cpi);

void av1_init_tile_data(struct AV1_COMP *cpi);
void av1_encode_tile(struct AV1_COMP *cpi, struct ThreadData *td, int tile_row,
                     int tile_col);

void av1_update_tx_type_count(const struct AV1Common *cm, MACROBLOCKD *xd,
#if CONFIG_TXK_SEL
                              int blk_row, int blk_col, int block, int plane,
#endif
                              BLOCK_SIZE bsize, TX_SIZE tx_size,
                              FRAME_COUNTS *counts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEFRAME_H_
