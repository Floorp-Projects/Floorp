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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#ifdef WIN32
#include <windows.h>
#endif

#ifdef XP_OS2_VACPP
#include <io.h>      /* for close() */
#endif

#ifdef XP_UNIX
#include <unistd.h>  /* for close() */
#endif

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

#define CLIENT_LOOPS	5
#define BUF_SIZE		128

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void
clientThreadFunc(void *arg)
{
    PRUint16 port = (PRUint16) arg;
    PRFileDesc *sock;
    PRNetAddr addr;
    char buf[BUF_SIZE];
    int i;

    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons(port);
    addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
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
    char buf[BUF_SIZE];
    PRThread *clientThread;
    PRPollDesc pds0[10], pds1[10], *pds, *other_pds;
    PRIntn npds;
    PRInt32 retVal;
    PRInt32 rv;
    PROsfd sd;
    struct sockaddr_in saddr;
    PRIntn saddr_len;
    PRUint16 listenPort3;
    PRFileDesc *socket_poll_fd;
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
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
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
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
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
    /* Set up the poll descriptor array */
    pds = pds0;
    other_pds = pds1;
    memset(pds, 0, sizeof(pds));
	npds = 0;
    pds[npds].fd = listenSock1;
    pds[npds].in_flags = PR_POLL_READ;
	npds++;
    pds[npds].fd = listenSock2;
    pds[npds].in_flags = PR_POLL_READ;
	npds++;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	PR_ASSERT(sd >= 0);
	memset((char *) &saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(0);

	rv = bind(sd, (struct sockaddr *)&saddr, sizeof(saddr));
	PR_ASSERT(rv == 0);
	saddr_len = sizeof(saddr);
	rv = getsockname(sd, (struct sockaddr *) &saddr, &saddr_len);
	PR_ASSERT(rv == 0);
    listenPort3 = ntohs(saddr.sin_port);

	rv = listen(sd, 5);
	PR_ASSERT(rv == 0);
    pds[npds].fd = socket_poll_fd = PR_CreateSocketPollFd(sd);
	PR_ASSERT(pds[npds].fd);
    pds[npds].in_flags = PR_POLL_READ;
    npds++;
    PR_snprintf(buf, sizeof(buf),
	    "The server thread is listening on ports %hu, %hu and %hu\n\n",
	    listenPort1, listenPort2, listenPort3);
    printf("%s", buf);

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

    pds[npds].fd = badFD;
    pds[npds].in_flags = PR_POLL_READ;
    npds++;
    PR_Close(badFD);  /* make the fd bad */
#if 0
    retVal = PR_Poll(pds, npds, PR_INTERVAL_NO_TIMEOUT);
    if (retVal != 1 || (unsigned short) pds[2].out_flags != PR_POLL_NVAL) {
	fprintf(stderr, "Failed to detect the bad fd: "
		"PR_Poll returns %d, out_flags is 0x%hx\n",
		retVal, pds[npds - 1].out_flags);
	exit(1);
    }
    printf("PR_Poll detected the bad fd.  Test passed.\n\n");
#endif
    npds--;

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
	    PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	exit(1);
    }

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort3,
	    PR_PRIORITY_NORMAL, PR_GLOBAL_BOUND_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	exit(1);
    }


    printf("Three client threads are created.  Each of them will\n");
    printf("send data to one of the three ports the server is listening on.\n");
    printf("The data they send is the port number.  Each of them send\n");
    printf("the data five times, so you should see ten lines below,\n");
    printf("interleaved in an arbitrary order.\n");

    /* 30 events total */
    i = 0;
    while (i < 30) {
		PRPollDesc *tmp;
		int nextIndex;
		int nEvents = 0;

		retVal = PR_Poll(pds, npds, PR_INTERVAL_NO_TIMEOUT);
		PR_ASSERT(retVal != 0);  /* no timeout */
		if (retVal == -1) {
			fprintf(stderr, "PR_Poll failed\n");
			exit(1);
		}

		nextIndex = 3;
		/* the three listening sockets */
		for (j = 0; j < 3; j++) {
			other_pds[j] = pds[j];
			PR_ASSERT((pds[j].out_flags & PR_POLL_WRITE) == 0
				&& (pds[j].out_flags & PR_POLL_EXCEPT) == 0);
			if (pds[j].out_flags & PR_POLL_READ) {
				PRFileDesc *sock;

				nEvents++;
				if (j == 2) {
					PROsfd newsd;
					newsd = accept(PR_FileDesc2NativeHandle(pds[j].fd), NULL, 0);
					if (newsd == -1) {
						fprintf(stderr, "accept() failed\n");
						exit(1);
					}
					other_pds[nextIndex].fd  = PR_CreateSocketPollFd(newsd);
					PR_ASSERT(other_pds[nextIndex].fd);
					other_pds[nextIndex].in_flags = PR_POLL_READ;
				} else {
					sock = PR_Accept(pds[j].fd, NULL, PR_INTERVAL_NO_TIMEOUT);
					if (sock == NULL) {
						fprintf(stderr, "PR_Accept() failed\n");
						exit(1);
					}
					other_pds[nextIndex].fd = sock;
					other_pds[nextIndex].in_flags = PR_POLL_READ;
				}
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

		for (j = 3; j < npds; j++) {
			PR_ASSERT((pds[j].out_flags & PR_POLL_WRITE) == 0
				&& (pds[j].out_flags & PR_POLL_EXCEPT) == 0);
			if (pds[j].out_flags & PR_POLL_READ) {
				PRInt32 nBytes;

				nEvents++;
				/* XXX: This call is a hack and should be fixed */
				if (PR_GetDescType(pds[j].fd) == (PRDescType) 0) {
					nBytes = recv(PR_FileDesc2NativeHandle(pds[j].fd), buf,
										sizeof(buf), 0);
					if (nBytes == -1) {
						fprintf(stderr, "recv() failed\n");
						exit(1);
					}
					printf("Server read %d bytes from native fd %d\n",nBytes,
										PR_FileDesc2NativeHandle(pds[j].fd));
#ifdef WIN32
					closesocket((SOCKET)PR_FileDesc2NativeHandle(pds[j].fd));
#else
					close(PR_FileDesc2NativeHandle(pds[j].fd));
#endif
					PR_DestroySocketPollFd(pds[j].fd);
				} else {
					nBytes = PR_Read(pds[j].fd, buf, sizeof(buf));
					if (nBytes == -1) {
						fprintf(stderr, "PR_Read() failed\n");
						exit(1);
					}
					PR_Close(pds[j].fd);
				}
				/* Just to be safe */
				buf[BUF_SIZE - 1] = '\0';
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
    PR_DestroySocketPollFd(socket_poll_fd);

    printf("All tests finished\n");
    PR_Cleanup();
    return 0;
}
