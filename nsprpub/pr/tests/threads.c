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

#include "nspr.h"
#include "prinrval.h"
#include "plgetopt.h"
#include "pprthred.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

PRMonitor *mon;
PRInt32 count, iterations, alive;

PRBool debug_mode = PR_FALSE, passed = PR_TRUE;

void 
PR_CALLBACK
ReallyDumbThread(void *arg)
{
    return;
}

void
PR_CALLBACK
DumbThread(void *arg)
{
    PRInt32 tmp = (PRInt32)arg;
    PRThreadScope scope = (PRThreadScope)tmp;
    PRThread *thr;

    thr = PR_CreateThread(PR_USER_THREAD,
                          ReallyDumbThread,
                          NULL,
                          PR_PRIORITY_NORMAL,
                          scope,
                          PR_JOINABLE_THREAD,
                          0);

    if (!thr) {
        if (debug_mode) {
            printf("Could not create really dumb thread (%d, %d)!\n",
                    PR_GetError(), PR_GetOSError());
        }
        passed = PR_FALSE;
    } else {
        PR_JoinThread(thr);
    }
    PR_EnterMonitor(mon);
    alive--;
    PR_Notify(mon);
    PR_ExitMonitor(mon);
}

static void CreateThreads(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *thr;
    int n;

    alive = 0;
    mon = PR_NewMonitor();

    alive = count;
    for (n=0; n<count; n++) {
        thr = PR_CreateThread(PR_USER_THREAD,
                              DumbThread, 
                              (void *)scope2, 
                              PR_PRIORITY_NORMAL,
                              scope1,
                              PR_UNJOINABLE_THREAD,
                              0);
        if (!thr) {
            if (debug_mode) {
                printf("Could not create dumb thread (%d, %d)!\n",
                        PR_GetError(), PR_GetOSError());
            }
            passed = PR_FALSE;
            alive--;
        }
         
        PR_Sleep(0);
    }

    /* Wait for all threads to exit */
    PR_EnterMonitor(mon);
    while (alive) {
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }

    PR_ExitMonitor(mon);
	PR_DestroyMonitor(mon);
}

static void CreateThreadsUU(void)
{
    CreateThreads(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
}

static void CreateThreadsUK(void)
{
    CreateThreads(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
}

static void CreateThreadsKU(void)
{
    CreateThreads(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
}

static void CreateThreadsKK(void)
{
    CreateThreads(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
}

/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    if (debug_mode)
    {
        d = (double)PR_IntervalToMicroseconds(stop - start);
        printf("%40s: %6.2f usec\n", msg, d / count);
    }
}

int main(int argc, char **argv)
{
    int index;

    PR_STDIO_INIT();
    PR_Init(PR_USER_THREAD, PR_PRIORITY_HIGH, 0);
    
    {
    	PLOptStatus os;
    	PLOptState *opt = PL_CreateOptState(argc, argv, "dc:i:");
    	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
    		if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'd':  /* debug mode */
    			debug_mode = PR_TRUE;
                break;
            case 'c':  /* loop counter */
    			count = atoi(opt->value);
                break;
            case 'i':  /* loop counter */
    			iterations = atoi(opt->value);
                break;
             default:
                break;
            }
        }
    	PL_DestroyOptState(opt);
    }

#ifdef XP_MAC
	SetupMacPrintfLog("threads.log");
	count = 10;
	iterations = 10;
	debug_mode = PR_TRUE;
#else
    if (0 == count) count = 50;
    if (0 == iterations) iterations = 10;

#endif

    if (debug_mode)
    {
    printf("\
** Tests lots of thread creations.  \n\
** Create %ld native threads %ld times. \n\
** Create %ld user threads %ld times \n", iterations,count,iterations,count);
    }

    for (index=0; index<iterations; index++) {
        Measure(CreateThreadsUU, "Create user/user threads");
        Measure(CreateThreadsUK, "Create user/native threads");
        Measure(CreateThreadsKU, "Create native/user threads");
        Measure(CreateThreadsKK, "Create native/native threads");
    }

    if (debug_mode) printf("\nNow switch to recycling threads \n\n");
    PR_SetThreadRecycleMode(1);

    for (index=0; index<iterations; index++) {
        Measure(CreateThreadsUU, "Create user/user threads");
        Measure(CreateThreadsUK, "Create user/native threads");
        Measure(CreateThreadsKU, "Create native/user threads");
        Measure(CreateThreadsKK, "Create native/native threads");
    }


    printf("%s\n", ((passed) ? "PASS" : "FAIL"));

    PR_Cleanup();

    if (passed) {
        return 0;
    } else {
        return 1;
    }
}
