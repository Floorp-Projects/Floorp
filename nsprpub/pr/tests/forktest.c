/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/***********************************************************************
**
** Name: forktest.c
**
** Description: UNIX test for fork functions.
**
** Modification History:
** 15-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**                 The debug mode will print all of the printfs associated with this test.
**                         The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**                         have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**                        recognize the return code from tha main program.
** 12-June-97 AGarcic - Revert to return code 0 and 1, remove debug option (obsolete).
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "nspr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

PRIntn failed_already=0;

#ifdef XP_UNIX

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

static char *message = "Hello world!";

static void
ClientThreadFunc(void *arg)
{
    PRNetAddr addr;
    PRFileDesc *sock = NULL;
    PRInt32 tmp = (PRInt32)arg;

    /*
     * Make sure the PR_Accept call will block
     */

    printf("Wait one second before connect\n");
    fflush(stdout);
    PR_Sleep(PR_SecondsToInterval(1));

    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = 0;
    if ((sock = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr, "failed to create TCP socket: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    if (PR_Bind(sock, &addr) != PR_SUCCESS) {
        fprintf(stderr, "PR_Bind failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    addr.inet.ip = PR_htonl(INADDR_LOOPBACK);
    addr.inet.port = PR_htons((PRInt16)tmp);
    printf("Connecting to port %hu\n", PR_ntohs(addr.inet.port));
    fflush(stdout);
    if (PR_Connect(sock, &addr, PR_SecondsToInterval(5)) !=
        PR_SUCCESS) {
        fprintf(stderr, "PR_Connect failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    printf("Writing message \"%s\"\n", message);
    fflush(stdout);
    if (PR_Send(sock, message, strlen(message) + 1, 0, PR_INTERVAL_NO_TIMEOUT) ==
        -1) {
        fprintf(stderr, "PR_Send failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
finish:
    if (sock) {
        PR_Close(sock);
    }
    return;
}

/*
 * DoIO --
 *     This function creates a thread that acts as a client and itself.
 *     acts as a server.  Then it joins the client thread.
 */
static void
DoIO(void)
{
    PRThread *clientThread;
    PRFileDesc *listenSock = NULL;
    PRFileDesc *sock = NULL;
    PRNetAddr addr;
    PRInt32 nBytes;
    char buf[128];

    listenSock = PR_NewTCPSocket();
    if (!listenSock) {
        fprintf(stderr, "failed to create a TCP socket: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    addr.inet.family = AF_INET;
    addr.inet.ip = PR_htonl(INADDR_ANY);
    addr.inet.port = 0;
    if (PR_Bind(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "failed to bind socket: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    if (PR_GetSockName(listenSock, &addr) == PR_FAILURE) {
        fprintf(stderr, "failed to get socket port number: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
        fprintf(stderr, "PR_Listen failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    clientThread = PR_CreateThread( PR_USER_THREAD, ClientThreadFunc,
        (void *) PR_ntohs(addr.inet.port), PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
        PR_JOINABLE_THREAD, 0);
    if (clientThread == NULL) {
        fprintf(stderr, "Cannot create client thread: (%d, %d)\n",
            PR_GetError(), PR_GetOSError());
        failed_already = 1;
        goto finish;
    }
    printf("Accepting connection at port %hu\n", PR_ntohs(addr.inet.port));
    fflush(stdout);
    sock = PR_Accept(listenSock, &addr, PR_SecondsToInterval(5));
    if (!sock) {
        fprintf(stderr, "PR_Accept failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }
    nBytes = PR_Recv(sock, buf, sizeof(buf), 0, PR_INTERVAL_NO_TIMEOUT);
    if (nBytes == -1) {
        fprintf(stderr, "PR_Recv failed: error code %d\n",
            PR_GetError());
        failed_already = 1;
        goto finish;
    }

    /*
     * Make sure it has proper null byte to mark end of string 
     */

    buf[sizeof(buf) - 1] = '\0';
    printf("Received \"%s\" from the client\n", buf);
    fflush(stdout);
    if (!strcmp(buf, message)) {
        PR_JoinThread(clientThread);

        printf("The message is received correctly\n");
        fflush(stdout);
    } else {
        fprintf(stderr, "The message should be \"%s\"\n",
            message);
        failed_already = 1;
    }

finish:
    if (listenSock) {
        PR_Close(listenSock);
    }
    if (sock) {
        PR_Close(sock);
    }
    return;
}

#ifdef _PR_DCETHREADS

#include <syscall.h>

pid_t PR_UnixFork1(void)
{
    pid_t parent = getpid();
    int rv = syscall(SYS_fork);

    if (rv == -1) {
        return (pid_t) -1;
    } else {
        /* For each process, rv is the pid of the other process */
        if (rv == parent) {
            /* the child */
            return 0;
        } else {
            /* the parent */
            return rv;
        }
    }
}

#elif defined(SOLARIS)

/*
 * It seems like that in Solaris 2.4 one must call fork1() if the
 * the child process is going to use thread functions.  Solaris 2.5
 * doesn't have this problem. Calling fork() also works. 
 */

pid_t PR_UnixFork1(void)
{
    return fork1();
}

#else

pid_t PR_UnixFork1(void)
{
    return fork();
}

#endif  /* PR_DCETHREADS */

int main(
int     argc,
char   *argv[]
)
{
    pid_t pid;
	int rv;

    /* main test program */

    DoIO();

    pid = PR_UnixFork1();

    if (pid  == (pid_t) -1) {
        fprintf(stderr, "Fork failed: errno %d\n", errno);
        failed_already=1;
        return 1;
    } else if (pid > 0) {
        int childStatus;

        printf("Fork succeeded.  Parent process continues.\n");
        DoIO();
        if ((rv = waitpid(pid, &childStatus, 0)) != pid) {
#if defined(IRIX) && !defined(_PR_PTHREADS)
			/*
			 * nspr may handle SIGCLD signal
			 */
			if ((rv < 0) && (errno == ECHILD)) {
			} else 
#endif
			{
				fprintf(stderr, "waitpid failed: %d\n", errno);
				failed_already = 1;
			}
        } else if (!WIFEXITED(childStatus)
                || WEXITSTATUS(childStatus) != 0) {
            failed_already = 1;
        }
        printf("Parent process exits.\n");
        if (!failed_already) {
            printf("PASSED\n");
        } else {
            printf("FAILED\n");
        }
        return failed_already;
    } else {
#if defined(IRIX) && !defined(_PR_PTHREADS)
		extern void _PR_IRIX_CHILD_PROCESS(void);
		_PR_IRIX_CHILD_PROCESS();
#endif
        printf("Fork succeeded.  Child process continues.\n");
        DoIO();
        printf("Child process exits.\n");
        return failed_already;
    }
}

#else  /* XP_UNIX */

int main(    int     argc,
char   *argv[]
)
{

    printf("The fork test is applicable to Unix only.\n");
    return 0;

}

#endif  /* XP_UNIX */
