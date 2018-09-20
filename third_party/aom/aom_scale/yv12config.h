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

#ifndef AOM_AOM_SCALE_YV12CONFIG_H_
#define AOM_AOM_SCALE_YV12CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "config/aom_config.h"

#include "aom/aom_codec.h"
#include "aom/aom_frame_buffer.h"
#include "aom/aom_integer.h"

#define AOMINNERBORDERINPIXELS 160
#define AOM_INTERP_EXTEND 4

// TODO(jingning): Use unified inter predictor for encoder and
// decoder during the development process. Revisit the frame border
// to improve the decoder performance.
#if CONFIG_REDUCED_ENCODER_BORDER
#define AOM_BORDER_IN_PIXELS 160
#else
#define AOM_BORDER_IN_PIXELS 288
#endif  // CONFIG_REDUCED_ENCODER_BORDER

typedef struct yv12_buffer_config {
  union {
    struct {
      int y_width;
      int uv_width;
      int alpha_width;
    };
    int widths[3];
  };
  union {
    struct {
      int y_height;
      int uv_height;
      int alpha_height;
    };
    int heights[3];
  };
  union {
    struct {
      int y_crop_width;
      int uv_crop_width;
    };
    int crop_widths[2];
  };
  union {
    struct {
      int y_crop_height;
      int uv_crop_height;
    };
    int crop_heights[2];
  };
  union {
    struct {
      int y_stride;
      int uv_stride;
      int alpha_stride;
    };
    int strides[3];
  };
  union {
    struct {
      uint8_t *y_buffer;
      uint8_t *u_buffer;
      uint8_t *v_buffer;
      uint8_t *alpha_buffer;
    };
    uint8_t *buffers[4];
  };

  // Indicate whether y_buffer, u_buffer, and v_buffer points to the internally
  // allocated memory or external buffers.
  int use_external_reference_buffers;
  // This is needed to store y_buffer, u_buffer, and v_buffer when set reference
  // uses an external refernece, and restore those buffer pointers after the
  // external reference frame is no longer used.
  uint8_t *store_buf_adr[3];

  // If the frame is stored in a 16-bit buffer, this stores an 8-bit version
  // for use in global motion detection. It is allocated on-demand.
  uint8_t *y_buffer_8bit;
  int buf_8bit_valid;

  uint8_t *buffer_alloc;
  size_t buffer_alloc_sz;
  int border;
  size_t frame_size;
  int subsampling_x;
  int subsampling_y;
  unsigned int bit_depth;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  int monochrome;
  aom_chroma_sample_position_t chroma_sample_position;
  aom_color_range_t color_range;
  int render_width;
  int render_height;

  int corrupted;
  int flags;
} YV12_BUFFER_CONFIG;

#define YV12_FLAG_HIGHBITDEPTH 8

int aom_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                           int ss_x, int ss_y, int use_highbitdepth, int border,
                           int byte_alignment);

// Updates the yv12 buffer config with the frame buffer. |byte_alignment| must
// be a power of 2, from 32 to 1024. 0 sets legacy alignment. If cb is not
// NULL, then libaom is using the frame buffer callbacks to handle memory.
// If cb is not NULL, libaom will call cb with minimum size in bytes needed
// to decode the current frame. If cb is NULL, libaom will allocate memory
// internally to decode the current frame. Returns 0 on success. Returns < 0
// on failure.
int aom_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                             int ss_x, int ss_y, int use_highbitdepth,
                             int border, int byte_alignment,
                             aom_codec_frame_buffer_t *fb,
                             aom_get_frame_buffer_cb_fn_t cb, void *cb_priv);
int aom_free_frame_buffer(YV12_BUFFER_CONFIG *ybf);

#ifdef __cplusplus
}
#endif

#endif  // AOM_AOM_SCALE_YV12CONFIG_H_
