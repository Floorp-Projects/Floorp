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
#include "tied_mgau_common.h"
#include "ptm_mgau.h"

static ps_mgaufuncs_t ptm_mgau_funcs = {
    "ptm",
    ptm_mgau_frame_eval,      /* frame_eval */
    ptm_mgau_mllr_transform,  /* transform */
    ptm_mgau_free             /* free */
};

#define COMPUTE_GMM_MAP(_idx)                           \
    diff[_idx] = obs[_idx] - mean[_idx];                \
    sqdiff[_idx] = MFCCMUL(diff[_idx], diff[_idx]);     \
    compl[_idx] = MFCCMUL(sqdiff[_idx], var[_idx]);
#define COMPUTE_GMM_REDUCE(_idx)                \
    d = GMMSUB(d, compl[_idx]);

static void
insertion_sort_topn(ptm_topn_t *topn, int i, int32 d)
{
    ptm_topn_t vtmp;
    int j;

    topn[i].score = d;
    if (i == 0)
        return;
    vtmp = topn[i];
    for (j = i - 1; j >= 0 && d > topn[j].score; j--) {
        topn[j + 1] = topn[j];
    }
    topn[j + 1] = vtmp;
}

static int
eval_topn(ptm_mgau_t *s, int cb, int feat, mfcc_t *z)
{
    ptm_topn_t *topn;
    int i, ceplen;

    topn = s->f->topn[cb][feat];
    ceplen = s->g->featlen[feat];

    for (i = 0; i < s->max_topn; i++) {
        mfcc_t *mean, diff[4], sqdiff[4], compl[4]; /* diff, diff^2, component likelihood */
        mfcc_t *var, d;
        mfcc_t *obs;
        int32 cw, j;

        cw = topn[i].cw;
        mean = s->g->mean[cb][feat][0] + cw * ceplen;
        var = s->g->var[cb][feat][0] + cw * ceplen;
        d = s->g->det[cb][feat][cw];
        obs = z;
        for (j = 0; j < ceplen % 4; ++j) {
            diff[0] = *obs++ - *mean++;
            sqdiff[0] = MFCCMUL(diff[0], diff[0]);
            compl[0] = MFCCMUL(sqdiff[0], *var);
            d = GMMSUB(d, compl[0]);
            ++var;
        }
        /* We could vectorize this but it's unlikely to make much
         * difference as the outer loop here isn't very big. */
        for (;j < ceplen; j += 4) {
            COMPUTE_GMM_MAP(0);
            COMPUTE_GMM_MAP(1);
            COMPUTE_GMM_MAP(2);
            COMPUTE_GMM_MAP(3);
            COMPUTE_GMM_REDUCE(0);
            COMPUTE_GMM_REDUCE(1);
            COMPUTE_GMM_REDUCE(2);
            COMPUTE_GMM_REDUCE(3);
            var += 4;
            obs += 4;
            mean += 4;
        }
        insertion_sort_topn(topn, i, (int32)d);
    }

    return topn[0].score;
}

/* This looks bad, but it actually isn't.  Less than 1% of eval_cb's
 * time is spent doing this. */
static void
insertion_sort_cb(ptm_topn_t **cur, ptm_topn_t *worst, ptm_topn_t *best,
                  int cw, int32 intd)
{
    for (*cur = worst - 1; *cur >= best && intd >= (*cur)->score; --*cur)
        memcpy(*cur + 1, *cur, sizeof(**cur));
    ++*cur;
    (*cur)->cw = cw;
    (*cur)->score = intd;
}

