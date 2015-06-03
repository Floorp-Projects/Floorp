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
 * tmat.h
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: tmat.h,v $
 * Revision 1.1.1.1  2006/05/23 18:45:03  dhuggins
 * re-importation
 *
 * Revision 1.1  2005/09/29 21:51:19  dhuggins
 * Add support for Sphinx3 tmat files.  Amazingly enough, it Just Works
 * (but it isn't terribly robust)
 *
 * Revision 1.9  2005/06/21 19:23:35  arthchan2003
 * 1, Fixed doxygen documentation. 2, Added $ keyword.
 *
 * Revision 1.6  2005/06/13 04:02:56  archan
 * Fixed most doxygen-style documentation under libs3decoder.
 *
 * Revision 1.5  2005/05/03 04:09:09  archan
 * Implemented the heart of word copy search. For every ci-phone, every word end, a tree will be allocated to preserve its pathscore.  This is different from 3.5 or below, only the best score for a particular ci-phone, regardless of the word-ends will be preserved at every frame.  The graph propagation will not collect unused word tree at this point. srch_WST_propagate_wd_lv2 is also as the most stupid in the century.  But well, after all, everything needs a start.  I will then really get the results from the search and see how it looks.
 *
 * Revision 1.4  2005/04/21 23:50:26  archan
 * Some more refactoring on the how reporting of structures inside kbcore_t is done, it is now 50% nice. Also added class-based LM test case into test-decode.sh.in.  At this moment, everything in search mode 5 is already done.  It is time to test the idea whether the search can really be used.
 *
 * Revision 1.3  2005/03/30 01:22:47  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 20.Apr.2001  RAH (rhoughton@mediasite.com, ricky.houghton@cs.cmu.edu)
 *              Added tmat_free to free allocated memory 
 *
 * 29-Feb-2000	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added tmat_chk_1skip(), and made tmat_chk_uppertri() public.
 * 
 * 10-Dec-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added tmat_dump().
 * 
 * 11-Mar-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Started based on original S3 implementation.
 */


#ifndef _S3_TMAT_H_
#define _S3_TMAT_H_

/* System headers. */
#include <stdio.h>

/* SphinxBase headers. */
#include <sphinxbase/logmath.h>

/** \file tmat.h
 *  \brief Transition matrix data structure.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \struct tmat_t
 * \brief Transition matrix data structure.  All phone HMMs are assumed to have the same
 * topology.
 */
typedef struct {
    uint8 ***tp;	/**< The transition matrices; kept in the same scale as acoustic scores;
			   tp[tmatid][from-state][to-state] */
    int16 n_tmat;	/**< Number matrices */
    int16 n_state;	/**< Number source states in matrix (only the emitting states);
			   Number destination states = n_state+1, it includes the exit state */
} tmat_t;


/** Initialize transition matrix */

tmat_t *tmat_init (char const *tmatfile,/**< In: input file */
		   logmath_t *lmath,    /**< In: log math parameters */
		   float64 tpfloor,	/**< In: floor value for each non-zero transition probability */
		   int32 breport      /**< In: whether reporting the process of tmat_t  */
    );
					    


/** Dumping the transition matrix for debugging */

void tmat_dump (tmat_t *tmat,  /**< In: transition matrix */
		FILE *fp       /**< In: file pointer */
    );	


/**
 * RAH, add code to remove memory allocated by tmat_init
 */

void tmat_free (tmat_t *t /**< In: transition matrix */
    );

/**
 * Report the detail of the transition matrix structure. 
 */
void tmat_report(tmat_t *t /**< In: transition matrix*/
    );

#ifdef __cplusplus
}
#endif

#endif
