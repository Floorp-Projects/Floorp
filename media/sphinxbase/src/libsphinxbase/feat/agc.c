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
/*
 * agc.c -- Various forms of automatic gain control (AGC)
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log$
 * Revision 1.5  2005/06/21  19:25:41  arthchan2003
 * 1, Fixed doxygen documentation. 2, Added $ keyword.
 * 
 * Revision 1.3  2005/03/30 01:22:46  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 04-Nov-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */

#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sphinxbase/err.h"
#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/agc.h"

/* NOTE!  These must match the enum in agc.h */
const char *agc_type_str[] = {
    "none",
    "max",
    "emax",
    "noise"
};
static const int n_agc_type_str = sizeof(agc_type_str)/sizeof(agc_type_str[0]);

agc_type_t
agc_type_from_str(const char *str)
{
    int i;

    for (i = 0; i < n_agc_type_str; ++i) {
        if (0 == strcmp(str, agc_type_str[i]))
            return (agc_type_t)i;
    }
    E_FATAL("Unknown AGC type '%s'\n", str);
    return AGC_NONE;
}

agc_t *agc_init(void)
{
    agc_t *agc;
    agc = ckd_calloc(1, sizeof(*agc));
    agc->noise_thresh = FLOAT2MFCC(2.0);
    
    return agc;
}

void agc_free(agc_t *agc)
{
    ckd_free(agc);
}

/**
 * Normalize c0 for all frames such that max(c0) = 0.
 */
void
agc_max(agc_t *agc, mfcc_t **mfc, int32 n_frame)
{
    int32 i;

    if (n_frame <= 0)
        return;
    agc->obs_max = mfc[0][0];
    for (i = 1; i < n_frame; i++) {
        if (mfc[i][0] > agc->obs_max) {
            agc->obs_max = mfc[i][0];
            agc->obs_frame = 1;
        }
    }

    E_INFO("AGCMax: obs=max= %.2f\n", agc->obs_max);
    for (i = 0; i < n_frame; i++)
        mfc[i][0] -= agc->obs_max;
}

void
agc_emax_set(agc_t *agc, float32 m)
{
    agc->max = FLOAT2MFCC(m);
    E_INFO("AGCEMax: max= %.2f\n", m);
}

float32
agc_emax_get(agc_t *agc)
{
    return MFCC2FLOAT(agc->max);
}

void
agc_emax(agc_t *agc, mfcc_t **mfc, int32 n_frame)
{
    int i;

    if (n_frame <= 0)
        return;
    for (i = 0; i < n_frame; ++i) {
        if (mfc[i][0] > agc->obs_max) {
            agc->obs_max = mfc[i][0];
            agc->obs_frame = 1;
        }
        mfc[i][0] -= agc->max;
    }
}

/* Update estimated max for next utterance */
void
agc_emax_update(agc_t *agc)
{
    if (agc->obs_frame) {            /* Update only if some data observed */
        agc->obs_max_sum += agc->obs_max;
        agc->obs_utt++;

        /* Re-estimate max over past history; decay the history */
        agc->max = agc->obs_max_sum / agc->obs_utt;
        if (agc->obs_utt == 16) {
            agc->obs_max_sum /= 2;
            agc->obs_utt = 8;
        }
    }
    E_INFO("AGCEMax: obs= %.2f, new= %.2f\n", agc->obs_max, agc->max);

    /* Reset the accumulators for the next utterance. */
    agc->obs_frame = 0;
    agc->obs_max = FLOAT2MFCC(-1000.0); /* Less than any real C0 value (hopefully!!) */
}

void
agc_noise(agc_t *agc,
          mfcc_t **cep,
          int32 nfr)
{
    mfcc_t min_energy; /* Minimum log-energy */
    mfcc_t noise_level;        /* Average noise_level */
    int32 i;           /* frame index */
    int32 noise_frames;        /* Number of noise frames */

    /* Determine minimum log-energy in utterance */
    min_energy = cep[0][0];
    for (i = 0; i < nfr; ++i) {
        if (cep[i][0] < min_energy)
            min_energy = cep[i][0];
    }

    /* Average all frames between min_energy and min_energy + agc->noise_thresh */
    noise_frames = 0;
    noise_level = 0;
    min_energy += agc->noise_thresh;
    for (i = 0; i < nfr; ++i) {
        if (cep[i][0] < min_energy) {
            noise_level += cep[i][0];
            noise_frames++;
        }
    }

    if (noise_frames > 0) {
        noise_level /= noise_frames;
        E_INFO("AGC NOISE: max= %6.3f\n", MFCC2FLOAT(noise_level));
        /* Subtract noise_level from all log_energy values */
        for (i = 0; i < nfr; i++) {
            cep[i][0] -= noise_level;
        }
    }
}

void
agc_set_threshold(agc_t *agc, float32 threshold)
{
    agc->noise_thresh = FLOAT2MFCC(threshold);
}

float32
agc_get_threshold(agc_t *agc)
{
    return FLOAT2MFCC(agc->noise_thresh);
}
