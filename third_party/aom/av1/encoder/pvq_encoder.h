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

#if !defined(_pvq_encoder_H)
# define _pvq_encoder_H (1)
# include "aom_dsp/bitwriter.h"
# include "aom_dsp/entenc.h"
# include "av1/common/blockd.h"
# include "av1/common/pvq.h"
# include "av1/encoder/encint.h"

void aom_write_symbol_pvq(aom_writer *w, int symb, aom_cdf_prob *cdf,
    int nsymbs);

void aom_encode_band_pvq_splits(aom_writer *w, od_pvq_codeword_ctx *adapt,
 const int *y, int n, int k, int level);

void aom_laplace_encode_special(aom_writer *w, int x, unsigned decay);

void pvq_encode_partition(aom_writer *w,
                                 int qg,
                                 int theta,
                                 const od_coeff *in,
                                 int n,
                                 int k,
                                 generic_encoder model[3],
                                 od_adapt_ctx *adapt,
                                 int *exg,
                                 int *ext,
                                 int cdf_ctx,
                                 int is_keyframe,
                                 int code_skip,
                                 int skip_rest,
                                 int encode_flip,
                                 int flip);

PVQ_SKIP_TYPE od_pvq_encode(daala_enc_ctx *enc, od_coeff *ref,
    const od_coeff *in, od_coeff *out, int q_dc, int q_ac, int pli, int bs,
    const od_val16 *beta, int is_keyframe,
    const int16_t *qm, const int16_t *qm_inv, int speed,
    PVQ_INFO *pvq_info);

#endif
