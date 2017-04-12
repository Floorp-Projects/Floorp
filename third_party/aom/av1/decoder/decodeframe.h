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

#ifndef AV1_DECODER_DECODEFRAME_H_
#define AV1_DECODER_DECODEFRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

struct AV1Decoder;
struct aom_read_bit_buffer;

#if CONFIG_REFERENCE_BUFFER
/* Placeholder for now */
void read_sequence_header(SequenceHeader *seq_params);
#endif

int av1_read_sync_code(struct aom_read_bit_buffer *const rb);
void av1_read_frame_size(struct aom_read_bit_buffer *rb, int *width,
                         int *height);
BITSTREAM_PROFILE av1_read_profile(struct aom_read_bit_buffer *rb);

void av1_decode_frame(struct AV1Decoder *pbi, const uint8_t *data,
                      const uint8_t *data_end, const uint8_t **p_data_end);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_DECODER_DECODEFRAME_H_
