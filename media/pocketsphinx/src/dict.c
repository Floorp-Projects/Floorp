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

/* System headers. */
#include <string.h>

/* SphinxBase headers. */
#include <sphinxbase/pio.h>
#include <sphinxbase/strfuncs.h>

/* Local headers. */
#include "dict.h"


#define DELIM	" \t\n"         /* Set of field separator characters */
#define DEFAULT_NUM_PHONE	(MAX_S3CIPID+1)

#if WIN32
#define snprintf sprintf_s
#endif 

extern const char *const cmu6_lts_phone_table[];

static s3cipid_t
dict_ciphone_id(dict_t * d, const char *str)
{
    if (d->nocase)
        return bin_mdef_ciphone_id_nocase(d->mdef, str);
    else
        return bin_mdef_ciphone_id(d->mdef, str);
}


const char *
dict_ciphone_str(dict_t * d, s3wid_t wid, int32 pos)
{
    assert(d != NULL);
    assert((wid >= 0) && (wid < d->n_word));
    assert((pos >= 0) && (pos < d->word[wid].pronlen));

    return bin_mdef_ciphone_str(d->mdef, d->word[wid].ciphone[pos]);
}


s3wid_t
dict_add_word(dict_t * d, char const *word, s3cipid_t const * p, int32 np)
{
    int32 len;
    dictword_t *wordp;
    s3wid_t newwid;
    char *wword;

    if (d->n_word >= d->max_words) {
        E_INFO("Reallocating to %d KiB for word entries\n",
               (d->max_words + S3DICT_INC_SZ) * sizeof(dictword_t) / 1024);
        d->word =
            (dictword_t *) ckd_realloc(d->word,
                                       (d->max_words +
                                        S3DICT_INC_SZ) * sizeof(dictword_t));
        d->max_words = d->max_words + S3DICT_INC_SZ;
    }

    wordp = d->word + d->n_word;
    wordp->word = (char *) ckd_salloc(word);    /* Freed in dict_free */

    /* Determine base/alt wids */
    wword = ckd_salloc(word);
    if ((len = dict_word2basestr(wword)) > 0) {
        int32 w;

        /* Truncated to a baseword string; find its ID */
        if (hash_table_lookup_int32(d->ht, wword, &w) < 0) {
            E_ERROR("Missing base word for: %s\n", word);
            ckd_free(wword);
            ckd_free(wordp->word);
            wordp->word = NULL;
            return BAD_S3WID;
        }

        /* Link into alt list */
        wordp->basewid = w;
        wordp->alt = d->word[w].alt;
        d->word[w].alt = d->n_word;
    } else {
        wordp->alt = BAD_S3WID;
        wordp->basewid = d->n_word;
    }
    ckd_free(wword);

    /* Associate word string with d->n_word in hash table */
    if (hash_table_enter_int32(d->ht, wordp->word, d->n_word) != d->n_word) {
        ckd_free(wordp->word);
        wordp->word = NULL;
        return BAD_S3WID;
    }

    /* Fill in word entry, and set defaults */
    if (p && (np > 0)) {
        wordp->ciphone = (s3cipid_t *) ckd_malloc(np * sizeof(s3cipid_t));      /* Freed in dict_free */
        memcpy(wordp->ciphone, p, np * sizeof(s3cipid_t));
        wordp->pronlen = np;
    }
    else {
        wordp->ciphone = NULL;
        wordp->pronlen = 0;
    }

    newwid = d->n_word++;

    return newwid;
}


static int32
dict_read(FILE * fp, dict_t * d)
{
    lineiter_t *li;
    char **wptr;
    s3cipid_t *p;
    int32 lineno, nwd;
    s3wid_t w;
    int32 i, maxwd;
    size_t stralloc, phnalloc;

    maxwd = 512;
    p = (s3cipid_t *) ckd_calloc(maxwd + 4, sizeof(*p));
    wptr = (char **) ckd_calloc(maxwd, sizeof(char *)); /* Freed below */

    lineno = 0;
    stralloc = phnalloc = 0;
    for (li = lineiter_start(fp); li; li = lineiter_next(li)) {
        lineno++;
        if (0 == strncmp(li->buf, "##", 2)
            || 0 == strncmp(li->buf, ";;", 2))
            continue;

        if ((nwd = str2words(li->buf, wptr, maxwd)) < 0) {
            /* Increase size of p, wptr. */
            nwd = str2words(li->buf, NULL, 0);
            assert(nwd > maxwd); /* why else would it fail? */
            maxwd = nwd;
            p = (s3cipid_t *) ckd_realloc(p, (maxwd + 4) * sizeof(*p));
            wptr = (char **) ckd_realloc(wptr, maxwd * sizeof(*wptr));
        }

        if (nwd == 0)           /* Empty line */
            continue;
        /* wptr[0] is the word-string and wptr[1..nwd-1] the pronunciation sequence */
        if (nwd == 1) {
            E_ERROR("Line %d: No pronunciation for word '%s'; ignored\n",
                    lineno, wptr[0]);
            continue;
        }


        /* Convert pronunciation string to CI-phone-ids */
        for (i = 1; i < nwd; i++) {
            p[i - 1] = dict_ciphone_id(d, wptr[i]);
            if (NOT_S3CIPID(p[i - 1])) {
                E_ERROR("Line %d: Phone '%s' is mising in the acoustic model; word '%s' ignored\n",
                        lineno, wptr[i], wptr[0]);
                break;
            }
        }

        if (i == nwd) {         /* All CI-phones successfully converted to IDs */
            w = dict_add_word(d, wptr[0], p, nwd - 1);
            if (NOT_S3WID(w))
                E_ERROR
                    ("Line %d: Failed to add the word '%s' (duplicate?); ignored\n",
                     lineno, wptr[0]);
            else {
                stralloc += strlen(d->word[w].word);
                phnalloc += d->word[w].pronlen * sizeof(s3cipid_t);
            }
        }
    }
    E_INFO("Allocated %d KiB for strings, %d KiB for phones\n",
           (int)stralloc / 1024, (int)phnalloc / 1024);
    ckd_free(p);
    ckd_free(wptr);

    return 0;
}

