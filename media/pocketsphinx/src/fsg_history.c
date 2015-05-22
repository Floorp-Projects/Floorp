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

/*
 * fsg_history.c -- FSG Viterbi decode history
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * 25-Feb-2004	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Started..
 */

/* System headers. */
#include <assert.h>

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>

/* Local headers. */
#include "fsg_search_internal.h"
#include "fsg_history.h"


#define __FSG_DBG__	0


fsg_history_t *
fsg_history_init(fsg_model_t * fsg, dict_t *dict)
{
    fsg_history_t *h;

    h = (fsg_history_t *) ckd_calloc(1, sizeof(fsg_history_t));
    h->fsg = fsg;
    h->entries = blkarray_list_init();

    if (fsg && dict) {
        h->n_ciphone = bin_mdef_n_ciphone(dict->mdef);
        h->frame_entries =
            (glist_t **) ckd_calloc_2d(fsg_model_n_state(fsg),
                                       bin_mdef_n_ciphone(dict->mdef),
                                       sizeof(**h->frame_entries));
    }
    else {
        h->frame_entries = NULL;
    }

    return h;
}

void
fsg_history_free(fsg_history_t *h)
{
    int32 s, lc, ns, np;
    gnode_t *gn;

    if (h->fsg) {
        ns = fsg_model_n_state(h->fsg);
        np = h->n_ciphone;

        for (s = 0; s < ns; s++) {
            for (lc = 0; lc < np; lc++) {
                for (gn = h->frame_entries[s][lc]; gn; gn = gnode_next(gn)) {
                    ckd_free(gnode_ptr(gn));
                }
                glist_free(h->frame_entries[s][lc]);
            }
        }
    }
    ckd_free_2d(h->frame_entries);
    blkarray_list_free(h->entries);
    ckd_free(h);
}


void
fsg_history_set_fsg(fsg_history_t *h, fsg_model_t *fsg, dict_t *dict)
{
    if (blkarray_list_n_valid(h->entries) != 0) {
        E_WARN("Switching FSG while history not empty; history cleared\n");
        blkarray_list_reset(h->entries);
    }

    if (h->frame_entries)
        ckd_free_2d((void **) h->frame_entries);
    h->frame_entries = NULL;
    h->fsg = fsg;

    if (fsg && dict) {
        h->n_ciphone = bin_mdef_n_ciphone(dict->mdef);
        h->frame_entries =
            (glist_t **) ckd_calloc_2d(fsg_model_n_state(fsg),
                                       bin_mdef_n_ciphone(dict->mdef),
                                       sizeof(glist_t));
    }
}


void
fsg_history_entry_add(fsg_history_t * h,
                      fsg_link_t * link,
                      int32 frame, int32 score, int32 pred,
                      int32 lc, fsg_pnode_ctxt_t rc)
{
    fsg_hist_entry_t *entry, *new_entry;
    int32 s;
    gnode_t *gn, *prev_gn;

    /* Skip the optimization for the initial dummy entries; always enter them */
    if (frame < 0) {
        new_entry =
            (fsg_hist_entry_t *) ckd_calloc(1, sizeof(fsg_hist_entry_t));
        new_entry->fsglink = link;
        new_entry->frame = frame;
        new_entry->score = score;
        new_entry->pred = pred;
        new_entry->lc = lc;
        new_entry->rc = rc;

        blkarray_list_append(h->entries, (void *) new_entry);
        return;
    }

    s = fsg_link_to_state(link);

    /* Locate where this entry should be inserted in frame_entries[s][lc] */
    prev_gn = NULL;
    for (gn = h->frame_entries[s][lc]; gn; gn = gnode_next(gn)) {
        entry = (fsg_hist_entry_t *) gnode_ptr(gn);

        if (score BETTER_THAN entry->score)
            break;              /* Found where to insert new entry */

        /* Existing entry score not worse than new score */
        if (FSG_PNODE_CTXT_SUB(&rc, &(entry->rc)) == 0)
            return;             /* rc set reduced to 0; new entry can be ignored */

        prev_gn = gn;
    }

    /* Create new entry after prev_gn (if prev_gn is NULL, at head) */
    new_entry =
        (fsg_hist_entry_t *) ckd_calloc(1, sizeof(fsg_hist_entry_t));
    new_entry->fsglink = link;
    new_entry->frame = frame;
    new_entry->score = score;
    new_entry->pred = pred;
    new_entry->lc = lc;
    new_entry->rc = rc;         /* Note: rc set must be non-empty at this point */

    if (!prev_gn) {
        h->frame_entries[s][lc] = glist_add_ptr(h->frame_entries[s][lc],
                                                (void *) new_entry);
        prev_gn = h->frame_entries[s][lc];
    }
    else
        prev_gn = glist_insert_ptr(prev_gn, (void *) new_entry);

    /*
     * Update the rc set of all the remaining entries in the list.  At this
     * point, gn is the entry, if any, immediately following new entry.
     */
    while (gn) {
        entry = (fsg_hist_entry_t *) gnode_ptr(gn);

        if (FSG_PNODE_CTXT_SUB(&(entry->rc), &rc) == 0) {
            /* rc set of entry reduced to 0; can prune this entry */
            ckd_free((void *) entry);
            gn = gnode_free(gn, prev_gn);
        }
        else {
            prev_gn = gn;
            gn = gnode_next(gn);
        }
    }
}


