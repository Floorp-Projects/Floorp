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

#ifndef AV1_DECODER_DECODEMV_H_
#define AV1_DECODER_DECODEMV_H_

#include "aom_dsp/bitreader.h"

#include "av1/decoder/decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_read_mode_info(AV1Decoder *const pbi, MACROBLOCKD *xd,
#if CONFIG_SUPERTX
                        int supertx_enabled,
#endif

                        int mi_row, int mi_col, aom_reader *r, int x_mis,
                        int y_mis);

#ifdef __cplusplus
}  // extern "C"
#endif

void av1_read_tx_type(const AV1_COMMON *const cm, MACROBLOCKD *xd,
#if CONFIG_SUPERTX
                      int supertx_enabled,
#endif
#if CONFIG_TXK_SEL
                      int blk_row, int blk_col, int block, int plane,
                      TX_SIZE tx_size,
#endif
                      aom_reader *r);

#endif  // AV1_DECODER_DECODEMV_H_
