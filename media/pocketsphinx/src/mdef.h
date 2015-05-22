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
 * mdef.h -- HMM model definition: base (CI) phones and triphones
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 */


#ifndef __MDEF_H__
#define __MDEF_H__


/* System headers. */
#include <stdio.h>

/* SphinxBase headers. */
#include <sphinxbase/hash_table.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \file mdef.h
 * \brief Model definition 
 */

/** \enum word_posn_t
 * \brief Union of different type of word position
 */

typedef enum {
    WORD_POSN_INTERNAL = 0,	/**< Internal phone of word */
    WORD_POSN_BEGIN = 1,	/**< Beginning phone of word */
    WORD_POSN_END = 2,		/**< Ending phone of word */
    WORD_POSN_SINGLE = 3,	/**< Single phone word (i.e. begin & end) */
    WORD_POSN_UNDEFINED = 4	/**< Undefined value, used for initial conditions, etc */
} word_posn_t;
#define N_WORD_POSN	4	/**< total # of word positions (excluding undefined) */
#define WPOS_NAME	"ibesu"	/**< Printable code for each word position above */
#define S3_SILENCE_CIPHONE "SIL" /**< Hard-coded silence CI phone name */

/**
   \struct ciphone_t
   \brief CI phone information 
*/
typedef struct {
    char *name;                 /**< The name of the CI phone */
    int32 filler;		/**< Whether a filler phone; if so, can be substituted by
				   silence phone in left or right context position */
} ciphone_t;

/**
 * \struct phone_t
 * \brief Triphone information, including base phones as a subset.  For the latter, lc, rc and wpos are non-existent.
 */
typedef struct {
    int32 ssid;			/**< State sequence (or senone sequence) ID, considering the
				   n_emit_state senone-ids are a unit.  The senone sequences
				   themselves are in a separate table */
    int32 tmat;			/**< Transition matrix id */
    int16 ci, lc, rc;		/**< Base, left, right context ciphones */
    word_posn_t wpos;		/**< Word position */
    
} phone_t;

/**
 * \struct ph_rc_t
 * \brief Structures needed for mapping <ci,lc,rc,wpos> into pid.  (See mdef_t.wpos_ci_lclist below.)  (lc = left context; rc = right context.)
 * NOTE: Both ph_rc_t and ph_lc_t FOR INTERNAL USE ONLY.
 */
typedef struct ph_rc_s {
    int16 rc;			/**< Specific rc for a parent <wpos,ci,lc> */
    int32 pid;			/**< Triphone id for above rc instance */
    struct ph_rc_s *next;	/**< Next rc entry for same parent <wpos,ci,lc> */
} ph_rc_t;

/**
 * \struct ph_lc_t
 * \brief Structures for storing the left context. 
 */

typedef struct ph_lc_s {
    int16 lc;			/**< Specific lc for a parent <wpos,ci> */
    ph_rc_t *rclist;		/**< rc list for above lc instance */
    struct ph_lc_s *next;	/**< Next lc entry for same parent <wpos,ci> */
} ph_lc_t;


/** The main model definition structure */
/**
   \struct mdef_t
   \brief strcture for storing the model definition. 
*/
typedef struct {
    int32 n_ciphone;		/**< number basephones actually present */
    int32 n_phone;		/**< number basephones + number triphones actually present */
    int32 n_emit_state;		/**< number emitting states per phone */
    int32 n_ci_sen;		/**< number CI senones; these are the first */
    int32 n_sen;		/**< number senones (CI+CD) */
    int32 n_tmat;		/**< number transition matrices */
    
    hash_table_t *ciphone_ht;	/**< Hash table for mapping ciphone strings to ids */
    ciphone_t *ciphone;		/**< CI-phone information for all ciphones */
    phone_t *phone;		/**< Information for all ciphones and triphones */
    uint16 **sseq;		/**< Unique state (or senone) sequences in this model, shared
                                   among all phones/triphones */
    int32 n_sseq;		/**< No. of unique senone sequences in this model */
    
    int16 *cd2cisen;		/**< Parent CI-senone id for each senone; the first
				   n_ci_sen are identity mappings; the CD-senones are
				   contiguous for each parent CI-phone */
    int16 *sen2cimap;		/**< Parent CI-phone for each senone (CI or CD) */
    
    int16 sil;			/**< SILENCE_CIPHONE id */
    
    ph_lc_t ***wpos_ci_lclist;	/**< wpos_ci_lclist[wpos][ci] = list of lc for <wpos,ci>.
                                   wpos_ci_lclist[wpos][ci][lc].rclist = list of rc for
                                   <wpos,ci,lc>.  Only entries for the known triphones
                                   are created to conserve space.
                                   (NOTE: FOR INTERNAL USE ONLY.) */
} mdef_t;

