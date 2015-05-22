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
 * @file hmm.h Implementation of HMM base structure.
 */

/* System headers. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* SphinxBase headers. */
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/err.h>

/* Local headers. */
#include "hmm.h"

hmm_context_t *
hmm_context_init(int32 n_emit_state,
		 uint8 ** const *tp,
		 int16 const *senscore,
		 uint16 * const *sseq)
{
    hmm_context_t *ctx;

    assert(n_emit_state > 0);
    if (n_emit_state > HMM_MAX_NSTATE) {
        E_ERROR("Number of emitting states must be <= %d\n", HMM_MAX_NSTATE);
        return NULL;
    }

    ctx = ckd_calloc(1, sizeof(*ctx));
    ctx->n_emit_state = n_emit_state;
    ctx->tp = tp;
    ctx->senscore = senscore;
    ctx->sseq = sseq;
    ctx->st_sen_scr = ckd_calloc(n_emit_state, sizeof(*ctx->st_sen_scr));

    return ctx;
}

void
hmm_context_free(hmm_context_t *ctx)
{
    if (ctx == NULL)
        return;
    ckd_free(ctx->st_sen_scr);
    ckd_free(ctx);
}

void
hmm_init(hmm_context_t *ctx, hmm_t *hmm, int mpx, int ssid, int tmatid)
{
    hmm->ctx = ctx;
    hmm->mpx = mpx;
    hmm->n_emit_state = ctx->n_emit_state;
    if (mpx) {
        int i;
        hmm->ssid = BAD_SSID;
        hmm->senid[0] = ssid;
        for (i = 1; i < hmm_n_emit_state(hmm); ++i) {
            hmm->senid[i] = BAD_SSID;
        }
    }
    else {
        hmm->ssid = ssid;
        memcpy(hmm->senid, ctx->sseq[ssid], hmm->n_emit_state * sizeof(*hmm->senid));
    }
    hmm->tmatid = tmatid;
    hmm_clear(hmm);
}

void
hmm_deinit(hmm_t *hmm)
{
}

void
hmm_dump(hmm_t * hmm,
         FILE * fp)
{
    int32 i;

    if (hmm_is_mpx(hmm)) {
        fprintf(fp, "MPX   ");
        for (i = 0; i < hmm_n_emit_state(hmm); i++)
            fprintf(fp, " %11d", hmm_senid(hmm, i));
        fprintf(fp, " ( ");
        for (i = 0; i < hmm_n_emit_state(hmm); i++)
            fprintf(fp, "%d ", hmm_ssid(hmm, i));
        fprintf(fp, ")\n");
    }
    else {
        fprintf(fp, "SSID  ");
        for (i = 0; i < hmm_n_emit_state(hmm); i++)
            fprintf(fp, " %11d", hmm_senid(hmm, i));
        fprintf(fp, " (%d)\n", hmm_ssid(hmm, 0));
    }

    if (hmm->ctx->senscore) {
        fprintf(fp, "SENSCR");
        for (i = 0; i < hmm_n_emit_state(hmm); i++)
            fprintf(fp, " %11d", hmm_senscr(hmm, i));
        fprintf(fp, "\n");
    }

    fprintf(fp, "SCORES %11d", hmm_in_score(hmm));
    for (i = 1; i < hmm_n_emit_state(hmm); i++)
        fprintf(fp, " %11d", hmm_score(hmm, i));
    fprintf(fp, " %11d", hmm_out_score(hmm));
    fprintf(fp, "\n");

    fprintf(fp, "HISTID %11d", hmm_in_history(hmm));
    for (i = 1; i < hmm_n_emit_state(hmm); i++)
        fprintf(fp, " %11d", hmm_history(hmm, i));
    fprintf(fp, " %11d", hmm_out_history(hmm));
    fprintf(fp, "\n");

    if (hmm_in_score(hmm) > 0)
        fprintf(fp,
                "ALERT!! The input score %d is large than 0. Probably wrap around.\n",
                hmm_in_score(hmm));
    if (hmm_out_score(hmm) > 0)
        fprintf(fp,
                "ALERT!! The output score %d is large than 0. Probably wrap around\n.",
                hmm_out_score(hmm));

    fflush(fp);
}


