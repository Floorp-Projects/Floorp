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

#include <assert.h>

#include "config/aom_config.h"

#include "aom/aom_codec.h"
#include "aom_dsp/bitreader_buffer.h"
#include "aom_ports/mem_ops.h"

#include "av1/common/common.h"
#include "av1/common/timing.h"
#include "av1/decoder/decoder.h"
#include "av1/decoder/decodeframe.h"
#include "av1/decoder/obu.h"

// Picture prediction structures (0-12 are predefined) in scalability metadata.
typedef enum {
  SCALABILITY_L1T2 = 0,
  SCALABILITY_L1T3 = 1,
  SCALABILITY_L2T1 = 2,
  SCALABILITY_L2T2 = 3,
  SCALABILITY_L2T3 = 4,
  SCALABILITY_S2T1 = 5,
  SCALABILITY_S2T2 = 6,
  SCALABILITY_S2T3 = 7,
  SCALABILITY_L2T1h = 8,
  SCALABILITY_L2T2h = 9,
  SCALABILITY_L2T3h = 10,
  SCALABILITY_S2T1h = 11,
  SCALABILITY_S2T2h = 12,
  SCALABILITY_S2T3h = 13,
  SCALABILITY_SS = 14
} SCALABILITY_STRUCTURES;

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type) {
  int valid_type = 0;
  switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
    case OBU_TEMPORAL_DELIMITER:
    case OBU_FRAME_HEADER:
    case OBU_TILE_GROUP:
    case OBU_METADATA:
    case OBU_FRAME:
    case OBU_REDUNDANT_FRAME_HEADER:
    case OBU_TILE_LIST:
    case OBU_PADDING: valid_type = 1; break;
    default: break;
  }
  return valid_type;
}

// Parses OBU header and stores values in 'header'.
static aom_codec_err_t read_obu_header(struct aom_read_bit_buffer *rb,
                                       int is_annexb, ObuHeader *header) {
  if (!rb || !header) return AOM_CODEC_INVALID_PARAM;

  const ptrdiff_t bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
  if (bit_buffer_byte_length < 1) return AOM_CODEC_CORRUPT_FRAME;

  header->size = 1;

  if (aom_rb_read_bit(rb) != 0) {
    // Forbidden bit. Must not be set.
    return AOM_CODEC_CORRUPT_FRAME;
  }

  header->type = (OBU_TYPE)aom_rb_read_literal(rb, 4);

  if (!valid_obu_type(header->type)) return AOM_CODEC_CORRUPT_FRAME;

  header->has_extension = aom_rb_read_bit(rb);
  header->has_size_field = aom_rb_read_bit(rb);

  if (!header->has_size_field && !is_annexb) {
    // section 5 obu streams must have obu_size field set.
    return AOM_CODEC_UNSUP_BITSTREAM;
  }

  if (aom_rb_read_bit(rb) != 0) {
    // obu_reserved_1bit must be set to 0.
    return AOM_CODEC_CORRUPT_FRAME;
  }

  if (header->has_extension) {
    if (bit_buffer_byte_length == 1) return AOM_CODEC_CORRUPT_FRAME;

    header->size += 1;
    header->temporal_layer_id = aom_rb_read_literal(rb, 3);
    header->spatial_layer_id = aom_rb_read_literal(rb, 2);
    if (aom_rb_read_literal(rb, 3) != 0) {
      // extension_header_reserved_3bits must be set to 0.
      return AOM_CODEC_CORRUPT_FRAME;
    }
  }

  return AOM_CODEC_OK;
}

aom_codec_err_t aom_read_obu_header(uint8_t *buffer, size_t buffer_length,
                                    size_t *consumed, ObuHeader *header,
                                    int is_annexb) {
  if (buffer_length < 1 || !consumed || !header) return AOM_CODEC_INVALID_PARAM;

  // TODO(tomfinegan): Set the error handler here and throughout this file, and
  // confirm parsing work done via aom_read_bit_buffer is successful.
  struct aom_read_bit_buffer rb = { buffer, buffer + buffer_length, 0, NULL,
                                    NULL };
  aom_codec_err_t parse_result = read_obu_header(&rb, is_annexb, header);
  if (parse_result == AOM_CODEC_OK) *consumed = header->size;
  return parse_result;
}

