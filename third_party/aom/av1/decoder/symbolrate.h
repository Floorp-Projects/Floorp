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

#ifndef AV1_DECODER_SYMBOLRATE_H_
#define AV1_DECODER_SYMBOLRATE_H_

#if CONFIG_SYMBOLRATE
static INLINE void av1_dump_symbol_rate(struct AV1Common *cm) {
  const FRAME_COUNTS *counts = &cm->counts;
  printf("%d %d %d %d\n", counts->coeff_num[0], counts->coeff_num[1],
         counts->symbol_num[0], counts->symbol_num[1]);
}
static INLINE int av1_read_record_symbol(FRAME_COUNTS *counts, aom_reader *r,
                                         aom_cdf_prob *cdf, int nsymbs,
                                         const char *str) {
  (void)str;
  if (counts) ++counts->symbol_num[0];
  return aom_read_symbol(r, cdf, nsymbs, str);
}

#if CONFIG_LV_MAP
static INLINE int av1_read_record_bin(FRAME_COUNTS *counts, aom_reader *r,
                                      aom_cdf_prob *cdf, int nsymbs,
                                      const char *str) {
  (void)str;
  if (counts) ++counts->symbol_num[0];
  return aom_read_bin(r, cdf, nsymbs, str);
}
#endif

static INLINE int av1_read_record(FRAME_COUNTS *counts, aom_reader *r, int prob,
                                  const char *str) {
  (void)str;
  if (counts) ++counts->symbol_num[0];
  return aom_read(r, prob, str);
}

static INLINE int av1_read_record_cdf(FRAME_COUNTS *counts, aom_reader *r,
                                      const aom_cdf_prob *cdf, int nsymbs,
                                      const char *str) {
  (void)str;
  if (counts) ++counts->symbol_num[0];
  return aom_read_cdf(r, cdf, nsymbs, str);
}

static INLINE int av1_read_record_bit(FRAME_COUNTS *counts, aom_reader *r,
                                      const char *str) {
  (void)str;
  if (counts) ++counts->symbol_num[1];
  return aom_read_bit(r, str);
}

static INLINE void av1_record_coeff(FRAME_COUNTS *counts, tran_low_t qcoeff) {
  assert(qcoeff >= 0);
  if (counts) ++counts->coeff_num[qcoeff != 0];
}
#else  // CONFIG_SYMBOLRATE

#define av1_read_record_symbol(counts, r, cdf, nsymbs, ACCT_STR_NAME) \
  aom_read_symbol(r, cdf, nsymbs, ACCT_STR_NAME)

#if CONFIG_LV_MAP
#define av1_read_record_bin(counts, r, cdf, nsymbs, ACCT_STR_NAME) \
  aom_read_bin(r, cdf, nsymbs, ACCT_STR_NAME)
#endif

#define av1_read_record(counts, r, prob, ACCT_STR_NAME) \
  aom_read(r, prob, ACCT_STR_NAME)

#define av1_read_record_cdf(counts, r, cdf, nsymbs, ACCT_STR_NAME) \
  aom_read_cdf(r, cdf, nsymbs, ACCT_STR_NAME)

#define av1_read_record_bit(counts, r, ACCT_STR_NAME) \
  aom_read_bit(r, ACCT_STR_NAME)

#endif  // CONFIG_SYMBOLRATE

#endif  // AV1_DECODER_SYMBOLRATE_H_
