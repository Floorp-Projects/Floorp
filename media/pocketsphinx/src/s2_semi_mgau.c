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

/* System headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#if defined(__ADSPBLACKFIN__)
#elif !defined(_WIN32_WCE)
#include <sys/types.h>
#endif

/* SphinxBase headers */
#include <sphinx_config.h>
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/fixpoint.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/bio.h>
#include <sphinxbase/err.h>
#include <sphinxbase/prim_type.h>

/* Local headers */
#include "s2_semi_mgau.h"
#include "tied_mgau_common.h"

static ps_mgaufuncs_t s2_semi_mgau_funcs = {
    "s2_semi",
    s2_semi_mgau_frame_eval,      /* frame_eval */
    s2_semi_mgau_mllr_transform,  /* transform */
    s2_semi_mgau_free             /* free */
};

struct vqFeature_s {
    int32 score; /* score or distance */
    int32 codeword; /* codeword (vector index) */
};

static void
eval_topn(s2_semi_mgau_t *s, int32 feat, mfcc_t *z)
{
    int i, ceplen;
    vqFeature_t *topn;

    topn = s->f[feat];
    ceplen = s->g->featlen[feat];

    for (i = 0; i < s->max_topn; i++) {
        mfcc_t *mean, diff, sqdiff, compl; /* diff, diff^2, component likelihood */
        vqFeature_t vtmp;
        mfcc_t *var, d;
        mfcc_t *obs;
        int32 cw, j;

        cw = topn[i].codeword;
        mean = s->g->mean[0][feat][0] + cw * ceplen;
        var = s->g->var[0][feat][0] + cw * ceplen;
        d = s->g->det[0][feat][cw];
        obs = z;
        for (j = 0; j < ceplen; j++) {
            diff = *obs++ - *mean++;
            sqdiff = MFCCMUL(diff, diff);
            compl = MFCCMUL(sqdiff, *var);
            d = GMMSUB(d, compl);
            ++var;
        }
        topn[i].score = (int32)d;
        if (i == 0)
            continue;
        vtmp = topn[i];
        for (j = i - 1; j >= 0 && (int32)d > topn[j].score; j--) {
            topn[j + 1] = topn[j];
        }
        topn[j + 1] = vtmp;
    }
}

static void
eval_cb(s2_semi_mgau_t *s, int32 feat, mfcc_t *z)
{
    vqFeature_t *worst, *best, *topn;
    mfcc_t *mean;
    mfcc_t *var, *det, *detP, *detE;
    int32 i, ceplen;

    best = topn = s->f[feat];
    worst = topn + (s->max_topn - 1);
    mean = s->g->mean[0][feat][0];
    var = s->g->var[0][feat][0];
    det = s->g->det[0][feat];
    detE = det + s->g->n_density;
    ceplen = s->g->featlen[feat];

    for (detP = det; detP < detE; ++detP) {
        mfcc_t diff, sqdiff, compl; /* diff, diff^2, component likelihood */
        mfcc_t d;
        mfcc_t *obs;
        vqFeature_t *cur;
        int32 cw, j;

        d = *detP;
        obs = z;
        cw = (int)(detP - det);
        for (j = 0; (j < ceplen) && (d >= worst->score); ++j) {
            diff = *obs++ - *mean++;
            sqdiff = MFCCMUL(diff, diff);
            compl = MFCCMUL(sqdiff, *var);
            d = GMMSUB(d, compl);
            ++var;
        }
        if (j < ceplen) {
            /* terminated early, so not in topn */
            mean += (ceplen - j);
            var += (ceplen - j);
            continue;
        }
        if ((int32)d < worst->score)
            continue;
        for (i = 0; i < s->max_topn; i++) {
            /* already there, so don't need to insert */
            if (topn[i].codeword == cw)
                break;
        }
        if (i < s->max_topn)
            continue;       /* already there.  Don't insert */
        /* remaining code inserts codeword and dist in correct spot */
        for (cur = worst - 1; cur >= best && (int32)d >= cur->score; --cur)
            memcpy(cur + 1, cur, sizeof(vqFeature_t));
        ++cur;
        cur->codeword = cw;
        cur->score = (int32)d;
    }
}

static void
mgau_dist(s2_semi_mgau_t * s, int32 frame, int32 feat, mfcc_t * z)
{
    eval_topn(s, feat, z);

    /* If this frame is skipped, do nothing else. */
    if (frame % s->ds_ratio)
        return;

    /* Evaluate the rest of the codebook (or subset thereof). */
    eval_cb(s, feat, z);
}

