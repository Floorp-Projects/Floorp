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
**  1996 - Netscape Communications Corporation
**
** Name: cvar2.c
**
** Description: Simple test creates several local and global threads;
**              half use a single,shared condvar, and the
**              other half have their own condvar. The main thread then loops
**				notifying them to wakeup. 
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement. 
***********************************************************************/

#include "nspr.h"
#include "plerror.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int _debug_on = 0;
#define DPRINTF(arg) if (_debug_on) printf arg

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

#define DEFAULT_COUNT   100
#define DEFAULT_THREADS 5
PRInt32 count = DEFAULT_COUNT;

typedef struct threadinfo {
    PRThread        *thread;
    PRInt32          id;
    PRBool           internal;
    PRInt32         *tcount;
    PRLock          *lock;
    PRCondVar       *cvar;
    PRIntervalTime   timeout;
    PRInt32          loops;

    PRLock          *exitlock;
    PRCondVar       *exitcvar;
    PRInt32         *exitcount;
} threadinfo;

/*
** Make exitcount, tcount static. for Win16.
*/
static PRInt32 exitcount=0;
static PRInt32 tcount=0;


/* Thread that gets notified; many threads share the same condvar */
void PR_CALLBACK
SharedCondVarThread(void *_info)
{
    threadinfo *info = (threadinfo *)_info;
    PRInt32 index;

    for (index=0; index<info->loops; index++) {
        PR_Lock(info->lock);
        if (*info->tcount == 0)
            PR_WaitCondVar(info->cvar, info->timeout);
#if 0
        printf("shared thread %ld notified in loop %ld\n", info->id, index);
#endif
        (*info->tcount)--;
        PR_Unlock(info->lock);

        PR_Lock(info->exitlock);
        (*info->exitcount)++;
        PR_NotifyCondVar(info->exitcvar);
        PR_Unlock(info->exitlock);
    }
#if 0
    printf("shared thread %ld terminating\n", info->id);
#endif
}

/* Thread that gets notified; no other threads use the same condvar */
void PR_CALLBACK
PrivateCondVarThread(void *_info)
{
    threadinfo *info = (threadinfo *)_info;
    PRInt32 index;

    for (index=0; index<info->loops; index++) {
        PR_Lock(info->lock);
        if (*info->tcount == 0) {
	    DPRINTF(("PrivateCondVarThread: thread 0x%lx waiting on cvar = 0x%lx\n",
				PR_GetCurrentThread(), info->cvar));
            PR_WaitCondVar(info->cvar, info->timeout);
	}
#if 0
        printf("solo   thread %ld notified in loop %ld\n", info->id, index);
#endif
        (*info->tcount)--;
        PR_Unlock(info->lock);

        PR_Lock(info->exitlock);
        (*info->exitcount)++;
        PR_NotifyCondVar(info->exitcvar);
DPRINTF(("PrivateCondVarThread: thread 0x%lx notified exitcvar = 0x%lx cnt = %ld\n",
			PR_GetCurrentThread(), info->exitcvar,(*info->exitcount)));
        PR_Unlock(info->exitlock);
    }
#if 0
    printf("solo   thread %ld terminating\n", info->id);
#endif
}

void 
CreateTestThread(threadinfo *info, 
                 PRInt32 id,
                 PRLock *lock,
                 PRCondVar *cvar,
                 PRInt32 loops,
                 PRIntervalTime timeout,
                 PRInt32 *tcount,
                 PRLock *exitlock,
                 PRCondVar *exitcvar,
                 PRInt32 *exitcount,
                 PRBool shared, 
                 PRThreadScope scope)
{
    info->id = id;
    info->internal = (shared) ? PR_FALSE : PR_TRUE;
    info->lock = lock;
    info->cvar = cvar;
    info->loops = loops;
    info->timeout = timeout;
    info->tcount = tcount;
    info->exitlock = exitlock;
    info->exitcvar = exitcvar;
    info->exitcount = exitcount;
    info->thread = PR_CreateThread(
                           PR_USER_THREAD,
                           shared?SharedCondVarThread:PrivateCondVarThread,
                           info,
                           PR_PRIORITY_NORMAL,
                           scope,
                           PR_JOINABLE_THREAD,
                           0);
    if (!info->thread)
        PL_PrintError("error creating thread\n");
}


