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

/*
 * A test for nonblocking connect.  Functions tested include PR_Connect,
 * PR_Poll, and PR_GetConnectStatus.
 *
 * The test should be invoked with a host name, for example:
 *     nbconn www.netscape.com
 * It will do a nonblocking connect to port 80 (HTTP) on that host,
 * and when connected, issue the "GET /" HTTP command.
 *
 * You should run this test in three ways:
 * 1. To a known web site, such as www.netscape.com.  The HTML of the
 *    top-level page at the web site should be printed.
 * 2. To a machine not running a web server at port 80.  This test should
 *    fail.  Ideally the error code should be PR_CONNECT_REFUSED_ERROR.
 *    But it is possible to return PR_UNKNOWN_ERROR on certain platforms.
 * 3. To an unreachable machine, for example, a machine that is off line.
 *    The test should fail after the connect times out.  Ideally the
 *    error code should be PR_IO_TIMEOUT_ERROR, but it is possible to
 *    return PR_UNKNOWN_ERROR on certain platforms.
 */

#include "nspr.h"
#include <stdio.h>

#ifdef XP_MAC
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
static char *hosts[4] = {"cynic", "warp", "gandalf", "neon"};
#endif


int main(int argc, char **argv)
{
    PRHostEnt he;
    char buf[1024];
    PRNetAddr addr;
    PRFileDesc *sock;
    PRPollDesc pd;
    PRStatus rv;
    PRSocketOptionData optData;
    PRIntn n;

#ifdef XP_MAC
	int index;
	PRIntervalTime timeout;
	SetupMacPrintfLog("nbconn.log");
	for (index=0; index<4; index++) {
	argv[1] = hosts[index];
	timeout = PR_INTERVAL_NO_TIMEOUT;
	if (index == 3)
		timeout = PR_SecondsToInterval(10UL);
#endif

    PR_STDIO_INIT();
#ifndef XP_MAC
    if (argc != 2) {
        fprintf(stderr, "Usage: nbconn <hostname>\n");
        exit(1);
    }
#endif

    if (PR_GetHostByName(argv[1], buf, sizeof(buf), &he) == PR_FAILURE) {
        printf( "Unknown host: %s\n", buf);
        exit(1);
    } else {
        printf( "host: %s\n", buf);
    }
    PR_EnumerateHostEnt(0, &he, 80, &addr);

    sock = PR_NewTCPSocket();
    optData.option = PR_SockOpt_Nonblocking;
    optData.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(sock, &optData);
    rv = PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT);
    if (rv == PR_FAILURE && PR_GetError() == PR_IN_PROGRESS_ERROR) {
        printf( "Connect in progress\n");
    }

    pd.fd = sock;
    pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
#ifndef XP_MAC
    n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
#else
    n = PR_Poll(&pd, 1, timeout);
#endif
    if (n == -1) {
        printf( "PR_Poll failed\n");
        exit(1);
    }
    printf( "PR_Poll returns %d\n", n);
    if (pd.out_flags & PR_POLL_READ) {
        printf( "PR_POLL_READ\n");
    }
    if (pd.out_flags & PR_POLL_WRITE) {
        printf( "PR_POLL_WRITE\n");
    }
    if (pd.out_flags & PR_POLL_EXCEPT) {
        printf( "PR_POLL_EXCEPT\n");
    }
    if (pd.out_flags & PR_POLL_ERR) {
        printf( "PR_POLL_ERR\n");
    }
    if (pd.out_flags & PR_POLL_NVAL) {
        printf( "PR_POLL_NVAL\n");
    }

    if (PR_GetConnectStatus(&pd) == PR_SUCCESS) {
        printf("PR_GetConnectStatus: connect succeeded\n");
        /* Mac and Win16 have trouble printing to the console. */
#if !defined(XP_MAC) && !defined(WIN16)
        PR_Write(sock, "GET /\r\n\r\n", 9);
        PR_Shutdown(sock, PR_SHUTDOWN_SEND);
        pd.in_flags = PR_POLL_READ;
        while (1) {
            n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
            printf( "poll returns %d\n", n);
            n = PR_Read(sock, buf, sizeof(buf));
            printf( "read returns %d\n", n);
            if (n <= 0) {
                break;
            }
            PR_Write(PR_STDOUT, buf, n);
        }
#endif
    } else {
        if (PR_GetError() == PR_IN_PROGRESS_ERROR) {
            printf( "PR_GetConnectStatus: connect still in progress\n");
            exit(1);
        }
        printf( "PR_GetConnectStatus: connect failed: (%ld, %ld)\n",
                PR_GetError(), PR_GetOSError());
    }
    PR_Close(sock);
#ifdef XP_MAC
	} /* end of for loop */
#endif

    printf( "PASS\n");
    return 0;
}
