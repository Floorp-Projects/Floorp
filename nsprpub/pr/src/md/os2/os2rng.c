/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdlib.h>
#include <time.h>
#include "primpl.h"

static BOOL clockTickTime(unsigned long *phigh, unsigned long *plow)
{
    APIRET rc = NO_ERROR;
    QWORD qword = {0,0};

    rc = DosTmrQueryTime(&qword);
    if (rc != NO_ERROR)
       return FALSE;

    *phigh = qword.ulHi;
    *plow  = qword.ulLo;

    return TRUE;
}

extern PRSize _PR_MD_GetRandomNoise(void *buf, PRSize size )
{
    unsigned long high = 0;
    unsigned long low  = 0;
    clock_t val = 0;
    int n = 0;
    int nBytes = 0;
    time_t sTime;

    if (size <= 0)
       return 0;

    clockTickTime(&high, &low);

    /* get the maximally changing bits first */
    nBytes = sizeof(low) > size ? size : sizeof(low);
    memcpy(buf, &low, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
       return n;

    nBytes = sizeof(high) > size ? size : sizeof(high);
    memcpy(((char *)buf) + n, &high, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
       return n;

    /* get the number of milliseconds that have elapsed since application started */
    val = clock();

    nBytes = sizeof(val) > size ? size : sizeof(val);
    memcpy(((char *)buf) + n, &val, nBytes);
    n += nBytes;
    size -= nBytes;

    if (size <= 0)
       return n;

    /* get the time in seconds since midnight Jan 1, 1970 */
    time(&sTime);
    nBytes = sizeof(sTime) > size ? size : sizeof(sTime);
    memcpy(((char *)buf) + n, &sTime, nBytes);
    n += nBytes;

    return n;
}
