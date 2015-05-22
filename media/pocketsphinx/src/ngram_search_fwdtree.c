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
 * @file ngram_search_fwdtree.c Lexicon tree search.
 */

/* System headers. */
#include <string.h>
#include <assert.h>

/* SphinxBase headers. */
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/listelem_alloc.h>
#include <sphinxbase/err.h>

/* Local headers. */
#include "ngram_search_fwdtree.h"
#include "phone_loop_search.h"

/* Turn this on to dump channels for debugging */
#define __CHAN_DUMP__		0
#if __CHAN_DUMP__
#define chan_v_eval(chan) hmm_dump_vit_eval(&(chan)->hmm, stderr)
#else
#define chan_v_eval(chan) hmm_vit_eval(&(chan)->hmm)
#endif

/*
 * Allocate that part of the search channel tree structure that is independent of the
 * LM in use.
 */
static void
init_search_tree(ngram_search_t *ngs)
{
    int32 w, ndiph, i, n_words, n_ci;
    dict_t *dict = ps_search_dict(ngs);
    bitvec_t *dimap;

    n_words = ps_search_n_words(ngs);
    ngs->homophone_set = ckd_calloc(n_words, sizeof(*ngs->homophone_set));

    /* Find #single phone words, and #unique first diphones (#root channels) in dict. */
    ndiph = 0;
    ngs->n_1ph_words = 0;
    n_ci = bin_mdef_n_ciphone(ps_search_acmod(ngs)->mdef);
    /* Allocate a bitvector with flags for each possible diphone. */
    dimap = bitvec_alloc(n_ci * n_ci);
    for (w = 0; w < n_words; w++) {
        if (!dict_real_word(dict, w))
            continue;
        if (dict_is_single_phone(dict, w))
            ++ngs->n_1ph_words;
        else {
            int ph0, ph1;
            ph0 = dict_first_phone(dict, w);
            ph1 = dict_second_phone(dict, w);
            /* Increment ndiph the first time we see a diphone. */
            if (bitvec_is_clear(dimap, ph0 * n_ci + ph1)) {
                bitvec_set(dimap, ph0 * n_ci + ph1);
                ++ndiph;
            }
        }
    }
    E_INFO("%d unique initial diphones\n", ndiph);
    bitvec_free(dimap);

    /* Add remaining dict words (</s>, <s>, <sil>, noise words) to single-phone words */
    ngs->n_1ph_words += dict_num_fillers(dict) + 2;
    ngs->n_root_chan_alloc = ndiph + 1;
    /* Verify that these are all *actually* single-phone words,
     * otherwise really bad things will happen to us. */
    for (w = 0; w < n_words; ++w) {
        if (dict_real_word(dict, w))
            continue;
        if (!dict_is_single_phone(dict, w)) {
            E_WARN("Filler word %d = %s has more than one phone, ignoring it.\n",
                   w, dict_wordstr(dict, w));
            --ngs->n_1ph_words;
        }
    }

    /* Allocate and initialize root channels */
    ngs->root_chan =
        ckd_calloc(ngs->n_root_chan_alloc, sizeof(*ngs->root_chan));
    for (i = 0; i < ngs->n_root_chan_alloc; i++) {
        hmm_init(ngs->hmmctx, &ngs->root_chan[i].hmm, TRUE, -1, -1);
        ngs->root_chan[i].penult_phn_wid = -1;
        ngs->root_chan[i].next = NULL;
    }

    /* Permanently allocate and initialize channels for single-phone
     * words (1/word). */
    ngs->rhmm_1ph = ckd_calloc(ngs->n_1ph_words, sizeof(*ngs->rhmm_1ph));
    i = 0;
    for (w = 0; w < n_words; w++) {
        if (!dict_is_single_phone(dict, w))
            continue;
        /* Use SIL as right context for these. */
        ngs->rhmm_1ph[i].ci2phone = bin_mdef_silphone(ps_search_acmod(ngs)->mdef);
        ngs->rhmm_1ph[i].ciphone = dict_first_phone(dict, w);
        hmm_init(ngs->hmmctx, &ngs->rhmm_1ph[i].hmm, TRUE,
                 bin_mdef_pid2ssid(ps_search_acmod(ngs)->mdef, ngs->rhmm_1ph[i].ciphone),
                 bin_mdef_pid2tmatid(ps_search_acmod(ngs)->mdef, ngs->rhmm_1ph[i].ciphone));
        ngs->rhmm_1ph[i].next = NULL;

        ngs->word_chan[w] = (chan_t *) &(ngs->rhmm_1ph[i]);
        i++;
    }

    ngs->single_phone_wid = ckd_calloc(ngs->n_1ph_words,
                                       sizeof(*ngs->single_phone_wid));
    E_INFO("%d root, %d non-root channels, %d single-phone words\n",
           ngs->n_root_chan, ngs->n_nonroot_chan, ngs->n_1ph_words);
}

/*
 * One-time initialization of internal channels in HMM tree.
 */
static void
init_nonroot_chan(ngram_search_t *ngs, chan_t * hmm, int32 ph, int32 ci, int32 tmatid)
{
    hmm->next = NULL;
    hmm->alt = NULL;
    hmm->info.penult_phn_wid = -1;
    hmm->ciphone = ci;
    hmm_init(ngs->hmmctx, &hmm->hmm, FALSE, ph, tmatid);
}

/*
 * Allocate and initialize search channel-tree structure.
 * At this point, all the root-channels have been allocated and partly initialized
 * (as per init_search_tree()), and channels for all the single-phone words have been
 * allocated and initialized.  None of the interior channels of search-trees have
 * been allocated.
 * This routine may be called on every utterance, after reinit_search_tree() clears
 * the search tree created for the previous utterance.  Meant for reconfiguring the
 * search tree to suit the currently active LM.
 */
