/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_DECODER_VP9_READER_H_
#define VP9_DECODER_VP9_READER_H_

#include <stddef.h>
#include <limits.h>

#include "./vpx_config.h"
#include "vpx_ports/mem.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_prob.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t BD_VALUE;

#define BD_VALUE_SIZE ((int)sizeof(BD_VALUE) * CHAR_BIT)

typedef struct {
  // Be careful when reordering this struct, it may impact the cache negatively.
  BD_VALUE value;
  unsigned int range;
  int count;
  const uint8_t *buffer_end;
  const uint8_t *buffer;
  vpx_decrypt_cb decrypt_cb;
  void *decrypt_state;
  uint8_t clear_buffer[sizeof(BD_VALUE) + 1];
} vp9_reader;

int vp9_reader_init(vp9_reader *r,
                    const uint8_t *buffer,
                    size_t size,
                    vpx_decrypt_cb decrypt_cb,
                    void *decrypt_state);

void vp9_reader_fill(vp9_reader *r);

int vp9_reader_has_error(vp9_reader *r);

const uint8_t *vp9_reader_find_end(vp9_reader *r);

static INLINE int vp9_read(vp9_reader *r, int prob) {
  unsigned int bit = 0;
  BD_VALUE value;
  BD_VALUE bigsplit;
  int count;
  unsigned int range;
  unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;

  if (r->count < 0)
    vp9_reader_fill(r);

  value = r->value;
  count = r->count;

  bigsplit = (BD_VALUE)split << (BD_VALUE_SIZE - CHAR_BIT);

  range = split;

  if (value >= bigsplit) {
    range = r->range - split;
    value = value - bigsplit;
    bit = 1;
  }

  {
    register unsigned int shift = vp9_norm[range];
    range <<= shift;
    value <<= shift;
    count -= shift;
  }
  r->value = value;
  r->count = count;
  r->range = range;

  return bit;
}

static INLINE int vp9_read_bit(vp9_reader *r) {
  return vp9_read(r, 128);  // vp9_prob_half
}

static INLINE int vp9_read_literal(vp9_reader *r, int bits) {
  int literal = 0, bit;

  for (bit = bits - 1; bit >= 0; bit--)
    literal |= vp9_read_bit(r) << bit;

  return literal;
}

static INLINE int vp9_read_tree(vp9_reader *r, const vp9_tree_index *tree,
                                const vp9_prob *probs) {
  vp9_tree_index i = 0;

  while ((i = tree[i + vp9_read(r, probs[i >> 1])]) > 0)
    continue;

  return -i;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_READER_H_
