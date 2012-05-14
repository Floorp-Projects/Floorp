/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int _debug_on = 0;
#define DPRINTF(arg) if (_debug_on) printf arg

#include "obsolete/prsem.h"

PRLock *lock;
PRMonitor *mon;
PRMonitor *mon2;

#define DEFAULT_COUNT    1000

PRInt32 count;

static void nop(int a, int b, int c)
{
}

static void LocalProcedureCall(void)
{
    PRInt32 i;

    for (i = 0; i < count; i++) {
    nop(i, i, 5);
    }
}

static void DLLProcedureCall(void)
{
    PRInt32 i;
	PRThreadState state;
	PRThread *self = PR_GetCurrentThread();

    for (i = 0; i < count; i++) {
    	state = PR_GetThreadState(self);
    }
}

static void Now(void)
{
    PRInt32 i;
    PRTime time;

    for (i = 0; i < count; i++) {
        time = PR_Now();
    }
}

static void Interval(void)
{
    PRInt32 i;
    PRIntervalTime time;

    for (i = 0; i < count; i++) {
        time = PR_IntervalNow();
    }
}

static void IdleLock(void)
{
    PRInt32 i;

    for (i = 0; i < count; i++) {
    PR_Lock(lock);
    PR_Unlock(lock);
    }
}

static void IdleMonitor(void)
{
    PRInt32 i;

    for (i = 0; i < count; i++) {
    PR_EnterMonitor(mon);
    PR_ExitMonitor(mon);
    }
}

static void IdleCMonitor(void)
{
    PRInt32 i;

    for (i = 0; i < count; i++) {
    PR_CEnterMonitor((void*)7);
    PR_CExitMonitor((void*)7);
    }
}

/************************************************************************/

static void PR_CALLBACK dull(void *arg)
{
}

static void CDThread(void)
{
    PRInt32 i;
    int num_threads = count;

    /*
     * Cannot create too many threads
     */
    if (num_threads > 1000)
    num_threads = 1000;

    for (i = 0; i < num_threads; i++) {
        PRThread *t = PR_CreateThread(PR_USER_THREAD,
                      dull, 0, 
                      PR_PRIORITY_NORMAL,
                      PR_LOCAL_THREAD,
                      PR_UNJOINABLE_THREAD,
                      0);
        if (NULL == t) {
            fprintf(stderr, "CDThread: cannot create thread %3d\n", i);
        } else {
            DPRINTF(("CDThread: created thread %3d \n",i));
        }
        PR_Sleep(0);
    }
}

static int alive;
static int cxq;

static void PR_CALLBACK CXReader(void *arg)
{
    PRInt32 i, n;

    PR_EnterMonitor(mon);
    n = count / 2;
    for (i = 0; i < n; i++) {
    while (cxq == 0) {
            DPRINTF(("CXReader: thread = 0x%lx waiting\n",
                    PR_GetCurrentThread()));
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }
    --cxq;
    PR_Notify(mon);
    }
    PR_ExitMonitor(mon);

    PR_EnterMonitor(mon2);
    --alive;
    PR_Notify(mon2);
    PR_ExitMonitor(mon2);
    DPRINTF(("CXReader: thread = 0x%lx exiting\n", PR_GetCurrentThread()));
}

static void PR_CALLBACK CXWriter(void *arg)
{
    PRInt32 i, n;

    PR_EnterMonitor(mon);
    n = count / 2;
    for (i = 0; i < n; i++) {
    while (cxq == 1) {
            DPRINTF(("CXWriter: thread = 0x%lx waiting\n",
                    PR_GetCurrentThread()));
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }
    ++cxq;
    PR_Notify(mon);
    }
    PR_ExitMonitor(mon);

    PR_EnterMonitor(mon2);
    --alive;
    PR_Notify(mon2);
    PR_ExitMonitor(mon2);
    DPRINTF(("CXWriter: thread = 0x%lx exiting\n", PR_GetCurrentThread()));
}

