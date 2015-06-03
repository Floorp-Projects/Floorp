/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2013 Carnegie Mellon University.  All rights
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
* kws_search.c -- Search object for key phrase spotting.
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/strfuncs.h>
#include <sphinxbase/pio.h>
#include <sphinxbase/cmd_ln.h>

#include "pocketsphinx_internal.h"
#include "kws_search.h"

/** Access macros */
#define hmm_is_active(hmm) ((hmm)->frame > 0)
#define kws_nth_hmm(keyword,n) (&((keyword)->hmms[n]))

static ps_lattice_t *
kws_search_lattice(ps_search_t * search)
{
    return NULL;
}

static int
kws_search_prob(ps_search_t * search)
{
    return 0;
}

static void
kws_seg_free(ps_seg_t *seg)
{
    kws_seg_t *itor = (kws_seg_t *)seg;
    ckd_free(itor);
}

static void
kws_seg_fill(kws_seg_t *itor)
{
    kws_detection_t* detection = (kws_detection_t*)gnode_ptr(itor->detection);

    itor->base.word = detection->keyphrase;
    itor->base.sf = detection->sf;
    itor->base.ef = detection->ef;
    itor->base.prob = detection->prob;
    itor->base.ascr = detection->ascr;
    itor->base.lscr = 0;
}

static ps_seg_t *
kws_seg_next(ps_seg_t *seg)
{
    kws_seg_t *itor = (kws_seg_t *)seg;
    itor->detection = gnode_next(itor->detection);
    if (!itor->detection) {
        kws_seg_free(seg);
        return NULL;
    }

    kws_seg_fill(itor);

    return seg;
}

static ps_segfuncs_t kws_segfuncs = {
    /* seg_next */ kws_seg_next,
    /* seg_free */ kws_seg_free
};

static ps_seg_t *
kws_search_seg_iter(ps_search_t * search, int32 * out_score)
{
    kws_search_t *kwss = (kws_search_t *)search;
    kws_seg_t *itor;
    
    if (!kwss->detections->detect_list)
        return NULL;

    if (out_score)
        *out_score = 0;
    
    itor = (kws_seg_t *)ckd_calloc(1, sizeof(*itor));
    itor->base.vt = &kws_segfuncs;
    itor->base.search = search;
    itor->base.lwf = 1.0;
    itor->detection = kwss->detections->detect_list;
    kws_seg_fill(itor);
    return (ps_seg_t *)itor;
}

static ps_searchfuncs_t kws_funcs = {
    /* name: */ "kws",
    /* start: */ kws_search_start,
    /* step: */ kws_search_step,
    /* finish: */ kws_search_finish,
    /* reinit: */ kws_search_reinit,
    /* free: */ kws_search_free,
    /* lattice: */ kws_search_lattice,
    /* hyp: */ kws_search_hyp,
    /* prob: */ kws_search_prob,
    /* seg_iter: */ kws_search_seg_iter,
};

/* Scans the dictionary and check if all words are present. */
static int
kws_search_check_dict(kws_search_t * kwss)
{
    dict_t *dict;
    char **wrdptr;
    char *tmp_keyphrase;
    int32 nwrds, wid;
    int keyword_iter, i;
    uint8 success;

    success = TRUE;
    dict = ps_search_dict(kwss);

    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        tmp_keyphrase = (char *) ckd_salloc(kwss->keyphrases[keyword_iter].word);
        nwrds = str2words(tmp_keyphrase, NULL, 0);
        wrdptr = (char **) ckd_calloc(nwrds, sizeof(*wrdptr));
        str2words(tmp_keyphrase, wrdptr, nwrds);
        for (i = 0; i < nwrds; i++) {
            wid = dict_wordid(dict, wrdptr[i]);
            if (wid == BAD_S3WID) {
                E_ERROR("The word '%s' is missing in the dictionary\n",
                        wrdptr[i]);
                success = FALSE;
                break;
            }
        }
        ckd_free(wrdptr);
        ckd_free(tmp_keyphrase);    
    }
    return success;
}

