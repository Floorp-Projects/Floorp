/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test program demonstrates that PR_ExitMonitor needs to add a
 * reference to the PRMonitor object before unlocking the internal
 * mutex.
 */

#include "prlog.h"
#include "prmon.h"
#include "prthread.h"

#include <stdio.h>
#include <stdlib.h>

/* Protected by the PRMonitor 'mon' in the main function. */
static PRBool done = PR_FALSE;

static void ThreadFunc(void *arg)
{
    PRMonitor *mon = (PRMonitor *)arg;
    PRStatus rv;

    PR_EnterMonitor(mon);
    done = PR_TRUE;
    rv = PR_Notify(mon);
    PR_ASSERT(rv == PR_SUCCESS);
    rv = PR_ExitMonitor(mon);
    PR_ASSERT(rv == PR_SUCCESS);
}

int main()
{
    PRMonitor *mon;
    PRThread *thread;
    PRStatus rv;

    mon = PR_NewMonitor();
    if (!mon) {
        fprintf(stderr, "PR_NewMonitor failed\n");
        exit(1);
    }

    thread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, mon,
                             PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                             PR_JOINABLE_THREAD, 0);
    if (!thread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }

    PR_EnterMonitor(mon);
    while (!done) {
        rv = PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(rv == PR_SUCCESS);
    }
    rv = PR_ExitMonitor(mon);
    PR_ASSERT(rv == PR_SUCCESS);

    /*
     * Do you agree it should be safe to destroy 'mon' now?
     * See bug 844784 comment 27.
     */
    PR_DestroyMonitor(mon);

    rv = PR_JoinThread(thread);
    PR_ASSERT(rv == PR_SUCCESS);

    printf("PASS\n");
    return 0;
}
