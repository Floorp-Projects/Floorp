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

#include "plerror.h"
#include "plgetopt.h"

#include "prinit.h"
#include "prprf.h"
#include "prio.h"
#include "prcvar.h"
#include "prmon.h"
#include "prcmon.h"
#include "prlock.h"
#include "prerror.h"
#include "prinit.h"
#include "prinrval.h"
#include "prthread.h"

static PRLock *ml = NULL;
static PRIntervalTime base;
static PRFileDesc *err = NULL;

typedef struct CMonShared
{
    PRInt32 o1, o2;
} CMonShared;

typedef struct MonShared
{
    PRMonitor *o1, *o2;
} MonShared;

typedef struct LockShared
{
    PRLock *o1, *o2;
    PRCondVar *cv1, *cv2;
} LockShared;

static void LogNow(const char *msg, PRStatus rv)
{
    PRIntervalTime now = PR_IntervalNow();
    PR_Lock(ml);
    PR_fprintf(err, "%6ld: %s", (now - base), msg);
    if (PR_FAILURE == rv) PL_FPrintError(err, " ");
    else PR_fprintf(err, "\n");
    PR_Unlock(ml);
}  /* LogNow */

static void Help(void)
{
    PR_fprintf(err, "Usage: [-[d][l][m][c]] [-h]\n");
    PR_fprintf(err, "\t-d   debug mode                  (default: FALSE)\n");
    PR_fprintf(err, "\t-l   test with locks             (default: FALSE)\n");
    PR_fprintf(err, "\t-m   tests with monitors         (default: FALSE)\n");
    PR_fprintf(err, "\t-c   tests with cmonitors        (default: FALSE)\n");
    PR_fprintf(err, "\t-h   help\n");
}  /* Help */