static void
create_search_tree(ngram_search_t *ngs)
{
    chan_t *hmm;
    root_chan_t *rhmm;
    int32 w, i, j, p, ph, tmatid;
    int32 n_words;
    dict_t *dict = ps_search_dict(ngs);
    dict2pid_t *d2p = ps_search_dict2pid(ngs);

    n_words = ps_search_n_words(ngs);

    E_INFO("Creating search tree\n");

    for (w = 0; w < n_words; w++)
        ngs->homophone_set[w] = -1;

    E_INFO("before: %d root, %d non-root channels, %d single-phone words\n",
           ngs->n_root_chan, ngs->n_nonroot_chan, ngs->n_1ph_words);

    ngs->n_1ph_LMwords = 0;
    ngs->n_root_chan = 0;
    ngs->n_nonroot_chan = 0;

    for (w = 0; w < n_words; w++) {
        int ciphone, ci2phone;

        /* Ignore dictionary words not in LM */
        if (!ngram_model_set_known_wid(ngs->lmset, dict_basewid(dict, w)))
            continue;

        /* Handle single-phone words individually; not in channel tree */
        if (dict_is_single_phone(dict, w)) {
            E_DEBUG(1,("single_phone_wid[%d] = %s\n",
                       ngs->n_1ph_LMwords, dict_wordstr(dict, w)));
            ngs->single_phone_wid[ngs->n_1ph_LMwords++] = w;
            continue;
        }

        /* Find a root channel matching the initial diphone, or
         * allocate one if not found. */
        ciphone = dict_first_phone(dict, w);
        ci2phone = dict_second_phone(dict, w);
        for (i = 0; i < ngs->n_root_chan; ++i) {
            if (ngs->root_chan[i].ciphone == ciphone
                && ngs->root_chan[i].ci2phone == ci2phone)
                break;
        }
        if (i == ngs->n_root_chan) {
            rhmm = &(ngs->root_chan[ngs->n_root_chan]);
            rhmm->hmm.tmatid = bin_mdef_pid2tmatid(ps_search_acmod(ngs)->mdef, ciphone);
            /* Begin with CI phone?  Not sure this makes a difference... */
            hmm_mpx_ssid(&rhmm->hmm, 0) =
                bin_mdef_pid2ssid(ps_search_acmod(ngs)->mdef, ciphone);
            rhmm->ciphone = ciphone;
            rhmm->ci2phone = ci2phone;
            ngs->n_root_chan++;
        }
        else
            rhmm = &(ngs->root_chan[i]);

        E_DEBUG(3,("word %s rhmm %d\n", dict_wordstr(dict, w), rhmm - ngs->root_chan));
        /* Now, rhmm = root channel for w.  Go on to remaining phones */
        if (dict_pronlen(dict, w) == 2) {
            /* Next phone is the last; not kept in tree; add w to penult_phn_wid set */
            if ((j = rhmm->penult_phn_wid) < 0)
                rhmm->penult_phn_wid = w;
            else {
                for (; ngs->homophone_set[j] >= 0; j = ngs->homophone_set[j]);
                ngs->homophone_set[j] = w;
            }
        }
        else {
            /* Add remaining phones, except the last, to tree */
            ph = dict2pid_internal(d2p, w, 1);
            tmatid = bin_mdef_pid2tmatid(ps_search_acmod(ngs)->mdef, dict_pron(dict, w, 1));
            hmm = rhmm->next;
            if (hmm == NULL) {
                rhmm->next = hmm = listelem_malloc(ngs->chan_alloc);
                init_nonroot_chan(ngs, hmm, ph, dict_pron(dict, w, 1), tmatid);
                ngs->n_nonroot_chan++;
            }
            else {
                chan_t *prev_hmm = NULL;

                for (; hmm && (hmm_nonmpx_ssid(&hmm->hmm) != ph); hmm = hmm->alt)
                    prev_hmm = hmm;
                if (!hmm) {     /* thanks, rkm! */
                    prev_hmm->alt = hmm = listelem_malloc(ngs->chan_alloc);
                    init_nonroot_chan(ngs, hmm, ph, dict_pron(dict, w, 1), tmatid);
                    ngs->n_nonroot_chan++;
                }
            }
            E_DEBUG(3,("phone %s = %d\n",
                       bin_mdef_ciphone_str(ps_search_acmod(ngs)->mdef,
                                            dict_second_phone(dict, w)), ph));
            for (p = 2; p < dict_pronlen(dict, w) - 1; p++) {
                ph = dict2pid_internal(d2p, w, p);
                tmatid = bin_mdef_pid2tmatid(ps_search_acmod(ngs)->mdef, dict_pron(dict, w, p));
                if (!hmm->next) {
                    hmm->next = listelem_malloc(ngs->chan_alloc);
                    hmm = hmm->next;
                    init_nonroot_chan(ngs, hmm, ph, dict_pron(dict, w, p), tmatid);
                    ngs->n_nonroot_chan++;
                }
                else {
                    chan_t *prev_hmm = NULL;

                    for (hmm = hmm->next; hmm && (hmm_nonmpx_ssid(&hmm->hmm) != ph);
                         hmm = hmm->alt)
                        prev_hmm = hmm;
                    if (!hmm) { /* thanks, rkm! */
                        prev_hmm->alt = hmm = listelem_malloc(ngs->chan_alloc);
                        init_nonroot_chan(ngs, hmm, ph, dict_pron(dict, w, p), tmatid);
                        ngs->n_nonroot_chan++;
                    }
                }
                E_DEBUG(3,("phone %s = %d\n",
                           bin_mdef_ciphone_str(ps_search_acmod(ngs)->mdef,
                                                dict_pron(dict, w, p)), ph));
            }

            /* All but last phone of w in tree; add w to hmm->info.penult_phn_wid set */
            if ((j = hmm->info.penult_phn_wid) < 0)
                hmm->info.penult_phn_wid = w;
            else {
                for (; ngs->homophone_set[j] >= 0; j = ngs->homophone_set[j]);
                ngs->homophone_set[j] = w;
            }
        }
    }

    ngs->n_1ph_words = ngs->n_1ph_LMwords;

    /* Add filler words to the array of 1ph words. */
    for (w = 0; w < n_words; ++w) {
        /* Skip anything that doesn't actually have a single phone. */
        if (!dict_is_single_phone(dict, w))
            continue;
        /* Also skip "real words" and things that are in the LM. */
        if (dict_real_word(dict, w))
            continue;
        if (ngram_model_set_known_wid(ngs->lmset, dict_basewid(dict, w)))
            continue;
        E_DEBUG(1,("single_phone_wid[%d] = %s\n",
                   ngs->n_1ph_words, dict_wordstr(dict, w)));
        ngs->single_phone_wid[ngs->n_1ph_words++] = w;
    }

    if (ngs->n_nonroot_chan >= ngs->max_nonroot_chan) {
        /* Give some room for channels for new words added dynamically at run time */
        ngs->max_nonroot_chan = ngs->n_nonroot_chan + 128;
        E_INFO("after: max nonroot chan increased to %d\n", ngs->max_nonroot_chan);

        /* Free old active channel list array if any and allocate new one */
        if (ngs->active_chan_list)
            ckd_free_2d(ngs->active_chan_list);
        ngs->active_chan_list = ckd_calloc_2d(2, ngs->max_nonroot_chan,
                                              sizeof(**ngs->active_chan_list));
    }

    if (!ngs->n_root_chan)
	E_ERROR("No word from the language model has pronunciation in the dictionary\n");

    E_INFO("after: %d root, %d non-root channels, %d single-phone words\n",
           ngs->n_root_chan, ngs->n_nonroot_chan, ngs->n_1ph_words);
}

static void
reinit_search_subtree(ngram_search_t *ngs, chan_t * hmm)
{
    chan_t *child, *sibling;

    /* First free all children under hmm */
    for (child = hmm->next; child; child = sibling) {
        sibling = child->alt;
        reinit_search_subtree(ngs, child);
    }

    /* Now free hmm */
    hmm_deinit(&hmm->hmm);
    listelem_free(ngs->chan_alloc, hmm);
}

/*
 * Delete search tree by freeing all interior channels within search tree and
 * restoring root channel state to the init state (i.e., just after init_search_tree()).
 */
static void
reinit_search_tree(ngram_search_t *ngs)
{
    int32 i;
    chan_t *hmm, *sibling;

    for (i = 0; i < ngs->n_root_chan; i++) {
        hmm = ngs->root_chan[i].next;

        while (hmm) {
            sibling = hmm->alt;
            reinit_search_subtree(ngs, hmm);
            hmm = sibling;
        }

        ngs->root_chan[i].penult_phn_wid = -1;
        ngs->root_chan[i].next = NULL;
    }
    ngs->n_nonroot_chan = 0;
}

void
ngram_fwdtree_init(ngram_search_t *ngs)
{
    /* Allocate bestbp_rc, lastphn_cand, last_ltrans */
    ngs->bestbp_rc = ckd_calloc(bin_mdef_n_ciphone(ps_search_acmod(ngs)->mdef),
                                sizeof(*ngs->bestbp_rc));
    ngs->lastphn_cand = ckd_calloc(ps_search_n_words(ngs),
                                   sizeof(*ngs->lastphn_cand));
    init_search_tree(ngs);
    create_search_tree(ngs);
}

static void
deinit_search_tree(ngram_search_t *ngs)
{
    int i, w, n_words;

    n_words = ps_search_n_words(ngs);
    for (i = 0; i < ngs->n_root_chan_alloc; i++) {
        hmm_deinit(&ngs->root_chan[i].hmm);
    }
    if (ngs->rhmm_1ph) {
        for (i = w = 0; w < n_words; ++w) {
            if (!dict_is_single_phone(ps_search_dict(ngs), w))
                continue;
            hmm_deinit(&ngs->rhmm_1ph[i].hmm);
            ++i;
        }
        ckd_free(ngs->rhmm_1ph);
        ngs->rhmm_1ph = NULL;
    }
    ngs->n_root_chan = 0;
    ngs->n_root_chan_alloc = 0;
    ckd_free(ngs->root_chan);
    ngs->root_chan = NULL;
    ckd_free(ngs->single_phone_wid);
    ngs->single_phone_wid = NULL;
    ckd_free(ngs->homophone_set);
    ngs->homophone_set = NULL;
}

