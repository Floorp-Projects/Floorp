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
 * @file phone_loop_search.h Fast and rough context-independent phoneme loop search.
 */

#include <sphinxbase/err.h>

#include "phone_loop_search.h"

static int phone_loop_search_start(ps_search_t *search);
static int phone_loop_search_step(ps_search_t *search, int frame_idx);
static int phone_loop_search_finish(ps_search_t *search);
static int phone_loop_search_reinit(ps_search_t *search, dict_t *dict, dict2pid_t *d2p);
static void phone_loop_search_free(ps_search_t *search);
static char const *phone_loop_search_hyp(ps_search_t *search, int32 *out_score, int32 *out_is_final);
static int32 phone_loop_search_prob(ps_search_t *search);
static ps_seg_t *phone_loop_search_seg_iter(ps_search_t *search, int32 *out_score);

static ps_searchfuncs_t phone_loop_search_funcs = {
    /* name: */   "phone_loop",
    /* start: */  phone_loop_search_start,
    /* step: */   phone_loop_search_step,
    /* finish: */ phone_loop_search_finish,
    /* reinit: */ phone_loop_search_reinit,
    /* free: */   phone_loop_search_free,
    /* lattice: */  NULL,
    /* hyp: */      phone_loop_search_hyp,
    /* prob: */     phone_loop_search_prob,
    /* seg_iter: */ phone_loop_search_seg_iter,
};

static int
phone_loop_search_reinit(ps_search_t *search, dict_t *dict, dict2pid_t *d2p)
{
    phone_loop_search_t *pls = (phone_loop_search_t *)search;
    cmd_ln_t *config = ps_search_config(search);
    acmod_t *acmod = ps_search_acmod(search);
    int i;

    /* Free old dict2pid, dict, if necessary. */
    ps_search_base_reinit(search, dict, d2p);

    /* Initialize HMM context. */
    if (pls->hmmctx)
        hmm_context_free(pls->hmmctx);
    pls->hmmctx = hmm_context_init(bin_mdef_n_emit_state(acmod->mdef),
                                   acmod->tmat->tp, NULL, acmod->mdef->sseq);
    if (pls->hmmctx == NULL)
        return -1;

    /* Initialize penalty storage */
    pls->n_phones = bin_mdef_n_ciphone(acmod->mdef);
    pls->window = cmd_ln_int32_r(config, "-pl_window");
    if (pls->penalties)
        ckd_free(pls->penalties);
    pls->penalties = (int32 *)ckd_calloc(pls->n_phones, sizeof(*pls->penalties));
    if (pls->pen_buf)
        ckd_free_2d(pls->pen_buf);
    pls->pen_buf = (int32 **)ckd_calloc_2d(pls->window, pls->n_phones, sizeof(**pls->pen_buf));

    /* Initialize phone HMMs. */
    if (pls->hmms) {
        for (i = 0; i < pls->n_phones; ++i)
            hmm_deinit((hmm_t *)&pls->hmms[i]);
        ckd_free(pls->hmms);
    }
    pls->hmms = (hmm_t *)ckd_calloc(pls->n_phones, sizeof(*pls->hmms));
    for (i = 0; i < pls->n_phones; ++i) {
        hmm_init(pls->hmmctx, (hmm_t *)&pls->hmms[i],
                 FALSE,
                 bin_mdef_pid2ssid(acmod->mdef, i),
                 bin_mdef_pid2tmatid(acmod->mdef, i));
    }
    pls->penalty_weight = cmd_ln_float64_r(config, "-pl_weight");
    pls->beam = logmath_log(acmod->lmath, cmd_ln_float64_r(config, "-pl_beam")) >> SENSCR_SHIFT;
    pls->pbeam = logmath_log(acmod->lmath, cmd_ln_float64_r(config, "-pl_pbeam")) >> SENSCR_SHIFT;
    pls->pip = logmath_log(acmod->lmath, cmd_ln_float32_r(config, "-pl_pip")) >> SENSCR_SHIFT;
    E_INFO("State beam %d Phone exit beam %d Insertion penalty %d\n",
           pls->beam, pls->pbeam, pls->pip);

    return 0;
}

