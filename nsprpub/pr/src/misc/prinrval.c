/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * file:			prinrval.c
 * description:		implementation for the kernel interval timing functions
 */

#include "primpl.h"

/*
 *-----------------------------------------------------------------------
 *
 * _PR_InitClock --
 *
 *
 *-----------------------------------------------------------------------
 */

void _PR_InitClock(void)
{
    _PR_MD_INTERVAL_INIT();
#ifdef DEBUG
    {
        PRIntervalTime ticksPerSec = PR_TicksPerSecond();

        PR_ASSERT(ticksPerSec >= PR_INTERVAL_MIN);
        PR_ASSERT(ticksPerSec <= PR_INTERVAL_MAX);
    }
#endif /* DEBUG */
}

/*
 * This version of interval times is based on the time of day
 * capability offered by system. This isn't valid for two reasons:
 * 1) The time of day is neither linear nor montonically increasing
 * 2) The units here are milliseconds. That's not appropriate for our use.
 */

PR_IMPLEMENT(PRIntervalTime) PR_IntervalNow(void)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return _PR_MD_GET_INTERVAL();
}  /* PR_IntervalNow */

PR_EXTERN(PRUint32) PR_TicksPerSecond(void)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();
    return _PR_MD_INTERVAL_PER_SEC();
}  /* PR_TicksPerSecond */

PR_IMPLEMENT(PRIntervalTime) PR_SecondsToInterval(PRUint32 seconds)
{
    return seconds * PR_TicksPerSecond();
}  /* PR_SecondsToInterval */

PR_IMPLEMENT(PRIntervalTime) PR_MillisecondsToInterval(PRUint32 milli)
{
    PRIntervalTime ticks;
    PRUint64 tock, tps, msecPerSec, rounding;
    LL_UI2L(tock, milli);
    LL_I2L(msecPerSec, PR_MSEC_PER_SEC);
    LL_I2L(rounding, (PR_MSEC_PER_SEC >> 1));
    LL_I2L(tps, PR_TicksPerSecond());
    LL_MUL(tock, tock, tps);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, msecPerSec);
    LL_L2UI(ticks, tock);
    return ticks;
}  /* PR_MillisecondsToInterval */

PR_IMPLEMENT(PRIntervalTime) PR_MicrosecondsToInterval(PRUint32 micro)
{
    PRIntervalTime ticks;
    PRUint64 tock, tps, usecPerSec, rounding;
    LL_UI2L(tock, micro);
    LL_I2L(usecPerSec, PR_USEC_PER_SEC);
    LL_I2L(rounding, (PR_USEC_PER_SEC >> 1));
    LL_I2L(tps, PR_TicksPerSecond());
    LL_MUL(tock, tock, tps);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, usecPerSec);
    LL_L2UI(ticks, tock);
    return ticks;
}  /* PR_MicrosecondsToInterval */

PR_IMPLEMENT(PRUint32) PR_IntervalToSeconds(PRIntervalTime ticks)
{
    return ticks / PR_TicksPerSecond();
}  /* PR_IntervalToSeconds */

PR_IMPLEMENT(PRUint32) PR_IntervalToMilliseconds(PRIntervalTime ticks)
{
    PRUint32 milli;
    PRUint64 tock, tps, msecPerSec, rounding;
    LL_UI2L(tock, ticks);
    LL_I2L(msecPerSec, PR_MSEC_PER_SEC);
    LL_I2L(tps, PR_TicksPerSecond());
    LL_USHR(rounding, tps, 1);
    LL_MUL(tock, tock, msecPerSec);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, tps);
    LL_L2UI(milli, tock);
    return milli;
}  /* PR_IntervalToMilliseconds */

PR_IMPLEMENT(PRUint32) PR_IntervalToMicroseconds(PRIntervalTime ticks)
{
    PRUint32 micro;
    PRUint64 tock, tps, usecPerSec, rounding;
    LL_UI2L(tock, ticks);
    LL_I2L(usecPerSec, PR_USEC_PER_SEC);
    LL_I2L(tps, PR_TicksPerSecond());
    LL_USHR(rounding, tps, 1);
    LL_MUL(tock, tock, usecPerSec);
    LL_ADD(tock, tock, rounding);
    LL_DIV(tock, tock, tps);
    LL_L2UI(micro, tock);
    return micro;
}  /* PR_IntervalToMicroseconds */

/* prinrval.c */

