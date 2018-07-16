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

#ifndef AOM_DSP_BITREADER_H_
#define AOM_DSP_BITREADER_H_

#include <assert.h>
#include <limits.h>

#include "config/aom_config.h"

#include "aom/aomdx.h"
#include "aom/aom_integer.h"
#include "aom_dsp/daalaboolreader.h"
#include "aom_dsp/prob.h"
#include "av1/common/odintrin.h"

#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#define ACCT_STR_NAME acct_str
#define ACCT_STR_PARAM , const char *ACCT_STR_NAME
#define ACCT_STR_ARG(s) , s
#else
#define ACCT_STR_PARAM
#define ACCT_STR_ARG(s)
#endif

#define aom_read(r, prob, ACCT_STR_NAME) \
  aom_read_(r, prob ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_bit(r, ACCT_STR_NAME) \
  aom_read_bit_(r ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_tree(r, tree, probs, ACCT_STR_NAME) \
  aom_read_tree_(r, tree, probs ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_literal(r, bits, ACCT_STR_NAME) \
  aom_read_literal_(r, bits ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_cdf(r, cdf, nsymbs, ACCT_STR_NAME) \
  aom_read_cdf_(r, cdf, nsymbs ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_read_symbol(r, cdf, nsymbs, ACCT_STR_NAME) \
  aom_read_symbol_(r, cdf, nsymbs ACCT_STR_ARG(ACCT_STR_NAME))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct daala_reader aom_reader;

static INLINE int aom_reader_init(aom_reader *r, const uint8_t *buffer,
                                  size_t size) {
  return aom_daala_reader_init(r, buffer, (int)size);
}

static INLINE const uint8_t *aom_reader_find_begin(aom_reader *r) {
  return aom_daala_reader_find_begin(r);
}

static INLINE const uint8_t *aom_reader_find_end(aom_reader *r) {
  return aom_daala_reader_find_end(r);
}

static INLINE int aom_reader_has_error(aom_reader *r) {
  return aom_daala_reader_has_error(r);
}

// Returns the position in the bit reader in bits.
static INLINE uint32_t aom_reader_tell(const aom_reader *r) {
  return aom_daala_reader_tell(r);
}

// Returns the position in the bit reader in 1/8th bits.
static INLINE uint32_t aom_reader_tell_frac(const aom_reader *r) {
  return aom_daala_reader_tell_frac(r);
}

#if CONFIG_ACCOUNTING
static INLINE void aom_process_accounting(const aom_reader *r ACCT_STR_PARAM) {
  if (r->accounting != NULL) {
    uint32_t tell_frac;
    tell_frac = aom_reader_tell_frac(r);
    aom_accounting_record(r->accounting, ACCT_STR_NAME,
                          tell_frac - r->accounting->last_tell_frac);
    r->accounting->last_tell_frac = tell_frac;
  }
}

static INLINE void aom_update_symb_counts(const aom_reader *r, int is_binary) {
  if (r->accounting != NULL) {
    r->accounting->syms.num_multi_syms += !is_binary;
    r->accounting->syms.num_binary_syms += !!is_binary;
  }
}
#endif

static INLINE int aom_read_(aom_reader *r, int prob ACCT_STR_PARAM) {
  int ret;
  ret = aom_daala_read(r, prob);
#if CONFIG_ACCOUNTING
  if (ACCT_STR_NAME) aom_process_accounting(r, ACCT_STR_NAME);
  aom_update_symb_counts(r, 1);
#endif
  return ret;
}

static INLINE int aom_read_bit_(aom_reader *r ACCT_STR_PARAM) {
  int ret;
  ret = aom_read(r, 128, NULL);  // aom_prob_half
#if CONFIG_ACCOUNTING
  if (ACCT_STR_NAME) aom_process_accounting(r, ACCT_STR_NAME);
#endif
  return ret;
}

static INLINE int aom_read_literal_(aom_reader *r, int bits ACCT_STR_PARAM) {
  int literal = 0, bit;

  for (bit = bits - 1; bit >= 0; bit--) literal |= aom_read_bit(r, NULL) << bit;
#if CONFIG_ACCOUNTING
  if (ACCT_STR_NAME) aom_process_accounting(r, ACCT_STR_NAME);
#endif
  return literal;
}

static INLINE int aom_read_cdf_(aom_reader *r, const aom_cdf_prob *cdf,
                                int nsymbs ACCT_STR_PARAM) {
  int ret;
  ret = daala_read_symbol(r, cdf, nsymbs);

#if CONFIG_ACCOUNTING
  if (ACCT_STR_NAME) aom_process_accounting(r, ACCT_STR_NAME);
  aom_update_symb_counts(r, (nsymbs == 2));
#endif
  return ret;
}

static INLINE int aom_read_symbol_(aom_reader *r, aom_cdf_prob *cdf,
                                   int nsymbs ACCT_STR_PARAM) {
  int ret;
  ret = aom_read_cdf(r, cdf, nsymbs, ACCT_STR_NAME);
  if (r->allow_update_cdf) update_cdf(cdf, ret, nsymbs);
  return ret;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_BITREADER_H_