static void PR_CALLBACK T2CMon(void *arg)
{
    PRStatus rv;
    CMonShared *shared = (CMonShared*)arg;

    PR_CEnterMonitor(&shared->o1);
    LogNow("T2 waiting 5 seconds on o1", PR_SUCCESS);
    rv = PR_CWait(&shared->o1, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T2 resuming on o1", rv);
    else LogNow("T2 wait failed on o1", rv);

    rv = PR_CNotify(&shared->o1);
    if (PR_SUCCESS == rv) LogNow("T2 notified o1", rv);
    else LogNow("T2 notify on o1 failed", rv);

    PR_CExitMonitor(&shared->o1);
}  /* T2CMon */

static void PR_CALLBACK T3CMon(void *arg)
{
    PRStatus rv;
    CMonShared *shared = (CMonShared*)arg;

    PR_CEnterMonitor(&shared->o2);
    LogNow("T3 waiting 5 seconds on o2", PR_SUCCESS);
    rv = PR_CWait(&shared->o2, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T3 resuming on o2", rv);
    else LogNow("T3 wait failed on o2", rv);
    rv = PR_CNotify(&shared->o2);
    LogNow("T3 notify on o2", rv);
    PR_CExitMonitor(&shared->o2);

}  /* T3CMon */

static CMonShared sharedCM;

static void T1CMon(void)
{
    PRStatus rv;
    PRThread *t2, *t3;

    PR_fprintf(err, "\n**********************************\n");
    PR_fprintf(err, "         CACHED MONITORS\n");
    PR_fprintf(err, "**********************************\n");

    base =  PR_IntervalNow();

    PR_CEnterMonitor(&sharedCM.o1);
    LogNow("T1 waiting 3 seconds on o1", PR_SUCCESS);
    rv = PR_CWait(&sharedCM.o1, PR_SecondsToInterval(3));
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o1", rv);
    else LogNow("T1 wait on o1 failed", rv);
    PR_CExitMonitor(&sharedCM.o1);

    LogNow("T1 creating T2", PR_SUCCESS);
    t2 = PR_CreateThread(
        PR_USER_THREAD, T2CMon, &sharedCM, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    LogNow("T1 creating T3", PR_SUCCESS);
    t3 = PR_CreateThread(
        PR_USER_THREAD, T3CMon, &sharedCM, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    PR_CEnterMonitor(&sharedCM.o2);
    LogNow("T1 waiting forever on o2", PR_SUCCESS);
    rv = PR_CWait(&sharedCM.o2, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o2", rv);
    else LogNow("T1 wait on o2 failed", rv);
    PR_CExitMonitor(&sharedCM.o2);

    (void)PR_JoinThread(t2);
    (void)PR_JoinThread(t3);

}  /* T1CMon */

static void PR_CALLBACK T2Mon(void *arg)
{
    PRStatus rv;
    MonShared *shared = (MonShared*)arg;

    PR_EnterMonitor(shared->o1);
    LogNow("T2 waiting 5 seconds on o1", PR_SUCCESS);
    rv = PR_Wait(shared->o1, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T2 resuming on o1", rv);
    else LogNow("T2 wait failed on o1", rv);

    rv = PR_Notify(shared->o1);
    if (PR_SUCCESS == rv) LogNow("T2 notified o1", rv);
    else LogNow("T2 notify on o1 failed", rv);

    PR_ExitMonitor(shared->o1);
}  /* T2Mon */

static void PR_CALLBACK T3Mon(void *arg)
{
    PRStatus rv;
    MonShared *shared = (MonShared*)arg;

    PR_EnterMonitor(shared->o2);
    LogNow("T3 waiting 5 seconds on o2", PR_SUCCESS);
    rv = PR_Wait(shared->o2, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T3 resuming on o2", rv);
    else LogNow("T3 wait failed on o2", rv);
    rv = PR_Notify(shared->o2);
    LogNow("T3 notify on o2", rv);
    PR_ExitMonitor(shared->o2);

}  /* T3Mon */

static MonShared sharedM;
static void T1Mon(void)
{
    PRStatus rv;
    PRThread *t2, *t3;

    PR_fprintf(err, "\n**********************************\n");
    PR_fprintf(err, "            MONITORS\n");
    PR_fprintf(err, "**********************************\n");

    sharedM.o1 = PR_NewMonitor();
    sharedM.o2 = PR_NewMonitor();

    base =  PR_IntervalNow();

    PR_EnterMonitor(sharedM.o1);
    LogNow("T1 waiting 3 seconds on o1", PR_SUCCESS);
    rv = PR_Wait(sharedM.o1, PR_SecondsToInterval(3));
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o1", rv);
    else LogNow("T1 wait on o1 failed", rv);
    PR_ExitMonitor(sharedM.o1);

    LogNow("T1 creating T2", PR_SUCCESS);
    t2 = PR_CreateThread(
        PR_USER_THREAD, T2Mon, &sharedM, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    LogNow("T1 creating T3", PR_SUCCESS);
    t3 = PR_CreateThread(
        PR_USER_THREAD, T3Mon, &sharedM, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    PR_EnterMonitor(sharedM.o2);
    LogNow("T1 waiting forever on o2", PR_SUCCESS);
    rv = PR_Wait(sharedM.o2, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o2", rv);
    else LogNow("T1 wait on o2 failed", rv);
    PR_ExitMonitor(sharedM.o2);

    (void)PR_JoinThread(t2);
    (void)PR_JoinThread(t3);

    PR_DestroyMonitor(sharedM.o1);
    PR_DestroyMonitor(sharedM.o2);

}  /* T1Mon */

static void PR_CALLBACK T2Lock(void *arg)
{
    PRStatus rv;
    LockShared *shared = (LockShared*)arg;

    PR_Lock(shared->o1);
    LogNow("T2 waiting 5 seconds on o1", PR_SUCCESS);
    rv = PR_WaitCondVar(shared->cv1, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T2 resuming on o1", rv);
    else LogNow("T2 wait failed on o1", rv);

    rv = PR_NotifyCondVar(shared->cv1);
    if (PR_SUCCESS == rv) LogNow("T2 notified o1", rv);
    else LogNow("T2 notify on o1 failed", rv);

    PR_Unlock(shared->o1);
}  /* T2Lock */

static void PR_CALLBACK T3Lock(void *arg)
{
    PRStatus rv;
    LockShared *shared = (LockShared*)arg;

    PR_Lock(shared->o2);
    LogNow("T3 waiting 5 seconds on o2", PR_SUCCESS);
    rv = PR_WaitCondVar(shared->cv2, PR_SecondsToInterval(5));
    if (PR_SUCCESS == rv) LogNow("T3 resuming on o2", rv);
    else LogNow("T3 wait failed on o2", rv);
    rv = PR_NotifyCondVar(shared->cv2);
    LogNow("T3 notify on o2", rv);
    PR_Unlock(shared->o2);

}  /* T3Lock */

/*
** Make shared' a static variable for Win16
*/
static LockShared sharedL;

static void T1Lock(void)
{
    PRStatus rv;
    PRThread *t2, *t3;
    sharedL.o1 = PR_NewLock();
    sharedL.o2 = PR_NewLock();
    sharedL.cv1 = PR_NewCondVar(sharedL.o1);
    sharedL.cv2 = PR_NewCondVar(sharedL.o2);

    PR_fprintf(err, "\n**********************************\n");
    PR_fprintf(err, "             LOCKS\n");
    PR_fprintf(err, "**********************************\n");

    base =  PR_IntervalNow();

    PR_Lock(sharedL.o1);
    LogNow("T1 waiting 3 seconds on o1", PR_SUCCESS);
    rv = PR_WaitCondVar(sharedL.cv1, PR_SecondsToInterval(3));
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o1", rv);
    else LogNow("T1 wait on o1 failed", rv);
    PR_Unlock(sharedL.o1);

    LogNow("T1 creating T2", PR_SUCCESS);
    t2 = PR_CreateThread(
        PR_USER_THREAD, T2Lock, &sharedL, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    LogNow("T1 creating T3", PR_SUCCESS);
    t3 = PR_CreateThread(
        PR_USER_THREAD, T3Lock, &sharedL, PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);

    PR_Lock(sharedL.o2);
    LogNow("T1 waiting forever on o2", PR_SUCCESS);
    rv = PR_WaitCondVar(sharedL.cv2, PR_INTERVAL_NO_TIMEOUT);
    if (PR_SUCCESS == rv) LogNow("T1 resuming on o2", rv);
    else LogNow("T1 wait on o2 failed", rv);
    PR_Unlock(sharedL.o2);

    (void)PR_JoinThread(t2);
    (void)PR_JoinThread(t3);

    PR_DestroyLock(sharedL.o1);
    PR_DestroyLock(sharedL.o2);
    PR_DestroyCondVar(sharedL.cv1);
    PR_DestroyCondVar(sharedL.cv2);
}  /* T1Lock */

static PRIntn PR_CALLBACK RealMain( PRIntn argc, char **argv )
{
	PLOptStatus os;
  	PLOptState *opt = PL_CreateOptState(argc, argv, "dhlmc");
	PRBool locks = PR_FALSE, monitors = PR_FALSE, cmonitors = PR_FALSE;

    err = PR_GetSpecialFD(PR_StandardError);

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode (noop) */
            break;
        case 'l':  /* locks */
			locks = PR_TRUE;
            break;
        case 'm':  /* monitors */
			monitors = PR_TRUE;
            break;
        case 'c':  /* cached monitors */
			cmonitors = PR_TRUE;
            break;
        case 'h':  /* needs guidance */
         default:
            Help();
            return 2;
        }
    }
	PL_DestroyOptState(opt);

    ml = PR_NewLock();
    if (locks) T1Lock();
    if (monitors) T1Mon();
    if (cmonitors) T1CMon();

    PR_DestroyLock(ml);

    PR_fprintf(err, "Done!\n");    
    return 0;
}  /* main */


PRIntn main(PRIntn argc, char **argv)
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
/* xnotify.c */
