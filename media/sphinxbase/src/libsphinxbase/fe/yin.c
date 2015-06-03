/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 2008 Beyond Access, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY BEYOND ACCESS, INC. ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL BEYOND ACCESS, INC.  NOR
 * ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file yin.c Implementation of pitch extraction.
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

/* This implements part of the YIN algorithm:
 *
 * "YIN, a fundamental frequency estimator for speech and music".
 * Alain de Cheveigné and Hideki Kawahara.  Journal of the Acoustical
 * Society of America, 111 (4), April 2002.
 */

#include "sphinxbase/prim_type.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/fixpoint.h"

#include "sphinxbase/yin.h"

#include <stdio.h>
#include <string.h>

struct yin_s {
    uint16 frame_size;       /** Size of analysis frame. */
#ifndef FIXED_POINT
    float search_threshold; /**< Threshold for finding period */
    float search_range;     /**< Range around best local estimate to search */
#else
    uint16 search_threshold; /**< Threshold for finding period, in Q15 */
    uint16 search_range;     /**< Range around best local estimate to search, in Q15 */
#endif
    uint16 nfr;              /**< Number of frames read so far. */

    unsigned char wsize;    /**< Size of smoothing window. */
    unsigned char wstart;   /**< First frame in window. */
    unsigned char wcur;     /**< Current frame of analysis. */
    unsigned char endut;    /**< Hoch Hech! Are we at the utterance end? */

#ifndef FIXED_POINT
    float **diff_window;    /**< Window of difference function outputs. */
#else
    fixed32 **diff_window;  /**< Window of difference function outputs. */
#endif
    uint16 *period_window;  /**< Window of best period estimates. */
    int16 *frame;           /**< Storage for frame */
};

/**
 * The core of YIN: cumulative mean normalized difference function.
 */
#ifndef FIXED_POINT
static void
cmn_diff(int16 const *signal, float *out_diff, int ndiff)
{
    double cum;
    int t, j;

    cum = 0.0f;
    out_diff[0] = 1.0f;

    for (t = 1; t < ndiff; ++t) {
        float dd;
        dd = 0.0f;
        for (j = 0; j < ndiff; ++j) {
             int diff = signal[j] - signal[t + j];
             dd += (diff * diff);
        }
        cum += dd;
        out_diff[t] = (float)(dd * t / cum);
    }
}
#else
static void
cmn_diff(int16 const *signal, int32 *out_diff, int ndiff)
{
    uint32 cum, cshift;
    int32 t, tscale;

    out_diff[0] = 32768;
    cum = 0;
    cshift = 0;

    /* Determine how many bits we can scale t up by below. */
    for (tscale = 0; tscale < 32; ++tscale)
        if (ndiff & (1<<(31-tscale)))
            break;
    --tscale; /* Avoid teh overflowz. */
    /* printf("tscale is %d (ndiff - 1) << tscale is %d\n",
       tscale, (ndiff-1) << tscale); */

    /* Somewhat elaborate block floating point implementation.
     * The fp implementation of this is really a lot simpler. */
    for (t = 1; t < ndiff; ++t) {
        uint32 dd, dshift, norm;
        int j;

        dd = 0;
        dshift = 0;
        for (j = 0; j < ndiff; ++j) {
            int diff = signal[j] - signal[t + j];
            /* Guard against overflows. */
            if (dd > (1UL<<tscale)) {
                dd >>= 1;
                ++dshift;
            }
            dd += (diff * diff) >> dshift;
        }
        /* Make sure the diffs and cum are shifted to the same
         * scaling factor (usually dshift will be zero) */
        if (dshift > cshift) {
            cum += dd << (dshift-cshift);
        }
        else {
            cum += dd >> (cshift-dshift);
        }

        /* Guard against overflows and also ensure that (t<<tscale) > cum. */
        while (cum > (1UL<<tscale)) {
            cum >>= 1;
            ++cshift;
        }
        /* Avoid divide-by-zero! */
        if (cum == 0) cum = 1;
        /* Calculate the normalizer in high precision. */
        norm = (t << tscale) / cum;
        /* Do a long multiply and shift down to Q15. */
        out_diff[t] = (int32)(((long long)dd * norm)
                              >> (tscale - 15 + cshift - dshift));
        /* printf("dd %d cshift %d dshift %d scaledt %d cum %d norm %d cmn %d\n",
           dd, cshift, dshift, (t<<tscale), cum, norm, out_diff[t]); */
    }
}
#endif

