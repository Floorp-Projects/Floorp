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
 * Copyright (C) 2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * This test exercises the code that allocates and frees the syspoll_list
 * array of PRThread in the pthreads version.  This test is intended to be
 * run under Purify to verify that there is no memory leak.
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>

#define POLL_DESC_COUNT 256  /* This should be greater than the
                              * STACK_POLL_DESC_COUNT macro in
                              * ptio.c to cause syspoll_list to
                              * be created. */

static PRPollDesc pd[POLL_DESC_COUNT];

static void Test(void)
{
    int i;
    PRInt32 rv;
    PRIntervalTime timeout;

    timeout = PR_MillisecondsToInterval(10);
    /* cause syspoll_list to grow */
    for (i = 1; i <= POLL_DESC_COUNT; i++) {
        rv = PR_Poll(pd, i, timeout);
        if (rv != 0) {
            fprintf(stderr,
                "PR_Poll should time out but returns %d (%d, %d)\n",
                rv, PR_GetError(), PR_GetOSError());
            exit(1);
        }
    }
    /* syspoll_list should be large enough for all these */
    for (i = POLL_DESC_COUNT; i >= 1; i--) {
        rv = PR_Poll(pd, i, timeout);
        if (rv != 0) {
            fprintf(stderr, "PR_Poll should time out but returns %d\n", rv);
            exit(1);
        }
    }
}

static void ThreadFunc(void *arg)
{
    Test();
}

int main(int argc, char **argv)
{
    int i;
    PRThread *thread;

    for (i = 0; i < POLL_DESC_COUNT; i++) {
        pd[i].fd = NULL;
    }

    /* first run the test on the primordial thread */
    Test();

    /* then run the test on all three kinds of threads */
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, NULL,
            PR_PRIORITY_NORMAL, PR_GLOBAL_BOUND_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(thread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    PR_Cleanup();
    printf("PASS\n");
    return 0;
}
