/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:        primblok.c
 * Purpose:     testing whether the primordial thread can block in a
 *              native blocking function without affecting the correct
 *              functioning of NSPR I/O functions (Bugzilla bug #30746)
 */

#if !defined(WINNT)

#include <stdio.h>

int main(int argc, char **argv)
{
    printf("This test is not relevant on this platform\n");
    return 0;
}

#else /* WINNT */

#include "nspr.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE_NAME "primblok.dat"

/* use InterlockedExchange to update this variable */
static LONG iothread_done;

static void PR_CALLBACK IOThread(void *arg)
{
    PRFileDesc *fd;
    char buf[32];
    PRInt32 nbytes;

    /* Give the primordial thread one second to block */
    Sleep(1000);

    /*
     * See if our PR_Write call will hang when the primordial
     * thread is blocking in a native blocking function.
     */
    fd = PR_Open(TEST_FILE_NAME, PR_WRONLY|PR_CREATE_FILE, 0666);
    if (NULL == fd) {
        fprintf(stderr, "PR_Open failed\n");
        exit(1);
    }
    memset(buf, 0xaf, sizeof(buf));
    fprintf(stderr, "iothread: calling PR_Write\n");
    nbytes = PR_Write(fd, buf, sizeof(buf));
    fprintf(stderr, "iothread: PR_Write returned\n");
    if (nbytes != sizeof(buf)) {
        fprintf(stderr, "PR_Write returned %d\n", nbytes);
        exit(1);
    }
    if (PR_Close(fd) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (PR_Delete(TEST_FILE_NAME) == PR_FAILURE) {
        fprintf(stderr, "PR_Delete failed\n");
        exit(1);
    }

    /* Tell the main thread that we are done */
    InterlockedExchange(&iothread_done, 1);
}

int main(int argc, char **argv)
{
    PRThread *iothread;

    /* Must be a global thread */
    iothread = PR_CreateThread(
                   PR_USER_THREAD, IOThread, NULL, PR_PRIORITY_NORMAL,
                   PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (iothread == NULL) {
        fprintf(stderr, "cannot create thread\n");
        exit(1);
    }

    /*
     * Block in a native blocking function.
     * Give iothread 5 seconds to finish its task.
     */
    Sleep(5000);

    /*
     * Is iothread done or is it hung?
     *
     * I'm actually only interested in reading the value
     * of iothread_done.  I'm using InterlockedExchange as
     * a thread-safe way to read iothread_done.
     */
    if (InterlockedExchange(&iothread_done, 1) == 0) {
        fprintf(stderr, "iothread is hung\n");
        fprintf(stderr, "FAILED\n");
        exit(1);
    }

    if (PR_JoinThread(iothread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    printf("PASSED\n");
    return 0;
}  /* main */

#endif /* WINNT */