void 
CondVarTestSUU(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    
    exitcount=0;
    tcount=0;
    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg; ) {
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_LOCAL_THREAD);
        index++;
	DPRINTF(("CondVarTestSUU: created thread 0x%lx\n",list[index].thread));
    }

    for (loops = 0; loops < count; loops++) {
        /* Notify the threads */
        for(index=0; index<(arg); index++) {
            PR_Lock(list[index].lock);
            (*list[index].tcount)++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
	    DPRINTF(("PrivateCondVarThread: thread 0x%lx notified cvar = 0x%lx\n",
				PR_GetCurrentThread(), list[index].cvar));
        }

        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg);
        exitcount -= arg;
        PR_Unlock(exitlock);
    }

    /* Join all the threads */
    for(index=0; index<(arg); index++) 
        PR_JoinThread(list[index].thread);

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
}

void 
CondVarTestSUK(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    exitcount=0;
    tcount=0;

    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg; ) {
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_GLOBAL_THREAD);
        index++;
    }

    for (loops = 0; loops < count; loops++) {
        /* Notify the threads */
        for(index=0; index<(arg); index++) {

            PR_Lock(list[index].lock);
            (*list[index].tcount)++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
        }

#if 0
        printf("wait for threads to be done\n");
#endif
        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg);
        exitcount -= arg;
        PR_Unlock(exitlock);
#if 0
        printf("threads ready\n");
#endif
    }

    /* Join all the threads */
    for(index=0; index<(arg); index++) 
        PR_JoinThread(list[index].thread);

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
}

void 
CondVarTestPUU(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    PRInt32 *tcount, *saved_tcount;

    exitcount=0;
    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));
    saved_tcount = tcount = (PRInt32 *)PR_CALLOC(sizeof(*tcount) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg; ) {
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_LOCAL_THREAD);

	DPRINTF(("CondVarTestPUU: created thread 0x%lx\n",list[index].thread));
        index++;
	tcount++;
    }

    for (loops = 0; loops < count; loops++) {
        /* Notify the threads */
        for(index=0; index<(arg); index++) {

            PR_Lock(list[index].lock);
            (*list[index].tcount)++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
        }

	PR_Lock(exitlock);
        /* Wait for threads to finish */
        while(exitcount < arg) {
DPRINTF(("CondVarTestPUU: thread 0x%lx waiting on exitcvar = 0x%lx cnt = %ld\n",
				PR_GetCurrentThread(), exitcvar, exitcount));
            	PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
	}
        PR_ASSERT(exitcount >= arg);
        exitcount -= arg;
        PR_Unlock(exitlock);
    }

    /* Join all the threads */
    for(index=0; index<(arg); index++)  {
	DPRINTF(("CondVarTestPUU: joining thread 0x%lx\n",list[index].thread));
        PR_JoinThread(list[index].thread);
        if (list[index].internal) {
            PR_Lock(list[index].lock);
            PR_DestroyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
            PR_DestroyLock(list[index].lock);
        }
    }

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
    PR_DELETE(saved_tcount);
}

void 
CondVarTestPUK(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    PRInt32 *tcount, *saved_tcount;

    exitcount=0;
    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));
    saved_tcount = tcount = (PRInt32 *)PR_CALLOC(sizeof(*tcount) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg; ) {
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_GLOBAL_THREAD);

        index++;
        tcount++;
    }

    for (loops = 0; loops < count; loops++) {
        /* Notify the threads */
        for(index=0; index<(arg); index++) {

            PR_Lock(list[index].lock);
            (*list[index].tcount)++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
        }

        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg);
        exitcount -= arg;
        PR_Unlock(exitlock);
    }

    /* Join all the threads */
    for(index=0; index<(arg); index++) {
        PR_JoinThread(list[index].thread);
        if (list[index].internal) {
            PR_Lock(list[index].lock);
            PR_DestroyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
            PR_DestroyLock(list[index].lock);
        }
    }

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
    PR_DELETE(saved_tcount);
}

