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

static PRIntn _nt_bitShift = 0;
static PRInt32 _nt_ticksPerSec = -1;

void
_PR_MD_INTERVAL_INIT()
{
    LARGE_INTEGER count;

    if (QueryPerformanceFrequency(&count)) {
        /*
         * HighPart is signed (LONG).  Assert that its sign bit is 0
         * because we will be right shifting it.  LowPart is unsigned
         * (DWORD).
         */
        PR_ASSERT(count.HighPart >= 0);
        while(count.HighPart) {
            count.LowPart = (count.HighPart << 31) + (count.LowPart >> 1);
            count.HighPart >>= 1;
            _nt_bitShift++;
        }
        while(count.LowPart > PR_INTERVAL_MAX) {
            count.LowPart >>= 1;
            _nt_bitShift++;
        }

        /*
         * We can't use the performance counter if after
         * normalization we are left with fewer than 32 bits.
         */
        if (_nt_bitShift <= 32) {
            _nt_ticksPerSec = count.LowPart;
            PR_ASSERT(_nt_ticksPerSec > PR_INTERVAL_MIN);
            return;
        }
    }
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
    if (_nt_ticksPerSec != -1) {
        (void)QueryPerformanceCounter(&count);
        PR_ASSERT(_nt_bitShift <= 32);
        if (_nt_bitShift == 32) {
            return (PRUint32)count.HighPart;
        } else {
            return (PRUint32)((count.HighPart << (32 - _nt_bitShift))
                    + (count.LowPart >> _nt_bitShift));
        }
    } else
#if defined(__MINGW32__)
        return time();
#elif defined(WIN16)
        return clock();        /* milliseconds since application start */
#else
        return GetTickCount();  /* milliseconds since system start */
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
