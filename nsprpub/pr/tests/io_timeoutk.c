/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** name io_timeoutk.c
** Description:Test socket IO timeouts (kernel level)
**
** Modification History:
** 19-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**          recognize the return code from tha main program.
***********************************************************************/
/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include <stdio.h>
#include "nspr.h"

#define NUM_THREADS 1
#define BASE_PORT   8000
#define DEFAULT_ACCEPT_TIMEOUT 2

typedef struct threadInfo {
    PRInt16 id;
    PRInt16 accept_timeout;
    PRLock *dead_lock;
    PRCondVar *dead_cv;
    PRInt32   *alive;
} threadInfo;

PRIntn failed_already=0;
PRIntn debug_mode;


void
thread_main(void *_info)
{
    threadInfo *info = (threadInfo *)_info;
    PRNetAddr listenAddr;
    PRNetAddr clientAddr;
    PRFileDesc *listenSock = NULL;
    PRFileDesc *clientSock;
    PRStatus rv;

    if (debug_mode) {
        printf("thread %d is alive\n", info->id);
    }

    listenSock = PR_NewTCPSocket();
    if (!listenSock) {
        if (debug_mode) {
            printf("unable to create listen socket\n");
        }
        goto dead;
    }

    listenAddr.inet.family = AF_INET;
    listenAddr.inet.port = PR_htons(BASE_PORT + info->id);
    listenAddr.inet.ip = PR_htonl(INADDR_ANY);
    rv = PR_Bind(listenSock, &listenAddr);
    if (rv == PR_FAILURE) {
        if (debug_mode) {
            printf("unable to bind\n");
        }
        goto dead;
    }

    rv = PR_Listen(listenSock, 4);
    if (rv == PR_FAILURE) {
        if (debug_mode) {
            printf("unable to listen\n");
        }
        goto dead;
    }

    if (debug_mode) printf("thread %d going into accept for %d seconds\n",
                               info->id, info->accept_timeout + info->id);

    clientSock = PR_Accept(listenSock, &clientAddr, PR_SecondsToInterval(info->accept_timeout +info->id));

    if (clientSock == NULL) {
        if (PR_GetError() == PR_IO_TIMEOUT_ERROR)
            if (debug_mode) {
                printf("PR_Accept() timeout worked!\n");
                printf("TEST FAILED! PR_Accept() returned error %d\n",
                       PR_GetError());
            }
            else {
                failed_already=1;
            }
    } else {
        if (debug_mode) {
            printf ("TEST FAILED! PR_Accept() succeeded?\n");
        }
        else {
            failed_already=1;
        }
        PR_Close(clientSock);
    }

dead:
    if (listenSock) {
        PR_Close(listenSock);
    }
    PR_Lock(info->dead_lock);
    (*info->alive)--;
    PR_NotifyCondVar(info->dead_cv);
    PR_Unlock(info->dead_lock);

    if (debug_mode) {
        printf("thread %d is dead\n", info->id);
    }
}

void
thread_test(PRInt32 scope, PRInt32 num_threads)
{
    PRInt32 index;
    PRThread *thr;
    PRLock *dead_lock;
    PRCondVar *dead_cv;
    PRInt32 alive;

    if (debug_mode) {
        printf("IO Timeout test started with %d threads\n", num_threads);
    }

    dead_lock = PR_NewLock();
    dead_cv = PR_NewCondVar(dead_lock);
    alive = num_threads;

    for (index = 0; index < num_threads; index++) {
        threadInfo *info = (threadInfo *)malloc(sizeof(threadInfo));

        info->id = index;
        info->dead_lock = dead_lock;
        info->dead_cv = dead_cv;
        info->alive = &alive;
        info->accept_timeout = DEFAULT_ACCEPT_TIMEOUT;

        thr = PR_CreateThread( PR_USER_THREAD,
                               thread_main,
                               (void *)info,
                               PR_PRIORITY_NORMAL,
                               scope,
                               PR_UNJOINABLE_THREAD,
                               0);

        if (!thr) {
            PR_Lock(dead_lock);
            alive--;
            PR_Unlock(dead_lock);
        }
    }

    PR_Lock(dead_lock);
    while(alive) {
        if (debug_mode) {
            printf("main loop awake; alive = %d\n", alive);
        }
        PR_WaitCondVar(dead_cv, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(dead_lock);
}

int main(int argc, char **argv)
{
    PRInt32 num_threads;

    /* The command line argument: -d is used to determine if the test is being run
    in debug mode. The regress tool requires only one line output:PASS or FAIL.
    All of the printfs associated with this test has been handled with a if (debug_mode)
    test.
    Usage: test_name -d
    */
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d:");
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
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);

    /* main test */

    if (argc > 2) {
        num_threads = atoi(argv[2]);
    }
    else {
        num_threads = NUM_THREADS;
    }

    PR_Init(PR_USER_THREAD, PR_PRIORITY_LOW, 0);
    PR_STDIO_INIT();

    if (debug_mode) {
        printf("kernel level test\n");
    }
    thread_test(PR_GLOBAL_THREAD, num_threads);

    PR_Cleanup();

    if(failed_already) {
        return 1;
    }
    else {
        return 0;
    }

}
