/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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

