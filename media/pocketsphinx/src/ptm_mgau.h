/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/**
 * @file ptm_mgau.h Fast phonetically-tied mixture evaluation.
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#ifndef __PTM_MGAU_H__
#define __PTM_MGAU_H__

/* SphinxBase headesr. */
#include <sphinxbase/fe.h>
#include <sphinxbase/logmath.h>
#include <sphinxbase/mmio.h>

/* Local headers. */
#include "acmod.h"
#include "hmm.h"
#include "bin_mdef.h"
#include "ms_gauden.h"

typedef struct ptm_mgau_s ptm_mgau_t;

typedef struct ptm_topn_s {
    int32 cw;    /**< Codeword index. */
    int32 score; /**< Score. */
} ptm_topn_t;

typedef struct ptm_fast_eval_s {
    ptm_topn_t ***topn;     /**< Top-N for each codebook (mgau x feature x topn) */
    bitvec_t *mgau_active; /**< Set of active codebooks */
} ptm_fast_eval_t;

struct ptm_mgau_s {
    ps_mgau_t base;     /**< base structure. */
    cmd_ln_t *config;   /**< Configuration parameters */
    gauden_t *g;        /**< Set of Gaussians. */
    int32 n_sen;       /**< Number of senones. */
    uint8 *sen2cb;     /**< Senone to codebook mapping. */
    uint8 ***mixw;     /**< Mixture weight distributions by feature, codeword, senone */
    mmio_file_t *sendump_mmap;/* Memory map for mixw (or NULL if not mmap) */
    uint8 *mixw_cb;    /* Mixture weight codebook, if any (assume it contains 16 values) */
    int16 max_topn;
    int16 ds_ratio;

    ptm_fast_eval_t *hist;   /**< Fast evaluation info for past frames. */
    ptm_fast_eval_t *f;      /**< Fast eval info for current frame. */
    int n_fast_hist;         /**< Number of past frames tracked. */

    /* Log-add table for compressed values. */
    logmath_t *lmath_8b;
    /* Log-add object for reloading means/variances. */
    logmath_t *lmath;
};

ps_mgau_t *ptm_mgau_init(acmod_t *acmod, bin_mdef_t *mdef);
void ptm_mgau_free(ps_mgau_t *s);
int ptm_mgau_frame_eval(ps_mgau_t *s,
                        int16 *senone_scores,
                        uint8 *senone_active,
                        int32 n_senone_active,
                        mfcc_t **featbuf,
                        int32 frame,
                        int32 compallsen);
int ptm_mgau_mllr_transform(ps_mgau_t *s,
                            ps_mllr_t *mllr);


#endif /*  __PTM_MGAU_H__ */
