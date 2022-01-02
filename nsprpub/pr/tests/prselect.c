/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1997 - Netscape Communications Corporation
**
** Name: prselect_err.c
**
** Description: tests PR_Select with sockets Error condition functions.
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**           The debug mode will print all of the printfs associated with this test.
**           The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**           have been handled with an if (debug_mode) statement.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"
#include "prttools.h"


#include "prinit.h"
#include "prio.h"
#include "prlog.h"
#include "prprf.h"
#include "prerror.h"
#include "prnetdb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************************
** PRIVATE FUNCTION:    Test_Result
** DESCRIPTION: Used in conjunction with the regress tool, prints out the
**              status of the test case.
** INPUTS:      PASS/FAIL
** OUTPUTS:     None
** RETURN:      None
** SIDE EFFECTS:
**
** RESTRICTIONS:
**      None
** MEMORY:      NA
** ALGORITHM:   Determine what the status is and print accordingly.
**
***********************************************************************/


static Test_Result (int result)
{
    if (result == PASS) {
        printf ("PASS\n");
    }
    else {
        printf ("FAIL\n");
    }
}

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
        if (PL_OPT_BAD == os) {
            continue;
        }
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
        printf("This program tests PR_Select with sockets.  Timeout, error\n");
        printf("reporting, and normal operation are tested.\n\n");
    }

    /* Create two listening sockets */
    if ((listenSock1 = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr, "Can't create a new TCP socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock1, &addr) == PR_FAILURE) {
        fprintf(stderr, "Can't bind socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    if (PR_GetSockName(listenSock1, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_GetSockName failed\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    listenPort1 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock1, 5) == PR_FAILURE) {
        fprintf(stderr, "Can't listen on a socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }

    if ((listenSock2  = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr, "Can't create a new TCP socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = PR_htons(0);
    if (PR_Bind(listenSock2, &addr) == PR_FAILURE) {
        fprintf(stderr, "Can't bind socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    if (PR_GetSockName(listenSock2, &addr) == PR_FAILURE) {
        fprintf(stderr, "PR_GetSockName failed\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    listenPort2 = PR_ntohs(addr.inet.port);
    if (PR_Listen(listenSock2, 5) == PR_FAILURE) {
        fprintf(stderr, "Can't listen on a socket\n");
        if (!debug_mode) {
            Test_Result(FAIL);
        }
        exit(1);
    }
    PR_snprintf(buf, sizeof(buf),
                "The server thread is listening on ports %hu and %hu\n\n",
                listenPort1, listenPort2);
    printf("%s", buf);

    /* Set up the fd set */
    PR_FD_ZERO(&readFdSet);
    PR_FD_SET(listenSock1, &readFdSet);
    PR_FD_SET(listenSock2, &readFdSet);

    /* Testing timeout */
    if (debug_mode) {
        printf("PR_Select should time out in 5 seconds\n");
    }
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
            if (!debug_mode) {
                Test_Result(FAIL);
            }
        }
        exit(1);
    }
    if (debug_mode) {
        printf("PR_Select timed out.  Test passed.\n\n");
    }
    else {
        Test_Result(PASS);
    }

    /* Testing bad fd */
    printf("PR_Select should detect a bad file descriptor\n");
    if ((badFD = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr, "Can't create a TCP socket\n");
        exit(1);
    }

    PR_FD_SET(listenSock1, &readFdSet);
    PR_FD_SET(listenSock2, &readFdSet);
    PR_FD_SET(badFD, &readFdSet);
    PR_Close(badFD);  /* make the fd bad */
    retVal = PR_Select(0 /* unused */, &readFdSet, NULL, NULL,
                       PR_INTERVAL_NO_TIMEOUT);
    if (retVal != -1 || PR_GetError() != PR_BAD_DESCRIPTOR_ERROR) {
        fprintf(stderr, "Failed to detect the bad fd: "
                "PR_Select returns %d\n", retVal);
        if (retVal == -1) {
            fprintf(stderr, "Error %d, oserror %d\n", PR_GetError(),
                    PR_GetOSError());
        }
        exit(1);
    }
    printf("PR_Select detected a bad fd.  Test passed.\n\n");
    PR_FD_CLR(badFD, &readFdSet);

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

    /* set up the fd array */
    fds = fds0;
    other_fds = fds1;
    fds[0] = listenSock1;
    fds[1] = listenSock2;
    nfds = 2;
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
            exit(1);
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
                    exit(1);
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
                    exit(1);
                }
                /* Just to be safe */
                buf[127] = '\0';
                PR_Close(fds[j]);
                printf("The server received \"%s\" from a client\n", buf);
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

    printf("All tests finished\n");
    PR_Cleanup();
    return 0;
}
