/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2010 Carnegie Mellon University.  All rights
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
 * @file ps_alignment.h Multi-level alignment structure
 */

#ifndef __PS_ALIGNMENT_H__
#define __PS_ALIGNMENT_H__

/* System headers. */

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>

/* Local headers. */
#include "dict2pid.h"
#include "hmm.h"

#define PS_ALIGNMENT_NONE ((uint16)0xffff)

struct ps_alignment_entry_s {
    union {
        int32 wid;
        struct {
            uint16 ssid;
            uint16 cipid;
            uint16 tmatid;
        } pid;
        uint16 senid;
    } id;
    int16 start;
    int16 duration;
    uint16 parent;
    uint16 child;
};
typedef struct ps_alignment_entry_s ps_alignment_entry_t;

struct ps_alignment_vector_s {
    ps_alignment_entry_t *seq;
    uint16 n_ent, n_alloc;
};
typedef struct ps_alignment_vector_s ps_alignment_vector_t;

struct ps_alignment_s {
    dict2pid_t *d2p;
    ps_alignment_vector_t word;
    ps_alignment_vector_t sseq;
    ps_alignment_vector_t state;
};
typedef struct ps_alignment_s ps_alignment_t;

struct ps_alignment_iter_s {
    ps_alignment_t *al;
    ps_alignment_vector_t *vec;
    int pos;
};
typedef struct ps_alignment_iter_s ps_alignment_iter_t;

/**
 * Create a new, empty alignment.
 */
ps_alignment_t *ps_alignment_init(dict2pid_t *d2p);

/**
 * Release an alignment
 */
int ps_alignment_free(ps_alignment_t *al);

/**
 * Append a word.
 */
int ps_alignment_add_word(ps_alignment_t *al,
                          int32 wid, int duration);

/**
 * Populate lower layers using available word information.
 */
int ps_alignment_populate(ps_alignment_t *al);

/**
 * Populate lower layers using context-independent phones.
 */
int ps_alignment_populate_ci(ps_alignment_t *al);

/**
 * Propagate timing information up from state sequence.
 */
int ps_alignment_propagate(ps_alignment_t *al);

/**
 * Number of words.
 */
int ps_alignment_n_words(ps_alignment_t *al);

/**
 * Number of phones.
 */
int ps_alignment_n_phones(ps_alignment_t *al);

/**
 * Number of states.
 */
int ps_alignment_n_states(ps_alignment_t *al);

/**
 * Iterate over the alignment starting at the first word.
 */
ps_alignment_iter_t *ps_alignment_words(ps_alignment_t *al);

/**
 * Iterate over the alignment starting at the first phone.
 */
ps_alignment_iter_t *ps_alignment_phones(ps_alignment_t *al);

/**
 * Iterate over the alignment starting at the first state.
 */
ps_alignment_iter_t *ps_alignment_states(ps_alignment_t *al);

/**
 * Get the alignment entry pointed to by an iterator.
 */
ps_alignment_entry_t *ps_alignment_iter_get(ps_alignment_iter_t *itor);

/**
 * Move alignment iterator to given index.
 */
ps_alignment_iter_t *ps_alignment_iter_goto(ps_alignment_iter_t *itor, int pos);

/**
 * Move an alignment iterator forward.
 */
ps_alignment_iter_t *ps_alignment_iter_next(ps_alignment_iter_t *itor);

/**
 * Move an alignment iterator back.
 */
ps_alignment_iter_t *ps_alignment_iter_prev(ps_alignment_iter_t *itor);

/**
 * Get a new iterator starting at the parent of the current node.
 */
ps_alignment_iter_t *ps_alignment_iter_up(ps_alignment_iter_t *itor);
/**
 * Get a new iterator starting at the first child of the current node.
 */
ps_alignment_iter_t *ps_alignment_iter_down(ps_alignment_iter_t *itor);

/**
 * Release an iterator before completing all iterations.
 */
int ps_alignment_iter_free(ps_alignment_iter_t *itor);

#endif /* __PS_ALIGNMENT_H__ */
