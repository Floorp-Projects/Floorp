/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "prinrval.h"
#include "plgetopt.h"
#include "pprthred.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if (0 == count) count = 50;
    if (0 == iterations) iterations = 10;

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
