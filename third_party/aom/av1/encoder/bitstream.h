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

#ifndef AV1_ENCODER_BITSTREAM_H_
#define AV1_ENCODER_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

struct aom_write_bit_buffer;

void write_sequence_header(AV1_COMP *cpi, struct aom_write_bit_buffer *wb);

uint32_t write_obu_header(OBU_TYPE obu_type, int obu_extension,
                          uint8_t *const dst);

int write_uleb_obu_size(uint32_t obu_header_size, uint32_t obu_payload_size,
                        uint8_t *dest);

int av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dest, size_t *size);

static INLINE int av1_preserve_existing_gf(AV1_COMP *cpi) {
  // Do not swap gf and arf indices for internal overlay frames
  return !cpi->multi_arf_allowed && cpi->rc.is_src_frame_alt_ref &&
         !cpi->rc.is_src_frame_ext_arf;
}

void av1_write_tx_type(const AV1_COMMON *const cm, const MACROBLOCKD *xd,
                       int blk_row, int blk_col, int plane, TX_SIZE tx_size,
                       aom_writer *w);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_BITSTREAM_H_
