/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2008 Carnegie Mellon University.  All rights
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
 * @file ngram_search_fwdtree.h Lexicon tree based Viterbi search.
 */

#ifndef __NGRAM_SEARCH_FWDTREE_H__
#define __NGRAM_SEARCH_FWDTREE_H__

/* SphinxBase headers. */

/* Local headers. */
#include "ngram_search.h"

/**
 * Initialize N-Gram search for fwdtree decoding.
 */
void ngram_fwdtree_init(ngram_search_t *ngs);

/**
 * Release memory associated with fwdtree decoding.
 */
void ngram_fwdtree_deinit(ngram_search_t *ngs);

/**
 * Rebuild search structures for updated language models.
 */
int ngram_fwdtree_reinit(ngram_search_t *ngs);

/**
 * Start fwdtree decoding for an utterance.
 */
void ngram_fwdtree_start(ngram_search_t *ngs);

/**
 * Search one frame forward in an utterance.
 *
 * @return Number of frames searched (either 0 or 1).
 */
int ngram_fwdtree_search(ngram_search_t *ngs, int frame_idx);

/**
 * Finish fwdtree decoding for an utterance.
 */
void ngram_fwdtree_finish(ngram_search_t *ngs);


#endif /* __NGRAM_SEARCH_FWDTREE_H__ */
