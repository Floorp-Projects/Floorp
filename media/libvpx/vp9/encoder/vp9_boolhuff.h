/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
*
*   Module Title :     vp9_boolhuff.h
*
*   Description  :     Bool Coder header file.
*
****************************************************************************/
#ifndef VP9_ENCODER_VP9_BOOLHUFF_H_
#define VP9_ENCODER_VP9_BOOLHUFF_H_

#include "vpx_ports/mem.h"

typedef struct {
  unsigned int lowvalue;
  unsigned int range;
  unsigned int value;
  int count;
  unsigned int pos;
  uint8_t *buffer;

  // Variables used to track bit costs without outputing to the bitstream
  unsigned int  measure_cost;
  unsigned long bit_counter;
} vp9_writer;

extern const unsigned int vp9_prob_cost[256];

void vp9_start_encode(vp9_writer *bc, uint8_t *buffer);
void vp9_stop_encode(vp9_writer *bc);

DECLARE_ALIGNED(16, extern const unsigned char, vp9_norm[256]);

static void vp9_write(vp9_writer *br, int bit, int probability) {
  unsigned int split;
  int count = br->count;
  unsigned int range = br->range;
  unsigned int lowvalue = br->lowvalue;
  register unsigned int shift;

#ifdef ENTROPY_STATS
#if defined(SECTIONBITS_OUTPUT)

  if (bit)
    Sectionbits[active_section] += vp9_prob_cost[255 - probability];
  else
    Sectionbits[active_section] += vp9_prob_cost[probability];

#endif
#endif

  split = 1 + (((range - 1) * probability) >> 8);

  range = split;

  if (bit) {
    lowvalue += split;
    range = br->range - split;
  }

  shift = vp9_norm[range];

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

static void vp9_write_bit(vp9_writer *w, int bit) {
  vp9_write(w, bit, 128);  // vp9_prob_half
}

static void vp9_write_literal(vp9_writer *w, int data, int bits) {
  int bit;

  for (bit = bits - 1; bit >= 0; bit--)
    vp9_write_bit(w, 1 & (data >> bit));
}


#endif  // VP9_ENCODER_VP9_BOOLHUFF_H_
