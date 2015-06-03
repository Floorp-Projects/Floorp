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

#ifndef _LIBFBS_GAUDEN_H_
#define _LIBFBS_GAUDEN_H_

/** \file ms_gauden.h
 * \brief (Sphinx 3.0 specific) Gaussian density module.
 *
 * Gaussian density distribution implementation. There are two major
 * difference bettwen ms_gauden and cont_mgau. One is the fact that
 * ms_gauden only take cares of the Gaussian computation part where
 * cont_mgau actually take care of senone computation as well. The
 * other is the fact that ms_gauden is a multi-stream implementation
 * of GMM computation.
 *
 */

/* SphinxBase headers. */
#include <sphinxbase/feat.h>
#include <sphinxbase/logmath.h>
#include <sphinxbase/cmd_ln.h>

/* Local headers. */
#include "vector.h"
#include "pocketsphinx_internal.h"
#include "hmm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \struct gauden_dist_t
 * \brief Structure to store distance (density) values for a given input observation wrt density values in some given codebook.
 */
typedef struct {
    int32 id;		/**< Index of codeword (gaussian density) */
    mfcc_t dist;		/**< Density value for input observation wrt above codeword;
                           NOTE: result in logs3 domain, but var_t used for speed */

} gauden_dist_t;

/**
 * \struct gauden_t
 * \brief Multivariate gaussian mixture density parameters
 */
typedef struct {
    mfcc_t ****mean;	/**< mean[codebook][feature][codeword] vector */
    mfcc_t ****var;	/**< like mean; diagonal covariance vector only */
    mfcc_t ***det;	/**< log(determinant) for each variance vector;
			   actually, log(sqrt(2*pi*det)) */
    logmath_t *lmath;   /**< log math computation */
    int32 n_mgau;	/**< Number codebooks */
    int32 n_feat;	/**< Number feature streams in each codebook */
    int32 n_density;	/**< Number gaussian densities in each codebook-feature stream */
    int32 *featlen;	/**< feature length for each feature */
} gauden_t;


/**
 * Read mixture gaussian codebooks from the given files.  Allocate memory space needed
 * for them.  Apply the specified variance floor value.
 * Return value: ptr to the model created; NULL if error.
 * (See Sphinx3 model file-format documentation.)
 */
gauden_t *
gauden_init (char const *meanfile,/**< Input: File containing means of mixture gaussians */
	     char const *varfile,/**< Input: File containing variances of mixture gaussians */
	     float32 varfloor,	/**< Input: Floor value to be applied to variances */
             logmath_t *lmath
    );

/** Release memory allocated by gauden_init. */
void gauden_free(gauden_t *g); /**< In: The gauden_t to free */

/** Transform Gaussians according to an MLLR matrix (or, eventually, more). */
int32 gauden_mllr_transform(gauden_t *s, ps_mllr_t *mllr, cmd_ln_t *config);

/**
 * Compute gaussian density values for the given input observation vector wrt the
 * specified mixture gaussian codebook (which may consist of several feature streams).
 * Density values are left UNnormalized.
 * @return 0 if successful, -1 otherwise.
 */
int32
gauden_dist (gauden_t *g,	/**< In: handle to entire ensemble of codebooks */
	     int mgau,		/**< In: codebook for which density values to be evaluated
				   (g->{mean,var}[mgau]) */
	     int n_top,		/**< In: Number top densities to be evaluated */
	     mfcc_t **obs,	/**< In: Observation vector; obs[f] = for feature f */
	     gauden_dist_t **out_dist
	     /**< Out: n_top best codewords and density values,
		in worsening order, for each feature stream.
		out_dist[f][i] = i-th best density for feature f.
		Caller must allocate memory for this output */
    );

/**
   Dump the definitionn of Gaussian distribution. 
*/
void gauden_dump (const gauden_t *g  /**< In: Gaussian distribution g*/
    );

/**
   Dump the definition of Gaussian distribution of a particular index to the standard output stream
*/
void gauden_dump_ind (const gauden_t *g,  /**< In: Gaussian distribution g*/
		      int senidx          /**< In: The senone index of the Gaussian */
    );

#ifdef __cplusplus
}
#endif

#endif /* GAUDEN_H */ 
