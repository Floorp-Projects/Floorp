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
#include <limits.h>
#include <stdlib.h>

#include "config/aom_config.h"

#include "aom_dsp/bitwriter_buffer.h"

int aom_wb_is_byte_aligned(const struct aom_write_bit_buffer *wb) {
  return (wb->bit_offset % CHAR_BIT == 0);
}

uint32_t aom_wb_bytes_written(const struct aom_write_bit_buffer *wb) {
  return wb->bit_offset / CHAR_BIT + (wb->bit_offset % CHAR_BIT > 0);
}

void aom_wb_write_bit(struct aom_write_bit_buffer *wb, int bit) {
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  if (q == CHAR_BIT - 1) {
    // Zero next char and write bit
    wb->bit_buffer[p] = bit << q;
  } else {
    wb->bit_buffer[p] &= ~(1 << q);
    wb->bit_buffer[p] |= bit << q;
  }
  wb->bit_offset = off + 1;
}

void aom_wb_overwrite_bit(struct aom_write_bit_buffer *wb, int bit) {
  // Do not zero bytes but overwrite exisiting values
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  wb->bit_buffer[p] &= ~(1 << q);
  wb->bit_buffer[p] |= bit << q;
  wb->bit_offset = off + 1;
}

void aom_wb_write_literal(struct aom_write_bit_buffer *wb, int data, int bits) {
  assert(bits <= 31);
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) aom_wb_write_bit(wb, (data >> bit) & 1);
}

void aom_wb_write_unsigned_literal(struct aom_write_bit_buffer *wb,
                                   uint32_t data, int bits) {
  assert(bits <= 32);
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) aom_wb_write_bit(wb, (data >> bit) & 1);
}

void aom_wb_overwrite_literal(struct aom_write_bit_buffer *wb, int data,
                              int bits) {
  int bit;
  for (bit = bits - 1; bit >= 0; bit--)
    aom_wb_overwrite_bit(wb, (data >> bit) & 1);
}

void aom_wb_write_inv_signed_literal(struct aom_write_bit_buffer *wb, int data,
                                     int bits) {
  aom_wb_write_literal(wb, data, bits + 1);
}
