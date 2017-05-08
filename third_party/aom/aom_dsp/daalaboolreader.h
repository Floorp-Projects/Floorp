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

#ifndef AOM_DSP_DAALABOOLREADER_H_
#define AOM_DSP_DAALABOOLREADER_H_

#include "aom/aom_integer.h"
#include "aom_dsp/entdec.h"
#include "aom_dsp/prob.h"
#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#endif
#if CONFIG_BITSTREAM_DEBUG
#include <stdio.h>
#include "aom_util/debug_util.h"
#endif  // CONFIG_BITSTREAM_DEBUG

#ifdef __cplusplus
extern "C" {
#endif

struct daala_reader {
  const uint8_t *buffer;
  const uint8_t *buffer_end;
  od_ec_dec ec;
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
};

typedef struct daala_reader daala_reader;

int aom_daala_reader_init(daala_reader *r, const uint8_t *buffer, int size);
const uint8_t *aom_daala_reader_find_end(daala_reader *r);
uint32_t aom_daala_reader_tell(const daala_reader *r);
uint32_t aom_daala_reader_tell_frac(const daala_reader *r);

static INLINE int aom_daala_read(daala_reader *r, int prob) {
  int bit;
#if CONFIG_EC_SMALLMUL
  int p = (0x7FFFFF - (prob << 15) + prob) >> 8;
#else
  int p = ((prob << 15) + 256 - prob) >> 8;
#endif
#if CONFIG_BITSTREAM_DEBUG
/*{
  const int queue_r = bitstream_queue_get_read();
  const int frame_idx = bitstream_queue_get_frame_read();
  if (frame_idx == 0 && queue_r == 0) {
    fprintf(stderr, "\n *** bitstream queue at frame_idx_r %d queue_r %d\n",
            frame_idx, queue_r);
  }
}*/
#endif

  bit = od_ec_decode_bool_q15(&r->ec, p);

#if CONFIG_BITSTREAM_DEBUG
  {
    int i;
    int ref_bit, ref_nsymbs;
    aom_cdf_prob ref_cdf[16];
    const int queue_r = bitstream_queue_get_read();
    const int frame_idx = bitstream_queue_get_frame_read();
    bitstream_queue_pop(&ref_bit, ref_cdf, &ref_nsymbs);
    if (ref_nsymbs != 2) {
      fprintf(stderr,
              "\n *** [bit] nsymbs error, frame_idx_r %d nsymbs %d ref_nsymbs "
              "%d queue_r %d\n",
              frame_idx, 2, ref_nsymbs, queue_r);
      assert(0);
    }
    if ((ref_nsymbs != 2) || (ref_cdf[0] != (aom_cdf_prob)p) ||
        (ref_cdf[1] != 32767)) {
      fprintf(stderr,
              "\n *** [bit] cdf error, frame_idx_r %d cdf {%d, %d} ref_cdf {%d",
              frame_idx, p, 32767, ref_cdf[0]);
      for (i = 1; i < ref_nsymbs; ++i) fprintf(stderr, ", %d", ref_cdf[i]);
      fprintf(stderr, "} queue_r %d\n", queue_r);
      assert(0);
    }
    if (bit != ref_bit) {
      fprintf(stderr,
              "\n *** [bit] symb error, frame_idx_r %d symb %d ref_symb %d "
              "queue_r %d\n",
              frame_idx, bit, ref_bit, queue_r);
      assert(0);
    }
  }
#endif

  return bit;
}

#if CONFIG_RAWBITS
static INLINE int aom_daala_read_bit(daala_reader *r) {
  return od_ec_dec_bits(&r->ec, 1, "aom_bits");
}
#endif

static INLINE int aom_daala_reader_has_error(daala_reader *r) {
  return r->ec.error;
}

static INLINE int daala_read_symbol(daala_reader *r, const aom_cdf_prob *cdf,
                                    int nsymbs) {
  int symb;
  symb = od_ec_decode_cdf_q15(&r->ec, cdf, nsymbs);

#if CONFIG_BITSTREAM_DEBUG
  {
    int i;
    int cdf_error = 0;
    int ref_symb, ref_nsymbs;
    aom_cdf_prob ref_cdf[16];
    const int queue_r = bitstream_queue_get_read();
    const int frame_idx = bitstream_queue_get_frame_read();
    bitstream_queue_pop(&ref_symb, ref_cdf, &ref_nsymbs);
    if (nsymbs != ref_nsymbs) {
      fprintf(stderr,
              "\n *** nsymbs error, frame_idx_r %d nsymbs %d ref_nsymbs %d "
              "queue_r %d\n",
              frame_idx, nsymbs, ref_nsymbs, queue_r);
      cdf_error = 0;
      assert(0);
    } else {
      for (i = 0; i < nsymbs; ++i)
        if (cdf[i] != ref_cdf[i]) cdf_error = 1;
    }
    if (cdf_error) {
      fprintf(stderr, "\n *** cdf error, frame_idx_r %d cdf {%d", frame_idx,
              cdf[0]);
      for (i = 1; i < nsymbs; ++i) fprintf(stderr, ", %d", cdf[i]);
      fprintf(stderr, "} ref_cdf {%d", ref_cdf[0]);
      for (i = 1; i < ref_nsymbs; ++i) fprintf(stderr, ", %d", ref_cdf[i]);
      fprintf(stderr, "} queue_r %d\n", queue_r);
      assert(0);
    }
    if (symb != ref_symb) {
      fprintf(
          stderr,
          "\n *** symb error, frame_idx_r %d symb %d ref_symb %d queue_r %d\n",
          frame_idx, symb, ref_symb, queue_r);
      assert(0);
    }
  }
#endif

  return symb;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
