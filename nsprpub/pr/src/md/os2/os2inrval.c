/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * OS/2 interval timers
 *
 */

#include "primpl.h"

static PRBool useHighResTimer = PR_FALSE;
PRIntervalTime _os2_ticksPerSec = -1;
PRIntn _os2_bitShift = 0;
PRInt32 _os2_highMask = 0;
   
void
_PR_MD_INTERVAL_INIT()
{
    char *envp;
    ULONG timerFreq;
    APIRET rc;

    if ((envp = getenv("NSPR_OS2_NO_HIRES_TIMER")) != NULL) {
        if (atoi(envp) == 1)
           return;
    }

    timerFreq = 0; /* OS/2 high-resolution timer frequency in Hz */
    rc = DosTmrQueryFreq(&timerFreq);
    if (NO_ERROR == rc) {
        useHighResTimer = PR_TRUE;
        PR_ASSERT(timerFreq != 0);
        while (timerFreq > PR_INTERVAL_MAX) {
            timerFreq >>= 1;
            _os2_bitShift++;
            _os2_highMask = (_os2_highMask << 1)+1;
        }

        _os2_ticksPerSec = timerFreq;
        PR_ASSERT(_os2_ticksPerSec > PR_INTERVAL_MIN);
    }
}

PRIntervalTime
_PR_MD_GET_INTERVAL()
{
    if (useHighResTimer) {
        QWORD timestamp;
        PRInt32 top;
        APIRET rc = DosTmrQueryTime(&timestamp);
        if (NO_ERROR != rc) {
            return -1;
        }
        /* Sadly, nspr requires the interval to range from 1000 ticks per
         * second to only 100000 ticks per second. DosTmrQueryTime is too
         * high resolution...
         */
        top = timestamp.ulHi & _os2_highMask;
        top = top << (32 - _os2_bitShift);
        timestamp.ulLo = timestamp.ulLo >> _os2_bitShift;   
        timestamp.ulLo = timestamp.ulLo + top; 
        return (PRUint32)timestamp.ulLo;
    } else {
        ULONG msCount = -1;
        DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &msCount, sizeof(msCount));
        return msCount;
    }
}

PRIntervalTime
_PR_MD_INTERVAL_PER_SEC()
{
    if (useHighResTimer) {
        return _os2_ticksPerSec;
    } else {
        return 1000;
    }
}
