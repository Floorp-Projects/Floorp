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
PL_strpbrk(const char *s, const char *list)
{
    const char *p;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) return (char *)0;

    for( ; *s; s++ )
        for( p = list; *p; p++ )
            if( *s == *p )
                return (char *)s;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strprbrk(const char *s, const char *list)
{
    const char *p;
    const char *r;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) return (char *)0;

    for( r = s; *r; r++ )
        ;

    for( r--; r >= s; r-- )
        for( p = list; *p; p++ )
            if( *r == *p )
                return (char *)r;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnpbrk(const char *s, const char *list, PRUint32 max)
{
    const char *p;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) return (char *)0;

    for( ; *s && max; s++, max-- )
        for( p = list; *p; p++ )
            if( *s == *p )
                return (char *)s;

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnprbrk(const char *s, const char *list, PRUint32 max)
{
    const char *p;
    const char *r;

    if( ((const char *)0 == s) || ((const char *)0 == list) ) return (char *)0;

    for( r = s; *r && max; r++, max-- )
        ;

    for( r--; r >= s; r-- )
        for( p = list; *p; p++ )
            if( *r == *p )
                return (char *)r;

    return (char *)0;
}
