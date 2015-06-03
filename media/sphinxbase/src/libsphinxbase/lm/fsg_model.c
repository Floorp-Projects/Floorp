/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
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

/* System headers. */
#ifdef _WIN32_WCE
/*MC in a debug build it's implicitly included by assert.h
     but you need this in a release build */
#include <windows.h>
#else
#include <time.h>
#endif                          /* _WIN32_WCE */
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* SphinxBase headers. */
#include "sphinxbase/err.h"
#include "sphinxbase/pio.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/prim_type.h"
#include "sphinxbase/strfuncs.h"
#include "sphinxbase/hash_table.h"
#include "sphinxbase/fsg_model.h"

/**
 * Adjacency list (opaque) for a state in an FSG.
 *
 * Actually we use hash tables so that random access is a bit faster.
 * Plus it allows us to make the lookup code a bit less ugly.
 */

struct trans_list_s {
    hash_table_t *null_trans;   /* Null transitions keyed by state. */
    hash_table_t *trans;        /* Lists of non-null transitions keyed by state. */
};

/**
 * Implementation of arc iterator.
 */
struct fsg_arciter_s {
    hash_iter_t *itor, *null_itor;
    gnode_t *gn;
};

#define FSG_MODEL_BEGIN_DECL		"FSG_BEGIN"
#define FSG_MODEL_END_DECL		"FSG_END"
#define FSG_MODEL_N_DECL			"N"
#define FSG_MODEL_NUM_STATES_DECL	"NUM_STATES"
#define FSG_MODEL_S_DECL			"S"
#define FSG_MODEL_START_STATE_DECL	"START_STATE"
#define FSG_MODEL_F_DECL			"F"
#define FSG_MODEL_FINAL_STATE_DECL	"FINAL_STATE"
#define FSG_MODEL_T_DECL			"T"
#define FSG_MODEL_TRANSITION_DECL	"TRANSITION"
#define FSG_MODEL_COMMENT_CHAR		'#'


static int32
nextline_str2words(FILE * fp, int32 * lineno,
                   char **lineptr, char ***wordptr)
{
    for (;;) {
        size_t len;
        int32 n;

        ckd_free(*lineptr);
        if ((*lineptr = fread_line(fp, &len)) == NULL)
            return -1;

        (*lineno)++;

        if ((*lineptr)[0] == FSG_MODEL_COMMENT_CHAR)
            continue;           /* Skip comment lines */

        n = str2words(*lineptr, NULL, 0);
        if (n == 0)
            continue;           /* Skip blank lines */

        /* Abuse of realloc(), but this doesn't have to be fast. */
        if (*wordptr == NULL)
            *wordptr = ckd_calloc(n, sizeof(**wordptr));
        else
            *wordptr = ckd_realloc(*wordptr, n * sizeof(**wordptr));
        return str2words(*lineptr, *wordptr, n);
    }
}

void
fsg_model_trans_add(fsg_model_t * fsg,
                    int32 from, int32 to, int32 logp, int32 wid)
{
    fsg_link_t *link;
    glist_t gl;
    gnode_t *gn;

    if (fsg->trans[from].trans == NULL)
        fsg->trans[from].trans = hash_table_new(5, HASH_CASE_YES);

    /* Check for duplicate link (i.e., link already exists with label=wid) */
    for (gn = gl = fsg_model_trans(fsg, from, to); gn; gn = gnode_next(gn)) {
        link = (fsg_link_t *) gnode_ptr(gn);
        if (link->wid == wid) {
            if (link->logs2prob < logp)
                link->logs2prob = logp;
            return;
        }
    }

    /* Create transition object */
    link = listelem_malloc(fsg->link_alloc);
    link->from_state = from;
    link->to_state = to;
    link->logs2prob = logp;
    link->wid = wid;

    /* Add it to the list of transitions and update the hash table */
    gl = glist_add_ptr(gl, (void *) link);
    hash_table_replace_bkey(fsg->trans[from].trans,
                            (char const *) &link->to_state,
                            sizeof(link->to_state), gl);
}