void
hmm_clear_scores(hmm_t * h)
{
    int32 i;

    hmm_in_score(h) = WORST_SCORE;
    for (i = 1; i < hmm_n_emit_state(h); i++)
        hmm_score(h, i) = WORST_SCORE;
    hmm_out_score(h) = WORST_SCORE;

    h->bestscore = WORST_SCORE;
}

void
hmm_clear(hmm_t * h)
{
    int32 i;

    hmm_in_score(h) = WORST_SCORE;
    hmm_in_history(h) = -1;
    for (i = 1; i < hmm_n_emit_state(h); i++) {
        hmm_score(h, i) = WORST_SCORE;
        hmm_history(h, i) = -1;
    }
    hmm_out_score(h) = WORST_SCORE;
    hmm_out_history(h) = -1;

    h->bestscore = WORST_SCORE;
    h->frame = -1;
}

void
hmm_enter(hmm_t *h, int32 score, int32 histid, int frame)
{
    hmm_in_score(h) = score;
    hmm_in_history(h) = histid;
    hmm_frame(h) = frame;
}

void
hmm_normalize(hmm_t *h, int32 bestscr)
{
    int32 i;

    for (i = 0; i < hmm_n_emit_state(h); i++) {
        if (hmm_score(h, i) BETTER_THAN WORST_SCORE)
            hmm_score(h, i) -= bestscr;
    }
    if (hmm_out_score(h) BETTER_THAN WORST_SCORE)
        hmm_out_score(h) -= bestscr;
}

#define hmm_tprob_5st(i, j) (-tp[(i)*6+(j)])
#define nonmpx_senscr(i) (-senscore[sseq[i]])

