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

#ifndef _S3_DICT_H_
#define _S3_DICT_H_

/** \file dict.h
 * \brief Operations on dictionary. 
 */

/* SphinxBase headers. */
#include <sphinxbase/hash_table.h>
#include <sphinxbase/ngram_model.h>

/* Local headers. */
#include "s3types.h"
#include "bin_mdef.h"
#include "pocketsphinx_export.h"

#define S3DICT_INC_SZ 4096

#ifdef __cplusplus
extern "C" {
#endif

/** 
    \struct dictword_t
    \brief a structure for one dictionary word. 
*/
typedef struct {
    char *word;		/**< Ascii word string */
    s3cipid_t *ciphone;	/**< Pronunciation */
    int32 pronlen;	/**< Pronunciation length */
    s3wid_t alt;	/**< Next alternative pronunciation id, NOT_S3WID if none */
    s3wid_t basewid;	/**< Base pronunciation id */
} dictword_t;

/** 
    \struct dict_t
    \brief a structure for a dictionary. 
*/

typedef struct {
    int refcnt;
    bin_mdef_t *mdef;	/**< Model definition used for phone IDs; NULL if none used */
    dictword_t *word;	/**< Array of entries in dictionary */
    hash_table_t *ht;	/**< Hash table for mapping word strings to word ids */
    int32 max_words;	/**< #Entries allocated in dict, including empty slots */
    int32 n_word;	/**< #Occupied entries in dict; ie, excluding empty slots */
    int32 filler_start;	/**< First filler word id (read from filler dict) */
    int32 filler_end;	/**< Last filler word id (read from filler dict) */
    s3wid_t startwid;	/**< FOR INTERNAL-USE ONLY */
    s3wid_t finishwid;	/**< FOR INTERNAL-USE ONLY */
    s3wid_t silwid;	/**< FOR INTERNAL-USE ONLY */
    int nocase;
    ngram_model_t *ngram_g2p_model;
} dict_t;

struct winner_t
{
    size_t length_match;
    int winner_wid;
    size_t len_phoneme;
};

typedef struct
{
    char *word;
    char *phone;
} unigram_t;

/**
 * Initialize a new dictionary.
 *
 * If config and mdef are supplied, then the dictionary will be read
 * from the files specified by the -dict and -fdict options in config,
 * with case sensitivity determined by the -dictcase option.
 *
 * Otherwise an empty case-sensitive dictionary will be created.
 *
 * Return ptr to dict_t if successful, NULL otherwise.
 */
dict_t *dict_init(cmd_ln_t *config, /**< Configuration (-dict, -fdict, -dictcase) or NULL */
                  bin_mdef_t *mdef,  /**< For looking up CI phone IDs (or NULL) */
                  logmath_t *logmath // To load ngram_model for g2p load. logmath must be retained with logmath_retain() if it is to be used elsewhere.
    );

/**
 * Write dictionary to a file.
 */
int dict_write(dict_t *dict, char const *filename, char const *format);

/** Return word id for given word string if present.  Otherwise return BAD_S3WID */
POCKETSPHINX_EXPORT
s3wid_t dict_wordid(dict_t *d, const char *word);

/**
 * Return 1 if w is a filler word, 0 if not.  A filler word is one that was read in from the
 * filler dictionary; however, sentence START and FINISH words are not filler words.
 */
int dict_filler_word(dict_t *d,  /**< The dictionary structure */
                     s3wid_t w     /**< The word ID */
    );

/**
 * Test if w is a "real" word, i.e. neither a filler word nor START/FINISH.
 */
POCKETSPHINX_EXPORT
int dict_real_word(dict_t *d,  /**< The dictionary structure */
                   s3wid_t w     /**< The word ID */
    );

/**
 * Add a word with the given ciphone pronunciation list to the dictionary.
 * Return value: Result word id if successful, BAD_S3WID otherwise
 */
s3wid_t dict_add_word(dict_t *d,          /**< The dictionary structure. */
                      char const *word,   /**< The word. */
                      s3cipid_t const *p, /**< The pronunciation. */
                      int32 np            /**< Number of phones. */
    );

/**
 * Return value: CI phone string for the given word, phone position.
 */
const char *dict_ciphone_str(dict_t *d,	/**< In: Dictionary to look up */
                             s3wid_t wid,	/**< In: Component word being looked up */
                             int32 pos   	/**< In: Pronunciation phone position */
    );

/** Packaged macro access to dictionary members */
#define dict_size(d)		((d)->n_word)
#define dict_num_fillers(d)   (dict_filler_end(d) - dict_filler_start(d))
/**
 * Number of "real words" in the dictionary.
 *
 * This is the number of words that are not fillers, <s>, or </s>.
 */
#define dict_num_real_words(d)                                          \
    (dict_size(d) - (dict_filler_end(d) - dict_filler_start(d)) - 2)
#define dict_basewid(d,w)	((d)->word[w].basewid)
#define dict_wordstr(d,w)	((w) < 0 ? NULL : (d)->word[w].word)
#define dict_basestr(d,w)	((d)->word[dict_basewid(d,w)].word)
#define dict_nextalt(d,w)	((d)->word[w].alt)
#define dict_pronlen(d,w)	((d)->word[w].pronlen) 
#define dict_pron(d,w,p)	((d)->word[w].ciphone[p]) /**< The CI phones of the word w at position p */
#define dict_filler_start(d)	((d)->filler_start)
#define dict_filler_end(d)	((d)->filler_end)
#define dict_startwid(d)	((d)->startwid)
#define dict_finishwid(d)	((d)->finishwid)
#define dict_silwid(d)		((d)->silwid)
#define dict_is_single_phone(d,w)	((d)->word[w].pronlen == 1)
#define dict_first_phone(d,w)	((d)->word[w].ciphone[0])
#define dict_second_phone(d,w)	((d)->word[w].ciphone[1])
#define dict_second_last_phone(d,w)	((d)->word[w].ciphone[(d)->word[w].pronlen - 2])
#define dict_last_phone(d,w)	((d)->word[w].ciphone[(d)->word[w].pronlen - 1])

/* Hard-coded special words */
#define S3_START_WORD		"<s>"
#define S3_FINISH_WORD		"</s>"
#define S3_SILENCE_WORD		"<sil>"
#define S3_UNKNOWN_WORD		"<UNK>"

/**
 * If the given word contains a trailing "(....)" (i.e., a Sphinx-II style alternative
 * pronunciation specification), strip that trailing portion from it.  Note that the given
 * string is modified.
 * Return value: If string was modified, the character position at which the original string
 * was truncated; otherwise -1.
 */
int32 dict_word2basestr(char *word);

/**
 * Retain a pointer to an dict_t.
 */
dict_t *dict_retain(dict_t *d);

/**
 * Release a pointer to a dictionary.
 */
int dict_free(dict_t *d);

/** Report a dictionary structure */
void dict_report(dict_t *d /**< A dictionary structure */
    );

// g2p functions
int dict_add_g2p_word(dict_t * dict, char const *word);

#ifdef __cplusplus
}
#endif

#endif
