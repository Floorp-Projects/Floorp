/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
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
/**
 * @file fsg_lextree.c
 * @brief The collection of all the lextrees for the entire FSM.
 * @author M K Ravishankar <rkm@cs.cmu.edu>
 * @author Bhiksha Raj <bhiksha@cs.cmu.edu>
 */

/* System headers. */
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* SphinxBase headers. */
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/err.h>

/* Local headers. */
#include "fsg_lextree.h"

#define __FSG_DBG__		0

/* A linklist structure that is actually used to build local lextrees at grammar nodes */
typedef struct fsg_glist_linklist_t {
    int32    ci, rc;
    glist_t  glist;
    struct   fsg_glist_linklist_t *next;
} fsg_glist_linklist_t;

/**
 * Build the phone lextree for all transitions out of state from_state.
 * Return the root node of this tree.
 * Also, return a linear linked list of all allocated fsg_pnode_t nodes in
 * *alloc_head (for memory management purposes).
 */
static fsg_pnode_t *fsg_psubtree_init(fsg_lextree_t *tree,
                                      fsg_model_t *fsg,
                                      int32 from_state,
                                      fsg_pnode_t **alloc_head);

/**
 * Free the given lextree.  alloc_head: head of linear list of allocated
 * nodes updated by fsg_psubtree_init().
 */
static void fsg_psubtree_free(fsg_pnode_t *alloc_head);

/**
 * Dump the list of nodes in the given lextree to the given file.  alloc_head:
 * head of linear list of allocated nodes updated by fsg_psubtree_init().
 */
static void fsg_psubtree_dump(fsg_lextree_t *tree, fsg_pnode_t *root, FILE *fp);

/**
 * Compute the left and right context CIphone sets for each state.
 */
