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
** file:            inrval.c
** description:     Interval conversion test.
** Modification History:
** 15-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
**/
/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prinit.h"
#ifdef XP_MAC
#include "pralarm.h"
#else
#include "obsolete/pralarm.h"
#endif

#include "prio.h"
#include "prprf.h"
#include "prlock.h"
#include "prlong.h"
#include "prcvar.h"
#include "prinrval.h"
#include "prtime.h"

#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

static PRIntn debug_mode;
static PRFileDesc *output;


static void TestConversions(void)
{
    PRIntervalTime ticks = PR_TicksPerSecond();

	if (debug_mode) {
    PR_fprintf(output, "PR_TicksPerSecond: %ld\n\n", ticks);
    PR_fprintf(output, "PR_SecondsToInterval(1): %ld\n", PR_SecondsToInterval(1));
    PR_fprintf(output, "PR_MillisecondsToInterval(1000): %ld\n", PR_MillisecondsToInterval(1000));
    PR_fprintf(output, "PR_MicrosecondsToInterval(1000000): %ld\n\n", PR_MicrosecondsToInterval(1000000));

    PR_fprintf(output, "PR_SecondsToInterval(3): %ld\n", PR_SecondsToInterval(3));
    PR_fprintf(output, "PR_MillisecondsToInterval(3000): %ld\n", PR_MillisecondsToInterval(3000));
    PR_fprintf(output, "PR_MicrosecondsToInterval(3000000): %ld\n\n", PR_MicrosecondsToInterval(3000000));

    PR_fprintf(output, "PR_IntervalToSeconds(%ld): %ld\n", ticks, PR_IntervalToSeconds(ticks));
    PR_fprintf(output, "PR_IntervalToMilliseconds(%ld): %ld\n", ticks, PR_IntervalToMilliseconds(ticks));
    PR_fprintf(output, "PR_IntervalToMicroseconds(%ld): %ld\n\n", ticks, PR_IntervalToMicroseconds(ticks));

    ticks *= 3;
    PR_fprintf(output, "PR_IntervalToSeconds(%ld): %ld\n", ticks, PR_IntervalToSeconds(ticks));
    PR_fprintf(output, "PR_IntervalToMilliseconds(%ld): %ld\n", ticks, PR_IntervalToMilliseconds(ticks));
    PR_fprintf(output, "PR_IntervalToMicroseconds(%ld): %ld\n\n", ticks, PR_IntervalToMicroseconds(ticks));
	} /*end debug mode */
}  /* TestConversions */

static void TestIntervalOverhead(void)
{
    /* Hopefully the optimizer won't delete this function */
    PRUint32 elapsed, per_call, loops = 1000000;

    PRIntervalTime timeout, timein = PR_IntervalNow();
    while (--loops > 0)
        timeout = PR_IntervalNow();

    elapsed = 1000U * PR_IntervalToMicroseconds(timeout - timein);
    per_call = elapsed / 1000000U;
    PR_fprintf(
        output, "Overhead of 'PR_IntervalNow()' is %u nsecs\n\n", per_call);
}  /* TestIntervalOverhead */

static void TestNowOverhead(void)
{
    PRTime timeout, timein;
    PRInt32 overhead, loops = 1000000;
    PRInt64 elapsed, per_call, ten23rd, ten26th;

    LL_I2L(ten23rd, 1000);
    LL_I2L(ten26th, 1000000);

    timein = PR_Now();
    while (--loops > 0)
        timeout = PR_Now();

    LL_SUB(elapsed, timeout, timein);
    LL_MUL(elapsed, elapsed, ten23rd);
    LL_DIV(per_call, elapsed, ten26th);
    LL_L2I(overhead, per_call);
    PR_fprintf(
        output, "Overhead of 'PR_Now()' is %u nsecs\n\n", overhead);
}  /* TestNowOverhead */

static void TestIntervals(void)
{
    PRStatus rv;
    PRUint32 delta;
    PRInt32 seconds;
    PRUint64 elapsed, thousand;
    PRTime timein, timeout;
    PRLock *ml = PR_NewLock();
    PRCondVar *cv = PR_NewCondVar(ml);
    for (seconds = 0; seconds < 10; ++seconds)
    {
        PRIntervalTime ticks = PR_SecondsToInterval(seconds);
        PR_Lock(ml);
        timein = PR_Now();
        rv = PR_WaitCondVar(cv, ticks);
        timeout = PR_Now();
        PR_Unlock(ml);
        LL_SUB(elapsed, timeout, timein);
        LL_I2L(thousand, 1000);
        LL_DIV(elapsed, elapsed, thousand);
        LL_L2UI(delta, elapsed);
        if (debug_mode) PR_fprintf(output, 
            "TestIntervals: %swaiting %ld seconds took %ld msecs\n",
            ((rv == PR_SUCCESS) ? "" : "FAILED "), seconds, delta);
    }
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    if (debug_mode) PR_fprintf(output, "\n");
}  /* TestIntervals */

static PRIntn PR_CALLBACK RealMain(int argc, char** argv)
{
    PRUint32 vcpu, cpus = 0, loops = 1000;

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/

 /* main test */
	
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dl:c:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'c':  /* concurrency counter */
			cpus = atoi(opt->value);
            break;
        case 'l':  /* loop counter */
			loops = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);
	
    output = PR_GetSpecialFD(PR_StandardOutput);
    PR_fprintf(output, "inrval: Examine stdout to determine results.\n");

    if (cpus == 0) cpus = 8;
    if (loops == 0) loops = 1000;

    if (debug_mode > 0)
    {
        PR_fprintf(output, "Inrval: Using %d loops\n", loops);
        PR_fprintf(output, "Inrval: Using 1 and %d cpu(s)\n", cpus);
    }

    for (vcpu = 1; vcpu <= cpus; vcpu += cpus - 1)
    {
        if (debug_mode)
            PR_fprintf(output, "\nInrval: Using %d CPU(s)\n\n", vcpu);
        PR_SetConcurrency(vcpu);

        TestNowOverhead();
        TestIntervalOverhead();
        TestConversions();
        TestIntervals();
    }
        
    return 0;
}


PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */

