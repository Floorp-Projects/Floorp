/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_WRITE_BIT_BUFFER_H_
#define VP9_ENCODER_VP9_WRITE_BIT_BUFFER_H_

#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vp9_write_bit_buffer {
  uint8_t *bit_buffer;
  size_t bit_offset;
};

size_t vp9_wb_bytes_written(const struct vp9_write_bit_buffer *wb);

void vp9_wb_write_bit(struct vp9_write_bit_buffer *wb, int bit);

void vp9_wb_write_literal(struct vp9_write_bit_buffer *wb, int data, int bits);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_WRITE_BIT_BUFFER_H_
