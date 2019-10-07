/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:        intrio.c
 * Purpose:     testing i/o interrupts (see Bugzilla bug #31120)
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* for synchronization between the main thread and iothread */
static PRLock *lock;
static PRCondVar *cvar;
static PRBool iothread_ready;

static void PR_CALLBACK AbortIO(void *arg)
{
    PRStatus rv;
    PR_Sleep(PR_SecondsToInterval(2));
    rv = PR_Interrupt((PRThread*)arg);
    PR_ASSERT(PR_SUCCESS == rv);
}  /* AbortIO */

static void PR_CALLBACK IOThread(void *arg)
{
    PRFileDesc *sock, *newsock;
    PRNetAddr addr;

    sock = PR_OpenTCPSocket(PR_AF_INET6);
    if (sock == NULL) {
        fprintf(stderr, "PR_OpenTCPSocket failed\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    if (PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_Bind(sock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    if (PR_Listen(sock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }
    /* tell the main thread that we are ready */
    PR_Lock(lock);
    iothread_ready = PR_TRUE;
    PR_NotifyCondVar(cvar);
    PR_Unlock(lock);
    newsock = PR_Accept(sock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (newsock != NULL) {
        fprintf(stderr, "PR_Accept shouldn't have succeeded\n");
        exit(1);
    }
    if (PR_GetError() != PR_PENDING_INTERRUPT_ERROR) {
        fprintf(stderr, "PR_Accept failed (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        exit(1);
    }
    printf("PR_Accept() is interrupted as expected\n");
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
}

static void Test(PRThreadScope scope1, PRThreadScope scope2)
{
    PRThread *iothread, *abortio;

    printf("A %s thread will be interrupted by a %s thread\n",
           (scope1 == PR_LOCAL_THREAD ? "local" : "global"),
           (scope2 == PR_LOCAL_THREAD ? "local" : "global"));
    iothread_ready = PR_FALSE;
    iothread = PR_CreateThread(
                   PR_USER_THREAD, IOThread, NULL, PR_PRIORITY_NORMAL,
                   scope1, PR_JOINABLE_THREAD, 0);
    if (iothread == NULL) {
        fprintf(stderr, "cannot create thread\n");
        exit(1);
    }
    PR_Lock(lock);
    while (!iothread_ready) {
        PR_WaitCondVar(cvar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(lock);
    abortio = PR_CreateThread(
                  PR_USER_THREAD, AbortIO, iothread, PR_PRIORITY_NORMAL,
                  scope2, PR_JOINABLE_THREAD, 0);
    if (abortio == NULL) {
        fprintf(stderr, "cannot create thread\n");
        exit(1);
    }
    if (PR_JoinThread(iothread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(abortio) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
    lock = PR_NewLock();
    if (lock == NULL) {
        fprintf(stderr, "PR_NewLock failed\n");
        exit(1);
    }
    cvar = PR_NewCondVar(lock);
    if (cvar == NULL) {
        fprintf(stderr, "PR_NewCondVar failed\n");
        exit(1);
    }
    /* test all four combinations */
    Test(PR_LOCAL_THREAD, PR_LOCAL_THREAD);
    Test(PR_LOCAL_THREAD, PR_GLOBAL_THREAD);
    Test(PR_GLOBAL_THREAD, PR_LOCAL_THREAD);
    Test(PR_GLOBAL_THREAD, PR_GLOBAL_THREAD);
    printf("PASSED\n");
    return 0;
}  /* main */