/* Activate senones for scoring */
static void
kws_search_sen_active(kws_search_t * kwss)
{
    int i, keyword_iter;

    acmod_clear_active(ps_search_acmod(kwss));

    /* active phone loop hmms */
    for (i = 0; i < kwss->n_pl; i++)
        acmod_activate_hmm(ps_search_acmod(kwss), &kwss->pl_hmms[i]);

    /* activate hmms in active nodes */
    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword = &kwss->keyphrases[keyword_iter];
        for (i = 0; i < keyword->n_hmms; i++) {
            if (hmm_is_active(kws_nth_hmm(keyword, i)))
                acmod_activate_hmm(ps_search_acmod(kwss), kws_nth_hmm(keyword, i));
        }
    }
}

/*
* Evaluate all the active HMMs.
* (Executed once per frame.)
*/
static void
kws_search_hmm_eval(kws_search_t * kwss, int16 const *senscr)
{
    int32 i, keyword_iter;
    int32 bestscore = WORST_SCORE;

    hmm_context_set_senscore(kwss->hmmctx, senscr);

    /* evaluate hmms from phone loop */
    for (i = 0; i < kwss->n_pl; ++i) {
        hmm_t *hmm = &kwss->pl_hmms[i];
        int32 score;

        score = hmm_vit_eval(hmm);
        if (score BETTER_THAN bestscore)
            bestscore = score;
    }
    /* evaluate hmms for active nodes */
    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword = &kwss->keyphrases[keyword_iter];
        for (i = 0; i < keyword->n_hmms; i++) {
            hmm_t *hmm = kws_nth_hmm(keyword, i);

            if (hmm_is_active(hmm)) {
                int32 score;
                score = hmm_vit_eval(hmm);
                if (score BETTER_THAN bestscore)
                    bestscore = score;
            }
        }
    }

    kwss->bestscore = bestscore;
}

/*
* (Beam) prune the just evaluated HMMs, determine which ones remain
* active. Executed once per frame.
*/
static void
kws_search_hmm_prune(kws_search_t * kwss)
{
    int32 thresh, i, keyword_iter;

    thresh = kwss->bestscore + kwss->beam;

    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword = &kwss->keyphrases[keyword_iter];
        for (i = 0; i < keyword->n_hmms; i++) {
    	    hmm_t *hmm = kws_nth_hmm(keyword, i);
            if (hmm_is_active(hmm) && hmm_bestscore(hmm) < thresh)
                hmm_clear(hmm);
        }
    }
}


/**
* Do phone transitions
*/
static void
kws_search_trans(kws_search_t * kwss)
{
    hmm_t *pl_best_hmm = NULL;
    int32 best_out_score = WORST_SCORE;
    int i, keyword_iter;
    uint8 to_clear;

    /* select best hmm in phone-loop to be a predecessor */
    for (i = 0; i < kwss->n_pl; i++)
        if (hmm_out_score(&kwss->pl_hmms[i]) BETTER_THAN best_out_score) {
            best_out_score = hmm_out_score(&kwss->pl_hmms[i]);
            pl_best_hmm = &kwss->pl_hmms[i];
        }

    /* out probs are not ready yet */
    if (!pl_best_hmm)
        return;

    /* Check whether keyword wasn't spotted yet */
    to_clear = FALSE;
    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword;
        hmm_t *last_hmm;
        
        keyword = &kwss->keyphrases[keyword_iter];
        last_hmm = kws_nth_hmm(keyword, keyword->n_hmms - 1);
        if (hmm_is_active(last_hmm)
            && hmm_out_score(pl_best_hmm) BETTER_THAN WORST_SCORE) {
            
            if (hmm_out_score(last_hmm) - hmm_out_score(pl_best_hmm) 
                >= keyword->threshold) {

                int32 prob = hmm_out_score(last_hmm) - hmm_out_score(pl_best_hmm);
                kws_detections_add(kwss->detections, keyword->word, 
                                  hmm_out_history(last_hmm), 
                                  kwss->frame, prob, 
                                  hmm_out_score(last_hmm));
                to_clear = TRUE;
            } /* keyword is spotted */
        } /* last hmm of keyword is active */
    } /* keywords loop */

    if (to_clear) {
        for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
            kws_keyword_t* keyword = &kwss->keyphrases[keyword_iter];
            for (i = 0; i < keyword->n_hmms; i++) {
                hmm_clear(kws_nth_hmm(keyword, i));
            }
        }
    } /* clear all keywords because something was spotted */

    /* Make transition for all phone loop hmms */
    for (i = 0; i < kwss->n_pl; i++) {
        if (hmm_out_score(pl_best_hmm) + kwss->plp BETTER_THAN
            hmm_in_score(&kwss->pl_hmms[i])) {
            hmm_enter(&kwss->pl_hmms[i],
                      hmm_out_score(pl_best_hmm) + kwss->plp,
                      hmm_out_history(pl_best_hmm), kwss->frame + 1);
        }
    }

    /* Activate new keyword nodes, enter their hmms */
    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword = &kwss->keyphrases[keyword_iter];
        for (i = keyword->n_hmms - 1; i > 0; i--) {
            hmm_t *pred_hmm = kws_nth_hmm(keyword, i - 1);
	    hmm_t *hmm = kws_nth_hmm(keyword, i);

            if (hmm_is_active(pred_hmm)) {    
                if (!hmm_is_active(hmm)
                    || hmm_out_score(pred_hmm) BETTER_THAN
                    hmm_in_score(hmm))
                        hmm_enter(hmm, hmm_out_score(pred_hmm),
                                  hmm_out_history(pred_hmm), kwss->frame + 1);
            }
        }

        /* Enter keyword start node from phone loop */
        if (hmm_out_score(pl_best_hmm) BETTER_THAN
            hmm_in_score(kws_nth_hmm(keyword, 0)))
                hmm_enter(kws_nth_hmm(keyword, 0), hmm_out_score(pl_best_hmm),
                    kwss->frame, kwss->frame + 1);
    } /* keywords loop */
}

