/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "plstr.h"

PR_IMPLEMENT(char *)
PL_strchr(const char *s, char c)
{
    if( (const char *)0 == s ) return (char *)0;

    for( ; *s; s++ )
        if( *s == c )
            return (char *)s;

    if( (char)0 == c ) return (char *)s;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strrchr(const char *s, char c)
{
    const char *p;

    if( (const char *)0 == s ) return (char *)0;

    for( p = s; *p; p++ )
        ;

    for( ; p >= s; p-- )
        if( *p == c )
            return (char *)p;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnchr(const char *s, char c, PRUint32 n)
{
    if( (const char *)0 == s ) return (char *)0;

    for( ; *s && n; s++, n-- )
        if( *s == c )
            return (char *)s;

    if( ((char)0 == c) && ((char)0 == *s) && (n > 0)) return (char *)s;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnrchr(const char *s, char c, PRUint32 n)
{
    const char *p;

    if( (const char *)0 == s ) return (char *)0;

    for( p = s; *p && n; p++, n-- )
        ;

    if( ((char)0 == c) && ((char)0 == *p) && (n > 0) ) return (char *)p;

    for( p--; p >= s; p-- )
        if( *p == c )
            return (char *)p;

    return (char *)0;
}
