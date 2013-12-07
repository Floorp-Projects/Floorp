/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_ports/mem.h"
#include "vpx_mem/vpx_mem.h"

#include "vp9/decoder/vp9_dboolhuff.h"

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define LOTS_OF_BITS 0x40000000


int vp9_reader_init(vp9_reader *r, const uint8_t *buffer, size_t size) {
  int marker_bit;

  r->buffer_end = buffer + size;
  r->buffer = buffer;
  r->value = 0;
  r->count = -8;
  r->range = 255;

  if (size && !buffer)
    return 1;

  vp9_reader_fill(r);
  marker_bit = vp9_read_bit(r);
  return marker_bit != 0;
}

void vp9_reader_fill(vp9_reader *r) {
  const uint8_t *const buffer_end = r->buffer_end;
  const uint8_t *buffer = r->buffer;
  VP9_BD_VALUE value = r->value;
  int count = r->count;
  int shift = BD_VALUE_SIZE - 8 - (count + 8);
  int loop_end = 0;
  const int bits_left = (int)((buffer_end - buffer)*CHAR_BIT);
  const int x = shift + CHAR_BIT - bits_left;

  if (x >= 0) {
    count += LOTS_OF_BITS;
    loop_end = x;
  }

  if (x < 0 || bits_left) {
    while (shift >= loop_end) {
      count += CHAR_BIT;
      value |= (VP9_BD_VALUE)*buffer++ << shift;
      shift -= CHAR_BIT;
    }
  }

  r->buffer = buffer;
  r->value = value;
  r->count = count;
}

const uint8_t *vp9_reader_find_end(vp9_reader *r) {
  // Find the end of the coded buffer
  while (r->count > CHAR_BIT && r->count < BD_VALUE_SIZE) {
    r->count -= CHAR_BIT;
    r->buffer--;
  }
  return r->buffer;
}

int vp9_reader_has_error(vp9_reader *r) {
  // Check if we have reached the end of the buffer.
  //
  // Variable 'count' stores the number of bits in the 'value' buffer, minus
  // 8. The top byte is part of the algorithm, and the remainder is buffered
  // to be shifted into it. So if count == 8, the top 16 bits of 'value' are
  // occupied, 8 for the algorithm and 8 in the buffer.
  //
  // When reading a byte from the user's buffer, count is filled with 8 and
  // one byte is filled into the value buffer. When we reach the end of the
  // data, count is additionally filled with LOTS_OF_BITS. So when
  // count == LOTS_OF_BITS - 1, the user's data has been exhausted.
  //
  // 1 if we have tried to decode bits after the end of stream was encountered.
  // 0 No error.
  return r->count > BD_VALUE_SIZE && r->count < LOTS_OF_BITS;
}
