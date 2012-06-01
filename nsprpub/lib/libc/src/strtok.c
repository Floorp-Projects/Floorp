/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"

PR_IMPLEMENT(char *)
PL_strtok_r(char *s1, const char *s2, char **lasts)
{
    const char *sepp;
    int         c, sc;
    char       *tok;

    if( s1 == NULL )
    {
        if( *lasts == NULL )
            return NULL;

        s1 = *lasts;
    }
  
    for( ; (c = *s1) != 0; s1++ )
    {
        for( sepp = s2 ; (sc = *sepp) != 0 ; sepp++ )
        {
            if( c == sc )
                break;
        }
        if( sc == 0 )
            break; 
    }

    if( c == 0 )
    {
        *lasts = NULL;
        return NULL;
    }
  
    tok = s1++;

    for( ; (c = *s1) != 0; s1++ )
    {
        for( sepp = s2; (sc = *sepp) != 0; sepp++ )
        {
            if( c == sc )
            {
                *s1++ = '\0';
                *lasts = s1;
                return tok;
            }
        }
    }
    *lasts = NULL;
    return tok;
}
