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

#ifndef AOM_DSP_DKBOOLREADER_H_
#define AOM_DSP_DKBOOLREADER_H_

#include <assert.h>
#include <stddef.h>
#include <limits.h>

#include "./aom_config.h"
#if CONFIG_BITSTREAM_DEBUG
#include <assert.h>
#include <stdio.h>
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#include "aom_ports/mem.h"
#include "aom/aomdx.h"
#include "aom/aom_integer.h"
#include "aom_dsp/prob.h"
#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t BD_VALUE;

#define BD_VALUE_SIZE ((int)sizeof(BD_VALUE) * CHAR_BIT)

// This is meant to be a large, positive constant that can still be efficiently
// loaded as an immediate (on platforms like ARM, for example).
// Even relatively modest values like 100 would work fine.
#define LOTS_OF_BITS 0x40000000

struct aom_dk_reader {
  // Be careful when reordering this struct, it may impact the cache negatively.
  BD_VALUE value;
  unsigned int range;
  int count;
  const uint8_t *buffer_start;
  const uint8_t *buffer_end;
  const uint8_t *buffer;
  aom_decrypt_cb decrypt_cb;
  void *decrypt_state;
  uint8_t clear_buffer[sizeof(BD_VALUE) + 1];
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
};

int aom_dk_reader_init(struct aom_dk_reader *r, const uint8_t *buffer,
                       size_t size, aom_decrypt_cb decrypt_cb,
                       void *decrypt_state);

void aom_dk_reader_fill(struct aom_dk_reader *r);

const uint8_t *aom_dk_reader_find_end(struct aom_dk_reader *r);

static INLINE uint32_t aom_dk_reader_tell(const struct aom_dk_reader *r) {
  const uint32_t bits_read =
      (uint32_t)((r->buffer - r->buffer_start) * CHAR_BIT);
  const int count =
      (r->count < LOTS_OF_BITS) ? r->count : r->count - LOTS_OF_BITS;
  assert(r->buffer >= r->buffer_start);
  return bits_read - (count + CHAR_BIT);
}

/*The resolution of fractional-precision bit usage measurements, i.e.,
   3 => 1/8th bits.*/
#define DK_BITRES (3)

static INLINE uint32_t aom_dk_reader_tell_frac(const struct aom_dk_reader *r) {
  uint32_t num_bits;
  uint32_t range;
  int l;
  int i;
  num_bits = aom_dk_reader_tell(r) << DK_BITRES;
  range = r->range;
  l = 0;
  for (i = DK_BITRES; i-- > 0;) {
    int b;
    range = range * range >> 7;
    b = (int)(range >> 8);
    l = l << 1 | b;
    range >>= b;
  }
  return num_bits - l;
}

static INLINE int aom_dk_reader_has_error(struct aom_dk_reader *r) {
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

static INLINE int aom_dk_read(struct aom_dk_reader *r, int prob) {
  unsigned int bit = 0;
  BD_VALUE value;
  BD_VALUE bigsplit;
  int count;
  unsigned int range;
  unsigned int split = (r->range * prob + (256 - prob)) >> CHAR_BIT;

  if (r->count < 0) aom_dk_reader_fill(r);

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
    register int shift = aom_norm[range];
    range <<= shift;
    value <<= shift;
    count -= shift;
  }
  r->value = value;
  r->count = count;
  r->range = range;

#if CONFIG_BITSTREAM_DEBUG
  {
    int ref_bit, ref_prob;
    const int queue_r = bitstream_queue_get_read();
    const int frame_idx = bitstream_queue_get_frame_read();
    bitstream_queue_pop(&ref_bit, &ref_prob);
    if (prob != ref_prob) {
      fprintf(
          stderr,
          "\n *** prob error, frame_idx_r %d prob %d ref_prob %d queue_r %d\n",
          frame_idx, prob, ref_prob, queue_r);
      assert(0);
    }
    if ((int)bit != ref_bit) {
      fprintf(stderr, "\n *** bit error, frame_idx_r %d bit %d ref_bit %d\n",
              frame_idx, bit, ref_bit);
      assert(0);
    }
  }
#endif  // CONFIG_BITSTREAM_DEBUG

  return bit;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_DKBOOLREADER_H_
