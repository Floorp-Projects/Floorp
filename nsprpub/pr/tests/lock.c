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
** File:        lock.c
** Purpose:     test basic locking functions
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
**
** 11-Aug-97 LarryH. Win16 port of NSPR.
**           - Added "PASS", "FAIL" messages on completion.
**           - Change stack variables to static scope variables
**             because of shadow-stack use by Win16
**           - Added PR_CALLBACK attribute to functions called by NSPR
**           - Added command line arguments:
**             - l <num> to control the number of loops
**             - c <num> to control the number of CPUs.
**             (was positional argv).
** 
**
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prio.h"
#include "prcmon.h"
#include "prinit.h"
#include "prinrval.h"
#include "prprf.h"
#include "prlock.h"
#include "prlog.h"
#include "prmon.h"
#include "prmem.h"
#include "prthread.h"
#include "prtypes.h"

#include "plstr.h"

#include <stdlib.h>

#if defined(XP_UNIX)
#include <string.h>
#endif

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

static PRIntn failed_already=0;
static PRFileDesc *std_err = NULL;
static PRBool verbosity = PR_FALSE;
static PRBool debug_mode = PR_FALSE;

const static PRIntervalTime contention_interval = 50;

typedef struct LockContentious_s {
    PRLock *ml;
    PRInt32 loops;
    PRUint32 contender;
    PRUint32 contentious;
    PRIntervalTime overhead;
    PRIntervalTime interval;
} LockContentious_t;

typedef struct MonitorContentious_s {
    PRMonitor *ml;
    PRInt32 loops;
    PRUint32 contender;
    PRUint32 contentious;
    PRIntervalTime overhead;
    PRIntervalTime interval;
} MonitorContentious_t;


static PRIntervalTime Sleeper(PRUint32 loops)
{
    PRIntervalTime predicted = 0;
    while (loops-- > 0)
    {
        predicted += contention_interval;
        (void)PR_Sleep(contention_interval);
    }
    return predicted;
}  /* Sleeper */

/*
** BASIC LOCKS
*/
static PRIntervalTime MakeLock(PRUint32 loops)
{
    PRLock *ml = NULL;
    while (loops-- > 0)
    {
        ml = PR_NewLock();
        PR_DestroyLock(ml);
        ml = NULL;
    }
    return 0;
}  /* MakeLock */

static PRIntervalTime NonContentiousLock(PRUint32 loops)
{
    PRLock *ml = NULL;
    ml = PR_NewLock();
    while (loops-- > 0)
    {
        PR_Lock(ml);
        PR_Unlock(ml);
    }
    PR_DestroyLock(ml);
    return 0;
}  /* NonContentiousLock */

static void PR_CALLBACK LockContender(void *arg)
{
    LockContentious_t *contention = (LockContentious_t*)arg;
    while (contention->loops-- > 0)
    {
        PR_Lock(contention->ml);
        contention->contender+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_Unlock(contention->ml);
    }
}  /* LockContender */