void
ngram_fwdtree_deinit(ngram_search_t *ngs)
{
    double n_speech = (double)ngs->n_tot_frame
            / cmd_ln_int32_r(ps_search_config(ngs), "-frate");

    E_INFO("TOTAL fwdtree %.2f CPU %.3f xRT\n",
           ngs->fwdtree_perf.t_tot_cpu,
           ngs->fwdtree_perf.t_tot_cpu / n_speech);
    E_INFO("TOTAL fwdtree %.2f wall %.3f xRT\n",
           ngs->fwdtree_perf.t_tot_elapsed,
           ngs->fwdtree_perf.t_tot_elapsed / n_speech);

    /* Reset non-root channels. */
    reinit_search_tree(ngs);
    /* Free the search tree. */
    deinit_search_tree(ngs);
    /* Free other stuff. */
    ngs->max_nonroot_chan = 0;
    ckd_free_2d(ngs->active_chan_list);
    ngs->active_chan_list = NULL;
    ckd_free(ngs->cand_sf);
    ngs->cand_sf = NULL;
    ckd_free(ngs->bestbp_rc);
    ngs->bestbp_rc = NULL;
    ckd_free(ngs->lastphn_cand);
    ngs->lastphn_cand = NULL;
}

int
ngram_fwdtree_reinit(ngram_search_t *ngs)
{
    /* Reset non-root channels. */
    reinit_search_tree(ngs);
    /* Free the search tree. */
    deinit_search_tree(ngs);
    /* Reallocate things that depend on the number of words. */
    ckd_free(ngs->lastphn_cand);
    ngs->lastphn_cand = ckd_calloc(ps_search_n_words(ngs),
                                   sizeof(*ngs->lastphn_cand));
    ckd_free(ngs->word_chan);
    ngs->word_chan = ckd_calloc(ps_search_n_words(ngs),
                                sizeof(*ngs->word_chan));
    /* Rebuild the search tree. */
    init_search_tree(ngs);
    create_search_tree(ngs);
    return 0;
}

void
ngram_fwdtree_start(ngram_search_t *ngs)
{
    ps_search_t *base = (ps_search_t *)ngs;
    int32 i, w, n_words;
    root_chan_t *rhmm;

    n_words = ps_search_n_words(ngs);

    /* Reset utterance statistics. */
    memset(&ngs->st, 0, sizeof(ngs->st));
    ptmr_reset(&ngs->fwdtree_perf);
    ptmr_start(&ngs->fwdtree_perf);

    /* Reset backpointer table. */
    ngs->bpidx = 0;
    ngs->bss_head = 0;

    /* Reset word lattice. */
    for (i = 0; i < n_words; ++i)
        ngs->word_lat_idx[i] = NO_BP;

    /* Reset active HMM and word lists. */
    ngs->n_active_chan[0] = ngs->n_active_chan[1] = 0;
    ngs->n_active_word[0] = ngs->n_active_word[1] = 0;

    /* Reset scores. */
    ngs->best_score = 0;
    ngs->renormalized = 0;

    /* Reset other stuff. */
    for (i = 0; i < n_words; i++)
        ngs->last_ltrans[i].sf = -1;
    ngs->n_frame = 0;

    /* Clear the hypothesis string. */
    ckd_free(base->hyp_str);
    base->hyp_str = NULL;

    /* Reset the permanently allocated single-phone words, since they
     * may have junk left over in them from FWDFLAT. */
    for (i = 0; i < ngs->n_1ph_words; i++) {
        w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];
        hmm_clear(&rhmm->hmm);
    }

    /* Start search with <s>; word_chan[<s>] is permanently allocated */
    rhmm = (root_chan_t *) ngs->word_chan[dict_startwid(ps_search_dict(ngs))];
    hmm_clear(&rhmm->hmm);
    hmm_enter(&rhmm->hmm, 0, NO_BP, 0);
}

/*
 * Mark the active senones for all senones belonging to channels that are active in the
 * current frame.
 */
static void
compute_sen_active(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    chan_t *hmm, **acl;
    int32 i, w, *awl;

    acmod_clear_active(ps_search_acmod(ngs));

    /* Flag active senones for root channels */
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        if (hmm_frame(&rhmm->hmm) == frame_idx)
            acmod_activate_hmm(ps_search_acmod(ngs), &rhmm->hmm);
    }

    /* Flag active senones for nonroot channels in HMM tree */
    i = ngs->n_active_chan[frame_idx & 0x1];
    acl = ngs->active_chan_list[frame_idx & 0x1];
    for (hmm = *(acl++); i > 0; --i, hmm = *(acl++)) {
        acmod_activate_hmm(ps_search_acmod(ngs), &hmm->hmm);
    }

    /* Flag active senones for individual word channels */
    i = ngs->n_active_word[frame_idx & 0x1];
    awl = ngs->active_word_list[frame_idx & 0x1];
    for (w = *(awl++); i > 0; --i, w = *(awl++)) {
        for (hmm = ngs->word_chan[w]; hmm; hmm = hmm->next) {
            acmod_activate_hmm(ps_search_acmod(ngs), &hmm->hmm);
        }
    }
    for (i = 0; i < ngs->n_1ph_words; i++) {
        w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];

        if (hmm_frame(&rhmm->hmm) == frame_idx)
            acmod_activate_hmm(ps_search_acmod(ngs), &rhmm->hmm);
    }
}

static void
renormalize_scores(ngram_search_t *ngs, int frame_idx, int32 norm)
{
    root_chan_t *rhmm;
    chan_t *hmm, **acl;
    int32 i, w, *awl;

    /* Renormalize root channels */
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        if (hmm_frame(&rhmm->hmm) == frame_idx) {
            hmm_normalize(&rhmm->hmm, norm);
        }
    }

    /* Renormalize nonroot channels in HMM tree */
    i = ngs->n_active_chan[frame_idx & 0x1];
    acl = ngs->active_chan_list[frame_idx & 0x1];
    for (hmm = *(acl++); i > 0; --i, hmm = *(acl++)) {
        hmm_normalize(&hmm->hmm, norm);
    }

    /* Renormalize individual word channels */
    i = ngs->n_active_word[frame_idx & 0x1];
    awl = ngs->active_word_list[frame_idx & 0x1];
    for (w = *(awl++); i > 0; --i, w = *(awl++)) {
        for (hmm = ngs->word_chan[w]; hmm; hmm = hmm->next) {
            hmm_normalize(&hmm->hmm, norm);
        }
    }
    for (i = 0; i < ngs->n_1ph_words; i++) {
        w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];
        if (hmm_frame(&rhmm->hmm) == frame_idx) {
            hmm_normalize(&rhmm->hmm, norm);
        }
    }

    ngs->renormalized = TRUE;
}

static int32
eval_root_chan(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    int32 i, bestscore;

    bestscore = WORST_SCORE;
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        if (hmm_frame(&rhmm->hmm) == frame_idx) {
            int32 score = chan_v_eval(rhmm);
            if (score BETTER_THAN bestscore)
                bestscore = score;
            ++ngs->st.n_root_chan_eval;
        }
    }
    return (bestscore);
}

static int32
eval_nonroot_chan(ngram_search_t *ngs, int frame_idx)
{
    chan_t *hmm, **acl;
    int32 i, bestscore;

    i = ngs->n_active_chan[frame_idx & 0x1];
    acl = ngs->active_chan_list[frame_idx & 0x1];
    bestscore = WORST_SCORE;
    ngs->st.n_nonroot_chan_eval += i;

    for (hmm = *(acl++); i > 0; --i, hmm = *(acl++)) {
        int32 score = chan_v_eval(hmm);
        assert(hmm_frame(&hmm->hmm) == frame_idx);
        if (score BETTER_THAN bestscore)
            bestscore = score;
    }

    return bestscore;
}

