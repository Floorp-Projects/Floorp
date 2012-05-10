/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1996 - Netscape Communications Corporation
**
** Name: accept.c
**
** Description: Run accept() sucessful connection tests.
**
** Modification History:
** 04-Jun-97 AGarcia - Reconvert test file to return a 0 for PASS and a 1 for FAIL
** 13-May-97 AGarcia- Converted the test to accomodate the debug_mode 
**             The debug mode will print all of the printfs associated with this test.
**             The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**             have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**            recognize the return code from tha main program.
** 12-June-97 Revert to return code 0 and 1.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/

#include "nspr.h"
#include "prpriv.h"

#include <stdlib.h>
#include <string.h>

#include "plgetopt.h"
#include "plerror.h"

#define BASE_PORT 10000

#define CLIENT_DATA        128

#define ACCEPT_NORMAL        0x1
#define ACCEPT_FAST        0x2
#define ACCEPT_READ        0x3
#define ACCEPT_READ_FAST    0x4
#define ACCEPT_READ_FAST_CB    0x5

#define CLIENT_NORMAL        0x1
#define CLIENT_TIMEOUT_ACCEPT    0x2
#define CLIENT_TIMEOUT_SEND    0x3

#define SERVER_MAX_BIND_COUNT        100

#if defined(XP_OS2) || defined(SYMBIAN)
#define TIMEOUTSECS 10
#else
#define TIMEOUTSECS 2
#endif
PRIntervalTime timeoutTime;

static PRInt32 count = 1;
static PRFileDesc *output;
static PRNetAddr serverAddr;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;
static PRInt32 clientCommand;
static PRInt32 iterations;
static PRStatus rv;
static PRFileDesc *listenSock;
static PRFileDesc *clientSock = NULL;
static PRNetAddr listenAddr;
static PRNetAddr clientAddr;
static PRThread *clientThread;
static PRNetAddr *raddr;
static char buf[4096 + 2*sizeof(PRNetAddr) + 32];
static PRInt32 status;
static PRInt32 bytesRead;

PRIntn failed_already=0;
PRIntn debug_mode;

void Test_Assert(const char *msg, const char *file, PRIntn line)
{
    failed_already=1;
    if (debug_mode) {
        PR_fprintf(output,  "@%s:%d ", file, line);
        PR_fprintf(output, msg);
    }
}  /* Test_Assert */

#define TEST_ASSERT(expr) \
    if (!(expr)) Test_Assert(#expr, __FILE__, __LINE__)

#ifdef WINNT
#define CALLBACK_MAGIC 0x12345678

void timeout_callback(void *magic)
{
    TEST_ASSERT(magic == (void *)CALLBACK_MAGIC);
    if (debug_mode)
        PR_fprintf(output, "timeout callback called okay\n");
}
#endif


static void PR_CALLBACK
ClientThread(void *_action)
{
    PRInt32 action = * (PRInt32 *) _action;
    PRInt32 iterations = count;
    PRFileDesc *sock = NULL;

    serverAddr.inet.family = PR_AF_INET;
    serverAddr.inet.port = listenAddr.inet.port;
    serverAddr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);

    for (; iterations--;) {
        PRInt32 rv;
        char buf[CLIENT_DATA];

        memset(buf, 0xaf, sizeof(buf)); /* initialize with arbitrary data */
        sock = PR_NewTCPSocket();
        if (!sock) {
            if (!debug_mode)
                failed_already=1;
            else    
                PR_fprintf(output, "client: unable to create socket\n");
            return;
        }

        if (action != CLIENT_TIMEOUT_ACCEPT) {

            if ((rv = PR_Connect(sock, &serverAddr,
                timeoutTime)) < 0) {
                if (!debug_mode)
                    failed_already=1;
                else    
                    PR_fprintf(output, 
                        "client: unable to connect to server (%ld, %ld, %ld, %ld)\n",
                        iterations, rv, PR_GetError(), PR_GetOSError());
                goto ErrorExit;
            }

            if (action != CLIENT_TIMEOUT_SEND) {
                if ((rv = PR_Send(sock, buf, CLIENT_DATA,
                    0, timeoutTime))< 0) {
                    if (!debug_mode)
                        failed_already=1;
                    else    
                        PR_fprintf(output, 
                            "client: unable to send to server (%d, %ld, %ld)\n",
                            CLIENT_DATA, rv, PR_GetError());
                	goto ErrorExit;
                }
            } else {
                PR_Sleep(PR_SecondsToInterval(TIMEOUTSECS + 1));
            }
        } else {
            PR_Sleep(PR_SecondsToInterval(TIMEOUTSECS + 1));
        }
        if (debug_mode)
            PR_fprintf(output, ".");
        PR_Close(sock);
		sock = NULL;
    }
    if (debug_mode)
        PR_fprintf(output, "\n");

ErrorExit:
	if (sock != NULL)
        PR_Close(sock);
}


