/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2007 Carnegie Mellon University.  All rights
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
/*
 * \file ngram_model_arpa.h ARPABO text format for N-Gram models
 *
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#ifndef __NGRAM_MODEL_ARPA_H__
#define __NGRAM_MODEL_ARPA_H__

#include "ngram_model_internal.h"
#include "lm3g_model.h"

/**
 * Bigram structure.
 */
struct bigram_s {
    uint32 wid;	/**< Index of unigram entry for this.  (NOT dictionary id.) */
    uint16 prob2;	/**< Index into array of actual bigram probs */
    uint16 bo_wt2;	/**< Index into array of actual bigram backoff wts */
    uint16 trigrams;	/**< Index of 1st entry in lm_t.trigrams[],
			     RELATIVE TO its segment base (see above) */
};

/**
 * Trigram structure.
 *
 * As with bigrams, trigram prob info kept in a separate table for conserving
 * memory space.
 */
struct trigram_s {
    uint32 wid;	  /**< Index of unigram entry for this.  (NOT dictionary id.) */
    uint16 prob3; /**< Index into array of actual trigram probs */
};


/**
 * Subclass of ngram_model for ARPA file reading.
 */
typedef struct ngram_model_arpa_s {
    ngram_model_t base;  /**< Base ngram_model_t structure */
    lm3g_model_t lm3g;  /**< Shared lm3g structure */

    /* Arrays of unique bigram probs and bo-wts, and trigram probs
     * (these are temporary, actually) */
    sorted_list_t sorted_prob2;
    sorted_list_t sorted_bo_wt2;
    sorted_list_t sorted_prob3;
} ngram_model_arpa_t;

#endif /* __NGRAM_MODEL_ARPA_H__ */