static void
fsg_lextree_lc_rc(fsg_lextree_t *lextree)
{
    int32 s, i, j;
    int32 n_ci;
    fsg_model_t *fsg;
    int32 silcipid;
    int32 len;

    silcipid = bin_mdef_silphone(lextree->mdef);
    assert(silcipid >= 0);
    n_ci = bin_mdef_n_ciphone(lextree->mdef);

    fsg = lextree->fsg;
    /*
     * lextree->lc[s] = set of left context CIphones for state s.  Similarly, rc[s]
     * for right context CIphones.
     */
    lextree->lc = ckd_calloc_2d(fsg->n_state, n_ci + 1, sizeof(**lextree->lc));
    lextree->rc = ckd_calloc_2d(fsg->n_state, n_ci + 1, sizeof(**lextree->rc));
    E_INFO("Allocated %d bytes (%d KiB) for left and right context phones\n",
           fsg->n_state * (n_ci + 1) * 2,
           fsg->n_state * (n_ci + 1) * 2 / 1024);


    for (s = 0; s < fsg->n_state; s++) {
        fsg_arciter_t *itor;
        for (itor = fsg_model_arcs(fsg, s); itor; itor = fsg_arciter_next(itor)) {
            fsg_link_t *l = fsg_arciter_get(itor);
            int32 dictwid; /**< Dictionary (not FSG) word ID!! */

            if (fsg_link_wid(l) >= 0) {
                dictwid = dict_wordid(lextree->dict,
                                      fsg_model_word_str(lextree->fsg, l->wid));

                /*
                 * Add the first CIphone of l->wid to the rclist of state s, and
                 * the last CIphone to lclist of state d.
                 * (Filler phones are a pain to deal with.  There is no direct
                 * marking of a filler phone; but only filler words are supposed to
                 * use such phones, so we use that fact.  HACK!!  FRAGILE!!)
                 *
                 * UPD: tests carsh here if .fsg model used with wrong hmm and
                 *      dictionary
                 */
                if (fsg_model_is_filler(fsg, fsg_link_wid(l))) {
                    /* Filler phone; use silence phone as context */
                    lextree->rc[fsg_link_from_state(l)][silcipid] = 1;
                    lextree->lc[fsg_link_to_state(l)][silcipid] = 1;
                }
                else {
                    len = dict_pronlen(lextree->dict, dictwid);
                    lextree->rc[fsg_link_from_state(l)][dict_pron(lextree->dict, dictwid, 0)] = 1;
                    lextree->lc[fsg_link_to_state(l)][dict_pron(lextree->dict, dictwid, len - 1)] = 1;
                }
            }
        }
    }

    for (s = 0; s < fsg->n_state; s++) {
        /*
         * Add SIL phone to the lclist and rclist of each state.  Strictly
         * speaking, only needed at start and final states, respectively, but
         * all states considered since the user may change the start and final
         * states.  In any case, most applications would have a silence self
         * loop at each state, hence these would be needed anyway.
         */
        lextree->lc[s][silcipid] = 1;
        lextree->rc[s][silcipid] = 1;
    }

    /*
     * Propagate lc and rc lists past null transitions.  (Since FSG contains
     * null transitions closure, no need to worry about a chain of successive
     * null transitions.  Right??)
     *
     * This can't be joined with the previous loop because we first calculate 
     * contexts and only then we can propagate them.
     */
    for (s = 0; s < fsg->n_state; s++) {
        fsg_arciter_t *itor;
        for (itor = fsg_model_arcs(fsg, s); itor; itor = fsg_arciter_next(itor)) {
            fsg_link_t *l = fsg_arciter_get(itor);
            if (fsg_link_wid(l) < 0) {
                /*
                 * lclist(d) |= lclist(s), because all the words ending up at s, can
                 * now also end at d, becoming the left context for words leaving d.
                 */
                for (i = 0; i < n_ci; i++)
                    lextree->lc[fsg_link_to_state(l)][i] |= lextree->lc[fsg_link_from_state(l)][i];
                /*
                 * Similarly, rclist(s) |= rclist(d), because all the words leaving d
                 * can equivalently leave s, becoming the right context for words
                 * ending up at s.
                 */
                for (i = 0; i < n_ci; i++)
                    lextree->rc[fsg_link_from_state(l)][i] |= lextree->rc[fsg_link_to_state(l)][i];
            }
        }
    }

    /* Convert the bit-vector representation into a list */
    for (s = 0; s < fsg->n_state; s++) {
        j = 0;
        for (i = 0; i < n_ci; i++) {
            if (lextree->lc[s][i]) {
                lextree->lc[s][j] = i;
                j++;
            }
        }
        lextree->lc[s][j] = -1;     /* Terminate the list */

        j = 0;
        for (i = 0; i < n_ci; i++) {
            if (lextree->rc[s][i]) {
                lextree->rc[s][j] = i;
                j++;
            }
        }
        lextree->rc[s][j] = -1;     /* Terminate the list */
    }
}

/*
 * For now, allocate the entire lextree statically.
 */
