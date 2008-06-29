/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A test for the pollable events.
 *
 * A number of threads are in a ring configuration, each waiting on
 * a pollable event that is set by its upstream neighbor.
 */

#include "prinit.h"
#include "prio.h"
#include "prthread.h"
#include "prerror.h"
#include "prmem.h"
#include "prlog.h"
#include "prprf.h"

#include "plgetopt.h"

#include <stdlib.h>

#define DEFAULT_THREADS 10
#define DEFAULT_LOOPS 100

PRIntn numThreads = DEFAULT_THREADS;
PRIntn numIterations = DEFAULT_LOOPS;
PRIntervalTime dally = PR_INTERVAL_NO_WAIT;
PRFileDesc *debug_out = NULL;
PRBool debug_mode = PR_FALSE;
PRBool verbosity = PR_FALSE;

typedef struct ThreadData {
    PRFileDesc *event;
    int index;
    struct ThreadData *next;
} ThreadData;

void ThreadRoutine(void *arg)
{
    ThreadData *data = (ThreadData *) arg;
    PRIntn i;
    PRPollDesc pd;
    PRInt32 rv;

    pd.fd = data->event;
    pd.in_flags = PR_POLL_READ;

    for (i = 0; i < numIterations; i++) {
        rv = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
        if (rv == -1) {
            PR_fprintf(PR_STDERR, "PR_Poll failed\n");
            exit(1);
        }
        if (verbosity) {
            PR_fprintf(debug_out, "thread %d awakened\n", data->index);
        }
        PR_ASSERT(rv != 0);
        PR_ASSERT(pd.out_flags & PR_POLL_READ);
        if (PR_WaitForPollableEvent(data->event) == PR_FAILURE) {
            PR_fprintf(PR_STDERR, "consume event failed\n");
            exit(1);
        }
        if (dally != PR_INTERVAL_NO_WAIT) {
            PR_Sleep(dally);
        }
        if (verbosity) {
            PR_fprintf(debug_out, "thread %d posting event\n", data->index);
        }
        if (PR_SetPollableEvent(data->next->event) == PR_FAILURE) {
            PR_fprintf(PR_STDERR, "post event failed\n");
            exit(1);
        }
    }
}

static void Help(void)
{
    debug_out = PR_STDOUT;

    PR_fprintf(
            debug_out, "Usage: pollable [-c n] [-t n] [-d] [-v] [-G] [-C n] [-D n]\n");
    PR_fprintf(
            debug_out, "-c n\tloops at thread level (default: %d)\n", DEFAULT_LOOPS);
    PR_fprintf(
            debug_out, "-t n\tnumber of threads (default: %d)\n", DEFAULT_THREADS);
    PR_fprintf(debug_out, "-d\tturn on debugging output (default: FALSE)\n");
    PR_fprintf(debug_out, "-v\tturn on verbose output (default: FALSE)\n");
    PR_fprintf(debug_out, "-G\tglobal threads only (default: FALSE)\n");
    PR_fprintf(debug_out, "-C n\tconcurrency setting (default: 1)\n");
    PR_fprintf(debug_out, "-D n\tdally setting (msecs) (default: 0)\n");
}  /* Help */

