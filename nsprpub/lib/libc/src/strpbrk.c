/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strpbrk(const char *s, const char *list)
{
    if( ((const char *)0 == s) || ((const char *)0 == list) ) {
        return (char *)0;
    }

    return strpbrk(s, list);
}

PR_IMPLEMENT(char *)
PL_strprbrk(const char *s, const char *list)
{
    const char *p;
    const char *r;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) {
        return (char *)0;
    }

    for( r = s; *r; r++ )
        ;

    for( r--; r >= s; r-- )
        for( p = list; *p; p++ )
            if( *r == *p ) {
                return (char *)r;
            }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnpbrk(const char *s, const char *list, PRUint32 max)
{
    const char *p;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) {
        return (char *)0;
    }

    for( ; max && *s; s++, max-- )
        for( p = list; *p; p++ )
            if( *s == *p ) {
                return (char *)s;
            }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnprbrk(const char *s, const char *list, PRUint32 max)
{
    const char *p;
    const char *r;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) {
        return (char *)0;
    }

    for( r = s; max && *r; r++, max-- )
        ;

    for( r--; r >= s; r-- )
        for( p = list; *p; p++ )
            if( *r == *p ) {
                return (char *)r;
            }

    return (char *)0;
}
