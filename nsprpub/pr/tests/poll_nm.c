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
**
** Name: prpoll_norm.c
**
** Description: This program tests PR_Poll with sockets.
**              Normal operation are tested
**
** Modification History:
** 19-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
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
#include "prnetdb.h"
#ifndef XP_MAC
#include "obsolete/probslet.h"
#else
#include "probslet.h"
#endif

#ifndef XP_MAC
#include "private/pprio.h"
#else
#include "pprio.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
PRIntn failed_already=0;
PRIntn debug_mode;

#define NUM_ITERATIONS 5

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
    PRUintn port = (PRUintn) arg;
    PRFileDesc *sock;
    PRNetAddr addr;
    char buf[128];
    int i;
    PRStatus sts;
    PRInt32 n;

    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons((PRUint16)port);
    addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    memset(buf, 0, sizeof(buf));
    PR_snprintf(buf, sizeof(buf), "%hu", port);

    for (i = 0; i < NUM_ITERATIONS; i++) {
	sock = PR_NewTCPSocket();
	PR_ASSERT(sock != NULL);
	
    sts = PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(sts == PR_SUCCESS);

	n = PR_Write(sock, buf, sizeof(buf));
	PR_ASSERT(n >= 0);

	sts = PR_Close(sock);
	PR_ASSERT(sts == PR_SUCCESS);
    }
}

int main(int argc, char **argv)
{
    PRFileDesc *listenSock1 = NULL, *listenSock2 = NULL;
    PRUint16 listenPort1, listenPort2;
    PRNetAddr addr;
    char buf[128];
    PRThread *clientThread;
    PRPollDesc pds0[20], pds1[20], *pds, *other_pds;
    PRIntn npds;
    PRInt32 retVal;
    PRIntn i, j;
    PRSocketOptionData optval;

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

#ifdef XP_MAC
	debug_mode = 1;
	SetupMacPrintfLog("poll_nm.log");
#endif

    if (debug_mode) {
		printf("This program tests PR_Poll with sockets.\n");
		printf("Normal operation are tested.\n\n");
	}

    /* Create two listening sockets */
    if ((listenSock1 = PR_NewTCPSocket()) == NULL) {
	fprintf(stderr, "Can't create a new TCP socket\n");
	failed_already=1;
	goto exit_now;
    }
    memset(&addr, 0, sizeof(addr));
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
    optval.option = PR_SockOpt_Nonblocking;
    optval.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(listenSock1, &optval);
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
    PR_SetSocketOption(listenSock2, &optval);
    if (PR_Listen(listenSock2, 5) == PR_FAILURE) {
	fprintf(stderr, "Can't listen on a socket\n");
	failed_already=1;	
	goto exit_now;
    }
    PR_snprintf(buf, sizeof(buf),
	    "The server thread is listening on ports %hu and %hu\n\n",
	    listenPort1, listenPort2);
    if (debug_mode) printf("%s", buf);

    /* Set up the poll descriptor array */
    pds = pds0;
    other_pds = pds1;
    memset(pds, 0, sizeof(pds));
    pds[0].fd = listenSock1;
    pds[0].in_flags = PR_POLL_READ;
    pds[1].fd = listenSock2;
    pds[1].in_flags = PR_POLL_READ;
    /* Add some unused entries to test if they are ignored by PR_Poll() */
    memset(&pds[2], 0, sizeof(pds[2]));
    memset(&pds[3], 0, sizeof(pds[3]));
    memset(&pds[4], 0, sizeof(pds[4]));
    npds = 5;

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

    /* two clients, three events per iteration: accept, read, close */
    i = 0;
    while (i < 2 * 3 * NUM_ITERATIONS) {
	PRPollDesc *tmp;
	int nextIndex;
	int nEvents = 0;

	retVal = PR_Poll(pds, npds, PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(retVal != 0);  /* no timeout */
	if (retVal == -1) {
	    fprintf(stderr, "PR_Poll failed\n");
		failed_already=1;			
	    goto exit_now;
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
			failed_already=1;	
		    goto exit_now;
		}
		other_pds[nextIndex].fd = sock;
		other_pds[nextIndex].in_flags = PR_POLL_READ;
		nextIndex++;
	    } else if (pds[j].out_flags & PR_POLL_ERR) {
		fprintf(stderr, "PR_Poll() indicates that an fd has error\n");
		failed_already=1;	
		goto exit_now;
	    } else if (pds[j].out_flags & PR_POLL_NVAL) {
		fprintf(stderr, "PR_Poll() indicates that fd %d is invalid\n",
			PR_FileDesc2NativeHandle(pds[j].fd));
		failed_already=1;	
		goto exit_now;
	    }
	}

	for (j = 2; j < npds; j++) {
            if (NULL == pds[j].fd) {
                /*
                 * Keep the unused entries in the poll descriptor array
                 * for testing purposes.
                 */
                other_pds[nextIndex] = pds[j];
                nextIndex++;
                continue;
            }

	    PR_ASSERT((pds[j].out_flags & PR_POLL_WRITE) == 0
		    && (pds[j].out_flags & PR_POLL_EXCEPT) == 0);
	    if (pds[j].out_flags & PR_POLL_READ) {
                PRInt32 nAvail;
		PRInt32 nRead;

		nEvents++;
                nAvail = PR_Available(pds[j].fd);
		nRead = PR_Read(pds[j].fd, buf, sizeof(buf));
                PR_ASSERT(nAvail == nRead);
		if (nRead == -1) {
		    fprintf(stderr, "PR_Read() failed\n");
			failed_already=1;	
		    goto exit_now;
                } else if (nRead == 0) {
                    PR_Close(pds[j].fd);
                    continue;
                } else {
                    /* Just to be safe */
                    buf[127] = '\0';
                    if (debug_mode) printf("The server received \"%s\" from a client\n", buf);
                }
	    } else if (pds[j].out_flags & PR_POLL_ERR) {
		fprintf(stderr, "PR_Poll() indicates that an fd has error\n");
		failed_already=1;			
		goto exit_now;
	    } else if (pds[j].out_flags & PR_POLL_NVAL) {
		fprintf(stderr, "PR_Poll() indicates that an fd is invalid\n");
		failed_already=1;			
		goto exit_now;
	    }
            other_pds[nextIndex] = pds[j];
            nextIndex++;
	}

	PR_ASSERT(retVal == nEvents);
	/* swap */
	tmp = pds;
	pds = other_pds;
	other_pds = tmp;
	npds = nextIndex;
	i += nEvents;
    }

    if (debug_mode) printf("Tests passed\n");

exit_now:

    if (listenSock1) {
        PR_Close(listenSock1);
    }
    if (listenSock2) {
        PR_Close(listenSock2);
    }

    PR_Cleanup();
	
	if(failed_already)	
		return 1;
	else
		return 0;

}
