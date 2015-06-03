/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1995-2004 Carnegie Mellon University.  All rights
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
/**
 * @file strfuncs.h
 * @brief Miscellaneous useful string functions
 */

#ifndef __SB_STRFUNCS_H__
#define __SB_STRFUNCS_H__

#include <stdarg.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>


#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Concatenate a NULL-terminated argument list of strings, returning a
 * newly allocated string.
 **/
SPHINXBASE_EXPORT
char *string_join(const char *base, ...);

/**
 * Which end of a string to operate on for string_trim().
 */
enum string_edge_e {
    STRING_START,	/**< Beginning of string. */
    STRING_END,		/**< End of string. */
    STRING_BOTH		/**< Both ends of string. */
};

/**
 * Remove whitespace from a string, modifying it in-place.
 *
 * @param string string to trim, contents will be modified.
 * @param which one of STRING_START, STRING_END, or STRING_BOTH.
 */
SPHINXBASE_EXPORT
char *string_trim(char *string, enum string_edge_e which);

/**
 * Locale independent version of atof().
 *
 * This function behaves like atof() in the "C" locale.  Switching
 * locale in a threaded program is extremely uncool, therefore we need
 * this since we pass floats as strings in 1000 different places.
 */
SPHINXBASE_EXPORT
double atof_c(char const *str);

/* FIXME: Both of these string splitting functions basically suck.  I
 have attempted to fix them as best I can.  (dhuggins@cs, 20070808) */

/** 
 * Convert a line to an array of "words", based on whitespace separators.  A word
 * is a string with no whitespace chars in it.
 * Note that the string line is modified as a result: NULL chars are placed after
 * every word in the line.
 * Return value: No. of words found; -1 if no. of words in line exceeds n_wptr.
 */
SPHINXBASE_EXPORT
int32 str2words (char *line,	/**< In/Out: line to be parsed.  This
				   string will be modified! (NUL
				   characters inserted at word
				   boundaries) */
		 char **wptr,	/**< In/Out: Array of pointers to
				   words found in line.  The array
				   must be allocated by the caller.
				   It may be NULL in which case the
				   number of words will be counted.
				   This allows you to allcate it to
				   the proper size, e.g.:
				   
				   n = str2words(line, NULL, 0);
				   wptr = ckd_calloc(n, sizeof(*wptr));
				   str2words(line, wptr, n);
				*/
		 int32 n_wptr	/**< In: Size of wptr array, ignored
				   if wptr == NULL */
	);

/**
 * Yet another attempt at a clean "next-word-in-string" function.  See arguments below.
 * @return Length of word returned, or -1 if nothing found.
 * This allows you to scan through a line:
 *
 * <pre>
 * while ((n = nextword(line, delim, &word, &delimfound)) >= 0) {
 *     ... do something with word ..
 *     word[n] = delimfound;
 *     line = word + n;
 * }
 * </pre>
 */
SPHINXBASE_EXPORT
int32 nextword (char *line, /**< Input: String being searched for next word.
			       Will be modified by this function (NUL characters inserted) */
		const char *delim, /**< Input: A word, if found, must be delimited at either
			         end by a character from this string (or at the end
			         by the NULL char) */
		char **word,/**< Output: *word = ptr within line to beginning of first
			         word, if found.  Delimiter at the end of word replaced
			         with the NULL char. */
		char *delimfound /**< Output: *delimfound = original delimiter found at the end
				    of the word.  (This way, the caller can restore the
				    delimiter, preserving the original string.) */
	);

#ifdef __cplusplus
}
#endif


#endif /* __SB_STRFUNCS_H__ */
