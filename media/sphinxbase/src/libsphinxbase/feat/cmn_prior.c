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
/*************************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 2000 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * 30-Dec-2000  Rita Singh (rsingh@cs.cmu.edu) at Carnegie Mellon University
 * Created
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4244)
#endif

#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/err.h"
#include "sphinxbase/cmn.h"

void
cmn_prior_set(cmn_t *cmn, mfcc_t const * vec)
{
    int32 i;

    E_INFO("cmn_prior_set: from < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");

    for (i = 0; i < cmn->veclen; i++) {
        cmn->cmn_mean[i] = vec[i];
        cmn->sum[i] = vec[i] * CMN_WIN;
    }
    cmn->nframe = CMN_WIN;

    E_INFO("cmn_prior_set: to   < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");
}

void
cmn_prior_get(cmn_t *cmn, mfcc_t * vec)
{
    int32 i;

    for (i = 0; i < cmn->veclen; i++)
        vec[i] = cmn->cmn_mean[i];

}

static void
cmn_prior_shiftwin(cmn_t *cmn)
{
    mfcc_t sf;
    int32 i;

    E_INFO("cmn_prior_update: from < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");

    sf = FLOAT2MFCC(1.0) / cmn->nframe;
    for (i = 0; i < cmn->veclen; i++)
        cmn->cmn_mean[i] = cmn->sum[i] / cmn->nframe; /* sum[i] * sf */

    /* Make the accumulation decay exponentially */
    if (cmn->nframe >= CMN_WIN_HWM) {
        sf = CMN_WIN * sf;
        for (i = 0; i < cmn->veclen; i++)
            cmn->sum[i] = MFCCMUL(cmn->sum[i], sf);
        cmn->nframe = CMN_WIN;
    }

    E_INFO("cmn_prior_update: to   < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");
}

void
cmn_prior_update(cmn_t *cmn)
{
    mfcc_t sf;
    int32 i;

    if (cmn->nframe <= 0)
        return;

    E_INFO("cmn_prior_update: from < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");

    /* Update mean buffer */
    sf = FLOAT2MFCC(1.0) / cmn->nframe;
    for (i = 0; i < cmn->veclen; i++)
        cmn->cmn_mean[i] = cmn->sum[i] / cmn->nframe; /* sum[i] * sf; */

    /* Make the accumulation decay exponentially */
    if (cmn->nframe > CMN_WIN_HWM) {
        sf = CMN_WIN * sf;
        for (i = 0; i < cmn->veclen; i++)
            cmn->sum[i] = MFCCMUL(cmn->sum[i], sf);
        cmn->nframe = CMN_WIN;
    }

    E_INFO("cmn_prior_update: to   < ");
    for (i = 0; i < cmn->veclen; i++)
        E_INFOCONT("%5.2f ", MFCC2FLOAT(cmn->cmn_mean[i]));
    E_INFOCONT(">\n");
}

void
cmn_prior(cmn_t *cmn, mfcc_t **incep, int32 varnorm, int32 nfr)
{
    int32 i, j;

    if (nfr <= 0)
        return;

    if (varnorm)
        E_FATAL
            ("Variance normalization not implemented in live mode decode\n");

    for (i = 0; i < nfr; i++) {

	/* Skip zero energy frames */
	if (incep[i][0] < 0)
	    continue;

        for (j = 0; j < cmn->veclen; j++) {
            cmn->sum[j] += incep[i][j];
            incep[i][j] -= cmn->cmn_mean[j];
        }

        ++cmn->nframe;
    }

    /* Shift buffer down if we have more than CMN_WIN_HWM frames */
    if (cmn->nframe > CMN_WIN_HWM)
        cmn_prior_shiftwin(cmn);
}
