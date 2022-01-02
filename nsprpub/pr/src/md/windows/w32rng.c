/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    if (!QueryPerformanceCounter(&liCount)) {
        return FALSE;
    }

    *lpdwHigh = liCount.u.HighPart;
    *lpdwLow = liCount.u.LowPart;
    return TRUE;
}

extern PRSize _PR_MD_GetRandomNoise( void *buf, PRSize size )
{
    DWORD   dwHigh, dwLow, dwVal;
    size_t  n = 0;
    size_t  nBytes;
    time_t  sTime;

    if (size <= 0) {
        return 0;
    }

    CurrentClockTickTime(&dwHigh, &dwLow);

    // get the maximally changing bits first
    nBytes = sizeof(dwLow) > size ? size : sizeof(dwLow);
    memcpy((char *)buf, &dwLow, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0) {
        return n;
    }

    nBytes = sizeof(dwHigh) > size ? size : sizeof(dwHigh);
    memcpy(((char *)buf) + n, &dwHigh, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0) {
        return n;
    }

    // get the number of milliseconds that have elapsed since Windows started
    dwVal = GetTickCount();

    nBytes = sizeof(dwVal) > size ? size : sizeof(dwVal);
    memcpy(((char *)buf) + n, &dwVal, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0) {
        return n;
    }

    // get the time in seconds since midnight Jan 1, 1970
    time(&sTime);
    nBytes = sizeof(sTime) > size ? size : sizeof(sTime);
    memcpy(((char *)buf) + n, &sTime, nBytes);
    n += nBytes;

    return n;
}

