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
 * case.c -- Upper/lower case conversion routines
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: case.c,v $
 * Revision 1.7  2005/06/22 02:58:54  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 18-Jun-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Added strcmp_nocase.  Moved UPPER_CASE and LOWER_CASE definitions to .h.
 * 
 * 16-Feb-97	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon
 * 		Created.
 */


#include <stdlib.h>
#include <assert.h>

#include "sphinxbase/case.h"
#include "sphinxbase/err.h"


void
lcase(register char *cp)
{
    if (cp) {
        while (*cp) {
            *cp = LOWER_CASE(*cp);
            cp++;
        }
    }
}

void
ucase(register char *cp)
{
    if (cp) {
        while (*cp) {
            *cp = UPPER_CASE(*cp);
            cp++;
        }
    }
}

int32
strcmp_nocase(const char *str1, const char *str2)
{
    char c1, c2;

    if (str1 == str2)
        return 0;
    if (str1 && str2) {
        for (;;) {
            c1 = *(str1++);
            c1 = UPPER_CASE(c1);
            c2 = *(str2++);
            c2 = UPPER_CASE(c2);
            if (c1 != c2)
                return (c1 - c2);
            if (c1 == '\0')
                return 0;
        }
    }
    else
        return (str1 == NULL) ? -1 : 1;

    return 0;
}

int32
strncmp_nocase(const char *str1, const char *str2, size_t len)
{
    char c1, c2;

    if (str1 && str2) {
        size_t n;

        for (n = 0; n < len; ++n) {
            c1 = *(str1++);
            c1 = UPPER_CASE(c1);
            c2 = *(str2++);
            c2 = UPPER_CASE(c2);
            if (c1 != c2)
                return (c1 - c2);
            if (c1 == '\0')
                return 0;
        }
    }
    else
        return (str1 == NULL) ? -1 : 1;

    return 0;
}