void 
CondVarTest(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    PRInt32 *ptcount, *saved_ptcount;

    exitcount=0;
    tcount=0;
    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));
    saved_ptcount = ptcount = (PRInt32 *)PR_CALLOC(sizeof(*ptcount) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg*4; ) {
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_LOCAL_THREAD);

        index++;
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_GLOBAL_THREAD);

        index++;
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         ptcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_LOCAL_THREAD);
        index++;
	ptcount++;
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_INTERVAL_NO_TIMEOUT,
                         ptcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_GLOBAL_THREAD);

        index++;
	ptcount++;
    }

    for (loops = 0; loops < count; loops++) {

        /* Notify the threads */
        for(index=0; index<(arg*4); index++) {
            PR_Lock(list[index].lock);
            (*list[index].tcount)++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
        }

#if 0
        printf("wait for threads done\n");
#endif

        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg*4)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg*4);
        exitcount -= arg*4;
        PR_Unlock(exitlock);
#if 0
        printf("threads ready\n");
#endif
    }

    /* Join all the threads */
    for(index=0; index<(arg*4); index++) {
        PR_JoinThread(list[index].thread);
        if (list[index].internal) {
            PR_Lock(list[index].lock);
            PR_DestroyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
            PR_DestroyLock(list[index].lock);
        }
    }

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
    PR_DELETE(saved_ptcount);
}

void 
CondVarTimeoutTest(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;

    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg*4; ) {
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_LOCAL_THREAD);
        index++;
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_GLOBAL_THREAD);
        index++;
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_LOCAL_THREAD);
        index++;

        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_GLOBAL_THREAD);

        index++;
    }

    for (loops = 0; loops < count; loops++) {

        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg*4)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg*4);
        exitcount -= arg*4;
        PR_Unlock(exitlock);
    }


    /* Join all the threads */
    for(index=0; index<(arg*4); index++) {
        PR_JoinThread(list[index].thread);
        if (list[index].internal) {
            PR_Lock(list[index].lock);
            PR_DestroyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
            PR_DestroyLock(list[index].lock);
        }
    }

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);
    PR_DestroyCondVar(exitcvar);
    PR_DestroyLock(exitlock);

    PR_DELETE(list);
}

void 
CondVarMixedTest(void *_arg)
{
    PRInt32 arg = (PRInt32)_arg;
    PRInt32 index, loops;
    threadinfo *list;
    PRLock *sharedlock;
    PRCondVar *sharedcvar;
    PRLock *exitlock;
    PRCondVar *exitcvar;
    PRInt32 *ptcount;

    exitcount=0;
    tcount=0;
    list = (threadinfo *)PR_MALLOC(sizeof(threadinfo) * (arg * 4));
    ptcount = (PRInt32 *)PR_CALLOC(sizeof(*ptcount) * (arg * 4));

    sharedlock = PR_NewLock();
    sharedcvar = PR_NewCondVar(sharedlock);
    exitlock = PR_NewLock();
    exitcvar = PR_NewCondVar(exitlock);

    /* Create the threads */
    for(index=0; index<arg*4; ) {
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_LOCAL_THREAD);
        index++;
        CreateTestThread(&list[index],
                         index,
                         sharedlock,
                         sharedcvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         &tcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_TRUE,
                         PR_GLOBAL_THREAD);
        index++;
        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         ptcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_LOCAL_THREAD);
        index++;
	ptcount++;

        list[index].lock = PR_NewLock();
        list[index].cvar = PR_NewCondVar(list[index].lock);
        CreateTestThread(&list[index],
                         index,
                         list[index].lock,
                         list[index].cvar,
                         count,
                         PR_MillisecondsToInterval(50),
                         ptcount,
                         exitlock,
                         exitcvar,
                         &exitcount,
                         PR_FALSE,
                         PR_GLOBAL_THREAD);
        index++;
	ptcount++;
    }


    /* Notify every 3rd thread */
    for (loops = 0; loops < count; loops++) {

        /* Notify the threads */
        for(index=0; index<(arg*4); index+=3) {

            PR_Lock(list[index].lock);
            *list[index].tcount++;
            PR_NotifyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);

        }
        /* Wait for threads to finish */
        PR_Lock(exitlock);
        while(exitcount < arg*4)
            PR_WaitCondVar(exitcvar, PR_SecondsToInterval(60));
        PR_ASSERT(exitcount >= arg*4);
        exitcount -= arg*4;
        PR_Unlock(exitlock);
    }

    /* Join all the threads */
    for(index=0; index<(arg*4); index++) {
        PR_JoinThread(list[index].thread);
        if (list[index].internal) {
            PR_Lock(list[index].lock);
            PR_DestroyCondVar(list[index].cvar);
            PR_Unlock(list[index].lock);
            PR_DestroyLock(list[index].lock);
        }
    }

    PR_DestroyCondVar(sharedcvar);
    PR_DestroyLock(sharedlock);

    PR_DELETE(list);
}

