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

#ifdef XP_MAC
#include "prlog.h"
unsigned int PR_fprintf(PRFileDesc *fd, const char *fmt, ...)
{
PR_LogPrint(fmt);
return 0;
}
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

#define DEFAULT_DURATION 20

static PRBool failed = PR_FALSE;
static PRIntervalTime oneSecond;
static PRFileDesc *debug_out = NULL;

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

PRIntn main(PRIntn argc, char **argv)
{
    PLOptStatus os;
    PRThread *low, *high;
    PRThreadPriority priority;
    PRBool debug_mode = PR_FALSE;
    PRIntn duration = DEFAULT_DURATION;
    PRUint32 totalCount, highCount = 0, lowCount = 0;
	PLOptState *opt = PL_CreateOptState(argc, argv, "hdc:");


#ifdef XP_MAC
	SetupMacPrintfLog("priotest.log");
	debug_mode = PR_TRUE;
#endif

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

    printf("Priority test: running for %d seconds\n\n", duration);

    (void)PerSecond(PR_IntervalNow());
    totalCount = PerSecond(PR_IntervalNow());

    /*
    ** Try some rudimentary tests like setting valid priority and
    ** getting it back, or setting invalid priorities and getting
    ** back a valid answer.
    */
    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_URGENT);
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_URGENT != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_URGENT != priority))
    {
        PR_fprintf(debug_out, "PR_[S/G]etThreadPriority() failed\n");
    }

/*
	The following test is bogus.  PRThreadPriority is enum and (PR_PRIORITY_FIRST - 1)
	evaluates to 255 (at least on Mac).  This causes the pinning test in PR_SetThreadPriority
	to fail.  That test in PR_SetThreadPriority should be looked at.
*/
#if 0
    PR_SetThreadPriority(
        PR_GetCurrentThread(), (PRThreadPriority)(PR_PRIORITY_FIRST - 1));
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_FIRST != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_FIRST != priority))
    {
        PR_fprintf(debug_out, "PR_SetThreadPriority(-1) failed\n");
    }
#endif

    PR_SetThreadPriority(
        PR_GetCurrentThread(), (PRThreadPriority)(PR_PRIORITY_LAST + 1));
    priority = PR_GetThreadPriority(PR_GetCurrentThread());
    failed = ((PR_TRUE == failed) || (PR_PRIORITY_LAST != priority))
        ? PR_TRUE : PR_FALSE;
    if (debug_mode && (PR_PRIORITY_LAST != priority))
    {
        PR_fprintf(debug_out, "PR_SetThreadPriority(+1) failed\n");
    }


    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_URGENT);

    if (debug_mode)
    {
        PR_fprintf(debug_out,
		    "The high priority thread should get approximately three\n");
        PR_fprintf( debug_out,
		    "times what the low priority thread manages. A maximum of \n");
        PR_fprintf( debug_out, "%d cycles are available.\n\n", totalCount);
    }

#ifdef XP_MAC
    if (debug_mode)
    {
        PR_fprintf(debug_out,
		    "On Mac, this test should hang indefinitely \n");
        PR_fprintf( debug_out,
		    "because low priority task never gives up time \n");
        PR_fprintf( debug_out, "and the program will hang there.\n");
    }

#endif

    low = PR_CreateThread(
        PR_USER_THREAD, Low, &lowCount, PR_PRIORITY_LOW,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    high = PR_CreateThread(
        PR_USER_THREAD, High, &highCount, PR_PRIORITY_HIGH,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    duration = (duration + 4) / 5;
    while (duration--)
    {
        PRIntn loop = 5;
        while (loop--) PR_Sleep(oneSecond);
        if (debug_mode)
            PR_fprintf(debug_out, "high : low :: %d : %d\n", highCount, lowCount);
    }

    PR_ProcessExit((failed) ? 1 : 0);

	PR_ASSERT(!"Can't get here");
	return 1;  /* or here */

}  /* main */

/* priotest.c */
