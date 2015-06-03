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
 * senone.h -- Mixture density weights associated with each tied state.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * $Log$
 * Revision 1.1  2006/04/05  20:27:30  dhdfu
 * A Great Reorganzation of header files and executables
 * 
 * Revision 1.7  2006/02/22 17:27:39  arthchan2003
 * Merged from SPHINX3_5_2_RCI_IRII_BRANCH: 1, NOT doing truncation in the multi-stream GMM computation \n. 2, Added .s3cont. to be the alias of the old multi-stream GMM computation routine \n. 3, Added license \n.  4, Fixed dox-doc. \n
 *
 * Revision 1.6.4.4  2006/01/16 19:47:05  arthchan2003
 * Removed the truncation of senone probability code.
 *
 * Revision 1.6.4.3  2005/08/03 18:53:43  dhdfu
 * Add memory deallocation functions.  Also move all the initialization
 * of ms_mgau_model_t into ms_mgau_init (duh!), which entails removing it
 * from decode_anytopo and friends.
 *
 * Revision 1.6.4.2  2005/07/20 19:39:01  arthchan2003
 * Added licences in ms_* series of code.
 *
 * Revision 1.6.4.1  2005/07/05 05:47:59  arthchan2003
 * Fixed dox-doc. struct level of documentation are included.
 *
 * Revision 1.6  2005/06/21 19:00:19  arthchan2003
 * Add more detail comments  to ms_senone.h
 *
 * Revision 1.5  2005/06/21 18:57:31  arthchan2003
 * 1, Fixed doxygen documentation. 2, Added $ keyword.
 *
 * Revision 1.2  2005/06/13 04:02:56  archan
 * Fixed most doxygen-style documentation under libs3decoder.
 *
 * Revision 1.1.1.1  2005/03/24 15:24:00  archan
 * I found Evandro's suggestion is quite right after yelling at him 2 days later. So I decide to check this in again without any binaries. (I have done make distcheck. ) . Again, this is a candidate for s3.6 and I believe I need to work out 4-5 intermediate steps before I can complete the first prototype.  That's why I keep local copies. 
 *
 * Revision 1.4  2004/12/06 10:52:01  arthchan2003
 * Enable doxygen documentation in libs3decoder
 *
 * Revision 1.3  2004/11/13 21:25:19  arthchan2003
 * commit of 1, absolute CI-GMMS , 2, fast CI senone computation using svq, 3, Decrease the number of static variables, 4, fixing the random generator problem of vector_vqgen, 5, move all unused files to NOTUSED
 *
 * Revision 1.2  2004/08/31 08:43:47  arthchan2003
 * Fixing _cpluscplus directive
 *
 * Revision 1.1  2004/08/09 00:17:11  arthchan2003
 * Incorporating s3.0 align, at this point, there are still some small problems in align but they don't hurt. For example, the score doesn't match with s3.0 and the output will have problem if files are piped to /dev/null/. I think we can go for it.
 *
 * Revision 1.1  2003/02/14 14:40:34  cbq
 * Compiles.  Analysis is probably hosed.
 *
 * Revision 1.1  2000/04/24 09:39:41  lenzo
 * s3 import.
 *
 * 
 * 13-Dec-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added senone_eval_all().
 * 
 * 12-Nov-95	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Created.
 */


#ifndef _LIBFBS_SENONE_H_
#define _LIBFBS_SENONE_H_


/* SphinxBase headers. */
#include <sphinxbase/err.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/logmath.h>

/* Local headers. */
#include "ms_gauden.h"
#include "bin_mdef.h"

/** \file ms_senone.h
 *  \brief (Sphinx 3.0 specific) multiple streams senones. used with ms_gauden.h
 *  In Sphinx 3.0 family of tools, ms_senone is used to combine the Gaussian scores.
 *  Its existence is crucial in Sphinx 3.0 because 3.0 supports both SCHMM and CDHMM. 
 *  There are optimization scheme for SCHMM (e.g. compute the top-N Gaussian) that is 
 *  applicable to SCHMM than CDHMM.  This is wrapped in senone_eval_all. 
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8 senprob_t;	/**< Senone logs3-probs, truncated to 8 bits */

/**
 * \struct senone_t
 * \brief 8-bit senone PDF structure. 
 * 
 * 8-bit senone PDF structure.  Senone pdf values are normalized, floored, converted to
 * logs3 domain, and finally truncated to 8 bits precision to conserve memory space.
 */
typedef struct {
    senprob_t ***pdf;		/**< gaussian density mixture weights, organized two possible
                                   ways depending on n_gauden:
                                   if (n_gauden > 1): pdf[sen][feat][codeword].  Not an
                                   efficient representation--memory access-wise--but
                                   evaluating the many codebooks will be more costly.
                                   if (n_gauden == 1): pdf[feat][codeword][sen].  Optimized
                                   for the shared-distribution semi-continuous case. */
    logmath_t *lmath;           /**< log math computation */
    uint32 n_sen;		/**< Number senones in this set */
    uint32 n_feat;		/**< Number feature streams */ 
    uint32 n_cw;		/**< Number codewords per codebook,stream */
    uint32 n_gauden;		/**< Number gaussian density codebooks referred to by senones */
    float32 mixwfloor;		/**< floor applied to each PDF entry */
    uint32 *mgau;		/**< senone-id -> mgau-id mapping for senones in this set */
    int32 *featscr;              /**< The feature score for every senone, will be initialized inside senone_eval_all */
    int32 aw;			/**< Inverse acoustic weight */
} senone_t;


/**
 * Load a set of senones (mixing weights and mixture gaussian codebook mappings) from
 * the given files.  Normalize weights for each codebook, apply the given floor, convert
 * PDF values to logs3 domain and quantize to 8-bits.
 * @return pointer to senone structure created.  Caller MUST NOT change its contents.
 */
senone_t *senone_init (gauden_t *g,             /**< In: codebooks */
                       char const *mixwfile,	/**< In: mixing weights file */
		       char const *mgau_mapfile,/**< In: file specifying mapping from each
						   senone to mixture gaussian codebook.
						   If NULL all senones map to codebook 0 */
		       float32 mixwfloor,	/**< In: Floor value for senone weights */
                       logmath_t *lmath,        /**< In: log math computation */
                       bin_mdef_t *mdef         /**< In: model definition */
    );

/** Release memory allocated by senone_init. */
void senone_free(senone_t *s); /**< In: The senone_t to free */

/**
 * Evaluate the score for the given senone wrt to the given top N gaussian codewords.
 * @return senone score (in logs3 domain).
 */
int32 senone_eval (senone_t *s, int id,		/**< In: senone for which score desired */
		   gauden_dist_t **dist,	/**< In: top N codewords and densities for
						   all features, to be combined into
						   senone score.  IE, dist[f][i] = i-th
						   best <codeword,density> for feaure f */
		   int n_top		/**< In: Length of dist[f], for each f */
    );

#ifdef __cplusplus
}
#endif

#endif