static PRIntervalTime ContentiousLock(PRUint32 loops)
{
    PRStatus status;
    PRThread *thread = NULL;
    LockContentious_t * contention;
    PRIntervalTime rv, overhead, timein = PR_IntervalNow();

    contention = PR_NEWZAP(LockContentious_t);
    contention->loops = loops;
    contention->overhead = 0;
    contention->ml = PR_NewLock();
    contention->interval = contention_interval;
    thread = PR_CreateThread(
        PR_USER_THREAD, LockContender, contention,
        PR_PRIORITY_LOW, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_ASSERT(thread != NULL);

    overhead = PR_IntervalNow() - timein;

    while (contention->loops-- > 0)
    {
        PR_Lock(contention->ml);
        contention->contentious+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_Unlock(contention->ml);
    }

    timein = PR_IntervalNow();
    status = PR_JoinThread(thread);
    PR_DestroyLock(contention->ml);
    overhead += (PR_IntervalNow() - timein);
    rv = overhead + contention->overhead;
    if (verbosity)
        PR_fprintf(
            std_err, "Access ratio: %u to %u\n",
            contention->contentious, contention->contender);
    PR_Free(contention);
    return rv;
}  /* ContentiousLock */

/*
** MONITORS
*/
static PRIntervalTime MakeMonitor(PRUint32 loops)
{
    PRMonitor *ml = NULL;
    while (loops-- > 0)
    {
        ml = PR_NewMonitor();
        PR_DestroyMonitor(ml);
        ml = NULL;
    }
    return 0;
}  /* MakeMonitor */

static PRIntervalTime NonContentiousMonitor(PRUint32 loops)
{
    PRMonitor *ml = NULL;
    ml = PR_NewMonitor();
    while (loops-- > 0)
    {
        PR_EnterMonitor(ml);
        PR_ExitMonitor(ml);
    }
    PR_DestroyMonitor(ml);
    return 0;
}  /* NonContentiousMonitor */

static void PR_CALLBACK TryEntry(void *arg)
{
    PRMonitor *ml = (PRMonitor*)arg;
    if (debug_mode) PR_fprintf(std_err, "Reentrant thread created\n");
    PR_EnterMonitor(ml);
    if (debug_mode) PR_fprintf(std_err, "Reentrant thread acquired monitor\n");
    PR_ExitMonitor(ml);
    if (debug_mode) PR_fprintf(std_err, "Reentrant thread released monitor\n");
}  /* TryEntry */

static PRIntervalTime ReentrantMonitor(PRUint32 loops)
{
    PRStatus status;
    PRThread *thread;
    PRMonitor *ml = PR_NewMonitor();
    if (debug_mode) PR_fprintf(std_err, "\nMonitor created for reentrant test\n");

    PR_EnterMonitor(ml);
    PR_EnterMonitor(ml);
    if (debug_mode) PR_fprintf(std_err, "Monitor acquired twice\n");

    thread = PR_CreateThread(
        PR_USER_THREAD, TryEntry, ml,
        PR_PRIORITY_LOW, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_ASSERT(thread != NULL);
    PR_Sleep(PR_SecondsToInterval(1));

    PR_ExitMonitor(ml);
    if (debug_mode) PR_fprintf(std_err, "Monitor released first time\n");

    PR_ExitMonitor(ml);
    if (debug_mode) PR_fprintf(std_err, "Monitor released second time\n");

    status = PR_JoinThread(thread);
    if (debug_mode) PR_fprintf(std_err, 
        "Reentrant thread joined %s\n",
        (status == PR_SUCCESS) ? "successfully" : "in error");

    PR_DestroyMonitor(ml);
    return 0;
}  /* ReentrantMonitor */

static void PR_CALLBACK MonitorContender(void *arg)
{
    MonitorContentious_t *contention = (MonitorContentious_t*)arg;
    while (contention->loops-- > 0)
    {
        PR_EnterMonitor(contention->ml);
        contention->contender+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_ExitMonitor(contention->ml);
    }
}  /* MonitorContender */

static PRUint32 ContentiousMonitor(PRUint32 loops)
{
    PRStatus status;
    PRThread *thread = NULL;
    MonitorContentious_t * contention;
    PRIntervalTime rv, overhead, timein = PR_IntervalNow();

    contention = PR_NEWZAP(MonitorContentious_t);
    contention->loops = loops;
    contention->overhead = 0;
    contention->ml = PR_NewMonitor();
    contention->interval = contention_interval;
    thread = PR_CreateThread(
        PR_USER_THREAD, MonitorContender, contention,
        PR_PRIORITY_LOW, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_ASSERT(thread != NULL);

    overhead = PR_IntervalNow() - timein;

    while (contention->loops-- > 0)
    {
        PR_EnterMonitor(contention->ml);
        contention->contentious+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_ExitMonitor(contention->ml);
    }

    timein = PR_IntervalNow();
    status = PR_JoinThread(thread);
    PR_DestroyMonitor(contention->ml);
    overhead += (PR_IntervalNow() - timein);
    rv = overhead + contention->overhead;
    if (verbosity)
        PR_fprintf(
            std_err, "Access ratio: %u to %u\n",
            contention->contentious, contention->contender);
    PR_Free(contention);
    return rv;
}  /* ContentiousMonitor */

/*
** CACHED MONITORS
*/
static PRIntervalTime NonContentiousCMonitor(PRUint32 loops)
{
    MonitorContentious_t contention;
    while (loops-- > 0)
    {
        PR_CEnterMonitor(&contention);
        PR_CExitMonitor(&contention);
    }
    return 0;
}  /* NonContentiousCMonitor */

static void PR_CALLBACK Contender(void *arg)
{
    MonitorContentious_t *contention = (MonitorContentious_t*)arg;
    while (contention->loops-- > 0)
    {
        PR_CEnterMonitor(contention);
        contention->contender+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_CExitMonitor(contention);
    }
}  /* Contender */

static PRIntervalTime ContentiousCMonitor(PRUint32 loops)
{
    PRStatus status;
    PRThread *thread = NULL;
    MonitorContentious_t * contention;
    PRIntervalTime overhead, timein = PR_IntervalNow();

    contention = PR_NEWZAP(MonitorContentious_t);
    contention->ml = NULL;
    contention->loops = loops;
    contention->interval = contention_interval;
    thread = PR_CreateThread(
        PR_USER_THREAD, Contender, contention,
        PR_PRIORITY_LOW, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_ASSERT(thread != NULL);

    overhead = PR_IntervalNow() - timein;

    while (contention->loops-- > 0)
    {
        PR_CEnterMonitor(contention);
        contention->contentious+= 1;
        contention->overhead += contention->interval;
        PR_Sleep(contention->interval);
        PR_CExitMonitor(contention);
    }

    timein = PR_IntervalNow();
    status = PR_JoinThread(thread);
    overhead += (PR_IntervalNow() - timein);
    overhead += overhead + contention->overhead;
    if (verbosity)
        PR_fprintf(
            std_err, "Access ratio: %u to %u\n",
            contention->contentious, contention->contender);
    PR_Free(contention);
    return overhead;
}  /* ContentiousCMonitor */

static PRIntervalTime Test(
    const char* msg, PRUint32 (*test)(PRUint32 loops),
    PRUint32 loops, PRIntervalTime overhead)
{ 
    /*
     * overhead - overhead not measured by the test.
     * duration - wall clock time it took to perform test.
     * predicted - extra time test says should not be counted 
     *
     * Time accountable to the test is duration - overhead - predicted
     * All times are Intervals and accumulated for all iterations.
     */
    PRFloat64 elapsed;
    PRIntervalTime accountable, duration;    
    PRUintn spaces = PL_strlen(msg);
    PRIntervalTime timeout, timein = PR_IntervalNow();
    PRIntervalTime predicted = test(loops);
    timeout = PR_IntervalNow();
    duration = timeout - timein;

    if (debug_mode)
    {
        accountable = duration - predicted;
        accountable -= overhead;
        elapsed = (PRFloat64)PR_IntervalToMicroseconds(accountable);
        PR_fprintf(PR_STDOUT, "%s:", msg);
        while (spaces++ < 50) PR_fprintf(PR_STDOUT, " ");
        if ((PRInt32)accountable < 0)
            PR_fprintf(PR_STDOUT, "*****.** usecs/iteration\n");
        else
            PR_fprintf(PR_STDOUT, "%8.2f usecs/iteration\n", elapsed/loops);
    }
    return duration;
}  /* Test */

int main(int argc,  char **argv)
{
    PRBool rv = PR_TRUE;
    PRIntervalTime duration;
    PRUint32 cpu, cpus = 2, loops = 100;

	
    PR_STDIO_INIT();
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    {
    	/* The command line argument: -d is used to determine if the test is being run
    	in debug mode. The regress tool requires only one line output:PASS or FAIL.
    	All of the printfs associated with this test has been handled with a if (debug_mode)
    	test.
        Command line argument -l <num> sets the number of loops.
        Command line argument -c <num> sets the number of cpus.
        Usage: lock [-d] [-l <num>] [-c <num>]
    	*/
    	PLOptStatus os;
    	PLOptState *opt = PL_CreateOptState(argc, argv, "dvl:c:");
    	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
        {
    		if (PL_OPT_BAD == os) continue;
            switch (opt->option)
            {
            case 'd':  /* debug mode */
    			debug_mode = PR_TRUE;
                break;
            case 'v':  /* debug mode */
    			verbosity = PR_TRUE;
                break;
            case 'l':  /* number of loops */
                loops = atoi(opt->value);
                break;
            case 'c':  /* number of cpus */
                cpus = atoi(opt->value);
                break;
             default:
                break;
            }
        }
    	PL_DestroyOptState(opt);
    }

 /* main test */
    PR_SetConcurrency(8);

#ifdef XP_MAC
	SetupMacPrintfLog("lock.log");
	debug_mode = 1;
#endif

    if (loops == 0) loops = 100;
    if (debug_mode)
    {
        std_err = PR_STDERR;
        PR_fprintf(std_err, "Lock: Using %d loops\n", loops);
    }

    if (cpus == 0) cpus = 2;
    if (debug_mode) PR_fprintf(std_err, "Lock: Using %d cpu(s)\n", cpus);

    (void)Sleeper(10);  /* try filling in the caches */

    for (cpu = 1; cpu <= cpus; ++cpu)
    {
        if (debug_mode) PR_fprintf(std_err, "\nLock: Using %d CPU(s)\n", cpu);
        PR_SetConcurrency(cpu);

        duration = Test("Overhead of PR_Sleep", Sleeper, loops, 0);
        duration = 0;

        (void)Test("Lock creation/deletion", MakeLock, loops, 0);
        (void)Test("Lock non-contentious locking/unlocking", NonContentiousLock, loops, 0);
        (void)Test("Lock contentious locking/unlocking", ContentiousLock, loops, duration);
        (void)Test("Monitor creation/deletion", MakeMonitor, loops, 0);
        (void)Test("Monitor non-contentious locking/unlocking", NonContentiousMonitor, loops, 0);
        (void)Test("Monitor contentious locking/unlocking", ContentiousMonitor, loops, duration);

        (void)Test("Cached monitor non-contentious locking/unlocking", NonContentiousCMonitor, loops, 0);
        (void)Test("Cached monitor contentious locking/unlocking", ContentiousCMonitor, loops, duration);

        (void)ReentrantMonitor(loops);
    }

    if (debug_mode)
        PR_fprintf(
            std_err, "%s: test %s\n", "Lock(mutex) test",
            ((rv) ? "passed" : "failed"));
	else {
		 if (!rv)
			 failed_already=1;
	}

	if(failed_already)	
	{
	    PR_fprintf(PR_STDOUT, "FAIL\n"); 
		return 1;
    } 
	else
    {
	    PR_fprintf(PR_STDOUT, "PASS\n"); 
		return 0;
    }

}  /* main */

/* testlock.c */
