/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2006 Carnegie Mellon University.  All rights
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
 * strfuncs.c -- String functions
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "sphinxbase/ckd_alloc.h"
#include "sphinxbase/strfuncs.h"

/* Defined in dtoa.c */
double sb_strtod(const char *s00, char **se);

double
atof_c(char const *str)
{
    return sb_strtod(str, NULL);
}

/* Locale-independent isspace to avoid different incompatibilities */
static int
isspace_c(char ch)
{
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
	return 1;
    return 0;
}

char *
string_join(const char *base, ...)
{
    va_list args;
    size_t len;
    const char *c;
    char *out;

    va_start(args, base);
    len = strlen(base);
    while ((c = va_arg(args, const char *)) != NULL) {
        len += strlen(c);
    }
    len++;
    va_end(args);

    out = ckd_calloc(len, 1);
    va_start(args, base);
    strcpy(out, base);
    while ((c = va_arg(args, const char *)) != NULL) {
        strcat(out, c);
    }
    va_end(args);

    return out;
}

char *
string_trim(char *string, enum string_edge_e which)
{
    size_t len;

    len = strlen(string);
    if (which == STRING_START || which == STRING_BOTH) {
        size_t sub = strspn(string, " \t\n\r\f");
        if (sub > 0) {
            memmove(string, string + sub, len + 1 - sub);
            len -= sub;
        }
    }
    if (which == STRING_END || which == STRING_BOTH) {
        long sub = len;
        while (--sub >= 0)
            if (strchr(" \t\n\r\f", string[sub]) == NULL)
                break;
        if (sub == -1)
            string[0] = '\0';
        else
            string[sub+1] = '\0';
    }
    return string;
}

int32
str2words(char *line, char **ptr, int32 max_ptr)
{
    int32 i, n;

    n = 0;                      /* #words found so far */
    i = 0;                      /* For scanning through the input string */
    while (1) {
        /* Skip whitespace before next word */
        while (line[i] && isspace_c(line[i]))
            ++i;
        if (!line[i])
            break;

        if (ptr != NULL && n >= max_ptr) {
            /*
             * Pointer array size insufficient.  Restore NULL chars inserted so far
             * to space chars.  Not a perfect restoration, but better than nothing.
             */
            for (; i >= 0; --i)
                if (line[i] == '\0')
                    line[i] = ' ';

            return -1;
        }

        /* Scan to end of word */
        if (ptr != NULL)
            ptr[n] = line + i;
        ++n;
        while (line[i] && !isspace_c(line[i]))
            ++i;
        if (!line[i])
            break;
        if (ptr != NULL)
            line[i] = '\0';
        ++i;
    }

    return n;
}


int32
nextword(char *line, const char *delim, char **word, char *delimfound)
{
    const char *d;
    char *w;

    /* Skip past any preceding delimiters */
    for (w = line; *w; w++) {
        for (d = delim; *d && (*d != *w); d++);
        if (!*d)
            break;
    }
    if (!*w)
        return -1;

    *word = w;                  /* Beginning of word */

    /* Skip until first delimiter char */
    for (w++; *w; w++) {
        for (d = delim; *d && (*d != *w); d++);
        if (*d)
            break;
    }

    /* Replace delimiter with NULL char, but return the original first */
    *delimfound = *w;
    *w = '\0';

    return (w - *word);
}
