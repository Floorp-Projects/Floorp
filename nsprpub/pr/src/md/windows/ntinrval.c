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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * NT interval timers
 *
 */

#include "primpl.h"

#if defined(WIN16)
#include <win/compobj.h>
#define QueryPerformanceFrequency(x)   FALSE
#define QueryPerformanceCounter(x)     FALSE
#endif

PRIntn _nt_bitShift = 0;
PRInt32 _nt_highMask = 0;
PRInt32 _nt_ticksPerSec = -1;

void
_PR_MD_INTERVAL_INIT()
{
    LARGE_INTEGER count;

    if (QueryPerformanceFrequency(&count)) {
        while(count.LowPart > PR_INTERVAL_MAX) {
            count.LowPart >>= 1;
            _nt_bitShift++;
            _nt_highMask = (_nt_highMask << 1)+1;
        }

        _nt_ticksPerSec = count.LowPart;
        PR_ASSERT(_nt_ticksPerSec > PR_INTERVAL_MIN);
    } else 
        _nt_ticksPerSec = -1;
}

PRIntervalTime 
_PR_MD_GET_INTERVAL()
{
    LARGE_INTEGER count;

   /* Sadly; nspr requires the interval to range from 1000 ticks per second
    * to only 100000 ticks per second; QueryPerformanceCounter is too high
    * resolution...
    */
    if (QueryPerformanceCounter(&count)) {
        PRInt32 top = count.HighPart & _nt_highMask;
        top = top << (32 - _nt_bitShift);
        count.LowPart = count.LowPart >> _nt_bitShift;   
        count.LowPart = count.LowPart + top; 
        return (PRUint32)count.LowPart;
    } else
#if defined(WIN16)
        return clock();        /* milliseconds since application start */
#else
        return timeGetTime();  /* milliseconds since system start */
#endif
}

PRIntervalTime 
_PR_MD_INTERVAL_PER_SEC()
{
    if (_nt_ticksPerSec != -1)
        return _nt_ticksPerSec;
    else
        return 1000;
}
