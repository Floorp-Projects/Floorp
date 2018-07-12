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

#ifndef AOM_DSP_ANSREADER_H_
#define AOM_DSP_ANSREADER_H_
// An implementation of Asymmetric Numeral Systems
// http://arxiv.org/abs/1311.2540v2
// Implements decoding of:
// * rABS (range Asymmetric Binary Systems), a boolean coder
// * rANS (range Asymmetric Numeral Systems), a multi-symbol coder

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/prob.h"
#include "aom_dsp/ans.h"
#include "aom_ports/mem_ops.h"
#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct AnsDecoder {
  const uint8_t *buf;
  int buf_offset;
  uint32_t state;
#if ANS_MAX_SYMBOLS
  int symbols_left;
  int window_size;
#endif
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
};

static INLINE int ans_read_reinit(struct AnsDecoder *const ans);

static INLINE unsigned refill_state(struct AnsDecoder *const ans,
                                    unsigned state) {
#if ANS_REVERSE
  while (state < L_BASE && ans->buf_offset < 0) {
    state = state * IO_BASE + ans->buf[ans->buf_offset++];
  }
#else
  while (state < L_BASE && ans->buf_offset > 0) {
    state = state * IO_BASE + ans->buf[--ans->buf_offset];
  }
#endif
  return state;
}

// Decode one rABS encoded boolean where the probability of the value being zero
// is p0.
static INLINE int rabs_read(struct AnsDecoder *ans, AnsP8 p0) {
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  unsigned state = refill_state(ans, ans->state);
  const unsigned quotient = state / ANS_P8_PRECISION;
  const unsigned remainder = state % ANS_P8_PRECISION;
  const int value = remainder >= p0;
  const unsigned qp0 = quotient * p0;
  if (value)
    state = state - qp0 - p0;
  else
    state = qp0 + remainder;
  ans->state = state;
  return value;
}

// Decode one rABS encoded boolean where the probability of the value being zero
// is one half.
static INLINE int rabs_read_bit(struct AnsDecoder *ans) {
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  unsigned state = refill_state(ans, ans->state);
  const int value = !!(state & 0x80);
  ans->state = ((state >> 1) & ~0x7F) | (state & 0x7F);
  return value;
}

struct rans_dec_sym {
  uint8_t val;
  aom_cdf_prob prob;
  aom_cdf_prob cum_prob;  // not-inclusive
};

static INLINE void fetch_sym(struct rans_dec_sym *out, const aom_cdf_prob *cdf,
                             aom_cdf_prob rem) {
  int i;
  aom_cdf_prob cum_prob = 0, top_prob;
  // TODO(skal): if critical, could be a binary search.
  // Or, better, an O(1) alias-table.
  for (i = 0; rem >= (top_prob = cdf[i]); ++i) {
    cum_prob = top_prob;
  }
  out->val = i;
  out->prob = top_prob - cum_prob;
  out->cum_prob = cum_prob;
}

static INLINE int rans_read(struct AnsDecoder *ans, const aom_cdf_prob *tab) {
  unsigned rem;
  unsigned quo;
  struct rans_dec_sym sym;
#if ANS_MAX_SYMBOLS
  if (ans->symbols_left-- == 0) {
    ans_read_reinit(ans);
    ans->symbols_left--;
  }
#endif
  ans->state = refill_state(ans, ans->state);
  quo = ans->state / RANS_PRECISION;
  rem = ans->state % RANS_PRECISION;
  fetch_sym(&sym, tab, rem);
  ans->state = quo * sym.prob + rem - sym.cum_prob;
  return sym.val;
}

static INLINE int ans_read_init(struct AnsDecoder *const ans,
                                const uint8_t *const buf, int offset) {
  unsigned x;
  if (offset < 1) return 1;
#if ANS_REVERSE
  ans->buf = buf + offset;
  ans->buf_offset = -offset;
  x = buf[0];
  if ((x & 0x80) == 0) {  // Marker is 0xxx xxxx
    if (offset < 2) return 1;
    ans->buf_offset += 2;
    ans->state = mem_get_be16(buf) & 0x7FFF;
#if L_BASE * IO_BASE > (1 << 23)
  } else if ((x & 0xC0) == 0x80) {  // Marker is 10xx xxxx
    if (offset < 3) return 1;
    ans->buf_offset += 3;
    ans->state = mem_get_be24(buf) & 0x3FFFFF;
  } else {  // Marker is 11xx xxxx
    if (offset < 4) return 1;
    ans->buf_offset += 4;
    ans->state = mem_get_be32(buf) & 0x3FFFFFFF;
#else
  } else {  // Marker is 1xxx xxxx
    if (offset < 3) return 1;
    ans->buf_offset += 3;
    ans->state = mem_get_be24(buf) & 0x7FFFFF;
#endif
  }
#else
  ans->buf = buf;
  x = buf[offset - 1];
  if ((x & 0x80) == 0) {  // Marker is 0xxx xxxx
    if (offset < 2) return 1;
    ans->buf_offset = offset - 2;
    ans->state = mem_get_le16(buf + offset - 2) & 0x7FFF;
  } else if ((x & 0xC0) == 0x80) {  // Marker is 10xx xxxx
    if (offset < 3) return 1;
    ans->buf_offset = offset - 3;
    ans->state = mem_get_le24(buf + offset - 3) & 0x3FFFFF;
  } else if ((x & 0xE0) == 0xE0) {  // Marker is 111x xxxx
    if (offset < 4) return 1;
    ans->buf_offset = offset - 4;
    ans->state = mem_get_le32(buf + offset - 4) & 0x1FFFFFFF;
  } else {
    // Marker 110x xxxx implies this byte is a superframe marker
    return 1;
  }
#endif  // ANS_REVERSE
#if CONFIG_ACCOUNTING
  ans->accounting = NULL;
#endif
  ans->state += L_BASE;
  if (ans->state >= L_BASE * IO_BASE) return 1;
#if ANS_MAX_SYMBOLS
  assert(ans->window_size > 1);
  ans->symbols_left = ans->window_size;
#endif
  return 0;
}

#if ANS_REVERSE
static INLINE int ans_read_reinit(struct AnsDecoder *const ans) {
  return ans_read_init(ans, ans->buf + ans->buf_offset, -ans->buf_offset);
}
#endif

static INLINE int ans_read_end(const struct AnsDecoder *const ans) {
  return ans->buf_offset == 0 && ans->state < L_BASE;
}

static INLINE int ans_reader_has_error(const struct AnsDecoder *const ans) {
  return ans->state < L_BASE / RANS_PRECISION;
}
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_DSP_ANSREADER_H_