void 
CondVarCombinedTest(void *arg)
{
    PRThread *threads[3];

    threads[0] = PR_CreateThread(PR_USER_THREAD,
                                 CondVarTest,
                                 (void *)arg,
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);
    threads[1] = PR_CreateThread(PR_USER_THREAD,
                                 CondVarTimeoutTest,
                                 (void *)arg,
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);
    threads[2] = PR_CreateThread(PR_USER_THREAD,
                                 CondVarMixedTest,
                                 (void *)arg,
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);

    PR_JoinThread(threads[0]);
    PR_JoinThread(threads[1]);
    PR_JoinThread(threads[2]);
}

/************************************************************************/

static void Measure(void (*func)(void *), PRInt32 arg, const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)((void *)arg);
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);

    printf("%40s: %6.2f usec\n", msg, d / count);
}

static PRIntn PR_CALLBACK RealMain(int argc, char **argv)
{
    PRInt32 threads, default_threads = DEFAULT_THREADS;
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "vc:t:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'v':  /* debug mode */
			_debug_on = 1;
            break;
        case 'c':  /* loop counter */
			count = atoi(opt->value);
            break;
        case 't':  /* number of threads involved */
			default_threads = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    if (0 == count) count = DEFAULT_COUNT;
    if (0 == default_threads) default_threads = DEFAULT_THREADS;

#ifdef XP_MAC
	SetupMacPrintfLog("cvar2.log");
#endif

    printf("\n\
CondVar Test:                                                           \n\
                                                                        \n\
Simple test creates several local and global threads; half use a single,\n\
shared condvar, and the other half have their own condvar.  The main    \n\
thread then loops notifying them to wakeup.                             \n\
                                                                        \n\
The timeout test is very similar except that the threads are not        \n\
notified.  They will all wakeup on a 1 second timeout.                  \n\
                                                                        \n\
The mixed test combines the simple test and the timeout test; every     \n\
third thread is notified, the other threads are expected to timeout     \n\
correctly.                                                              \n\
                                                                        \n\
Lastly, the combined test creates a thread for each of the above three  \n\
cases and they all run simultaneously.                                  \n\
                                                                        \n\
This test is run with %d, %d, %d, and %d threads of each type.\n\n",
default_threads, default_threads*2, default_threads*3, default_threads*4);

    PR_SetConcurrency(2);

    for (threads = default_threads; threads < default_threads*5; threads+=default_threads) {
        printf("\n%ld Thread tests\n", threads);
        Measure(CondVarTestSUU, threads, "Condvar simple test shared UU");
        Measure(CondVarTestSUK, threads, "Condvar simple test shared UK");
        Measure(CondVarTestPUU, threads, "Condvar simple test priv UU");
        Measure(CondVarTestPUK, threads, "Condvar simple test priv UK");
#ifdef XP_MAC
	/* Mac heaps can't handle thread*4 stack allocations at a time for (10, 15, 20)*4 */
        Measure(CondVarTest, 5, "Condvar simple test All");
        Measure(CondVarTimeoutTest, 5,  "Condvar timeout test");
#else
        Measure(CondVarTest, threads, "Condvar simple test All");
        Measure(CondVarTimeoutTest, threads,  "Condvar timeout test");
#endif
#if 0
        Measure(CondVarMixedTest, threads,  "Condvar mixed timeout test");
        Measure(CondVarCombinedTest, threads, "Combined condvar test");
#endif
    }

    printf("PASS\n");

    return 0;
}

PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
