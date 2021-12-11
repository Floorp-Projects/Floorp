/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test case for the PR_MSG_PEEK flag of PR_Recv().
 *
 * Test both blocking and non-blocking sockets.
 */

#include "nspr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

static int iterations = 10;

/*
 * In iteration i, recv_amount[i] is the number of bytes we
 * wish to receive, and send_amount[i] is the number of bytes
 * we actually send.  Therefore, the number of elements in the
 * recv_amount or send_amount array should equal to 'iterations'.
 * For this test to pass we need to ensure that
 *     recv_amount[i] <= BUFFER_SIZE,
 *     send_amount[i] <= BUFFER_SIZE,
 *     send_amount[i] <= recv_amount[i].
 */
static PRInt32 recv_amount[10] = {
    16, 128, 256, 1024, 512, 512, 128, 256, 32, 32
};
static PRInt32 send_amount[10] = {
    16,  64, 128, 1024, 512, 256, 128,  64, 16, 32
};

/* Blocking I/O */
static void ServerB(void *arg)
{
    PRFileDesc *listenSock = (PRFileDesc *) arg;
    PRFileDesc *sock;
    char buf[BUFFER_SIZE];
    PRInt32 nbytes;
    int i;
    int j;

    sock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == sock) {
        fprintf(stderr, "PR_Accept failed\n");
        exit(1);
    }

    for (i = 0; i < iterations; i++) {
        memset(buf, 0, sizeof(buf));
        nbytes = PR_Recv(sock, buf, recv_amount[i],
                         PR_MSG_PEEK, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Recv failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "server: peeked expected data\n");

        memset(buf, 0, sizeof(buf));
        nbytes = PR_Recv(sock, buf, recv_amount[i],
                         PR_MSG_PEEK, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Recv failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "server: peeked expected data\n");

        memset(buf, 0, sizeof(buf));
        nbytes = PR_Recv(sock, buf, recv_amount[i],
                         0, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Recv failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "server: received expected data\n");

        PR_Sleep(PR_SecondsToInterval(1));
        memset(buf, 2*i+1, send_amount[i]);
        nbytes = PR_Send(sock, buf, send_amount[i],
                         0, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Send failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Send returned %d, absurd!\n", nbytes);
            exit(1);
        }
    }
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
}

