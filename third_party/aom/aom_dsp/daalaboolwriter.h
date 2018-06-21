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

#ifndef AOM_DSP_DAALABOOLWRITER_H_
#define AOM_DSP_DAALABOOLWRITER_H_

#include <stdio.h>

#include "aom_dsp/entenc.h"
#include "aom_dsp/prob.h"
#if CONFIG_BITSTREAM_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

struct daala_writer {
  unsigned int pos;
  uint8_t *buffer;
  od_ec_enc ec;
  uint8_t allow_update_cdf;
};

typedef struct daala_writer daala_writer;

void aom_daala_start_encode(daala_writer *w, uint8_t *buffer);
int aom_daala_stop_encode(daala_writer *w);

static INLINE void aom_daala_write(daala_writer *w, int bit, int prob) {
  int p = (0x7FFFFF - (prob << 15) + prob) >> 8;
#if CONFIG_BITSTREAM_DEBUG
  aom_cdf_prob cdf[2] = { (aom_cdf_prob)p, 32767 };
  /*int queue_r = 0;
  int frame_idx_r = 0;
  int queue_w = bitstream_queue_get_write();
  int frame_idx_w = bitstream_queue_get_frame_write();
  if (frame_idx_w == frame_idx_r && queue_w == queue_r) {
    fprintf(stderr, "\n *** bitstream queue at frame_idx_w %d queue_w %d\n",
    frame_idx_w, queue_w);
  }*/
  bitstream_queue_push(bit, cdf, 2);
#endif

  od_ec_encode_bool_q15(&w->ec, bit, p);
}

static INLINE void daala_write_symbol(daala_writer *w, int symb,
                                      const aom_cdf_prob *cdf, int nsymbs) {
#if CONFIG_BITSTREAM_DEBUG
  /*int queue_r = 0;
  int frame_idx_r = 0;
  int queue_w = bitstream_queue_get_write();
  int frame_idx_w = bitstream_queue_get_frame_write();
  if (frame_idx_w == frame_idx_r && queue_w == queue_r) {
    fprintf(stderr, "\n *** bitstream queue at frame_idx_w %d queue_w %d\n",
    frame_idx_w, queue_w);
  }*/
  bitstream_queue_push(symb, cdf, nsymbs);
#endif

  od_ec_encode_cdf_q15(&w->ec, symb, cdf, nsymbs);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
