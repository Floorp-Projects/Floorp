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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nspr.h"
#include "prio.h"
#include "prerror.h"
#include "prlog.h"
#include "prprf.h"
#include "prnetdb.h"
#include "plerror.h"
#ifndef XP_MAC
#include "obsolete/probslet.h"
#else
#include "probslet.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUMBER_ROUNDS 5

#if defined(WIN16)
/*
** Make win16 unit_time interval 300 milliseconds, others get 100
*/
#define UNIT_TIME  200       /* unit time in milliseconds */
#else
#define UNIT_TIME  100       /* unit time in milliseconds */
#endif
#define CHUNK_SIZE 10
#undef USE_PR_SELECT         /* If defined, we use PR_Select.
                              * If not defined, use PR_Poll instead. */

#if defined(USE_PR_SELECT)
#include "pprio.h"
#endif

#ifdef XP_MAC
int fprintf(FILE *stream, const char *fmt, ...)
{
PR_LogPrint(fmt);
return 0;
}
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

static void PR_CALLBACK
clientThreadFunc(void *arg)
{
    PRUintn port = (PRUintn)arg;
    PRFileDesc *sock;
    PRNetAddr addr;
    char buf[CHUNK_SIZE];
    int i;
    PRIntervalTime unitTime = PR_MillisecondsToInterval(UNIT_TIME);
    PRIntn optval = 1;
    PRStatus retVal;
    PRInt32 nBytes;

    /* Initialize the buffer so that Purify won't complain */
    memset(buf, 0, sizeof(buf));

    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons((PRUint16)port);
    addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    PR_snprintf(buf, sizeof(buf), "%hu", addr.inet.ip);

    /* time 1 */
    PR_Sleep(unitTime);
    sock = PR_NewTCPSocket();
    PR_SetSockOpt(sock, PR_SockOpt_Nonblocking, &optval, sizeof(optval));
    retVal = PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (retVal == PR_FAILURE && PR_GetError() == PR_IN_PROGRESS_ERROR) {
#if !defined(USE_PR_SELECT)
	PRPollDesc pd;
	PRInt32 n;
	fprintf(stderr, "connect: EWOULDBLOCK, good\n");
	pd.fd = sock;
	pd.in_flags = PR_POLL_WRITE;
	n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(n == 1);
        PR_ASSERT(pd.out_flags == PR_POLL_WRITE);
#else
        PR_fd_set writeSet;
        PRInt32 n;
        fprintf(stderr, "connect: EWOULDBLOCK, good\n");
        PR_FD_ZERO(&writeSet);
        PR_FD_SET(sock, &writeSet);
        n = PR_Select(0, NULL, &writeSet, NULL, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(n == 1);
        PR_ASSERT(PR_FD_ISSET(sock, &writeSet));
#endif
    }
    printf("client connected\n");
    fflush(stdout);

    /* time 4, 7, 11, etc. */
    for (i = 0; i < NUMBER_ROUNDS; i++) {
        PR_Sleep(3 * unitTime);
    	nBytes = PR_Write(sock, buf, sizeof(buf));
	    if (nBytes == -1) {
	    if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
		fprintf(stderr, "write: EWOULDBLOCK\n");
		exit(1);
            } else {
		fprintf(stderr, "write: failed\n");
            }
	}
	printf("client sent %d bytes\n", nBytes);
	fflush(stdout);
    }

    PR_Close(sock);
}

static PRIntn PR_CALLBACK RealMain( PRIntn argc, char **argv )
{
    PRFileDesc *listenSock, *sock;
    PRUint16 listenPort;
    PRNetAddr addr;
    char buf[CHUNK_SIZE];
    PRThread *clientThread;
    PRInt32 retVal;
    PRIntn optval = 1;
    PRIntn i;
    PRIntervalTime unitTime = PR_MillisecondsToInterval(UNIT_TIME);

#ifdef XP_MAC
	SetupMacPrintfLog("nonblock.log");
#endif

    /* Create a listening socket */
    if ((listenSock = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	exit(1);
    }
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock, &addr) == PR_FAILURE) {
	fprintf(stderr, "Can't bind socket\n");
	exit(1);
    }
    if (PR_GetSockName(listenSock, &addr) == PR_FAILURE) {
	fprintf(stderr, "PR_GetSockName failed\n");
	exit(1);
    }
    listenPort = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
	exit(1);
    }

    PR_snprintf(buf, sizeof(buf),
	    "The server thread is listening on port %hu\n\n",
	    listenPort);
    printf("%s", buf);

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort,
	    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	exit(1);
    }

    printf("client thread created.\n");

    PR_SetSockOpt(listenSock, PR_SockOpt_Nonblocking, &optval,
	sizeof(PRIntn));
    /* time 0 */
    sock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (sock != NULL || PR_GetError() != PR_WOULD_BLOCK_ERROR) {
        PL_PrintError("First Accept\n");
        fprintf(stderr, "First PR_Accept() xxx\n" );
		    exit(1);
    }
    printf("accept: EWOULDBLOCK, good\n");
    fflush(stdout);
    /* time 2 */
    PR_Sleep(2 * unitTime);
    sock = PR_Accept(listenSock, NULL, PR_INTERVAL_NO_TIMEOUT);
    if (sock == NULL) {
        PL_PrintError("Second Accept\n");
        fprintf(stderr, "Second PR_Accept() failed: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
		    exit(1);
    }
    printf("accept: succeeded, good\n");
    fflush(stdout);
    PR_Close(listenSock);

    PR_SetSockOpt(sock, PR_SockOpt_Nonblocking, &optval, sizeof(PRIntn));

    /* time 3, 5, 6, 8, etc. */
    for (i = 0; i < NUMBER_ROUNDS; i++) {
	PR_Sleep(unitTime);
	retVal = PR_Recv(sock, buf, sizeof(buf), 0, PR_INTERVAL_NO_TIMEOUT);
	if (retVal != -1 || PR_GetError() != PR_WOULD_BLOCK_ERROR) {
        PL_PrintError("First Receive:\n");
	    fprintf(stderr, "First PR_Recv: retVal: %ld, Error: %ld\n",
            retVal, PR_GetError());
	    exit(1);
        }
	printf("read: EWOULDBLOCK, good\n");
	fflush(stdout);
	PR_Sleep(2 * unitTime);
	retVal = PR_Recv(sock, buf, sizeof(buf), 0, PR_INTERVAL_NO_TIMEOUT);
	if (retVal != CHUNK_SIZE) {
        PL_PrintError("Second Receive:\n");
	    fprintf(stderr, "Second PR_Recv: retVal: %ld, Error: %ld\n", 
            retVal, PR_GetError());
	    exit(1);
        }
	printf("read: %d bytes, good\n", retVal);
	fflush(stdout);
    }
    PR_Close(sock);

    printf("All tests finished\n");
    printf("PASS\n");
    return 0;
}

PRIntn main(PRIntn argc, char *argv[])
{
    PRIntn rv;
    
    PR_STDIO_INIT();
    rv = PR_Initialize(RealMain, argc, argv, 0);
    return rv;
}  /* main */

