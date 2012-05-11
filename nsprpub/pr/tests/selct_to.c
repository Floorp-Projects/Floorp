/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1997 - Netscape Communications Corporation
**
** Name: prselect_to.c
**
** Description: tests PR_Select with sockets. Time out functions
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
#include "prnetdb.h"

#include "obsolete/probslet.h"

#include "prerror.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

PRIntn failed_already=0;
PRIntn debug_mode;

int main(int argc, char **argv)
{
    PRFileDesc *listenSock1, *listenSock2;
    PRUint16 listenPort1, listenPort2;
    PRNetAddr addr;
    PR_fd_set readFdSet;
    char buf[128];
    PRInt32 retVal;

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
		printf("This program tests PR_Select with sockets.  Timeout \n");
		printf("operations are tested.\n\n");
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

    /* Set up the fd set */
    PR_FD_ZERO(&readFdSet);
    PR_FD_SET(listenSock1, &readFdSet);
    PR_FD_SET(listenSock2, &readFdSet);

    /* Testing timeout */
    if (debug_mode) printf("PR_Select should time out in 5 seconds\n");
    retVal = PR_Select(0 /* unused */, &readFdSet, NULL, NULL,
	    PR_SecondsToInterval(5));
    if (retVal != 0) {
	PR_snprintf(buf, sizeof(buf),
		"PR_Select should time out and return 0, but it returns %ld\n",
		retVal);
	fprintf(stderr, "%s", buf);
	if (retVal == -1) {
	    fprintf(stderr, "Error %d, oserror %d\n", PR_GetError(),
		    PR_GetOSError());
			failed_already=1;
	}
	goto exit_now;
    }
    if (debug_mode) printf("PR_Select timed out.  Test passed.\n\n");

    PR_Cleanup();

exit_now:
	if(failed_already)	
		return 1;
	else
		return 0;
}