static int
eval_cb(ptm_mgau_t *s, int cb, int feat, mfcc_t *z)
{
    ptm_topn_t *worst, *best, *topn;
    mfcc_t *mean;
    mfcc_t *var, *det, *detP, *detE;
    int32 i, ceplen;

    best = topn = s->f->topn[cb][feat];
    worst = topn + (s->max_topn - 1);
    mean = s->g->mean[cb][feat][0];
    var = s->g->var[cb][feat][0];
    det = s->g->det[cb][feat];
    detE = det + s->g->n_density;
    ceplen = s->g->featlen[feat];

    for (detP = det; detP < detE; ++detP) {
        mfcc_t diff[4], sqdiff[4], compl[4]; /* diff, diff^2, component likelihood */
        mfcc_t d, thresh;
        mfcc_t *obs;
        ptm_topn_t *cur;
        int32 cw, j;

        d = *detP;
        thresh = (mfcc_t) worst->score; /* Avoid int-to-float conversions */
        obs = z;
        cw = (int)(detP - det);

        /* Unroll the loop starting with the first dimension(s).  In
         * theory this might be a bit faster if this Gaussian gets
         * "knocked out" by C0. In practice not. */
        for (j = 0; (j < ceplen % 4) && (d >= thresh); ++j) {
            diff[0] = *obs++ - *mean++;
            sqdiff[0] = MFCCMUL(diff[0], diff[0]);
            compl[0] = MFCCMUL(sqdiff[0], *var++);
            d = GMMSUB(d, compl[0]);
        }
        /* Now do 4 dimensions at a time.  You'd think that GCC would
         * vectorize this?  Apparently not.  And it's right, because
         * that won't make this any faster, at least on x86-64. */
        for (; j < ceplen && d >= thresh; j += 4) {
            COMPUTE_GMM_MAP(0);
            COMPUTE_GMM_MAP(1);
            COMPUTE_GMM_MAP(2);
            COMPUTE_GMM_MAP(3);
            COMPUTE_GMM_REDUCE(0);
            COMPUTE_GMM_REDUCE(1);
            COMPUTE_GMM_REDUCE(2);
            COMPUTE_GMM_REDUCE(3);
            var += 4;
            obs += 4;
            mean += 4;
        }
        if (j < ceplen) {
            /* terminated early, so not in topn */
            mean += (ceplen - j);
            var += (ceplen - j);
            continue;
        }
        if (d < thresh)
            continue;
        for (i = 0; i < s->max_topn; i++) {
            /* already there, so don't need to insert */
            if (topn[i].cw == cw)
                break;
        }
        if (i < s->max_topn)
            continue;       /* already there.  Don't insert */
        insertion_sort_cb(&cur, worst, best, cw, (int32)d);
    }

    return best->score;
}

/**
 * Compute top-N densities for active codebooks (and prune)
 */
static int
ptm_mgau_codebook_eval(ptm_mgau_t *s, mfcc_t **z, int frame)
{
    int i, j;

    /* First evaluate top-N from previous frame. */
    for (i = 0; i < s->g->n_mgau; ++i)
        for (j = 0; j < s->g->n_feat; ++j)
            eval_topn(s, i, j, z[j]);

    /* If frame downsampling is in effect, possibly do nothing else. */
    if (frame % s->ds_ratio)
        return 0;

    /* Evaluate remaining codebooks. */
    for (i = 0; i < s->g->n_mgau; ++i) {
        if (bitvec_is_clear(s->f->mgau_active, i))
            continue;
        for (j = 0; j < s->g->n_feat; ++j) {
            eval_cb(s, i, j, z[j]);
        }
    }
    return 0;
}

/**
 * Normalize densities to produce "posterior probabilities",
 * i.e. things with a reasonable dynamic range, then scale and
 * clamp them to the acceptable range.  This is actually done
 * solely to ensure that we can use fast_logmath_add().  Note that
 * unless we share the same normalizer across all codebooks for
 * each feature stream we get defective scores (that's why these
 * loops are inside out - doing it per-feature should give us
 * greater precision). */
