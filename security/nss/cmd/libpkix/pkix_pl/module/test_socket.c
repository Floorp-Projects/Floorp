/*
 * test_socket.c
 *
 * Test Socket Type
 *
 * Copyright 2004-2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistribution of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistribution in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * Neither the name of Sun Microsystems, Inc. or the names of contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING
 * ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MICROSYSTEMS, INC. ("SUN")
 * AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE
 * AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS
 * DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST
 * REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL,
 * INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY
 * OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * You acknowledge that this software is not designed or intended for use in
 * the design, construction, operation or maintenance of any nuclear facility.
 */

#include "testutil.h"
#include "testutil_nss.h"
#include "pkix_pl_common.h"

#define LDAP_PORT 389

static void *plContext = NULL;

typedef enum {
    SERVER_LISTENING,
    SERVER_RECV1,
    SERVER_POLL1,
    SERVER_SEND2,
    SERVER_POLL2,
    SERVER_RECV3,
    SERVER_POLL3,
    SERVER_SEND4,
    SERVER_POLL4,
    SERVER_DONE,
    SERVER_FAILED
} SERVER_STATE;

typedef enum {
    CLIENT_WAITFORCONNECT,
    CLIENT_SEND1,
    CLIENT_POLL1,
    CLIENT_RECV2,
    CLIENT_POLL2,
    CLIENT_SEND3,
    CLIENT_POLL3,
    CLIENT_RECV4,
    CLIENT_POLL4,
    CLIENT_DONE,
    CLIENT_FAILED
} CLIENT_STATE;

SERVER_STATE serverState;
CLIENT_STATE clientState;
PKIX_PL_Socket *sSock = NULL;
PKIX_PL_Socket *cSock = NULL;
PKIX_PL_Socket *rendezvousSock = NULL;
PKIX_PL_Socket_Callback *sCallbackList;
PKIX_PL_Socket_Callback *cCallbackList;
PKIX_PL_Socket_Callback *rvCallbackList;
PRNetAddr serverNetAddr;
PRNetAddr clientNetAddr;
PRIntn backlog = 0;
PRIntervalTime timeout = 0;
char *sendBuf1 = "Hello, world!";
char *sendBuf2 = "Ack";
char *sendBuf3 = "What do you mean, \"Ack\"?";
char *sendBuf4 = "What do you mean, \"What do you mean, \'Ack\'?\"?";
char rcvBuf1[100];
char rcvBuf2[100];

static void
printUsage(char *testname)
{
    char *fmt = "USAGE: %s [-arenas] server:port\n";
    printf(fmt, testname);
}

/* Functional tests for Socket public functions */
static void
do_other_work(void)
{ /* while waiting for nonblocking I/O to complete */
    (void)PR_Sleep(2 * 60);
}

