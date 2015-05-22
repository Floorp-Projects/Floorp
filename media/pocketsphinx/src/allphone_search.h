/* -*- c-basic-offset:4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2014 Carnegie Mellon University.  All rights
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

/*
 * allphone_search.h -- Search structures for phoneme decoding.
 */


#ifndef __ALLPHONE_SEARCH_H__
#define __ALLPHONE_SEARCH_H__


/* SphinxBase headers. */
#include <sphinxbase/glist.h>
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/ngram_model.h>
#include <sphinxbase/bitvec.h>

/* Local headers. */
#include "pocketsphinx_internal.h"
#include "blkarray_list.h"
#include "hmm.h"

/**
 * Models a single unique <senone-sequence, tmat> pair.
 * Can represent several different triphones, but all with the same parent basephone.
 * (NOTE: Word-position attribute of triphone is ignored.)
 */
typedef struct phmm_s {
    hmm_t hmm;          /**< Base HMM structure */
    s3pid_t pid;        /**< Phone id (temp. during init.) */
    s3cipid_t ci;       /**< Parent basephone for this PHMM */
    bitvec_t *lc;         /**< Set (bit-vector) of left context phones seen for this PHMM */
    bitvec_t *rc;         /**< Set (bit-vector) of right context phones seen for this PHMM */
    struct phmm_s *next;        /**< Next unique PHMM for same parent basephone */
    struct plink_s *succlist;   /**< List of predecessor PHMM nodes */
} phmm_t;

/**
 * List of links from a PHMM node to its successors; one link per successor.
 */
typedef struct plink_s {
    phmm_t *phmm;               /**< Successor PHMM node */
    struct plink_s *next;       /**< Next link for parent PHMM node */
} plink_t;

/**
 * History (paths) information at any point in allphone Viterbi search.
 */
typedef struct history_s {
    phmm_t *phmm;       /**< PHMM ending this path */
    int32 score;        /**< Path score for this path */
    int32 tscore;       /**< Transition score for this path */
    frame_idx_t ef;     /**< End frame */
    int32 hist;         /**< Previous history entry */
} history_t;

/**
 * Phone level segmentation information
 */
typedef struct phseg_s {
    s3cipid_t ci;               /* CI-phone id */
    frame_idx_t sf, ef;         /* Start and end frame for this phone occurrence */
    int32 score;                /* Acoustic score for this segment of alignment */
    int32 tscore;               /* Transition ("LM") score for this segment */
} phseg_t;

/**
 * Segment iterator over list of phseg
 */
typedef struct phseg_iter_s {
    ps_seg_t base;
    glist_t seg;
} phseg_iter_t;

/**
 * Implementation of allphone search structure.
 */
typedef struct allphone_search_s {
    ps_search_t base;

    hmm_context_t *hmmctx;    /**< HMM context. */
    ngram_model_t *lm;        /**< Ngram model set */
    int32 ci_only; 	      /**< Use context-independent phones for decoding */
    phmm_t **ci_phmm;         /**< PHMM lists (for each CI phone) */
    int32 *ci2lmwid;          /**< Mapping of CI phones to LM word IDs */

    int32 beam, pbeam;        /**< Effective beams after applying beam_factor */
    int32 lw, inspen;         /**< Language weights */

    frame_idx_t frame;          /**< Current frame. */
    float32 ascale;           /**< Acoustic score scale for posterior probabilities. */

    int32 n_tot_frame;         /**< Total number of frames processed */
    int32 n_hmm_eval;          /**< Total HMMs evaluated this utt */
    int32 n_sen_eval;          /**< Total senones evaluated this utt */

    /* Backtrace information */
    blkarray_list_t *history;     /**< List of history nodes allocated in each frame */
    /* Hypothesis DAG */
    glist_t segments;

    ptmr_t perf; /**< Performance counter */

} allphone_search_t;

/**
 * Create, initialize and return a search module.
 */
ps_search_t *allphone_search_init(ngram_model_t * lm,
                                  cmd_ln_t * config,
                                  acmod_t * acmod,
                                  dict_t * dict, dict2pid_t * d2p);

/**
 * Deallocate search structure.
 */
void allphone_search_free(ps_search_t * search);

/**
 * Update allphone search module.
 */
int allphone_search_reinit(ps_search_t * search, dict_t * dict,
                           dict2pid_t * d2p);

/**
 * Prepare the allphone search structure for beginning decoding of the next
 * utterance.
 */
int allphone_search_start(ps_search_t * search);

/**
 * Step one frame forward through the Viterbi search.
 */
int allphone_search_step(ps_search_t * search, int frame_idx);

/**
 * Windup and clean the allphone search structure after utterance.
 */
int allphone_search_finish(ps_search_t * search);

/**
 * Get hypothesis string from the allphone search.
 */
char const *allphone_search_hyp(ps_search_t * search, int32 * out_score,
                                int32 * out_is_final);

#endif                          /* __ALLPHONE_SEARCH_H__ */
