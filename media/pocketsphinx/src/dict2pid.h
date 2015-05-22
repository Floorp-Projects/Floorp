/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2014 Carnegie Mellon University.  All rights
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

#ifndef _S3_DICT2PID_H_
#define _S3_DICT2PID_H_

/* System headers. */
#include <stdio.h>

/* SphinxBase headers. */
#include <sphinxbase/logmath.h>
#include <sphinxbase/bitvec.h>

/* Local headers. */
#include "s3types.h"
#include "bin_mdef.h"
#include "dict.h"

/** \file dict2pid.h
 * \brief Building triphones for a dictionary. 
 *
 * This is one of the more complicated parts of a cross-word
 * triphone model decoder.  The first and last phones of each word
 * get their left and right contexts, respectively, from other
 * words.  For single-phone words, both its contexts are from other
 * words, simultaneously.  As these words are not known beforehand,
 * life gets complicated.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \struct xwdssid_t
 * \brief cross word triphone model structure 
 */

typedef struct {
    s3ssid_t  *ssid;	/**< Senone Sequence ID list for all context ciphones */
    s3cipid_t *cimap;	/**< Index into ssid[] above for each ci phone */
    int32     n_ssid;	/**< #Unique ssid in above, compressed ssid list */
} xwdssid_t;

/**
   \struct dict2pid_t
   \brief Building composite triphone (as well as word internal triphones) with the dictionary. 
*/

typedef struct {
    int refcount;

    bin_mdef_t *mdef;           /**< Model definition, used to generate
                                   internal ssids on the fly. */
    dict_t *dict;               /**< Dictionary this table refers to. */

    /*Notice the order of the arguments */
    /* FIXME: This is crying out for compression - in Mandarin we have
     * 180 context independent phones, which makes this an 11MB
     * array. */
    s3ssid_t ***ldiph_lc;	/**< For multi-phone words, [base][rc][lc] -> ssid; filled out for
				   word-initial base x rc combinations in current vocabulary */


    xwdssid_t **rssid;          /**< Right context state sequence id table 
                                   First dimension: base phone,
                                   Second dimension: left context. 
                                */


    s3ssid_t ***lrdiph_rc;      /**< For single-phone words, [base][lc][rc] -> ssid; filled out for
                                   single-phone base x lc combinations in current vocabulary */

    xwdssid_t **lrssid;          /**< Left-Right context state sequence id table 
                                    First dimension: base phone,
                                    Second dimension: left context. 
                                 */
} dict2pid_t;

/** Access macros; not designed for arbitrary use */
#define dict2pid_rssid(d,ci,lc)  (&(d)->rssid[ci][lc])
#define dict2pid_ldiph_lc(d,b,r,l) ((d)->ldiph_lc[b][r][l])
#define dict2pid_lrdiph_rc(d,b,l,r) ((d)->lrdiph_rc[b][l][r])

/**
 * Build the dict2pid structure for the given model/dictionary
 */
dict2pid_t *dict2pid_build(bin_mdef_t *mdef,   /**< A  model definition*/
                           dict_t *dict        /**< An initialized dictionary */
    );

/**
 * Retain a pointer to dict2pid
 */
dict2pid_t *dict2pid_retain(dict2pid_t *d2p);  

/**
 * Free the memory dict2pid structure
 */
int dict2pid_free(dict2pid_t *d2p /**< In: the d2p */
    );

/**
 * Return the senone sequence ID for the given word position.
 */
s3ssid_t dict2pid_internal(dict2pid_t *d2p,
                           int32 wid,
                           int pos);

/**
 * Add a word to the dict2pid structure (after adding it to dict).
 */
int dict2pid_add_word(dict2pid_t *d2p,
                      int32 wid);

/**
 * For debugging
 */
void dict2pid_dump(FILE *fp,        /**< In: a file pointer */
                   dict2pid_t *d2p /**< In: a dict2pid_t structure */
    );

/** Report a dict2pid data structure */
void dict2pid_report(dict2pid_t *d2p /**< In: a dict2pid_t structure */
    );

/**
 * Get number of rc 
 */
int32 get_rc_nssid(dict2pid_t *d2p,  /**< In: a dict2pid */
		   s3wid_t w         /**< In: a wid */
    );

/**
 * Get RC map 
 */
s3cipid_t* dict2pid_get_rcmap(dict2pid_t *d2p,  /**< In: a dict2pid */
			      s3wid_t w        /**< In: a wid */
    );

#ifdef __cplusplus
}
#endif


#endif