static PKIX_Boolean
server()
{
    PKIX_Int32 bytesRead = 0;
    PKIX_Int32 bytesWritten = 0;
    PKIX_Boolean keepGoing = PKIX_FALSE;

    PKIX_TEST_STD_VARS();

    switch (serverState) {
        case SERVER_LISTENING:
            subTest("SERVER_LISTENING");
            PKIX_TEST_EXPECT_NO_ERROR(sCallbackList->acceptCallback(sSock, &rendezvousSock, plContext));
            if (rendezvousSock) {
                PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_GetCallbackList(rendezvousSock, &rvCallbackList, plContext));

                serverState = SERVER_RECV1;
            }
            break;
        case SERVER_RECV1:
            subTest("SERVER_RECV1");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->recvCallback(rendezvousSock,
                                                                   rcvBuf1,
                                                                   sizeof(rcvBuf1),
                                                                   &bytesRead,
                                                                   plContext));

            if (bytesRead > 0) {
                /* confirm that rcvBuf1 = sendBuf1 */
                if ((bytesRead != (PRInt32)PL_strlen(sendBuf1) + 1) ||
                    (strncmp(sendBuf1, rcvBuf1, bytesRead) != 0)) {
                    testError("Receive buffer mismatch\n");
                }

                serverState = SERVER_SEND2;
                keepGoing = PKIX_TRUE;
            } else {
                serverState = SERVER_POLL1;
            }
            break;
        case SERVER_POLL1:
            subTest("SERVER_POLL1");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->pollCallback(rendezvousSock, NULL, &bytesRead, plContext));

            if (bytesRead > 0) {
                /* confirm that rcvBuf1 = sendBuf1 */
                if ((bytesRead != (PRInt32)PL_strlen(sendBuf1) + 1) ||
                    (strncmp(sendBuf1, rcvBuf1, bytesRead) != 0)) {
                    testError("Receive buffer mismatch\n");
                }

                serverState = SERVER_SEND2;
                keepGoing = PKIX_TRUE;
            }
            break;
        case SERVER_SEND2:
            subTest("SERVER_SEND2");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->sendCallback(rendezvousSock,
                                                                   sendBuf2,
                                                                   strlen(sendBuf2) +
                                                                       1,
                                                                   &bytesWritten,
                                                                   plContext));
            if (bytesWritten > 0) {
                serverState = SERVER_RECV3;
            } else {
                serverState = SERVER_POLL2;
            }
            break;
        case SERVER_POLL2:
            subTest("SERVER_POLL2");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->pollCallback(rendezvousSock, &bytesWritten, NULL, plContext));
            if (bytesWritten > 0) {
                serverState = SERVER_RECV3;
            }
            break;
        case SERVER_RECV3:
            subTest("SERVER_RECV3");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->recvCallback(rendezvousSock,
                                                                   rcvBuf1,
                                                                   sizeof(rcvBuf1),
                                                                   &bytesRead,
                                                                   plContext));

            if (bytesRead > 0) {
                serverState = SERVER_SEND4;
                keepGoing = PKIX_TRUE;
            } else {
                serverState = SERVER_POLL3;
            }
            break;
        case SERVER_POLL3:
            subTest("SERVER_POLL3");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->pollCallback(rendezvousSock, NULL, &bytesRead, plContext));
            if (bytesRead > 0) {
                serverState = SERVER_SEND4;
                keepGoing = PKIX_TRUE;
            }
            break;
        case SERVER_SEND4:
            subTest("SERVER_SEND4");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->sendCallback(rendezvousSock,
                                                                   sendBuf4,
                                                                   strlen(sendBuf4) +
                                                                       1,
                                                                   &bytesWritten,
                                                                   plContext));

            if (bytesWritten > 0) {
                PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->shutdownCallback(rendezvousSock, plContext));
                PKIX_TEST_DECREF_BC(sSock);
                PKIX_TEST_DECREF_BC(rendezvousSock);
                serverState = SERVER_DONE;
            } else {
                serverState = SERVER_POLL4;
            }
            break;
        case SERVER_POLL4:
            subTest("SERVER_POLL4");
            PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->pollCallback(rendezvousSock, &bytesWritten, NULL, plContext));
            if (bytesWritten > 0) {
                PKIX_TEST_EXPECT_NO_ERROR(rvCallbackList->shutdownCallback(rendezvousSock, plContext));
                PKIX_TEST_DECREF_BC(sSock);
                PKIX_TEST_DECREF_BC(rendezvousSock);
                serverState = SERVER_DONE;
            }
            break;
        case SERVER_DONE:
        default:
            subTest("SERVER_DONE");
            break;
    }

cleanup:

    PKIX_TEST_RETURN();

    return (keepGoing);
}