static int32
eval_word_chan(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    chan_t *hmm;
    int32 i, w, bestscore, *awl, j, k;

    k = 0;
    bestscore = WORST_SCORE;
    awl = ngs->active_word_list[frame_idx & 0x1];

    i = ngs->n_active_word[frame_idx & 0x1];
    for (w = *(awl++); i > 0; --i, w = *(awl++)) {
        assert(bitvec_is_set(ngs->word_active, w));
        bitvec_clear(ngs->word_active, w);
        assert(ngs->word_chan[w] != NULL);

        for (hmm = ngs->word_chan[w]; hmm; hmm = hmm->next) {
            int32 score;

            assert(hmm_frame(&hmm->hmm) == frame_idx);
            score = chan_v_eval(hmm);
            /*printf("eval word chan %d score %d\n", w, score); */

            if (score BETTER_THAN bestscore)
                bestscore = score;

            k++;
        }
    }

    /* Similarly for statically allocated single-phone words */
    j = 0;
    for (i = 0; i < ngs->n_1ph_words; i++) {
        int32 score;

        w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];
        if (hmm_frame(&rhmm->hmm) < frame_idx)
            continue;

        score = chan_v_eval(rhmm);
        /* printf("eval 1ph word chan %d score %d\n", w, score); */
        if (score BETTER_THAN bestscore && w != ps_search_finish_wid(ngs))
            bestscore = score;

        j++;
    }

    ngs->st.n_last_chan_eval += k + j;
    ngs->st.n_nonroot_chan_eval += k + j;
    ngs->st.n_word_lastchan_eval +=
        ngs->n_active_word[frame_idx & 0x1] + j;

    return bestscore;
}

static int32
evaluate_channels(ngram_search_t *ngs, int16 const *senone_scores, int frame_idx)
{
    int32 bs;

    hmm_context_set_senscore(ngs->hmmctx, senone_scores);
    ngs->best_score = eval_root_chan(ngs, frame_idx);
    if ((bs = eval_nonroot_chan(ngs, frame_idx)) BETTER_THAN ngs->best_score)
        ngs->best_score = bs;
    if ((bs = eval_word_chan(ngs, frame_idx)) BETTER_THAN ngs->best_score)
        ngs->best_score = bs;
    ngs->last_phone_best_score = bs;

    return ngs->best_score;
}

/*
 * Prune currently active root channels for next frame.  Also, perform exit
 * transitions out of them and activate successors.
 * score[] of pruned root chan set to WORST_SCORE elsewhere.
 */
static void
prune_root_chan(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    chan_t *hmm;
    int32 i, nf, w;
    int32 thresh, newphone_thresh, lastphn_thresh, newphone_score;
    chan_t **nacl;              /* next active list */
    lastphn_cand_t *candp;
    phone_loop_search_t *pls;

    nf = frame_idx + 1;
    thresh = ngs->best_score + ngs->dynamic_beam;
    newphone_thresh = ngs->best_score + ngs->pbeam;
    lastphn_thresh = ngs->best_score + ngs->lpbeam;
    nacl = ngs->active_chan_list[nf & 0x1];
    pls = (phone_loop_search_t *)ps_search_lookahead(ngs);

    for (i = 0, rhmm = ngs->root_chan; i < ngs->n_root_chan; i++, rhmm++) {
        E_DEBUG(3,("Root channel %d frame %d score %d thresh %d\n",
                   i, hmm_frame(&rhmm->hmm), hmm_bestscore(&rhmm->hmm), thresh));
        /* First check if this channel was active in current frame */
        if (hmm_frame(&rhmm->hmm) < frame_idx)
            continue;

        if (hmm_bestscore(&rhmm->hmm) BETTER_THAN thresh) {
            hmm_frame(&rhmm->hmm) = nf;  /* rhmm will be active in next frame */
            E_DEBUG(3,("Preserving root channel %d score %d\n", i, hmm_bestscore(&rhmm->hmm)));
            /* transitions out of this root channel */
            /* transition to all next-level channels in the HMM tree */
            newphone_score = hmm_out_score(&rhmm->hmm) + ngs->pip;
            if (pls != NULL || newphone_score BETTER_THAN newphone_thresh) {
                for (hmm = rhmm->next; hmm; hmm = hmm->alt) {
                    int32 pl_newphone_score = newphone_score
                        + phone_loop_search_score(pls, hmm->ciphone);
                    if (pl_newphone_score BETTER_THAN newphone_thresh) {
                        if ((hmm_frame(&hmm->hmm) < frame_idx)
                            || (newphone_score BETTER_THAN hmm_in_score(&hmm->hmm))) {
                            hmm_enter(&hmm->hmm, newphone_score,
                                      hmm_out_history(&rhmm->hmm), nf);
                            *(nacl++) = hmm;
                        }
                    }
                }
            }

            /*
             * Transition to last phone of all words for which this is the
             * penultimate phone (the last phones may need multiple right contexts).
             * Remember to remove the temporary newword_penalty.
             */
            if (pls != NULL || newphone_score BETTER_THAN lastphn_thresh) {
                for (w = rhmm->penult_phn_wid; w >= 0;
                     w = ngs->homophone_set[w]) {
                    int32 pl_newphone_score = newphone_score
                        + phone_loop_search_score
                        (pls, dict_last_phone(ps_search_dict(ngs),w));
                    E_DEBUG(3,("word %s newphone_score %d\n", dict_wordstr(ps_search_dict(ngs), w), newphone_score));
                    if (pl_newphone_score BETTER_THAN lastphn_thresh) {
                        candp = ngs->lastphn_cand + ngs->n_lastphn_cand;
                        ngs->n_lastphn_cand++;
                        candp->wid = w;
                        candp->score =
                            newphone_score - ngs->nwpen;
                        candp->bp = hmm_out_history(&rhmm->hmm);
                    }
                }
            }
        }
    }
    ngs->n_active_chan[nf & 0x1] = (int)(nacl - ngs->active_chan_list[nf & 0x1]);
}

/*
 * Prune currently active nonroot channels in HMM tree for next frame.  Also, perform
 * exit transitions out of such channels and activate successors.
 */
static void
prune_nonroot_chan(ngram_search_t *ngs, int frame_idx)
{
    chan_t *hmm, *nexthmm;
    int32 nf, w, i;
    int32 thresh, newphone_thresh, lastphn_thresh, newphone_score;
    chan_t **acl, **nacl;       /* active list, next active list */
    lastphn_cand_t *candp;
    phone_loop_search_t *pls;

    nf = frame_idx + 1;

    thresh = ngs->best_score + ngs->dynamic_beam;
    newphone_thresh = ngs->best_score + ngs->pbeam;
    lastphn_thresh = ngs->best_score + ngs->lpbeam;
    pls = (phone_loop_search_t *)ps_search_lookahead(ngs);

    acl = ngs->active_chan_list[frame_idx & 0x1];   /* currently active HMMs in tree */
    nacl = ngs->active_chan_list[nf & 0x1] + ngs->n_active_chan[nf & 0x1];

    for (i = ngs->n_active_chan[frame_idx & 0x1], hmm = *(acl++); i > 0;
         --i, hmm = *(acl++)) {
        assert(hmm_frame(&hmm->hmm) >= frame_idx);

        if (hmm_bestscore(&hmm->hmm) BETTER_THAN thresh) {
            /* retain this channel in next frame */
            if (hmm_frame(&hmm->hmm) != nf) {
                hmm_frame(&hmm->hmm) = nf;
                *(nacl++) = hmm;
            }

            /* transition to all next-level channel in the HMM tree */
            newphone_score = hmm_out_score(&hmm->hmm) + ngs->pip;
            if (pls != NULL || newphone_score BETTER_THAN newphone_thresh) {
                for (nexthmm = hmm->next; nexthmm; nexthmm = nexthmm->alt) {
                    int32 pl_newphone_score = newphone_score
                        + phone_loop_search_score(pls, nexthmm->ciphone);
                    if ((pl_newphone_score BETTER_THAN newphone_thresh)
                        && ((hmm_frame(&nexthmm->hmm) < frame_idx)
                            || (newphone_score
                                BETTER_THAN hmm_in_score(&nexthmm->hmm)))) {
                        if (hmm_frame(&nexthmm->hmm) != nf) {
                            /* Keep this HMM on the active list */
                            *(nacl++) = nexthmm;
                        }
                        hmm_enter(&nexthmm->hmm, newphone_score,
                                  hmm_out_history(&hmm->hmm), nf);
                    }
                }
            }

            /*
             * Transition to last phone of all words for which this is the
             * penultimate phone (the last phones may need multiple right contexts).
             * Remember to remove the temporary newword_penalty.
             */
            if (pls != NULL || newphone_score BETTER_THAN lastphn_thresh) {
                for (w = hmm->info.penult_phn_wid; w >= 0;
                     w = ngs->homophone_set[w]) {
                    int32 pl_newphone_score = newphone_score
                        + phone_loop_search_score
                        (pls, dict_last_phone(ps_search_dict(ngs),w));
                    if (pl_newphone_score BETTER_THAN lastphn_thresh) {
                        candp = ngs->lastphn_cand + ngs->n_lastphn_cand;
                        ngs->n_lastphn_cand++;
                        candp->wid = w;
                        candp->score =
                            newphone_score - ngs->nwpen;
                        candp->bp = hmm_out_history(&hmm->hmm);
                    }
                }
            }
        }
        else if (hmm_frame(&hmm->hmm) != nf) {
            hmm_clear(&hmm->hmm);
        }
    }
    ngs->n_active_chan[nf & 0x1] = (int)(nacl - ngs->active_chan_list[nf & 0x1]);
}