int
dict_write(dict_t *dict, char const *filename, char const *format)
{
    FILE *fh;
    int i;

    if ((fh = fopen(filename, "w")) == NULL) {
        E_ERROR_SYSTEM("Failed to open '%s'", filename);
        return -1;
    }
    for (i = 0; i < dict->n_word; ++i) {
        char *phones;
        int j, phlen;
        if (!dict_real_word(dict, i))
            continue;
        for (phlen = j = 0; j < dict_pronlen(dict, i); ++j)
            phlen += strlen(dict_ciphone_str(dict, i, j)) + 1;
        phones = ckd_calloc(1, phlen);
        for (j = 0; j < dict_pronlen(dict, i); ++j) {
            strcat(phones, dict_ciphone_str(dict, i, j));
            if (j != dict_pronlen(dict, i) - 1)
                strcat(phones, " ");
        }
        fprintf(fh, "%-30s %s\n", dict_wordstr(dict, i), phones);
        ckd_free(phones);
    }
    fclose(fh);
    return 0;
}


dict_t *
dict_init(cmd_ln_t *config, bin_mdef_t * mdef)
{
    FILE *fp, *fp2;
    int32 n;
    lineiter_t *li;
    dict_t *d;
    s3cipid_t sil;
    char const *dictfile = NULL, *fillerfile = NULL;

    if (config) {
        dictfile = cmd_ln_str_r(config, "-dict");
        fillerfile = cmd_ln_str_r(config, "-fdict");
    }

    /*
     * First obtain #words in dictionary (for hash table allocation).
     * Reason: The PC NT system doesn't like to grow memory gradually.  Better to allocate
     * all the required memory in one go.
     */
    fp = NULL;
    n = 0;
    if (dictfile) {
        if ((fp = fopen(dictfile, "r")) == NULL) {
            E_ERROR_SYSTEM("Failed to open dictionary file '%s' for reading", dictfile);
            return NULL;
        }
        for (li = lineiter_start(fp); li; li = lineiter_next(li)) {
            if (0 != strncmp(li->buf, "##", 2)
                && 0 != strncmp(li->buf, ";;", 2))
                n++;
        }
	fseek(fp, 0L, SEEK_SET);
    }

    fp2 = NULL;
    if (fillerfile) {
        if ((fp2 = fopen(fillerfile, "r")) == NULL) {
            E_ERROR_SYSTEM("Failed to open filler dictionary file '%s' for reading", fillerfile);
            fclose(fp);
            return NULL;
	}
        for (li = lineiter_start(fp2); li; li = lineiter_next(li)) {
	    if (0 != strncmp(li->buf, "##", 2)
    	        && 0 != strncmp(li->buf, ";;", 2))
                n++;
        }
        fseek(fp2, 0L, SEEK_SET);
    }

    /*
     * Allocate dict entries.  HACK!!  Allow some extra entries for words not in file.
     * Also check for type size restrictions.
     */
    d = (dict_t *) ckd_calloc(1, sizeof(dict_t));       /* freed in dict_free() */
    d->refcnt = 1;
    d->max_words =
        (n + S3DICT_INC_SZ < MAX_S3WID) ? n + S3DICT_INC_SZ : MAX_S3WID;
    if (n >= MAX_S3WID) {
        E_ERROR("Number of words in dictionaries (%d) exceeds limit (%d)\n", n,
                MAX_S3WID);
        fclose(fp);
        fclose(fp2);
        ckd_free(d);
        return NULL;
    }

    E_INFO("Allocating %d * %d bytes (%d KiB) for word entries\n",
           d->max_words, sizeof(dictword_t),
           d->max_words * sizeof(dictword_t) / 1024);
    d->word = (dictword_t *) ckd_calloc(d->max_words, sizeof(dictword_t));      /* freed in dict_free() */
    d->n_word = 0;
    if (mdef)
        d->mdef = bin_mdef_retain(mdef);

    /* Create new hash table for word strings; case-insensitive word strings */
    if (config && cmd_ln_exists_r(config, "-dictcase"))
        d->nocase = cmd_ln_boolean_r(config, "-dictcase");
    d->ht = hash_table_new(d->max_words, d->nocase);

    /* Digest main dictionary file */
    if (fp) {
        E_INFO("Reading main dictionary: %s\n", dictfile);
        dict_read(fp, d);
        fclose(fp);
        E_INFO("%d words read\n", d->n_word);
    }

    /* Now the filler dictionary file, if it exists */
    d->filler_start = d->n_word;
    if (fillerfile) {
        E_INFO("Reading filler dictionary: %s\n", fillerfile);
        dict_read(fp2, d);
        fclose(fp2);
        E_INFO("%d words read\n", d->n_word - d->filler_start);
    }
    if (mdef)
        sil = bin_mdef_silphone(mdef);
    else
        sil = 0;
    if (dict_wordid(d, S3_START_WORD) == BAD_S3WID) {
        dict_add_word(d, S3_START_WORD, &sil, 1);
    }
    if (dict_wordid(d, S3_FINISH_WORD) == BAD_S3WID) {
        dict_add_word(d, S3_FINISH_WORD, &sil, 1);
    }
    if (dict_wordid(d, S3_SILENCE_WORD) == BAD_S3WID) {
        dict_add_word(d, S3_SILENCE_WORD, &sil, 1);
    }

    d->filler_end = d->n_word - 1;

    /* Initialize distinguished word-ids */
    d->startwid = dict_wordid(d, S3_START_WORD);
    d->finishwid = dict_wordid(d, S3_FINISH_WORD);
    d->silwid = dict_wordid(d, S3_SILENCE_WORD);

    if ((d->filler_start > d->filler_end)
        || (!dict_filler_word(d, d->silwid))) {
        E_ERROR("Word '%s' must occur (only) in filler dictionary\n",
                S3_SILENCE_WORD);
        dict_free(d);
        return NULL;
    }

    /* No check that alternative pronunciations for filler words are in filler range!! */

    return d;
}