static int32
hmm_vit_eval_5st_lr(hmm_t * hmm)
{
    int16 const *senscore = hmm->ctx->senscore;
    uint8 const *tp = hmm->ctx->tp[hmm->tmatid][0];
    uint16 const *sseq = hmm->senid;
    int32 s5, s4, s3, s2, s1, s0, t2, t1, t0, bestScore;

    /* It was the best of scores, it was the worst of scores. */
    bestScore = WORST_SCORE;

    /* Cache problem here! */
    s4 = hmm_score(hmm, 4) + nonmpx_senscr(4);
    s3 = hmm_score(hmm, 3) + nonmpx_senscr(3);
    /* Transitions into non-emitting state 5 */
    if (s3 BETTER_THAN WORST_SCORE) {
        t1 = s4 + hmm_tprob_5st(4, 5);
        t2 = s3 + hmm_tprob_5st(3, 5);
        if (t1 BETTER_THAN t2) {
            s5 = t1;
            hmm_out_history(hmm)  = hmm_history(hmm, 4);
        } else {
            s5 = t2;
            hmm_out_history(hmm)  = hmm_history(hmm, 3);
        }
        if (s5 WORSE_THAN WORST_SCORE) s5 = WORST_SCORE;
        hmm_out_score(hmm) = s5;
        bestScore = s5;
    }

    s2 = hmm_score(hmm, 2) + nonmpx_senscr(2);
    /* All transitions into state 4 */
    if (s2 BETTER_THAN WORST_SCORE) {
        t0 = s4 + hmm_tprob_5st(4, 4);
        t1 = s3 + hmm_tprob_5st(3, 4);
        t2 = s2 + hmm_tprob_5st(2, 4);
        if (t0 BETTER_THAN t1) {
            if (t2 BETTER_THAN t0) {
                s4 = t2;
                hmm_history(hmm, 4)  = hmm_history(hmm, 2);
            } else
                s4 = t0;
        } else {
            if (t2 BETTER_THAN t1) {
                s4 = t2;
                hmm_history(hmm, 4)  = hmm_history(hmm, 2);
            } else {
                s4 = t1;
                hmm_history(hmm, 4)  = hmm_history(hmm, 3);
            }
        }
        if (s4 WORSE_THAN WORST_SCORE) s4 = WORST_SCORE;
        if (s4 BETTER_THAN bestScore) bestScore = s4;
        hmm_score(hmm, 4) = s4;
    }

    s1 = hmm_score(hmm, 1) + nonmpx_senscr(1);
    /* All transitions into state 3 */
    if (s1 BETTER_THAN WORST_SCORE) {
        t0 = s3 + hmm_tprob_5st(3, 3);
        t1 = s2 + hmm_tprob_5st(2, 3);
        t2 = s1 + hmm_tprob_5st(1, 3);
        if (t0 BETTER_THAN t1) {
            if (t2 BETTER_THAN t0) {
                s3 = t2;
                hmm_history(hmm, 3)  = hmm_history(hmm, 1);
            } else
                s3 = t0;
        } else {
            if (t2 BETTER_THAN t1) {
                s3 = t2;
                hmm_history(hmm, 3)  = hmm_history(hmm, 1);
            } else {
                s3 = t1;
                hmm_history(hmm, 3)  = hmm_history(hmm, 2);
            }
        }
        if (s3 WORSE_THAN WORST_SCORE) s3 = WORST_SCORE;
        if (s3 BETTER_THAN bestScore) bestScore = s3;
        hmm_score(hmm, 3) = s3;
    }

    s0 = hmm_in_score(hmm) + nonmpx_senscr(0);
    /* All transitions into state 2 (state 0 is always active) */
    t0 = s2 + hmm_tprob_5st(2, 2);
    t1 = s1 + hmm_tprob_5st(1, 2);
    t2 = s0 + hmm_tprob_5st(0, 2);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s2 = t2;
            hmm_history(hmm, 2)  = hmm_in_history(hmm);
        } else
            s2 = t0;
    } else {
        if (t2 BETTER_THAN t1) {
            s2 = t2;
            hmm_history(hmm, 2)  = hmm_in_history(hmm);
        } else {
            s2 = t1;
            hmm_history(hmm, 2)  = hmm_history(hmm, 1);
        }
    }
    if (s2 WORSE_THAN WORST_SCORE) s2 = WORST_SCORE;
    if (s2 BETTER_THAN bestScore) bestScore = s2;
    hmm_score(hmm, 2) = s2;


    /* All transitions into state 1 */
    t0 = s1 + hmm_tprob_5st(1, 1);
    t1 = s0 + hmm_tprob_5st(0, 1);
    if (t0 BETTER_THAN t1) {
        s1 = t0;
    } else {
        s1 = t1;
        hmm_history(hmm, 1)  = hmm_in_history(hmm);
    }
    if (s1 WORSE_THAN WORST_SCORE) s1 = WORST_SCORE;
    if (s1 BETTER_THAN bestScore) bestScore = s1;
    hmm_score(hmm, 1) = s1;

    /* All transitions into state 0 */
    s0 = s0 + hmm_tprob_5st(0, 0);
    if (s0 WORSE_THAN WORST_SCORE) s0 = WORST_SCORE;
    if (s0 BETTER_THAN bestScore) bestScore = s0;
    hmm_in_score(hmm) = s0;

    hmm_bestscore(hmm) = bestScore;
    return bestScore;
}

#define mpx_senid(st) sseq[ssid[st]][st]
#define mpx_senscr(st) (-senscore[mpx_senid(st)])

