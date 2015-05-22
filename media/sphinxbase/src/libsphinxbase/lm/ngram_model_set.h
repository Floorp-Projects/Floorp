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
/**
 * @file ngram_model_set.h Set of language models.
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#ifndef __NGRAM_MODEL_SET_H__
#define __NGRAM_MODEL_SET_H__

#include "ngram_model_internal.h"
#include "lm3g_model.h"

/**
 * Subclass of ngram_model for grouping language models.
 */
typedef struct ngram_model_set_s {
    ngram_model_t base;  /**< Base ngram_model_t structure. */

    int32 n_models;      /**< Number of models in this set. */
    int32 cur;           /**< Currently selected model, or -1 for none. */
    ngram_model_t **lms; /**< Language models in this set. */
    char **names;        /**< Names for language models. */
    int32 *lweights;     /**< Log interpolation weights. */
    int32 **widmap;      /**< Word ID mapping for submodels. */
    int32 *maphist;      /**< Word ID mapping for N-Gram history. */
} ngram_model_set_t;

/**
 * Iterator over a model set.
 */
struct ngram_model_set_iter_s {
    ngram_model_set_t *set;
    int32 cur;
};

#endif /* __NGRAM_MODEL_SET_H__ */
