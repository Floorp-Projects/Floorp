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
 * case.h -- Upper/lower case conversion routines
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: case.h,v $
 * Revision 1.7  2005/06/22 02:58:54  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 18-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Added strcmp_nocase, UPPER_CASE and LOWER_CASE definitions.
 * 
 * 16-Feb-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Created.
 */


/**
 * @file case.h
 * @brief Locale-independent implementation of case swapping operation. 
 *
 * This function implements ASCII-only case switching and comparison
 * related operations, which do not depend on the locale and are
 * guaranteed to exist on all versions of Windows.
 */

#ifndef _LIBUTIL_CASE_H_
#define _LIBUTIL_CASE_H_

#include <string.h>

#include <sphinxbase/prim_type.h>
#include <sphinxbase/sphinxbase_export.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

  /** 
   * Return upper case form for c 
   */
#define UPPER_CASE(c)	((((c) >= 'a') && ((c) <= 'z')) ? (c-32) : c)

  /**
   * Return lower case form for c 
   */
#define LOWER_CASE(c)	((((c) >= 'A') && ((c) <= 'Z')) ? (c+32) : c)


  /** 
   * Convert str to all upper case.
   * @param str is a string.
   */
SPHINXBASE_EXPORT
void ucase(char *str);

  /** 
   * Convert str to all lower case
   * @param str is a string.
   */
SPHINXBASE_EXPORT
void lcase(char *str);

  /**
   * (FIXME! The implementation is incorrect!) 
   * Case insensitive string compare.  Return the usual -1, 0, +1, depending on
   * str1 <, =, > str2 (case insensitive, of course).
   * @param str1 is the first string.
   * @param str2 is the second string. 
   */
SPHINXBASE_EXPORT
int32 strcmp_nocase(const char *str1, const char *str2);

/**
 * Like strcmp_nocase() but with a maximum length.
 */
SPHINXBASE_EXPORT
int32 strncmp_nocase(const char *str1, const char *str2, size_t len);


#ifdef __cplusplus
}
#endif

#endif
