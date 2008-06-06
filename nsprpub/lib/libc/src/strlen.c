/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        PR_ASSERT(l < 2147483647);

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