/*
 * Execute the transition into the last phone for all candidates words emerging from
 * the HMM tree.  Attach LM scores to such transitions.
 * (Executed after pruning root and non-root, but before pruning word-chan.)
 */
static void
last_phone_transition(ngram_search_t *ngs, int frame_idx)
{
    int32 i, j, k, nf, bp, bpend, w;
    lastphn_cand_t *candp;
    int32 *nawl;
    int32 thresh;
    int32 bestscore, dscr;
    chan_t *hmm;
    bptbl_t *bpe;
    int32 n_cand_sf = 0;

    nf = frame_idx + 1;
    nawl = ngs->active_word_list[nf & 0x1];
    ngs->st.n_lastphn_cand_utt += ngs->n_lastphn_cand;

    /* For each candidate word (entering its last phone) */
    /* If best LM score and bp for candidate known use it, else sort cands by startfrm */
    for (i = 0, candp = ngs->lastphn_cand; i < ngs->n_lastphn_cand; i++, candp++) {
        int32 start_score;

        /* This can happen if recognition fails. */
        if (candp->bp == -1)
            continue;
        /* Backpointer entry for it. */
        bpe = &(ngs->bp_table[candp->bp]);

        /* Subtract starting score for candidate, leave it with only word score */
        start_score = ngram_search_exit_score
            (ngs, bpe, dict_first_phone(ps_search_dict(ngs), candp->wid));
        assert(start_score BETTER_THAN WORST_SCORE);
        candp->score -= start_score;

        /*
         * If this candidate not occurred in an earlier frame, prepare for finding
         * best transition score into last phone; sort by start frame.
         */
        /* i.e. if we don't have an entry in last_ltrans for this
         * <word,sf>, then create one */
        if (ngs->last_ltrans[candp->wid].sf != bpe->frame + 1) {
            /* Look for an entry in cand_sf matching the backpointer
             * for this candidate. */
            for (j = 0; j < n_cand_sf; j++) {
                if (ngs->cand_sf[j].bp_ef == bpe->frame)
                    break;
            }
            /* Oh, we found one, so chain onto it. */
            if (j < n_cand_sf)
                candp->next = ngs->cand_sf[j].cand;
            else {
                /* Nope, let's make a new one, allocating cand_sf if necessary. */
                if (n_cand_sf >= ngs->cand_sf_alloc) {
                    if (ngs->cand_sf_alloc == 0) {
                        ngs->cand_sf =
                            ckd_calloc(CAND_SF_ALLOCSIZE,
                                       sizeof(*ngs->cand_sf));
                        ngs->cand_sf_alloc = CAND_SF_ALLOCSIZE;
                    }
                    else {
                        ngs->cand_sf_alloc += CAND_SF_ALLOCSIZE;
                        ngs->cand_sf = ckd_realloc(ngs->cand_sf,
                                                   ngs->cand_sf_alloc
                                                   * sizeof(*ngs->cand_sf));
                        E_INFO("cand_sf[] increased to %d entries\n",
                               ngs->cand_sf_alloc);
                    }
                }

                /* Use the newly created cand_sf. */
                j = n_cand_sf++;
                candp->next = -1; /* End of the chain. */
                ngs->cand_sf[j].bp_ef = bpe->frame;
            }
            /* Update it to point to this candidate. */
            ngs->cand_sf[j].cand = i;

            ngs->last_ltrans[candp->wid].dscr = WORST_SCORE;
            ngs->last_ltrans[candp->wid].sf = bpe->frame + 1;
        }
    }

    /* Compute best LM score and bp for new cands entered in the sorted lists above */
    for (i = 0; i < n_cand_sf; i++) {
        /* For the i-th unique end frame... */
        bp = ngs->bp_table_idx[ngs->cand_sf[i].bp_ef];
        bpend = ngs->bp_table_idx[ngs->cand_sf[i].bp_ef + 1];
        for (bpe = &(ngs->bp_table[bp]); bp < bpend; bp++, bpe++) {
            if (!bpe->valid)
                continue;
            /* For each candidate at the start frame find bp->cand transition-score */
            for (j = ngs->cand_sf[i].cand; j >= 0; j = candp->next) {
                int32 n_used;
                candp = &(ngs->lastphn_cand[j]);
                dscr = 
                    ngram_search_exit_score
                    (ngs, bpe, dict_first_phone(ps_search_dict(ngs), candp->wid));
                if (dscr BETTER_THAN WORST_SCORE) {
                    assert(!dict_filler_word(ps_search_dict(ngs), candp->wid));
                    dscr += ngram_tg_score(ngs->lmset,
                                           dict_basewid(ps_search_dict(ngs), candp->wid),
                                           bpe->real_wid,
                                           bpe->prev_real_wid,
                                           &n_used)>>SENSCR_SHIFT;
                }

                if (dscr BETTER_THAN ngs->last_ltrans[candp->wid].dscr) {
                    ngs->last_ltrans[candp->wid].dscr = dscr;
                    ngs->last_ltrans[candp->wid].bp = bp;
                }
            }
        }
    }

    /* Update best transitions for all candidates; also update best lastphone score */
    bestscore = ngs->last_phone_best_score;
    for (i = 0, candp = ngs->lastphn_cand; i < ngs->n_lastphn_cand; i++, candp++) {
        candp->score += ngs->last_ltrans[candp->wid].dscr;
        candp->bp = ngs->last_ltrans[candp->wid].bp;

        if (candp->score BETTER_THAN bestscore)
            bestscore = candp->score;
    }
    ngs->last_phone_best_score = bestscore;

    /* At this pt, we know the best entry score (with LM component) for all candidates */
    thresh = bestscore + ngs->lponlybeam;
    for (i = ngs->n_lastphn_cand, candp = ngs->lastphn_cand; i > 0; --i, candp++) {
        if (candp->score BETTER_THAN thresh) {
            w = candp->wid;

            ngram_search_alloc_all_rc(ngs, w);

            k = 0;
            for (hmm = ngs->word_chan[w]; hmm; hmm = hmm->next) {
                if ((hmm_frame(&hmm->hmm) < frame_idx)
                    || (candp->score BETTER_THAN hmm_in_score(&hmm->hmm))) {
                    assert(hmm_frame(&hmm->hmm) != nf);
                    hmm_enter(&hmm->hmm,
                              candp->score, candp->bp, nf);
                    k++;
                }
            }
            if (k > 0) {
                assert(bitvec_is_clear(ngs->word_active, w));
                assert(!dict_is_single_phone(ps_search_dict(ngs), w));
                *(nawl++) = w;
                bitvec_set(ngs->word_active, w);
            }
        }
    }
    ngs->n_active_word[nf & 0x1] = (int)(nawl - ngs->active_word_list[nf & 0x1]);
}

