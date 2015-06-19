/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "vp9/decoder/vp9_read_bit_buffer.h"

size_t vp9_rb_bytes_read(struct vp9_read_bit_buffer *rb) {
  return (rb->bit_offset + 7) >> 3;
}

int vp9_rb_read_bit(struct vp9_read_bit_buffer *rb) {
  const size_t off = rb->bit_offset;
  const size_t p = off >> 3;
  const int q = 7 - (int)(off & 0x7);
  if (rb->bit_buffer + p < rb->bit_buffer_end) {
    const int bit = (rb->bit_buffer[p] >> q) & 1;
    rb->bit_offset = off + 1;
    return bit;
  } else {
    rb->error_handler(rb->error_handler_data);
    return 0;
  }
}

int vp9_rb_read_literal(struct vp9_read_bit_buffer *rb, int bits) {
  int value = 0, bit;
  for (bit = bits - 1; bit >= 0; bit--)
    value |= vp9_rb_read_bit(rb) << bit;
  return value;
}

int vp9_rb_read_signed_literal(struct vp9_read_bit_buffer *rb,
                               int bits) {
  const int value = vp9_rb_read_literal(rb, bits);
  return vp9_rb_read_bit(rb) ? -value : value;
}