static PKIX_Boolean
client()
{
    PKIX_Boolean keepGoing = PKIX_FALSE;
    PKIX_Int32 bytesRead = 0;
    PKIX_Int32 bytesWritten = 0;
    PRErrorCode cStat = 0;

    /* At 2 seconds each cycle, this should suffice! */
    PKIX_UInt32 giveUpCount = 10;

    PKIX_TEST_STD_VARS();

    switch (clientState) {
        case CLIENT_WAITFORCONNECT:
            subTest("CLIENT_WAITFORCONNECT");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->connectcontinueCallback(cSock, &cStat, plContext));
            if (cStat == 0) {
                clientState = CLIENT_SEND1;
                keepGoing = PKIX_TRUE;
            } else {
                clientState = CLIENT_WAITFORCONNECT;
                if (--giveUpCount == 0) {
                    testError("Client unable to connect");
                }
            }
            break;
        case CLIENT_SEND1:
            subTest("CLIENT_SEND1");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->sendCallback(cSock,
                                                                  sendBuf1,
                                                                  strlen(sendBuf1) +
                                                                      1,
                                                                  &bytesWritten,
                                                                  plContext));
            if (bytesWritten > 0) {
                clientState = CLIENT_RECV2;
            } else {
                clientState = CLIENT_POLL1;
            }
            break;
        case CLIENT_POLL1:
            subTest("CLIENT_POLL1");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->pollCallback(cSock, &bytesWritten, NULL, plContext));
            if (bytesWritten > 0) {
                clientState = CLIENT_RECV2;
            } else {
                clientState = CLIENT_POLL1;
            }
            break;
        case CLIENT_RECV2:
            subTest("CLIENT_RECV2");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->recvCallback(cSock,
                                                                  rcvBuf2,
                                                                  sizeof(rcvBuf2),
                                                                  &bytesRead,
                                                                  plContext));

            if (bytesRead > 0) {
                /* confirm that rcvBuf2 = sendBuf2 */
                if ((bytesRead != (PRInt32)PL_strlen(sendBuf2) + 1) ||
                    (strncmp(sendBuf2, rcvBuf2, bytesRead) != 0)) {
                    testError("Receive buffer mismatch\n");
                }
                clientState = CLIENT_SEND3;
                keepGoing = PKIX_TRUE;
            } else {
                clientState = CLIENT_POLL2;
            }
            break;
        case CLIENT_POLL2:
            subTest("CLIENT_POLL2");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->pollCallback(cSock, NULL, &bytesRead, plContext));
            if (bytesRead > 0) {
                /* confirm that rcvBuf2 = sendBuf2 */
                if ((bytesRead != (PRInt32)PL_strlen(sendBuf2) + 1) ||
                    (strncmp(sendBuf2, rcvBuf2, bytesRead) != 0)) {
                    testError("Receive buffer mismatch\n");
                }
                clientState = CLIENT_SEND3;
            } else {
                clientState = CLIENT_POLL2;
            }
            break;
        case CLIENT_SEND3:
            subTest("CLIENT_SEND3");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->sendCallback(cSock,
                                                                  sendBuf3,
                                                                  strlen(sendBuf3) +
                                                                      1,
                                                                  &bytesWritten,
                                                                  plContext));

            if (bytesWritten > 0) {
                clientState = CLIENT_RECV4;
            } else {
                clientState = CLIENT_POLL3;
            }
            break;
        case CLIENT_POLL3:
            subTest("CLIENT_POLL3");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->pollCallback(cSock, &bytesWritten, NULL, plContext));
            if (bytesWritten > 0) {
                clientState = CLIENT_RECV4;
            } else {
                clientState = CLIENT_POLL3;
            }
            break;
        case CLIENT_RECV4:
            subTest("CLIENT_RECV4");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->recvCallback(cSock,
                                                                  rcvBuf2,
                                                                  sizeof(rcvBuf2),
                                                                  &bytesRead,
                                                                  plContext));

            if (bytesRead > 0) {
                PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->shutdownCallback(cSock, plContext));
                PKIX_TEST_DECREF_BC(cSock);
                clientState = CLIENT_DONE;
            } else {
                clientState = CLIENT_POLL4;
            }
            break;
        case CLIENT_POLL4:
            subTest("CLIENT_POLL4");
            clientState = CLIENT_FAILED;
            PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->pollCallback(cSock, NULL, &bytesRead, plContext));
            if (bytesRead > 0) {
                PKIX_TEST_EXPECT_NO_ERROR(cCallbackList->shutdownCallback(cSock, plContext));
                PKIX_TEST_DECREF_BC(cSock);
                clientState = CLIENT_DONE;
            } else {
                clientState = CLIENT_POLL4;
            }
            break;
        case CLIENT_DONE:
        default:
            subTest("CLIENT_DONE");
            break;
    }

cleanup:

    PKIX_TEST_RETURN();

    return (keepGoing);
}

static void
dispatcher()
{
    PKIX_Boolean keepGoing = PKIX_FALSE;

    PKIX_TEST_STD_VARS();

    do {
        if (serverState < SERVER_DONE) {
            do {
                keepGoing = server();
            } while (keepGoing == PKIX_TRUE);
        }
        if (clientState < CLIENT_DONE) {
            do {
                keepGoing = client();
            } while (keepGoing == PKIX_TRUE);
        }
        do_other_work();

    } while ((serverState < SERVER_DONE) || (clientState < CLIENT_DONE));

    PKIX_TEST_RETURN();
}

