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
** File:		prinrval.h
** Description:	API to interval timing functions of NSPR.
**
**
** NSPR provides interval times that are independent of network time
** of day values. Interval times are (in theory) accurate regardless
** of host processing requirements and also very cheap to acquire. It
** is expected that getting an interval time while in a synchronized
** function (holding one's lock).
**/

#if !defined(prinrval_h)
#define prinrval_h

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

typedef PRUint32 PRIntervalTime;

/***********************************************************************
** DEFINES:     PR_INTERVAL_MIN
**              PR_INTERVAL_MAX
** DESCRIPTION:
**  These two constants define the range (in ticks / second) of the
**  platform dependent type, PRIntervalTime. These constants bound both
**  the period and the resolution of a PRIntervalTime. 
***********************************************************************/
#define PR_INTERVAL_MIN 1000UL
#define PR_INTERVAL_MAX 100000UL

/***********************************************************************
** DEFINES:     PR_INTERVAL_NO_WAIT
**              PR_INTERVAL_NO_TIMEOUT
** DESCRIPTION:
**  Two reserved constants are defined in the PRIntervalTime namespace.
**  They are used to indicate that the process should wait no time (return
**  immediately) or wait forever (never time out), respectively.
***********************************************************************/
#define PR_INTERVAL_NO_WAIT 0UL
#define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/***********************************************************************
** FUNCTION:    PR_IntervalNow
** DESCRIPTION:
**  Return the value of NSPR's free running interval timer. That timer
**  can be used to establish epochs and determine intervals (be computing
**  the difference between two times).
** INPUTS:      void
** OUTPUTS:     void
** RETURN:      PRIntervalTime
**  
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  The units of PRIntervalTime are platform dependent. They are chosen
**  such that they are appropriate for the host OS, yet provide sufficient
**  resolution and period to be useful to clients. 
** MEMORY:      N/A
** ALGORITHM:   Platform dependent
***********************************************************************/
PR_EXTERN(PRIntervalTime) PR_IntervalNow(void);

/***********************************************************************
** FUNCTION:    PR_TicksPerSecond
** DESCRIPTION:
**  Return the number of ticks per second for PR_IntervalNow's clock.
**  The value will be in the range [PR_INTERVAL_MIN..PR_INTERVAL_MAX].
** INPUTS:      void
** OUTPUTS:     void
** RETURN:      PRUint32
**  
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  None
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
PR_EXTERN(PRUint32) PR_TicksPerSecond(void);

/***********************************************************************
** FUNCTION:    PR_SecondsToInterval
**              PR_MillisecondsToInterval
**              PR_MicrosecondsToInterval
** DESCRIPTION:
**  Convert standard clock units to platform dependent intervals.
** INPUTS:      PRUint32
** OUTPUTS:     void
** RETURN:      PRIntervalTime
**  
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  Conversion may cause overflow, which is not reported.
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
PR_EXTERN(PRIntervalTime) PR_SecondsToInterval(PRUint32 seconds);
PR_EXTERN(PRIntervalTime) PR_MillisecondsToInterval(PRUint32 milli);
PR_EXTERN(PRIntervalTime) PR_MicrosecondsToInterval(PRUint32 micro);

/***********************************************************************
** FUNCTION:    PR_IntervalToSeconds
**              PR_IntervalToMilliseconds
**              PR_IntervalToMicroseconds
** DESCRIPTION:
**  Convert platform dependent intervals to standard clock units.
** INPUTS:      PRIntervalTime
** OUTPUTS:     void
** RETURN:      PRUint32
**  
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  Conversion may cause overflow, which is not reported.
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
PR_EXTERN(PRUint32) PR_IntervalToSeconds(PRIntervalTime ticks);
PR_EXTERN(PRUint32) PR_IntervalToMilliseconds(PRIntervalTime ticks);
PR_EXTERN(PRUint32) PR_IntervalToMicroseconds(PRIntervalTime ticks);

PR_END_EXTERN_C


#endif /* !defined(prinrval_h) */

/* prinrval.h */
