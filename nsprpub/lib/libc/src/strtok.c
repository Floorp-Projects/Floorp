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
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *   Roland Mainz <roland mainz@informatik.med.uni-giessen.de>
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