static int
mgau_norm(s2_semi_mgau_t *s, int feat)
{
    int32 norm;
    int j;

    /* Compute quantized normalizing constant. */
    norm = s->f[feat][0].score >> SENSCR_SHIFT;

    /* Normalize the scores, negate them, and clamp their dynamic range. */
    for (j = 0; j < s->max_topn; ++j) {
        s->f[feat][j].score = -((s->f[feat][j].score >> SENSCR_SHIFT) - norm);
        if (s->f[feat][j].score > MAX_NEG_ASCR)
            s->f[feat][j].score = MAX_NEG_ASCR;
        if (s->topn_beam[feat] && s->f[feat][j].score > s->topn_beam[feat])
            break;
    }
    return j;
}

static int32
get_scores_8b_feat_6(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3, *pid_cw4, *pid_cw5;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];
    pid_cw4 = s->mixw[i][s->f[i][4].codeword];
    pid_cw5 = s->mixw[i][s->f[i][5].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;

        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw1[sen] + s->f[i][1].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw2[sen] + s->f[i][2].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw3[sen] + s->f[i][3].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw4[sen] + s->f[i][4].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw5[sen] + s->f[i][5].score);

        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_5(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3, *pid_cw4;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];
    pid_cw4 = s->mixw[i][s->f[i][4].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;

        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw1[sen] + s->f[i][1].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw2[sen] + s->f[i][2].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw3[sen] + s->f[i][3].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw4[sen] + s->f[i][4].score);

        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_4(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;

        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw1[sen] + s->f[i][1].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw2[sen] + s->f[i][2].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw3[sen] + s->f[i][3].score);

        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_3(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;

        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw1[sen] + s->f[i][1].score);
        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw2[sen] + s->f[i][2].score);

        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_2(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;

        tmp = fast_logmath_add(s->lmath_8b, tmp,
                               pid_cw1[sen] + s->f[i][1].score);

        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_1(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0;

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        int32 tmp = pid_cw0[sen] + s->f[i][0].score;
        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat_any(s2_semi_mgau_t * s, int i, int topn,
                       int16 *senone_scores, uint8 *senone_active,
                       int32 n_senone_active)
{
    int32 j, k, l;

    for (l = j = 0; j < n_senone_active; j++) {
        int sen = senone_active[j] + l;
        uint8 *pid_cw;
        int32 tmp;
        pid_cw = s->mixw[i][s->f[i][0].codeword];
        tmp = pid_cw[sen] + s->f[i][0].score;
        for (k = 1; k < topn; ++k) {
            pid_cw = s->mixw[i][s->f[i][k].codeword];
            tmp = fast_logmath_add(s->lmath_8b, tmp,
                                   pid_cw[sen] + s->f[i][k].score);
        }
        senone_scores[sen] += tmp;
        l = sen;
    }
    return 0;
}

static int32
get_scores_8b_feat(s2_semi_mgau_t * s, int i, int topn,
                   int16 *senone_scores, uint8 *senone_active, int32 n_senone_active)
{
    switch (topn) {
    case 6:
        return get_scores_8b_feat_6(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 5:
        return get_scores_8b_feat_5(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 4:
        return get_scores_8b_feat_4(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 3:
        return get_scores_8b_feat_3(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 2:
        return get_scores_8b_feat_2(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 1:
        return get_scores_8b_feat_1(s, i, senone_scores,
                                    senone_active, n_senone_active);
    default:
        return get_scores_8b_feat_any(s, i, topn, senone_scores,
                                      senone_active, n_senone_active);
    }
}

static int32
get_scores_8b_feat_all(s2_semi_mgau_t * s, int i, int topn, int16 *senone_scores)
{
    int32 j, k;

    for (j = 0; j < s->n_sen; j++) {
        uint8 *pid_cw;
        int32 tmp;
        pid_cw = s->mixw[i][s->f[i][0].codeword];
        tmp = pid_cw[j] + s->f[i][0].score;
        for (k = 1; k < topn; ++k) {
            pid_cw = s->mixw[i][s->f[i][k].codeword];
            tmp = fast_logmath_add(s->lmath_8b, tmp,
                                   pid_cw[j] + s->f[i][k].score);
        }
        senone_scores[j] += tmp;
    }
    return 0;
}

static int32
get_scores_4b_feat_6(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3, *pid_cw4, *pid_cw5;
    uint8 w_den[6][16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[0][j] = s->mixw_cb[j] + s->f[i][0].score;
        w_den[1][j] = s->mixw_cb[j] + s->f[i][1].score;
        w_den[2][j] = s->mixw_cb[j] + s->f[i][2].score;
        w_den[3][j] = s->mixw_cb[j] + s->f[i][3].score;
        w_den[4][j] = s->mixw_cb[j] + s->f[i][4].score;
        w_den[5][j] = s->mixw_cb[j] + s->f[i][5].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];
    pid_cw4 = s->mixw[i][s->f[i][4].codeword];
    pid_cw5 = s->mixw[i][s->f[i][5].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
            cw = pid_cw4[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[4][cw]);
            cw = pid_cw5[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[5][cw]);
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
            cw = pid_cw4[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[4][cw]);
            cw = pid_cw5[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[5][cw]);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_5(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3, *pid_cw4;
    uint8 w_den[5][16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[0][j] = s->mixw_cb[j] + s->f[i][0].score;
        w_den[1][j] = s->mixw_cb[j] + s->f[i][1].score;
        w_den[2][j] = s->mixw_cb[j] + s->f[i][2].score;
        w_den[3][j] = s->mixw_cb[j] + s->f[i][3].score;
        w_den[4][j] = s->mixw_cb[j] + s->f[i][4].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];
    pid_cw4 = s->mixw[i][s->f[i][4].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
            cw = pid_cw4[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[4][cw]);
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
            cw = pid_cw4[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[4][cw]);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_4(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2, *pid_cw3;
    uint8 w_den[4][16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[0][j] = s->mixw_cb[j] + s->f[i][0].score;
        w_den[1][j] = s->mixw_cb[j] + s->f[i][1].score;
        w_den[2][j] = s->mixw_cb[j] + s->f[i][2].score;
        w_den[3][j] = s->mixw_cb[j] + s->f[i][3].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];
    pid_cw3 = s->mixw[i][s->f[i][3].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
            cw = pid_cw3[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[3][cw]);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_3(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1, *pid_cw2;
    uint8 w_den[3][16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[0][j] = s->mixw_cb[j] + s->f[i][0].score;
        w_den[1][j] = s->mixw_cb[j] + s->f[i][1].score;
        w_den[2][j] = s->mixw_cb[j] + s->f[i][2].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];
    pid_cw2 = s->mixw[i][s->f[i][2].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
            cw = pid_cw2[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[2][cw]);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_2(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0, *pid_cw1;
    uint8 w_den[2][16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[0][j] = s->mixw_cb[j] + s->f[i][0].score;
        w_den[1][j] = s->mixw_cb[j] + s->f[i][1].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];
    pid_cw1 = s->mixw[i][s->f[i][1].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] >> 4;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[0][cw];
            cw = pid_cw1[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp, w_den[1][cw]);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_1(s2_semi_mgau_t * s, int i,
                     int16 *senone_scores, uint8 *senone_active,
                     int32 n_senone_active)
{
    int32 j, l;
    uint8 *pid_cw0;
    uint8 w_den[16];

    /* Precompute scaled densities. */
    for (j = 0; j < 16; ++j) {
        w_den[j] = s->mixw_cb[j] + s->f[i][0].score;
    }

    pid_cw0 = s->mixw[i][s->f[i][0].codeword];

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;

        if (n & 1) {
            cw = pid_cw0[n/2] >> 4;
            tmp = w_den[cw];
        }
        else {
            cw = pid_cw0[n/2] & 0x0f;
            tmp = w_den[cw];
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat_any(s2_semi_mgau_t * s, int i, int topn,
                       int16 *senone_scores, uint8 *senone_active,
                       int32 n_senone_active)
{
    int32 j, k, l;

    for (l = j = 0; j < n_senone_active; j++) {
        int n = senone_active[j] + l;
        int tmp, cw;
        uint8 *pid_cw;
    
        pid_cw = s->mixw[i][s->f[i][0].codeword];
        if (n & 1)
            cw = pid_cw[n/2] >> 4;
        else
            cw = pid_cw[n/2] & 0x0f;
        tmp = s->mixw_cb[cw] + s->f[i][0].score;
        for (k = 1; k < topn; ++k) {
            pid_cw = s->mixw[i][s->f[i][k].codeword];
            if (n & 1)
                cw = pid_cw[n/2] >> 4;
            else
                cw = pid_cw[n/2] & 0x0f;
            tmp = fast_logmath_add(s->lmath_8b, tmp,
                                   s->mixw_cb[cw] + s->f[i][k].score);
        }
        senone_scores[n] += tmp;
        l = n;
    }
    return 0;
}

static int32
get_scores_4b_feat(s2_semi_mgau_t * s, int i, int topn,
                   int16 *senone_scores, uint8 *senone_active, int32 n_senone_active)
{
    switch (topn) {
    case 6:
        return get_scores_4b_feat_6(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 5:
        return get_scores_4b_feat_5(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 4:
        return get_scores_4b_feat_4(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 3:
        return get_scores_4b_feat_3(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 2:
        return get_scores_4b_feat_2(s, i, senone_scores,
                                    senone_active, n_senone_active);
    case 1:
        return get_scores_4b_feat_1(s, i, senone_scores,
                                    senone_active, n_senone_active);
    default:
        return get_scores_4b_feat_any(s, i, topn, senone_scores,
                                      senone_active, n_senone_active);
    }
}

static int32
get_scores_4b_feat_all(s2_semi_mgau_t * s, int i, int topn, int16 *senone_scores)
{
    int j, last_sen;

    j = 0;
    /* Number of senones is always even, but don't overrun if it isn't. */
    last_sen = s->n_sen & ~1;
    while (j < last_sen) {
        uint8 *pid_cw;
        int32 tmp0, tmp1;
        int k;

        pid_cw = s->mixw[i][s->f[i][0].codeword];
        tmp0 = s->mixw_cb[pid_cw[j/2] & 0x0f] + s->f[i][0].score;
        tmp1 = s->mixw_cb[pid_cw[j/2] >> 4] + s->f[i][0].score;
        for (k = 1; k < topn; ++k) {
            int32 w_den0, w_den1;

            pid_cw = s->mixw[i][s->f[i][k].codeword];
            w_den0 = s->mixw_cb[pid_cw[j/2] & 0x0f] + s->f[i][k].score;
            w_den1 = s->mixw_cb[pid_cw[j/2] >> 4] + s->f[i][k].score;
            tmp0 = fast_logmath_add(s->lmath_8b, tmp0, w_den0);
            tmp1 = fast_logmath_add(s->lmath_8b, tmp1, w_den1);
        }
        senone_scores[j++] += tmp0;
        senone_scores[j++] += tmp1;
    }
    return 0;
}

/*
 * Compute senone scores for the active senones.
 */
int32
s2_semi_mgau_frame_eval(ps_mgau_t *ps,
                        int16 *senone_scores,
                        uint8 *senone_active,
                        int32 n_senone_active,
			mfcc_t ** featbuf, int32 frame,
			int32 compallsen)
{
    s2_semi_mgau_t *s = (s2_semi_mgau_t *)ps;
    int i, topn_idx;
    int n_feat = s->g->n_feat;

    memset(senone_scores, 0, s->n_sen * sizeof(*senone_scores));
    /* No bounds checking is done here, which just means you'll get
     * semi-random crap if you request a frame in the future or one
     * that's too far in the past. */
    topn_idx = frame % s->n_topn_hist;
    s->f = s->topn_hist[topn_idx];
    for (i = 0; i < n_feat; ++i) {
        /* For past frames this will already be computed. */
        if (frame >= ps_mgau_base(ps)->frame_idx) {
            vqFeature_t **lastf;
            if (topn_idx == 0)
                lastf = s->topn_hist[s->n_topn_hist-1];
            else
                lastf = s->topn_hist[topn_idx-1];
            memcpy(s->f[i], lastf[i], sizeof(vqFeature_t) * s->max_topn);
            mgau_dist(s, frame, i, featbuf[i]);
            s->topn_hist_n[topn_idx][i] = mgau_norm(s, i);
        }
        if (s->mixw_cb) {
            if (compallsen)
                get_scores_4b_feat_all(s, i, s->topn_hist_n[topn_idx][i], senone_scores);
            else
                get_scores_4b_feat(s, i, s->topn_hist_n[topn_idx][i], senone_scores,
                                   senone_active, n_senone_active);
        }
        else {
            if (compallsen)
                get_scores_8b_feat_all(s, i, s->topn_hist_n[topn_idx][i], senone_scores);
            else
                get_scores_8b_feat(s, i, s->topn_hist_n[topn_idx][i], senone_scores,
                                   senone_active, n_senone_active);
        }
    }

    return 0;
}

static int32
read_sendump(s2_semi_mgau_t *s, bin_mdef_t *mdef, char const *file)
{
    FILE *fp;
    char line[1000];
    int32 i, n, r, c;
    int32 do_swap, do_mmap;
    size_t offset;
    int n_clust = 0;
    int n_feat = s->g->n_feat;
    int n_density = s->g->n_density;
    int n_sen = bin_mdef_n_sen(mdef);
    int n_bits = 8;

    s->n_sen = n_sen; /* FIXME: Should have been done earlier */
    do_mmap = cmd_ln_boolean_r(s->config, "-mmap");

    if ((fp = fopen(file, "rb")) == NULL)
        return -1;

    E_INFO("Loading senones from dump file %s\n", file);
    /* Read title size, title */
    if (fread(&n, sizeof(int32), 1, fp) != 1) {
        E_ERROR_SYSTEM("Failed to read title size from %s", file);
        goto error_out;
    }
    /* This is extremely bogus */
    do_swap = 0;
    if (n < 1 || n > 999) {
        SWAP_INT32(&n);
        if (n < 1 || n > 999) {
            E_ERROR("Title length %x in dump file %s out of range\n", n, file);
            goto error_out;
        }
        do_swap = 1;
    }
    if (fread(line, sizeof(char), n, fp) != n) {
        E_ERROR_SYSTEM("Cannot read title");
        goto error_out;
    }
    if (line[n - 1] != '\0') {
        E_ERROR("Bad title in dump file\n");
        goto error_out;
    }
    E_INFO("%s\n", line);

    /* Read header size, header */
    if (fread(&n, sizeof(n), 1, fp) != 1) {
        E_ERROR_SYSTEM("Failed to read header size from %s", file);
        goto error_out;
    }
    if (do_swap) SWAP_INT32(&n);
    if (fread(line, sizeof(char), n, fp) != n) {
        E_ERROR_SYSTEM("Cannot read header");
        goto error_out;
    }
    if (line[n - 1] != '\0') {
        E_ERROR("Bad header in dump file\n");
        goto error_out;
    }

    /* Read other header strings until string length = 0 */
    for (;;) {
        if (fread(&n, sizeof(n), 1, fp) != 1) {
            E_ERROR_SYSTEM("Failed to read header string size from %s", file);
            goto error_out;
        }
        if (do_swap) SWAP_INT32(&n);
        if (n == 0)
            break;
        if (fread(line, sizeof(char), n, fp) != n) {
            E_ERROR_SYSTEM("Cannot read header");
            goto error_out;
        }
        /* Look for a cluster count, if present */
        if (!strncmp(line, "feature_count ", strlen("feature_count "))) {
            n_feat = atoi(line + strlen("feature_count "));
        }
        if (!strncmp(line, "mixture_count ", strlen("mixture_count "))) {
            n_density = atoi(line + strlen("mixture_count "));
        }
        if (!strncmp(line, "model_count ", strlen("model_count "))) {
            n_sen = atoi(line + strlen("model_count "));
        }
        if (!strncmp(line, "cluster_count ", strlen("cluster_count "))) {
            n_clust = atoi(line + strlen("cluster_count "));
        }
        if (!strncmp(line, "cluster_bits ", strlen("cluster_bits "))) {
            n_bits = atoi(line + strlen("cluster_bits "));
        }
    }

    /* Defaults for #rows, #columns in mixw array. */
    c = n_sen;
    r = n_density;
    if (n_clust == 0) {
        /* Older mixw files have them here, and they might be padded. */
        if (fread(&r, sizeof(r), 1, fp) != 1) {
            E_ERROR_SYSTEM("Cannot read #rows");
            goto error_out;
        }
        if (do_swap) SWAP_INT32(&r);
        if (fread(&c, sizeof(c), 1, fp) != 1) {
            E_ERROR_SYSTEM("Cannot read #columns");
            goto error_out;
        }
        if (do_swap) SWAP_INT32(&c);
        E_INFO("Rows: %d, Columns: %d\n", r, c);
    }

    if (n_feat != s->g->n_feat) {
        E_ERROR("Number of feature streams mismatch: %d != %d\n",
                n_feat, s->g->n_feat);
        goto error_out;
    }
    if (n_density != s->g->n_density) {
        E_ERROR("Number of densities mismatch: %d != %d\n",
                n_density, s->g->n_density);
        goto error_out;
    }
    if (n_sen != s->n_sen) {
        E_ERROR("Number of senones mismatch: %d != %d\n",
                n_sen, s->n_sen);
        goto error_out;
    }

    if (!((n_clust == 0) || (n_clust == 15) || (n_clust == 16))) {
        E_ERROR("Cluster count must be 0, 15, or 16\n");
        goto error_out;
    }
    if (n_clust == 15)
        ++n_clust;

    if (!((n_bits == 8) || (n_bits == 4))) {
        E_ERROR("Cluster count must be 4 or 8\n");
        goto error_out;
    }

    if (do_mmap) {
            E_INFO("Using memory-mapped I/O for senones\n");
    }
    offset = ftell(fp);

    /* Allocate memory for pdfs (or memory map them) */
    if (do_mmap) {
        s->sendump_mmap = mmio_file_read(file);
        /* Get cluster codebook if any. */
        if (n_clust) {
            s->mixw_cb = ((uint8 *) mmio_file_ptr(s->sendump_mmap)) + offset;
            offset += n_clust;
        }
    }
    else {
        /* Get cluster codebook if any. */
        if (n_clust) {
            s->mixw_cb = ckd_calloc(1, n_clust);
            if (fread(s->mixw_cb, 1, n_clust, fp) != (size_t) n_clust) {
                E_ERROR("Failed to read %d bytes from sendump\n", n_clust);
                goto error_out;
            }
        }
    }

    /* Set up pointers, or read, or whatever */
    if (s->sendump_mmap) {
        s->mixw = ckd_calloc_2d(n_feat, n_density, sizeof(*s->mixw));
        for (n = 0; n < n_feat; n++) {
            int step = c;
            if (n_bits == 4)
                step = (step + 1) / 2;
            for (i = 0; i < r; i++) {
                s->mixw[n][i] = ((uint8 *) mmio_file_ptr(s->sendump_mmap)) + offset;
                offset += step;
            }
        }
    }
    else {
        s->mixw = ckd_calloc_3d(n_feat, n_density, n_sen, sizeof(***s->mixw));
        /* Read pdf values and ids */
        for (n = 0; n < n_feat; n++) {
            int step = c;
            if (n_bits == 4)
                step = (step + 1) / 2;
            for (i = 0; i < r; i++) {
                if (fread(s->mixw[n][i], sizeof(***s->mixw), step, fp)
                    != (size_t) step) {
                    E_ERROR("Failed to read %d bytes from sendump\n", step);
                    goto error_out;
                }
            }
        }
    }

    fclose(fp);
    return 0;
error_out:
    fclose(fp);
    return -1;
}

static int32
read_mixw(s2_semi_mgau_t * s, char const *file_name, double SmoothMin)
{
    char **argname, **argval;
    char eofchk;
    FILE *fp;
    int32 byteswap, chksum_present;
    uint32 chksum;
    float32 *pdf;
    int32 i, f, c, n;
    int32 n_sen;
    int32 n_feat;
    int32 n_comp;
    int32 n_err;

    E_INFO("Reading mixture weights file '%s'\n", file_name);

    if ((fp = fopen(file_name, "rb")) == NULL)
        E_FATAL_SYSTEM("Failed to open mixture weights file '%s' for reading", file_name);

    /* Read header, including argument-value info and 32-bit byteorder magic */
    if (bio_readhdr(fp, &argname, &argval, &byteswap) < 0)
        E_FATAL("Failed to read header from file '%s'\n", file_name);

    /* Parse argument-value list */
    chksum_present = 0;
    for (i = 0; argname[i]; i++) {
        if (strcmp(argname[i], "version") == 0) {
            if (strcmp(argval[i], MGAU_MIXW_VERSION) != 0)
                E_WARN("Version mismatch(%s): %s, expecting %s\n",
                       file_name, argval[i], MGAU_MIXW_VERSION);
        }
        else if (strcmp(argname[i], "chksum0") == 0) {
            chksum_present = 1; /* Ignore the associated value */
        }
    }
    bio_hdrarg_free(argname, argval);
    argname = argval = NULL;

    chksum = 0;

    /* Read #senones, #features, #codewords, arraysize */
    if ((bio_fread(&n_sen, sizeof(int32), 1, fp, byteswap, &chksum) != 1)
        || (bio_fread(&n_feat, sizeof(int32), 1, fp, byteswap, &chksum) !=
            1)
        || (bio_fread(&n_comp, sizeof(int32), 1, fp, byteswap, &chksum) !=
            1)
        || (bio_fread(&n, sizeof(int32), 1, fp, byteswap, &chksum) != 1)) {
        E_FATAL("bio_fread(%s) (arraysize) failed\n", file_name);
    }
    if (n_feat != s->g->n_feat)
        E_FATAL("#Features streams(%d) != %d\n", n_feat, s->g->n_feat);
    if (n != n_sen * n_feat * n_comp) {
        E_FATAL
            ("%s: #float32s(%d) doesn't match header dimensions: %d x %d x %d\n",
             file_name, i, n_sen, n_feat, n_comp);
    }

    /* n_sen = number of mixture weights per codeword, which is
     * fixed at the number of senones since we have only one codebook.
     */
    s->n_sen = n_sen;

    /* Quantized mixture weight arrays. */
    s->mixw = ckd_calloc_3d(n_feat, s->g->n_density, n_sen, sizeof(***s->mixw));

    /* Temporary structure to read in floats before conversion to (int32) logs3 */
    pdf = (float32 *) ckd_calloc(n_comp, sizeof(float32));

    /* Read senone probs data, normalize, floor, convert to logs3, truncate to 8 bits */
    n_err = 0;
    for (i = 0; i < n_sen; i++) {
        for (f = 0; f < n_feat; f++) {
            if (bio_fread((void *) pdf, sizeof(float32),
                          n_comp, fp, byteswap, &chksum) != n_comp) {
                E_FATAL("bio_fread(%s) (arraydata) failed\n", file_name);
            }

            /* Normalize and floor */
            if (vector_sum_norm(pdf, n_comp) <= 0.0)
                n_err++;
            vector_floor(pdf, n_comp, SmoothMin);
            vector_sum_norm(pdf, n_comp);

            /* Convert to LOG, quantize, and transpose */
            for (c = 0; c < n_comp; c++) {
                int32 qscr;

                qscr = -logmath_log(s->lmath_8b, pdf[c]);
                if ((qscr > MAX_NEG_MIXW) || (qscr < 0))
                    qscr = MAX_NEG_MIXW;
                s->mixw[f][c][i] = qscr;
            }
        }
    }
    if (n_err > 0)
        E_WARN("Weight normalization failed for %d mixture weights components\n", n_err);

    ckd_free(pdf);

    if (chksum_present)
        bio_verify_chksum(fp, byteswap, chksum);

    if (fread(&eofchk, 1, 1, fp) == 1)
        E_FATAL("More data than expected in %s\n", file_name);

    fclose(fp);

    E_INFO("Read %d x %d x %d mixture weights\n", n_sen, n_feat, n_comp);
    return n_sen;
}


static int
split_topn(char const *str, uint8 *out, int nfeat)
{
    char *topn_list = ckd_salloc(str);
    char *c, *cc;
    int i, maxn;

    c = topn_list;
    i = 0;
    maxn = 0;
    while (i < nfeat && (cc = strchr(c, ',')) != NULL) {
        *cc = '\0';
        out[i] = atoi(c);
        if (out[i] > maxn) maxn = out[i];
        c = cc + 1;
        ++i;
    }
    if (i < nfeat && *c != '\0') {
        out[i] = atoi(c);
        if (out[i] > maxn) maxn = out[i];
        ++i;
    }
    while (i < nfeat)
        out[i++] = maxn;

    ckd_free(topn_list);
    return maxn;
}


ps_mgau_t *
s2_semi_mgau_init(acmod_t *acmod)
{
    s2_semi_mgau_t *s;
    ps_mgau_t *ps;
    char const *sendump_path;
    int i;
    int n_feat;

    s = ckd_calloc(1, sizeof(*s));
    s->config = acmod->config;

    s->lmath = logmath_retain(acmod->lmath);
    /* Log-add table. */
    s->lmath_8b = logmath_init(logmath_get_base(acmod->lmath), SENSCR_SHIFT, TRUE);
    if (s->lmath_8b == NULL)
        goto error_out;
    /* Ensure that it is only 8 bits wide so that fast_logmath_add() works. */
    if (logmath_get_width(s->lmath_8b) != 1) {
        E_ERROR("Log base %f is too small to represent add table in 8 bits\n",
                logmath_get_base(s->lmath_8b));
        goto error_out;
    }

    /* Read means and variances. */
    if ((s->g = gauden_init(cmd_ln_str_r(s->config, "-mean"),
                            cmd_ln_str_r(s->config, "-var"),
                            cmd_ln_float32_r(s->config, "-varfloor"),
                            s->lmath)) == NULL)
        goto error_out;
    /* Currently only a single codebook is supported. */
    if (s->g->n_mgau != 1)
        goto error_out;

    n_feat = s->g->n_feat;

    /* Verify n_feat and veclen, against acmod. */
    if (n_feat != feat_dimension1(acmod->fcb)) {
        E_ERROR("Number of streams does not match: %d != %d\n",
                n_feat, feat_dimension1(acmod->fcb));
        goto error_out;
    }
    for (i = 0; i < n_feat; ++i) {
        if (s->g->featlen[i] != feat_dimension2(acmod->fcb, i)) {
            E_ERROR("Dimension of stream %d does not match: %d != %d\n",
                    i, s->g->featlen[i], feat_dimension2(acmod->fcb, i));
            goto error_out;
        }
    }
    /* Read mixture weights */
    if ((sendump_path = cmd_ln_str_r(s->config, "-sendump"))) {
        if (read_sendump(s, acmod->mdef, sendump_path) < 0) {
            goto error_out;
        }
    }
    else {
        if (read_mixw(s, cmd_ln_str_r(s->config, "-mixw"),
                      cmd_ln_float32_r(s->config, "-mixwfloor")) < 0) {
            goto error_out;
        }
    }
    s->ds_ratio = cmd_ln_int32_r(s->config, "-ds");

    /* Determine top-N for each feature */
    s->topn_beam = ckd_calloc(n_feat, sizeof(*s->topn_beam));
    s->max_topn = cmd_ln_int32_r(s->config, "-topn");
    split_topn(cmd_ln_str_r(s->config, "-topn_beam"), s->topn_beam, n_feat);
    E_INFO("Maximum top-N: %d ", s->max_topn);
    E_INFOCONT("Top-N beams:");
    for (i = 0; i < n_feat; ++i) {
        E_INFOCONT(" %d", s->topn_beam[i]);
    }
    E_INFOCONT("\n");

    /* Top-N scores from recent frames */
    s->n_topn_hist = cmd_ln_int32_r(s->config, "-pl_window") + 2;
    s->topn_hist = (vqFeature_t ***)
        ckd_calloc_3d(s->n_topn_hist, n_feat, s->max_topn,
                      sizeof(***s->topn_hist));
    s->topn_hist_n = ckd_calloc_2d(s->n_topn_hist, n_feat,
                                   sizeof(**s->topn_hist_n));
    for (i = 0; i < s->n_topn_hist; ++i) {
        int j;
        for (j = 0; j < n_feat; ++j) {
            int k;
            for (k = 0; k < s->max_topn; ++k) {
                s->topn_hist[i][j][k].score = WORST_DIST;
                s->topn_hist[i][j][k].codeword = k;
            }
        }
    }

    ps = (ps_mgau_t *)s;
    ps->vt = &s2_semi_mgau_funcs;
    return ps;
error_out:
    s2_semi_mgau_free(ps_mgau_base(s));
    return NULL;
}

int
s2_semi_mgau_mllr_transform(ps_mgau_t *ps,
                            ps_mllr_t *mllr)
{
    s2_semi_mgau_t *s = (s2_semi_mgau_t *)ps;
    return gauden_mllr_transform(s->g, mllr, s->config);
}

void
s2_semi_mgau_free(ps_mgau_t *ps)
{
    s2_semi_mgau_t *s = (s2_semi_mgau_t *)ps;

    logmath_free(s->lmath);
    logmath_free(s->lmath_8b);
    if (s->sendump_mmap) {
        ckd_free_2d(s->mixw); 
        mmio_file_unmap(s->sendump_mmap);
    }
    else {
        ckd_free_3d(s->mixw);
        if (s->mixw_cb)
            ckd_free(s->mixw_cb);
    }
    gauden_free(s->g);
    ckd_free(s->topn_beam);
    ckd_free_2d(s->topn_hist_n);
    ckd_free_3d((void **)s->topn_hist);
    ckd_free(s);
}