yin_t *
yin_init(int frame_size, float search_threshold,
         float search_range, int smooth_window)
{
    yin_t *pe;

    pe = ckd_calloc(1, sizeof(*pe));
    pe->frame_size = frame_size;
#ifndef FIXED_POINT
    pe->search_threshold = search_threshold;
    pe->search_range = search_range;
#else
    pe->search_threshold = (uint16)(search_threshold * 32768);
    pe->search_range = (uint16)(search_range * 32768);
#endif
    pe->wsize = smooth_window * 2 + 1;
    pe->diff_window = ckd_calloc_2d(pe->wsize,
                                    pe->frame_size / 2,
                                    sizeof(**pe->diff_window));
    pe->period_window = ckd_calloc(pe->wsize,
                                   sizeof(*pe->period_window));
    pe->frame = ckd_calloc(pe->frame_size, sizeof(*pe->frame));
    return pe;
}

void
yin_free(yin_t *pe)
{
    ckd_free_2d(pe->diff_window);
    ckd_free(pe->period_window);
    ckd_free(pe);
}

void
yin_start(yin_t *pe)
{
    /* Reset the circular window pointers. */
    pe->wstart = pe->endut = 0;
    pe->nfr = 0;
}

void
yin_end(yin_t *pe)
{
    pe->endut = 1;
}

int
#ifndef FIXED_POINT
thresholded_search(float *diff_window, float threshold, int start, int end)
#else
thresholded_search(int32 *diff_window, fixed32 threshold, int start, int end)
#endif
{
    int i, argmin;
#ifndef FIXED_POINT
    float min;
#else
    int min;
#endif

    min = diff_window[start];
    argmin = start;
    for (i = start + 1; i < end; ++i) {
#ifndef FIXED_POINT
        float diff = diff_window[i];
#else
        int diff = diff_window[i];
#endif

        if (diff < threshold) {
            min = diff;
            argmin = i;
            break;
        }
        if (diff < min) {
            min = diff;
            argmin = i;
        }
    }
    return argmin;
}

void 
yin_store(yin_t *pe, int16 const *frame)
{
    memcpy(pe->frame, frame, pe->frame_size * sizeof(*pe->frame));
}

void
yin_write(yin_t *pe, int16 const *frame)
{
    int outptr, difflen;

    /* Rotate the window one frame forward. */
    ++pe->wstart;
    /* Fill in the frame before wstart. */
    outptr = pe->wstart - 1;
    /* Wrap around the window pointer. */
    if (pe->wstart == pe->wsize)
        pe->wstart = 0;

    /* Now calculate normalized difference function. */
    difflen = pe->frame_size / 2;
    cmn_diff(frame, pe->diff_window[outptr], difflen);

    /* Find the first point under threshold.  If not found, then
     * use the absolute minimum. */
    pe->period_window[outptr]
        = thresholded_search(pe->diff_window[outptr],
                             pe->search_threshold, 0, difflen);

    /* Increment total number of frames. */
    ++pe->nfr;
}

void 
yin_write_stored(yin_t *pe)
{
    yin_write(pe, pe->frame);
}

