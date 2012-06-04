/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "prtypes.h"
#include "prlog.h"
#include <string.h>

PR_IMPLEMENT(PRUint32)
PL_strlen(const char *str)
{
    size_t l;

    if( (const char *)0 == str ) return 0;

    l = strlen(str);

    /* error checking in case we have a 64-bit platform -- make sure
     * we don't have ultra long strings that overflow an int32
     */ 
    if( sizeof(PRUint32) < sizeof(size_t) )
    {
        if( l > PR_INT32_MAX )
            PR_Assert("l <= PR_INT32_MAX", __FILE__, __LINE__);
    }

    return (PRUint32)l;
}

PR_IMPLEMENT(PRUint32)
PL_strnlen(const char *str, PRUint32 max)
{
    register const char *s;

    if( (const char *)0 == str ) return 0;
    for( s = str; max && *s; s++, max-- )
        ;

    return (PRUint32)(s - str);
}