s3wid_t
dict_wordid(dict_t *d, const char *word)
{
    int32 w;

    assert(d);
    assert(word);

    if (hash_table_lookup_int32(d->ht, word, &w) < 0)
        return (BAD_S3WID);
    return w;
}


int
dict_filler_word(dict_t *d, s3wid_t w)
{
    assert(d);
    assert((w >= 0) && (w < d->n_word));

    w = dict_basewid(d, w);
    if ((w == d->startwid) || (w == d->finishwid))
        return 0;
    if ((w >= d->filler_start) && (w <= d->filler_end))
        return 1;
    return 0;
}

int
dict_real_word(dict_t *d, s3wid_t w)
{
    assert(d);
    assert((w >= 0) && (w < d->n_word));

    w = dict_basewid(d, w);
    if ((w == d->startwid) || (w == d->finishwid))
        return 0;
    if ((w >= d->filler_start) && (w <= d->filler_end))
        return 0;
    return 1;
}


int32
dict_word2basestr(char *word)
{
    int32 i, len;

    len = strlen(word);
    if (word[len - 1] == ')') {
        for (i = len - 2; (i > 0) && (word[i] != '('); --i);

        if (i > 0) {
            /* The word is of the form <baseword>(...); strip from left-paren */
            word[i] = '\0';
            return i;
        }
    }

    return -1;
}

dict_t *
dict_retain(dict_t *d)
{
    ++d->refcnt;
    return d;
}

int
dict_free(dict_t * d)
{
    int i;
    dictword_t *word;

    if (d == NULL)
        return 0;
    if (--d->refcnt > 0)
        return d->refcnt;

    /* First Step, free all memory allocated for each word */
    for (i = 0; i < d->n_word; i++) {
        word = (dictword_t *) & (d->word[i]);
        if (word->word)
            ckd_free((void *) word->word);
        if (word->ciphone)
            ckd_free((void *) word->ciphone);
    }

    if (d->word)
        ckd_free((void *) d->word);
    if (d->ht)
        hash_table_free(d->ht);
    if (d->mdef)
        bin_mdef_free(d->mdef);
    ckd_free((void *) d);

    return 0;
}

void
dict_report(dict_t * d)
{
    E_INFO_NOFN("Initialization of dict_t, report:\n");
    E_INFO_NOFN("Max word: %d\n", d->max_words);
    E_INFO_NOFN("No of word: %d\n", d->n_word);
    E_INFO_NOFN("\n");
}