/*
 * Prune currently active word channels for next frame.  Also, perform exit
 * transitions out of such channels and active successors.
 */
static void
prune_word_chan(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    chan_t *hmm, *thmm;
    chan_t **phmmp;             /* previous HMM-pointer */
    int32 nf, w, i, k;
    int32 newword_thresh, lastphn_thresh;
    int32 *awl, *nawl;

    nf = frame_idx + 1;
    newword_thresh = ngs->last_phone_best_score + ngs->wbeam;
    lastphn_thresh = ngs->last_phone_best_score + ngs->lponlybeam;

    awl = ngs->active_word_list[frame_idx & 0x1];
    nawl = ngs->active_word_list[nf & 0x1] + ngs->n_active_word[nf & 0x1];

    /* Dynamically allocated last channels of multi-phone words */
    for (i = ngs->n_active_word[frame_idx & 0x1], w = *(awl++); i > 0;
         --i, w = *(awl++)) {
        k = 0;
        phmmp = &(ngs->word_chan[w]);
        for (hmm = ngs->word_chan[w]; hmm; hmm = thmm) {
            assert(hmm_frame(&hmm->hmm) >= frame_idx);

            thmm = hmm->next;
            if (hmm_bestscore(&hmm->hmm) BETTER_THAN lastphn_thresh) {
                /* retain this channel in next frame */
                hmm_frame(&hmm->hmm) = nf;
                k++;
                phmmp = &(hmm->next);

                /* Could if ((! skip_alt_frm) || (frame_idx & 0x1)) the following */
                if (hmm_out_score(&hmm->hmm) BETTER_THAN newword_thresh) {
                    /* can exit channel and recognize word */
                    ngram_search_save_bp(ngs, frame_idx, w,
                                 hmm_out_score(&hmm->hmm),
                                 hmm_out_history(&hmm->hmm),
                                 hmm->info.rc_id);
                }
            }
            else if (hmm_frame(&hmm->hmm) == nf) {
                phmmp = &(hmm->next);
            }
            else {
                hmm_deinit(&hmm->hmm);
                listelem_free(ngs->chan_alloc, hmm);
                *phmmp = thmm;
            }
        }
        if ((k > 0) && (bitvec_is_clear(ngs->word_active, w))) {
            assert(!dict_is_single_phone(ps_search_dict(ngs), w));
            *(nawl++) = w;
            bitvec_set(ngs->word_active, w);
        }
    }
    ngs->n_active_word[nf & 0x1] = (int)(nawl - ngs->active_word_list[nf & 0x1]);

    /*
     * Prune permanently allocated single-phone channels.
     * NOTES: score[] of pruned channels set to WORST_SCORE elsewhere.
     */
    for (i = 0; i < ngs->n_1ph_words; i++) {
        w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];
        E_DEBUG(3,("Single phone word %s frame %d score %d thresh %d outscore %d nwthresh %d\n",
                   dict_wordstr(ps_search_dict(ngs),w),
                   hmm_frame(&rhmm->hmm), hmm_bestscore(&rhmm->hmm),
                   lastphn_thresh, hmm_out_score(&rhmm->hmm), newword_thresh));
        if (hmm_frame(&rhmm->hmm) < frame_idx)
            continue;
        if (hmm_bestscore(&rhmm->hmm) BETTER_THAN lastphn_thresh) {
            hmm_frame(&rhmm->hmm) = nf;

            /* Could if ((! skip_alt_frm) || (frame_idx & 0x1)) the following */
            if (hmm_out_score(&rhmm->hmm) BETTER_THAN newword_thresh) {
                E_DEBUG(4,("Exiting single phone word %s with %d > %d, %d\n",
                           dict_wordstr(ps_search_dict(ngs),w),
                           hmm_out_score(&rhmm->hmm),
                           lastphn_thresh, newword_thresh));
                ngram_search_save_bp(ngs, frame_idx, w,
                             hmm_out_score(&rhmm->hmm),
                             hmm_out_history(&rhmm->hmm), 0);
            }
        }
    }
}

static void
prune_channels(ngram_search_t *ngs, int frame_idx)
{
    /* Clear last phone candidate list. */
    ngs->n_lastphn_cand = 0;
    /* Set the dynamic beam based on maxhmmpf here. */
    ngs->dynamic_beam = ngs->beam;
    if (ngs->maxhmmpf != -1
        && ngs->st.n_root_chan_eval + ngs->st.n_nonroot_chan_eval > ngs->maxhmmpf) {
        /* Build a histogram to approximately prune them. */
        int32 bins[256], bw, nhmms, i;
        root_chan_t *rhmm;
        chan_t **acl, *hmm;

        /* Bins go from zero (best score) to edge of beam. */
        bw = -ngs->beam / 256;
        memset(bins, 0, sizeof(bins));
        /* For each active root channel. */
        for (i = 0, rhmm = ngs->root_chan; i < ngs->n_root_chan; i++, rhmm++) {
            int32 b;

            /* Put it in a bin according to its bestscore. */
            b = (ngs->best_score - hmm_bestscore(&rhmm->hmm)) / bw;
            if (b >= 256)
                b = 255;
            ++bins[b];
        }
        /* For each active non-root channel. */
        acl = ngs->active_chan_list[frame_idx & 0x1];       /* currently active HMMs in tree */
        for (i = ngs->n_active_chan[frame_idx & 0x1], hmm = *(acl++);
             i > 0; --i, hmm = *(acl++)) {
            int32 b;

            /* Put it in a bin according to its bestscore. */
            b = (ngs->best_score - hmm_bestscore(&hmm->hmm)) / bw;
            if (b >= 256)
                b = 255;
            ++bins[b];
        }
        /* Walk down the bins to find the new beam. */
        for (i = nhmms = 0; i < 256; ++i) {
            nhmms += bins[i];
            if (nhmms > ngs->maxhmmpf)
                break;
        }
        ngs->dynamic_beam = -(i * bw);
    }

    prune_root_chan(ngs, frame_idx);
    prune_nonroot_chan(ngs, frame_idx);
    last_phone_transition(ngs, frame_idx);
    prune_word_chan(ngs, frame_idx);
}

/*
 * Limit the number of word exits in each frame to maxwpf.  And also limit the number of filler
 * words to 1.
 */
static void
bptable_maxwpf(ngram_search_t *ngs, int frame_idx)
{
    int32 bp, n;
    int32 bestscr, worstscr;
    bptbl_t *bpe, *bestbpe, *worstbpe;

    /* Don't prune if no pruing. */
    if (ngs->maxwpf == -1 || ngs->maxwpf == ps_search_n_words(ngs))
        return;

    /* Allow only one filler word exit (the best) per frame */
    bestscr = (int32) 0x80000000;
    bestbpe = NULL;
    n = 0;
    for (bp = ngs->bp_table_idx[frame_idx]; bp < ngs->bpidx; bp++) {
        bpe = &(ngs->bp_table[bp]);
        if (dict_filler_word(ps_search_dict(ngs), bpe->wid)) {
            if (bpe->score BETTER_THAN bestscr) {
                bestscr = bpe->score;
                bestbpe = bpe;
            }
            bpe->valid = FALSE;
            n++;                /* No. of filler words */
        }
    }
    /* Restore bestbpe to valid state */
    if (bestbpe != NULL) {
        bestbpe->valid = TRUE;
        --n;
    }

    /* Allow up to maxwpf best entries to survive; mark the remaining with valid = 0 */
    n = (ngs->bpidx
         - ngs->bp_table_idx[frame_idx]) - n;  /* No. of entries after limiting fillers */
    for (; n > ngs->maxwpf; --n) {
        /* Find worst BPTable entry */
        worstscr = (int32) 0x7fffffff;
        worstbpe = NULL;
        for (bp = ngs->bp_table_idx[frame_idx]; (bp < ngs->bpidx); bp++) {
            bpe = &(ngs->bp_table[bp]);
            if (bpe->valid && (bpe->score WORSE_THAN worstscr)) {
                worstscr = bpe->score;
                worstbpe = bpe;
            }
        }
        /* FIXME: Don't panic! */
        if (worstbpe == NULL)
            E_FATAL("PANIC: No worst BPtable entry remaining\n");
        worstbpe->valid = FALSE;
    }
}

