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
 * @file ps_alignment.c Multi-level alignment structure
 */

/* System headers. */

/* SphinxBase headers. */
#include <sphinxbase/ckd_alloc.h>

/* Local headers. */
#include "ps_alignment.h"

ps_alignment_t *
ps_alignment_init(dict2pid_t *d2p)
{
    ps_alignment_t *al = ckd_calloc(1, sizeof(*al));
    al->d2p = dict2pid_retain(d2p);
    return al;
}

int
ps_alignment_free(ps_alignment_t *al)
{
    if (al == NULL)
        return 0;
    dict2pid_free(al->d2p);
    ckd_free(al->word.seq);
    ckd_free(al->sseq.seq);
    ckd_free(al->state.seq);
    ckd_free(al);
    return 0;
}

#define VECTOR_GROW 10
static void *
vector_grow_one(void *ptr, uint16 *n_alloc, uint16 *n, size_t item_size)
{
    int newsize = *n + 1;
    if (newsize < *n_alloc) {
        *n += 1;
        return ptr;
    }
    newsize += VECTOR_GROW;
    if (newsize > 0xffff)
        return NULL;
    ptr = ckd_realloc(ptr, newsize * item_size);
    *n += 1;
    *n_alloc = newsize;
    return ptr;
}

static ps_alignment_entry_t *
ps_alignment_vector_grow_one(ps_alignment_vector_t *vec)
{
    void *ptr;
    ptr = vector_grow_one(vec->seq, &vec->n_alloc,
                          &vec->n_ent, sizeof(*vec->seq));
    if (ptr == NULL)
        return NULL;
    vec->seq = ptr;
    return vec->seq + vec->n_ent - 1;
}

static void
ps_alignment_vector_empty(ps_alignment_vector_t *vec)
{
    vec->n_ent = 0;
}

int
ps_alignment_add_word(ps_alignment_t *al,
                      int32 wid, int duration)
{
    ps_alignment_entry_t *ent;

    if ((ent = ps_alignment_vector_grow_one(&al->word)) == NULL)
        return 0;
    ent->id.wid = wid;
    if (al->word.n_ent > 1)
        ent->start = ent[-1].start + ent[-1].duration;
    else
        ent->start = 0;
    ent->duration = duration;
    ent->parent = PS_ALIGNMENT_NONE;
    ent->child = PS_ALIGNMENT_NONE;

    return al->word.n_ent;
}

int
ps_alignment_populate(ps_alignment_t *al)
{
    dict2pid_t *d2p;
    dict_t *dict;
    bin_mdef_t *mdef;
    int i, lc;

    /* Clear phone and state sequences. */
    ps_alignment_vector_empty(&al->sseq);
    ps_alignment_vector_empty(&al->state);

    /* For each word, expand to phones/senone sequences. */
    d2p = al->d2p;
    dict = d2p->dict;
    mdef = d2p->mdef;
    lc = bin_mdef_silphone(mdef);
    for (i = 0; i < al->word.n_ent; ++i) {
        ps_alignment_entry_t *went = al->word.seq + i;
        ps_alignment_entry_t *sent;
        int wid = went->id.wid;
        int len = dict_pronlen(dict, wid);
        int j, rc;

        if (i < al->word.n_ent - 1)
            rc = dict_first_phone(dict, al->word.seq[i+1].id.wid);
        else
            rc = bin_mdef_silphone(mdef);

        /* First phone. */
        if ((sent = ps_alignment_vector_grow_one(&al->sseq)) == NULL) {
            E_ERROR("Failed to add phone entry!\n");
            return -1;
        }
        sent->id.pid.cipid = dict_first_phone(dict, wid);
        sent->id.pid.tmatid = bin_mdef_pid2tmatid(mdef, sent->id.pid.cipid);
        sent->start = went->start;
        sent->duration = went->duration;
        sent->parent = i;
        went->child = (uint16)(sent - al->sseq.seq);
        if (len == 1)
            sent->id.pid.ssid
                = dict2pid_lrdiph_rc(d2p, sent->id.pid.cipid, lc, rc);
        else
            sent->id.pid.ssid
                = dict2pid_ldiph_lc(d2p, sent->id.pid.cipid,
                                    dict_second_phone(dict, wid), lc);
        assert(sent->id.pid.ssid != BAD_SSID);

        /* Internal phones. */
        for (j = 1; j < len - 1; ++j) {
            if ((sent = ps_alignment_vector_grow_one(&al->sseq)) == NULL) {
                E_ERROR("Failed to add phone entry!\n");
                return -1;
            }
            sent->id.pid.cipid = dict_pron(dict, wid, j);
            sent->id.pid.tmatid = bin_mdef_pid2tmatid(mdef, sent->id.pid.cipid);
            sent->id.pid.ssid = dict2pid_internal(d2p, wid, j);
            assert(sent->id.pid.ssid != BAD_SSID);
            sent->start = went->start;
            sent->duration = went->duration;
            sent->parent = i;
        }

        /* Last phone. */
        if (j < len) {
            xwdssid_t *rssid;
            assert(j == len - 1);
            if ((sent = ps_alignment_vector_grow_one(&al->sseq)) == NULL) {
                E_ERROR("Failed to add phone entry!\n");
                return -1;
            }
            sent->id.pid.cipid = dict_last_phone(dict, wid);
            sent->id.pid.tmatid = bin_mdef_pid2tmatid(mdef, sent->id.pid.cipid);
            rssid = dict2pid_rssid(d2p, sent->id.pid.cipid,
                                   dict_second_last_phone(dict, wid));
            sent->id.pid.ssid = rssid->ssid[rssid->cimap[rc]];
            assert(sent->id.pid.ssid != BAD_SSID);
            sent->start = went->start;
            sent->duration = went->duration;
            sent->parent = i;
        }
        /* Update lc.  Could just use sent->id.pid.cipid here but that
         * seems needlessly obscure. */
        lc = dict_last_phone(dict, wid);
    }

    /* For each senone sequence, expand to senones.  (we could do this
     * nested above but this makes it more clear and easier to
     * refactor) */
    for (i = 0; i < al->sseq.n_ent; ++i) {
        ps_alignment_entry_t *pent = al->sseq.seq + i;
        ps_alignment_entry_t *sent;
        int j;

        for (j = 0; j < bin_mdef_n_emit_state(mdef); ++j) {
            if ((sent = ps_alignment_vector_grow_one(&al->state)) == NULL) {
                E_ERROR("Failed to add state entry!\n");
                return -1;
            }
            sent->id.senid = bin_mdef_sseq2sen(mdef, pent->id.pid.ssid, j);
            assert(sent->id.senid != BAD_SENID);
            sent->start = pent->start;
            sent->duration = pent->duration;
            sent->parent = i;
            if (j == 0)
                pent->child = (uint16)(sent - al->state.seq);
        }
    }

    return 0;
}