static int32
hmm_vit_eval_5st_lr_mpx(hmm_t * hmm)
{
    uint8 const *tp = hmm->ctx->tp[hmm->tmatid][0];
    int16 const *senscore = hmm->ctx->senscore;
    uint16 * const *sseq = hmm->ctx->sseq;
    uint16 *ssid = hmm->senid;
    int32 bestScore;
    int32 s5, s4, s3, s2, s1, s0, t2, t1, t0;

    /* Don't propagate WORST_SCORE */
    if (ssid[4] == BAD_SSID)
        s4 = t1 = WORST_SCORE;
    else {
        s4 = hmm_score(hmm, 4) + mpx_senscr(4);
        t1 = s4 + hmm_tprob_5st(4, 5);
    }
    if (ssid[3] == BAD_SSID)
        s3 = t2 = WORST_SCORE;
    else {
        s3 = hmm_score(hmm, 3) + mpx_senscr(3);
        t2 = s3 + hmm_tprob_5st(3, 5);
    }
    if (t1 BETTER_THAN t2) {
        s5 = t1;
        hmm_out_history(hmm) = hmm_history(hmm, 4);
    }
    else {
        s5 = t2;
        hmm_out_history(hmm) = hmm_history(hmm, 3);
    }
    if (s5 WORSE_THAN WORST_SCORE) s5 = WORST_SCORE;
    hmm_out_score(hmm) = s5;
    bestScore = s5;

    /* Don't propagate WORST_SCORE */
    if (ssid[2] == BAD_SSID)
        s2 = t2 = WORST_SCORE;
    else {
        s2 = hmm_score(hmm, 2) + mpx_senscr(2);
        t2 = s2 + hmm_tprob_5st(2, 4);
    }

    t0 = t1 = WORST_SCORE;
    if (s4 != WORST_SCORE)
        t0 = s4 + hmm_tprob_5st(4, 4);
    if (s3 != WORST_SCORE)
        t1 = s3 + hmm_tprob_5st(3, 4);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s4 = t2;
            hmm_history(hmm, 4) = hmm_history(hmm, 2);
            ssid[4] = ssid[2];
        }
        else
            s4 = t0;
    }
    else {
        if (t2 BETTER_THAN t1) {
            s4 = t2;
            hmm_history(hmm, 4) = hmm_history(hmm, 2);
            ssid[4] = ssid[2];
        }
        else {
            s4 = t1;
            hmm_history(hmm, 4) = hmm_history(hmm, 3);
            ssid[4] = ssid[3];
        }
    }
    if (s4 WORSE_THAN WORST_SCORE) s4 = WORST_SCORE;
    if (s4 BETTER_THAN bestScore)
        bestScore = s4;
    hmm_score(hmm, 4) = s4;

    /* Don't propagate WORST_SCORE */
    if (ssid[1] == BAD_SSID)
        s1 = t2 = WORST_SCORE;
    else {
        s1 = hmm_score(hmm, 1) + mpx_senscr(1);
        t2 = s1 + hmm_tprob_5st(1, 3);
    }
    t0 = t1 = WORST_SCORE;
    if (s3 != WORST_SCORE)
        t0 = s3 + hmm_tprob_5st(3, 3);
    if (s2 != WORST_SCORE)
        t1 = s2 + hmm_tprob_5st(2, 3);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s3 = t2;
            hmm_history(hmm, 3) = hmm_history(hmm, 1);
            ssid[3] = ssid[1];
        }
        else
            s3 = t0;
    }
    else {
        if (t2 BETTER_THAN t1) {
            s3 = t2;
            hmm_history(hmm, 3) = hmm_history(hmm, 1);
            ssid[3] = ssid[1];
        }
        else {
            s3 = t1;
            hmm_history(hmm, 3) = hmm_history(hmm, 2);
            ssid[3] = ssid[2];
        }
    }
    if (s3 WORSE_THAN WORST_SCORE) s3 = WORST_SCORE;
    if (s3 BETTER_THAN bestScore) bestScore = s3;
    hmm_score(hmm, 3) = s3;

    /* State 0 is always active */
    s0 = hmm_in_score(hmm) + mpx_senscr(0);

    /* Don't propagate WORST_SCORE */
    t0 = t1 = WORST_SCORE;
    if (s2 != WORST_SCORE)
        t0 = s2 + hmm_tprob_5st(2, 2);
    if (s1 != WORST_SCORE)
        t1 = s1 + hmm_tprob_5st(1, 2);
    t2 = s0 + hmm_tprob_5st(0, 2);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s2 = t2;
            hmm_history(hmm, 2) = hmm_in_history(hmm);
            ssid[2] = ssid[0];
        }
        else
            s2 = t0;
    }
    else {
        if (t2 BETTER_THAN t1) {
            s2 = t2;
            hmm_history(hmm, 2) = hmm_in_history(hmm);
            ssid[2] = ssid[0];
        }
        else {
            s2 = t1;
            hmm_history(hmm, 2) = hmm_history(hmm, 1);
            ssid[2] = ssid[1];
        }
    }
    if (s2 WORSE_THAN WORST_SCORE) s2 = WORST_SCORE;
    if (s2 BETTER_THAN bestScore) bestScore = s2;
    hmm_score(hmm, 2) = s2;

    /* Don't propagate WORST_SCORE */
    t0 = WORST_SCORE;
    if (s1 != WORST_SCORE)
        t0 = s1 + hmm_tprob_5st(1, 1);
    t1 = s0 + hmm_tprob_5st(0, 1);
    if (t0 BETTER_THAN t1) {
        s1 = t0;
    }
    else {
        s1 = t1;
        hmm_history(hmm, 1) = hmm_in_history(hmm);
        ssid[1] = ssid[0];
    }
    if (s1 WORSE_THAN WORST_SCORE) s1 = WORST_SCORE;
    if (s1 BETTER_THAN bestScore) bestScore = s1;
    hmm_score(hmm, 1) = s1;

    s0 += hmm_tprob_5st(0, 0);
    if (s0 WORSE_THAN WORST_SCORE) s0 = WORST_SCORE;
    if (s0 BETTER_THAN bestScore) bestScore = s0;
    hmm_in_score(hmm) = s0;

    hmm_bestscore(hmm) = bestScore;
    return bestScore;
}

