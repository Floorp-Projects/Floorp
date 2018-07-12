/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/binary_codes_reader.h"

#include "av1/common/common.h"

// Inverse recenters a non-negative literal v around a reference r
static uint16_t inv_recenter_nonneg(uint16_t r, uint16_t v) {
  if (v > (r << 1))
    return v;
  else if ((v & 1) == 0)
    return (v >> 1) + r;
  else
    return r - ((v + 1) >> 1);
}

// Inverse recenters a non-negative literal v in [0, n-1] around a
// reference r also in [0, n-1]
static uint16_t inv_recenter_finite_nonneg(uint16_t n, uint16_t r, uint16_t v) {
  if ((r << 1) <= n) {
    return inv_recenter_nonneg(r, v);
  } else {
    return n - 1 - inv_recenter_nonneg(n - 1 - r, v);
  }
}

int16_t aom_read_primitive_symmetric_(aom_reader *r,
                                      unsigned int mag_bits ACCT_STR_PARAM) {
  if (aom_read_bit(r, ACCT_STR_NAME)) {
    int s = aom_read_bit(r, ACCT_STR_NAME);
    int16_t x = aom_read_literal(r, mag_bits, ACCT_STR_NAME) + 1;
    return (s > 0 ? -x : x);
  } else {
    return 0;
  }
}

uint16_t aom_read_primitive_quniform_(aom_reader *r,
                                      uint16_t n ACCT_STR_PARAM) {
  if (n <= 1) return 0;
  const int l = get_msb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = aom_read_literal(r, l - 1, ACCT_STR_NAME);
  return v < m ? v : (v << 1) - m + aom_read_bit(r, ACCT_STR_NAME);
}

static uint16_t aom_rb_read_primitive_quniform(struct aom_read_bit_buffer *rb,
                                               uint16_t n) {
  if (n <= 1) return 0;
  const int l = get_msb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = aom_rb_read_literal(rb, l - 1);
  return v < m ? v : (v << 1) - m + aom_rb_read_bit(rb);
}

uint16_t aom_read_primitive_refbilevel_(aom_reader *r, uint16_t n, uint16_t p,
                                        uint16_t ref ACCT_STR_PARAM) {
  if (n <= 1) return 0;
  assert(p > 0 && p <= n);
  assert(ref < n);
  int lolimit = ref - p / 2;
  const int hilimit = lolimit + p - 1;
  if (lolimit < 0) {
    lolimit = 0;
  } else if (hilimit >= n) {
    lolimit = n - p;
  }
  int v;
  if (aom_read_bit(r, ACCT_STR_NAME)) {
    v = aom_read_primitive_quniform(r, p, ACCT_STR_NAME) + lolimit;
  } else {
    v = aom_read_primitive_quniform(r, n - p, ACCT_STR_NAME);
    if (v >= lolimit) v += p;
  }
  return v;
}

// Decode finite subexponential code that for a symbol v in [0, n-1] with
// parameter k
uint16_t aom_read_primitive_subexpfin_(aom_reader *r, uint16_t n,
                                       uint16_t k ACCT_STR_PARAM) {
  int i = 0;
  int mk = 0;
  uint16_t v;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      v = aom_read_primitive_quniform(r, n - mk, ACCT_STR_NAME) + mk;
      break;
    } else {
      if (aom_read_bit(r, ACCT_STR_NAME)) {
        i = i + 1;
        mk += a;
      } else {
        v = aom_read_literal(r, b, ACCT_STR_NAME) + mk;
        break;
      }
    }
  }
  return v;
}

static uint16_t aom_rb_read_primitive_subexpfin(struct aom_read_bit_buffer *rb,
                                                uint16_t n, uint16_t k) {
  int i = 0;
  int mk = 0;
  uint16_t v;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      v = aom_rb_read_primitive_quniform(rb, n - mk) + mk;
      break;
    } else {
      if (aom_rb_read_bit(rb)) {
        i = i + 1;
        mk += a;
      } else {
        v = aom_rb_read_literal(rb, b) + mk;
        break;
      }
    }
  }
  return v;
}

uint16_t aom_read_primitive_refsubexpfin_(aom_reader *r, uint16_t n, uint16_t k,
                                          uint16_t ref ACCT_STR_PARAM) {
  return inv_recenter_finite_nonneg(
      n, ref, aom_read_primitive_subexpfin(r, n, k, ACCT_STR_NAME));
}

static uint16_t aom_rb_read_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, uint16_t ref) {
  return inv_recenter_finite_nonneg(n, ref,
                                    aom_rb_read_primitive_subexpfin(rb, n, k));
}

// Decode finite subexponential code that for a symbol v in [-(n-1), n-1] with
// parameter k based on a reference ref also in [-(n-1), n-1].
int16_t aom_read_signed_primitive_refsubexpfin_(aom_reader *r, uint16_t n,
                                                uint16_t k,
                                                int16_t ref ACCT_STR_PARAM) {
  ref += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_read_primitive_refsubexpfin(r, scaled_n, k, ref, ACCT_STR_NAME) -
         n + 1;
}

int16_t aom_rb_read_signed_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, int16_t ref) {
  ref += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_rb_read_primitive_refsubexpfin(rb, scaled_n, k, ref) - n + 1;
}