int32
fsg_model_tag_trans_add(fsg_model_t * fsg, int32 from, int32 to,
                        int32 logp, int32 wid)
{
    fsg_link_t *link, *link2;

    /* Check for transition probability */
    if (logp > 0) {
        E_FATAL("Null transition prob must be <= 1.0 (state %d -> %d)\n",
                from, to);
    }

    /* Self-loop null transitions (with prob <= 1.0) are redundant */
    if (from == to)
        return -1;

    if (fsg->trans[from].null_trans == NULL)
        fsg->trans[from].null_trans = hash_table_new(5, HASH_CASE_YES);

    /* Check for a duplicate link; if found, keep the higher prob */
    link = fsg_model_null_trans(fsg, from, to);
    if (link) {
        if (link->logs2prob < logp) {
            link->logs2prob = logp;
            return 0;
        }
        else
            return -1;
    }

    /* Create null transition object */
    link = listelem_malloc(fsg->link_alloc);
    link->from_state = from;
    link->to_state = to;
    link->logs2prob = logp;
    link->wid = -1;

    link2 = (fsg_link_t *)
        hash_table_enter_bkey(fsg->trans[from].null_trans,
                              (char const *) &link->to_state,
                              sizeof(link->to_state), link);
    assert(link == link2);

    return 1;
}

int32
fsg_model_null_trans_add(fsg_model_t * fsg, int32 from, int32 to,
                         int32 logp)
{
    return fsg_model_tag_trans_add(fsg, from, to, logp, -1);
}

glist_t
fsg_model_null_trans_closure(fsg_model_t * fsg, glist_t nulls)
{
    gnode_t *gn1;
    int updated;
    fsg_link_t *tl1, *tl2;
    int32 k, n;

    E_INFO("Computing transitive closure for null transitions\n");

    /* If our caller didn't give us a list of null-transitions,
       make such a list. Just loop through all the FSG states, 
       and all the null-transitions in that state (which are kept in
       their own hash table). */
    if (nulls == NULL) {
        int i;
        for (i = 0; i < fsg->n_state; ++i) {
            hash_iter_t *itor;
            hash_table_t *null_trans = fsg->trans[i].null_trans;
            if (null_trans == NULL)
                continue;
            for (itor = hash_table_iter(null_trans);
                 itor != NULL;
                 itor = hash_table_iter_next(itor)) {
                nulls = glist_add_ptr(nulls, hash_entry_val(itor->ent));
            }
        }
    }

    /*
     * Probably not the most efficient closure implementation, in general, but
     * probably reasonably efficient for a sparse null transition matrix.
     */
    n = 0;
    do {
        updated = FALSE;

        for (gn1 = nulls; gn1; gn1 = gnode_next(gn1)) {
            hash_iter_t *itor;

            tl1 = (fsg_link_t *) gnode_ptr(gn1);
            assert(tl1->wid < 0);

            if (fsg->trans[tl1->to_state].null_trans == NULL)
                continue;

            for (itor = hash_table_iter(fsg->trans[tl1->to_state].null_trans);
                 itor; itor = hash_table_iter_next(itor)) {

                tl2 = (fsg_link_t *) hash_entry_val(itor->ent);

                k = fsg_model_null_trans_add(fsg,
                                             tl1->from_state,
                                             tl2->to_state,
                                             tl1->logs2prob +
                                             tl2->logs2prob);
                if (k >= 0) {
                    updated = TRUE;
                    if (k > 0) {
                        nulls = glist_add_ptr(nulls, (void *)
                                              fsg_model_null_trans
                                              (fsg, tl1->from_state,
                                               tl2->to_state));
                        n++;
                    }
                }
            }
        }
    } while (updated);
    
    E_INFO("%d null transitions added\n", n);

    return nulls;
}

glist_t
fsg_model_trans(fsg_model_t * fsg, int32 i, int32 j)
{
    void *val;

    if (fsg->trans[i].trans == NULL)
        return NULL;
    if (hash_table_lookup_bkey(fsg->trans[i].trans, (char const *) &j,
                               sizeof(j), &val) < 0)
        return NULL;
    return (glist_t) val;
}

fsg_link_t *
fsg_model_null_trans(fsg_model_t * fsg, int32 i, int32 j)
{
    void *val;

    if (fsg->trans[i].null_trans == NULL)
        return NULL;
    if (hash_table_lookup_bkey(fsg->trans[i].null_trans, (char const *) &j,
                               sizeof(j), &val) < 0)
        return NULL;
    return (fsg_link_t *) val;
}

