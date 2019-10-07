/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:        intrupt.c
 * Purpose:     testing thread interrupts
 */

#include "plgetopt.h"
#include "prcvar.h"
#include "prerror.h"
#include "prinit.h"
#include "prinrval.h"
#include "prio.h"
#include "prlock.h"
#include "prlog.h"
#include "prthread.h"
#include "prtypes.h"
#include "prnetdb.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_TCP_PORT 12500

static PRLock *ml = NULL;
static PRCondVar *cv = NULL;

static PRBool passed = PR_TRUE;
static PRBool debug_mode = PR_FALSE;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;

static void PR_CALLBACK AbortCV(void *arg)
{
    PRStatus rv;
    PRThread *me = PR_GetCurrentThread();

    /* some other thread (main) is doing the interrupt */
    PR_Lock(ml);
    rv = PR_WaitCondVar(cv, PR_INTERVAL_NO_TIMEOUT);
    if (debug_mode) {
        printf( "Expected interrupt on wait CV and ");
    }
    if (PR_FAILURE == rv)
    {
        if (PR_PENDING_INTERRUPT_ERROR == PR_GetError())
        {
            if (debug_mode) {
                printf("got it\n");
            }
        }
        else
        {
            if (debug_mode) {
                printf("got random error\n");
            }
            passed = PR_FALSE;
        }
    }
    else
    {
        if (debug_mode) {
            printf("got a successful completion\n");
        }
        passed = PR_FALSE;
    }

    rv = PR_WaitCondVar(cv, 10);
    if (debug_mode)
    {
        printf(
            "Expected success on wait CV and %s\n",
            (PR_SUCCESS == rv) ? "got it" : "failed");
    }
    passed = ((PR_TRUE == passed) && (PR_SUCCESS == rv)) ? PR_TRUE : PR_FALSE;

    /* interrupt myself, then clear */
    PR_Interrupt(me);
    PR_ClearInterrupt();
    rv = PR_WaitCondVar(cv, 10);
    if (debug_mode)
    {
        printf("Expected success on wait CV and ");
        if (PR_FAILURE == rv)
        {
            printf(
                "%s\n", (PR_PENDING_INTERRUPT_ERROR == PR_GetError()) ?
                "got interrupted" : "a random failure");
        }
        printf("got it\n");
    }
    passed = ((PR_TRUE == passed) && (PR_SUCCESS == rv)) ? PR_TRUE : PR_FALSE;

    /* set, then wait - interrupt - then wait again */
    PR_Interrupt(me);
    rv = PR_WaitCondVar(cv, 10);
    if (debug_mode) {
        printf( "Expected interrupt on wait CV and ");
    }
    if (PR_FAILURE == rv)
    {
        if (PR_PENDING_INTERRUPT_ERROR == PR_GetError())
        {
            if (debug_mode) {
                printf("got it\n");
            }
        }
        else
        {
            if (debug_mode) {
                printf("failed\n");
            }
            passed = PR_FALSE;
        }
    }
    else
    {
        if (debug_mode) {
            printf("got a successful completion\n");
        }
        passed = PR_FALSE;
    }

    rv = PR_WaitCondVar(cv, 10);
    if (debug_mode)
    {
        printf(
            "Expected success on wait CV and %s\n",
            (PR_SUCCESS == rv) ? "got it" : "failed");
    }
    passed = ((PR_TRUE == passed) && (PR_SUCCESS == rv)) ? PR_TRUE : PR_FALSE;

    PR_Unlock(ml);

}  /* AbortCV */

static void PR_CALLBACK AbortIO(void *arg)
{
    PRStatus rv;
    PR_Sleep(PR_SecondsToInterval(2));
    rv = PR_Interrupt((PRThread*)arg);
    PR_ASSERT(PR_SUCCESS == rv);
}  /* AbortIO */

static void PR_CALLBACK AbortJoin(void *arg)
{
}  /* AbortJoin */

static void setup_listen_socket(PRFileDesc **listner, PRNetAddr *netaddr)
{
    PRStatus rv;
    PRInt16 port = DEFAULT_TCP_PORT;

    *listner = PR_NewTCPSocket();
    PR_ASSERT(*listner != NULL);
    memset(netaddr, 0, sizeof(*netaddr));
    (*netaddr).inet.ip = PR_htonl(PR_INADDR_ANY);
    (*netaddr).inet.family = PR_AF_INET;
    do
    {
        (*netaddr).inet.port = PR_htons(port);
        rv = PR_Bind(*listner, netaddr);
        port += 1;
        PR_ASSERT(port < (DEFAULT_TCP_PORT + 10));
    } while (PR_FAILURE == rv);

    rv = PR_Listen(*listner, 5);

    if (PR_GetSockName(*listner, netaddr) < 0) {
        if (debug_mode) {
            printf("intrupt: ERROR - PR_GetSockName failed\n");
        }
        passed = PR_FALSE;
        return;
    }

}

