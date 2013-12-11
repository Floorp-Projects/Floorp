/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef YV12_CONFIG_H
#define YV12_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vpx/vpx_integer.h"

#define VP8BORDERINPIXELS       32
#define VP9INNERBORDERINPIXELS  96
#define VP9BORDERINPIXELS      160
#define VP9_INTERP_EXTEND        4

  typedef struct yv12_buffer_config {
    int   y_width;
    int   y_height;
    int   y_crop_width;
    int   y_crop_height;
    int   y_stride;
    /*    int   yinternal_width; */

    int   uv_width;
    int   uv_height;
    int   uv_crop_width;
    int   uv_crop_height;
    int   uv_stride;
    /*    int   uvinternal_width; */

    int   alpha_width;
    int   alpha_height;
    int   alpha_stride;

    uint8_t *y_buffer;
    uint8_t *u_buffer;
    uint8_t *v_buffer;
    uint8_t *alpha_buffer;

    uint8_t *buffer_alloc;
    int buffer_alloc_sz;
    int border;
    int frame_size;

    int corrupted;
    int flags;
  } YV12_BUFFER_CONFIG;

  int vp8_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
                                  int width, int height, int border);
  int vp8_yv12_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
                                    int width, int height, int border);
  int vp8_yv12_de_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf);

  int vp9_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
                             int width, int height, int ss_x, int ss_y,
                             int border);
  int vp9_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf,
                               int width, int height, int ss_x, int ss_y,
                               int border);
  int vp9_free_frame_buffer(YV12_BUFFER_CONFIG *ybf);

#ifdef __cplusplus
}
#endif

#endif  // YV12_CONFIG_H