fsg_arciter_t *
fsg_model_arcs(fsg_model_t * fsg, int32 i)
{
    fsg_arciter_t *itor;

    if (fsg->trans[i].trans == NULL && fsg->trans[i].null_trans == NULL)
        return NULL;
    itor = ckd_calloc(1, sizeof(*itor));
    if (fsg->trans[i].null_trans)
        itor->null_itor = hash_table_iter(fsg->trans[i].null_trans);
    if (fsg->trans[i].trans)
        itor->itor = hash_table_iter(fsg->trans[i].trans);
    if (itor->itor != NULL)
        itor->gn = hash_entry_val(itor->itor->ent);
    return itor;
}

fsg_link_t *
fsg_arciter_get(fsg_arciter_t * itor)
{
    /* Iterate over non-null arcs first. */
    if (itor->gn)
        return (fsg_link_t *) gnode_ptr(itor->gn);
    else if (itor->null_itor)
        return (fsg_link_t *) hash_entry_val(itor->null_itor->ent);
    else
        return NULL;
}

fsg_arciter_t *
fsg_arciter_next(fsg_arciter_t * itor)
{
    /* Iterate over non-null arcs first. */
    if (itor->gn) {
        itor->gn = gnode_next(itor->gn);
        /* Move to the next destination arc. */
        if (itor->gn == NULL) {
            itor->itor = hash_table_iter_next(itor->itor);
            if (itor->itor != NULL)
                itor->gn = hash_entry_val(itor->itor->ent);
            else if (itor->null_itor == NULL)
                goto stop_iteration;
        }
    }
    else {
        if (itor->null_itor == NULL)
            goto stop_iteration;
        itor->null_itor = hash_table_iter_next(itor->null_itor);
        if (itor->null_itor == NULL)
            goto stop_iteration;
    }
    return itor;
  stop_iteration:
    fsg_arciter_free(itor);
    return NULL;

}

void
fsg_arciter_free(fsg_arciter_t * itor)
{
    if (itor == NULL)
        return;
    hash_table_iter_free(itor->null_itor);
    hash_table_iter_free(itor->itor);
    ckd_free(itor);
}

int
fsg_model_word_id(fsg_model_t * fsg, char const *word)
{
    int wid;

    /* Search for an existing word matching this. */
    for (wid = 0; wid < fsg->n_word; ++wid) {
        if (0 == strcmp(fsg->vocab[wid], word))
            break;
    }
    /* If not found, add this to the vocab. */
    if (wid == fsg->n_word)
        return -1;
    return wid;
}

int
fsg_model_word_add(fsg_model_t * fsg, char const *word)
{
    int wid, old_size;

    /* Search for an existing word matching this. */
    wid = fsg_model_word_id(fsg, word);
    /* If not found, add this to the vocab. */
    if (wid == -1) {
        wid = fsg->n_word;
        if (fsg->n_word == fsg->n_word_alloc) {
    	    old_size = fsg->n_word_alloc;
            fsg->n_word_alloc += 10;
            fsg->vocab = ckd_realloc(fsg->vocab,
                                     fsg->n_word_alloc *
                                     sizeof(*fsg->vocab));
            if (fsg->silwords)
                fsg->silwords =
                    bitvec_realloc(fsg->silwords, old_size, fsg->n_word_alloc);
            if (fsg->altwords)
                fsg->altwords =
                    bitvec_realloc(fsg->altwords, old_size, fsg->n_word_alloc);
        }
        ++fsg->n_word;
        fsg->vocab[wid] = ckd_salloc(word);
    }
    return wid;
}