static int
ptm_mgau_codebook_norm(ptm_mgau_t *s, mfcc_t **z, int frame)
{
    int i, j;

    for (j = 0; j < s->g->n_feat; ++j) {
        int32 norm = WORST_SCORE;
        for (i = 0; i < s->g->n_mgau; ++i) {
            if (bitvec_is_clear(s->f->mgau_active, i))
                continue;
            if (norm < s->f->topn[i][j][0].score >> SENSCR_SHIFT)
                norm = s->f->topn[i][j][0].score >> SENSCR_SHIFT;
        }
        assert(norm != WORST_SCORE);
        for (i = 0; i < s->g->n_mgau; ++i) {
            int32 k;
            if (bitvec_is_clear(s->f->mgau_active, i))
                continue;
            for (k = 0; k < s->max_topn; ++k) {
                s->f->topn[i][j][k].score >>= SENSCR_SHIFT;
                s->f->topn[i][j][k].score -= norm;
                s->f->topn[i][j][k].score = -s->f->topn[i][j][k].score;
                if (s->f->topn[i][j][k].score > MAX_NEG_ASCR) 
                    s->f->topn[i][j][k].score = MAX_NEG_ASCR;
            }
        }
    }

    return 0;
}

static int
ptm_mgau_calc_cb_active(ptm_mgau_t *s, uint8 *senone_active,
                        int32 n_senone_active, int compallsen)
{
    int i, lastsen;

    if (compallsen) {
        bitvec_set_all(s->f->mgau_active, s->g->n_mgau);
        return 0;
    }
    bitvec_clear_all(s->f->mgau_active, s->g->n_mgau);
    for (lastsen = i = 0; i < n_senone_active; ++i) {
        int sen = senone_active[i] + lastsen;
        int cb = s->sen2cb[sen];
        bitvec_set(s->f->mgau_active, cb);
        lastsen = sen;
    }
    E_DEBUG(1, ("Active codebooks:"));
    for (i = 0; i < s->g->n_mgau; ++i) {
        if (bitvec_is_clear(s->f->mgau_active, i))
            continue;
        E_DEBUGCONT(1, (" %d", i));
    }
    E_DEBUGCONT(1, ("\n"));
    return 0;
}

/**
 * Compute senone scores from top-N densities for active codebooks.
 */
static int
ptm_mgau_senone_eval(ptm_mgau_t *s, int16 *senone_scores,
                     uint8 *senone_active, int32 n_senone_active,
                     int compall)
{
    int i, lastsen, bestscore;

    memset(senone_scores, 0, s->n_sen * sizeof(*senone_scores));
    /* FIXME: This is the non-cache-efficient way to do this.  We want
     * to evaluate one codeword at a time but this requires us to have
     * a reverse codebook to senone mapping, which we don't have
     * (yet), since different codebooks have different top-N
     * codewords. */
    if (compall)
        n_senone_active = s->n_sen;
    bestscore = 0x7fffffff;
    for (lastsen = i = 0; i < n_senone_active; ++i) {
        int sen, f, cb;
        int ascore;

        if (compall)
            sen = i;
        else
            sen = senone_active[i] + lastsen;
        lastsen = sen;
        cb = s->sen2cb[sen];

        if (bitvec_is_clear(s->f->mgau_active, cb)) {
            int j;
            /* Because senone_active is deltas we can't really "knock
             * out" senones from pruned codebooks, and in any case,
             * it wouldn't make any difference to the search code,
             * which doesn't expect senone_active to change. */
            for (f = 0; f < s->g->n_feat; ++f) {
                for (j = 0; j < s->max_topn; ++j) {
                    s->f->topn[cb][f][j].score = MAX_NEG_ASCR;
                }
            }
        }
        /* For each feature, log-sum codeword scores + mixw to get
         * feature density, then sum (multiply) to get ascore */
        ascore = 0;
        for (f = 0; f < s->g->n_feat; ++f) {
            ptm_topn_t *topn;
            int j, fden = 0;
            topn = s->f->topn[cb][f];
            for (j = 0; j < s->max_topn; ++j) {
                int mixw;
                /* Find mixture weight for this codeword. */
                if (s->mixw_cb) {
                    int dcw = s->mixw[f][topn[j].cw][sen/2];
                    dcw = (dcw & 1) ? dcw >> 4 : dcw & 0x0f;
                    mixw = s->mixw_cb[dcw];
                }
                else {
                    mixw = s->mixw[f][topn[j].cw][sen];
                }
                if (j == 0)
                    fden = mixw + topn[j].score;
                else
                    fden = fast_logmath_add(s->lmath_8b, fden,
                                       mixw + topn[j].score);
                E_DEBUG(3, ("fden[%d][%d] l+= %d + %d = %d\n",
                            sen, f, mixw, topn[j].score, fden));
            }
            ascore += fden;
        }
        if (ascore < bestscore) bestscore = ascore;
        senone_scores[sen] = ascore;
    }
    /* Normalize the scores again (finishing the job we started above
     * in ptm_mgau_codebook_eval...) */
    for (i = 0; i < s->n_sen; ++i) {
        senone_scores[i] -= bestscore;
    }

    return 0;
}

