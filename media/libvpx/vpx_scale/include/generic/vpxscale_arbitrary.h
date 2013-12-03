/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __VPX_SCALE_ARBITRARY_H__
#define __VPX_SCALE_ARBITRARY_H__

#include "vpx_scale/yv12config.h"

typedef struct {
  int in_width;
  int in_height;

  int out_width;
  int out_height;
  int max_usable_out_width;

  // numerator for the width and height
  int nw;
  int nh;
  int nh_uv;

  // output to input correspondance array
  short *l_w;
  short *l_h;
  short *l_h_uv;

  // polyphase coefficients
  short *c_w;
  short *c_h;
  short *c_h_uv;

  // buffer for horizontal filtering.
  unsigned char *hbuf;
  unsigned char *hbuf_uv;
} BICUBIC_SCALER_STRUCT;

int bicubic_coefficient_setup(int in_width, int in_height, int out_width, int out_height);
int bicubic_scale(int in_width, int in_height, int in_stride,
                  int out_width, int out_height, int out_stride,
                  unsigned char *input_image, unsigned char *output_image);
void bicubic_scale_frame_reset();
void bicubic_scale_frame(YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                         int new_width, int new_height);
void bicubic_coefficient_init();
void bicubic_coefficient_destroy();

#endif /* __VPX_SCALE_ARBITRARY_H__ */
