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

/*
** Test socket IO timeouts
**
**
**
**
** Modification History:
** 14-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
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

#include <stdio.h>
#include "nspr.h"

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
#endif

#define NUM_THREADS 1
#define BASE_PORT   8000
#define DEFAULT_ACCEPT_TIMEOUT 2

typedef struct threadInfo {
    PRInt16 id;
    PRInt16 accept_timeout;
    PRLock *dead_lock;
    PRCondVar *dead_cv;
    PRInt32   *alive;
} threadInfo;

PRIntn failed_already = 0;
PRIntn debug_mode = 0;

#define	LOCAL_SCOPE_STRING			"LOCAL scope"
#define	GLOBAL_SCOPE_STRING			"GLOBAL scope"
#define	GLOBAL_BOUND_SCOPE_STRING	"GLOBAL_BOUND scope"

void 
thread_main(void *_info)
{
    threadInfo *info = (threadInfo *)_info;
    PRNetAddr listenAddr;
    PRNetAddr clientAddr;
    PRFileDesc *listenSock = NULL;
    PRFileDesc *clientSock;
    PRStatus rv;
	PRThreadScope tscope;
	char *scope_str;

 
	if (debug_mode)
    	printf("thread %d is alive\n", info->id);
	tscope = PR_GetThreadScope(PR_GetCurrentThread());

	switch(tscope) {
		case PR_LOCAL_THREAD:
			scope_str = LOCAL_SCOPE_STRING;
			break;
		case PR_GLOBAL_THREAD:
			scope_str = GLOBAL_SCOPE_STRING;
			break;
		case PR_GLOBAL_BOUND_THREAD:
			scope_str = GLOBAL_BOUND_SCOPE_STRING;
			break;
		default:
			PR_ASSERT(!"Invalid thread scope");
			break;
	}
	printf("thread id %d, scope %s\n", info->id, scope_str);

    listenSock = PR_NewTCPSocket();
    if (!listenSock) {
		if (debug_mode)
        	printf("unable to create listen socket\n");
		failed_already=1;
        goto dead;
    }
  
    listenAddr.inet.family = PR_AF_INET;
    listenAddr.inet.port = PR_htons(BASE_PORT + info->id);
    listenAddr.inet.ip = PR_htonl(PR_INADDR_ANY);
    rv = PR_Bind(listenSock, &listenAddr);
    if (rv == PR_FAILURE) {
		if (debug_mode)
        	printf("unable to bind\n");
		failed_already=1;
        goto dead;
    }

    rv = PR_Listen(listenSock, 4);
    if (rv == PR_FAILURE) {
		if (debug_mode)
        	printf("unable to listen\n");
		failed_already=1;
        goto dead;
    }

	if (debug_mode)
    	printf("thread %d going into accept for %d seconds\n", 
        	info->id, info->accept_timeout + info->id);

    clientSock = PR_Accept(listenSock, &clientAddr, PR_SecondsToInterval(info->accept_timeout +info->id));

    if (clientSock == NULL) {
        if (PR_GetError() == PR_IO_TIMEOUT_ERROR) {
			if (debug_mode) {	
            	printf("PR_Accept() timeout worked!\n"); 
				printf("TEST PASSED! PR_Accept() returned error %d\n",
							PR_IO_TIMEOUT_ERROR);
			}
    	} else {
			if (debug_mode)
            	printf("TEST FAILED! PR_Accept() returned error %d\n",
														PR_GetError());
			failed_already=1;
		}
    } else {
		if (debug_mode)
        	printf ("TEST FAILED! PR_Accept() succeeded?\n");
		failed_already=1;
		PR_Close(clientSock);
    }

dead:
    if (listenSock) {
		PR_Close(listenSock);
    }
    PR_Lock(info->dead_lock);
    (*info->alive)--;
    PR_NotifyCondVar(info->dead_cv);
    PR_Unlock(info->dead_lock);

	if (debug_mode)
    	printf("thread %d is dead\n", info->id);
}

void
thread_test(PRThreadScope scope, PRInt32 num_threads)
{
    PRInt32 index;
    PRThread *thr;
    PRLock *dead_lock;
    PRCondVar *dead_cv;
    PRInt32 alive;

	if (debug_mode)
    	printf("IO Timeout test started with %d threads\n", num_threads);

    dead_lock = PR_NewLock();
    dead_cv = PR_NewCondVar(dead_lock);
    alive = num_threads;
    
    for (index = 0; index < num_threads; index++) {
        threadInfo *info = (threadInfo *)PR_Malloc(sizeof(threadInfo));

        info->id = index;
        info->dead_lock = dead_lock;
        info->dead_cv = dead_cv;
        info->alive = &alive;
        info->accept_timeout = DEFAULT_ACCEPT_TIMEOUT;
        
        thr = PR_CreateThread( PR_USER_THREAD,
                               thread_main,
                               (void *)info,
                               PR_PRIORITY_NORMAL,
                               scope,
                               PR_UNJOINABLE_THREAD,
                               0);

        if (!thr) {
        	printf("Failed to create thread, error = %d(%d)\n",
											PR_GetError(), PR_GetOSError());
			failed_already=1;

            PR_Lock(dead_lock);
            alive--;
            PR_Unlock(dead_lock);
        }
    }

    PR_Lock(dead_lock);
    while(alive) {
		if (debug_mode)
        	printf("main loop awake; alive = %d\n", alive);
        PR_WaitCondVar(dead_cv, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(dead_lock);
}

int main(int argc, char **argv)
{
    PRInt32 num_threads = 0;

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name [-d] [-t <threads>]
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dt:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 't':  /* threads to involve */
			num_threads = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

 /* main test */
	
    if (0 == num_threads)
        num_threads = NUM_THREADS;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_LOW, 0);
    PR_STDIO_INIT();

#ifdef XP_MAC
	SetupMacPrintfLog("io_timeout.log");
	debug_mode = 1;
#endif

    printf("test with global bound thread\n");
    thread_test(PR_GLOBAL_BOUND_THREAD, num_threads);

    printf("test with local thread\n");
    thread_test(PR_LOCAL_THREAD, num_threads);

    printf("test with global thread\n");
    thread_test(PR_GLOBAL_THREAD, num_threads);

    PR_Cleanup();

	if (failed_already)
		return 1;
	else
    	return 0;
}
