/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strchr(const char *s, char c)
{
    if( (const char *)0 == s ) return (char *)0;

    return strchr(s, c);
}

PR_IMPLEMENT(char *)
PL_strrchr(const char *s, char c)
{
    if( (const char *)0 == s ) return (char *)0;

    return strrchr(s, c);
}

PR_IMPLEMENT(char *)
PL_strnchr(const char *s, char c, PRUint32 n)
{
    if( (const char *)0 == s ) return (char *)0;

    for( ; n && *s; s++, n-- )
        if( *s == c )
            return (char *)s;

    if( ((char)0 == c) && (n > 0) && ((char)0 == *s) ) return (char *)s;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnrchr(const char *s, char c, PRUint32 n)
{
    const char *p;

    if( (const char *)0 == s ) return (char *)0;

    for( p = s; n && *p; p++, n-- )
        ;

    if( ((char)0 == c) && (n > 0) && ((char)0 == *p) ) return (char *)p;

    for( p--; p >= s; p-- )
        if( *p == c )
            return (char *)p;

    return (char *)0;
}