ps_search_t *
phone_loop_search_init(cmd_ln_t *config,
               acmod_t *acmod,
               dict_t *dict)
{
    phone_loop_search_t *pls;

    /* Allocate and initialize. */
    pls = (phone_loop_search_t *)ckd_calloc(1, sizeof(*pls));
    ps_search_init(ps_search_base(pls), &phone_loop_search_funcs,
                   config, acmod, dict, NULL);
    phone_loop_search_reinit(ps_search_base(pls), ps_search_dict(pls),
                             ps_search_dict2pid(pls));

    return ps_search_base(pls);
}

static void
phone_loop_search_free_renorm(phone_loop_search_t *pls)
{
    gnode_t *gn;
    for (gn = pls->renorm; gn; gn = gnode_next(gn))
        ckd_free(gnode_ptr(gn));
    glist_free(pls->renorm);
    pls->renorm = NULL;
}

static void
phone_loop_search_free(ps_search_t *search)
{
    phone_loop_search_t *pls = (phone_loop_search_t *)search;
    int i;

    ps_search_deinit(search);
    for (i = 0; i < pls->n_phones; ++i)
        hmm_deinit((hmm_t *)&pls->hmms[i]);
    phone_loop_search_free_renorm(pls);
    ckd_free_2d(pls->pen_buf);
    ckd_free(pls->hmms);
    ckd_free(pls->penalties);
    hmm_context_free(pls->hmmctx);
    ckd_free(pls);
}

static int
phone_loop_search_start(ps_search_t *search)
{
    phone_loop_search_t *pls = (phone_loop_search_t *)search;
    int i;

    /* Reset and enter all phone HMMs. */
    for (i = 0; i < pls->n_phones; ++i) {
        hmm_t *hmm = (hmm_t *)&pls->hmms[i];
        hmm_clear(hmm);
        hmm_enter(hmm, 0, -1, 0);
    }
    memset(pls->penalties, 0, pls->n_phones * sizeof(*pls->penalties));
    for (i = 0; i < pls->window; i++)
        memset(pls->pen_buf[i], 0, pls->n_phones * sizeof(*pls->pen_buf[i]));
    phone_loop_search_free_renorm(pls);
    pls->best_score = 0;
    pls->pen_buf_ptr = 0;

    return 0;
}

static void
renormalize_hmms(phone_loop_search_t *pls, int frame_idx, int32 norm)
{
    phone_loop_renorm_t *rn = (phone_loop_renorm_t *)ckd_calloc(1, sizeof(*rn));
    int i;

    pls->renorm = glist_add_ptr(pls->renorm, rn);
    rn->frame_idx = frame_idx;
    rn->norm = norm;

    for (i = 0; i < pls->n_phones; ++i) {
        hmm_normalize((hmm_t *)&pls->hmms[i], norm);
    }
}

static void
evaluate_hmms(phone_loop_search_t *pls, int16 const *senscr, int frame_idx)
{
    int32 bs = WORST_SCORE;
    int i;

    hmm_context_set_senscore(pls->hmmctx, senscr);

    for (i = 0; i < pls->n_phones; ++i) {
        hmm_t *hmm = (hmm_t *)&pls->hmms[i];
        int32 score;

        if (hmm_frame(hmm) < frame_idx)
            continue;
        score = hmm_vit_eval(hmm);
        if (score BETTER_THAN bs) {
            bs = score;
        }
    }
    pls->best_score = bs;
}

static void
store_scores(phone_loop_search_t *pls, int frame_idx)
{
    int i, j, itr;

    for (i = 0; i < pls->n_phones; ++i) {
        hmm_t *hmm = (hmm_t *)&pls->hmms[i];
        pls->pen_buf[pls->pen_buf_ptr][i] = (hmm_bestscore(hmm) - pls->best_score) * pls->penalty_weight;
    }
    pls->pen_buf_ptr++;
    pls->pen_buf_ptr = pls->pen_buf_ptr % pls->window;

    //update penalties
    for (i = 0; i < pls->n_phones; ++i) {
        pls->penalties[i] = WORST_SCORE;
        for (j = 0, itr = pls->pen_buf_ptr + 1; j < pls->window; j++, itr++) {
            itr = itr % pls->window;
            if (pls->pen_buf[itr][i] > pls->penalties[i])
                pls->penalties[i] = pls->pen_buf[itr][i];
        }
    }
}

