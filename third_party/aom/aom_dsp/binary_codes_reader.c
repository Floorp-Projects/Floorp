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

// Decode finite subexponential code that for a symbol v in [0, n-1] with
// parameter k
uint16_t aom_read_primitive_subexpfin_(aom_reader *r, uint16_t n,
                                       uint16_t k ACCT_STR_PARAM) {
  int i = 0;
  int mk = 0;

  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);

    if (n <= mk + 3 * a) {
      return aom_read_primitive_quniform(r, n - mk, ACCT_STR_NAME) + mk;
    }

    if (!aom_read_bit(r, ACCT_STR_NAME)) {
      return aom_read_literal(r, b, ACCT_STR_NAME) + mk;
    }

    i = i + 1;
    mk += a;
  }

  assert(0);
  return 0;
}

static uint16_t aom_rb_read_primitive_subexpfin(struct aom_read_bit_buffer *rb,
                                                uint16_t n, uint16_t k) {
  int i = 0;
  int mk = 0;

  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);

    if (n <= mk + 3 * a) {
      return aom_rb_read_primitive_quniform(rb, n - mk) + mk;
    }

    if (!aom_rb_read_bit(rb)) {
      return aom_rb_read_literal(rb, b) + mk;
    }

    i = i + 1;
    mk += a;
  }

  assert(0);
  return 0;
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

int16_t aom_rb_read_signed_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, int16_t ref) {
  ref += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_rb_read_primitive_refsubexpfin(rb, scaled_n, k, ref) - n + 1;
}

uint32_t aom_rb_read_uvlc(struct aom_read_bit_buffer *rb) {
  int leading_zeros = 0;
  while (!aom_rb_read_bit(rb)) ++leading_zeros;
  // Maximum 32 bits.
  if (leading_zeros >= 32) return UINT32_MAX;
  const uint32_t base = (1u << leading_zeros) - 1;
  const uint32_t value = aom_rb_read_literal(rb, leading_zeros);
  return base + value;
}
