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
#include "prmem.h"

PR_IMPLEMENT(char *)
PL_strdup(const char *s)
{
    char *rv;
    PRUint32 l;

    l = PL_strlen(s);

    rv = (char *)malloc(l+1);
    if( (char *)0 == rv ) return rv;

    if( (const char *)0 == s )
        *rv = '\0';
    else
        (void)PL_strcpy(rv, s);

    return rv;
}

PR_IMPLEMENT(void)
PL_strfree(char *s)
{
	free(s);
}

PR_IMPLEMENT(char *)
PL_strndup(const char *s, PRUint32 max)
{
    char *rv;
    PRUint32 l;

    l = PL_strnlen(s, max);

    rv = (char *)malloc(l+1);
    if( (char *)0 == rv ) return rv;

    if( (const char *)0 == s )
        *rv = '\0';
    else
        (void)PL_strncpyz(rv, s, l+1);

    return rv;
}
