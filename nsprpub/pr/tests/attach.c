/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**  1996 - Netscape Communications Corporation
**
** Name: attach.c
**
** Description: Platform-specific code to create a native thread. The native thread will
**                            repeatedly call PR_AttachThread and PR_DetachThread. The
**                            primordial thread waits for this new thread to finish.
**
** Modification History:
** 13-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement.
** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
**			recognize the return code from tha main program.
** 12-June-97 Revert to return code 0 and 1.
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/

/* Used to get the command line option */
#include "nspr.h"
#include "pprthred.h"
#include "plgetopt.h"

#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#include <process.h>
#elif defined(_PR_PTHREADS)
#include <pthread.h>
#include "md/_pth.h"
#elif defined(IRIX)
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>
#elif defined(SOLARIS)
#include <thread.h>
#elif defined(OS2)
#define INCL_DOS
#define INCL_ERRORS
#include <os2.h>
#include <process.h>
#elif defined(XP_BEOS)
#include <kernel/OS.h>
#endif

#define DEFAULT_COUNT 1000
PRIntn failed_already=0;
PRIntn debug_mode;


int count;


static void 
AttachDetach(void)
{
    PRThread *me;
    PRInt32 index;

    for (index=0;index<count; index++) {
        me = PR_AttachThread(PR_USER_THREAD, 
                             PR_PRIORITY_NORMAL,
                             NULL);
 
        if (!me) {
            fprintf(stderr, "Error attaching thread %d: PR_AttachThread failed\n",
		    count);
	    	failed_already = 1;
	    	return;
        }
        PR_DetachThread();
    }
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
    printf("%40s: %6.2f usec\n", msg, d / count);
}

#ifdef WIN32
static unsigned __stdcall threadStartFunc(void *arg)
#elif defined(IRIX) && !defined(_PR_PTHREADS)
static void threadStartFunc(void *arg)
#elif defined(XP_BEOS)
static int32 threadStartFunc(void *arg)
#else
static void * threadStartFunc(void *arg)
#endif
{
    Measure(AttachDetach, "Attach/Detach");

#ifndef IRIX
    return 0;
#endif
}

