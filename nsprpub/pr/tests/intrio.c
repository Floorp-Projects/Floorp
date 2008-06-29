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
 * File:        intrio.c
 * Purpose:     testing i/o interrupts (see Bugzilla bug #31120)
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

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
    while (!iothread_ready)
        PR_WaitCondVar(cvar, PR_INTERVAL_NO_TIMEOUT);
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

PRIntn main(PRIntn argc, char **argv)
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