/** Access macros; not meant for arbitrary use */
#define mdef_is_fillerphone(m,p)	((m)->ciphone[p].filler)
#define mdef_n_ciphone(m)		((m)->n_ciphone)
#define mdef_n_phone(m)			((m)->n_phone)
#define mdef_n_sseq(m)			((m)->n_sseq)
#define mdef_n_emit_state(m)		((m)->n_emit_state)
#define mdef_n_sen(m)			((m)->n_sen)
#define mdef_n_tmat(m)			((m)->n_tmat)
#define mdef_pid2ssid(m,p)		((m)->phone[p].ssid)
#define mdef_pid2tmatid(m,p)		((m)->phone[p].tmat)
#define mdef_silphone(m)		((m)->sil)
#define mdef_sen2cimap(m)		((m)->sen2cimap)
#define mdef_sseq2sen(m,ss,pos)		((m)->sseq[ss][pos])
#define mdef_pid2ci(m,p)		((m)->phone[p].ci)
#define mdef_cd2cisen(m)		((m)->cd2cisen)

/**
 * Initialize the phone structure from the given model definition file.
 * It should be treated as a READ-ONLY structure.
 * @return pointer to the phone structure created.
 */
mdef_t *mdef_init (char *mdeffile, /**< In: Model definition file */
		   int breport     /**< In: whether to report the progress or not */
    );


/** 
    Get the ciphone id given a string name
    @return ciphone id for the given ciphone string name 
*/
int mdef_ciphone_id(mdef_t *m,		/**< In: Model structure being queried */
                    char *ciphone	/**< In: ciphone for which id wanted */
    );

/** 
    Get the phone string given the ci phone id.
    @return: READ-ONLY ciphone string name for the given ciphone id 
*/
const char *mdef_ciphone_str(mdef_t *m,	/**< In: Model structure being queried */
                             int ci	/**< In: ciphone id for which name wanted */
    );

/** 
    Decide whether the phone is ci phone.
    @return 1 if given triphone argument is a ciphone, 0 if not, -1 if error 
*/
int mdef_is_ciphone (mdef_t *m,		/**< In: Model structure being queried */
                     int p		/**< In: triphone id being queried */
    );

/**
   Decide whether the senone is a senone for a ci phone, or a ci senone
   @return 1 if a given senone is a ci senone
*/  
int mdef_is_cisenone(mdef_t *m,               /**< In: Model structure being queried */
                     int s		        /**< In: senone id being queried */
    );

/** 
    Decide the phone id given the left, right and base phones. 
    @return: phone id for the given constituents if found, else BAD_S3PID 
*/
int mdef_phone_id (mdef_t *m,		/**< In: Model structure being queried */
                   int b,		/**< In: base ciphone id */
                   int l,		/**< In: left context ciphone id */
                   int r,		/**< In: right context ciphone id */
                   word_posn_t pos	/**< In: Word position */
    );

/**
 * Create a phone string for the given phone (base or triphone) id in the given buf.
 * @return 0 if successful, -1 if error.
 */
int mdef_phone_str(mdef_t *m,		/**< In: Model structure being queried */
                   int pid,		/**< In: phone id being queried */
                   char *buf		/**< Out: On return, buf has the string */
    );

/**
 * Compare the underlying HMMs for two given phones (i.e., compare the two transition
 * matrix IDs and the individual state(senone) IDs).
 * @return 0 iff the HMMs are identical, -1 otherwise.
 */
int mdef_hmm_cmp (mdef_t *m,	/**< In: Model being queried */
                  int p1, 	/**< In: One of the two triphones being compared */
                  int p2	/**< In: One of the two triphones being compared */
    );

/** Report the model definition's parameters */
void mdef_report(mdef_t *m /**<  In: model definition structure */
    );

/** RAH, For freeing memory */
void mdef_free_recursive_lc (ph_lc_t *lc /**< In: A list of left context */
    );
void mdef_free_recursive_rc (ph_rc_t *rc /**< In: A list of right context */
    );

/** Free an mdef_t */
void mdef_free (mdef_t *mdef /**< In : The model definition*/
    );


#ifdef __cplusplus
}
#endif

#endif
