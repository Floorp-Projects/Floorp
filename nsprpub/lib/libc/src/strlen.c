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
#include "prtypes.h"
#include "prlog.h"

PR_IMPLEMENT(PRUint32)
PL_strlen(const char *str)
{
    register const char *s;

    if( (const char *)0 == str ) return 0;
    for( s = str; *s; s++ )
        ;
/* error checking in case we have a 64-bit platform -- make sure we dont
 * have ultra long strings that overflow a int32
 */ 
    if (sizeof(PRUint32) < sizeof(PRUptrdiff))
        PR_ASSERT((s-str) < 2147483647);

    return (PRUint32)(s - str);
}

PR_IMPLEMENT(PRUint32)
PL_strnlen(const char *str, PRUint32 max)
{
    register const char *s;

    if( (const char *)0 == str ) return 0;
    for( s = str; *s && max; s++, max-- )
        ;

    return (PRUint32)(s - str);
}