fsg_lextree_t *
fsg_lextree_init(fsg_model_t * fsg, dict_t *dict, dict2pid_t *d2p,
                 bin_mdef_t *mdef, hmm_context_t *ctx,
                 int32 wip, int32 pip)
{
    int32 s, n_leaves;
    fsg_lextree_t *lextree;
    fsg_pnode_t *pn;

    lextree = ckd_calloc(1, sizeof(fsg_lextree_t));
    lextree->fsg = fsg;
    lextree->root = ckd_calloc(fsg_model_n_state(fsg),
                               sizeof(fsg_pnode_t *));
    lextree->alloc_head = ckd_calloc(fsg_model_n_state(fsg),
                                     sizeof(fsg_pnode_t *));
    lextree->ctx = ctx;
    lextree->dict = dict;
    lextree->d2p = d2p;
    lextree->mdef = mdef;
    lextree->wip = wip;
    lextree->pip = pip;

    /* Compute lc and rc for fsg. */
    fsg_lextree_lc_rc(lextree);

    /* Create lextree for each state, i.e. an HMM network that
     * represents words for all arcs exiting that state.  Note that
     * for a dense grammar such as an N-gram model, this will
     * rapidly exhaust all available memory. */
    lextree->n_pnode = 0;
    n_leaves = 0;
    for (s = 0; s < fsg_model_n_state(fsg); s++) {
        lextree->root[s] =
            fsg_psubtree_init(lextree, fsg, s, &(lextree->alloc_head[s]));

        for (pn = lextree->alloc_head[s]; pn; pn = pn->alloc_next) {
            lextree->n_pnode++;
            if (pn->leaf)
                ++n_leaves;
        }
    }
    E_INFO("%d HMM nodes in lextree (%d leaves)\n",
           lextree->n_pnode, n_leaves);
    E_INFO("Allocated %d bytes (%d KiB) for all lextree nodes\n",
           lextree->n_pnode * sizeof(fsg_pnode_t),
           lextree->n_pnode * sizeof(fsg_pnode_t) / 1024);
    E_INFO("Allocated %d bytes (%d KiB) for lextree leafnodes\n",
           n_leaves * sizeof(fsg_pnode_t),
           n_leaves * sizeof(fsg_pnode_t) / 1024);

#if __FSG_DBG__
    fsg_lextree_dump(lextree, stdout);
#endif

    return lextree;
}


void
fsg_lextree_dump(fsg_lextree_t * lextree, FILE * fp)
{
    int32 s;

    for (s = 0; s < fsg_model_n_state(lextree->fsg); s++) {
        fprintf(fp, "State %5d root %p\n", s, lextree->root[s]);
        fsg_psubtree_dump(lextree, lextree->root[s], fp);
    }
    fflush(fp);
}


void
fsg_lextree_free(fsg_lextree_t * lextree)
{
    int32 s;

    if (lextree == NULL)
        return;

    if (lextree->fsg)
        for (s = 0; s < fsg_model_n_state(lextree->fsg); s++)
            fsg_psubtree_free(lextree->alloc_head[s]);

    ckd_free_2d(lextree->lc);
    ckd_free_2d(lextree->rc);
    ckd_free(lextree->root);
    ckd_free(lextree->alloc_head);
    ckd_free(lextree);
}

/******************************
 * psubtree stuff starts here *
 ******************************/

void fsg_glist_linklist_free(fsg_glist_linklist_t *glist)
{
    if (glist) {
        fsg_glist_linklist_t *nxtglist;
        if (glist->glist)
            glist_free(glist->glist);
        nxtglist = glist->next;
        while (nxtglist) {
            ckd_free(glist);
            glist = nxtglist;
            if (glist->glist)
                glist_free(glist->glist);
            nxtglist = glist->next;
        }
        ckd_free(glist);
    }
    return;
}

void
fsg_pnode_add_all_ctxt(fsg_pnode_ctxt_t * ctxt)
{
    int32 i;

    for (i = 0; i < FSG_PNODE_CTXT_BVSZ; i++)
        ctxt->bv[i] = 0xffffffff;
}

uint32 fsg_pnode_ctxt_sub_generic(fsg_pnode_ctxt_t *src, fsg_pnode_ctxt_t *sub)
{
    int32 i;
    uint32 res = 0;
    
    for (i = 0; i < FSG_PNODE_CTXT_BVSZ; i++)
        res |= (src->bv[i] = ~(sub->bv[i]) & src->bv[i]);
    return res;
}


/*
 * fsg_pnode_ctxt_sub(fsg_pnode_ctxt_t * src, fsg_pnode_ctxt_t * sub)
 * This has been moved into a macro in fsg_psubtree.h 
 * because it is called so frequently!
 */


/*
 * Add the word emitted by the given transition (fsglink) to the given lextree
 * (rooted at root), and return the new lextree root.  (There may actually be
 * several root nodes, maintained in a linked list via fsg_pnode_t.sibling.
 * "root" is the head of this list.)
 * lclist, rclist: sets of left and right context phones for this link.
 * alloc_head: head of a linear list of all allocated pnodes for the parent
 * FSG state, kept elsewhere and updated by this routine.
 */