int
fsg_model_add_silence(fsg_model_t * fsg, char const *silword,
                      int state, float32 silprob)
{
    int32 logsilp;
    int n_trans, silwid, src;

    E_INFO("Adding silence transitions for %s to FSG\n", silword);

    silwid = fsg_model_word_add(fsg, silword);
    logsilp = (int32) (logmath_log(fsg->lmath, silprob) * fsg->lw);
    if (fsg->silwords == NULL)
        fsg->silwords = bitvec_alloc(fsg->n_word_alloc);
    bitvec_set(fsg->silwords, silwid);

    n_trans = 0;
    if (state == -1) {
        for (src = 0; src < fsg->n_state; src++) {
            fsg_model_trans_add(fsg, src, src, logsilp, silwid);
            ++n_trans;
        }
    }
    else {
        fsg_model_trans_add(fsg, state, state, logsilp, silwid);
        ++n_trans;
    }

    E_INFO("Added %d silence word transitions\n", n_trans);
    return n_trans;
}

int
fsg_model_add_alt(fsg_model_t * fsg, char const *baseword,
                  char const *altword)
{
    int i, basewid, altwid;
    int ntrans;

    /* FIXME: This will get slow, eventually... */
    for (basewid = 0; basewid < fsg->n_word; ++basewid)
        if (0 == strcmp(fsg->vocab[basewid], baseword))
            break;
    if (basewid == fsg->n_word) {
        E_ERROR("Base word %s not present in FSG vocabulary!\n", baseword);
        return -1;
    }
    altwid = fsg_model_word_add(fsg, altword);
    if (fsg->altwords == NULL)
        fsg->altwords = bitvec_alloc(fsg->n_word_alloc);
    bitvec_set(fsg->altwords, altwid);
    if (fsg_model_is_filler(fsg, basewid)) {
         if (fsg->silwords == NULL)
	      fsg->silwords = bitvec_alloc(fsg->n_word_alloc);
         bitvec_set(fsg->silwords, altwid);
    }

    E_DEBUG(2, ("Adding alternate word transitions (%s,%s) to FSG\n",
                baseword, altword));

    /* Look for all transitions involving baseword and duplicate them. */
    /* FIXME: This will also get slow, eventually... */
    ntrans = 0;
    for (i = 0; i < fsg->n_state; ++i) {
        hash_iter_t *itor;
        if (fsg->trans[i].trans == NULL)
            continue;
        for (itor = hash_table_iter(fsg->trans[i].trans); itor;
             itor = hash_table_iter_next(itor)) {
            glist_t trans;
            gnode_t *gn;

            trans = hash_entry_val(itor->ent);
            for (gn = trans; gn; gn = gnode_next(gn)) {
                fsg_link_t *fl = gnode_ptr(gn);
                if (fl->wid == basewid) {
                    fsg_link_t *link;

                    /* Create transition object */
                    link = listelem_malloc(fsg->link_alloc);
                    link->from_state = fl->from_state;
                    link->to_state = fl->to_state;
                    link->logs2prob = fl->logs2prob;    /* FIXME!!!??? */
                    link->wid = altwid;

                    trans = glist_add_ptr(trans, (void *) link);
                    ++ntrans;
                }
            }
            hash_entry_val(itor->ent) = trans;
        }
    }

    E_DEBUG(2, ("Added %d alternate word transitions\n", ntrans));
    return ntrans;
}


fsg_model_t *
fsg_model_init(char const *name, logmath_t * lmath, float32 lw,
               int32 n_state)
{
    fsg_model_t *fsg;

    /* Allocate basic stuff. */
    fsg = ckd_calloc(1, sizeof(*fsg));
    fsg->refcount = 1;
    fsg->link_alloc = listelem_alloc_init(sizeof(fsg_link_t));
    fsg->lmath = lmath;
    fsg->name = name ? ckd_salloc(name) : NULL;
    fsg->n_state = n_state;
    fsg->lw = lw;

    fsg->trans = ckd_calloc(fsg->n_state, sizeof(*fsg->trans));

    return fsg;
}