/**
 * Compute senone scores for the active senones.
 */
int32
ptm_mgau_frame_eval(ps_mgau_t *ps,
                    int16 *senone_scores,
                    uint8 *senone_active,
                    int32 n_senone_active,
                    mfcc_t ** featbuf, int32 frame,
                    int32 compallsen)
{
    ptm_mgau_t *s = (ptm_mgau_t *)ps;
    int fast_eval_idx;

    /* Find the appropriate frame in the rotating history buffer
     * corresponding to the requested input frame.  No bounds checking
     * is done here, which just means you'll get semi-random crap if
     * you request a frame in the future or one that's too far in the
     * past.  Since the history buffer is just used for fast match
     * that might not be fatal. */
    fast_eval_idx = frame % s->n_fast_hist;
    s->f = s->hist + fast_eval_idx;
    /* Compute the top-N codewords for every codebook, unless this
     * is a past frame, in which case we already have them (we
     * hope!) */
    if (frame >= ps_mgau_base(ps)->frame_idx) {
        ptm_fast_eval_t *lastf;
        /* Get the previous frame's top-N information (on the
         * first frame of the input this is just all WORST_DIST,
         * no harm in that) */
        if (fast_eval_idx == 0)
            lastf = s->hist + s->n_fast_hist - 1;
        else
            lastf = s->hist + fast_eval_idx - 1;
        /* Copy in initial top-N info */
        memcpy(s->f->topn[0][0], lastf->topn[0][0],
               s->g->n_mgau * s->g->n_feat * s->max_topn * sizeof(ptm_topn_t));
        /* Generate initial active codebook list (this might not be
         * necessary) */
        ptm_mgau_calc_cb_active(s, senone_active, n_senone_active, compallsen);
        /* Now evaluate top-N, prune, and evaluate remaining codebooks. */
        ptm_mgau_codebook_eval(s, featbuf, frame);
        ptm_mgau_codebook_norm(s, featbuf, frame);
    }
    /* Evaluate intersection of active senones and active codebooks. */
    ptm_mgau_senone_eval(s, senone_scores, senone_active,
                         n_senone_active, compallsen);

    return 0;
}

static int32
read_sendump(ptm_mgau_t *s, bin_mdef_t *mdef, char const *file)
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
read_mixw(ptm_mgau_t * s, char const *file_name, double SmoothMin)
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
        E_FATAL_SYSTEM("Failed to open mixture file '%s' for reading", file_name);

    /* Read header, including argument-value info and 32-bit byteorder magic */
    if (bio_readhdr(fp, &argname, &argval, &byteswap) < 0)
        E_FATAL("Failed to read header from '%s'\n", file_name);

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
    s->mixw = ckd_calloc_3d(s->g->n_feat, s->g->n_density,
                            n_sen, sizeof(***s->mixw));

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