static fsg_pnode_t *
psubtree_add_trans(fsg_lextree_t *lextree, 
                   fsg_pnode_t * root,
                   fsg_glist_linklist_t **curglist,
                   fsg_link_t * fsglink,
                   int16 *lclist, int16 *rclist,
                   fsg_pnode_t ** alloc_head)
{
    int32 silcipid;             /* Silence CI phone ID */
    int32 pronlen;              /* Pronunciation length */
    int32 wid;                  /* FSG (not dictionary!!) word ID */
    int32 dictwid;              /* Dictionary (not FSG!!) word ID */
    int32 ssid;                 /* Senone Sequence ID */
    int32 tmatid;
    gnode_t *gn;
    fsg_pnode_t *pnode, *pred, *head;
    int32 n_ci, p, lc, rc;
    glist_t lc_pnodelist;       /* Temp pnodes list for different left contexts */
    glist_t rc_pnodelist;       /* Temp pnodes list for different right contexts */
    int32 i, j;
    int n_lc_alloc = 0, n_int_alloc = 0, n_rc_alloc = 0;

    silcipid = bin_mdef_silphone(lextree->mdef);
    n_ci = bin_mdef_n_ciphone(lextree->mdef);

    wid = fsg_link_wid(fsglink);
    assert(wid >= 0);           /* Cannot be a null transition */
    dictwid = dict_wordid(lextree->dict,
                          fsg_model_word_str(lextree->fsg, wid));
    pronlen = dict_pronlen(lextree->dict, dictwid);
    assert(pronlen >= 1);

    assert(lclist[0] >= 0);     /* At least one phonetic context provided */
    assert(rclist[0] >= 0);

    head = *alloc_head;
    pred = NULL;

    if (pronlen == 1) {         /* Single-phone word */
        int ci = dict_first_phone(lextree->dict, dictwid);
        /* Only non-filler words are mpx */
        if (!dict_filler_word(lextree->dict, dictwid)) {
            /*
             * Left diphone ID for single-phone words already assumes SIL is right
             * context; only left contexts need to be handled.
             */
            lc_pnodelist = NULL;

            for (i = 0; lclist[i] >= 0; i++) {
                lc = lclist[i];
                ssid = dict2pid_lrdiph_rc(lextree->d2p, ci, lc, silcipid);
                tmatid = bin_mdef_pid2tmatid(lextree->mdef, dict_first_phone(lextree->dict, dictwid));
                /* Check if this ssid already allocated for some other context */
                for (gn = lc_pnodelist; gn; gn = gnode_next(gn)) {
                    pnode = (fsg_pnode_t *) gnode_ptr(gn);

                    if (hmm_nonmpx_ssid(&pnode->hmm) == ssid) {
                        /* already allocated; share it for this context phone */
                        fsg_pnode_add_ctxt(pnode, lc);
                        break;
                    }
                }

                if (!gn) {      /* ssid not already allocated */
                    pnode =
                        (fsg_pnode_t *) ckd_calloc(1, sizeof(fsg_pnode_t));
                    pnode->ctx = lextree->ctx;
                    pnode->next.fsglink = fsglink;
                    pnode->logs2prob =
                        (fsg_link_logs2prob(fsglink) >> SENSCR_SHIFT)
                        + lextree->wip + lextree->pip;
                    pnode->ci_ext = dict_first_phone(lextree->dict, dictwid);
                    pnode->ppos = 0;
                    pnode->leaf = TRUE;
                    pnode->sibling = root;      /* All root nodes linked together */
                    fsg_pnode_add_ctxt(pnode, lc);      /* Initially zeroed by calloc above */
                    pnode->alloc_next = head;
                    head = pnode;
                    root = pnode;
                    ++n_lc_alloc;

                    hmm_init(lextree->ctx, &pnode->hmm, FALSE, ssid, tmatid);

                    lc_pnodelist =
                        glist_add_ptr(lc_pnodelist, (void *) pnode);
                }
            }

            glist_free(lc_pnodelist);
        }
        else {                  /* Filler word; no context modelled */
            ssid = bin_mdef_pid2ssid(lextree->mdef, ci); /* probably the same... */
            tmatid = bin_mdef_pid2tmatid(lextree->mdef, ci);

            pnode = (fsg_pnode_t *) ckd_calloc(1, sizeof(fsg_pnode_t));
            pnode->ctx = lextree->ctx;
            pnode->next.fsglink = fsglink;
            pnode->logs2prob = (fsg_link_logs2prob(fsglink) >> SENSCR_SHIFT)
                + lextree->wip + lextree->pip;
            pnode->ci_ext = silcipid;   /* Presents SIL as context to neighbors */
            pnode->ppos = 0;
            pnode->leaf = TRUE;
            pnode->sibling = root;
            fsg_pnode_add_all_ctxt(&(pnode->ctxt));
            pnode->alloc_next = head;
            head = pnode;
            root = pnode;
            ++n_int_alloc;

            hmm_init(lextree->ctx, &pnode->hmm, FALSE, ssid, tmatid);
        }
    }
    else {                      /* Multi-phone word */
        fsg_pnode_t **ssid_pnode_map;       /* Temp array of ssid->pnode mapping */
        ssid_pnode_map =
            (fsg_pnode_t **) ckd_calloc(n_ci, sizeof(fsg_pnode_t *));
        lc_pnodelist = NULL;
        rc_pnodelist = NULL;

        for (p = 0; p < pronlen; p++) {
            int ci = dict_pron(lextree->dict, dictwid, p);
            if (p == 0) {       /* Root phone, handle required left contexts */
                /* Find if we already have an lc_pnodelist for the first phone of this word */
		fsg_glist_linklist_t *glist;

                rc = dict_pron(lextree->dict, dictwid, 1);
		for (glist = *curglist;
                     glist && glist->glist;
                     glist = glist->next) {
		    if (glist->ci == ci && glist->rc == rc)
			break;
		}
		if (glist && glist->glist) {
		    assert(glist->ci == ci && glist->rc == rc);
		    /* We've found a valid glist. Hook to it and move to next phoneme */
                    E_DEBUG(2,("Found match for (%d,%d)\n", ci, rc));
		    lc_pnodelist = glist->glist;
                    /* Set the predecessor node for the future tree first */
		    pred = (fsg_pnode_t *) gnode_ptr(lc_pnodelist);
		    continue;
		}
		else {
		    /* Two cases that can bring us here
		     * a. glist == NULL, i.e. end of current list. Create new entry.
		     * b. glist->glist == NULL, i.e. first entry into list.
		     */
		    if (glist == NULL) { /* Case a; reduce it to case b by allocing glist */
		        glist = (fsg_glist_linklist_t*) ckd_calloc(1, sizeof(fsg_glist_linklist_t));
			glist->next = *curglist;
                        *curglist = glist;
		    }
		    glist->ci = ci;
                    glist->rc = rc;
		    lc_pnodelist = glist->glist = NULL; /* Gets created below */
		}

                for (i = 0; lclist[i] >= 0; i++) {
                    lc = lclist[i];
                    ssid = dict2pid_ldiph_lc(lextree->d2p, ci, rc, lc);
                    tmatid = bin_mdef_pid2tmatid(lextree->mdef, dict_first_phone(lextree->dict, dictwid));
                    /* Compression is not done by d2p, so we do it
                     * here.  This might be slow, but it might not
                     * be... we'll see. */
                    pnode = ssid_pnode_map[0];
                    for (j = 0; j < n_ci && ssid_pnode_map[j] != NULL; ++j) {
                        pnode = ssid_pnode_map[j];
                        if (hmm_nonmpx_ssid(&pnode->hmm) == ssid)
                            break;
                    }
                    assert(j < n_ci);
                    if (!pnode) {       /* Allocate pnode for this new ssid */
                        pnode =
                            (fsg_pnode_t *) ckd_calloc(1,
                                                       sizeof
                                                       (fsg_pnode_t));
                        pnode->ctx = lextree->ctx;
	                /* This bit is tricky! For now we'll put the prob in the final link only */
                        /* pnode->logs2prob = (fsg_link_logs2prob(fsglink) >> SENSCR_SHIFT)
                           + lextree->wip + lextree->pip; */
                        pnode->logs2prob = lextree->wip + lextree->pip;
                        pnode->ci_ext = dict_first_phone(lextree->dict, dictwid);
                        pnode->ppos = 0;
                        pnode->leaf = FALSE;
                        pnode->sibling = root;  /* All root nodes linked together */
                        pnode->alloc_next = head;
                        head = pnode;
                        root = pnode;
                        ++n_lc_alloc;

                        hmm_init(lextree->ctx, &pnode->hmm, FALSE, ssid, tmatid);

                        lc_pnodelist =
                            glist_add_ptr(lc_pnodelist, (void *) pnode);
                        ssid_pnode_map[j] = pnode;
                    }
                    fsg_pnode_add_ctxt(pnode, lc);
                }
		/* Put the lc_pnodelist back into glist */
		glist->glist = lc_pnodelist;

                /* The predecessor node for the future tree is the root */
		pred = root;
            }
            else if (p != pronlen - 1) {        /* Word internal phone */
                fsg_pnode_t    *pnodeyoungest;

                ssid = dict2pid_internal(lextree->d2p, dictwid, p);
                tmatid = bin_mdef_pid2tmatid(lextree->mdef, dict_pron (lextree->dict, dictwid, p));
	        /* First check if we already have this ssid in our tree */
		pnode = pred->next.succ;
		pnodeyoungest = pnode; /* The youngest sibling */
		while (pnode && (hmm_nonmpx_ssid(&pnode->hmm) != ssid || pnode->leaf)) {
		    pnode = pnode->sibling;
		}
		if (pnode && (hmm_nonmpx_ssid(&pnode->hmm) == ssid && !pnode->leaf)) {
		    /* Found the ssid; go to next phoneme */
                    E_DEBUG(2,("Found match for %d\n", ci));
		    pred = pnode;
		    continue;
		}

		/* pnode not found, allocate it */
                pnode = (fsg_pnode_t *) ckd_calloc(1, sizeof(fsg_pnode_t));
                pnode->ctx = lextree->ctx;
                pnode->logs2prob = lextree->pip;
                pnode->ci_ext = dict_pron(lextree->dict, dictwid, p);
                pnode->ppos = p;
                pnode->leaf = FALSE;
                pnode->sibling = pnodeyoungest; /* May be NULL */
                if (p == 1) {   /* Predecessor = set of root nodes for left ctxts */
                    for (gn = lc_pnodelist; gn; gn = gnode_next(gn)) {
                        pred = (fsg_pnode_t *) gnode_ptr(gn);
                        pred->next.succ = pnode;
                    }
                }
                else {          /* Predecessor = word internal node */
                    pred->next.succ = pnode;
                }
                pnode->alloc_next = head;
                head = pnode;
                ++n_int_alloc;

                hmm_init(lextree->ctx, &pnode->hmm, FALSE, ssid, tmatid);

                pred = pnode;
            }
            else {              /* Leaf phone, handle required right contexts */
	        /* Note, leaf phones are not part of the tree */
                xwdssid_t *rssid;
                memset((void *) ssid_pnode_map, 0,
                       n_ci * sizeof(fsg_pnode_t *));
                lc = dict_pron(lextree->dict, dictwid, p-1);
                rssid = dict2pid_rssid(lextree->d2p, ci, lc);
                tmatid = bin_mdef_pid2tmatid(lextree->mdef, dict_pron (lextree->dict, dictwid, p));

                for (i = 0; rclist[i] >= 0; i++) {
                    rc = rclist[i];

                    j = rssid->cimap[rc];
                    ssid = rssid->ssid[j];
                    pnode = ssid_pnode_map[j];

                    if (!pnode) {       /* Allocate pnode for this new ssid */
                        pnode =
                            (fsg_pnode_t *) ckd_calloc(1,
                                                       sizeof
                                                       (fsg_pnode_t));
                        pnode->ctx = lextree->ctx;
			/* We are plugging the word prob here. Ugly */
                        /* pnode->logs2prob = lextree->pip; */
                        pnode->logs2prob = (fsg_link_logs2prob(fsglink) >> SENSCR_SHIFT)
                            + lextree->pip;
                        pnode->ci_ext = dict_pron(lextree->dict, dictwid, p);
                        pnode->ppos = p;
                        pnode->leaf = TRUE;
                        pnode->sibling = rc_pnodelist ?
                            (fsg_pnode_t *) gnode_ptr(rc_pnodelist) : NULL;
                        pnode->next.fsglink = fsglink;
                        pnode->alloc_next = head;
                        head = pnode;
                        ++n_rc_alloc;

                        hmm_init(lextree->ctx, &pnode->hmm, FALSE, ssid, tmatid);

                        rc_pnodelist =
                            glist_add_ptr(rc_pnodelist, (void *) pnode);
                        ssid_pnode_map[j] = pnode;
                    }
                    else {
                        assert(hmm_nonmpx_ssid(&pnode->hmm) == ssid);
                    }
                    fsg_pnode_add_ctxt(pnode, rc);
                }

                if (p == 1) {   /* Predecessor = set of root nodes for left ctxts */
                    for (gn = lc_pnodelist; gn; gn = gnode_next(gn)) {
                        pred = (fsg_pnode_t *) gnode_ptr(gn);
                        if (!pred->next.succ)
                            pred->next.succ = (fsg_pnode_t *) gnode_ptr(rc_pnodelist);
                        else {
                            /* Link to the end of the sibling chain */
                            fsg_pnode_t *succ = pred->next.succ;
                            while (succ->sibling) succ = succ->sibling;
                            succ->sibling = (fsg_pnode_t*) gnode_ptr(rc_pnodelist);
                            /* Since all entries of lc_pnodelist point
                               to the same array, sufficient to update it once */
                            break; 
                        }
                    }
                }
                else {          /* Predecessor = word internal node */
                    if (!pred->next.succ)
                        pred->next.succ = (fsg_pnode_t *) gnode_ptr(rc_pnodelist);
                    else {
                        /* Link to the end of the sibling chain */
                        fsg_pnode_t *succ = pred->next.succ;
                        while (succ->sibling) succ = succ->sibling;
                        succ->sibling = (fsg_pnode_t *) gnode_ptr(rc_pnodelist);
                    }
                }
            }
        }

        ckd_free((void *) ssid_pnode_map);
        /* glist_free(lc_pnodelist);  Nope; this gets freed outside */
        glist_free(rc_pnodelist);
    }

    E_DEBUG(2,("Allocated %d HMMs (%d lc, %d rc, %d internal)\n",
               n_lc_alloc + n_rc_alloc + n_int_alloc,
               n_lc_alloc, n_rc_alloc, n_int_alloc));
    *alloc_head = head;

    return root;
}


