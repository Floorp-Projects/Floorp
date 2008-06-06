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

/***********************************************************************
**  1997 - Netscape Communications Corporation
**
** Name: prselect_norm.c
**
** Description: tests PR_Select with sockets - Normal operations.
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "prinit.h"
#include "prio.h"
#include "prlog.h"
#include "prprf.h"
#include "prerror.h"
#include "prnetdb.h"

#ifdef XP_MAC
#include "probslet.h"
#else
#include "obsolete/probslet.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

PRIntn failed_already=0;
PRIntn debug_mode;

static void
clientThreadFunc(void *arg)
{
    PRUintn port = (PRUintn) arg;
    PRFileDesc *sock;
    PRNetAddr addr;
    char buf[128];
    int i;

    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons((PRUint16)port);
    addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    PR_snprintf(buf, sizeof(buf), "%hu", addr.inet.port);

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
    PRFileDesc *fds0[10], *fds1[10], **fds, **other_fds;
    PRIntn nfds;
    PRUint16 listenPort1, listenPort2;
    PRNetAddr addr;
    PR_fd_set readFdSet;
    char buf[128];
    PRThread *clientThread;
    PRInt32 retVal;
    PRIntn i, j;

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "d:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

 /* main test */
	
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    if (debug_mode) {
		printf("This program tests PR_Select with sockets.  \n");
		printf(" Normal operation are tested.\n\n");
	}

    /* Create two listening sockets */
    if ((listenSock1 = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	failed_already=1;
	goto exit_now;
    }
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock1, &addr) == PR_FAILURE) {
	fprintf(stderr, "Can't bind socket\n");
	failed_already=1;
	goto exit_now;
    }
    if (PR_GetSockName(listenSock1, &addr) == PR_FAILURE) {
	fprintf(stderr, "PR_GetSockName failed\n");
	failed_already=1;
	goto exit_now;
    }
    listenPort1 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock1, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
	failed_already=1;
	goto exit_now;
    }

    if ((listenSock2  = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	failed_already=1;
	goto exit_now;
    }
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock2, &addr) == PR_FAILURE) {
	fprintf(stderr, "Can't bind socket\n");
	failed_already=1;
	goto exit_now;
    }
    if (PR_GetSockName(listenSock2, &addr) == PR_FAILURE) {
	fprintf(stderr, "PR_GetSockName failed\n");
	failed_already=1;
	goto exit_now;
    }
    listenPort2 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock2, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
failed_already=1;
	goto exit_now;
    }
    PR_snprintf(buf, sizeof(buf),
	    "The server thread is listening on ports %hu and %hu\n\n",
	    listenPort1, listenPort2);
    if (debug_mode) printf("%s", buf);

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort1,
	    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	failed_already=1;
	goto exit_now;
    }

    clientThread = PR_CreateThread(PR_USER_THREAD,
	    clientThreadFunc, (void *) listenPort2,
	    PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
	    PR_UNJOINABLE_THREAD, 0);
    if (clientThread == NULL) {
	fprintf(stderr, "can't create thread\n");
	failed_already=1;
	goto exit_now;
    }

    if (debug_mode) {
		printf("Two client threads are created.  Each of them will\n");
		printf("send data to one of the two ports the server is listening on.\n");
		printf("The data they send is the port number.  Each of them send\n");
		printf("the data five times, so you should see ten lines below,\n");
		printf("interleaved in an arbitrary order.\n");
	}
    /* set up the fd array */
    fds = fds0;
    other_fds = fds1;
    fds[0] = listenSock1;
    fds[1] = listenSock2;
    nfds = 2;
    /* Set up the fd set */
    PR_FD_ZERO(&readFdSet);
    PR_FD_SET(listenSock1, &readFdSet);
    PR_FD_SET(listenSock2, &readFdSet);

    /* 20 events total */
    i = 0;
    while (i < 20) {
	PRFileDesc **tmp;
	int nextIndex;
	int nEvents = 0;

	retVal = PR_Select(0 /* unused */, &readFdSet, NULL, NULL,
		PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(retVal != 0);  /* no timeout */
	if (retVal == -1) {
	    fprintf(stderr, "PR_Select failed (%d, %d)\n", PR_GetError(),
		    PR_GetOSError());
	failed_already=1;
	    goto exit_now;
	}

	nextIndex = 2;
	/* the two listening sockets */
	for (j = 0; j < 2; j++) {
	    other_fds[j] = fds[j];
	    if (PR_FD_ISSET(fds[j], &readFdSet)) {
		PRFileDesc *sock;

		nEvents++;
		sock = PR_Accept(fds[j], NULL, PR_INTERVAL_NO_TIMEOUT);
		if (sock == NULL) {
		    fprintf(stderr, "PR_Accept() failed\n");
		failed_already=1;
		    goto exit_now;
		}
		other_fds[nextIndex] = sock;
		PR_FD_SET(sock, &readFdSet);
		nextIndex++;
	    }
	    PR_FD_SET(fds[j], &readFdSet);
	}

	for (j = 2; j < nfds; j++) {
	    if (PR_FD_ISSET(fds[j], &readFdSet)) {
		PRInt32 nBytes;

		PR_FD_CLR(fds[j], &readFdSet);
		nEvents++;
		nBytes = PR_Read(fds[j], buf, sizeof(buf));
		if (nBytes == -1) {
		    fprintf(stderr, "PR_Read() failed\n");
		failed_already=1;
		    goto exit_now;
		}
		/* Just to be safe */
		buf[127] = '\0';
		PR_Close(fds[j]);
		if (debug_mode) printf("The server received \"%s\" from a client\n", buf);
	    } else {
		PR_FD_SET(fds[j], &readFdSet);
		other_fds[nextIndex] = fds[j];
		nextIndex++;
	    }
	}

	PR_ASSERT(retVal == nEvents);
	/* swap */
	tmp = fds;
	fds = other_fds;
	other_fds = tmp;
	nfds = nextIndex;
	i += nEvents;
    }

    if (debug_mode) printf("Test passed\n");

    PR_Cleanup();
	goto exit_now;
exit_now:
	if(failed_already)	
		return 1;
	else
		return 0;
}