ps_mgau_t *
ptm_mgau_init(acmod_t *acmod, bin_mdef_t *mdef)
{
    ptm_mgau_t *s;
    ps_mgau_t *ps;
    char const *sendump_path;
    int i;

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
    /* We only support 256 codebooks or less (like 640k or 2GB, this
     * should be enough for anyone) */
    if (s->g->n_mgau > 256) {
        E_INFO("Number of codebooks exceeds 256: %d\n", s->g->n_mgau);
        goto error_out;
    }
    if (s->g->n_mgau != bin_mdef_n_ciphone(mdef)) {
        E_INFO("Number of codebooks doesn't match number of ciphones, doesn't look like PTM: %d != %d\n", s->g->n_mgau, bin_mdef_n_ciphone(mdef));
        goto error_out;
    }
    /* Verify n_feat and veclen, against acmod. */
    if (s->g->n_feat != feat_dimension1(acmod->fcb)) {
        E_ERROR("Number of streams does not match: %d != %d\n",
                s->g->n_feat, feat_dimension1(acmod->fcb));
        goto error_out;
    }
    for (i = 0; i < s->g->n_feat; ++i) {
        if (s->g->featlen[i] != feat_dimension2(acmod->fcb, i)) {
            E_ERROR("Dimension of stream %d does not match: %d != %d\n",
                    s->g->featlen[i], feat_dimension2(acmod->fcb, i));
            goto error_out;
        }
    }
    /* Read mixture weights. */
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
    s->max_topn = cmd_ln_int32_r(s->config, "-topn");
    E_INFO("Maximum top-N: %d\n", s->max_topn);

    /* Assume mapping of senones to their base phones, though this
     * will become more flexible in the future. */
    s->sen2cb = ckd_calloc(s->n_sen, sizeof(*s->sen2cb));
    for (i = 0; i < s->n_sen; ++i)
        s->sen2cb[i] = bin_mdef_sen2cimap(acmod->mdef, i);

    /* Allocate fast-match history buffers.  We need enough for the
     * phoneme lookahead window, plus the current frame, plus one for
     * good measure? (FIXME: I don't remember why) */
    s->n_fast_hist = cmd_ln_int32_r(s->config, "-pl_window") + 2;
    s->hist = ckd_calloc(s->n_fast_hist, sizeof(*s->hist));
    /* s->f will be a rotating pointer into s->hist. */
    s->f = s->hist;
    for (i = 0; i < s->n_fast_hist; ++i) {
        int j, k, m;
        /* Top-N codewords for every codebook and feature. */
        s->hist[i].topn = ckd_calloc_3d(s->g->n_mgau, s->g->n_feat,
                                        s->max_topn, sizeof(ptm_topn_t));
        /* Initialize them to sane (yet arbitrary) defaults. */
        for (j = 0; j < s->g->n_mgau; ++j) {
            for (k = 0; k < s->g->n_feat; ++k) {
                for (m = 0; m < s->max_topn; ++m) {
                    s->hist[i].topn[j][k][m].cw = m;
                    s->hist[i].topn[j][k][m].score = WORST_DIST;
                }
            }
        }
        /* Active codebook mapping (just codebook, not features,
           at least not yet) */
        s->hist[i].mgau_active = bitvec_alloc(s->g->n_mgau);
        /* Start with them all on, prune them later. */
        bitvec_set_all(s->hist[i].mgau_active, s->g->n_mgau);
    }

    ps = (ps_mgau_t *)s;
    ps->vt = &ptm_mgau_funcs;
    return ps;
error_out:
    ptm_mgau_free(ps_mgau_base(s));
    return NULL;
}

int
ptm_mgau_mllr_transform(ps_mgau_t *ps,
                            ps_mllr_t *mllr)
{
    ptm_mgau_t *s = (ptm_mgau_t *)ps;
    return gauden_mllr_transform(s->g, mllr, s->config);
}

void
ptm_mgau_free(ps_mgau_t *ps)
{
    int i;
    ptm_mgau_t *s = (ptm_mgau_t *)ps;

    logmath_free(s->lmath);
    logmath_free(s->lmath_8b);
    if (s->sendump_mmap) {
        ckd_free_2d(s->mixw); 
        mmio_file_unmap(s->sendump_mmap);
    }
    else {
        ckd_free_3d(s->mixw);
    }
    ckd_free(s->sen2cb);
    
    for (i = 0; i < s->n_fast_hist; i++) {
	ckd_free_3d(s->hist[i].topn);
	bitvec_free(s->hist[i].mgau_active);
    }
    ckd_free(s->hist);
    
    gauden_free(s->g);
    ckd_free(s);
}
