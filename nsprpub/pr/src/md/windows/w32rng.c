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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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