aom_codec_err_t aom_get_num_layers_from_operating_point_idc(
    int operating_point_idc, unsigned int *number_spatial_layers,
    unsigned int *number_temporal_layers) {
  // derive number of spatial/temporal layers from operating_point_idc

  if (!number_spatial_layers || !number_temporal_layers)
    return AOM_CODEC_INVALID_PARAM;

  if (operating_point_idc == 0) {
    *number_temporal_layers = 1;
    *number_spatial_layers = 1;
  } else {
    *number_spatial_layers = 0;
    *number_temporal_layers = 0;
    for (int j = 0; j < MAX_NUM_SPATIAL_LAYERS; j++) {
      *number_spatial_layers +=
          (operating_point_idc >> (j + MAX_NUM_TEMPORAL_LAYERS)) & 0x1;
    }
    for (int j = 0; j < MAX_NUM_TEMPORAL_LAYERS; j++) {
      *number_temporal_layers += (operating_point_idc >> j) & 0x1;
    }
  }

  return AOM_CODEC_OK;
}

static int is_obu_in_current_operating_point(AV1Decoder *pbi,
                                             ObuHeader obu_header) {
  if (!pbi->current_operating_point) {
    return 1;
  }

  if ((pbi->current_operating_point >> obu_header.temporal_layer_id) & 0x1 &&
      (pbi->current_operating_point >> (obu_header.spatial_layer_id + 8)) &
          0x1) {
    return 1;
  }
  return 0;
}

static uint32_t read_temporal_delimiter_obu() { return 0; }

// Returns a boolean that indicates success.
static int read_bitstream_level(BitstreamLevel *bl,
                                struct aom_read_bit_buffer *rb) {
  const uint8_t seq_level_idx = aom_rb_read_literal(rb, LEVEL_BITS);
  if (!is_valid_seq_level_idx(seq_level_idx)) return 0;
  bl->major = (seq_level_idx >> LEVEL_MINOR_BITS) + LEVEL_MAJOR_MIN;
  bl->minor = seq_level_idx & ((1 << LEVEL_MINOR_BITS) - 1);
  return 1;
}

