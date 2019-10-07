/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "prmem.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strdup(const char *s)
{
    char *rv;
    size_t n;

    if( (const char *)0 == s ) {
        s = "";
    }

    n = strlen(s) + 1;

    rv = (char *)malloc(n);
    if( (char *)0 == rv ) {
        return rv;
    }

    (void)memcpy(rv, s, n);

    return rv;
}

PR_IMPLEMENT(void)
PL_strfree(char *s)
{
    free(s);
}

PR_IMPLEMENT(char *)
PL_strndup(const char *s, PRUint32 max)
{
    char *rv;
    size_t l;

    if( (const char *)0 == s ) {
        s = "";
    }

    l = PL_strnlen(s, max);

    rv = (char *)malloc(l+1);
    if( (char *)0 == rv ) {
        return rv;
    }

    (void)memcpy(rv, s, l);
    rv[l] = '\0';

    return rv;
}