/*
 * Transfer the surviving history entries for this frame into the permanent
 * history table.
 */
void
fsg_history_end_frame(fsg_history_t * h)
{
    int32 s, lc, ns, np;
    gnode_t *gn;
    fsg_hist_entry_t *entry;

    ns = fsg_model_n_state(h->fsg);
    np = h->n_ciphone;

    for (s = 0; s < ns; s++) {
        for (lc = 0; lc < np; lc++) {
            for (gn = h->frame_entries[s][lc]; gn; gn = gnode_next(gn)) {
                entry = (fsg_hist_entry_t *) gnode_ptr(gn);
                blkarray_list_append(h->entries, (void *) entry);
            }

            glist_free(h->frame_entries[s][lc]);
            h->frame_entries[s][lc] = NULL;
        }
    }
}


fsg_hist_entry_t *
fsg_history_entry_get(fsg_history_t * h, int32 id)
{
    return ((fsg_hist_entry_t *) blkarray_list_get(h->entries, id));
}


void
fsg_history_reset(fsg_history_t * h)
{
    blkarray_list_reset(h->entries);
}


int32
fsg_history_n_entries(fsg_history_t * h)
{
    return (blkarray_list_n_valid(h->entries));
}

void
fsg_history_utt_start(fsg_history_t * h)
{
    int32 s, lc, ns, np;

    assert(blkarray_list_n_valid(h->entries) == 0);
    assert(h->frame_entries);

    ns = fsg_model_n_state(h->fsg);
    np = h->n_ciphone;

    for (s = 0; s < ns; s++) {
        for (lc = 0; lc < np; lc++) {
            assert(h->frame_entries[s][lc] == NULL);
        }
    }
}

void
fsg_history_utt_end(fsg_history_t * h)
{
}

void
fsg_history_print(fsg_history_t *h, dict_t *dict) 
{
    int bpidx, bp;
    
    for (bpidx = 0; bpidx < blkarray_list_n_valid(h->entries); bpidx++) {
        bp = bpidx;
        printf("History entry: ");
        while (bp > 0) {
            fsg_hist_entry_t *hist_entry = fsg_history_entry_get(h, bp);
	    fsg_link_t *fl = fsg_hist_entry_fsglink(hist_entry);
    	    char const *baseword;
    	    int32 wid;
    	    bp = fsg_hist_entry_pred(hist_entry);
    	    wid = fsg_link_wid(fl);

    	    if (fl == NULL)
        	    continue;

    	    baseword = fsg_model_word_str(h->fsg, wid);

    	    printf("%s(%d->%d:%d) ", baseword, 
    				     fsg_link_from_state(hist_entry->fsglink), 
    				     fsg_link_to_state(hist_entry->fsglink), 
    				     hist_entry->frame);
	}
	printf("\n");
    }
}