int
test_socket(int argc, char *argv[])
{

    int j = 0;
    PKIX_UInt32 actualMinorVersion;
    char buf[PR_NETDB_BUF_SIZE];
    char *serverName = NULL;
    char *sepPtr = NULL;
    PRHostEnt hostent;
    PRUint16 portNum = 0;
    PRStatus prstatus = PR_FAILURE;
    PRErrorCode cStat = 0;
    void *ipaddr = NULL;
    PKIX_Error *bindError = NULL;
    PRIntn hostenum;

    PKIX_TEST_STD_VARS();

    startTests("Socket");

    PKIX_TEST_EXPECT_NO_ERROR(
        PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

    if (argc != (j + 2)) {
        printUsage(argv[0]);
        pkixTestErrorMsg = "Missing command line argument.";
        goto cleanup;
    }

    serverName = argv[j + 1];

    subTest("Using pkix_pl_Socket_CreateByName");

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_CreateByName(PKIX_TRUE, timeout, serverName, &cStat, &sSock, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_GetCallbackList(sSock, &sCallbackList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(sCallbackList->listenCallback(sSock, backlog, plContext));

    serverState = SERVER_LISTENING;

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_CreateByName(PKIX_FALSE, timeout, serverName, &cStat, &cSock, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_GetCallbackList(cSock, &cCallbackList, plContext));

    if ((timeout == 0) && (cStat == PR_IN_PROGRESS_ERROR)) {
        clientState = CLIENT_WAITFORCONNECT;
    } else {
        clientState = CLIENT_SEND1;
    }

    dispatcher();

    subTest("Using pkix_pl_Socket_Create");

    sepPtr = strchr(serverName, ':');
    /* First strip off the portnum, if present, from the end of the name */
    if (sepPtr) {
        *sepPtr++ = '\0';
        portNum = (PRUint16)atoi(sepPtr);
    } else {
        portNum = (PRUint16)LDAP_PORT;
    }
    /*
         * The hostname may be a fully-qualified name. Just
         * use the leftmost component in our lookup.
         */
    sepPtr = strchr(serverName, '.');
    if (sepPtr) {
        *sepPtr++ = '\0';
    }
    prstatus = PR_GetHostByName(serverName, buf, sizeof(buf), &hostent);

    if ((prstatus != PR_SUCCESS) || (hostent.h_length != 4)) {
        printUsage(argv[0]);
        pkixTestErrorMsg =
            "PR_GetHostByName rejects command line argument.";
        goto cleanup;
    }

    serverNetAddr.inet.family = PR_AF_INET;
    serverNetAddr.inet.port = PR_htons(portNum);
    serverNetAddr.inet.ip = PR_INADDR_ANY;

    hostenum = PR_EnumerateHostEnt(0, &hostent, portNum, &clientNetAddr);
    if (hostenum == -1) {
        pkixTestErrorMsg =
            "PR_EnumerateHostEnt failed.";
        goto cleanup;
    }

    backlog = 5;

    /* timeout = PR_INTERVAL_NO_TIMEOUT; */
    /* timeout = 0; nonblocking */
    timeout = 0;

    bindError = pkix_pl_Socket_Create(PKIX_TRUE, timeout, &serverNetAddr, &cStat, &sSock, plContext);

    /* If PR_Bind can't handle INADDR_ANY, try it with the real name */
    if (bindError) {
        PKIX_TEST_DECREF_BC(bindError);
        serverNetAddr.inet.ip = PR_htonl(*(PRUint32 *)ipaddr);

        PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_Create(PKIX_TRUE,
                                                        timeout,
                                                        &serverNetAddr,
                                                        &cStat,
                                                        &sSock,
                                                        plContext));
    }

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_GetCallbackList(sSock, &sCallbackList, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(sCallbackList->listenCallback(sSock, backlog, plContext));

    serverState = SERVER_LISTENING;

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_Create(PKIX_FALSE, timeout, &clientNetAddr, &cStat, &cSock, plContext));

    PKIX_TEST_EXPECT_NO_ERROR(pkix_pl_Socket_GetCallbackList(cSock, &cCallbackList, plContext));

    if ((timeout == 0) && (cStat == PR_IN_PROGRESS_ERROR)) {
        clientState = CLIENT_WAITFORCONNECT;
    } else {
        clientState = CLIENT_SEND1;
    }

    dispatcher();

cleanup:

    PKIX_TEST_DECREF_AC(sSock);
    PKIX_TEST_DECREF_AC(cSock);
    PKIX_TEST_DECREF_AC(rendezvousSock);

    PKIX_TEST_RETURN();

    endTests("Socket");

    return (0);
}
