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
 * Copyright (C) 1999-2000 Netscape Communications Corporation.  All
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

#include <windows.h>
#include <time.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <primpl.h>

static BOOL
CurrentClockTickTime(LPDWORD lpdwHigh, LPDWORD lpdwLow)
{
    LARGE_INTEGER   liCount;

    if (!QueryPerformanceCounter(&liCount))
        return FALSE;

    *lpdwHigh = liCount.u.HighPart;
    *lpdwLow = liCount.u.LowPart;
    return TRUE;
}

extern PRSize _PR_MD_GetRandomNoise( void *buf, PRSize size )
{
    DWORD   dwHigh, dwLow, dwVal;
    int     n = 0;
    int     nBytes;
    time_t  sTime;

    if (size <= 0)
        return 0;

    CurrentClockTickTime(&dwHigh, &dwLow);

    // get the maximally changing bits first
    nBytes = sizeof(dwLow) > size ? size : sizeof(dwLow);
    memcpy((char *)buf, &dwLow, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
        return n;

    nBytes = sizeof(dwHigh) > size ? size : sizeof(dwHigh);
    memcpy(((char *)buf) + n, &dwHigh, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
        return n;

    // get the number of milliseconds that have elapsed since Windows started
    dwVal = GetTickCount();

    nBytes = sizeof(dwVal) > size ? size : sizeof(dwVal);
    memcpy(((char *)buf) + n, &dwVal, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
        return n;

    // get the time in seconds since midnight Jan 1, 1970
    time(&sTime);
    nBytes = sizeof(sTime) > size ? size : sizeof(sTime);
    memcpy(((char *)buf) + n, &sTime, nBytes);
    n += nBytes;

    return n;
}

