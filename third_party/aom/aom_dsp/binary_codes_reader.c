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

#include "aom_dsp/bitreader.h"

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

int16_t aom_read_primitive_symmetric(aom_reader *r, unsigned int mag_bits) {
  if (aom_read_bit(r, NULL)) {
    int s = aom_read_bit(r, NULL);
    int16_t x = aom_read_literal(r, mag_bits, NULL) + 1;
    return (s > 0 ? -x : x);
  } else {
    return 0;
  }
}

uint16_t aom_read_primitive_quniform(aom_reader *r, uint16_t n) {
  if (n <= 1) return 0;
  const int l = get_msb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = aom_read_literal(r, l - 1, NULL);
  return v < m ? v : (v << 1) - m + aom_read_bit(r, NULL);
}

uint16_t aom_read_primitive_refbilevel(aom_reader *r, uint16_t n, uint16_t p,
                                       uint16_t ref) {
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
  if (aom_read_bit(r, NULL)) {
    v = aom_read_primitive_quniform(r, p) + lolimit;
  } else {
    v = aom_read_primitive_quniform(r, n - p);
    if (v >= lolimit) v += p;
  }
  return v;
}

// Decode finite subexponential code that for a symbol v in [0, n-1] with
// parameter k
uint16_t aom_read_primitive_subexpfin(aom_reader *r, uint16_t n, uint16_t k) {
  int i = 0;
  int mk = 0;
  uint16_t v;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      v = aom_read_primitive_quniform(r, n - mk) + mk;
      break;
    } else {
      if (aom_read_bit(r, NULL)) {
        i = i + 1;
        mk += a;
      } else {
        v = aom_read_literal(r, b, NULL) + mk;
        break;
      }
    }
  }
  return v;
}

// Decode finite subexponential code that for a symbol v in [0, n-1] with
// parameter k
// based on a reference ref also in [0, n-1].
uint16_t aom_read_primitive_refsubexpfin(aom_reader *r, uint16_t n, uint16_t k,
                                         uint16_t ref) {
  return inv_recenter_finite_nonneg(n, ref,
                                    aom_read_primitive_subexpfin(r, n, k));
}

// Decode finite subexponential code that for a symbol v in [-(n-1), n-1] with
// parameter k based on a reference ref also in [-(n-1), n-1].
int16_t aom_read_signed_primitive_refsubexpfin(aom_reader *r, uint16_t n,
                                               uint16_t k, int16_t ref) {
  ref += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_read_primitive_refsubexpfin(r, scaled_n, k, ref) - n + 1;
}