static fsg_pnode_t *
fsg_psubtree_init(fsg_lextree_t *lextree,
                  fsg_model_t * fsg, int32 from_state,
                  fsg_pnode_t ** alloc_head)
{
    fsg_arciter_t *itor;
    fsg_link_t *fsglink;
    fsg_pnode_t *root;
    int32 n_ci, n_arc;
    fsg_glist_linklist_t *glist = NULL;

    root = NULL;
    assert(*alloc_head == NULL);

    n_ci = bin_mdef_n_ciphone(lextree->mdef);
    if (n_ci > (FSG_PNODE_CTXT_BVSZ * 32)) {
        E_FATAL
            ("#phones > %d; increase FSG_PNODE_CTXT_BVSZ and recompile\n",
             FSG_PNODE_CTXT_BVSZ * 32);
    }

    n_arc = 0;
    for (itor = fsg_model_arcs(fsg, from_state); itor; 
         itor = fsg_arciter_next(itor)) {
        int32 dst;
        fsglink = fsg_arciter_get(itor);
        dst = fsglink->to_state;

        if (fsg_link_wid(fsglink) < 0)
            continue;

        E_DEBUG(2,("Building lextree for arc from %d to %d: %s\n",
                   from_state, dst, fsg_model_word_str(fsg, fsg_link_wid(fsglink))));
        root = psubtree_add_trans(lextree, root, &glist, fsglink,
                                  lextree->lc[from_state],
                                  lextree->rc[dst],
                                  alloc_head);
        ++n_arc;
    }
    E_DEBUG(2,("State %d has %d outgoing arcs\n", from_state, n_arc));

    fsg_glist_linklist_free(glist);

    return root;
}


