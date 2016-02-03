/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** This server simulates a server running in loopback mode.
**
** The idea is that a single server is created.  The server initially creates
** a number of worker threads.  Then, with the server running, a number of 
** clients are created which start requesting service from the server.
**
**
** Modification History:
** 19-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement. 
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "nspr.h"
#include "pprthred.h"

#include <string.h>

#define PORT 15004
#define THREAD_STACKSIZE 0

#define PASS 0
#define FAIL 1
static int debug_mode = 0;
static int failed_already = 0;

static int _iterations = 1000;
static int _clients = 1;
static int _client_data = 250;
static int _server_data = (8*1024);

static PRThreadScope ServerScope, ClientScope;

#define SERVER "Server"
#define MAIN   "Main"

#define SERVER_STATE_STARTUP 0
#define SERVER_STATE_READY   1
#define SERVER_STATE_DYING   2
#define SERVER_STATE_DEAD    4
int       ServerState;
PRLock    *ServerStateCVLock;
PRCondVar *ServerStateCV;

#undef DEBUGPRINTS
#ifdef DEBUGPRINTS
#define DPRINTF printf
#else
#define DPRINTF
#endif


/***********************************************************************
** PRIVATE FUNCTION:    Test_Result
** DESCRIPTION: Used in conjunction with the regress tool, prints out the
**				status of the test case.
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


static void Test_Result (int result)
{
	switch (result)
	{
		case PASS:
			printf ("PASS\n");
			break;
		case FAIL:
			printf ("FAIL\n");
			failed_already = 1;
			break;
		default:
			break;
	}
}

static void do_work(void);

/* --- Server state functions --------------------------------------------- */
void
SetServerState(char *waiter, PRInt32 state)
{
    PR_Lock(ServerStateCVLock);
    ServerState = state;
    PR_NotifyCondVar(ServerStateCV);

	if (debug_mode) DPRINTF("\t%s changed state to %d\n", waiter, state);

    PR_Unlock(ServerStateCVLock);
}

int
WaitServerState(char *waiter, PRInt32 state)
{
    PRInt32 rv;

    PR_Lock(ServerStateCVLock);

    if (debug_mode) DPRINTF("\t%s waiting for state %d\n", waiter, state);

    while(!(ServerState & state))
        PR_WaitCondVar(ServerStateCV, PR_INTERVAL_NO_TIMEOUT);
    rv = ServerState;

    if (debug_mode) DPRINTF("\t%s resuming from wait for state %d; state now %d\n", 
        waiter, state, ServerState);
    PR_Unlock(ServerStateCVLock);

    return rv;
}

/* --- Server Functions ------------------------------------------- */

PRLock *workerThreadsLock;
PRInt32 workerThreads;
PRInt32 workerThreadsBusy;

