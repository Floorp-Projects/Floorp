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

#ifndef AOM_DSP_ANSWRITER_H_
#define AOM_DSP_ANSWRITER_H_
// An implementation of Asymmetric Numeral Systems
// http://arxiv.org/abs/1311.2540v2
// Implements encoding of:
// * rABS (range Asymmetric Binary Systems), a boolean coder
// * rANS (range Asymmetric Numeral Systems), a multi-symbol coder

#include <assert.h>
#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_dsp/ans.h"
#include "aom_dsp/prob.h"
#include "aom_ports/mem_ops.h"
#include "av1/common/odintrin.h"

#if RANS_PRECISION <= OD_DIVU_DMAX
#define ANS_DIVREM(quotient, remainder, dividend, divisor) \
  do {                                                     \
    quotient = OD_DIVU_SMALL((dividend), (divisor));       \
    remainder = (dividend) - (quotient) * (divisor);       \
  } while (0)
#else
#define ANS_DIVREM(quotient, remainder, dividend, divisor) \
  do {                                                     \
    quotient = (dividend) / (divisor);                     \
    remainder = (dividend) % (divisor);                    \
  } while (0)
#endif

#define ANS_DIV8(dividend, divisor) OD_DIVU_SMALL((dividend), (divisor))

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct AnsCoder {
  uint8_t *buf;
  int buf_offset;
  uint32_t state;
};

static INLINE void ans_write_init(struct AnsCoder *const ans,
                                  uint8_t *const buf) {
  ans->buf = buf;
  ans->buf_offset = 0;
  ans->state = L_BASE;
}

static INLINE int ans_write_end(struct AnsCoder *const ans) {
  uint32_t state;
  int ans_size;
  assert(ans->state >= L_BASE);
  assert(ans->state < L_BASE * IO_BASE);
  state = ans->state - L_BASE;
  if (state < (1u << 15)) {
    mem_put_le16(ans->buf + ans->buf_offset, (0x00u << 15) + state);
    ans_size = ans->buf_offset + 2;
#if ANS_REVERSE
#if L_BASE * IO_BASE > (1 << 23)
  } else if (state < (1u << 22)) {
    mem_put_le24(ans->buf + ans->buf_offset, (0x02u << 22) + state);
    ans_size = ans->buf_offset + 3;
  } else if (state < (1u << 30)) {
    mem_put_le32(ans->buf + ans->buf_offset, (0x03u << 30) + state);
    ans_size = ans->buf_offset + 4;
#else
  } else if (state < (1u << 23)) {
    mem_put_le24(ans->buf + ans->buf_offset, (0x01u << 23) + state);
    ans_size = ans->buf_offset + 3;
#endif
#else
  } else if (state < (1u << 22)) {
    mem_put_le24(ans->buf + ans->buf_offset, (0x02u << 22) + state);
    ans_size = ans->buf_offset + 3;
  } else if (state < (1u << 29)) {
    mem_put_le32(ans->buf + ans->buf_offset, (0x07u << 29) + state);
    ans_size = ans->buf_offset + 4;
#endif
  } else {
    assert(0 && "State is too large to be serialized");
    return ans->buf_offset;
  }
#if ANS_REVERSE
  {
    int i;
    uint8_t tmp;
    for (i = 0; i < (ans_size >> 1); i++) {
      tmp = ans->buf[i];
      ans->buf[i] = ans->buf[ans_size - 1 - i];
      ans->buf[ans_size - 1 - i] = tmp;
    }
    ans->buf += ans_size;
    ans->buf_offset = 0;
    ans->state = L_BASE;
  }
#endif
  return ans_size;
}

// Write one boolean using rABS where p0 is the probability of the value being
// zero.
static INLINE void rabs_write(struct AnsCoder *ans, int value, AnsP8 p0) {
  const AnsP8 p = ANS_P8_PRECISION - p0;
  const unsigned l_s = value ? p : p0;
  unsigned state = ans->state;
  while (state >= L_BASE / ANS_P8_PRECISION * IO_BASE * l_s) {
    ans->buf[ans->buf_offset++] = state % IO_BASE;
    state /= IO_BASE;
  }
  const unsigned quotient = ANS_DIV8(state, l_s);
  const unsigned remainder = state - quotient * l_s;
  ans->state = quotient * ANS_P8_PRECISION + remainder + (value ? p0 : 0);
}

// Encode one symbol using rANS.
// cum_prob: The cumulative probability before this symbol (the offset of
// the symbol in the symbol cycle)
// prob: The probability of this symbol (l_s from the paper)
// RANS_PRECISION takes the place of m from the paper.
static INLINE void rans_write(struct AnsCoder *ans, aom_cdf_prob cum_prob,
                              aom_cdf_prob prob) {
  unsigned quotient, remainder;
  while (ans->state >= L_BASE / RANS_PRECISION * IO_BASE * prob) {
    ans->buf[ans->buf_offset++] = ans->state % IO_BASE;
    ans->state /= IO_BASE;
  }
  ANS_DIVREM(quotient, remainder, ans->state, prob);
  ans->state = quotient * RANS_PRECISION + remainder + cum_prob;
}

#undef ANS_DIV8
#undef ANS_DIVREM
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // AOM_DSP_ANSWRITER_H_
