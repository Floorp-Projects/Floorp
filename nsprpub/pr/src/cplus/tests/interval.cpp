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

/* interval.cpp - a test program */

#include "rclock.h"
#include "rcthread.h"
#include "rcinrval.h"
#include "rccv.h"

#include <prio.h>
#include <prlog.h>
#include <prprf.h>

#define DEFAULT_ITERATIONS 100

PRIntn main(PRIntn argc, char **argv)
{
    RCLock ml;
    PRStatus rv;
    RCCondition cv(&ml);

    RCInterval now, timeout, epoch, elapsed;
    PRFileDesc *output = PR_GetSpecialFD(PR_StandardOutput);
    PRIntn msecs, seconds, loops, iterations = DEFAULT_ITERATIONS;

    /* slow, agonizing waits */
    for (seconds = 0; seconds < 10; ++seconds)
    {
        timeout = RCInterval::FromSeconds(seconds);
        cv.SetTimeout(timeout);
        {
            RCEnter lock(&ml);

            epoch.SetToNow();

            rv = cv.Wait();
            PR_ASSERT(PR_SUCCESS == rv);

            now = RCInterval(RCInterval::now);
            elapsed = now - epoch;
        }

        PR_fprintf(
            output, "Waiting %u seconds took %s%u milliseconds\n",
            seconds, ((elapsed < timeout)? "**" : ""),
            elapsed.ToMilliseconds());
    }

    /* more slow, agonizing sleeps */
    for (seconds = 0; seconds < 10; ++seconds)
    {
        timeout = RCInterval::FromSeconds(seconds);
        {
            epoch.SetToNow();

            rv = RCThread::Sleep(timeout);
            PR_ASSERT(PR_SUCCESS == rv);

            now = RCInterval(RCInterval::now);
            elapsed = now - epoch;
        }

        PR_fprintf(
            output, "Sleeping %u seconds took %s%u milliseconds\n",
            seconds, ((elapsed < timeout)? "**" : ""),
            elapsed.ToMilliseconds());
    }

    /* fast, spritely little devils */
    for (msecs = 10; msecs < 100; msecs += 10)
    {
        timeout = RCInterval::FromMilliseconds(msecs);
        cv.SetTimeout(timeout);
        {
            RCEnter lock(&ml);

            epoch.SetToNow();

            for (loops = 0; loops < iterations; ++loops)
            {
                rv = cv.Wait();
                PR_ASSERT(PR_SUCCESS == rv);
            }

            now = RCInterval(RCInterval::now);
            elapsed = now - epoch;
        }
        elapsed /= iterations;

        PR_fprintf(
            output, "Waiting %u msecs took %s%u milliseconds average\n",
            msecs, ((elapsed < timeout)? "**" : ""), elapsed.ToMilliseconds());
    }
    return 0;
}  /* main */

/* interval.cpp */