/* Non-blocking I/O */
static void ClientNB(void *arg)
{
    PRFileDesc *sock;
    PRSocketOptionData opt;
    PRUint16 port = (PRUint16) arg;
    PRNetAddr addr;
    char buf[BUFFER_SIZE];
    PRPollDesc pd;
    PRInt32 npds;
    PRInt32 nbytes;
    int i;
    int j;

    sock = PR_OpenTCPSocket(PR_AF_INET6);
    if (NULL == sock) {
        fprintf(stderr, "PR_OpenTCPSocket failed\n");
        exit(1);
    }
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    if (PR_SetSocketOption(sock, &opt) == PR_FAILURE) {
        fprintf(stderr, "PR_SetSocketOption failed\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    if (PR_SetNetAddr(PR_IpAddrLoopback, PR_AF_INET6, port, &addr)
        == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
        if (PR_GetError() != PR_IN_PROGRESS_ERROR) {
            fprintf(stderr, "PR_Connect failed\n");
            exit(1);
        }
        pd.fd = sock;
        pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
        npds = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == npds) {
            fprintf(stderr, "PR_Poll failed\n");
            exit(1);
        }
        if (1 != npds) {
            fprintf(stderr, "PR_Poll returned %d, absurd!\n", npds);
            exit(1);
        }
        if (PR_GetConnectStatus(&pd) == PR_FAILURE) {
            fprintf(stderr, "PR_GetConnectStatus failed\n");
            exit(1);
        }
    }

    for (i = 0; i < iterations; i++) {
        PR_Sleep(PR_SecondsToInterval(1));
        memset(buf, 2*i, send_amount[i]);
        while ((nbytes = PR_Send(sock, buf, send_amount[i],
                                 0, PR_INTERVAL_NO_TIMEOUT)) == -1) {
            if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
                fprintf(stderr, "PR_Send failed\n");
                exit(1);
            }
            pd.fd = sock;
            pd.in_flags = PR_POLL_WRITE;
            npds = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
            if (-1 == npds) {
                fprintf(stderr, "PR_Poll failed\n");
                exit(1);
            }
            if (1 != npds) {
                fprintf(stderr, "PR_Poll returned %d, absurd!\n", npds);
                exit(1);
            }
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Send returned %d, absurd!\n", nbytes);
            exit(1);
        }

        memset(buf, 0, sizeof(buf));
        while ((nbytes = PR_Recv(sock, buf, recv_amount[i],
                                 PR_MSG_PEEK, PR_INTERVAL_NO_TIMEOUT)) == -1) {
            if (PR_GetError() != PR_WOULD_BLOCK_ERROR) {
                fprintf(stderr, "PR_Recv failed\n");
                exit(1);
            }
            pd.fd = sock;
            pd.in_flags = PR_POLL_READ;
            npds = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
            if (-1 == npds) {
                fprintf(stderr, "PR_Poll failed\n");
                exit(1);
            }
            if (1 != npds) {
                fprintf(stderr, "PR_Poll returned %d, absurd!\n", npds);
                exit(1);
            }
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i+1) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i+1, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "client: peeked expected data\n");

        memset(buf, 0, sizeof(buf));
        nbytes = PR_Recv(sock, buf, recv_amount[i],
                         PR_MSG_PEEK, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Recv failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i+1) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i+1, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "client: peeked expected data\n");

        memset(buf, 0, sizeof(buf));
        nbytes = PR_Recv(sock, buf, recv_amount[i],
                         0, PR_INTERVAL_NO_TIMEOUT);
        if (-1 == nbytes) {
            fprintf(stderr, "PR_Recv failed\n");
            exit(1);
        }
        if (send_amount[i] != nbytes) {
            fprintf(stderr, "PR_Recv returned %d, absurd!\n", nbytes);
            exit(1);
        }
        for (j = 0; j < nbytes; j++) {
            if (buf[j] != 2*i+1) {
                fprintf(stderr, "byte %d should be %d but is %d\n",
                        j, 2*i+1, buf[j]);
                exit(1);
            }
        }
        fprintf(stderr, "client: received expected data\n");
    }
    if (PR_Close(sock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
}

static void
RunTest(PRThreadScope scope, PRFileDesc *listenSock, PRUint16 port)
{
    PRThread *server, *client;

    server = PR_CreateThread(PR_USER_THREAD, ServerB, listenSock,
                             PR_PRIORITY_NORMAL, scope, PR_JOINABLE_THREAD, 0);
    if (NULL == server) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }
    client = PR_CreateThread(
                 PR_USER_THREAD, ClientNB, (void *) port,
                 PR_PRIORITY_NORMAL, scope, PR_JOINABLE_THREAD, 0);
    if (NULL == client) {
        fprintf(stderr, "PR_CreateThread failed\n");
        exit(1);
    }

    if (PR_JoinThread(server) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
    if (PR_JoinThread(client) == PR_FAILURE) {
        fprintf(stderr, "PR_JoinThread failed\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    PRFileDesc *listenSock;
    PRNetAddr addr;
    PRUint16 port;

    listenSock = PR_OpenTCPSocket(PR_AF_INET6);
    if (NULL == listenSock) {
        fprintf(stderr, "PR_OpenTCPSocket failed\n");
        exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    if (PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_SetNetAddr failed\n");
        exit(1);
    }
    if (PR_Bind(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_Bind failed\n");
        exit(1);
    }
    if (PR_GetSockName(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_GetSockName failed\n");
        exit(1);
    }
    port = PR_ntohs(addr.ipv6.port);
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed\n");
        exit(1);
    }

    fprintf(stderr, "Running the test with local threads\n");
    RunTest(PR_LOCAL_THREAD, listenSock, port);
    fprintf(stderr, "Running the test with global threads\n");
    RunTest(PR_GLOBAL_THREAD, listenSock, port);

    if (PR_Close(listenSock) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed\n");
        exit(1);
    }
    printf("PASS\n");
    return 0;
}