void
WorkerThreadFunc(void *_listenSock)
{
    PRFileDesc *listenSock = (PRFileDesc *)_listenSock;
    PRInt32 bytesRead;
    PRInt32 bytesWritten;
    char *dataBuf;
    char *sendBuf;

    if (debug_mode) DPRINTF("\tServer buffer is %d bytes; %d data, %d netaddrs\n",
            _client_data+(2*sizeof(PRNetAddr))+32, _client_data, (2*sizeof(PRNetAddr))+32);
    dataBuf = (char *)PR_MALLOC(_client_data + 2*sizeof(PRNetAddr) + 32);
    if (!dataBuf)
        if (debug_mode) printf("\tServer could not malloc space!?\n");
    sendBuf = (char *)PR_MALLOC(_server_data *sizeof(char));
    if (!sendBuf)
        if (debug_mode) printf("\tServer could not malloc space!?\n");

    if (debug_mode) DPRINTF("\tServer worker thread running\n");

    while(1) {
        PRInt32 bytesToRead = _client_data;
        PRInt32 bytesToWrite = _server_data;
        PRFileDesc *newSock;
        PRNetAddr *rAddr;
        PRInt32 loops = 0;

        loops++;

        if (debug_mode) DPRINTF("\tServer thread going into accept\n");

        bytesRead = PR_AcceptRead(listenSock, 
                                  &newSock,
                                  &rAddr,
                                  dataBuf,
                                  bytesToRead,
                                  PR_INTERVAL_NO_TIMEOUT);

        if (bytesRead < 0) {
            if (debug_mode) printf("\tServer error in accept (%d)\n", bytesRead);
            continue;
        }

        if (debug_mode) DPRINTF("\tServer accepted connection (%d bytes)\n", bytesRead);
        
        PR_AtomicIncrement(&workerThreadsBusy);
#ifdef SYMBIAN
        if (workerThreadsBusy == workerThreads && workerThreads<1) {
#else
        if (workerThreadsBusy == workerThreads) {
#endif
            PR_Lock(workerThreadsLock);
            if (workerThreadsBusy == workerThreads) {
                PRThread *WorkerThread;

                WorkerThread = PR_CreateThread(
                                  PR_SYSTEM_THREAD,
                                  WorkerThreadFunc,
                                  listenSock,
                                  PR_PRIORITY_NORMAL,
                                  ServerScope,
                                  PR_UNJOINABLE_THREAD,
                                  THREAD_STACKSIZE);

                if (!WorkerThread) {
                    if (debug_mode) printf("Error creating client thread %d\n", workerThreads);
                } else {
                    PR_AtomicIncrement(&workerThreads);
                    if (debug_mode) DPRINTF("\tServer creates worker (%d)\n", workerThreads);
                }
            }
            PR_Unlock(workerThreadsLock);
        }
 
        bytesToRead -= bytesRead;
        while (bytesToRead) {
            bytesRead = PR_Recv(newSock, 
                                dataBuf, 
                                bytesToRead, 
                                0, 
                                PR_INTERVAL_NO_TIMEOUT);
            if (bytesRead < 0) {
                if (debug_mode) printf("\tServer error receiving data (%d)\n", bytesRead);
                continue;
            }
            if (debug_mode) DPRINTF("\tServer received %d bytes\n", bytesRead);
        }

        bytesWritten = PR_Send(newSock,
                               sendBuf, 
                               bytesToWrite, 
                               0, 
                               PR_INTERVAL_NO_TIMEOUT);
        if (bytesWritten != _server_data) {
            if (debug_mode) printf("\tError sending data to client (%d, %d)\n", 
                bytesWritten, PR_GetOSError());
        } else {
            if (debug_mode) DPRINTF("\tServer sent %d bytes\n", bytesWritten);
        }	

        PR_Close(newSock);
        PR_AtomicDecrement(&workerThreadsBusy);
    }
}

PRFileDesc *
ServerSetup(void)
{
    PRFileDesc *listenSocket;
    PRSocketOptionData sockOpt;
    PRNetAddr serverAddr;
    PRThread *WorkerThread;

    if ((listenSocket = PR_NewTCPSocket()) == NULL) {
        if (debug_mode) printf("\tServer error creating listen socket\n");
		else Test_Result(FAIL);
        return NULL;
    }

    sockOpt.option = PR_SockOpt_Reuseaddr;
    sockOpt.value.reuse_addr = PR_TRUE;
    if (PR_SetSocketOption(listenSocket, &sockOpt) != PR_SUCCESS) {
        if (debug_mode) printf("\tServer error setting socket option: OS error %d\n",
                PR_GetOSError());
        else Test_Result(FAIL);
        PR_Close(listenSocket);
        return NULL;
    }

    memset(&serverAddr, 0, sizeof(PRNetAddr));
    serverAddr.inet.family = PR_AF_INET;
    serverAddr.inet.port = PR_htons(PORT);
    serverAddr.inet.ip = PR_htonl(PR_INADDR_ANY);

    if (PR_Bind(listenSocket, &serverAddr) != PR_SUCCESS) {
        if (debug_mode) printf("\tServer error binding to server address: OS error %d\n",
                PR_GetOSError());
		else Test_Result(FAIL);
        PR_Close(listenSocket);
        return NULL;
    }

    if (PR_Listen(listenSocket, 128) != PR_SUCCESS) {
        if (debug_mode) printf("\tServer error listening to server socket\n");
		else Test_Result(FAIL);
        PR_Close(listenSocket);

        return NULL;
    }

    /* Create Clients */
    workerThreads = 0;
    workerThreadsBusy = 0;

    workerThreadsLock = PR_NewLock();

    WorkerThread = PR_CreateThread(
                      PR_SYSTEM_THREAD,
                      WorkerThreadFunc,
                      listenSocket,
                      PR_PRIORITY_NORMAL,
                      ServerScope,
                      PR_UNJOINABLE_THREAD,
                      THREAD_STACKSIZE);

    if (!WorkerThread) {
        if (debug_mode) printf("error creating working thread\n");
        PR_Close(listenSocket);
        return NULL;
    }
    PR_AtomicIncrement(&workerThreads);
    if (debug_mode) DPRINTF("\tServer created primordial worker thread\n");

    return listenSocket;
}

/* The main server loop */
void
ServerThreadFunc(void *unused)
{
    PRFileDesc *listenSocket;

    /* Do setup */
    listenSocket = ServerSetup();

    if (!listenSocket) {
        SetServerState(SERVER, SERVER_STATE_DEAD);
    } else {

        if (debug_mode) DPRINTF("\tServer up\n");

        /* Tell clients they can start now. */
        SetServerState(SERVER, SERVER_STATE_READY);

        /* Now wait for server death signal */
        WaitServerState(SERVER, SERVER_STATE_DYING);

        /* Cleanup */
        SetServerState(SERVER, SERVER_STATE_DEAD);
    }
}

/* --- Client Functions ------------------------------------------- */

PRInt32 numRequests;
PRInt32 numClients;
PRMonitor *clientMonitor;

void
ClientThreadFunc(void *unused)
{
    PRNetAddr serverAddr;
    PRFileDesc *clientSocket;
    char *sendBuf;
    char *recvBuf;
    PRInt32 rv;
    PRInt32 bytesNeeded;

    sendBuf = (char *)PR_MALLOC(_client_data * sizeof(char));
    if (!sendBuf)
        if (debug_mode) printf("\tClient could not malloc space!?\n");
    recvBuf = (char *)PR_MALLOC(_server_data * sizeof(char));
    if (!recvBuf)
        if (debug_mode) printf("\tClient could not malloc space!?\n");

    memset(&serverAddr, 0, sizeof(PRNetAddr));
    serverAddr.inet.family = PR_AF_INET;
    serverAddr.inet.port = PR_htons(PORT);
    serverAddr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);

    while(numRequests > 0) {

        if ( (numRequests % 10) == 0 )
            if (debug_mode) printf(".");
        if (debug_mode) DPRINTF("\tClient starting request %d\n", numRequests);

        clientSocket = PR_NewTCPSocket();
        if (!clientSocket) {
            if (debug_mode) printf("Client error creating socket: OS error %d\n",
		    PR_GetOSError());
            continue;
        }

        if (debug_mode) DPRINTF("\tClient connecting\n");

        rv = PR_Connect(clientSocket, 
                        &serverAddr,
                        PR_INTERVAL_NO_TIMEOUT);
        if (!clientSocket) {
            if (debug_mode) printf("\tClient error connecting\n");
            continue;
        }

        if (debug_mode) DPRINTF("\tClient connected\n");

        rv = PR_Send(clientSocket, 
                     sendBuf, 
                     _client_data, 
                     0, 
                     PR_INTERVAL_NO_TIMEOUT);
        if (rv != _client_data) {
            if (debug_mode) printf("Client error sending data (%d)\n", rv);
            PR_Close(clientSocket);
            continue;
        }

        if (debug_mode) DPRINTF("\tClient sent %d bytes\n", rv);

        bytesNeeded = _server_data;
        while(bytesNeeded) {
            rv = PR_Recv(clientSocket, 
                         recvBuf, 
                         bytesNeeded, 
                         0, 
                         PR_INTERVAL_NO_TIMEOUT);
            if (rv <= 0) {
                if (debug_mode) printf("Client error receiving data (%d) (%d/%d)\n", 
                    rv, (_server_data - bytesNeeded), _server_data);
                break;
            }
            if (debug_mode) DPRINTF("\tClient received %d bytes; need %d more\n", rv, bytesNeeded - rv);
            bytesNeeded -= rv;
        }

        PR_Close(clientSocket);
 
        PR_AtomicDecrement(&numRequests);
    }

    PR_EnterMonitor(clientMonitor);
    --numClients;
    PR_Notify(clientMonitor);
    PR_ExitMonitor(clientMonitor);

    PR_DELETE(sendBuf);
    PR_DELETE(recvBuf);
}