static void
fsg_psubtree_free(fsg_pnode_t * head)
{
    fsg_pnode_t *next;

    while (head) {
        next = head->alloc_next;
        hmm_deinit(&head->hmm);
        ckd_free(head);
        head = next;
    }
}

void fsg_psubtree_dump_node(fsg_lextree_t *tree, fsg_pnode_t *node, FILE *fp)
{    
    int32 i;
    fsg_link_t *tl;

    /* Indentation */
    for (i = 0; i <= node->ppos; i++)
        fprintf(fp, "  ");

    fprintf(fp, "%p.@", node);    /* Pointer used as node
                    		   * ID */
    fprintf(fp, " %5d.SS", hmm_nonmpx_ssid(&node->hmm));
    fprintf(fp, " %10d.LP", node->logs2prob);
    fprintf(fp, " %p.SIB", node->sibling);
    fprintf(fp, " %s.%d", bin_mdef_ciphone_str(tree->mdef, node->ci_ext), node->ppos);
    if ((node->ppos == 0) || node->leaf) {
        fprintf(fp, " [");
        for (i = 0; i < FSG_PNODE_CTXT_BVSZ; i++)
            fprintf(fp, "%08x", node->ctxt.bv[i]);
        fprintf(fp, "]");
    }
    if (node->leaf) {
        tl = node->next.fsglink;
        fprintf(fp, " {%s[%d->%d](%d)}",
                fsg_model_word_str(tree->fsg, tl->wid),
                tl->from_state, tl->to_state, tl->logs2prob);
    } else {
        fprintf(fp, " %p.NXT", node->next.succ);
    }
    fprintf(fp, "\n");

    return;
}

void 
fsg_psubtree_dump(fsg_lextree_t *tree, fsg_pnode_t *root, FILE * fp)
{
    fsg_pnode_t *succ;

    if (root == NULL) return;
    if (root->ppos == 0) {
        while(root->sibling && root->sibling->next.succ == root->next.succ) {
            fsg_psubtree_dump_node(tree, root, fp);
            root = root->sibling;
        }
        fflush(fp);
    }
    
    fsg_psubtree_dump_node(tree, root, fp);

    if (root->leaf) {
        if (root->ppos == 0 && root->sibling) { /* For single-phone words */
            fsg_psubtree_dump(tree, root->sibling,fp);
        }
        return;
    }

    succ = root->next.succ;
    while(succ) {
        fsg_psubtree_dump(tree, succ,fp);
        succ = succ->sibling;
    }

    if (root->ppos == 0) {
        fsg_psubtree_dump(tree, root->sibling,fp);
        fflush(fp);
    }

    return;
}

void
fsg_psubtree_pnode_deactivate(fsg_pnode_t * pnode)
{
    hmm_clear(&pnode->hmm);
}
