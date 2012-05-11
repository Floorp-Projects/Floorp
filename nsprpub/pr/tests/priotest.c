/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:        priotest.c
 * Purpose:     testing priorities
 */

#include "prcmon.h"
#include "prinit.h"
#include "prinrval.h"
#include "prlock.h"
#include "prlog.h"
#include "prmon.h"
#include "prprf.h"
#include "prthread.h"
#include "prtypes.h"

#include "plerror.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DURATION 5

static PRBool failed = PR_FALSE;
static PRIntervalTime oneSecond;
static PRFileDesc *debug_out = NULL;
static PRBool debug_mode = PR_FALSE;

static PRUint32 PerSecond(PRIntervalTime timein)
{
    PRUint32 loop = 0;
    while (((PRIntervalTime)(PR_IntervalNow()) - timein) < oneSecond)
        loop += 1;
    return loop;
}  /* PerSecond */

static void PR_CALLBACK Low(void *arg)
{
    PRUint32 t3 = 0, t2 = 0, t1 = 0, t0, *tn = (PRUint32*)arg;
    while (1)
    {
        t0 = PerSecond(PR_IntervalNow());
        *tn = (t3 + 3 * t2 + 3 * t1 + t0) / 8;
        t3 = t2; t2 = t1; t1 = t0;
    }
}  /* Low */

static void PR_CALLBACK High(void *arg)
{
    PRUint32 t3 = 0, t2 = 0, t1 = 0, t0, *tn = (PRUint32*)arg;
    while (1)
    {
        PRIntervalTime timein = PR_IntervalNow();
        PR_Sleep(oneSecond >> 2);  /* 0.25 seconds */
        t0 = PerSecond(timein);
        *tn = (t3 + 3 * t2 + 3 * t1 + t0) / 8;
        t3 = t2; t2 = t1; t1 = t0;
    }
}  /* High */

static void Help(void)
{
    PR_fprintf(
		debug_out, "Usage: priotest [-d] [-c n]\n");
    PR_fprintf(
		debug_out, "-c n\tduration of test in seconds (default: %d)\n", DEFAULT_DURATION);
    PR_fprintf(
        debug_out, "-d\tturn on debugging output (default: FALSE)\n");
}  /* Help */

static void RudimentaryTests(void)
{
    /*
    ** Try some rudimentary tests like setting valid priority and
    ** getting it back, or setting invalid priorities and getting
    ** back a valid answer.
    */
    PRThreadPriority priority;
    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_URGENT);
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_URGENT != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_URGENT != priority))
    {
        PR_fprintf(debug_out, "PR_[S/G]etThreadPriority() failed\n");
    }


    PR_SetThreadPriority(
        PR_GetCurrentThread(), (PRThreadPriority)(PR_PRIORITY_FIRST - 1));
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_FIRST != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_FIRST != priority))
    {
        PR_fprintf(debug_out, "PR_SetThreadPriority(-1) failed\n");
    }

    PR_SetThreadPriority(
        PR_GetCurrentThread(), (PRThreadPriority)(PR_PRIORITY_LAST + 1));
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_LAST != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_LAST != priority))
    {
        PR_fprintf(debug_out, "PR_SetThreadPriority(+1) failed\n");
    }

}  /* RudimentataryTests */

static void CreateThreads(PRUint32 *lowCount, PRUint32 *highCount)
{
    (void)PR_CreateThread(
        PR_USER_THREAD, Low, lowCount, PR_PRIORITY_LOW,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    (void)PR_CreateThread(
        PR_USER_THREAD, High, highCount, PR_PRIORITY_HIGH,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
} /* CreateThreads */

int main(int argc, char **argv)
{
    PLOptStatus os;
    PRIntn duration = DEFAULT_DURATION;
    PRUint32 totalCount, highCount = 0, lowCount = 0;
	PLOptState *opt = PL_CreateOptState(argc, argv, "hdc:");

    debug_out = PR_STDOUT;
    oneSecond = PR_SecondsToInterval(1);

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = PR_TRUE;
            break;
        case 'c':  /* test duration */
			duration = atoi(opt->value);
            break;
        case 'h':  /* help message */
         default:
			Help();
			return 2;
        }
    }
	PL_DestroyOptState(opt);
    PR_STDIO_INIT();

    if (duration == 0) duration = DEFAULT_DURATION;

    RudimentaryTests();

    printf("Priority test: running for %d seconds\n\n", duration);

    (void)PerSecond(PR_IntervalNow());
    totalCount = PerSecond(PR_IntervalNow());

    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_URGENT);

    if (debug_mode)
    {
        PR_fprintf(debug_out,
		    "The high priority thread should get approximately three\n");
        PR_fprintf( debug_out,
		    "times what the low priority thread manages. A maximum of \n");
        PR_fprintf( debug_out, "%d cycles are available.\n\n", totalCount);
    }

    duration = (duration + 4) / 5;
    CreateThreads(&lowCount, &highCount);
    while (duration--)
    {
        PRIntn loop = 5;
        while (loop--) PR_Sleep(oneSecond);
        if (debug_mode)
            PR_fprintf(debug_out, "high : low :: %d : %d\n", highCount, lowCount);
    }


    PR_ProcessExit((failed) ? 1 : 0);

	PR_ASSERT(!"You can't get here -- but you did!");
	return 1;  /* or here */

}  /* main */

/* priotest.c */