// On success, sets pbi->sequence_header_ready to 1 and returns the number of
// bytes read from 'rb'.
// On failure, sets pbi->common.error.error_code and returns 0.
static uint32_t read_sequence_header_obu(AV1Decoder *pbi,
                                         struct aom_read_bit_buffer *rb) {
  AV1_COMMON *const cm = &pbi->common;
  const uint32_t saved_bit_offset = rb->bit_offset;

  // Verify rb has been configured to report errors.
  assert(rb->error_handler);

  cm->profile = av1_read_profile(rb);
  if (cm->profile > PROFILE_2) {
    cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
    return 0;
  }

  SequenceHeader *const seq_params = &cm->seq_params;

  // Still picture or not
  seq_params->still_picture = aom_rb_read_bit(rb);
  seq_params->reduced_still_picture_hdr = aom_rb_read_bit(rb);
  // Video must have reduced_still_picture_hdr = 0
  if (!seq_params->still_picture && seq_params->reduced_still_picture_hdr) {
    cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
    return 0;
  }

  if (seq_params->reduced_still_picture_hdr) {
    cm->timing_info_present = 0;
    seq_params->decoder_model_info_present_flag = 0;
    seq_params->display_model_info_present_flag = 0;
    seq_params->operating_points_cnt_minus_1 = 0;
    seq_params->operating_point_idc[0] = 0;
    if (!read_bitstream_level(&seq_params->level[0], rb)) {
      cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
      return 0;
    }
    seq_params->tier[0] = 0;
    cm->op_params[0].decoder_model_param_present_flag = 0;
    cm->op_params[0].display_model_param_present_flag = 0;
  } else {
    cm->timing_info_present = aom_rb_read_bit(rb);  // timing_info_present_flag
    if (cm->timing_info_present) {
      av1_read_timing_info_header(cm, rb);

      seq_params->decoder_model_info_present_flag = aom_rb_read_bit(rb);
      if (seq_params->decoder_model_info_present_flag)
        av1_read_decoder_model_info(cm, rb);
    } else {
      seq_params->decoder_model_info_present_flag = 0;
    }
    seq_params->display_model_info_present_flag = aom_rb_read_bit(rb);
    seq_params->operating_points_cnt_minus_1 =
        aom_rb_read_literal(rb, OP_POINTS_CNT_MINUS_1_BITS);
    for (int i = 0; i < seq_params->operating_points_cnt_minus_1 + 1; i++) {
      seq_params->operating_point_idc[i] =
          aom_rb_read_literal(rb, OP_POINTS_IDC_BITS);
      if (!read_bitstream_level(&seq_params->level[i], rb)) {
        cm->error.error_code = AOM_CODEC_UNSUP_BITSTREAM;
        return 0;
      }
      // This is the seq_level_idx[i] > 7 check in the spec. seq_level_idx 7
      // is equivalent to level 3.3.
      if (seq_params->level[i].major > 3)
        seq_params->tier[i] = aom_rb_read_bit(rb);
      else
        seq_params->tier[i] = 0;
      if (seq_params->decoder_model_info_present_flag) {
        cm->op_params[i].decoder_model_param_present_flag = aom_rb_read_bit(rb);
        if (cm->op_params[i].decoder_model_param_present_flag)
          av1_read_op_parameters_info(cm, rb, i);
      } else {
        cm->op_params[i].decoder_model_param_present_flag = 0;
      }
      if (cm->timing_info_present &&
          (cm->timing_info.equal_picture_interval ||
           cm->op_params[i].decoder_model_param_present_flag)) {
        cm->op_params[i].bitrate = max_level_bitrate(
            cm->profile, major_minor_to_seq_level_idx(seq_params->level[i]),
            seq_params->tier[i]);
        // Level with seq_level_idx = 31 returns a high "dummy" bitrate to pass
        // the check
        if (cm->op_params[i].bitrate == 0)
          aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                             "AV1 does not support this combination of "
                             "profile, level, and tier.");
        // Buffer size in bits/s is bitrate in bits/s * 1 s
        cm->op_params[i].buffer_size = cm->op_params[i].bitrate;
      }
      if (cm->timing_info_present && cm->timing_info.equal_picture_interval &&
          !cm->op_params[i].decoder_model_param_present_flag) {
        // When the decoder_model_parameters are not sent for this op, set
        // the default ones that can be used with the resource availability mode
        cm->op_params[i].decoder_buffer_delay = 70000;
        cm->op_params[i].encoder_buffer_delay = 20000;
        cm->op_params[i].low_delay_mode_flag = 0;
      }

      if (seq_params->display_model_info_present_flag) {
        cm->op_params[i].display_model_param_present_flag = aom_rb_read_bit(rb);
        if (cm->op_params[i].display_model_param_present_flag) {
          cm->op_params[i].initial_display_delay =
              aom_rb_read_literal(rb, 4) + 1;
          if (cm->op_params[i].initial_display_delay > 10)
            aom_internal_error(
                &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                "AV1 does not support more than 10 decoded frames delay");
        } else {
          cm->op_params[i].initial_display_delay = 10;
        }
      } else {
        cm->op_params[i].display_model_param_present_flag = 0;
        cm->op_params[i].initial_display_delay = 10;
      }
    }
  }
  // This decoder supports all levels.  Choose operating point provided by
  // external means
  int operating_point = pbi->operating_point;
  if (operating_point < 0 ||
      operating_point > seq_params->operating_points_cnt_minus_1)
    operating_point = 0;
  pbi->current_operating_point =
      seq_params->operating_point_idc[operating_point];
  if (aom_get_num_layers_from_operating_point_idc(
          pbi->current_operating_point, &cm->number_spatial_layers,
          &cm->number_temporal_layers) != AOM_CODEC_OK) {
    cm->error.error_code = AOM_CODEC_ERROR;
    return 0;
  }

  read_sequence_header(cm, rb);

  av1_read_color_config(cm, rb, pbi->allow_lowbitdepth);

  cm->film_grain_params_present = aom_rb_read_bit(rb);

  if (av1_check_trailing_bits(pbi, rb) != 0) {
    // cm->error.error_code is already set.
    return 0;
  }

  pbi->sequence_header_ready = 1;

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_frame_header_obu(AV1Decoder *pbi,
                                      struct aom_read_bit_buffer *rb,
                                      const uint8_t *data,
                                      const uint8_t **p_data_end,
                                      int trailing_bits_present) {
  av1_decode_frame_headers_and_setup(pbi, rb, data, p_data_end,
                                     trailing_bits_present);
  return (uint32_t)(pbi->uncomp_hdr_size);
}