static int
kws_search_read_list(kws_search_t *kwss, const char* keyfile)
{
    FILE *list_file;
    lineiter_t *li;
    int i;
    
    if ((list_file = fopen(keyfile, "r")) == NULL) {
        E_ERROR_SYSTEM("Failed to open keyword file '%s'", keyfile);
        return -1;
    }

    /* count keyphrases amount */
    kwss->n_keyphrases = 0;
    for (li = lineiter_start(list_file); li; li = lineiter_next(li))
        if (li->len > 0)
            kwss->n_keyphrases++;
    kwss->keyphrases = (kws_keyword_t *)ckd_calloc(kwss->n_keyphrases, sizeof(*kwss->keyphrases));
    fseek(list_file, 0L, SEEK_SET);

    /* read keyphrases */
    for (li = lineiter_start(list_file), i=0; li; li = lineiter_next(li), i++) {
        size_t last_ptr = li->len - 1;
        kwss->keyphrases[i].threshold = kwss->def_threshold;
        while (li->buf[last_ptr] == '\n')
            last_ptr--;
        if (li->buf[last_ptr] == '/') {
            size_t digit_len, start;
            char digit[16];
            
            start = last_ptr - 1;
            while (li->buf[start] != '/' && start > 0)
                start--;
            digit_len = last_ptr - start;
            memcpy(digit, &li->buf[start+1], digit_len);
            kwss->keyphrases[i].threshold =  (int32) logmath_log(kwss->base.acmod->lmath, atof_c(digit)) 
                                              >> SENSCR_SHIFT;
            li->buf[start-1] = '\0';
        }
        li->buf[last_ptr + 1] = '\0';
        kwss->keyphrases[i].word = ckd_salloc(li->buf);
    }

    fclose(list_file);
    return 0;
}

