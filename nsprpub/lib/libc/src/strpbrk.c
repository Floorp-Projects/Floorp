/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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
