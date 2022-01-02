/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strcat(char *dest, const char *src)
{
    if( ((char *)0 == dest) || ((const char *)0 == src) ) {
        return dest;
    }

    return strcat(dest, src);
}

PR_IMPLEMENT(char *)
PL_strncat(char *dest, const char *src, PRUint32 max)
{
    char *rv;

    if( ((char *)0 == dest) || ((const char *)0 == src) || (0 == max) ) {
        return dest;
    }

    for( rv = dest; *dest; dest++ )
        ;

    (void)PL_strncpy(dest, src, max);
    return rv;
}

PR_IMPLEMENT(char *)
PL_strcatn(char *dest, PRUint32 max, const char *src)
{
    char *rv;
    PRUint32 dl;

    if( ((char *)0 == dest) || ((const char *)0 == src) ) {
        return dest;
    }

    for( rv = dest, dl = 0; *dest; dest++, dl++ )
        ;

    if( max <= dl ) {
        return rv;
    }
    (void)PL_strncpyz(dest, src, max-dl);

    return rv;
}