static int32_t read_tile_group_header(AV1Decoder *pbi,
                                      struct aom_read_bit_buffer *rb,
                                      int *start_tile, int *end_tile,
                                      int tile_start_implicit) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t saved_bit_offset = rb->bit_offset;
  int tile_start_and_end_present_flag = 0;
  const int num_tiles = pbi->common.tile_rows * pbi->common.tile_cols;

  if (!pbi->common.large_scale_tile && num_tiles > 1) {
    tile_start_and_end_present_flag = aom_rb_read_bit(rb);
  }
  if (pbi->common.large_scale_tile || num_tiles == 1 ||
      !tile_start_and_end_present_flag) {
    *start_tile = 0;
    *end_tile = num_tiles - 1;
    return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
  }
  if (tile_start_implicit && tile_start_and_end_present_flag) {
    aom_internal_error(
        &cm->error, AOM_CODEC_UNSUP_BITSTREAM,
        "For OBU_FRAME type obu tile_start_and_end_present_flag must be 0");
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return -1;
  }
  *start_tile =
      aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);
  *end_tile = aom_rb_read_literal(rb, cm->log2_tile_rows + cm->log2_tile_cols);

  return ((rb->bit_offset - saved_bit_offset + 7) >> 3);
}

static uint32_t read_one_tile_group_obu(
    AV1Decoder *pbi, struct aom_read_bit_buffer *rb, int is_first_tg,
    const uint8_t *data, const uint8_t *data_end, const uint8_t **p_data_end,
    int *is_last_tg, int tile_start_implicit) {
  AV1_COMMON *const cm = &pbi->common;
  int start_tile, end_tile;
  int32_t header_size, tg_payload_size;

  header_size = read_tile_group_header(pbi, rb, &start_tile, &end_tile,
                                       tile_start_implicit);
  if (header_size == -1) return 0;
  if (start_tile > end_tile) return header_size;
  data += header_size;
  av1_decode_tg_tiles_and_wrapup(pbi, data, data_end, p_data_end, start_tile,
                                 end_tile, is_first_tg);

  tg_payload_size = (uint32_t)(*p_data_end - data);

  // TODO(shan):  For now, assume all tile groups received in order
  *is_last_tg = end_tile == cm->tile_rows * cm->tile_cols - 1;
  return header_size + tg_payload_size;
}