void
RunClients(void)
{
    PRInt32 index;

    numRequests = _iterations;
    numClients = _clients;
    clientMonitor = PR_NewMonitor();

    for (index=0; index<_clients; index++) {
        PRThread *clientThread;

  
        clientThread = PR_CreateThread(
                          PR_USER_THREAD,
                          ClientThreadFunc,
                          NULL,
                          PR_PRIORITY_NORMAL,
                          ClientScope,
                          PR_UNJOINABLE_THREAD,
                          THREAD_STACKSIZE);

        if (!clientThread) {
            if (debug_mode) printf("\terror creating client thread %d\n", index);
        } else
            if (debug_mode) DPRINTF("\tMain created client %d/%d\n", index+1, _clients);

    }

    PR_EnterMonitor(clientMonitor);
    while(numClients)
        PR_Wait(clientMonitor, PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor(clientMonitor);
}

/* --- Main Function ---------------------------------------------- */

static
void do_work()
{
    PRThread *ServerThread;
    PRInt32 state;

    SetServerState(MAIN, SERVER_STATE_STARTUP);
    ServerThread = PR_CreateThread(
                      PR_USER_THREAD,
                      ServerThreadFunc,
                      NULL,
                      PR_PRIORITY_NORMAL,
                      ServerScope,
                      PR_JOINABLE_THREAD,
                      THREAD_STACKSIZE);
    if (!ServerThread) {
        if (debug_mode) printf("error creating main server thread\n");
        return;
    }

    /* Wait for server to be ready */
    state = WaitServerState(MAIN, SERVER_STATE_READY|SERVER_STATE_DEAD);

    if (!(state & SERVER_STATE_DEAD)) {
        /* Run Test Clients */
        RunClients();

        /* Send death signal to server */
        SetServerState(MAIN, SERVER_STATE_DYING);
    }

    PR_JoinThread(ServerThread);
}

static void do_workUU(void)
{
    ServerScope = PR_LOCAL_THREAD;
    ClientScope = PR_LOCAL_THREAD;
    do_work();
}

static void do_workUK(void)
{
    ServerScope = PR_LOCAL_THREAD;
    ClientScope = PR_GLOBAL_THREAD;
    do_work();
}

static void do_workKU(void)
{
    ServerScope = PR_GLOBAL_THREAD;
    ClientScope = PR_LOCAL_THREAD;
    do_work();
}

static void do_workKK(void)
{
    ServerScope = PR_GLOBAL_THREAD;
    ClientScope = PR_GLOBAL_THREAD;
    do_work();
}


static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);

    if (debug_mode) printf("\n%40s: %6.2f usec\n", msg, d / _iterations);
}


int main(int argc, char **argv)
{
	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name -d
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "d");
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
#ifndef SYMBIAN
    if (debug_mode) {
		printf("Enter number of iterations: \n");
		scanf("%d", &_iterations);
		printf("Enter number of clients   : \n");
		scanf("%d", &_clients);
		printf("Enter size of client data : \n");
		scanf("%d", &_client_data);
		printf("Enter size of server data : \n");
		scanf("%d", &_server_data);
	}
	else 
#endif
	{

		_iterations = 10;
	    _clients = 1;
		_client_data = 10;
		_server_data = 10;
	}
	
    if (debug_mode) {
		printf("\n\n%d iterations with %d client threads.\n", 
        _iterations, _clients);
		printf("Sending %d bytes of client data and %d bytes of server data\n", 
        _client_data, _server_data);
	}
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    ServerStateCVLock = PR_NewLock();
    ServerStateCV = PR_NewCondVar(ServerStateCVLock);

    Measure(do_workUU, "server loop user/user");
 #if 0 
    Measure(do_workUK, "server loop user/kernel");
    Measure(do_workKU, "server loop kernel/user");
    Measure(do_workKK, "server loop kernel/kernel");
 #endif 

    PR_Cleanup();

    return failed_already;
}
