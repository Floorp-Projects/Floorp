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

#include <assert.h>

#include "config/aom_config.h"

#include "aom_dsp/bitreader_buffer.h"

size_t aom_rb_bytes_read(const struct aom_read_bit_buffer *rb) {
  return (rb->bit_offset + 7) >> 3;
}

int aom_rb_read_bit(struct aom_read_bit_buffer *rb) {
  const uint32_t off = rb->bit_offset;
  const uint32_t p = off >> 3;
  const int q = 7 - (int)(off & 0x7);
  if (rb->bit_buffer + p < rb->bit_buffer_end) {
    const int bit = (rb->bit_buffer[p] >> q) & 1;
    rb->bit_offset = off + 1;
    return bit;
  } else {
    if (rb->error_handler) rb->error_handler(rb->error_handler_data);
    return 0;
  }
}

int aom_rb_read_literal(struct aom_read_bit_buffer *rb, int bits) {
  assert(bits <= 31);
  int value = 0, bit;
  for (bit = bits - 1; bit >= 0; bit--) value |= aom_rb_read_bit(rb) << bit;
  return value;
}

uint32_t aom_rb_read_unsigned_literal(struct aom_read_bit_buffer *rb,
                                      int bits) {
  assert(bits <= 32);
  uint32_t value = 0;
  int bit;
  for (bit = bits - 1; bit >= 0; bit--)
    value |= (uint32_t)aom_rb_read_bit(rb) << bit;
  return value;
}

int aom_rb_read_inv_signed_literal(struct aom_read_bit_buffer *rb, int bits) {
  const int nbits = sizeof(unsigned) * 8 - bits - 1;
  const unsigned value = (unsigned)aom_rb_read_literal(rb, bits + 1) << nbits;
  return ((int)value) >> nbits;
}

uint32_t aom_rb_read_uvlc(struct aom_read_bit_buffer *rb) {
  int leading_zeros = 0;
  while (!aom_rb_read_bit(rb)) ++leading_zeros;
  // Maximum 32 bits.
  if (leading_zeros >= 32) return UINT32_MAX;
  const uint32_t base = (1u << leading_zeros) - 1;
  const uint32_t value = aom_rb_read_literal(rb, leading_zeros);
  return base + value;
}