static void
word_transition(ngram_search_t *ngs, int frame_idx)
{
    int32 i, k, bp, w, nf;
    int32 rc;
    int32 thresh, newscore, pl_newscore;
    bptbl_t *bpe;
    root_chan_t *rhmm;
    struct bestbp_rc_s *bestbp_rc_ptr;
    phone_loop_search_t *pls;
    dict_t *dict = ps_search_dict(ngs);
    dict2pid_t *d2p = ps_search_dict2pid(ngs);

    /*
     * Transition to start of new word instances (HMM tree roots); but only if words
     * other than </s> finished here.
     * But, first, find the best starting score for each possible right context phone.
     */
    for (i = bin_mdef_n_ciphone(ps_search_acmod(ngs)->mdef) - 1; i >= 0; --i)
        ngs->bestbp_rc[i].score = WORST_SCORE;
    k = 0;
    pls = (phone_loop_search_t *)ps_search_lookahead(ngs);
    /* Ugh, this is complicated.  Scan all word exits for this frame
     * (they have already been created by prune_word_chan()). */
    for (bp = ngs->bp_table_idx[frame_idx]; bp < ngs->bpidx; bp++) {
        bpe = &(ngs->bp_table[bp]);
        ngs->word_lat_idx[bpe->wid] = NO_BP;

        if (bpe->wid == ps_search_finish_wid(ngs))
            continue;
        k++;

        /* DICT2PID */
        /* Array of HMM scores corresponding to all the possible right
         * context expansions of the final phone.  It's likely that a
         * lot of these are going to be missing, actually. */
        if (bpe->last2_phone == -1) { /* implies s_idx == -1 */
            /* No right context expansion. */
            for (rc = 0; rc < bin_mdef_n_ciphone(ps_search_acmod(ngs)->mdef); ++rc) {
                if (bpe->score BETTER_THAN ngs->bestbp_rc[rc].score) {
                    E_DEBUG(4,("bestbp_rc[0] = %d lc %d\n",
                               bpe->score, bpe->last_phone));
                    ngs->bestbp_rc[rc].score = bpe->score;
                    ngs->bestbp_rc[rc].path = bp;
                    ngs->bestbp_rc[rc].lc = bpe->last_phone;
                }
            }
        }
        else {
            xwdssid_t *rssid = dict2pid_rssid(d2p, bpe->last_phone, bpe->last2_phone);
            int32 *rcss = &(ngs->bscore_stack[bpe->s_idx]);
            for (rc = 0; rc < bin_mdef_n_ciphone(ps_search_acmod(ngs)->mdef); ++rc) {
                if (rcss[rssid->cimap[rc]] BETTER_THAN ngs->bestbp_rc[rc].score) {
                    E_DEBUG(4,("bestbp_rc[%d] = %d lc %d\n",
                               rc, rcss[rssid->cimap[rc]], bpe->last_phone));
                    ngs->bestbp_rc[rc].score = rcss[rssid->cimap[rc]];
                    ngs->bestbp_rc[rc].path = bp;
                    ngs->bestbp_rc[rc].lc = bpe->last_phone;
                }
            }
        }
    }
    if (k == 0)
        return;

    nf = frame_idx + 1;
    thresh = ngs->best_score + ngs->dynamic_beam;
    /*
     * Hypothesize successors to words finished in this frame.
     * Main dictionary, multi-phone words transition to HMM-trees roots.
     */
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        bestbp_rc_ptr = &(ngs->bestbp_rc[rhmm->ciphone]);

        newscore = bestbp_rc_ptr->score + ngs->nwpen + ngs->pip;
        pl_newscore = newscore
            + phone_loop_search_score(pls, rhmm->ciphone);
        if (pl_newscore BETTER_THAN thresh) {
            if ((hmm_frame(&rhmm->hmm) < frame_idx)
                || (newscore BETTER_THAN hmm_in_score(&rhmm->hmm))) {
                hmm_enter(&rhmm->hmm, newscore,
                          bestbp_rc_ptr->path, nf);
                /* DICT2PID: Another place where mpx ssids are entered. */
                /* Look up the ssid to use when entering this mpx triphone. */
                hmm_mpx_ssid(&rhmm->hmm, 0) =
                    dict2pid_ldiph_lc(d2p, rhmm->ciphone, rhmm->ci2phone, bestbp_rc_ptr->lc);
                assert(hmm_mpx_ssid(&rhmm->hmm, 0) != BAD_SSID);
            }
        }
    }

    /*
     * Single phone words; no right context for these.  Cannot use bestbp_rc as
     * LM scores have to be included.  First find best transition to these words.
     */
    for (i = 0; i < ngs->n_1ph_LMwords; i++) {
        w = ngs->single_phone_wid[i];
        ngs->last_ltrans[w].dscr = (int32) 0x80000000;
    }
    for (bp = ngs->bp_table_idx[frame_idx]; bp < ngs->bpidx; bp++) {
        bpe = &(ngs->bp_table[bp]);
        if (!bpe->valid)
            continue;

        for (i = 0; i < ngs->n_1ph_LMwords; i++) {
            int32 n_used;
            w = ngs->single_phone_wid[i];
            newscore = ngram_search_exit_score
                (ngs, bpe, dict_first_phone(dict, w));
            E_DEBUG(4, ("initial newscore for %s: %d\n",
                        dict_wordstr(dict, w), newscore));
            if (newscore != WORST_SCORE)
                newscore += ngram_tg_score(ngs->lmset,
                                           dict_basewid(dict, w),
                                           bpe->real_wid,
                                           bpe->prev_real_wid,
                                           &n_used)>>SENSCR_SHIFT;

            /* FIXME: Not sure how WORST_SCORE could be better, but it
             * apparently happens. */
            if (newscore BETTER_THAN ngs->last_ltrans[w].dscr) {
                ngs->last_ltrans[w].dscr = newscore;
                ngs->last_ltrans[w].bp = bp;
            }
        }
    }

    /* Now transition to in-LM single phone words */
    for (i = 0; i < ngs->n_1ph_LMwords; i++) {
        w = ngs->single_phone_wid[i];
        /* Never transition into the start word (for one thing, it is
           a non-event in the language model.) */
        if (w == dict_startwid(ps_search_dict(ngs)))
            continue;
        rhmm = (root_chan_t *) ngs->word_chan[w];
        newscore = ngs->last_ltrans[w].dscr + ngs->pip;
	pl_newscore = newscore + phone_loop_search_score(pls, rhmm->ciphone);
        if (pl_newscore BETTER_THAN thresh) {
            bpe = ngs->bp_table + ngs->last_ltrans[w].bp;
            if ((hmm_frame(&rhmm->hmm) < frame_idx)
                || (newscore BETTER_THAN hmm_in_score(&rhmm->hmm))) {
                hmm_enter(&rhmm->hmm,
                          newscore, ngs->last_ltrans[w].bp, nf);
                /* DICT2PID: another place where mpx ssids are entered. */
                /* Look up the ssid to use when entering this mpx triphone. */
                hmm_mpx_ssid(&rhmm->hmm, 0) =
                    dict2pid_ldiph_lc(d2p, rhmm->ciphone, rhmm->ci2phone,
                                      dict_last_phone(dict, bpe->wid));
                assert(hmm_mpx_ssid(&rhmm->hmm, 0) != BAD_SSID);
            }
        }
    }

    /* Remaining words: <sil>, noise words.  No mpx for these! */
    w = ps_search_silence_wid(ngs);
    rhmm = (root_chan_t *) ngs->word_chan[w];
    bestbp_rc_ptr = &(ngs->bestbp_rc[ps_search_acmod(ngs)->mdef->sil]);
    newscore = bestbp_rc_ptr->score + ngs->silpen + ngs->pip;
    pl_newscore = newscore
        + phone_loop_search_score(pls, rhmm->ciphone);
    if (pl_newscore BETTER_THAN thresh) {
        if ((hmm_frame(&rhmm->hmm) < frame_idx)
            || (newscore BETTER_THAN hmm_in_score(&rhmm->hmm))) {
            hmm_enter(&rhmm->hmm,
                      newscore, bestbp_rc_ptr->path, nf);
        }
    }
    for (w = dict_filler_start(dict); w <= dict_filler_end(dict); w++) {
        if (w == ps_search_silence_wid(ngs))
            continue;
        /* Never transition into the start word (for one thing, it is
           a non-event in the language model.) */
        if (w == dict_startwid(ps_search_dict(ngs)))
            continue;
        rhmm = (root_chan_t *) ngs->word_chan[w];
        /* If this was not actually a single-phone word, rhmm will be NULL. */
        if (rhmm == NULL)
            continue;
        newscore = bestbp_rc_ptr->score + ngs->fillpen + ngs->pip;
        pl_newscore = newscore
            + phone_loop_search_score(pls, rhmm->ciphone);
        if (pl_newscore BETTER_THAN thresh) {
            if ((hmm_frame(&rhmm->hmm) < frame_idx)
                || (newscore BETTER_THAN hmm_in_score(&rhmm->hmm))) {
                hmm_enter(&rhmm->hmm,
                          newscore, bestbp_rc_ptr->path, nf);
            }
        }
    }
}

