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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

#define NUM_SERVER_THREADS 5

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