int
yin_read(yin_t *pe, uint16 *out_period, float *out_bestdiff)
{
    int wstart, wlen, half_wsize, i;
    int best, search_width, low_period, high_period;
#ifndef FIXED_POINT
    float best_diff;
#else
    int best_diff;
#endif

    half_wsize = (pe->wsize-1)/2;
    /* Without any smoothing, just return the current value (don't
     * need to do anything to the current poitner either). */
    if (half_wsize == 0) {
        if (pe->endut)
            return 0;
        *out_period = pe->period_window[0];
#ifndef FIXED_POINT
        *out_bestdiff = pe->diff_window[0][pe->period_window[0]];
#else
        *out_bestdiff = pe->diff_window[0][pe->period_window[0]] / 32768.0f;
#endif
        return 1;
    }

    /* We can't do anything unless we have at least (wsize-1)/2 + 1
     * frames, unless we're at the end of the utterance. */
    if (pe->endut == 0 && pe->nfr < half_wsize + 1) {
        /* Don't increment the current pointer either. */
        return 0;
    }

    /* Establish the smoothing window. */
    /* End of utterance. */
    if (pe->endut) {
        /* We are done (no more data) when pe->wcur = pe->wstart. */
        if (pe->wcur == pe->wstart)
            return 0;
        /* I.e. pe->wcur (circular minus) half_wsize. */
        wstart = (pe->wcur + pe->wsize - half_wsize) % pe->wsize;
        /* Number of frames from wstart up to pe->wstart. */
        wlen = pe->wstart - wstart;
        if (wlen < 0) wlen += pe->wsize;
        /*printf("ENDUT! ");*/
    }
    /* Beginning of utterance. */
    else if (pe->nfr < pe->wsize) {
        wstart = 0;
        wlen = pe->nfr;
    }
    /* Normal case, it is what it is. */
    else {
        wstart = pe->wstart;
        wlen = pe->wsize;
    }

    /* Now (finally) look for the best local estimate. */
    /* printf("Searching for local estimate in %d frames around %d\n",
       wlen, pe->nfr + 1 - wlen); */
    best = pe->period_window[pe->wcur];
    best_diff = pe->diff_window[pe->wcur][best];
    for (i = 0; i < wlen; ++i) {
        int j = wstart + i;
#ifndef FIXED_POINT
        float diff;
#else
        int diff;
#endif

        j %= pe->wsize;
        diff = pe->diff_window[j][pe->period_window[j]];
        /* printf("%.2f,%.2f ", 1.0 - (double)diff/32768,
           pe->period_window[j] ? 8000.0/pe->period_window[j] : 8000.0); */
        if (diff < best_diff) {
            best_diff = diff;
            best = pe->period_window[j];
        }
    }
    /* printf("best: %.2f, %.2f\n", 1.0 - (double)best_diff/32768,
       best ? 8000.0/best : 8000.0); */
    /* If it's the same as the current one then return it. */
    if (best == pe->period_window[pe->wcur]) {
        /* Increment the current pointer. */
        if (++pe->wcur == pe->wsize)
            pe->wcur = 0;
        *out_period = best;
#ifndef FIXED_POINT
        *out_bestdiff = best_diff;
#else
        *out_bestdiff = best_diff / 32768.0f;
#endif
        return 1;
    }
    /* Otherwise, redo the search inside a narrower range. */
#ifndef FIXED_POINT
    search_width = (int)(best * pe->search_range);
#else
    search_width = best * pe->search_range / 32768;
#endif
    /* printf("Search width = %d * %.2f = %d\n",
       best, (double)pe->search_range/32768, search_width); */
    if (search_width == 0) search_width = 1;
    low_period = best - search_width;
    high_period = best + search_width;
    if (low_period < 0) low_period = 0;
    if (high_period > pe->frame_size / 2) high_period = pe->frame_size / 2;
    /* printf("Searching from %d to %d\n", low_period, high_period); */
    best = thresholded_search(pe->diff_window[pe->wcur],
                              pe->search_threshold,
                              low_period, high_period);
    best_diff = pe->diff_window[pe->wcur][best];

    if (out_period)
        *out_period = (best > 65535) ? 65535 : best;
    if (out_bestdiff) {
#ifndef FIXED_POINT
        *out_bestdiff = (best_diff > 1.0f) ? 1.0f : best_diff;
#else
        *out_bestdiff = (best_diff > 32768) ? 1.0f : best_diff / 32768.0f;
#endif
    }

    /* Increment the current pointer. */
    if (++pe->wcur == pe->wsize)
        pe->wcur = 0;
    return 1;
}