int main(int argc, char **argv)
{
#ifdef _PR_PTHREADS
    int rv;
    pthread_t threadID;
    pthread_attr_t attr;
#elif defined(SOLARIS)
    int rv;
    thread_t threadID;
#elif defined(WIN32)
    DWORD rv;
    unsigned threadID;
    HANDLE hThread;
#elif defined(IRIX)
    int rv;
    int threadID;
#elif defined(OS2)
    int rv;
    TID threadID;
#elif defined(XP_BEOS)
	thread_id threadID;
	int32 threadRV;
	status_t waitRV;
#endif

	/* The command line argument: -d is used to determine if the test is being run
	in debug mode. The regress tool requires only one line output:PASS or FAIL.
	All of the printfs associated with this test has been handled with a if (debug_mode)
	test.
	Usage: test_name [-d] [-c n]
	*/
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dc:");
	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			debug_mode = 1;
            break;
        case 'c':  /* loop count */
			count = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

#if defined(WIN16)
    printf("attach: This test is not valid for Win16\n");
    goto exit_now;
#endif

	if(0 == count) count = DEFAULT_COUNT;	

    /*
     * To force the implicit initialization of nspr20
     */
    PR_SetError(0, 0);
    PR_STDIO_INIT();

    /*
     * Platform-specific code to create a native thread.  The native
     * thread will repeatedly call PR_AttachThread and PR_DetachThread.
     * The primordial thread waits for this new thread to finish.
     */

#ifdef _PR_PTHREADS

    rv = _PT_PTHREAD_ATTR_INIT(&attr);
    if (debug_mode) PR_ASSERT(0 == rv);
	else if (0 != rv) {
		failed_already=1;
		goto exit_now;
	}
	
    rv = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (debug_mode) PR_ASSERT(0 == rv);
	else if (0 != rv) {
		failed_already=1;
		goto exit_now;
	}
    rv = _PT_PTHREAD_CREATE(&threadID, attr, threadStartFunc, NULL);
    if (rv != 0) {
			fprintf(stderr, "thread creation failed: error code %d\n", rv);
			failed_already=1;
			goto exit_now;
	}
	else {
		if (debug_mode)
			printf ("thread creation succeeded \n");

	}
    rv = _PT_PTHREAD_ATTR_DESTROY(&attr);
    if (debug_mode) PR_ASSERT(0 == rv);
	else if (0 != rv) {
		failed_already=1;
		goto exit_now;
	}
    rv = pthread_join(threadID, NULL);
    if (debug_mode) PR_ASSERT(0 == rv);
	else if (0 != rv) {
		failed_already=1;
		goto exit_now;
	}

#elif defined(SOLARIS)

    rv = thr_create(NULL, 0, threadStartFunc, NULL, 0, &threadID);
    if (rv != 0) {
	if(!debug_mode) {
		failed_already=1;
		goto exit_now;
	} else	
		fprintf(stderr, "thread creation failed: error code %d\n", rv);
    }
    rv = thr_join(threadID, NULL, NULL);
    if (debug_mode) PR_ASSERT(0 == rv);
	else if (0 != rv)
	{
		failed_already=1;
		goto exit_now;
	}


#elif defined(WIN32)

    hThread = (HANDLE) _beginthreadex(NULL, 0, threadStartFunc, NULL,
            STACK_SIZE_PARAM_IS_A_RESERVATION, &threadID);
    if (hThread == 0) {
        fprintf(stderr, "thread creation failed: error code %d\n",
                GetLastError());
		failed_already=1;
		goto exit_now;
    }
    rv = WaitForSingleObject(hThread, INFINITE);
    if (debug_mode)PR_ASSERT(rv != WAIT_FAILED);
	else if (rv == WAIT_FAILED) {
		failed_already=1;
		goto exit_now;
	}

#elif defined(IRIX)

    threadID = sproc(threadStartFunc, PR_SALL, NULL);
    if (threadID == -1) {

			fprintf(stderr, "thread creation failed: error code %d\n",
					errno);
			failed_already=1;
			goto exit_now;
	
	}
	else {
		if (debug_mode) 
			printf ("thread creation succeeded \n");
		sleep(3);
		goto exit_now;
	}
    rv = waitpid(threadID, NULL, 0);
    if (debug_mode) PR_ASSERT(rv != -1);
	else  if (rv != -1) {
		failed_already=1;
		goto exit_now;
	}

#elif defined(OS2)

    threadID = (TID) _beginthread((void *)threadStartFunc, NULL,
            32768, NULL); 
    if (threadID == -1) {
        fprintf(stderr, "thread creation failed: error code %d\n", errno);
        failed_already=1;
        goto exit_now;
    }
    rv = DosWaitThread(&threadID, DCWW_WAIT);
    if (debug_mode) {
        PR_ASSERT(rv == NO_ERROR);
    } else if (rv != NO_ERROR) {
        failed_already=1;
        goto exit_now;
    }

#elif defined(XP_BEOS)
	
	threadID = spawn_thread(threadStartFunc, NULL, B_NORMAL_PRIORITY, NULL);
	if (threadID <= B_ERROR) {
		fprintf(stderr, "thread creation failed: error code %08lx\n", threadID);
		failed_already = 1;
		goto exit_now;
	}
	if (resume_thread(threadID) != B_OK) {
		fprintf(stderr, "failed starting thread: error code %08lx\n", threadID);
		failed_already = 1;
		goto exit_now;
	}

	waitRV = wait_for_thread(threadID, &threadRV);
	if (debug_mode)
		PR_ASSERT(waitRV == B_OK);
	else if (waitRV != B_OK) {
		failed_already = 1;
		goto exit_now;
	}
	
#else
	if (!debug_mode)
		failed_already=1;
	else	
		printf("The attach test does not apply to this platform because\n"
	    "either this platform does not have native threads or the\n"
	    "test needs to be written for this platform.\n");
	goto exit_now;
#endif

exit_now:
   if(failed_already)	
		return 1;
	else
		return 0;
}
