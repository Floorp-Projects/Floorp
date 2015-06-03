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
 * agc.h -- Various forms of automatic gain control (AGC)
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log$
 * Revision 1.1  2006/04/05  20:27:30  dhdfu
 * A Great Reorganzation of header files and executables
 * 
 * Revision 1.8  2005/06/21 19:25:41  arthchan2003
 * 1, Fixed doxygen documentation. 2, Added $ keyword.
 *
 * Revision 1.4  2005/06/13 04:02:56  archan
 * Fixed most doxygen-style documentation under libs3decoder.
 *
 * Revision 1.3  2005/03/30 01:22:46  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 28-Apr-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Copied from previous version.
 */


#ifndef _S3_AGC_H_
#define _S3_AGC_H_

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>

#include <sphinxbase/prim_type.h>
#include <sphinxbase/fe.h>

/** \file agc.h
 *  \brief routine that implements automatic gain control
 *  
 *  \warning This function may not be fully compatible with
 *  SphinxTrain's family of AGC. 
 *
 *  This implements AGC. 
 */
#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Types of acoustic gain control to apply to the features.
 */
typedef enum agc_type_e {
    AGC_NONE = 0,
    AGC_MAX,
    AGC_EMAX,
    AGC_NOISE
} agc_type_t;

/** Convert string representation (from command-line) to agc_type_t */
SPHINXBASE_EXPORT
agc_type_t agc_type_from_str(const char *str);

/** String representations of agc_type_t values. */
SPHINXBASE_EXPORT
extern const char *agc_type_str[];

/**
 * Structure holding data for doing AGC.
 **/
typedef struct agc_s {
    mfcc_t max;      /**< Estimated max for current utterance (for AGC_EMAX) */
    mfcc_t obs_max;  /**< Observed max in current utterance */
    int32 obs_frame; /**< Whether any data was observed after prev update */
    int32 obs_utt;   /**< Whether any utterances have been observed */
    mfcc_t obs_max_sum;
    mfcc_t noise_thresh; /**< Noise threshold (for AGC_NOISE only) */
} agc_t;

/**
 * Initialize AGC structure with default values.
 */
SPHINXBASE_EXPORT
agc_t *agc_init(void);

/**
 * Free AGC structure.
 */
SPHINXBASE_EXPORT
void agc_free(agc_t *agc);

/**
 * Apply AGC to the given mfc vectors (normalize all C0 mfc coefficients in the given
 * input such that the max C0 value is 0, by subtracting the input max C0 from all).
 * This function operates on an entire utterance at a time.  Hence, the entire utterance
 * must be available beforehand (batchmode).
 */
SPHINXBASE_EXPORT
void agc_max(agc_t *agc,	/**< In: AGC structure (not used) */
             mfcc_t **mfc,	/**< In/Out: mfc[f] = cepstrum vector in frame f */
             int32 n_frame	/**< In: number of frames of cepstrum vectors supplied */
    );

/**
 * Apply AGC to the given block of MFC vectors. 
 * Unlike agc_max() this does not require the entire utterance to be
 * available.  Call agc_emax_update() at the end of each utterance to
 * update the AGC parameters. */
SPHINXBASE_EXPORT
void agc_emax(agc_t *agc,	/**< In: AGC structure */
              mfcc_t **mfc,	/**< In/Out: mfc[f] = cepstrum vector in frame f */
              int32 n_frame	/**< In: number of frames of cepstrum vectors supplied */
    );

/**
 * Update AGC parameters for next utterance.
 **/
SPHINXBASE_EXPORT
void agc_emax_update(agc_t *agc /**< In: AGC structure */
    );

/**
 * Get the current AGC maximum estimate.
 **/
SPHINXBASE_EXPORT
float32 agc_emax_get(agc_t *agc);

/**
 * Set the current AGC maximum estimate.
 **/
SPHINXBASE_EXPORT
void agc_emax_set(agc_t *agc, float32 m);

/**
 * Apply AGC using noise threshold to the given block of MFC vectors. 
 **/
SPHINXBASE_EXPORT
void agc_noise(agc_t *agc,	/**< In: AGC structure */
               mfcc_t **mfc,	/**< In/Out: mfc[f] = cepstrum vector in frame f */
               int32 n_frame	/**< In: number of frames of cepstrum vectors supplied */
    );

/**
 * Get the current AGC noise threshold.
 **/
SPHINXBASE_EXPORT
float32 agc_get_threshold(agc_t *agc);

/**
 * Set the current AGC noise threshold.
 **/
SPHINXBASE_EXPORT
void agc_set_threshold(agc_t *agc, float32 threshold);


#ifdef __cplusplus
}
#endif

#endif