// Only called while large_scale_tile = 1.
static uint32_t read_and_decode_one_tile_list(AV1Decoder *pbi,
                                              struct aom_read_bit_buffer *rb,
                                              const uint8_t *data,
                                              const uint8_t *data_end,
                                              const uint8_t **p_data_end,
                                              int *frame_decoding_finished) {
  AV1_COMMON *const cm = &pbi->common;
  uint32_t tile_list_payload_size = 0;
  const int num_tiles = cm->tile_cols * cm->tile_rows;
  const int start_tile = 0;
  const int end_tile = num_tiles - 1;
  int i = 0;

  // Process the tile list info.
  pbi->output_frame_width_in_tiles_minus_1 = aom_rb_read_literal(rb, 8);
  pbi->output_frame_height_in_tiles_minus_1 = aom_rb_read_literal(rb, 8);
  pbi->tile_count_minus_1 = aom_rb_read_literal(rb, 16);
  if (pbi->tile_count_minus_1 > 511) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return 0;
  }

  // Allocate output frame buffer for the tile list.
  // TODO(yunqing): for now, copy each tile's decoded YUV data directly to the
  // output buffer. This needs to be modified according to the application
  // requirement.
  const int tile_width_in_pixels = cm->tile_width * MI_SIZE;
  const int tile_height_in_pixels = cm->tile_height * MI_SIZE;
  const int ssy = cm->subsampling_y;
  const int ssx = cm->subsampling_x;
  const int num_planes = av1_num_planes(cm);
  const size_t yplane_tile_size = tile_height_in_pixels * tile_width_in_pixels;
  const size_t uvplane_tile_size =
      (num_planes > 1)
          ? (tile_height_in_pixels >> ssy) * (tile_width_in_pixels >> ssx)
          : 0;
  const size_t tile_size = (cm->use_highbitdepth ? 2 : 1) *
                           (yplane_tile_size + 2 * uvplane_tile_size);
  pbi->tile_list_size = tile_size * (pbi->tile_count_minus_1 + 1);

  if (pbi->tile_list_size > pbi->buffer_sz) {
    if (pbi->tile_list_output != NULL) aom_free(pbi->tile_list_output);
    pbi->tile_list_output = NULL;

    pbi->tile_list_output = (uint8_t *)aom_memalign(32, pbi->tile_list_size);
    if (pbi->tile_list_output == NULL)
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate the tile list output buffer");
    pbi->buffer_sz = pbi->tile_list_size;
  }

  uint32_t tile_list_info_bytes = 4;
  tile_list_payload_size += tile_list_info_bytes;
  data += tile_list_info_bytes;
  uint8_t *output = pbi->tile_list_output;

  for (i = 0; i <= pbi->tile_count_minus_1; i++) {
    // Process 1 tile.
    // Reset the bit reader.
    rb->bit_offset = 0;
    rb->bit_buffer = data;

    // Read out the tile info.
    uint32_t tile_info_bytes = 5;
    // Set reference for each tile.
    int ref_idx = aom_rb_read_literal(rb, 8);
    if (ref_idx >= MAX_EXTERNAL_REFERENCES) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return 0;
    }
    av1_set_reference_dec(cm, 0, 1, &pbi->ext_refs.refs[ref_idx]);

    pbi->dec_tile_row = aom_rb_read_literal(rb, 8);
    pbi->dec_tile_col = aom_rb_read_literal(rb, 8);
    if (pbi->dec_tile_row < 0 || pbi->dec_tile_col < 0 ||
        pbi->dec_tile_row >= cm->tile_rows ||
        pbi->dec_tile_col >= cm->tile_cols) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return 0;
    }

    pbi->coded_tile_data_size = aom_rb_read_literal(rb, 16) + 1;
    data += tile_info_bytes;
    if ((size_t)(data_end - data) < pbi->coded_tile_data_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return 0;
    }

    av1_decode_tg_tiles_and_wrapup(pbi, data, data + pbi->coded_tile_data_size,
                                   p_data_end, start_tile, end_tile, 0);
    uint32_t tile_payload_size = (uint32_t)(*p_data_end - data);

    tile_list_payload_size += tile_info_bytes + tile_payload_size;

    // Update data ptr for next tile decoding.
    data = *p_data_end;
    assert(data <= data_end);

    // Copy decoded tile to the tile list output buffer.
    YV12_BUFFER_CONFIG *cur_frame = get_frame_new_buffer(cm);
    const int mi_row = pbi->dec_tile_row * cm->tile_height;
    const int mi_col = pbi->dec_tile_col * cm->tile_width;
    const int is_hbd = (cur_frame->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
    uint8_t *bufs[MAX_MB_PLANE] = { NULL, NULL, NULL };
    int strides[MAX_MB_PLANE] = { 0, 0, 0 };
    int plane;

    for (plane = 0; plane < num_planes; ++plane) {
      int shift_x = plane > 0 ? ssx : 0;
      int shift_y = plane > 0 ? ssy : 0;

      bufs[plane] = cur_frame->buffers[plane];
      strides[plane] =
          (plane > 0) ? cur_frame->strides[1] : cur_frame->strides[0];
      if (is_hbd) {
        bufs[plane] = (uint8_t *)CONVERT_TO_SHORTPTR(cur_frame->buffers[plane]);
        strides[plane] =
            (plane > 0) ? 2 * cur_frame->strides[1] : 2 * cur_frame->strides[0];
      }

      bufs[plane] += mi_row * (MI_SIZE >> shift_y) * strides[plane] +
                     mi_col * (MI_SIZE >> shift_x);

      int w, h;
      w = (plane > 0 && shift_x > 0) ? ((tile_width_in_pixels + 1) >> shift_x)
                                     : tile_width_in_pixels;
      w *= (1 + is_hbd);
      h = (plane > 0 && shift_y > 0) ? ((tile_height_in_pixels + 1) >> shift_y)
                                     : tile_height_in_pixels;
      int j;

      for (j = 0; j < h; ++j) {
        memcpy(output, bufs[plane], w);
        bufs[plane] += strides[plane];
        output += w;
      }
    }
  }

  *frame_decoding_finished = 1;
  return tile_list_payload_size;
}

static void read_metadata_itut_t35(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  for (size_t i = 0; i < sz; i++) {
    aom_rb_read_literal(&rb, 8);
  }
}

static void read_metadata_hdr_cll(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  aom_rb_read_literal(&rb, 16);  // max_cll
  aom_rb_read_literal(&rb, 16);  // max_fall
}

static void read_metadata_hdr_mdcv(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  for (int i = 0; i < 3; i++) {
    aom_rb_read_literal(&rb, 16);  // primary_i_chromaticity_x
    aom_rb_read_literal(&rb, 16);  // primary_i_chromaticity_y
  }

  aom_rb_read_literal(&rb, 16);  // white_point_chromaticity_x
  aom_rb_read_literal(&rb, 16);  // white_point_chromaticity_y

  aom_rb_read_unsigned_literal(&rb, 32);  // luminance_max
  aom_rb_read_unsigned_literal(&rb, 32);  // luminance_min
}

