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

typedef void (*vp9_rb_error_handler)(void *data, size_t bit_offset);

struct vp9_read_bit_buffer {
  const uint8_t *bit_buffer;
  const uint8_t *bit_buffer_end;
  size_t bit_offset;

  void *error_handler_data;
  vp9_rb_error_handler error_handler;
};

static size_t vp9_rb_bytes_read(struct vp9_read_bit_buffer *rb) {
  return rb->bit_offset / CHAR_BIT + (rb->bit_offset % CHAR_BIT > 0);
}

static int vp9_rb_read_bit(struct vp9_read_bit_buffer *rb) {
  const size_t off = rb->bit_offset;
  const size_t p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - (int)off % CHAR_BIT;
  if (rb->bit_buffer + p >= rb->bit_buffer_end) {
    rb->error_handler(rb->error_handler_data, rb->bit_offset);
    return 0;
  } else {
    const int bit = (rb->bit_buffer[p] & (1 << q)) >> q;
    rb->bit_offset = off + 1;
    return bit;
  }
}

static int vp9_rb_read_literal(struct vp9_read_bit_buffer *rb, int bits) {
  int value = 0, bit;
  for (bit = bits - 1; bit >= 0; bit--)
    value |= vp9_rb_read_bit(rb) << bit;
  return value;
}

static int vp9_rb_read_signed_literal(struct vp9_read_bit_buffer *rb,
                                      int bits) {
  const int value = vp9_rb_read_literal(rb, bits);
  return vp9_rb_read_bit(rb) ? -value : value;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_READ_BIT_BUFFER_H_
