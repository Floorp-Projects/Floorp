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

#ifndef AOM_AOM_DSP_BUF_ANS_H_
#define AOM_AOM_DSP_BUF_ANS_H_
// Buffered forward ANS writer.
// Symbols are written to the writer in forward (decode) order and serialized
// backwards due to ANS's stack like behavior.

#include <assert.h>
#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_dsp/ans.h"
#include "aom_dsp/answriter.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define ANS_METHOD_RABS 0
#define ANS_METHOD_RANS 1

struct buffered_ans_symbol {
  unsigned int method : 1;  // one of ANS_METHOD_RABS or ANS_METHOD_RANS
  // TODO(aconverse): Should be possible to write this in terms of start for ABS
  unsigned int val_start : RANS_PROB_BITS;  // Boolean value for ABS
                                            // start in symbol cycle for Rans
  unsigned int prob : RANS_PROB_BITS;       // Probability of this symbol
};

struct BufAnsCoder {
  struct aom_internal_error_info *error;
  struct buffered_ans_symbol *buf;
  struct AnsCoder ans;
  int size;
  int offset;
  int output_bytes;
#if ANS_MAX_SYMBOLS
  int window_size;
#endif
  int pos;  // Dummy variable to store the output buffer after closing
  uint8_t allow_update_cdf;
};

// Allocate a buffered ANS coder to store size symbols.
// When ANS_MAX_SYMBOLS is turned on, the size is the fixed size of each ANS
// partition.
// When ANS_MAX_SYMBOLS is turned off, size is merely an initial hint and the
// buffer will grow on demand
void aom_buf_ans_alloc(struct BufAnsCoder *c,
                       struct aom_internal_error_info *error);

void aom_buf_ans_free(struct BufAnsCoder *c);

#if !ANS_MAX_SYMBOLS
void aom_buf_ans_grow(struct BufAnsCoder *c);
#endif

void aom_buf_ans_flush(struct BufAnsCoder *const c);

static INLINE void buf_ans_write_init(struct BufAnsCoder *const c,
                                      uint8_t *const output_buffer) {
  c->offset = 0;
  c->output_bytes = 0;
  ans_write_init(&c->ans, output_buffer);
}

static INLINE void buf_rabs_write(struct BufAnsCoder *const c, uint8_t val,
                                  AnsP8 prob) {
  assert(c->offset <= c->size);
#if !ANS_MAX_SYMBOLS
  if (c->offset == c->size) {
    aom_buf_ans_grow(c);
  }
#endif
  c->buf[c->offset].method = ANS_METHOD_RABS;
  c->buf[c->offset].val_start = val;
  c->buf[c->offset].prob = prob;
  ++c->offset;
#if ANS_MAX_SYMBOLS
  if (c->offset == c->size) aom_buf_ans_flush(c);
#endif
}

// Buffer one symbol for encoding using rANS.
// cum_prob: The cumulative probability before this symbol (the offset of
// the symbol in the symbol cycle)
// prob: The probability of this symbol (l_s from the paper)
// RANS_PRECISION takes the place of m from the paper.
static INLINE void buf_rans_write(struct BufAnsCoder *const c,
                                  aom_cdf_prob cum_prob, aom_cdf_prob prob) {
  assert(c->offset <= c->size);
#if !ANS_MAX_SYMBOLS
  if (c->offset == c->size) {
    aom_buf_ans_grow(c);
  }
#endif
  c->buf[c->offset].method = ANS_METHOD_RANS;
  c->buf[c->offset].val_start = cum_prob;
  c->buf[c->offset].prob = prob;
  ++c->offset;
#if ANS_MAX_SYMBOLS
  if (c->offset == c->size) aom_buf_ans_flush(c);
#endif
}

static INLINE void buf_rabs_write_bit(struct BufAnsCoder *c, int bit) {
  buf_rabs_write(c, bit, 128);
}

static INLINE void buf_rabs_write_literal(struct BufAnsCoder *c, int literal,
                                          int bits) {
  int bit;

  assert(bits < 31);
  for (bit = bits - 1; bit >= 0; bit--)
    buf_rabs_write_bit(c, 1 & (literal >> bit));
}

static INLINE int buf_ans_write_end(struct BufAnsCoder *const c) {
  assert(c->offset == 0);
  return c->output_bytes;
}
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_AOM_DSP_BUF_ANS_H_
