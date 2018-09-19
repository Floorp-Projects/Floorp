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

#ifndef AOM_AV1_ENCODER_BITSTREAM_H_
#define AOM_AV1_ENCODER_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

struct aom_write_bit_buffer;

// Writes only the OBU Sequence Header payload, and returns the size of the
// payload written to 'dst'. This function does not write the OBU header, the
// optional extension, or the OBU size to 'dst'.
uint32_t write_sequence_header_obu(AV1_COMP *cpi, uint8_t *const dst);

// Writes the OBU header byte, and the OBU header extension byte when
// 'obu_extension' is non-zero. Returns number of bytes written to 'dst'.
uint32_t write_obu_header(OBU_TYPE obu_type, int obu_extension,
                          uint8_t *const dst);

int write_uleb_obu_size(uint32_t obu_header_size, uint32_t obu_payload_size,
                        uint8_t *dest);

int av1_pack_bitstream(AV1_COMP *const cpi, uint8_t *dest, size_t *size);

static INLINE int av1_preserve_existing_gf(AV1_COMP *cpi) {
  // Do not swap gf and arf indices for internal overlay frames
  return cpi->rc.is_src_frame_alt_ref && !cpi->rc.is_src_frame_ext_arf;
}

void av1_write_tx_type(const AV1_COMMON *const cm, const MACROBLOCKD *xd,
                       int blk_row, int blk_col, int plane, TX_SIZE tx_size,
                       aom_writer *w);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_BITSTREAM_H_
