/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_ONYXD_H_
#define VP9_DECODER_VP9_ONYXD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vpx_scale/yv12config.h"
#include "vp9/common/vp9_ppflags.h"
#include "vpx/vpx_codec.h"

typedef void *VP9D_PTR;

typedef struct {
  int width;
  int height;
  int version;
  int postprocess;
  int max_threads;
  int inv_tile_order;
  int input_partition;
} VP9D_CONFIG;

typedef enum {
  VP9_LAST_FLAG = 1,
  VP9_GOLD_FLAG = 2,
  VP9_ALT_FLAG = 4
} VP9_REFFRAME;

void vp9_initialize_dec();

int vp9_receive_compressed_data(VP9D_PTR comp,
                                size_t size, const uint8_t **dest,
                                int64_t time_stamp);

int vp9_get_raw_frame(VP9D_PTR comp, YV12_BUFFER_CONFIG *sd,
                      int64_t *time_stamp, int64_t *time_end_stamp,
                      vp9_ppflags_t *flags);

vpx_codec_err_t vp9_copy_reference_dec(VP9D_PTR comp,
                                       VP9_REFFRAME ref_frame_flag,
                                       YV12_BUFFER_CONFIG *sd);

vpx_codec_err_t vp9_set_reference_dec(VP9D_PTR comp,
                                      VP9_REFFRAME ref_frame_flag,
                                      YV12_BUFFER_CONFIG *sd);

int vp9_get_reference_dec(VP9D_PTR ptr, int index, YV12_BUFFER_CONFIG **fb);


VP9D_PTR vp9_create_decompressor(VP9D_CONFIG *oxcf);

void vp9_remove_decompressor(VP9D_PTR comp);

#ifdef __cplusplus
}
#endif

#endif  // VP9_DECODER_VP9_ONYXD_H_
