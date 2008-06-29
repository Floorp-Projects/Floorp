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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
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
 * File: multiacc.c
 *
 * Description:
 * This test creates multiple threads that accept on the
 * same listening socket.
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_SERVER_THREADS 10

static int num_server_threads = NUM_SERVER_THREADS;
static PRThreadScope thread_scope = PR_GLOBAL_THREAD;
static PRBool exit_flag = PR_FALSE;

static void ServerThreadFunc(void *arg)
{
    PRFileDesc *listenSock = (PRFileDesc *) arg;
    PRFileDesc *acceptSock;
    PRErrorCode err;
    PRStatus status;

    while (!exit_flag) {
        acceptSock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
        if (NULL == acceptSock) {
            err = PR_GetError();
            if (PR_PENDING_INTERRUPT_ERROR == err) {
                printf("server thread is interrupted\n");
                fflush(stdout);
                continue;
            }
            fprintf(stderr, "PR_Accept failed: %d\n", err);
            exit(1);
        }
        status = PR_Close(acceptSock);
        if (PR_FAILURE == status) {
            fprintf(stderr, "PR_Close failed\n");
            exit(1);
        }
    }
}

int main(int argc, char **argv)
{
    PRNetAddr serverAddr;
    PRFileDesc *dummySock;
    PRFileDesc *listenSock;
    PRFileDesc *clientSock;
    PRThread *dummyThread;
    PRThread **serverThreads;
    PRStatus status;
    PRUint16 port;
    int idx;
    PRInt32 nbytes;
    char buf[1024];

    serverThreads = (PRThread **)
            PR_Malloc(num_server_threads * sizeof(PRThread *));
    if (NULL == serverThreads) {
        fprintf(stderr, "PR_Malloc failed\n");
        exit(1);
    }

    /*
     * Create a dummy listening socket and have the first
     * (dummy) thread listen on it.  This is to ensure that
     * the first thread becomes the I/O continuation thread
     * in the pthreads implementation (see ptio.c) and remains
     * so throughout the test, so that we never have to
     * recycle the I/O continuation thread.
     */
    dummySock = PR_NewTCPSocket();
    if (NULL == dummySock) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    status = PR_InitializeNetAddr(PR_IpAddrAny, 0, &serverAddr);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    status = PR_Bind(dummySock, &serverAddr);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    status = PR_Listen(dummySock, 5);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }

    listenSock = PR_NewTCPSocket();
    if (NULL == listenSock) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    status = PR_InitializeNetAddr(PR_IpAddrAny, 0, &serverAddr);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    status = PR_Bind(listenSock, &serverAddr);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    status = PR_GetSockName(listenSock, &serverAddr);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_GetSockName failed\n");
        exit(1);
    }
    port = PR_ntohs(serverAddr.inet.port);
    status = PR_Listen(listenSock, 5);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }

    printf("creating dummy thread\n");
    fflush(stdout);
    dummyThread = PR_CreateThread(PR_USER_THREAD,
            ServerThreadFunc, dummySock, PR_PRIORITY_NORMAL,
            thread_scope, PR_JOINABLE_THREAD, 0);
    if (NULL == dummyThread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    printf("sleeping one second before creating server threads\n");
    fflush(stdout);
    PR_Sleep(PR_SecondsToInterval(1));
    for (idx = 0; idx < num_server_threads; idx++) {
        serverThreads[idx] = PR_CreateThread(PR_USER_THREAD,
                ServerThreadFunc, listenSock, PR_PRIORITY_NORMAL,
                thread_scope, PR_JOINABLE_THREAD, 0);
        if (NULL == serverThreads[idx]) {
            fprintf(stderr, "PR_CreateThread failed\n");
            exit(1);
        }
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    PR_InitializeNetAddr(PR_IpAddrLoopback, port, &serverAddr);
    clientSock = PR_NewTCPSocket();
    if (NULL == clientSock) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    printf("sleeping one second before connecting\n");
    fflush(stdout);
    PR_Sleep(PR_SecondsToInterval(1));
    status = PR_Connect(clientSock, &serverAddr, PR_INTERVAL_NO_TIMEOUT);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Connect failed\n");
        exit(1);
    }
    nbytes = PR_Read(clientSock, buf, sizeof(buf));
    if (nbytes != 0) {
        fprintf(stderr, "expected 0 bytes but got %d bytes\n", nbytes);
        exit(1);
    }
    status = PR_Close(clientSock);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    printf("sleeping one second before shutting down server threads\n");
    fflush(stdout);
    PR_Sleep(PR_SecondsToInterval(1));

    exit_flag = PR_TRUE;
    status = PR_Interrupt(dummyThread);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Interrupt failed\n");
        exit(1);
    }
    status = PR_JoinThread(dummyThread);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    for (idx = 0; idx < num_server_threads; idx++) {
        status = PR_Interrupt(serverThreads[idx]);
        if (PR_FAILURE == status) {
            fprintf(stderr, "PR_Interrupt failed\n");
            exit(1);
        }
        status = PR_JoinThread(serverThreads[idx]);
        if (PR_FAILURE == status) {
            fprintf(stderr, "PR_JoinThread failed\n");
            exit(1);
        }
    }
    PR_Free(serverThreads);
    status = PR_Close(dummySock);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    status = PR_Close(listenSock);
    if (PR_FAILURE == status) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }

    printf("PASS\n");
    return 0;
}
