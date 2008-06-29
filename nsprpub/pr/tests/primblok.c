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
 * Portions created by the Initial Developer are Copyright (C) 2000
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
 * File:        primblok.c
 * Purpose:     testing whether the primordial thread can block in a
 *              native blocking function without affecting the correct
 *              functioning of NSPR I/O functions (Bugzilla bug #30746)
 */

#if !defined(WINNT)

#include <stdio.h>

int main()
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

int main()
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