static void 
RunTest(PRInt32 acceptType, PRInt32 clientAction)
{
int i;

    /* First bind to the socket */
    listenSock = PR_NewTCPSocket();
    if (!listenSock) {
        failed_already=1;
        if (debug_mode)
            PR_fprintf(output, "unable to create listen socket\n");
        return;
    }
	memset(&listenAddr, 0 , sizeof(listenAddr));
    listenAddr.inet.family = PR_AF_INET;
    listenAddr.inet.port = PR_htons(BASE_PORT);
    listenAddr.inet.ip = PR_htonl(PR_INADDR_ANY);
    /*
     * try a few times to bind server's address, if addresses are in
     * use
     */
    i = 0;
    while (PR_Bind(listenSock, &listenAddr) == PR_FAILURE) {
        if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) {
            listenAddr.inet.port += 2;
            if (i++ < SERVER_MAX_BIND_COUNT)
                continue;
        }
        failed_already=1;
        if (debug_mode)
        	PR_fprintf(output,"accept: ERROR - PR_Bind failed\n");
		return;
    }


    rv = PR_Listen(listenSock, 100);
    if (rv == PR_FAILURE) {
        failed_already=1;
        if (debug_mode)
            PR_fprintf(output, "unable to listen\n");
        return;
    }

    clientCommand = clientAction;
    clientThread = PR_CreateThread(PR_USER_THREAD, ClientThread,
        (void *)&clientCommand, PR_PRIORITY_NORMAL, thread_scope,
        PR_JOINABLE_THREAD, 0);
    if (!clientThread) {
        failed_already=1;
        if (debug_mode)
            PR_fprintf(output, "error creating client thread\n");
        return;
    }

    iterations = count;
    for (;iterations--;) {
        switch (acceptType) {
        case ACCEPT_NORMAL:
            clientSock = PR_Accept(listenSock, &clientAddr,
                timeoutTime);
            switch(clientAction) {
            case CLIENT_TIMEOUT_ACCEPT:
                TEST_ASSERT(clientSock == 0);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            case CLIENT_NORMAL:
                TEST_ASSERT(clientSock);
                bytesRead = PR_Recv(clientSock,
                    buf,  CLIENT_DATA,  0,  timeoutTime);
                TEST_ASSERT(bytesRead == CLIENT_DATA);
                break;
            case CLIENT_TIMEOUT_SEND:
                TEST_ASSERT(clientSock);
                bytesRead = PR_Recv(clientSock,
                    buf,  CLIENT_DATA,  0,  timeoutTime);
                TEST_ASSERT(bytesRead == -1);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            }
            break;
        case ACCEPT_READ:
            status = PR_AcceptRead(listenSock, &clientSock,
                &raddr, buf, CLIENT_DATA, timeoutTime);
            switch(clientAction) {
            case CLIENT_TIMEOUT_ACCEPT:
                /* Invalid test case */
                TEST_ASSERT(0);
                break;
            case CLIENT_NORMAL:
                TEST_ASSERT(clientSock);
                TEST_ASSERT(status == CLIENT_DATA);
                break;
            case CLIENT_TIMEOUT_SEND:
                TEST_ASSERT(status == -1);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            }
            break;
#ifdef WINNT
        case ACCEPT_FAST:
            clientSock = PR_NTFast_Accept(listenSock,
                &clientAddr, timeoutTime);
            switch(clientAction) {
            case CLIENT_TIMEOUT_ACCEPT:
                TEST_ASSERT(clientSock == 0);
                if (debug_mode)
                    PR_fprintf(output, "PR_GetError is %ld\n", PR_GetError());
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            case CLIENT_NORMAL:
                TEST_ASSERT(clientSock);
                bytesRead = PR_Recv(clientSock,
                    buf,  CLIENT_DATA,  0,  timeoutTime);
                TEST_ASSERT(bytesRead == CLIENT_DATA);
                break;
            case CLIENT_TIMEOUT_SEND:
                TEST_ASSERT(clientSock);
                bytesRead = PR_Recv(clientSock,
                    buf,  CLIENT_DATA,  0,  timeoutTime);
                TEST_ASSERT(bytesRead == -1);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            }
            break;
            break;
        case ACCEPT_READ_FAST:
            status = PR_NTFast_AcceptRead(listenSock,
                &clientSock, &raddr, buf, 4096, timeoutTime);
            switch(clientAction) {
            case CLIENT_TIMEOUT_ACCEPT:
                /* Invalid test case */
                TEST_ASSERT(0);
                break;
            case CLIENT_NORMAL:
                TEST_ASSERT(clientSock);
                TEST_ASSERT(status == CLIENT_DATA);
                break;
            case CLIENT_TIMEOUT_SEND:
                TEST_ASSERT(clientSock == NULL);
                TEST_ASSERT(status == -1);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            }
            break;
        case ACCEPT_READ_FAST_CB:
            status = PR_NTFast_AcceptRead_WithTimeoutCallback(
                listenSock, &clientSock, &raddr, buf, 4096,
                timeoutTime, timeout_callback, (void *)CALLBACK_MAGIC);
            switch(clientAction) {
            case CLIENT_TIMEOUT_ACCEPT:
                /* Invalid test case */
                TEST_ASSERT(0);
                break;
            case CLIENT_NORMAL:
                TEST_ASSERT(clientSock);
                TEST_ASSERT(status == CLIENT_DATA);
                break;
            case CLIENT_TIMEOUT_SEND:
                if (debug_mode)
                    PR_fprintf(output, "clientSock = 0x%8.8lx\n", clientSock);
                TEST_ASSERT(clientSock == NULL);
                TEST_ASSERT(status == -1);
                TEST_ASSERT(PR_GetError() == PR_IO_TIMEOUT_ERROR);
                break;
            }
            break;
#endif
        }
        if (clientSock != NULL) {
            PR_Close(clientSock);
            clientSock = NULL;
        }
    }
    PR_Close(listenSock);

    PR_JoinThread(clientThread);
}