static void scalability_structure(struct aom_read_bit_buffer *rb) {
  int spatial_layers_cnt = aom_rb_read_literal(rb, 2);
  int spatial_layer_dimensions_present_flag = aom_rb_read_literal(rb, 1);
  int spatial_layer_description_present_flag = aom_rb_read_literal(rb, 1);
  int temporal_group_description_present_flag = aom_rb_read_literal(rb, 1);
  aom_rb_read_literal(rb, 3);  // reserved

  if (spatial_layer_dimensions_present_flag) {
    int i;
    for (i = 0; i < spatial_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 16);
      aom_rb_read_literal(rb, 16);
    }
  }
  if (spatial_layer_description_present_flag) {
    int i;
    for (i = 0; i < spatial_layers_cnt + 1; i++) {
      aom_rb_read_literal(rb, 8);
    }
  }
  if (temporal_group_description_present_flag) {
    int i, j, temporal_group_size;
    temporal_group_size = aom_rb_read_literal(rb, 8);
    for (i = 0; i < temporal_group_size; i++) {
      aom_rb_read_literal(rb, 3);
      aom_rb_read_literal(rb, 1);
      aom_rb_read_literal(rb, 1);
      int temporal_group_ref_cnt = aom_rb_read_literal(rb, 3);
      for (j = 0; j < temporal_group_ref_cnt; j++) {
        aom_rb_read_literal(rb, 8);
      }
    }
  }
}

static void read_metadata_scalability(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  int scalability_mode_idc = aom_rb_read_literal(&rb, 8);
  if (scalability_mode_idc == SCALABILITY_SS) {
    scalability_structure(&rb);
  }
}

static void read_metadata_timecode(const uint8_t *data, size_t sz) {
  struct aom_read_bit_buffer rb = { data, data + sz, 0, NULL, NULL };
  aom_rb_read_literal(&rb, 5);                     // counting_type f(5)
  int full_timestamp_flag = aom_rb_read_bit(&rb);  // full_timestamp_flag f(1)
  aom_rb_read_bit(&rb);                            // discontinuity_flag (f1)
  aom_rb_read_bit(&rb);                            // cnt_dropped_flag f(1)
  aom_rb_read_literal(&rb, 9);                     // n_frames f(9)
  if (full_timestamp_flag) {
    aom_rb_read_literal(&rb, 6);  // seconds_value f(6)
    aom_rb_read_literal(&rb, 6);  // minutes_value f(6)
    aom_rb_read_literal(&rb, 5);  // hours_value f(5)
  } else {
    int seconds_flag = aom_rb_read_bit(&rb);  // seconds_flag f(1)
    if (seconds_flag) {
      aom_rb_read_literal(&rb, 6);              // seconds_value f(6)
      int minutes_flag = aom_rb_read_bit(&rb);  // minutes_flag f(1)
      if (minutes_flag) {
        aom_rb_read_literal(&rb, 6);            // minutes_value f(6)
        int hours_flag = aom_rb_read_bit(&rb);  // hours_flag f(1)
        if (hours_flag) {
          aom_rb_read_literal(&rb, 5);  // hours_value f(5)
        }
      }
    }
  }
  // time_offset_length f(5)
  int time_offset_length = aom_rb_read_literal(&rb, 5);
  if (time_offset_length) {
    aom_rb_read_literal(&rb, time_offset_length);  // f(time_offset_length)
  }
}

