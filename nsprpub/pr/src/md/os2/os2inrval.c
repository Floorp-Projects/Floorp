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
 * OS/2 interval timers
 *
 */

#include "primpl.h"

ULONG _os2_ticksPerSec = -1;
PRIntn _os2_bitShift = 0;
PRInt32 _os2_highMask = 0;


   
PR_IMPLEMENT(void)
_PR_MD_INTERVAL_INIT()
{
   if (DosTmrQueryFreq(&_os2_ticksPerSec) == NO_ERROR)
   {
      while(_os2_ticksPerSec > PR_INTERVAL_MAX) {
          _os2_ticksPerSec >>= 1;
          _os2_bitShift++;
          _os2_highMask = (_os2_highMask << 1)+1;
      }
   }
   else
      _os2_ticksPerSec = -1;

   PR_ASSERT(_os2_ticksPerSec > PR_INTERVAL_MIN && _os2_ticksPerSec < PR_INTERVAL_MAX);
}

PR_IMPLEMENT(PRIntervalTime) 
_PR_MD_GET_INTERVAL()
{
   QWORD count;

   /* Sadly; nspr requires the interval to range from 1000 ticks per second
    * to only 100000 ticks per second; Counter is too high
    * resolution...
    */
    if (DosTmrQueryTime(&count) == NO_ERROR) {
        PRInt32 top = count.ulHi & _os2_highMask;
        top = top << (32 - _os2_bitShift);
        count.ulLo = count.ulLo >> _os2_bitShift;   
        count.ulHi = count.ulLo + top; 
        return (PRUint32)count.ulLo;
    }
    else{
       ULONG msCount = PR_FAILURE;
       DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &msCount, sizeof(msCount));
       return msCount;
    }
}

PR_IMPLEMENT(PRIntervalTime) 
_PR_MD_INTERVAL_PER_SEC()
{
    if(_os2_ticksPerSec != -1)
       return _os2_ticksPerSec;
    else
       return 1000;
}
