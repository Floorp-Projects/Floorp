/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is a test for the io continuation thread machinery
 * in pthreads.
 */

#include "nspr.h"
#include <stdio.h>

int num_threads = 10;  /* must be an even number */
PRThreadScope thread_scope = PR_GLOBAL_THREAD;

void ThreadFunc(void *arg)
{
    PRFileDesc *fd = (PRFileDesc *) arg;
    char buf[1024];
    PRInt32 nbytes;
    PRErrorCode err;

    nbytes = PR_Recv(fd, buf, sizeof(buf), 0, PR_SecondsToInterval(20));
    if (nbytes == -1) {
        err = PR_GetError();
        if (err != PR_PENDING_INTERRUPT_ERROR) {
            fprintf(stderr, "PR_Recv failed: (%d, %d)\n",
                    err, PR_GetOSError());
            PR_ProcessExit(1);
        }
        /*
         * After getting an I/O interrupt, this thread must
         * close the fd before it exits due to a limitation
         * of our NT implementation.
         */
        if (PR_Close(fd) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            PR_ProcessExit(1);
        }
    } else {
        fprintf(stderr, "PR_Recv received %d bytes!?\n", nbytes);
        PR_ProcessExit(1);
    }
}

int main(int argc, char **argv)
{
    PRFileDesc **fds;
    PRThread **threads;
    PRIntervalTime start, elapsed;
    int index;

    fds = (PRFileDesc **) PR_MALLOC(2 * num_threads * sizeof(PRFileDesc *));
    PR_ASSERT(fds != NULL);
    threads = (PRThread **) PR_MALLOC(num_threads * sizeof(PRThread *));
    PR_ASSERT(threads != NULL);

    for (index = 0; index < num_threads; index++) {
        if (PR_NewTCPSocketPair(&fds[2 * index]) == PR_FAILURE) {
            fprintf(stderr, "PR_NewTCPSocket failed\n");
            PR_ProcessExit(1);
        }
        threads[index] = PR_CreateThread(
                             PR_USER_THREAD, ThreadFunc, fds[2 * index],
                             PR_PRIORITY_NORMAL, thread_scope, PR_JOINABLE_THREAD, 0);
        if (NULL == threads[index]) {
            fprintf(stderr, "PR_CreateThread failed\n");
            PR_ProcessExit(1);
        }
    }

    /* Let the threads block in PR_Recv */
    PR_Sleep(PR_SecondsToInterval(2));

    printf("Interrupting the threads\n");
    fflush(stdout);
    start = PR_IntervalNow();
    for (index = 0; index < num_threads; index++) {
        if (PR_Interrupt(threads[index]) == PR_FAILURE) {
            fprintf(stderr, "PR_Interrupt failed\n");
            PR_ProcessExit(1);
        }
    }
    for (index = 0; index < num_threads; index++) {
        if (PR_JoinThread(threads[index]) == PR_FAILURE) {
            fprintf(stderr, "PR_JoinThread failed\n");
            PR_ProcessExit(1);
        }
    }
    elapsed = (PRIntervalTime)(PR_IntervalNow() - start);
    printf("Threads terminated in %d milliseconds\n",
           PR_IntervalToMilliseconds(elapsed));
    fflush(stdout);

    /* We are being very generous and allow 10 seconds. */
    if (elapsed >= PR_SecondsToInterval(10)) {
        fprintf(stderr, "Interrupting threads took longer than 10 seconds!!\n");
        PR_ProcessExit(1);
    }

    for (index = 0; index < num_threads; index++) {
        /* fds[2 * index] was passed to and closed by threads[index]. */
        if (PR_Close(fds[2 * index + 1]) == PR_FAILURE) {
            fprintf(stderr, "PR_Close failed\n");
            PR_ProcessExit(1);
        }
    }
    PR_DELETE(threads);
    PR_DELETE(fds);
    printf("PASS\n");
    PR_Cleanup();
    return 0;
}
