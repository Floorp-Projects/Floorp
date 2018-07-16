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

// Reads the middle part of the sequence header OBU (from
// frame_width_bits_minus_1 to enable_restoration) into cm->seq_params (a
// SequenceHeader). Reports errors by calling rb->error_handler() or
// aom_internal_error().
void read_sequence_header(AV1_COMMON *cm, struct aom_read_bit_buffer *rb);

void av1_read_frame_size(struct aom_read_bit_buffer *rb, int num_bits_width,
                         int num_bits_height, int *width, int *height);
BITSTREAM_PROFILE av1_read_profile(struct aom_read_bit_buffer *rb);

// Returns 0 on success. Sets pbi->common.error.error_code and returns -1 on
// failure.
int av1_check_trailing_bits(struct AV1Decoder *pbi,
                            struct aom_read_bit_buffer *rb);

int av1_decode_frame_headers_and_setup(struct AV1Decoder *pbi,
                                       struct aom_read_bit_buffer *rb,
                                       const uint8_t *data,
                                       const uint8_t **p_data_end,
                                       int trailing_bits_present);

void av1_decode_tg_tiles_and_wrapup(struct AV1Decoder *pbi, const uint8_t *data,
                                    const uint8_t *data_end,
                                    const uint8_t **p_data_end, int startTile,
                                    int endTile, int initialize_flag);

// Implements the color_config() function in the spec. Reports errors by
// calling rb->error_handler() or aom_internal_error().
void av1_read_color_config(AV1_COMMON *cm, struct aom_read_bit_buffer *rb,
                           int allow_lowbitdepth);

// Implements the timing_info() function in the spec. Reports errors by calling
// rb->error_handler().
void av1_read_timing_info_header(AV1_COMMON *cm,
                                 struct aom_read_bit_buffer *rb);

// Implements the decoder_model_info() function in the spec. Reports errors by
// calling rb->error_handler().
void av1_read_decoder_model_info(AV1_COMMON *cm,
                                 struct aom_read_bit_buffer *rb);

// Implements the operating_parameters_info() function in the spec. Reports
// errors by calling rb->error_handler() or aom_internal_error().
void av1_read_op_parameters_info(AV1_COMMON *const cm,
                                 struct aom_read_bit_buffer *rb, int op_num);

struct aom_read_bit_buffer *av1_init_read_bit_buffer(
    struct AV1Decoder *pbi, struct aom_read_bit_buffer *rb, const uint8_t *data,
    const uint8_t *data_end);

void av1_free_mc_tmp_buf(void *td, int use_highbd);

void av1_set_single_tile_decoding_mode(AV1_COMMON *const cm);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_DECODER_DECODEFRAME_H_
