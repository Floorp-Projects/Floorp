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
 * \file lm3g_model.h Core Sphinx 3-gram code used in
 * DMP/DMP32/ARPA (for now) model code.
 *
 * Author: A cast of thousands, probably.
 */

#ifndef __NGRAM_MODEL_LM3G_H__
#define __NGRAM_MODEL_LM3G_H__

#include "sphinxbase/listelem_alloc.h"

#include "ngram_model_internal.h"

/**
 * Type used to store language model probabilities
 */
typedef union {
    float32 f;
    int32 l;
} lmprob_t;

/**
 * Bigram probs and bo-wts, and trigram probs are kept in separate
 * tables rather than within the bigram_t and trigram_t structures.
 * These tables hold unique prob and bo-wt values. The following tree
 * structure is used to construct these tables of unique values.  
 * Whenever a new value is read from the LM file, the sorted tree 
 * structure is searched to see if the value already exists, and 
 * inserted if not found.
 */
typedef struct sorted_entry_s {
    lmprob_t val;               /**< value being kept in this node */
    uint32 lower;               /**< index of another entry.  All descendants down
                                   this path have their val < this node's val.
                                   0 => no son exists (0 is root index) */
    uint32 higher;              /**< index of another entry.  All descendants down
                                   this path have their val > this node's val
                                   0 => no son exists (0 is root index) */
} sorted_entry_t;

/**
 * The sorted list.  list is a (64K long) array.  The first entry is the
 * root of the tree and is created during initialization.
 */
typedef struct {
    sorted_entry_t *list;
    int32 free;                 /**< first free element in list */
    int32 size;
} sorted_list_t;

/**
 * Unigram structure (common among all lm3g implementations)
 */
typedef struct unigram_s {
    lmprob_t prob1;     /**< Unigram probability. */
    lmprob_t bo_wt1;    /**< Unigram backoff weight. */
    int32 bigrams;	/**< Index of 1st entry in lm_t.bigrams[] */
} unigram_t;

/**
 * Bigram structure (might be implemented differently)
 */
typedef struct bigram_s bigram_t;
/**
 * Trigram structure (might be implemented differently)
 */
typedef struct trigram_s trigram_t;


/*
 * To conserve space, bigram info is kept in many tables.  Since the number
 * of distinct values << #bigrams, these table indices can be 16-bit values.
 * prob2 and bo_wt2 are such indices, but keeping trigram index is less easy.
 * It is supposed to be the index of the first trigram entry for each bigram.
 * But such an index cannot be represented in 16-bits, hence the following
 * segmentation scheme: Partition bigrams into segments of BG_SEG_SZ
 * consecutive entries, such that #trigrams in each segment <= 2**16 (the
 * corresponding trigram segment).  The bigram_t.trigrams value is then a
 * 16-bit relative index within the trigram segment.  A separate table--
 * lm_t.tseg_base--has the index of the 1st trigram for each bigram segment.
 */
#define BG_SEG_SZ	512	/* chosen so that #trigram/segment <= 2**16 */
#define LOG_BG_SEG_SZ	9

/**
 * Trigram information cache.
 *
 * The following trigram information cache eliminates most traversals of 1g->2g->3g
 * tree to locate trigrams for a given bigram (lw1,lw2).  The organization is optimized
 * for locality of access (to the same lw1), given lw2.
 */
typedef struct tginfo_s {
    int32 w1;			/**< lw1 component of bigram lw1,lw2.  All bigrams with
				   same lw2 linked together (see lm_t.tginfo). */
    int32 n_tg;			/**< number tg for parent bigram lw1,lw2 */
    int32 bowt;                 /**< tg bowt for lw1,lw2 */
    int32 used;			/**< whether used since last lm_reset */
    trigram_t *tg;		/**< Trigrams for lw1,lw2 */
    struct tginfo_s *next;      /**< Next lw1 with same parent lw2; NULL if none. */
} tginfo_t;

/**
 * Common internal structure for Sphinx 3-gram models.
 */
typedef struct lm3g_model_s {
    unigram_t *unigrams;
    bigram_t *bigrams;
    trigram_t *trigrams;
    lmprob_t *prob2;	     /**< Table of actual bigram probs */
    int32 n_prob2;	     /**< prob2 size */
    lmprob_t *bo_wt2;	     /**< Table of actual bigram backoff weights */
    int32 n_bo_wt2;	     /**< bo_wt2 size */
    lmprob_t *prob3;	     /**< Table of actual trigram probs */
    int32 n_prob3;	     /**< prob3 size */
    int32 *tseg_base;    /**< tseg_base[i>>LOG_BG_SEG_SZ] = index of 1st
                            trigram for bigram segment (i>>LOG_BG_SEG_SZ) */
    tginfo_t **tginfo;   /**< tginfo[lw2] is head of linked list of trigram information for
                            some cached subset of bigrams (*,lw2). */
    listelem_alloc_t *le; /**< List element allocator for tginfo. */
} lm3g_model_t;

void lm3g_tginfo_free(ngram_model_t *base, lm3g_model_t *lm3g);
void lm3g_tginfo_reset(ngram_model_t *base, lm3g_model_t *lm3g);
void lm3g_apply_weights(ngram_model_t *base,
			lm3g_model_t *lm3g,
			float32 lw, float32 wip, float32 uw);
int32 lm3g_add_ug(ngram_model_t *base,
                  lm3g_model_t *lm3g, int32 wid, int32 lweight);


/**
 * Initialize sorted list with the 0-th entry = MIN_PROB_F, which may be needed
 * to replace spurious values in the Darpa LM file.
 */
void init_sorted_list(sorted_list_t *l);
void free_sorted_list(sorted_list_t *l);
lmprob_t *vals_in_sorted_list(sorted_list_t *l);
int32 sorted_id(sorted_list_t * l, int32 *val);

#endif /* __NGRAM_MODEL_LM3G_H__ */