/* FIXME: Somewhat the same as the above function, needs refactoring */
int
ps_alignment_populate_ci(ps_alignment_t *al)
{
    dict2pid_t *d2p;
    dict_t *dict;
    bin_mdef_t *mdef;
    int i;

    /* Clear phone and state sequences. */
    ps_alignment_vector_empty(&al->sseq);
    ps_alignment_vector_empty(&al->state);

    /* For each word, expand to phones/senone sequences. */
    d2p = al->d2p;
    dict = d2p->dict;
    mdef = d2p->mdef;
    for (i = 0; i < al->word.n_ent; ++i) {
        ps_alignment_entry_t *went = al->word.seq + i;
        ps_alignment_entry_t *sent;
        int wid = went->id.wid;
        int len = dict_pronlen(dict, wid);
        int j;

        for (j = 0; j < len; ++j) {
            if ((sent = ps_alignment_vector_grow_one(&al->sseq)) == NULL) {
                E_ERROR("Failed to add phone entry!\n");
                return -1;
            }
            sent->id.pid.cipid = dict_pron(dict, wid, j);
            sent->id.pid.tmatid = bin_mdef_pid2tmatid(mdef, sent->id.pid.cipid);
            sent->id.pid.ssid = bin_mdef_pid2ssid(mdef, sent->id.pid.cipid);
            assert(sent->id.pid.ssid != BAD_SSID);
            sent->start = went->start;
            sent->duration = went->duration;
            sent->parent = i;
        }
    }

    /* For each senone sequence, expand to senones.  (we could do this
     * nested above but this makes it more clear and easier to
     * refactor) */
    for (i = 0; i < al->sseq.n_ent; ++i) {
        ps_alignment_entry_t *pent = al->sseq.seq + i;
        ps_alignment_entry_t *sent;
        int j;

        for (j = 0; j < bin_mdef_n_emit_state(mdef); ++j) {
            if ((sent = ps_alignment_vector_grow_one(&al->state)) == NULL) {
                E_ERROR("Failed to add state entry!\n");
                return -1;
            }
            sent->id.senid = bin_mdef_sseq2sen(mdef, pent->id.pid.ssid, j);
            assert(sent->id.senid != BAD_SENID);
            sent->start = pent->start;
            sent->duration = pent->duration;
            sent->parent = i;
            if (j == 0)
                pent->child = (uint16)(sent - al->state.seq);
        }
    }

    return 0;
}

