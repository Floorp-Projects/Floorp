/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2013 Carnegie Mellon University.  All rights
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
 * kws_search.h -- Search structures for keyword spotting.
 */

#ifndef __KWS_SEARCH_H__
#define __KWS_SEARCH_H__

/* SphinxBase headers. */
#include <sphinxbase/glist.h>
#include <sphinxbase/cmd_ln.h>

/* Local headers. */
#include "pocketsphinx_internal.h"
#include "kws_detections.h"
#include "hmm.h"

/**
 * Segmentation "iterator" for KWS history.
 */
typedef struct kws_seg_s {
    ps_seg_t base;       /**< Base structure. */
    gnode_t *detection;  /**< Keyword detection correspondent to segment. */
} kws_seg_t;

typedef struct kws_keyword_s {
    char* word;
    int32 threshold;
    hmm_t* hmms;
    int32 n_hmms;
} kws_keyword_t;

/**
 * Implementation of KWS search structure.
 */
typedef struct kws_search_s {
    ps_search_t base;

    hmm_context_t *hmmctx;        /**< HMM context. */

    kws_detections_t *detections; /**< Keyword spotting history */
    kws_keyword_t* keyphrases;   /**< Keyphrases to spot */
    int n_keyphrases;             /**< Keyphrases amount */
    frame_idx_t frame;            /**< Frame index */

    int32 beam;

    int32 plp;                    /**< Phone loop probability */
    int32 bestscore;              /**< For beam pruning */
    int32 def_threshold;          /**< default threshold for p(hyp)/p(altern) ratio */

    int32 n_pl;                   /**< Number of CI phones */
    hmm_t *pl_hmms;               /**< Phone loop hmms - hmms of CI phones */

} kws_search_t;

/**
 * Create, initialize and return a search module. Gets keywords either
 * from keyphrase or from a keyphrase file.
 */
ps_search_t *kws_search_init(const char *keyphrase,
			     const char *keyfile,
                             cmd_ln_t * config,
                             acmod_t * acmod,
                             dict_t * dict, dict2pid_t * d2p);

/**
 * Deallocate search structure.
 */
void kws_search_free(ps_search_t * search);

/**
 * Update KWS search module for new key phrase.
 */
int kws_search_reinit(ps_search_t * kwss, dict_t * dict, dict2pid_t * d2p);

/**
 * Prepare the KWS search structure for beginning decoding of the next
 * utterance.
 */
int kws_search_start(ps_search_t * search);

/**
 * Step one frame forward through the Viterbi search.
 */
int kws_search_step(ps_search_t * search, int frame_idx);

/**
 * Windup and clean the KWS search structure after utterance.
 */
int kws_search_finish(ps_search_t * search);

/**
 * Get hypothesis string from the KWS search.
 */
char const *kws_search_hyp(ps_search_t * search, int32 * out_score,
                           int32 * out_is_final);
/**
 * Get active keyphrases
 */
char* kws_search_get_keywords(ps_search_t * search);

#endif                          /* __KWS_SEARCH_H__ */