#define hmm_tprob_3st(i, j) (-tp[(i)*4+(j)])

static int32
hmm_vit_eval_3st_lr(hmm_t * hmm)
{
    int16 const *senscore = hmm->ctx->senscore;
    uint8 const *tp = hmm->ctx->tp[hmm->tmatid][0];
    uint16 const *sseq = hmm->senid;
    int32 s3, s2, s1, s0, t2, t1, t0, bestScore;

    s2 = hmm_score(hmm, 2) + nonmpx_senscr(2);
    s1 = hmm_score(hmm, 1) + nonmpx_senscr(1);
    s0 = hmm_in_score(hmm) + nonmpx_senscr(0);

    /* It was the best of scores, it was the worst of scores. */
    bestScore = WORST_SCORE;
    t2 = INT_MIN; /* Not used unless skipstate is true */

    /* Transitions into non-emitting state 3 */
    if (s1 BETTER_THAN WORST_SCORE) {
        t1 = s2 + hmm_tprob_3st(2, 3);
        if (hmm_tprob_3st(1,3) BETTER_THAN TMAT_WORST_SCORE)
            t2 = s1 + hmm_tprob_3st(1, 3);
        if (t1 BETTER_THAN t2) {
            s3 = t1;
            hmm_out_history(hmm)  = hmm_history(hmm, 2);
        } else {
            s3 = t2;
            hmm_out_history(hmm)  = hmm_history(hmm, 1);
        }
        if (s3 WORSE_THAN WORST_SCORE) s3 = WORST_SCORE;
        hmm_out_score(hmm) = s3;
        bestScore = s3;
    }

    /* All transitions into state 2 (state 0 is always active) */
    t0 = s2 + hmm_tprob_3st(2, 2);
    t1 = s1 + hmm_tprob_3st(1, 2);
    if (hmm_tprob_3st(0, 2) BETTER_THAN TMAT_WORST_SCORE)
        t2 = s0 + hmm_tprob_3st(0, 2);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s2 = t2;
            hmm_history(hmm, 2)  = hmm_in_history(hmm);
        } else
            s2 = t0;
    } else {
        if (t2 BETTER_THAN t1) {
            s2 = t2;
            hmm_history(hmm, 2)  = hmm_in_history(hmm);
        } else {
            s2 = t1;
            hmm_history(hmm, 2)  = hmm_history(hmm, 1);
        }
    }
    if (s2 WORSE_THAN WORST_SCORE) s2 = WORST_SCORE;
    if (s2 BETTER_THAN bestScore) bestScore = s2;
    hmm_score(hmm, 2) = s2;

    /* All transitions into state 1 */
    t0 = s1 + hmm_tprob_3st(1, 1);
    t1 = s0 + hmm_tprob_3st(0, 1);
    if (t0 BETTER_THAN t1) {
        s1 = t0;
    } else {
        s1 = t1;
        hmm_history(hmm, 1)  = hmm_in_history(hmm);
    }
    if (s1 WORSE_THAN WORST_SCORE) s1 = WORST_SCORE;
    if (s1 BETTER_THAN bestScore) bestScore = s1;
    hmm_score(hmm, 1) = s1;

    /* All transitions into state 0 */
    s0 = s0 + hmm_tprob_3st(0, 0);
    if (s0 WORSE_THAN WORST_SCORE) s0 = WORST_SCORE;
    if (s0 BETTER_THAN bestScore) bestScore = s0;
    hmm_in_score(hmm) = s0;

    hmm_bestscore(hmm) = bestScore;
    return bestScore;
}

