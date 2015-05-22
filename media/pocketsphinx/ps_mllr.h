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
 * @file ps_mllr.h Model-space linear transforms for speaker adaptation
 */

#ifndef __PS_MLLR_H__
#define __PS_MLLR_H__

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>
#include <sphinxbase/ngram_model.h>

/* PocketSphinx headers. */
#include <pocketsphinx_export.h>

/**
 * Feature space linear transform object.
 */
typedef struct ps_mllr_s ps_mllr_t;

/**
 * Read a speaker-adaptive linear transform from a file.
 */
POCKETSPHINX_EXPORT
ps_mllr_t *ps_mllr_read(char const *file);

/**
 * Retain a pointer to a linear transform.
 */
POCKETSPHINX_EXPORT
ps_mllr_t *ps_mllr_retain(ps_mllr_t *mllr);

/**
 * Release a pointer to a linear transform.
 */
POCKETSPHINX_EXPORT
int ps_mllr_free(ps_mllr_t *mllr);

#endif /* __PS_MLLR_H__ */