static void
deactivate_channels(ngram_search_t *ngs, int frame_idx)
{
    root_chan_t *rhmm;
    int i;

    /* Clear score[] of pruned root channels */
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        if (hmm_frame(&rhmm->hmm) == frame_idx) {
            hmm_clear(&rhmm->hmm);
        }
    }
    /* Clear score[] of pruned single-phone channels */
    for (i = 0; i < ngs->n_1ph_words; i++) {
        int32 w = ngs->single_phone_wid[i];
        rhmm = (root_chan_t *) ngs->word_chan[w];
        if (hmm_frame(&rhmm->hmm) == frame_idx) {
            hmm_clear(&rhmm->hmm);
        }
    }
}

int
ngram_fwdtree_search(ngram_search_t *ngs, int frame_idx)
{
    int16 const *senscr;

    /* Activate our HMMs for the current frame if need be. */
    if (!ps_search_acmod(ngs)->compallsen)
        compute_sen_active(ngs, frame_idx);

    /* Compute GMM scores for the current frame. */
    if ((senscr = acmod_score(ps_search_acmod(ngs), &frame_idx)) == NULL)
        return 0;
    ngs->st.n_senone_active_utt += ps_search_acmod(ngs)->n_senone_active;

    /* Mark backpointer table for current frame. */
    ngram_search_mark_bptable(ngs, frame_idx);

    /* If the best score is equal to or worse than WORST_SCORE,
     * recognition has failed, don't bother to keep trying. */
    if (ngs->best_score == WORST_SCORE || ngs->best_score WORSE_THAN WORST_SCORE)
        return 0;
    /* Renormalize if necessary */
    if (ngs->best_score + (2 * ngs->beam) WORSE_THAN WORST_SCORE) {
        E_INFO("Renormalizing Scores at frame %d, best score %d\n",
               frame_idx, ngs->best_score);
        renormalize_scores(ngs, frame_idx, ngs->best_score);
    }

    /* Evaluate HMMs */
    evaluate_channels(ngs, senscr, frame_idx);
    /* Prune HMMs and do phone transitions. */
    prune_channels(ngs, frame_idx);
    /* Do absolute pruning on word exits. */
    bptable_maxwpf(ngs, frame_idx);
    /* Do word transitions. */
    word_transition(ngs, frame_idx);
    /* Deactivate pruned HMMs. */
    deactivate_channels(ngs, frame_idx);

    ++ngs->n_frame;
    /* Return the number of frames processed. */
    return 1;
}

void
ngram_fwdtree_finish(ngram_search_t *ngs)
{
    int32 i, w, cf, *awl;
    root_chan_t *rhmm;
    chan_t *hmm, **acl;

    /* This is the number of frames processed. */
    cf = ps_search_acmod(ngs)->output_frame;
    /* Add a mark in the backpointer table for one past the final frame. */
    ngram_search_mark_bptable(ngs, cf);

    /* Deactivate channels lined up for the next frame */
    /* First, root channels of HMM tree */
    for (i = ngs->n_root_chan, rhmm = ngs->root_chan; i > 0; --i, rhmm++) {
        hmm_clear(&rhmm->hmm);
    }

    /* nonroot channels of HMM tree */
    i = ngs->n_active_chan[cf & 0x1];
    acl = ngs->active_chan_list[cf & 0x1];
    for (hmm = *(acl++); i > 0; --i, hmm = *(acl++)) {
        hmm_clear(&hmm->hmm);
    }

    /* word channels */
    i = ngs->n_active_word[cf & 0x1];
    awl = ngs->active_word_list[cf & 0x1];
    for (w = *(awl++); i > 0; --i, w = *(awl++)) {
        /* Don't accidentally free single-phone words! */
        if (dict_is_single_phone(ps_search_dict(ngs), w))
            continue;
        bitvec_clear(ngs->word_active, w);
        if (ngs->word_chan[w] == NULL)
            continue;
        ngram_search_free_all_rc(ngs, w);
    }

    /*
     * The previous search code did a postprocessing of the
     * backpointer table here, but we will postpone this until it is
     * absolutely necessary, i.e. when generating a word graph.
     * Likewise we don't actually have to decide what the exit word is
     * until somebody requests a backtrace.
     */

    ptmr_stop(&ngs->fwdtree_perf);
    /* Print out some statistics. */
    if (cf > 0) {
        double n_speech = (double)(cf + 1)
            / cmd_ln_int32_r(ps_search_config(ngs), "-frate");
        E_INFO("%8d words recognized (%d/fr)\n",
               ngs->bpidx, (ngs->bpidx + (cf >> 1)) / (cf + 1));
        E_INFO("%8d senones evaluated (%d/fr)\n", ngs->st.n_senone_active_utt,
               (ngs->st.n_senone_active_utt + (cf >> 1)) / (cf + 1));
        E_INFO("%8d channels searched (%d/fr), %d 1st, %d last\n",
               ngs->st.n_root_chan_eval + ngs->st.n_nonroot_chan_eval,
               (ngs->st.n_root_chan_eval + ngs->st.n_nonroot_chan_eval) / (cf + 1),
               ngs->st.n_root_chan_eval, ngs->st.n_last_chan_eval);
        E_INFO("%8d words for which last channels evaluated (%d/fr)\n",
               ngs->st.n_word_lastchan_eval,
               ngs->st.n_word_lastchan_eval / (cf + 1));
        E_INFO("%8d candidate words for entering last phone (%d/fr)\n",
               ngs->st.n_lastphn_cand_utt, ngs->st.n_lastphn_cand_utt / (cf + 1));
        E_INFO("fwdtree %.2f CPU %.3f xRT\n",
               ngs->fwdtree_perf.t_cpu,
               ngs->fwdtree_perf.t_cpu / n_speech);
        E_INFO("fwdtree %.2f wall %.3f xRT\n",
               ngs->fwdtree_perf.t_elapsed,
               ngs->fwdtree_perf.t_elapsed / n_speech);
    }
    /* dump_bptable(ngs); */
}
