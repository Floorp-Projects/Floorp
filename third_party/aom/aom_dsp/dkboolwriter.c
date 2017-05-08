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

#include "./dkboolwriter.h"

static INLINE void aom_dk_write_bit(aom_dk_writer *w, int bit) {
  aom_dk_write(w, bit, 128);  // aom_prob_half
}

void aom_dk_start_encode(aom_dk_writer *br, uint8_t *source) {
  br->lowvalue = 0;
  br->range = 255;
  br->count = -24;
  br->buffer = source;
  br->pos = 0;
  aom_dk_write_bit(br, 0);
}

void aom_dk_stop_encode(aom_dk_writer *br) {
  int i;

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_set_skip_write(1);
#endif  // CONFIG_BITSTREAM_DEBUG

  for (i = 0; i < 32; i++) aom_dk_write_bit(br, 0);

#if CONFIG_BITSTREAM_DEBUG
  bitstream_queue_set_skip_write(0);
#endif  // CONFIG_BITSTREAM_DEBUG

  // Ensure there's no ambigous collision with any index marker bytes
  if ((br->buffer[br->pos - 1] & 0xe0) == 0xc0) br->buffer[br->pos++] = 0;
}
