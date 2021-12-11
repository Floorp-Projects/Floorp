/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1996 - Netscape Communications Corporation
**
** Name: cvar.c
**
** Description: Tests Condition Variable Operations
**
** Modification History:
** 13-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**          recognize the return code from tha main program.
** 12-June-97 Revert to return code 0 and 1.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/

#include "nspr.h"

/* Used to get the command line option */
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PRMonitor *mon;
#define DEFAULT_COUNT   1000
PRInt32 count = 0;
PRIntn debug_mode;

#define kQSIZE  1

typedef struct {
    PRLock      *bufLock;
    int         startIdx;
    int         numFull;
    PRCondVar   *notFull;
    PRCondVar   *notEmpty;
    void        *data[kQSIZE];
} CircBuf;

static PRBool failed = PR_FALSE;

/*
** NewCB creates and initializes a new circular buffer.
*/
static CircBuf* NewCB(void)
{
    CircBuf     *cbp;

    cbp = PR_NEW(CircBuf);
    if (cbp == NULL) {
        return (NULL);
    }

    cbp->bufLock    = PR_NewLock();
    cbp->startIdx   = 0;
    cbp->numFull    = 0;
    cbp->notFull    = PR_NewCondVar(cbp->bufLock);
    cbp->notEmpty   = PR_NewCondVar(cbp->bufLock);

    return (cbp);
}

/*
** DeleteCB frees a circular buffer.
*/
static void DeleteCB(CircBuf *cbp)
{
    PR_DestroyLock(cbp->bufLock);
    PR_DestroyCondVar(cbp->notFull);
    PR_DestroyCondVar(cbp->notEmpty);
    PR_DELETE(cbp);
}


/*
** PutCBData puts new data on the queue.  If the queue is full, it waits
** until there is room.
*/
static void PutCBData(CircBuf *cbp, void *data)
{
    PR_Lock(cbp->bufLock);
    /* wait while the buffer is full */
    while (cbp->numFull == kQSIZE) {
        PR_WaitCondVar(cbp->notFull,PR_INTERVAL_NO_TIMEOUT);
    }
    cbp->data[(cbp->startIdx + cbp->numFull) % kQSIZE] = data;
    cbp->numFull += 1;

    /* let a waiting reader know that there is data */
    PR_NotifyCondVar(cbp->notEmpty);
    PR_Unlock(cbp->bufLock);

}


/*
** GetCBData gets the oldest data on the queue.  If the queue is empty, it waits
** until new data appears.
*/
static void* GetCBData(CircBuf *cbp)
{
    void *data;

    PR_Lock(cbp->bufLock);
    /* wait while the buffer is empty */
    while (cbp->numFull == 0) {
        PR_WaitCondVar(cbp->notEmpty,PR_INTERVAL_NO_TIMEOUT);
    }
    data = cbp->data[cbp->startIdx];
    cbp->startIdx =(cbp->startIdx + 1) % kQSIZE;
    cbp->numFull -= 1;

    /* let a waiting writer know that there is room */
    PR_NotifyCondVar(cbp->notFull);
    PR_Unlock(cbp->bufLock);

    return (data);
}


/************************************************************************/

static int alive;

static void PR_CALLBACK CXReader(void *arg)
{
    CircBuf *cbp = (CircBuf *)arg;
    PRInt32 i, n;
    void *data;

    n = count / 2;
    for (i = 0; i < n; i++) {
        data = GetCBData(cbp);
        if ((int)data != i)
            if (debug_mode) {
                printf("data mismatch at for i = %d usec\n", i);
            }
    }

    PR_EnterMonitor(mon);
    --alive;
    PR_Notify(mon);
    PR_ExitMonitor(mon);
}

static void PR_CALLBACK CXWriter(void *arg)
{
    CircBuf *cbp = (CircBuf *)arg;
    PRInt32 i, n;

    n = count / 2;
    for (i = 0; i < n; i++) {
        PutCBData(cbp, (void *)i);
    }

    PR_EnterMonitor(mon);
    --alive;
    PR_Notify(mon);
    PR_ExitMonitor(mon);
}

static void CondWaitContextSwitch(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *t1, *t2;
    CircBuf *cbp;

    PR_EnterMonitor(mon);

    alive = 2;

    cbp =  NewCB();

    t1 = PR_CreateThread(PR_USER_THREAD,
                         CXReader, cbp,
                         PR_PRIORITY_NORMAL,
                         scope1,
                         PR_UNJOINABLE_THREAD,
                         0);
    PR_ASSERT(t1);
    t2 = PR_CreateThread(PR_USER_THREAD,
                         CXWriter, cbp,
                         PR_PRIORITY_NORMAL,
                         scope2,
                         PR_UNJOINABLE_THREAD,
                         0);
    PR_ASSERT(t2);

    /* Wait for both of the threads to exit */
    while (alive) {
        PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }

    DeleteCB(cbp);

    PR_ExitMonitor(mon);
}

static void CondWaitContextSwitchUU(void)
{
    CondWaitContextSwitch(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
}

static void CondWaitContextSwitchUK(void)
{
    CondWaitContextSwitch(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
}

static void CondWaitContextSwitchKK(void)
{
    CondWaitContextSwitch(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
}

/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);

    if (debug_mode) {
        printf("%40s: %6.2f usec\n", msg, d / count);
    }

    if (0 ==  d) {
        failed = PR_TRUE;
    }
}

static PRIntn PR_CALLBACK RealMain(int argc, char **argv)
{
    /* The command line argument: -d is used to determine if the test is being run
    in debug mode. The regress tool requires only one line output:PASS or FAIL.
    All of the printfs associated with this test has been handled with a if (debug_mode)
    test.
    Usage: test_name [-d] [-c n]
    */
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dc:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 'd':  /* debug mode */
                debug_mode = 1;
                break;
            case 'c':  /* loop count */
                count = atoi(opt->value);
                break;
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);

    if (0 == count) {
        count = DEFAULT_COUNT;
    }

    mon = PR_NewMonitor();

    Measure(CondWaitContextSwitchUU, "cond var wait context switch- user/user");
    Measure(CondWaitContextSwitchUK, "cond var wait context switch- user/kernel");
    Measure(CondWaitContextSwitchKK, "cond var wait context switch- kernel/kernel");

    PR_DestroyMonitor(mon);

    if (debug_mode) {
        printf("%s\n", (failed) ? "FAILED" : "PASSED");
    }

    if(failed) {
        return 1;
    }
    else {
        return 0;
    }
}


int main(int argc, char *argv[])
{
    PRIntn rv;

    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */
