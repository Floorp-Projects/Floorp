/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#if !defined(_generic_code_H)
# define _generic_code_H

# include "aom_dsp/bitreader.h"
# include "aom_dsp/bitwriter.h"

# define GENERIC_TABLES 12

#define generic_decode(r, model, ex_q16, integration, ACCT_STR_NAME) \
  generic_decode_(r, model, ex_q16, integration ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_decode_cdf_adapt_q15(r, cdf, n, count, rate, ACCT_STR_NAME) \
  aom_decode_cdf_adapt_q15_(r, cdf, n, count, rate ACCT_STR_ARG(ACCT_STR_NAME))
#define aom_decode_cdf_adapt(r, cdf, n, increment, ACCT_STR_NAME) \
  aom_decode_cdf_adapt_(r, cdf, n, increment ACCT_STR_ARG(ACCT_STR_NAME))

typedef struct {
  /** cdf for multiple expectations of x */
  uint16_t cdf[GENERIC_TABLES][CDF_SIZE(16)];
} generic_encoder;

#define OD_IIR_DIADIC(y, x, shift) ((y) += ((x) - (y)) >> (shift))

void generic_model_init(generic_encoder *model);

/* Initialize a CDF for use by aom_write_symbol_pvq()/aom_read_symbol_pvq().
   This is used for CDFs whose size might not match the declared array size.
   The only real requirement is that the first value of every CDF be zero.
   Then aom_cdf_init_q15_1D() will be called with the real size the first time
   the CDF is used. */
#define OD_CDFS_INIT_DYNAMIC(cdf) (memset(cdf, 0, sizeof(cdf)))

// WARNING: DO NOT USE this init function,
// if the size of cdf is different from what is declared by code.
#define OD_CDFS_INIT_Q15(cdfs) \
  { int n_cdfs = sizeof(cdfs)/sizeof(cdfs[0]); \
    int cdf_size = sizeof(cdfs[0])/sizeof(cdfs[0][0]); \
    int nsyms = cdf_size - 1; \
    int i_; \
    for (i_ = 0; i_ < n_cdfs; i_++) \
      aom_cdf_init_q15_1D(cdfs[i_], nsyms, cdf_size); \
  }

void aom_cdf_init(uint16_t *cdf, int ncdfs, int nsyms, int val, int first);

void aom_cdf_init_q15_1D(uint16_t *cdf, int nsyms, int cdf_size);

void aom_cdf_adapt_q15(int val, uint16_t *cdf, int n, int *count, int rate);

void aom_encode_cdf_adapt_q15(aom_writer *w, int val, uint16_t *cdf, int n,
 int *count, int rate);

void generic_encode(aom_writer *w, generic_encoder *model, int x,
 int *ex_q16, int integration);
double generic_encode_cost(generic_encoder *model, int x, int *ex_q16);

double od_encode_cdf_cost(int val, uint16_t *cdf, int n);

int aom_decode_cdf_adapt_q15_(aom_reader *r, uint16_t *cdf, int n,
 int *count, int rate ACCT_STR_PARAM);

int generic_decode_(aom_reader *r, generic_encoder *model,
 int *ex_q16, int integration ACCT_STR_PARAM);

int log_ex(int ex_q16);

void generic_model_update(int *ex_q16, int x, int integration);

#endif