fsg_model_t *
fsg_model_read(FILE * fp, logmath_t * lmath, float32 lw)
{
    fsg_model_t *fsg;
    hash_table_t *vocab;
    hash_iter_t *itor;
    int32 lastwid;
    char **wordptr;
    char *lineptr;
    char *fsgname;
    int32 lineno;
    int32 n, i, j;
    int n_state, n_trans, n_null_trans;
    glist_t nulls;
    float32 p;

    lineno = 0;
    vocab = hash_table_new(32, FALSE);
    wordptr = NULL;
    lineptr = NULL;
    nulls = NULL;
    fsgname = NULL;
    fsg = NULL;

    /* Scan upto FSG_BEGIN header */
    for (;;) {
        n = nextline_str2words(fp, &lineno, &lineptr, &wordptr);
        if (n < 0) {
            E_ERROR("%s declaration missing\n", FSG_MODEL_BEGIN_DECL);
            goto parse_error;
        }

        if ((strcmp(wordptr[0], FSG_MODEL_BEGIN_DECL) == 0)) {
            if (n > 2) {
                E_ERROR("Line[%d]: malformed FSG_BEGIN declaration\n",
                        lineno);
                goto parse_error;
            }
            break;
        }
    }
    /* Save FSG name, or it will get clobbered below :(.
     * If name is missing, try the default.
     */
    if (n == 2) {
        fsgname = ckd_salloc(wordptr[1]);
    }
    else {
        E_WARN("FSG name is missing\n");
        fsgname = ckd_salloc("unknown");
    }

    /* Read #states */
    n = nextline_str2words(fp, &lineno, &lineptr, &wordptr);
    if ((n != 2)
        || ((strcmp(wordptr[0], FSG_MODEL_N_DECL) != 0)
            && (strcmp(wordptr[0], FSG_MODEL_NUM_STATES_DECL) != 0))
        || (sscanf(wordptr[1], "%d", &n_state) != 1)
        || (n_state <= 0)) {
        E_ERROR
            ("Line[%d]: #states declaration line missing or malformed\n",
             lineno);
        goto parse_error;
    }

    /* Now create the FSG. */
    fsg = fsg_model_init(fsgname, lmath, lw, n_state);
    ckd_free(fsgname);
    fsgname = NULL;

    /* Read start state */
    n = nextline_str2words(fp, &lineno, &lineptr, &wordptr);
    if ((n != 2)
        || ((strcmp(wordptr[0], FSG_MODEL_S_DECL) != 0)
            && (strcmp(wordptr[0], FSG_MODEL_START_STATE_DECL) != 0))
        || (sscanf(wordptr[1], "%d", &(fsg->start_state)) != 1)
        || (fsg->start_state < 0)
        || (fsg->start_state >= fsg->n_state)) {
        E_ERROR
            ("Line[%d]: start state declaration line missing or malformed\n",
             lineno);
        goto parse_error;
    }

    /* Read final state */
    n = nextline_str2words(fp, &lineno, &lineptr, &wordptr);
    if ((n != 2)
        || ((strcmp(wordptr[0], FSG_MODEL_F_DECL) != 0)
            && (strcmp(wordptr[0], FSG_MODEL_FINAL_STATE_DECL) != 0))
        || (sscanf(wordptr[1], "%d", &(fsg->final_state)) != 1)
        || (fsg->final_state < 0)
        || (fsg->final_state >= fsg->n_state)) {
        E_ERROR
            ("Line[%d]: final state declaration line missing or malformed\n",
             lineno);
        goto parse_error;
    }

    /* Read transitions */
    lastwid = 0;
    n_trans = n_null_trans = 0;
    for (;;) {
        int32 wid, tprob;

        n = nextline_str2words(fp, &lineno, &lineptr, &wordptr);
        if (n <= 0) {
            E_ERROR("Line[%d]: transition or FSG_END statement expected\n",
                    lineno);
            goto parse_error;
        }

        if ((strcmp(wordptr[0], FSG_MODEL_END_DECL) == 0)) {
            break;
        }

        if ((strcmp(wordptr[0], FSG_MODEL_T_DECL) == 0)
            || (strcmp(wordptr[0], FSG_MODEL_TRANSITION_DECL) == 0)) {


            if (((n != 4) && (n != 5))
                || (sscanf(wordptr[1], "%d", &i) != 1)
                || (sscanf(wordptr[2], "%d", &j) != 1)
                || (i < 0) || (i >= fsg->n_state)
                || (j < 0) || (j >= fsg->n_state)) {
                E_ERROR
                    ("Line[%d]: transition spec malformed; Expecting: from-state to-state trans-prob [word]\n",
                     lineno);
                goto parse_error;
            }

            p = atof_c(wordptr[3]);
            if ((p <= 0.0) || (p > 1.0)) {
                E_ERROR
                    ("Line[%d]: transition spec malformed; Expecting float as transition probability\n",
                     lineno);
                goto parse_error;
            }
        }
        else {
            E_ERROR("Line[%d]: transition or FSG_END statement expected\n",
                    lineno);
            goto parse_error;
        }

        tprob = (int32) (logmath_log(lmath, p) * fsg->lw);
        /* Add word to "dictionary". */
        if (n > 4) {
            if (hash_table_lookup_int32(vocab, wordptr[4], &wid) < 0) {
                (void) hash_table_enter_int32(vocab,
                                              ckd_salloc(wordptr[4]),
                                              lastwid);
                wid = lastwid;
                ++lastwid;
            }
            fsg_model_trans_add(fsg, i, j, tprob, wid);
            ++n_trans;
        }
        else {
            if (fsg_model_null_trans_add(fsg, i, j, tprob) == 1) {
                ++n_null_trans;
                nulls =
                    glist_add_ptr(nulls, fsg_model_null_trans(fsg, i, j));
            }
        }
    }

    E_INFO("FSG: %d states, %d unique words, %d transitions (%d null)\n",
           fsg->n_state, hash_table_inuse(vocab), n_trans, n_null_trans);


    /* Now create a string table from the "dictionary" */
    fsg->n_word = hash_table_inuse(vocab);
    fsg->n_word_alloc = fsg->n_word + 10;       /* Pad it a bit. */
    fsg->vocab = ckd_calloc(fsg->n_word_alloc, sizeof(*fsg->vocab));
    for (itor = hash_table_iter(vocab); itor;
         itor = hash_table_iter_next(itor)) {
        char const *word = hash_entry_key(itor->ent);
        int32 wid = (int32) (long) hash_entry_val(itor->ent);
        fsg->vocab[wid] = (char *) word;
    }
    hash_table_free(vocab);

    /* Do transitive closure on null transitions */
    nulls = fsg_model_null_trans_closure(fsg, nulls);
    glist_free(nulls);

    ckd_free(lineptr);
    ckd_free(wordptr);

    return fsg;

  parse_error:
    for (itor = hash_table_iter(vocab); itor;
         itor = hash_table_iter_next(itor))
        ckd_free((char *) hash_entry_key(itor->ent));
    glist_free(nulls);
    hash_table_free(vocab);
    ckd_free(fsgname);
    ckd_free(lineptr);
    ckd_free(wordptr);
    fsg_model_free(fsg);
    return NULL;
}