static void PR_CALLBACK IntrBlock(void *arg)
{
    PRStatus rv;
    PRNetAddr netaddr;
    PRFileDesc *listner;

    /* some other thread (main) is doing the interrupt */
    /* block the interrupt */
    PR_BlockInterrupt();
    PR_Lock(ml);
    rv = PR_WaitCondVar(cv, PR_SecondsToInterval(4));
    PR_Unlock(ml);
    if (debug_mode)
    {
        printf("Expected success on wait CV and ");
        if (PR_FAILURE == rv)
        {
            printf(
                "%s\n", (PR_PENDING_INTERRUPT_ERROR == PR_GetError()) ?
                "got interrupted" : "got a random failure");
        } else {
            printf("got it\n");
        }
    }
    passed = ((PR_TRUE == passed) && (PR_SUCCESS == rv)) ? PR_TRUE : PR_FALSE;

    setup_listen_socket(&listner, &netaddr);
    PR_UnblockInterrupt();
    if (PR_Accept(listner, &netaddr, PR_INTERVAL_NO_TIMEOUT) == NULL)
    {
        PRInt32 error = PR_GetError();
        if (debug_mode) {
            printf("Expected interrupt on PR_Accept() and ");
        }
        if (PR_PENDING_INTERRUPT_ERROR == error)
        {
            if (debug_mode) {
                printf("got it\n");
            }
        }
        else
        {
            if (debug_mode) {
                printf("failed\n");
            }
            passed = PR_FALSE;
        }
    }
    else
    {
        if (debug_mode) {
            printf("Failed to interrupt PR_Accept()\n");
        }
        passed = PR_FALSE;
    }

    (void)PR_Close(listner); listner = NULL;
}  /* TestIntrBlock */

void PR_CALLBACK Intrupt(void *arg)
{
    PRStatus rv;
    PRNetAddr netaddr;
    PRFileDesc *listner;
    PRThread *abortCV, *abortIO, *abortJoin, *intrBlock;

    ml = PR_NewLock();
    cv = PR_NewCondVar(ml);

    /* Part I */
    if (debug_mode) {
        printf("Part I\n");
    }
    abortCV = PR_CreateThread(
                  PR_USER_THREAD, AbortCV, 0, PR_PRIORITY_NORMAL,
                  thread_scope, PR_JOINABLE_THREAD, 0);

    PR_Sleep(PR_SecondsToInterval(2));
    rv = PR_Interrupt(abortCV);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(abortCV);
    PR_ASSERT(PR_SUCCESS == rv);

    /* Part II */
    if (debug_mode) {
        printf("Part II\n");
    }
    abortJoin = PR_CreateThread(
                    PR_USER_THREAD, AbortJoin, 0, PR_PRIORITY_NORMAL,
                    thread_scope, PR_JOINABLE_THREAD, 0);
    PR_Sleep(PR_SecondsToInterval(2));
    if (debug_mode) {
        printf("Expecting to interrupt an exited thread ");
    }
    rv = PR_Interrupt(abortJoin);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(abortJoin);
    PR_ASSERT(PR_SUCCESS == rv);
    if (debug_mode) {
        printf("and succeeded\n");
    }

    /* Part III */
    if (debug_mode) {
        printf("Part III\n");
    }
    setup_listen_socket(&listner, &netaddr);
    abortIO = PR_CreateThread(
                  PR_USER_THREAD, AbortIO, PR_GetCurrentThread(), PR_PRIORITY_NORMAL,
                  thread_scope, PR_JOINABLE_THREAD, 0);

    if (PR_Accept(listner, &netaddr, PR_INTERVAL_NO_TIMEOUT) == NULL)
    {
        PRInt32 error = PR_GetError();
        if (debug_mode) {
            printf("Expected interrupt on PR_Accept() and ");
        }
        if (PR_PENDING_INTERRUPT_ERROR == error)
        {
            if (debug_mode) {
                printf("got it\n");
            }
        }
        else
        {
            if (debug_mode) {
                printf("failed\n");
            }
            passed = PR_FALSE;
        }
    }
    else
    {
        if (debug_mode) {
            printf("Failed to interrupt PR_Accept()\n");
        }
        passed = PR_FALSE;
    }

    (void)PR_Close(listner); listner = NULL;

    rv = PR_JoinThread(abortIO);
    PR_ASSERT(PR_SUCCESS == rv);
    /* Part VI */
    if (debug_mode) {
        printf("Part VI\n");
    }
    intrBlock = PR_CreateThread(
                    PR_USER_THREAD, IntrBlock, 0, PR_PRIORITY_NORMAL,
                    thread_scope, PR_JOINABLE_THREAD, 0);

    PR_Sleep(PR_SecondsToInterval(2));
    rv = PR_Interrupt(intrBlock);
    PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(intrBlock);
    PR_ASSERT(PR_SUCCESS == rv);

    PR_DestroyCondVar(cv);
    PR_DestroyLock(ml);
}  /* Intrupt */

int main(int argc, char **argv)
{
    PRThread *intrupt;
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dG");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'G':  /* use global threads */
                thread_scope = PR_GLOBAL_THREAD;
                break;
        }
    }
    PL_DestroyOptState(opt);
    PR_STDIO_INIT();
    intrupt = PR_CreateThread(
                  PR_USER_THREAD, Intrupt, NULL, PR_PRIORITY_NORMAL,
                  thread_scope, PR_JOINABLE_THREAD, 0);
    if (intrupt == NULL) {
        fprintf(stderr, "cannot create thread\n");
        passed = PR_FALSE;
    } else {
        PRStatus rv;
        rv = PR_JoinThread(intrupt);
        PR_ASSERT(rv == PR_SUCCESS);
    }
    printf("%s\n", ((passed) ? "PASSED" : "FAILED"));
    return ((passed) ? 0 : 1);
}  /* main */

/* intrupt.c */
