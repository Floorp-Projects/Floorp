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
 * ms_mgau.h -- Essentially a wrapper that wrap up gauden and
 * senone. It supports multi-stream. 
 *
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * HISTORY
 * $Log$
 * Revision 1.1  2006/04/05  20:27:30  dhdfu
 * A Great Reorganzation of header files and executables
 * 
 * Revision 1.3  2006/02/22 16:57:15  arthchan2003
 * Fixed minor dox-doc issue
 *
 * Revision 1.2  2006/02/22 16:56:01  arthchan2003
 * Merged from SPHINX3_5_2_RCI_IRII_BRANCH: Added ms_mgau.[ch] into the trunk. It is a wrapper of ms_gauden and ms_senone
 *
 * Revision 1.1.2.4  2005/09/25 18:55:19  arthchan2003
 * Added a flag to turn on and off precomputation.
 *
 * Revision 1.1.2.3  2005/08/03 18:53:44  dhdfu
 * Add memory deallocation functions.  Also move all the initialization
 * of ms_mgau_model_t into ms_mgau_init (duh!), which entails removing it
 * from decode_anytopo and friends.
 *
 * Revision 1.1.2.2  2005/08/02 21:05:38  arthchan2003
 * 1, Added dist and mgau_active as intermediate variable for computation. 2, Added ms_cont_mgau_frame_eval, which is a multi stream version of GMM computation mainly s3.0 family of tools. 3, Fixed dox-doc.
 *
 * Revision 1.1.2.1  2005/07/20 19:37:09  arthchan2003
 * Added a multi-stream cont_mgau (ms_mgau) which is a wrapper of both gauden and senone.  Add ms_mgau_init and model_set_mllr.  This allow eliminating 600 lines of code in decode_anytopo/align/allphone.
 *
 *
 *
 */

/** \file ms_mgau.h
 *
 * \brief (Sphinx 3.0 specific) A module that wraps up the code of
 * gauden and senone because they are closely related.  
 *
 * At the time at Sphinx 3.1 to 3.2, Ravi has decided to rewrite only
 * single-stream part of the code into cont_mgau.[ch].  This marks the
 * beginning of historical problem of having two sets of Gaussian
 * distribution computation routine, one for single-stream and one of
 * multi-stream.
 *
 * In Sphinx 3.5, when we figure out that it is possible to allow both
 * 3.0 family of tools and 3.x family of tools to coexist.  This
 * becomes one problem we found that very hard to reconcile.  That is
 * why we currently allow two versions of the code in the code
 * base. This is likely to change in the future.
 */


#ifndef _LIBFBS_MS_CONT_MGAU_H_
#define _LIBFBS_MS_CONT_MGAU_H_

/* SphinxBase headers. */
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/logmath.h>
#include <sphinxbase/feat.h>

/* Local headers. */
#include "acmod.h"
#include "bin_mdef.h"
#include "ms_gauden.h"
#include "ms_senone.h"

/** \struct ms_mgau_t
    \brief Multi-stream mixture gaussian. It is not necessary to be continr
*/

typedef struct {
    ps_mgau_t base;
    gauden_t* g;   /**< The codebook */
    senone_t* s;   /**< The senone */
    int topn;      /**< Top-n gaussian will be computed */

    /**< Intermediate used in computation */
    gauden_dist_t ***dist;  
    uint8 *mgau_active;
    cmd_ln_t *config;
} ms_mgau_model_t;  

#define ms_mgau_gauden(msg) (msg->g)
#define ms_mgau_senone(msg) (msg->s)
#define ms_mgau_topn(msg) (msg->topn)

ps_mgau_t* ms_mgau_init(acmod_t *acmod, logmath_t *lmath, bin_mdef_t *mdef);
void ms_mgau_free(ps_mgau_t *g);
int32 ms_cont_mgau_frame_eval(ps_mgau_t * msg,
                              int16 *senscr,
                              uint8 *senone_active,
                              int32 n_senone_active,
                              mfcc_t ** feat,
                              int32 frame,
                              int32 compallsen);
int32 ms_mgau_mllr_transform(ps_mgau_t *s,
                             ps_mllr_t *mllr);

#endif /* _LIBFBS_MS_CONT_MGAU_H_*/

