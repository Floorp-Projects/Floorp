/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
#include "prlock.h"
#include "prlong.h"
#include "prcvar.h"
#include "prinrval.h"
#include "prtime.h"

#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

PRIntn failed_already=0;
PRIntn debug_mode;


static void TestConversions(void)
{
    PRIntervalTime ticks = PR_TicksPerSecond();

	if (debug_mode) {
    printf("PR_TicksPerSecond: %ld\n\n", ticks);
    printf("PR_SecondsToInterval(1): %ld\n", PR_SecondsToInterval(1));
    printf("PR_MillisecondsToInterval(1000): %ld\n", PR_MillisecondsToInterval(1000));
    printf("PR_MicrosecondsToInterval(1000000): %ld\n\n", PR_MicrosecondsToInterval(1000000));

    printf("PR_SecondsToInterval(3): %ld\n", PR_SecondsToInterval(3));
    printf("PR_MillisecondsToInterval(3000): %ld\n", PR_MillisecondsToInterval(3000));
    printf("PR_MicrosecondsToInterval(3000000): %ld\n\n", PR_MicrosecondsToInterval(3000000));

    printf("PR_IntervalToSeconds(%ld): %ld\n", ticks, PR_IntervalToSeconds(ticks));
    printf("PR_IntervalToMilliseconds(%ld): %ld\n", ticks, PR_IntervalToMilliseconds(ticks));
    printf("PR_IntervalToMicroseconds(%ld): %ld\n\n", ticks, PR_IntervalToMicroseconds(ticks));

    ticks *= 3;
    printf("PR_IntervalToSeconds(%ld): %ld\n", ticks, PR_IntervalToSeconds(ticks));
    printf("PR_IntervalToMilliseconds(%ld): %ld\n", ticks, PR_IntervalToMilliseconds(ticks));
    printf("PR_IntervalToMicroseconds(%ld): %ld\n\n", ticks, PR_IntervalToMicroseconds(ticks));
	} /*end debug mode */
}  /* TestConversions */

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
        if (debug_mode) printf(
            "TestIntervals: %swaiting %ld seconds took %ld msecs\n",
            ((rv == PR_SUCCESS) ? "" : "FAILED "), seconds, delta);
    }
    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
    if (debug_mode) printf("\n");
}  /* TestIntervals */

static PRUint32 GetInterval(PRUint32 loops)
{
    PRIntervalTime interval = 0;
    while (loops-- > 0) interval += PR_IntervalNow();
    return 0;
}  /* GetInterval */

static PRUint32 TimeThis(
    const char *msg, PRUint32 (*func)(PRUint32 loops), PRUint32 loops)
{
    PRUint32 overhead, usecs32;
    PRTime timein, timeout, usecs;

    timein = PR_Now();
    overhead = func(loops);
    timeout = PR_Now();

    LL_SUB(usecs, timeout, timein);
    LL_L2I(usecs32, usecs);
   
    if(usecs32 < overhead)
    {
        if (debug_mode) {
			printf("%s ran in negative time\n", msg);
			printf("  predicted overhead was %ld usecs\n", overhead);
			printf("  test completed in %ld usecs\n\n", usecs);
		}
    }
    else
    {
        if (debug_mode)
			printf(
            "%s [\n\ttotal: %ld usecs\n\toverhead: %ld usecs\n\tcost: %6.3f usecs]\n\n",
            msg, usecs, overhead,
            ((double)(usecs32 - overhead) / (double)loops));
    }

    return overhead;
}  /* TimeThis */

static PRIntn PR_CALLBACK RealMain(int argc, char** argv)
{
    PRUint32 cpu, cpus = 2, loops = 1000;

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
	

    printf("inrval: Examine stdout to determine results.\n");

    if (cpus == 0) cpus = 2;
    if (loops == 0) loops = 1000;
    if (debug_mode) printf("Inrval: Using %d loops\n", loops);


    cpus = (argc < 3) ? 2 : atoi(argv[2]);
    if (cpus == 0) cpus = 2;
    if (debug_mode) printf("Inrval: Using %d cpu(s)\n", cpus);

    if (debug_mode > 0)
    {
        printf("Inrval: Using %d loops\n", loops);
        printf("Inrval: Using %d cpu(s)\n", cpus);
    }

#ifdef XP_MAC
	SetupMacPrintfLog("inrval.log");
	debug_mode = 1;
#endif

    for (cpu = 1; cpu <= cpus; ++cpu)
    {

        if (debug_mode) printf("\nInrval: Using %d CPU(s)\n", cpu);
        if (debug_mode > 0)
            printf("\nInrval: Using %d CPU(s)\n", cpu);

        PR_SetConcurrency(cpu);

        TestConversions();
        TestIntervals();

        (void)TimeThis("GetInterval", GetInterval, loops);
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