static size_t read_metadata(const uint8_t *data, size_t sz) {
  size_t type_length;
  uint64_t type_value;
  OBU_METADATA_TYPE metadata_type;
  if (aom_uleb_decode(data, sz, &type_value, &type_length) < 0) {
    return sz;
  }
  metadata_type = (OBU_METADATA_TYPE)type_value;
  if (metadata_type == OBU_METADATA_TYPE_ITUT_T35) {
    read_metadata_itut_t35(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_CLL) {
    read_metadata_hdr_cll(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_HDR_MDCV) {
    read_metadata_hdr_mdcv(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_SCALABILITY) {
    read_metadata_scalability(data + type_length, sz - type_length);
  } else if (metadata_type == OBU_METADATA_TYPE_TIMECODE) {
    read_metadata_timecode(data + type_length, sz - type_length);
  }

  return sz;
}

static aom_codec_err_t read_obu_size(const uint8_t *data,
                                     size_t bytes_available,
                                     size_t *const obu_size,
                                     size_t *const length_field_size) {
  uint64_t u_obu_size = 0;
  if (aom_uleb_decode(data, bytes_available, &u_obu_size, length_field_size) !=
      0) {
    return AOM_CODEC_CORRUPT_FRAME;
  }

  if (u_obu_size > UINT32_MAX) return AOM_CODEC_CORRUPT_FRAME;
  *obu_size = (size_t)u_obu_size;
  return AOM_CODEC_OK;
}

aom_codec_err_t aom_read_obu_header_and_size(const uint8_t *data,
                                             size_t bytes_available,
                                             int is_annexb,
                                             ObuHeader *obu_header,
                                             size_t *const payload_size,
                                             size_t *const bytes_read) {
  size_t length_field_size = 0, obu_size = 0;
  aom_codec_err_t status;

  if (is_annexb) {
    // Size field comes before the OBU header, and includes the OBU header
    status =
        read_obu_size(data, bytes_available, &obu_size, &length_field_size);

    if (status != AOM_CODEC_OK) return status;
  }

  struct aom_read_bit_buffer rb = { data + length_field_size,
                                    data + bytes_available, 0, NULL, NULL };

  status = read_obu_header(&rb, is_annexb, obu_header);
  if (status != AOM_CODEC_OK) return status;

  if (is_annexb) {
    // Derive the payload size from the data we've already read
    if (obu_size < obu_header->size) return AOM_CODEC_CORRUPT_FRAME;

    *payload_size = obu_size - obu_header->size;
  } else {
    // Size field comes after the OBU header, and is just the payload size
    status = read_obu_size(data + obu_header->size,
                           bytes_available - obu_header->size, payload_size,
                           &length_field_size);
    if (status != AOM_CODEC_OK) return status;
  }

  *bytes_read = length_field_size + obu_header->size;
  return AOM_CODEC_OK;
}

#define EXT_TILE_DEBUG 0
// On success, returns a boolean that indicates whether the decoding of the
// current frame is finished. On failure, sets cm->error.error_code and
// returns -1.
int aom_decode_frame_from_obus(struct AV1Decoder *pbi, const uint8_t *data,
                               const uint8_t *data_end,
                               const uint8_t **p_data_end) {
  AV1_COMMON *const cm = &pbi->common;
  int frame_decoding_finished = 0;
  int is_first_tg_obu_received = 1;
  int frame_header_size = 0;
  int seq_header_received = 0;
  size_t seq_header_size = 0;
  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));
  pbi->seen_frame_header = 0;

  if (data_end < data) {
    cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
    return -1;
  }

  // Reset pbi->camera_frame_header_ready to 0 if cm->large_scale_tile = 0.
  if (!cm->large_scale_tile) pbi->camera_frame_header_ready = 0;

  // decode frame as a series of OBUs
  while (!frame_decoding_finished && !cm->error.error_code) {
    struct aom_read_bit_buffer rb;
    size_t payload_size = 0;
    size_t decoded_payload_size = 0;
    size_t obu_payload_offset = 0;
    size_t bytes_read = 0;
    const size_t bytes_available = data_end - data;

    if (bytes_available == 0 && !pbi->seen_frame_header) {
      *p_data_end = data;
      cm->error.error_code = AOM_CODEC_OK;
      break;
    }

    aom_codec_err_t status =
        aom_read_obu_header_and_size(data, bytes_available, cm->is_annexb,
                                     &obu_header, &payload_size, &bytes_read);

    if (status != AOM_CODEC_OK) {
      cm->error.error_code = status;
      return -1;
    }

    // Record obu size header information.
    pbi->obu_size_hdr.data = data + obu_header.size;
    pbi->obu_size_hdr.size = bytes_read - obu_header.size;

    // Note: aom_read_obu_header_and_size() takes care of checking that this
    // doesn't cause 'data' to advance past 'data_end'.
    data += bytes_read;

    if ((size_t)(data_end - data) < payload_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return -1;
    }

    cm->temporal_layer_id = obu_header.temporal_layer_id;
    cm->spatial_layer_id = obu_header.spatial_layer_id;

    if (obu_header.type != OBU_TEMPORAL_DELIMITER &&
        obu_header.type != OBU_SEQUENCE_HEADER &&
        obu_header.type != OBU_PADDING) {
      // don't decode obu if it's not in current operating mode
      if (!is_obu_in_current_operating_point(pbi, obu_header)) {
        data += payload_size;
        continue;
      }
    }

    av1_init_read_bit_buffer(pbi, &rb, data, data_end);

    switch (obu_header.type) {
      case OBU_TEMPORAL_DELIMITER:
        decoded_payload_size = read_temporal_delimiter_obu();
        pbi->seen_frame_header = 0;
        break;
      case OBU_SEQUENCE_HEADER:
        if (!seq_header_received) {
          decoded_payload_size = read_sequence_header_obu(pbi, &rb);
          if (cm->error.error_code != AOM_CODEC_OK) return -1;

          seq_header_size = decoded_payload_size;
          seq_header_received = 1;
        } else {
          // Seeing another sequence header, skip as all sequence headers are
          // required to be identical except for the contents of
          // operating_parameters_info and the amount of trailing bits.
          // TODO(yaowu): verifying redundant sequence headers are identical.
          decoded_payload_size = seq_header_size;
        }
        break;
      case OBU_FRAME_HEADER:
      case OBU_REDUNDANT_FRAME_HEADER:
      case OBU_FRAME:
        // Only decode first frame header received
        if (!pbi->seen_frame_header ||
            (cm->large_scale_tile && !pbi->camera_frame_header_ready)) {
          pbi->seen_frame_header = 1;
          frame_header_size = read_frame_header_obu(
              pbi, &rb, data, p_data_end, obu_header.type != OBU_FRAME);
          if (cm->large_scale_tile) pbi->camera_frame_header_ready = 1;
        }
        decoded_payload_size = frame_header_size;
        pbi->frame_header_size = (size_t)frame_header_size;

        if (cm->show_existing_frame) {
          frame_decoding_finished = 1;
          pbi->seen_frame_header = 0;
          break;
        }

#if !EXT_TILE_DEBUG
        // In large scale tile coding, decode the common camera frame header
        // before any tile list OBU.
        if (!pbi->ext_tile_debug && pbi->camera_frame_header_ready) {
          frame_decoding_finished = 1;
          // Skip the rest of the frame data.
          decoded_payload_size = payload_size;
          // Update data_end.
          *p_data_end = data_end;
          break;
        }
#endif  // EXT_TILE_DEBUG

        if (obu_header.type != OBU_FRAME) break;
        obu_payload_offset = frame_header_size;
        AOM_FALLTHROUGH_INTENDED;  // fall through to read tile group.
      case OBU_TILE_GROUP:
        if (!pbi->seen_frame_header) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return -1;
        }
        if ((size_t)(data_end - data) < obu_payload_offset) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return -1;
        }
        decoded_payload_size += read_one_tile_group_obu(
            pbi, &rb, is_first_tg_obu_received, data + obu_payload_offset,
            data + payload_size, p_data_end, &frame_decoding_finished,
            obu_header.type == OBU_FRAME);
        is_first_tg_obu_received = 0;
        if (frame_decoding_finished) pbi->seen_frame_header = 0;
        break;
      case OBU_METADATA:
        decoded_payload_size = read_metadata(data, payload_size);
        break;
      case OBU_TILE_LIST:
        // This OBU type is purely for the large scale tile coding mode.
        // The common camera frame header has to be already decoded.
        if (!pbi->camera_frame_header_ready) {
          cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
          return -1;
        }

        cm->large_scale_tile = 1;
        av1_set_single_tile_decoding_mode(cm);
        decoded_payload_size =
            read_and_decode_one_tile_list(pbi, &rb, data, data + payload_size,
                                          p_data_end, &frame_decoding_finished);
        if (cm->error.error_code != AOM_CODEC_OK) return -1;
        break;
      case OBU_PADDING:
      default:
        // Skip unrecognized OBUs
        decoded_payload_size = payload_size;
        break;
    }

    // Check that the signalled OBU size matches the actual amount of data read
    if (decoded_payload_size > payload_size) {
      cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
      return -1;
    }

    // If there are extra padding bytes, they should all be zero
    while (decoded_payload_size < payload_size) {
      uint8_t padding_byte = data[decoded_payload_size++];
      if (padding_byte != 0) {
        cm->error.error_code = AOM_CODEC_CORRUPT_FRAME;
        return -1;
      }
    }

    data += payload_size;
  }

  return frame_decoding_finished;
}
#undef EXT_TILE_DEBUG