static void ContextSwitch(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *t1, *t2;

    PR_EnterMonitor(mon2);
    alive = 2;
    cxq = 0;

    t1 = PR_CreateThread(PR_USER_THREAD,
                      CXReader, 0, 
                      PR_PRIORITY_NORMAL,
                      scope1,
                      PR_UNJOINABLE_THREAD,
                      0);
    if (NULL == t1) {
        fprintf(stderr, "ContextSwitch: cannot create thread\n");
    } else {
        DPRINTF(("ContextSwitch: created %s thread = 0x%lx\n",
                (scope1 == PR_GLOBAL_THREAD ?
                "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD"),
                            t1));
    }
    t2 = PR_CreateThread(PR_USER_THREAD,
                      CXWriter, 0, 
                      PR_PRIORITY_NORMAL,
                      scope2,
                      PR_UNJOINABLE_THREAD,
                      0);
    if (NULL == t2) {
        fprintf(stderr, "ContextSwitch: cannot create thread\n");
    } else {
        DPRINTF(("ContextSwitch: created %s thread = 0x%lx\n",
                (scope2 == PR_GLOBAL_THREAD ?
                "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD"),
                            t2));
    }

    /* Wait for both of the threads to exit */
    while (alive) {
    PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_ExitMonitor(mon2);
}

static void ContextSwitchUU(void)
{
    ContextSwitch(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
}

static void ContextSwitchUK(void)
{
    ContextSwitch(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
}

static void ContextSwitchKU(void)
{
    ContextSwitch(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
}

static void ContextSwitchKK(void)
{
    ContextSwitch(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
}

/************************************************************************/

static void PR_CALLBACK SemaThread(void *argSema)
{
    PRSemaphore **sem = (PRSemaphore **)argSema;
    PRInt32 i, n;

    n = count / 2;
    for (i = 0; i < n; i++) {
        DPRINTF(("SemaThread: thread = 0x%lx waiting on sem = 0x%lx\n",
                PR_GetCurrentThread(), sem[0]));
        PR_WaitSem(sem[0]);
        DPRINTF(("SemaThread: thread = 0x%lx posting on sem = 0x%lx\n",
                PR_GetCurrentThread(), sem[1]));
        PR_PostSem(sem[1]);
    }

    PR_EnterMonitor(mon2);
    --alive;
    PR_Notify(mon2);
    PR_ExitMonitor(mon2);
    DPRINTF(("SemaThread: thread = 0x%lx exiting\n", PR_GetCurrentThread()));
}

static  PRSemaphore *sem_set1[2];
static  PRSemaphore *sem_set2[2];

static void SemaContextSwitch(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *t1, *t2;
    sem_set1[0] = PR_NewSem(1);
    sem_set1[1] = PR_NewSem(0);
    sem_set2[0] = sem_set1[1];
    sem_set2[1] = sem_set1[0];

    PR_EnterMonitor(mon2);
    alive = 2;
    cxq = 0;

    t1 = PR_CreateThread(PR_USER_THREAD,
                      SemaThread, 
                      sem_set1, 
                      PR_PRIORITY_NORMAL,
                      scope1,
                      PR_UNJOINABLE_THREAD,
                      0);
    if (NULL == t1) {
        fprintf(stderr, "SemaContextSwitch: cannot create thread\n");
    } else {
        DPRINTF(("SemaContextSwitch: created %s thread = 0x%lx\n",
                (scope1 == PR_GLOBAL_THREAD ?
                "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD"),
                            t1));
    }
    t2 = PR_CreateThread(PR_USER_THREAD,
                      SemaThread, 
                      sem_set2, 
                      PR_PRIORITY_NORMAL,
                      scope2,
                      PR_UNJOINABLE_THREAD,
                      0);
    if (NULL == t2) {
        fprintf(stderr, "SemaContextSwitch: cannot create thread\n");
    } else {
        DPRINTF(("SemaContextSwitch: created %s thread = 0x%lx\n",
                (scope2 == PR_GLOBAL_THREAD ?
                "PR_GLOBAL_THREAD" : "PR_LOCAL_THREAD"),
                            t2));
    }

    /* Wait for both of the threads to exit */
    while (alive) {
        PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_ExitMonitor(mon2);

    PR_DestroySem(sem_set1[0]);
    PR_DestroySem(sem_set1[1]);
}

static void SemaContextSwitchUU(void)
{
    SemaContextSwitch(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
}

static void SemaContextSwitchUK(void)
{
    SemaContextSwitch(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
}

static void SemaContextSwitchKU(void)
{
    SemaContextSwitch(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
}

static void SemaContextSwitchKK(void)
{
    SemaContextSwitch(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
}


/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow() - start;
    d = (double)PR_IntervalToMicroseconds(stop);

    printf("%40s: %6.2f usec\n", msg, d / count);
}

int main(int argc, char **argv)
{
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dc:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			_debug_on = 1;
            break;
        case 'c':  /* loop count */
            count = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    if (0 == count) count = DEFAULT_COUNT;
    
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
	PR_BlockClockInterrupts();
	PR_UnblockClockInterrupts();
    PR_STDIO_INIT();

    lock = PR_NewLock();
    mon = PR_NewMonitor();
    mon2 = PR_NewMonitor();

    Measure(LocalProcedureCall, "local procedure call overhead");
    Measure(DLLProcedureCall, "DLL procedure call overhead");
    Measure(Now, "current calendar time");
    Measure(Interval, "interval time");
    Measure(IdleLock, "idle lock lock/unlock pair");
    Measure(IdleMonitor, "idle monitor entry/exit pair");
    Measure(IdleCMonitor, "idle cache monitor entry/exit pair");
    Measure(CDThread, "create/destroy thread pair");
    Measure(ContextSwitchUU, "context switch - user/user");
    Measure(ContextSwitchUK, "context switch - user/kernel");
    Measure(ContextSwitchKU, "context switch - kernel/user");
    Measure(ContextSwitchKK, "context switch - kernel/kernel");
    Measure(SemaContextSwitchUU, "sema context switch - user/user");
    Measure(SemaContextSwitchUK, "sema context switch - user/kernel");
    Measure(SemaContextSwitchKU, "sema context switch - kernel/user");
    Measure(SemaContextSwitchKK, "sema context switch - kernel/kernel");

    printf("--------------\n");
    printf("Adding 7 additional CPUs\n");

    PR_SetConcurrency(8);
    printf("--------------\n");

    Measure(LocalProcedureCall, "local procedure call overhead");
    Measure(DLLProcedureCall, "DLL procedure call overhead");
    Measure(Now, "current calendar time");
    Measure(Interval, "interval time");
    Measure(IdleLock, "idle lock lock/unlock pair");
    Measure(IdleMonitor, "idle monitor entry/exit pair");
    Measure(IdleCMonitor, "idle cache monitor entry/exit pair");
    Measure(CDThread, "create/destroy thread pair");
    Measure(ContextSwitchUU, "context switch - user/user");
    Measure(ContextSwitchUK, "context switch - user/kernel");
    Measure(ContextSwitchKU, "context switch - kernel/user");
    Measure(ContextSwitchKK, "context switch - kernel/kernel");
    Measure(SemaContextSwitchUU, "sema context switch - user/user");
    Measure(SemaContextSwitchUK, "sema context switch - user/kernel");
    Measure(SemaContextSwitchKU, "sema context switch - kernel/user");
    Measure(SemaContextSwitchKK, "sema context switch - kernel/kernel");

    PR_DestroyLock(lock);
    PR_DestroyMonitor(mon);
    PR_DestroyMonitor(mon2);

    PR_Cleanup();
    return 0;
}