ps_search_t *
kws_search_init(const char *keyphrase,
                const char *keyfile,
                cmd_ln_t * config,
                acmod_t * acmod, dict_t * dict, dict2pid_t * d2p)
{
    kws_search_t *kwss = (kws_search_t *) ckd_calloc(1, sizeof(*kwss));
    ps_search_init(ps_search_base(kwss), &kws_funcs, config, acmod, dict,
                   d2p);

    kwss->detections = (kws_detections_t *)ckd_calloc(1, sizeof(*kwss->detections));

    kwss->beam =
        (int32) logmath_log(acmod->lmath,
                            cmd_ln_float64_r(config,
                                             "-beam")) >> SENSCR_SHIFT;

    kwss->plp =
        (int32) logmath_log(acmod->lmath,
                            cmd_ln_float32_r(config,
                                             "-kws_plp")) >> SENSCR_SHIFT;

    kwss->def_threshold =
        (int32) logmath_log(acmod->lmath,
                            cmd_ln_float64_r(config,
                                             "-kws_threshold")) >>
        SENSCR_SHIFT;

    E_INFO("KWS(beam: %d, plp: %d, default threshold %d)\n",
           kwss->beam, kwss->plp, kwss->def_threshold);

    if (keyfile) {
	if (kws_search_read_list(kwss, keyfile) < 0) {
	    E_ERROR("Failed to create kws search\n");
	    kws_search_free(ps_search_base(kwss));
	    return NULL;
	}
    } else {
        kwss->n_keyphrases = 1;
        kwss->keyphrases = (kws_keyword_t *)ckd_calloc(kwss->n_keyphrases, sizeof(*kwss->keyphrases));
        kwss->keyphrases[0].threshold = kwss->def_threshold;
        kwss->keyphrases[0].word = ckd_salloc(keyphrase);
    }

    /* Check if all words are in dictionary */
    if (!kws_search_check_dict(kwss)) {
        kws_search_free(ps_search_base(kwss));
        return NULL;
    }

    /* Reinit for provided keyword */
    if (kws_search_reinit(ps_search_base(kwss),
                          ps_search_dict(kwss),
                          ps_search_dict2pid(kwss)) < 0) {
        ps_search_free(ps_search_base(kwss));
        return NULL;
    }

    return ps_search_base(kwss);
}

void
kws_search_free(ps_search_t * search)
{
    int i;
    kws_search_t *kwss;

    kwss = (kws_search_t *) search;
    ps_search_deinit(search);
    hmm_context_free(kwss->hmmctx);
    kws_detections_reset(kwss->detections);
    ckd_free(kwss->pl_hmms);
    for (i = 0; i < kwss->n_keyphrases; i++) {
        ckd_free(kwss->keyphrases[i].hmms);
        ckd_free(kwss->keyphrases[i].word);
    }
    ckd_free(kwss->keyphrases);
    ckd_free(kwss);
}

int
kws_search_reinit(ps_search_t * search, dict_t * dict, dict2pid_t * d2p)
{
    char **wrdptr;
    char *tmp_keyphrase;
    int32 wid, pronlen;
    int32 n_hmms, n_wrds;
    int32 ssid, tmatid;
    int i, j, p, keyword_iter;
    kws_search_t *kwss = (kws_search_t *) search;
    bin_mdef_t *mdef = search->acmod->mdef;
    int32 silcipid = bin_mdef_silphone(mdef);

    /* Free old dict2pid, dict */
    ps_search_base_reinit(search, dict, d2p);

    /* Initialize HMM context. */
    if (kwss->hmmctx)
        hmm_context_free(kwss->hmmctx);
    kwss->hmmctx =
        hmm_context_init(bin_mdef_n_emit_state(search->acmod->mdef),
                         search->acmod->tmat->tp, NULL,
                         search->acmod->mdef->sseq);
    if (kwss->hmmctx == NULL)
        return -1;

    /* Initialize phone loop HMMs. */
    if (kwss->pl_hmms) {
        for (i = 0; i < kwss->n_pl; ++i)
            hmm_deinit((hmm_t *) & kwss->pl_hmms[i]);
        ckd_free(kwss->pl_hmms);
    }
    kwss->n_pl = bin_mdef_n_ciphone(search->acmod->mdef);
    kwss->pl_hmms =
        (hmm_t *) ckd_calloc(kwss->n_pl, sizeof(*kwss->pl_hmms));
    for (i = 0; i < kwss->n_pl; ++i) {
        hmm_init(kwss->hmmctx, (hmm_t *) & kwss->pl_hmms[i],
                 FALSE,
                 bin_mdef_pid2ssid(search->acmod->mdef, i),
                 bin_mdef_pid2tmatid(search->acmod->mdef, i));
    }

    for (keyword_iter = 0; keyword_iter < kwss->n_keyphrases; keyword_iter++) {
        kws_keyword_t *keyword = &kwss->keyphrases[keyword_iter];

        /* Initialize keyphrase HMMs */
        tmp_keyphrase = (char *) ckd_salloc(keyword->word);
        n_wrds = str2words(tmp_keyphrase, NULL, 0);
        wrdptr = (char **) ckd_calloc(n_wrds, sizeof(*wrdptr));
        str2words(tmp_keyphrase, wrdptr, n_wrds);

        /* count amount of hmms */
        n_hmms = 0;
        for (i = 0; i < n_wrds; i++) {
            wid = dict_wordid(dict, wrdptr[i]);
            pronlen = dict_pronlen(dict, wid);
            n_hmms += pronlen;
        }

        /* allocate node array */
        if (keyword->hmms)
            ckd_free(keyword->hmms);
        keyword->hmms = (hmm_t *) ckd_calloc(n_hmms, sizeof(hmm_t));
        keyword->n_hmms = n_hmms;

        /* fill node array */
        j = 0;
        for (i = 0; i < n_wrds; i++) {
            wid = dict_wordid(dict, wrdptr[i]);
            pronlen = dict_pronlen(dict, wid);
            for (p = 0; p < pronlen; p++) {
                int32 ci = dict_pron(dict, wid, p);
                if (p == 0) {
                    /* first phone of word */
                    int32 rc =
                        pronlen > 1 ? dict_pron(dict, wid, 1) : silcipid;
                    ssid = dict2pid_ldiph_lc(d2p, ci, rc, silcipid);
                }
                else if (p == pronlen - 1) {
                    /* last phone of the word */
                    int32 lc = dict_pron(dict, wid, p - 1);
                    xwdssid_t *rssid = dict2pid_rssid(d2p, ci, lc);
                    int j = rssid->cimap[silcipid];
                    ssid = rssid->ssid[j];
                }
                else {
                    /* word internal phone */
                    ssid = dict2pid_internal(d2p, wid, p);
                }
                tmatid = bin_mdef_pid2tmatid(mdef, ci);
                hmm_init(kwss->hmmctx, &keyword->hmms[j], FALSE, ssid,
                         tmatid);
                j++;
            }
        }

        ckd_free(wrdptr);
        ckd_free(tmp_keyphrase);
    }

    return 0;
}

