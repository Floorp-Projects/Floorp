/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test: zerolen.c
 *
 * Description: a test for Bugzilla bug #17699.  We perform
 * the same test for PR_Writev, PR_Write, and PR_Send.  In
 * each test the server thread first fills up the connection
 * to the client so that the next write operation will fail
 * with EAGAIN.  Then it calls PR_Writev, PR_Write, or PR_Send
 * with a zero-length buffer.  The client thread initially
 * does not read so that the connection can be filled up.
 * Then it empties the connection so that the server thread's
 * PR_Writev, PR_Write, or PR_Send call can succeed.
 *
 * Bug #17699 is specific to the pthreads version on Unix,
 * so on other platforms this test does nothing.
 */

#ifndef XP_UNIX

#include <stdio.h>

int main(int argc, char **argv)
{
    printf("PASS\n");
    return 0;
}

#else /* XP_UNIX */

#include "nspr.h"
#include "private/pprio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void ClientThread(void *arg)
{
    PRFileDesc *sock;
    PRNetAddr addr;
    PRUint16 port = (PRUint16) arg;
    char buf[1024];
    PRInt32 nbytes;

    sock = PR_NewTCPSocket();
    if (NULL == sock) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    if (PR_InitializeNetAddr(PR_IpAddrLoopback, port, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    if (PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
        fprintf(stderr, "PR_Connect failed\n");
        exit(1);
    }
    /*
     * Sleep 5 seconds to force the server thread to get EAGAIN.
     */
    if (PR_Sleep(PR_SecondsToInterval(5)) == PR_FAILURE) {
        fprintf(stderr, "PR_Sleep failed\n");
        exit(1);
    }
    /*
     * Then start reading.
     */
    while (nbytes = PR_Read(sock, buf, sizeof(buf)) > 0) {
        /* empty loop body */
    }
    if (-1 == nbytes) {
        fprintf(stderr, "PR_Read failed\n");
        exit(1);
    }
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
}

int main()
{
    PRFileDesc *listenSock;
    PRFileDesc *acceptSock;
    int osfd;
    PRThread *clientThread;
    PRNetAddr addr;
    char buf[1024];
    PRInt32 nbytes;
    PRIOVec iov;

    memset(buf, 0, sizeof(buf)); /* Initialize the buffer. */
    listenSock = PR_NewTCPSocket();
    if (NULL == listenSock) {
        fprintf(stderr, "PR_NewTCPSocket failed\n");
        exit(1);
    }
    if (PR_InitializeNetAddr(PR_IpAddrAny, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_InitializeNetAddr failed\n");
        exit(1);
    }
    if (PR_Bind(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    /* Find out what port number we are bound to. */
    if (PR_GetSockName(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_GetSockName failed\n");
        exit(1);
    }
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }

    /*
     * First test PR_Writev.
     */
    clientThread = PR_CreateThread(PR_USER_THREAD,
                                   ClientThread, (void *) PR_ntohs(PR_NetAddrInetPort(&addr)),
                                   PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == clientThread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    acceptSock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == acceptSock) {
        fprintf(stderr, "PR_Accept failed\n");
        exit(1);
    }
    osfd = PR_FileDesc2NativeHandle(acceptSock);
    while (write(osfd, buf, sizeof(buf)) != -1) {
        /* empty loop body */
    }
    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
        fprintf(stderr, "write failed\n");
        exit(1);
    }
    iov.iov_base = buf;
    iov.iov_len = 0;
    printf("calling PR_Writev with a zero-length buffer\n");
    fflush(stdout);
    nbytes = PR_Writev(acceptSock, &iov, 1, PR_INTERVAL_NO_TIMEOUT);
    if (nbytes != 0) {
        fprintf(stderr, "PR_Writev should return 0 but returns %d\n", nbytes);
        exit(1);
    }
    if (PR_Close(acceptSock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (PR_JoinThread(clientThread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }

    /*
     * Then test PR_Write.
     */
    clientThread = PR_CreateThread(PR_USER_THREAD,
                                   ClientThread, (void *) PR_ntohs(PR_NetAddrInetPort(&addr)),
                                   PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == clientThread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    acceptSock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == acceptSock) {
        fprintf(stderr, "PR_Accept failed\n");
        exit(1);
    }
    osfd = PR_FileDesc2NativeHandle(acceptSock);
    while (write(osfd, buf, sizeof(buf)) != -1) {
        /* empty loop body */
    }
    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
        fprintf(stderr, "write failed\n");
        exit(1);
    }
    printf("calling PR_Write with a zero-length buffer\n");
    fflush(stdout);
    nbytes = PR_Write(acceptSock, buf, 0);
    if (nbytes != 0) {
        fprintf(stderr, "PR_Write should return 0 but returns %d\n", nbytes);
        exit(1);
    }
    if (PR_Close(acceptSock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (PR_JoinThread(clientThread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }

    /*
     * Finally test PR_Send.
     */
    clientThread = PR_CreateThread(PR_USER_THREAD,
                                   ClientThread, (void *) PR_ntohs(PR_NetAddrInetPort(&addr)),
                                   PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (NULL == clientThread) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    acceptSock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == acceptSock) {
        fprintf(stderr, "PR_Accept failed\n");
        exit(1);
    }
    osfd = PR_FileDesc2NativeHandle(acceptSock);
    while (write(osfd, buf, sizeof(buf)) != -1) {
        /* empty loop body */
    }
    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
        fprintf(stderr, "write failed\n");
        exit(1);
    }
    printf("calling PR_Send with a zero-length buffer\n");
    fflush(stdout);
    nbytes = PR_Send(acceptSock, buf, 0, 0, PR_INTERVAL_NO_TIMEOUT);
    if (nbytes != 0) {
        fprintf(stderr, "PR_Send should return 0 but returns %d\n", nbytes);
        exit(1);
    }
    if (PR_Close(acceptSock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    if (PR_JoinThread(clientThread) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }

    if (PR_Close(listenSock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    printf("PASS\n");
    return 0;
}

#endif /* XP_UNIX */
