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
PL_strcat(char *dest, const char *src)
{
    char *rv;
    
    if( (char *)0 == dest ) return (char *)0;
    if( (const char *)0 == src ) return dest;

    for( rv = dest; *dest; dest++ )
        ;

    for( ; ((*dest = *src) != 0); dest++, src++ )
        ;

    return rv;
}

PR_IMPLEMENT(char *)
PL_strncat(char *dest, const char *src, PRUint32 max)
{
    char *rv;

    if( (char *)0 == dest ) return (char *)0;
    if( (const char *)0 == src ) return dest;
    if( 0 == max ) return dest;

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

    if( (char *)0 == dest ) return (char *)0;
    if( (const char *)0 == src ) return dest;

    for( rv = dest, dl = 0; *dest; dest++, dl++ )
        ;

    if( max <= dl ) return rv;
    (void)PL_strncpyz(dest, src, max-dl);

    return rv;
}