static int32
hmm_vit_eval_3st_lr_mpx(hmm_t * hmm)
{
    uint8 const *tp = hmm->ctx->tp[hmm->tmatid][0];
    int16 const *senscore = hmm->ctx->senscore;
    uint16 * const *sseq = hmm->ctx->sseq;
    uint16 *ssid = hmm->senid;
    int32 bestScore;
    int32 s3, s2, s1, s0, t2, t1, t0;

    /* Don't propagate WORST_SCORE */
    t2 = INT_MIN; /* Not used unless skipstate is true */
    if (ssid[2] == BAD_SSID)
        s2 = t1 = WORST_SCORE;
    else {
        s2 = hmm_score(hmm, 2) + mpx_senscr(2);
        t1 = s2 + hmm_tprob_3st(2, 3);
    }
    if (ssid[1] == BAD_SSID)
        s1 = t2 = WORST_SCORE;
    else {
        s1 = hmm_score(hmm, 1) + mpx_senscr(1);
        if (hmm_tprob_3st(1,3) BETTER_THAN TMAT_WORST_SCORE)
            t2 = s1 + hmm_tprob_3st(1, 3);
    }
    if (t1 BETTER_THAN t2) {
        s3 = t1;
        hmm_out_history(hmm) = hmm_history(hmm, 2);
    }
    else {
        s3 = t2;
        hmm_out_history(hmm) = hmm_history(hmm, 1);
    }
    if (s3 WORSE_THAN WORST_SCORE) s3 = WORST_SCORE;
    hmm_out_score(hmm) = s3;
    bestScore = s3;

    /* State 0 is always active */
    s0 = hmm_in_score(hmm) + mpx_senscr(0);

    /* Don't propagate WORST_SCORE */
    t0 = t1 = WORST_SCORE;
    if (s2 != WORST_SCORE)
        t0 = s2 + hmm_tprob_3st(2, 2);
    if (s1 != WORST_SCORE)
        t1 = s1 + hmm_tprob_3st(1, 2);
    if (hmm_tprob_3st(0,2) BETTER_THAN TMAT_WORST_SCORE)
        t2 = s0 + hmm_tprob_3st(0, 2);
    if (t0 BETTER_THAN t1) {
        if (t2 BETTER_THAN t0) {
            s2 = t2;
            hmm_history(hmm, 2) = hmm_in_history(hmm);
            ssid[2] = ssid[0];
        }
        else
            s2 = t0;
    }
    else {
        if (t2 BETTER_THAN t1) {
            s2 = t2;
            hmm_history(hmm, 2) = hmm_in_history(hmm);
            ssid[2] = ssid[0];
        }
        else {
            s2 = t1;
            hmm_history(hmm, 2) = hmm_history(hmm, 1);
            ssid[2] = ssid[1];
        }
    }
    if (s2 WORSE_THAN WORST_SCORE) s2 = WORST_SCORE;
    if (s2 BETTER_THAN bestScore) bestScore = s2;
    hmm_score(hmm, 2) = s2;

    /* Don't propagate WORST_SCORE */
    t0 = WORST_SCORE;
    if (s1 != WORST_SCORE)
        t0 = s1 + hmm_tprob_3st(1, 1);
    t1 = s0 + hmm_tprob_3st(0, 1);
    if (t0 BETTER_THAN t1) {
        s1 = t0;
    }
    else {
        s1 = t1;
        hmm_history(hmm, 1) = hmm_in_history(hmm);
        ssid[1] = ssid[0];
    }
    if (s1 WORSE_THAN WORST_SCORE) s1 = WORST_SCORE;
    if (s1 BETTER_THAN bestScore) bestScore = s1;
    hmm_score(hmm, 1) = s1;

    /* State 0 is always active */
    s0 += hmm_tprob_3st(0, 0);
    if (s0 WORSE_THAN WORST_SCORE) s0 = WORST_SCORE;
    if (s0 BETTER_THAN bestScore) bestScore = s0;
    hmm_in_score(hmm) = s0;

    hmm_bestscore(hmm) = bestScore;
    return bestScore;
}

