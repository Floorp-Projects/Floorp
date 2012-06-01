/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strcpy(char *dest, const char *src)
{
    if( ((char *)0 == dest) || ((const char *)0 == src) ) return (char *)0;

    return strcpy(dest, src);
}

PR_IMPLEMENT(char *)
PL_strncpy(char *dest, const char *src, PRUint32 max)
{
    char *rv;
    
    if( (char *)0 == dest ) return (char *)0;
    if( (const char *)0 == src ) return (char *)0;

    for( rv = dest; max && ((*dest = *src) != 0); dest++, src++, max-- )
        ;

#ifdef JLRU
    /* XXX I (wtc) think the -- and ++ operators should be postfix. */
    while( --max )
        *++dest = '\0';
#endif /* JLRU */

    return rv;
}

PR_IMPLEMENT(char *)
PL_strncpyz(char *dest, const char *src, PRUint32 max)
{
    char *rv;
    
    if( (char *)0 == dest ) return (char *)0;
    if( (const char *)0 == src ) return (char *)0;
    if( 0 == max ) return (char *)0;

    for( rv = dest, max--; max && ((*dest = *src) != 0); dest++, src++, max-- )
        ;

    *dest = '\0';

    return rv;
}