fsg_model_t *
fsg_model_readfile(const char *file, logmath_t * lmath, float32 lw)
{
    FILE *fp;
    fsg_model_t *fsg;

    if ((fp = fopen(file, "r")) == NULL) {
        E_ERROR_SYSTEM("Failed to open FSG file '%s' for reading", file);
        return NULL;
    }
    fsg = fsg_model_read(fp, lmath, lw);
    fclose(fp);
    return fsg;
}

fsg_model_t *
fsg_model_retain(fsg_model_t * fsg)
{
    ++fsg->refcount;
    return fsg;
}

static void
trans_list_free(fsg_model_t * fsg, int32 i)
{
    hash_iter_t *itor;

    /* FIXME (maybe): FSG links will all get freed when we call
     * listelem_alloc_free() so don't bother freeing them explicitly
     * here. */
    if (fsg->trans[i].trans) {
        for (itor = hash_table_iter(fsg->trans[i].trans);
             itor; itor = hash_table_iter_next(itor)) {
            glist_t gl = (glist_t) hash_entry_val(itor->ent);
            glist_free(gl);
        }
    }
    hash_table_free(fsg->trans[i].trans);
    hash_table_free(fsg->trans[i].null_trans);
}

int
fsg_model_free(fsg_model_t * fsg)
{
    int i;

    if (fsg == NULL)
        return 0;

    if (--fsg->refcount > 0)
        return fsg->refcount;

    for (i = 0; i < fsg->n_word; ++i)
        ckd_free(fsg->vocab[i]);
    for (i = 0; i < fsg->n_state; ++i)
        trans_list_free(fsg, i);
    ckd_free(fsg->trans);
    ckd_free(fsg->vocab);
    listelem_alloc_free(fsg->link_alloc);
    bitvec_free(fsg->silwords);
    bitvec_free(fsg->altwords);
    ckd_free(fsg->name);
    ckd_free(fsg);
    return 0;
}