static int32
hmm_vit_eval_anytopo(hmm_t * hmm)
{
    hmm_context_t *ctx = hmm->ctx;
    int32 to, from, bestfrom;
    int32 newscr, scr, bestscr;
    int final_state;

    /* Compute previous state-score + observation output prob for each emitting state */
    ctx->st_sen_scr[0] = hmm_in_score(hmm) + hmm_senscr(hmm, 0);
    for (from = 1; from < hmm_n_emit_state(hmm); ++from) {
        if ((ctx->st_sen_scr[from] =
             hmm_score(hmm, from) + hmm_senscr(hmm, from)) WORSE_THAN WORST_SCORE)
            ctx->st_sen_scr[from] = WORST_SCORE;
    }

    /* FIXME/TODO: Use the BLAS for all this. */
    /* Evaluate final-state first, which does not have a self-transition */
    final_state = hmm_n_emit_state(hmm);
    to = final_state;
    scr = WORST_SCORE;
    bestfrom = -1;
    for (from = to - 1; from >= 0; --from) {
        if ((hmm_tprob(hmm, from, to) BETTER_THAN TMAT_WORST_SCORE) &&
            ((newscr = ctx->st_sen_scr[from]
              + hmm_tprob(hmm, from, to)) BETTER_THAN scr)) {
            scr = newscr;
            bestfrom = from;
        }
    }
    hmm_out_score(hmm) = scr;
    if (bestfrom >= 0)
        hmm_out_history(hmm) = hmm_history(hmm, bestfrom);
    bestscr = scr;

    /* Evaluate all other states, which might have self-transitions */
    for (to = final_state - 1; to >= 0; --to) {
        /* Score from self-transition, if any */
        scr =
            (hmm_tprob(hmm, to, to) BETTER_THAN TMAT_WORST_SCORE)
            ? ctx->st_sen_scr[to] + hmm_tprob(hmm, to, to)
            : WORST_SCORE;

        /* Scores from transitions from other states */
        bestfrom = -1;
        for (from = to - 1; from >= 0; --from) {
            if ((hmm_tprob(hmm, from, to) BETTER_THAN TMAT_WORST_SCORE) &&
                ((newscr = ctx->st_sen_scr[from]
                  + hmm_tprob(hmm, from, to)) BETTER_THAN scr)) {
                scr = newscr;
                bestfrom = from;
            }
        }

        /* Update new result for state to */
        if (to == 0) {
            hmm_in_score(hmm) = scr;
            if (bestfrom >= 0)
                hmm_in_history(hmm) = hmm_history(hmm, bestfrom);
        }
        else {
            hmm_score(hmm, to) = scr;
            if (bestfrom >= 0)
                hmm_history(hmm, to) = hmm_history(hmm, bestfrom);
        }
        /* Propagate ssid for multiplex HMMs */
        if (bestfrom >= 0 && hmm_is_mpx(hmm))
            hmm->senid[to] = hmm->senid[bestfrom];

        if (bestscr WORSE_THAN scr)
            bestscr = scr;
    }

    hmm_bestscore(hmm) = bestscr;
    return bestscr;
}

int32
hmm_vit_eval(hmm_t * hmm)
{
    if (hmm_is_mpx(hmm)) {
        if (hmm_n_emit_state(hmm) == 5)
            return hmm_vit_eval_5st_lr_mpx(hmm);
        else if (hmm_n_emit_state(hmm) == 3)
            return hmm_vit_eval_3st_lr_mpx(hmm);
        else
            return hmm_vit_eval_anytopo(hmm);
    }
    else {
        if (hmm_n_emit_state(hmm) == 5)
            return hmm_vit_eval_5st_lr(hmm);
        else if (hmm_n_emit_state(hmm) == 3)
            return hmm_vit_eval_3st_lr(hmm);
        else
            return hmm_vit_eval_anytopo(hmm);
    }
}

int32
hmm_dump_vit_eval(hmm_t * hmm, FILE * fp)
{
    int32 bs = 0;

    if (fp) {
        fprintf(fp, "BEFORE:\n");
        hmm_dump(hmm, fp);
    }
    bs = hmm_vit_eval(hmm);
    if (fp) {
        fprintf(fp, "AFTER:\n");
        hmm_dump(hmm, fp);
    }

    return bs;
}
