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

PR_IMPLEMENT(PRIntn)
PL_strcmp(const char *a, const char *b)
{
    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    while( (*a == *b) && ('\0' != *a) )
    {
        a++;
        b++;
    }

    return (PRIntn)(*((const unsigned char *)a) - *((const unsigned char *)b));
}

PR_IMPLEMENT(PRIntn)
PL_strncmp(const char *a, const char *b, PRUint32 max)
{
    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    while( max && (*a == *b) && ('\0' != *a) )
    {
        a++;
        b++;
        max--;
    }

    if( 0 == max ) return (PRIntn)0;

    return (PRIntn)(*((const unsigned char *)a) - *((const unsigned char *)b));
}