int main(int argc, char **argv)
{
    ThreadData selfData;
    ThreadData *data;
    PRThread **thread;
    void *block;
    PRIntn i;
    PRIntervalTime timeStart, timeEnd;
    PRPollDesc pd;
    PRInt32 rv;
    PRThreadScope thread_scope = PR_LOCAL_THREAD;
    PRBool help = PR_FALSE;
    PRUintn concurrency = 1;
    PRUintn average;
    PLOptStatus os;
    PLOptState *opt;

    PR_STDIO_INIT();

    opt = PL_CreateOptState(argc, argv, "hdvc:t:C:GD:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt))) {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option) {
            case 'v':  /* verbose mode */
                verbosity = PR_TRUE;
            case 'd':  /* debug mode */
                debug_mode = PR_TRUE;
                break;
            case 'c':  /* loop counter */
                numIterations = atoi(opt->value);
                break;
            case 't':  /* thread limit */
                numThreads = atoi(opt->value);
                break;
            case 'C':  /* Concurrency limit */
                concurrency = atoi(opt->value);
                break;
            case 'G':  /* global threads only */
                thread_scope = PR_GLOBAL_THREAD;
                break;
            case 'D':  /* dally */
                dally = PR_MillisecondsToInterval(atoi(opt->value));
                break;
            case 'h':  /* help message */
                Help();
                help = PR_TRUE;
                break;
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);

    if (help) {
        return 1;
    }

    if (concurrency > 1) {
        PR_SetConcurrency(concurrency);
    }

    if (PR_TRUE == debug_mode) {
        debug_out = PR_STDOUT;
	PR_fprintf(debug_out, "Test parameters\n");
        PR_fprintf(debug_out, "\tThreads involved: %d\n", numThreads);
        PR_fprintf(debug_out, "\tIteration limit: %d\n", numIterations);
        PR_fprintf(debug_out, "\tConcurrency: %d\n", concurrency);
        PR_fprintf(debug_out, "\tThread type: %s\n",
                (PR_GLOBAL_THREAD == thread_scope) ? "GLOBAL" : "LOCAL");
    }

    /*
     * Malloc a block of memory and divide it into data and thread.
     */
    block = PR_MALLOC(numThreads * (sizeof(ThreadData) + sizeof(PRThread *)));
    if (block == NULL) {
        PR_fprintf(PR_STDERR, "cannot malloc, failed\n");
        exit(1);
    }
    data = (ThreadData *) block;
    thread = (PRThread **) &data[numThreads];

    /* Pollable event */
    selfData.event = PR_NewPollableEvent();
    if (selfData.event == NULL) {
        PR_fprintf(PR_STDERR, "cannot create event: (%ld, %ld)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    selfData.next = &data[0];
    for (i = 0; i < numThreads; i++) {
        data[i].event = PR_NewPollableEvent();
        if (data[i].event == NULL) {
            PR_fprintf(PR_STDERR, "cannot create event: (%ld, %ld)\n",
                    PR_GetError(), PR_GetOSError());
            exit(1);
        }
        data[i].index = i;
        if (i != numThreads - 1) {
            data[i].next = &data[i + 1];
        } else {
            data[i].next = &selfData;
        }

        thread[i] = PR_CreateThread(PR_USER_THREAD,
                ThreadRoutine, &data[i], PR_PRIORITY_NORMAL,
                thread_scope, PR_JOINABLE_THREAD, 0);
        if (thread[i] == NULL) {
            PR_fprintf(PR_STDERR, "cannot create thread\n");
            exit(1);
        }
    }

    timeStart = PR_IntervalNow();
    pd.fd = selfData.event;
    pd.in_flags = PR_POLL_READ;
    for (i = 0; i < numIterations; i++) {
        if (dally != PR_INTERVAL_NO_WAIT) {
            PR_Sleep(dally);
        }
        if (verbosity) {
            PR_fprintf(debug_out, "main thread posting event\n");
        }
        if (PR_SetPollableEvent(selfData.next->event) == PR_FAILURE) {
            PR_fprintf(PR_STDERR, "set event failed\n");
            exit(1);
        }
        rv = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
        if (rv == -1) {
            PR_fprintf(PR_STDERR, "wait failed\n");
            exit(1);
        }
        PR_ASSERT(rv != 0);
        PR_ASSERT(pd.out_flags & PR_POLL_READ);
        if (verbosity) {
            PR_fprintf(debug_out, "main thread awakened\n");
        }
	if (PR_WaitForPollableEvent(selfData.event) == PR_FAILURE) {
            PR_fprintf(PR_STDERR, "consume event failed\n");
            exit(1);
        }
    }
    timeEnd = PR_IntervalNow();

    if (debug_mode) {
        average = PR_IntervalToMicroseconds(timeEnd - timeStart)
                / (numIterations * numThreads);
        PR_fprintf(debug_out, "Average switch times %d usecs for %d threads\n",
                average, numThreads);
    }

    for (i = 0; i < numThreads; i++) {
        if (PR_JoinThread(thread[i]) == PR_FAILURE) {
            PR_fprintf(PR_STDERR, "join thread failed\n");
            exit(1);
        }
        PR_DestroyPollableEvent(data[i].event);
    }
    PR_DELETE(block);
	PR_DestroyPollableEvent(selfData.event);

    PR_fprintf(PR_STDOUT, "PASSED\n");
    return 0;
}
