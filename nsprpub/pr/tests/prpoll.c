/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include "prinit.h"
#include "prio.h"
#include "prlog.h"
#include "prprf.h"
#include "prnetdb.h"

#ifndef XP_MAC
#include "private/pprio.h"
#else
#include "pprio.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void
clientThreadFunc(void *arg)
{
    PRUint16 port = (PRUint16) arg;
    PRFileDesc *sock;
    PRNetAddr addr;
    char buf[128];
    int i;

    addr.inet.family = AF_INET;
    addr.inet.port = PR_htons(port);
    addr.inet.ip = PR_htonl(INADDR_LOOPBACK);
    PR_snprintf(buf, sizeof(buf), "%hu", port);

    for (i = 0; i < 5; i++) {
	sock = PR_NewTCPSocket();
        PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT);

	PR_Write(sock, buf, sizeof(buf));
	PR_Close(sock);
    }
}

int main(int argc, char **argv)
{
    PRFileDesc *listenSock1, *listenSock2;
    PRFileDesc *badFD;
    PRUint16 listenPort1, listenPort2;
    PRNetAddr addr;
    char buf[128];
    PRThread *clientThread;
    PRPollDesc pds0[10], pds1[10], *pds, *other_pds;
    PRIntn npds;
    PRInt32 retVal;
    PRIntn i, j;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    printf("This program tests PR_Poll with sockets.\n");
    printf("Timeout, error reporting, and normal operation are tested.\n\n");

    /* Create two listening sockets */
    if ((listenSock1 = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	exit(1);
    }
    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock1, &addr) == PR_FAILURE) {
	fprintf(stderr, "Can't bind socket\n");
	exit(1);
    }
    if (PR_GetSockName(listenSock1, &addr) == PR_FAILURE) {
	fprintf(stderr, "PR_GetSockName failed\n");
	exit(1);
    }
    listenPort1 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock1, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
	exit(1);
    }

    if ((listenSock2  = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	exit(1);
    }
    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock2, &addr) == PR_FAILURE) {
	fprintf(stderr, "Can't bind socket\n");
	exit(1);
    }
    if (PR_GetSockName(listenSock2, &addr) == PR_FAILURE) {
	fprintf(stderr, "PR_GetSockName failed\n");
	exit(1);
    }
    listenPort2 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock2, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
	exit(1);
    }
    PR_snprintf(buf, sizeof(buf),
	    "The server thread is listening on ports %hu and %hu\n\n",
	    listenPort1, listenPort2);
    printf("%s", buf);

    /* Set up the poll descriptor array */
    pds = pds0;
    other_pds = pds1;
    memset(pds, 0, sizeof(pds));
    pds[0].fd = listenSock1;
    pds[0].in_flags = PR_POLL_READ;
    pds[1].fd = listenSock2;
    pds[1].in_flags = PR_POLL_READ;
    npds = 2;

    /* Testing timeout */
    printf("PR_Poll should time out in 5 seconds\n");
    retVal = PR_Poll(pds, npds, PR_SecondsToInterval(5));
    if (retVal != 0) {
	PR_snprintf(buf, sizeof(buf),
		"PR_Poll should time out and return 0, but it returns %ld\n",
		retVal);
	fprintf(stderr, "%s", buf);
	exit(1);
    }
    printf("PR_Poll timed out.  Test passed.\n\n");

    /* Testing bad fd */
    printf("PR_Poll should detect a bad file descriptor\n");
    if ((badFD = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a TCP socket\n");
	exit(1);
    }

    pds[2].fd = badFD;
    pds[2].in_flags = PR_POLL_READ;
    npds = 3;
    PR_Close(badFD);  /* make the fd bad */
    retVal = PR_Poll(pds, npds, PR_INTERVAL_NO_TIMEOUT);
    if (retVal != 1 || (unsigned short) pds[2].out_flags != PR_POLL_NVAL) {
	fprintf(stderr, "Failed to detect the bad fd: "
		"PR_Poll returns %d, out_flags is 0x%hx\n",
		retVal, pds[2].out_flags);
	exit(1);
    }
    printf("PR_Poll detected the bad fd.  Test passed.\n\n");
    npds = 2;

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort1,
	    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	exit(1);
    }

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort2,
	    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	exit(1);
    }

    printf("Two client threads are created.  Each of them will\n");
    printf("send data to one of the two ports the server is listening on.\n");
    printf("The data they send is the port number.  Each of them send\n");
    printf("the data five times, so you should see ten lines below,\n");
    printf("interleaved in an arbitrary order.\n");

    /* 20 events total */
    i = 0;
    while (i < 20) {
	PRPollDesc *tmp;
	int nextIndex;
	int nEvents = 0;

	retVal = PR_Poll(pds, npds, PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(retVal != 0);  /* no timeout */
	if (retVal == -1) {
	    fprintf(stderr, "PR_Poll failed\n");
	    exit(1);
	}

	nextIndex = 2;
	/* the two listening sockets */
	for (j = 0; j < 2; j++) {
	    other_pds[j] = pds[j];
	    PR_ASSERT((pds[j].out_flags & PR_POLL_WRITE) == 0
		    && (pds[j].out_flags & PR_POLL_EXCEPT) == 0);
	    if (pds[j].out_flags & PR_POLL_READ) {
		PRFileDesc *sock;

		nEvents++;
		sock = PR_Accept(pds[j].fd, NULL, PR_INTERVAL_NO_TIMEOUT);
		if (sock == NULL) {
		    fprintf(stderr, "PR_Accept() failed\n");
		    exit(1);
		}
		other_pds[nextIndex].fd = sock;
		other_pds[nextIndex].in_flags = PR_POLL_READ;
		nextIndex++;
	    } else if (pds[j].out_flags & PR_POLL_ERR) {
		fprintf(stderr, "PR_Poll() indicates that an fd has error\n");
		exit(1);
	    } else if (pds[j].out_flags & PR_POLL_NVAL) {
		fprintf(stderr, "PR_Poll() indicates that fd %d is invalid\n",
			PR_FileDesc2NativeHandle(pds[j].fd));
		exit(1);
	    }
	}

	for (j = 2; j < npds; j++) {
	    PR_ASSERT((pds[j].out_flags & PR_POLL_WRITE) == 0
		    && (pds[j].out_flags & PR_POLL_EXCEPT) == 0);
	    if (pds[j].out_flags & PR_POLL_READ) {
		PRInt32 nBytes;

		nEvents++;
		nBytes = PR_Read(pds[j].fd, buf, sizeof(buf));
		if (nBytes == -1) {
		    fprintf(stderr, "PR_Read() failed\n");
		    exit(1);
		}
		/* Just to be safe */
		buf[127] = '\0';
		PR_Close(pds[j].fd);
		printf("The server received \"%s\" from a client\n", buf);
	    } else if (pds[j].out_flags & PR_POLL_ERR) {
		fprintf(stderr, "PR_Poll() indicates that an fd has error\n");
		exit(1);
	    } else if (pds[j].out_flags & PR_POLL_NVAL) {
		fprintf(stderr, "PR_Poll() indicates that an fd is invalid\n");
		exit(1);
	    } else {
		other_pds[nextIndex] = pds[j];
		nextIndex++;
	    }
	}

	PR_ASSERT(retVal == nEvents);
	/* swap */
	tmp = pds;
	pds = other_pds;
	other_pds = tmp;
	npds = nextIndex;
	i += nEvents;
    }

    printf("All tests finished\n");
    PR_Cleanup();
    return 0;
}