void AcceptUpdatedTest(void)
{ 
    RunTest(ACCEPT_NORMAL, CLIENT_NORMAL); 
}
void AcceptNotUpdatedTest(void)
{ 
    RunTest(ACCEPT_FAST, CLIENT_NORMAL); 
}
void AcceptReadTest(void)
{ 
    RunTest(ACCEPT_READ, CLIENT_NORMAL); 
}
void AcceptReadNotUpdatedTest(void)
{ 
    RunTest(ACCEPT_READ_FAST, CLIENT_NORMAL); 
}
void AcceptReadCallbackTest(void)
{ 
    RunTest(ACCEPT_READ_FAST_CB, CLIENT_NORMAL); 
}

void TimeoutAcceptUpdatedTest(void)
{ 
    RunTest(ACCEPT_NORMAL, CLIENT_TIMEOUT_ACCEPT); 
}
void TimeoutAcceptNotUpdatedTest(void)
{ 
    RunTest(ACCEPT_FAST, CLIENT_TIMEOUT_ACCEPT); 
}
void TimeoutAcceptReadCallbackTest(void)
{ 
    RunTest(ACCEPT_READ_FAST_CB, CLIENT_TIMEOUT_ACCEPT); 
}

void TimeoutReadUpdatedTest(void)
{ 
    RunTest(ACCEPT_NORMAL, CLIENT_TIMEOUT_SEND); 
}
void TimeoutReadNotUpdatedTest(void)
{ 
    RunTest(ACCEPT_FAST, CLIENT_TIMEOUT_SEND); 
}
void TimeoutReadReadTest(void)
{ 
    RunTest(ACCEPT_READ, CLIENT_TIMEOUT_SEND); 
}
void TimeoutReadReadNotUpdatedTest(void)
{ 
    RunTest(ACCEPT_READ_FAST, CLIENT_TIMEOUT_SEND); 
}
void TimeoutReadReadCallbackTest(void)
{ 
    RunTest(ACCEPT_READ_FAST_CB, CLIENT_TIMEOUT_SEND); 
}

/************************************************************************/

static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);
    if (debug_mode)
        PR_fprintf(output, "%40s: %6.2f usec\n", msg, d / count);

}

int main(int argc, char **argv)
{

    /* The command line argument: -d is used to determine if the test is being run
    in debug mode. The regress tool requires only one line output:PASS or FAIL.
    All of the printfs associated with this test has been handled with a if (debug_mode)
    test.
    Usage: test_name [-d] [-c n]
    */
    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "Gdc:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'G':  /* global threads */
            thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'd':  /* debug mode */
            debug_mode = 1;
            break;
        case 'c':  /* loop counter */
            count = atoi(opt->value);
            break;
        default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    output = PR_STDERR;
    PR_STDIO_INIT();

    timeoutTime = PR_SecondsToInterval(TIMEOUTSECS);
    if (debug_mode)
        PR_fprintf(output, "\nRun accept() sucessful connection tests\n");

    Measure(AcceptUpdatedTest, "PR_Accept()");
    Measure(AcceptReadTest, "PR_AcceptRead()");
#ifdef WINNT
    Measure(AcceptNotUpdatedTest, "PR_NTFast_Accept()");
    Measure(AcceptReadNotUpdatedTest, "PR_NTFast_AcceptRead()");
    Measure(AcceptReadCallbackTest, "PR_NTFast_AcceptRead_WithTimeoutCallback()");
#endif
    if (debug_mode)
        PR_fprintf(output, "\nRun accept() timeout in the accept tests\n");
#ifdef WINNT
    Measure(TimeoutReadReadCallbackTest, "PR_NTFast_AcceptRead_WithTimeoutCallback()");
#endif
    Measure(TimeoutReadUpdatedTest, "PR_Accept()");
    if (debug_mode)
        PR_fprintf(output, "\nRun accept() timeout in the read tests\n");
    Measure(TimeoutReadReadTest, "PR_AcceptRead()");
#ifdef WINNT
    Measure(TimeoutReadNotUpdatedTest, "PR_NTFast_Accept()");
    Measure(TimeoutReadReadNotUpdatedTest, "PR_NTFast_AcceptRead()");
    Measure(TimeoutReadReadCallbackTest, "PR_NTFast_AcceptRead_WithTimeoutCallback()");
#endif
    PR_fprintf(output, "%s\n", (failed_already) ? "FAIL" : "PASS");
    return failed_already;
}