int
kws_search_start(ps_search_t * search)
{
    int i;
    kws_search_t *kwss = (kws_search_t *) search;

    kwss->frame = 0;
    kwss->bestscore = 0;
    kws_detections_reset(kwss->detections);

    /* Reset and enter all phone-loop HMMs. */
    for (i = 0; i < kwss->n_pl; ++i) {
        hmm_t *hmm = (hmm_t *) & kwss->pl_hmms[i];
        hmm_clear(hmm);
        hmm_enter(hmm, 0, -1, 0);
    }
    return 0;
}

int
kws_search_step(ps_search_t * search, int frame_idx)
{
    int16 const *senscr;
    kws_search_t *kwss = (kws_search_t *) search;
    acmod_t *acmod = search->acmod;

    /* Activate senones */
    if (!acmod->compallsen)
        kws_search_sen_active(kwss);

    /* Calculate senone scores for current frame. */
    senscr = acmod_score(acmod, &frame_idx);

    /* Evaluate hmms in phone loop and in active keyword nodes */
    kws_search_hmm_eval(kwss, senscr);

    /* Prune hmms with low prob */
    kws_search_hmm_prune(kwss);

    /* Do hmms transitions */
    kws_search_trans(kwss);

    ++kwss->frame;
    return 0;
}

int
kws_search_finish(ps_search_t * search)
{
    /* Nothing here */
    return 0;
}

char const *
kws_search_hyp(ps_search_t * search, int32 * out_score,
               int32 * out_is_final)
{
    kws_search_t *kwss = (kws_search_t *) search;
    if (out_score)
        *out_score = 0;

    if (search->hyp_str)
        ckd_free(search->hyp_str);
    kws_detections_hyp_str(kwss->detections, &search->hyp_str);
    
    return search->hyp_str;
}

char * 
kws_search_get_keywords(ps_search_t * search)
{
    int i, c, len;
    kws_search_t *kwss;
    char* line;

    kwss = (kws_search_t *) search;
    
    len = 0;
    for (i = 0; i < kwss->n_keyphrases; i++)
        len += strlen(kwss->keyphrases[i].word);
    len += kwss->n_keyphrases;
    
    c = 0;
    line = (char *)ckd_calloc(len, sizeof(*line));
    for (i = 0; i < kwss->n_keyphrases; i++) {
        char *keyword_str = kwss->keyphrases[i].word;
        memcpy(&line[c], keyword_str, strlen(keyword_str));
        c += strlen(keyword_str);
        line[c++] = '\n';
    }
    line[--c] = '\0';

    return line;
}
