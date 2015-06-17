/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_READ_BIT_BUFFER_H_
#define VP9_DECODER_VP9_READ_BIT_BUFFER_H_

#include <limits.h>

#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*vp9_rb_error_handler)(void *data);

struct vp9_read_bit_buffer {
  const uint8_t *bit_buffer;
  const uint8_t *bit_buffer_end;
  size_t bit_offset;

  void *error_handler_data;
  vp9_rb_error_handler error_handler;
};

size_t vp9_rb_bytes_read(struct vp9_read_bit_buffer *rb);

int vp9_rb_read_bit(struct vp9_read_bit_buffer *rb);

int vp9_rb_read_literal(struct vp9_read_bit_buffer *rb, int bits);

int vp9_rb_read_signed_literal(struct vp9_read_bit_buffer *rb, int bits);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_READ_BIT_BUFFER_H_
