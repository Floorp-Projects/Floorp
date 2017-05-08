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

#ifndef AOM_DSP_DKBOOLWRITER_H_
#define AOM_DSP_DKBOOLWRITER_H_

#include "./aom_config.h"

#if CONFIG_BITSTREAM_DEBUG
#include <stdio.h>
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#include "aom_dsp/prob.h"
#include "aom_ports/mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aom_dk_writer {
  unsigned int lowvalue;
  unsigned int range;
  int count;
  unsigned int pos;
  uint8_t *buffer;
} aom_dk_writer;

void aom_dk_start_encode(aom_dk_writer *bc, uint8_t *buffer);
void aom_dk_stop_encode(aom_dk_writer *bc);

static INLINE void aom_dk_write(aom_dk_writer *br, int bit, int probability) {
  unsigned int split;
  int count = br->count;
  unsigned int range = br->range;
  unsigned int lowvalue = br->lowvalue;
  register int shift;

#if CONFIG_BITSTREAM_DEBUG
  // int queue_r = 0;
  // int frame_idx_r = 0;
  // int queue_w = bitstream_queue_get_write();
  // int frame_idx_w = bitstream_queue_get_frame_write();
  // if (frame_idx_w == frame_idx_r && queue_w == queue_r) {
  //   fprintf(stderr, "\n *** bitstream queue at frame_idx_w %d queue_w %d\n",
  //   frame_idx_w, queue_w);
  // }
  bitstream_queue_push(bit, probability);
#endif  // CONFIG_BITSTREAM_DEBUG

  split = 1 + (((range - 1) * probability) >> 8);

  range = split;

  if (bit) {
    lowvalue += split;
    range = br->range - split;
  }

  shift = aom_norm[range];

  range <<= shift;
  count += shift;

  if (count >= 0) {
    int offset = shift - count;

    if ((lowvalue << (offset - 1)) & 0x80000000) {
      int x = br->pos - 1;

      while (x >= 0 && br->buffer[x] == 0xff) {
        br->buffer[x] = 0;
        x--;
      }

      br->buffer[x] += 1;
    }

    br->buffer[br->pos++] = (lowvalue >> (24 - offset));
    lowvalue <<= offset;
    shift = count;
    lowvalue &= 0xffffff;
    count -= 8;
  }

  lowvalue <<= shift;
  br->count = count;
  br->lowvalue = lowvalue;
  br->range = range;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_DKBOOLWRITER_H_