int
ps_alignment_propagate(ps_alignment_t *al)
{
    ps_alignment_entry_t *last_ent = NULL;
    int i;

    /* Propagate duration up from states to phones. */
    for (i = 0; i < al->state.n_ent; ++i) {
        ps_alignment_entry_t *sent = al->state.seq + i;
        ps_alignment_entry_t *pent = al->sseq.seq + sent->parent;
        if (pent != last_ent) {
            pent->start = sent->start;
            pent->duration = 0;
        }
        pent->duration += sent->duration;
        last_ent = pent;
    }

    /* Propagate duration up from phones to words. */
    last_ent = NULL;
    for (i = 0; i < al->sseq.n_ent; ++i) {
        ps_alignment_entry_t *pent = al->sseq.seq + i;
        ps_alignment_entry_t *went = al->word.seq + pent->parent;
        if (went != last_ent) {
            went->start = pent->start;
            went->duration = 0;
        }
        went->duration += pent->duration;
        last_ent = went;
    }

    return 0;
}

int
ps_alignment_n_words(ps_alignment_t *al)
{
    return (int)al->word.n_ent;
}

int
ps_alignment_n_phones(ps_alignment_t *al)
{
    return (int)al->sseq.n_ent;
}

int
ps_alignment_n_states(ps_alignment_t *al)
{
    return (int)al->state.n_ent;
}

ps_alignment_iter_t *
ps_alignment_words(ps_alignment_t *al)
{
    ps_alignment_iter_t *itor;

    if (al->word.n_ent == 0)
        return NULL;
    itor = ckd_calloc(1, sizeof(*itor));
    itor->al = al;
    itor->vec = &al->word;
    itor->pos = 0;
    return itor;
}

ps_alignment_iter_t *
ps_alignment_phones(ps_alignment_t *al)
{
    ps_alignment_iter_t *itor;

    if (al->sseq.n_ent == 0)
        return NULL;
    itor = ckd_calloc(1, sizeof(*itor));
    itor->al = al;
    itor->vec = &al->sseq;
    itor->pos = 0;
    return itor;
}

ps_alignment_iter_t *
ps_alignment_states(ps_alignment_t *al)
{
    ps_alignment_iter_t *itor;

    if (al->state.n_ent == 0)
        return NULL;
    itor = ckd_calloc(1, sizeof(*itor));
    itor->al = al;
    itor->vec = &al->state;
    itor->pos = 0;
    return itor;
}

ps_alignment_entry_t *
ps_alignment_iter_get(ps_alignment_iter_t *itor)
{
    return itor->vec->seq + itor->pos;
}

int
ps_alignment_iter_free(ps_alignment_iter_t *itor)
{
    ckd_free(itor);
    return 0;
}

ps_alignment_iter_t *
ps_alignment_iter_goto(ps_alignment_iter_t *itor, int pos)
{
    if (itor == NULL)
        return NULL;
    if (pos >= itor->vec->n_ent) {
        ps_alignment_iter_free(itor);
        return NULL;
    }
    itor->pos = pos;
    return itor;
}

ps_alignment_iter_t *
ps_alignment_iter_next(ps_alignment_iter_t *itor)
{
    if (itor == NULL)
        return NULL;
    if (++itor->pos >= itor->vec->n_ent) {
        ps_alignment_iter_free(itor);
        return NULL;
    }
    return itor;
}

ps_alignment_iter_t *
ps_alignment_iter_prev(ps_alignment_iter_t *itor)
{
    if (itor == NULL)
        return NULL;
    if (--itor->pos < 0) {
        ps_alignment_iter_free(itor);
        return NULL;
    }
    return itor;
}

ps_alignment_iter_t *
ps_alignment_iter_up(ps_alignment_iter_t *itor)
{
    ps_alignment_iter_t *itor2;
    if (itor == NULL)
        return NULL;
    if (itor->vec == &itor->al->word)
        return NULL;
    if (itor->vec->seq[itor->pos].parent == PS_ALIGNMENT_NONE)
        return NULL;
    itor2 = ckd_calloc(1, sizeof(*itor2));
    itor2->al = itor->al;
    itor2->pos = itor->vec->seq[itor->pos].parent;
    if (itor->vec == &itor->al->sseq)
        itor2->vec = &itor->al->word;
    else
        itor2->vec = &itor->al->sseq;
    return itor2;
}

ps_alignment_iter_t *
ps_alignment_iter_down(ps_alignment_iter_t *itor)
{
    ps_alignment_iter_t *itor2;
    if (itor == NULL)
        return NULL;
    if (itor->vec == &itor->al->state)
        return NULL;
    if (itor->vec->seq[itor->pos].child == PS_ALIGNMENT_NONE)
        return NULL;
    itor2 = ckd_calloc(1, sizeof(*itor2));
    itor2->al = itor->al;
    itor2->pos = itor->vec->seq[itor->pos].child;
    if (itor->vec == &itor->al->word)
        itor2->vec = &itor->al->sseq;
    else
        itor2->vec = &itor->al->state;
    return itor2;
}
