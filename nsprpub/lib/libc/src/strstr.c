/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

PR_IMPLEMENT(char *)
PL_strstr(const char *big, const char *little)
{
    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    return strstr(big, little);
}

PR_IMPLEMENT(char *)
PL_strrstr(const char *big, const char *little)
{
    const char *p;
    size_t ll;
    size_t bl;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);
    bl = strlen(big);
    if( bl < ll ) {
        return (char *)0;
    }
    p = &big[ bl - ll ];

    for( ; p >= big; p-- )
        if( *little == *p )
            if( 0 == strncmp(p, little, ll) ) {
                return (char *)p;
            }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnstr(const char *big, const char *little, PRUint32 max)
{
    size_t ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);
    if( ll > (size_t)max ) {
        return (char *)0;
    }
    max -= (PRUint32)ll;
    max++;

    for( ; max && *big; big++, max-- )
        if( *little == *big )
            if( 0 == strncmp(big, little, ll) ) {
                return (char *)big;
            }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strnrstr(const char *big, const char *little, PRUint32 max)
{
    const char *p;
    size_t ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);

    for( p = big; max && *p; p++, max-- )
        ;

    p -= ll;
    if( p < big ) {
        return (char *)0;
    }

    for( ; p >= big; p-- )
        if( *little == *p )
            if( 0 == strncmp(p, little, ll) ) {
                return (char *)p;
            }

    return (char *)0;
}