static void
prune_hmms(phone_loop_search_t *pls, int frame_idx)
{
    int32 thresh = pls->best_score + pls->beam;
    int nf = frame_idx + 1;
    int i;

    /* Check all phones to see if they remain active in the next frame. */
    for (i = 0; i < pls->n_phones; ++i) {
        hmm_t *hmm = (hmm_t *)&pls->hmms[i];

        if (hmm_frame(hmm) < frame_idx)
            continue;
        /* Retain if score better than threshold. */
        if (hmm_bestscore(hmm) BETTER_THAN thresh) {
            hmm_frame(hmm) = nf;
        }
        else
            hmm_clear_scores(hmm);
    }
}

static void
phone_transition(phone_loop_search_t *pls, int frame_idx)
{
    int32 thresh = pls->best_score + pls->pbeam;
    int nf = frame_idx + 1;
    int i;

    /* Now transition out of phones whose last states are inside the
     * phone transition beam. */
    for (i = 0; i < pls->n_phones; ++i) {
        hmm_t *hmm = (hmm_t *)&pls->hmms[i];
        int32 newphone_score;
        int j;

        if (hmm_frame(hmm) != nf)
            continue;

        newphone_score = hmm_out_score(hmm) + pls->pip;
        if (newphone_score BETTER_THAN thresh) {
            /* Transition into all phones using the usual Viterbi rule. */
            for (j = 0; j < pls->n_phones; ++j) {
                hmm_t *nhmm = (hmm_t *)&pls->hmms[j];

                if (hmm_frame(nhmm) < frame_idx
                    || newphone_score BETTER_THAN hmm_in_score(nhmm)) {
                    hmm_enter(nhmm, newphone_score, hmm_out_history(hmm), nf);
                }
            }
        }
    }
}

static int
phone_loop_search_step(ps_search_t *search, int frame_idx)
{
    phone_loop_search_t *pls = (phone_loop_search_t *)search;
    acmod_t *acmod = ps_search_acmod(search);
    int16 const *senscr;
    int i;

    /* All CI senones are active all the time. */
    if (!ps_search_acmod(pls)->compallsen) {
        acmod_clear_active(ps_search_acmod(pls));
        for (i = 0; i < pls->n_phones; ++i)
            acmod_activate_hmm(acmod, (hmm_t *)&pls->hmms[i]);
    }

    /* Calculate senone scores for current frame. */
    senscr = acmod_score(acmod, &frame_idx);

    /* Renormalize, if necessary. */
    if (pls->best_score + (2 * pls->beam) WORSE_THAN WORST_SCORE) {
        E_INFO("Renormalizing Scores at frame %d, best score %d\n",
               frame_idx, pls->best_score);
        renormalize_hmms(pls, frame_idx, pls->best_score);
    }

    /* Evaluate phone HMMs for current frame. */
    evaluate_hmms(pls, senscr, frame_idx);

    /* Store hmm scores for senone penaly calculation */
    store_scores(pls, frame_idx);

    /* Prune phone HMMs. */
    prune_hmms(pls, frame_idx);

    /* Do phone transitions. */
    phone_transition(pls, frame_idx);

    return 0;
}

static int
phone_loop_search_finish(ps_search_t *search)
{
    /* Actually nothing to do here really. */
    return 0;
}

static char const *
phone_loop_search_hyp(ps_search_t *search, int32 *out_score, int32 *out_is_final)
{
    E_WARN("Hypotheses are not returned from phone loop search");
    return NULL;
}

static int32
phone_loop_search_prob(ps_search_t *search)
{
    /* FIXME: Actually... they ought to be. */
    E_WARN("Posterior probabilities are not returned from phone loop search");
    return 0;
}

static ps_seg_t *
phone_loop_search_seg_iter(ps_search_t *search, int32 *out_score)
{
    E_WARN("Hypotheses are not returned from phone loop search");
    return NULL;
}
