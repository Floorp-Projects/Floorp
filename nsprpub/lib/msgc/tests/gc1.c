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

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prgc.h"
#include "prinit.h"
#include "prmon.h"
#include "prinrval.h"
#ifndef XP_MAC
#include "private/pprthred.h"
#else
#include "pprthred.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

static PRMonitor *mon;
static PRInt32 threads, waiting, iterations;
static PRInt32 scanCount, finalizeCount, freeCount;

PRIntn failed_already=0;
PRIntn debug_mode;


typedef struct Array {
    PRUintn size;
    void *body[1];
} Array;

int arrayTypeIndex;

static void PR_CALLBACK ScanArray(void *a)
{
/*	printf ("In ScanArray a = %X size = %d \n", a, a->size); */
	scanCount++;
}

static void PR_CALLBACK FinalizeArray(void *a)
{
/*	printf ("In FinalizeArray a = %X size = %d \n", a, a->size); */	
	finalizeCount++;
}

static void PR_CALLBACK FreeArray(void *a)
{
/*	printf ("In FreeArray\n");	*/
	freeCount++;
}

static Array *NewArray(PRUintn size)
{
    Array *a;

    a = (Array *)PR_AllocMemory(sizeof(Array) + size*sizeof(void*) - 1*sizeof(void*),
                       arrayTypeIndex, PR_ALLOC_CLEAN);

/*	printf ("In NewArray a = %X \n", a); */	

    if (a)
        a->size = size;
    return a;
}

GCType arrayType = {
    ScanArray,
    FinalizeArray,
    0,
    0,
    FreeArray,
    0
};

static void Initialize(void)
{
    PR_InitGC(0, 0, 0, PR_GLOBAL_THREAD);
    arrayTypeIndex = PR_RegisterType(&arrayType);
}

static void PR_CALLBACK AllocateLikeMad(void *arg)
{
    Array *prev;
    PRInt32 i;
	PRInt32 count;

	count = (PRInt32)arg;
    prev = 0;
    for (i = 0; i < count; i++) {
        Array *leak = NewArray(i & 511);
        if ((i & 1023) == 0) {
            prev = 0;                   /* forget */
        } else {
            if (i & 1) {
                prev = leak;            /* remember */
            }
        }
    }
    PR_EnterMonitor(mon);
    waiting++;
    PR_Notify(mon);
    PR_ExitMonitor(mon);
}

int main(int argc, char **argv)
{
    PRIntervalTime start, stop, usec;
    double d;
    PRIntn i, totalIterations;
	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dt:c:");

	threads = 10;
	iterations = 100;

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            fprintf(stderr, "Invalid command-line option\n");
            exit(1);
        }
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 't':  /* number of threads */
            threads = atoi(opt->value);
            break;
        case 'c':  /* iteration count */
            iterations = atoi(opt->value);
            break;
        default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    fprintf(stderr, "t is %ld, i is %ld\n", (long) threads, (long) iterations);
	/* main test */

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 5);
    PR_STDIO_INIT();
    Initialize();

#ifdef XP_MAC
	SetupMacPrintfLog("gc1.log");
	debug_mode = 1;
#endif

    /* Spin all of the allocator threads and then wait for them to exit */
    start = PR_IntervalNow();
    mon = PR_NewMonitor();
    PR_EnterMonitor(mon);
    waiting = 0;
    for (i = 0; i < threads; i++) {
        (void) PR_CreateThreadGCAble(PR_USER_THREAD,
                               AllocateLikeMad, (void*)iterations,
						      PR_PRIORITY_NORMAL,
						      PR_LOCAL_THREAD,
		    				  PR_UNJOINABLE_THREAD,
						      0);
    }
    while (waiting != threads) {
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_ExitMonitor(mon);

	PR_GC();
	PR_ForceFinalize();	

	totalIterations = iterations * threads;
/*
	if (scanCount != totalIterations)
		printf ("scanCount discrepancy scanCount = %d totalIterations = %d \n", 
			scanCount, totalIterations);
	if (freeCount != totalIterations)
		printf ("freeCount discrepancy freeCount = %d totalIterations = %d \n", 
			freeCount, totalIterations);
	if ((finalizeCount != totalIterations) && (finalizeCount != (totalIterations-1)))
		printf ("finalizeCount discrepancy finalizeCount = %d totalIterations = %d \n", 
			finalizeCount,totalIterations);
*/

    stop = PR_IntervalNow();
    
    usec = stop = stop - start;
    d = (double)usec;

    if (debug_mode) printf("%40s: %6.2f usec\n", "GC allocation", d / (iterations * threads));
	else {
		if (d == 0.0) failed_already = PR_TRUE;

	}

    PR_Cleanup();
	if(failed_already)	
	{
	    printf("FAIL\n");
		return 1;
	}
	else
	{
	    printf("PASS\n");
		return 0;
	}
}