void
fsg_model_write(fsg_model_t * fsg, FILE * fp)
{
    int32 i;

    fprintf(fp, "%s %s\n", FSG_MODEL_BEGIN_DECL,
            fsg->name ? fsg->name : "");
    fprintf(fp, "%s %d\n", FSG_MODEL_NUM_STATES_DECL, fsg->n_state);
    fprintf(fp, "%s %d\n", FSG_MODEL_START_STATE_DECL, fsg->start_state);
    fprintf(fp, "%s %d\n", FSG_MODEL_FINAL_STATE_DECL, fsg->final_state);

    for (i = 0; i < fsg->n_state; i++) {
        fsg_arciter_t *itor;

        for (itor = fsg_model_arcs(fsg, i); itor;
             itor = fsg_arciter_next(itor)) {
            fsg_link_t *tl = fsg_arciter_get(itor);

            fprintf(fp, "%s %d %d %f %s\n", FSG_MODEL_TRANSITION_DECL,
                    tl->from_state, tl->to_state,
                    logmath_exp(fsg->lmath,
                                (int32) (tl->logs2prob / fsg->lw)),
                    (tl->wid < 0) ? "" : fsg_model_word_str(fsg, tl->wid));
        }
    }

    fprintf(fp, "%s\n", FSG_MODEL_END_DECL);

    fflush(fp);
}

void
fsg_model_writefile(fsg_model_t * fsg, char const *file)
{
    FILE *fp;

    assert(fsg);

    E_INFO("Writing FSG file '%s'\n", file);

    if ((fp = fopen(file, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open FSG file '%s' for reading", file);
        return;
    }

    fsg_model_write(fsg, fp);

    fclose(fp);
}

static void
fsg_model_write_fsm_trans(fsg_model_t * fsg, int i, FILE * fp)
{
    fsg_arciter_t *itor;

    for (itor = fsg_model_arcs(fsg, i); itor;
         itor = fsg_arciter_next(itor)) {
        fsg_link_t *tl = fsg_arciter_get(itor);
        fprintf(fp, "%d %d %s %f\n",
                tl->from_state, tl->to_state,
                (tl->wid < 0) ? "<eps>" : fsg_model_word_str(fsg, tl->wid),
                -logmath_log_to_ln(fsg->lmath, tl->logs2prob / fsg->lw));
    }
}

void
fsg_model_write_fsm(fsg_model_t * fsg, FILE * fp)
{
    int i;

    /* Write transitions from initial state first. */
    fsg_model_write_fsm_trans(fsg, fsg_model_start_state(fsg), fp);

    /* Other states. */
    for (i = 0; i < fsg->n_state; i++) {
        if (i == fsg_model_start_state(fsg))
            continue;
        fsg_model_write_fsm_trans(fsg, i, fp);
    }

    /* Final state. */
    fprintf(fp, "%d 0\n", fsg_model_final_state(fsg));

    fflush(fp);
}

void
fsg_model_writefile_fsm(fsg_model_t * fsg, char const *file)
{
    FILE *fp;

    assert(fsg);

    E_INFO("Writing FSM file '%s'\n", file);

    if ((fp = fopen(file, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open fsm file '%s' for writing", file);
        return;
    }

    fsg_model_write_fsm(fsg, fp);

    fclose(fp);
}

void
fsg_model_write_symtab(fsg_model_t * fsg, FILE * file)
{
    int i;

    fprintf(file, "<eps> 0\n");
    for (i = 0; i < fsg_model_n_word(fsg); ++i) {
        fprintf(file, "%s %d\n", fsg_model_word_str(fsg, i), i + 1);
    }
    fflush(file);
}

void
fsg_model_writefile_symtab(fsg_model_t * fsg, char const *file)
{
    FILE *fp;

    assert(fsg);

    E_INFO("Writing FSM symbol table '%s'\n", file);

    if ((fp = fopen(file, "w")) == NULL) {
        E_ERROR("Failed to open symbol table '%s' for writing", file);
        return;
    }

    fsg_model_write_symtab(fsg, fp);

    fclose(fp);
}
