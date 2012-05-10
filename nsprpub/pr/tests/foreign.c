/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        foreign.c
** Description: Testing various functions w/ foreign threads
**
**      We create a thread and get it to call exactly one runtime function.
**      The thread is allowed to be created by some other environment that
**      NSPR, but it does not announce itself to the runtime prior to calling
**      in.
**
**      The goal: try to survive.
**      
*/

#include "prcvar.h"
#include "prenv.h"
#include "prerror.h"
#include "prinit.h"
#include "prinrval.h"
#include "prio.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"
#include "prthread.h"
#include "prtypes.h"
#include "prprf.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>

static enum {
    thread_nspr, thread_pthread, thread_sproc, thread_win32
} thread_provider;

typedef void (*StartFn)(void*);
typedef struct StartObject
{
    StartFn start;
    void *arg;
} StartObject;

static PRFileDesc *output;

static int _debug_on = 0;

#define DEFAULT_THREAD_COUNT	10

#define DPRINTF(arg)	if (_debug_on) PR_fprintf arg

#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
#include <pthread.h>
#include "md/_pth.h"
static void *pthread_start(void *arg)
{
    StartFn start = ((StartObject*)arg)->start;
    void *data = ((StartObject*)arg)->arg;
    PR_Free(arg);
    start(data);
    return NULL;
}  /* pthread_start */
#endif /* defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS) */

#if defined(IRIX) && !defined(_PR_PTHREADS)
#include <sys/types.h>
#include <sys/prctl.h>
static void sproc_start(void *arg, PRSize size)
{
    StartObject *so = (StartObject*)arg;
    StartFn start = so->start;
    void *data = so->arg;
    PR_Free(so);
    start(data);
}  /* sproc_start */
#endif  /* defined(IRIX) && !defined(_PR_PTHREADS) */

#if defined(WIN32)
#if defined(WINCE)
#include <windows.h>
#endif
#include <process.h>  /* for _beginthreadex() */

static PRUintn __stdcall windows_start(void *arg)
{
    StartObject *so = (StartObject*)arg;
    StartFn start = so->start;
    void *data = so->arg;
    PR_Free(so);
    start(data);
    return 0;
}  /* windows_start */
#endif /* defined(WIN32) */

static PRStatus NSPRPUB_TESTS_CreateThread(StartFn start, void *arg)
{
    PRStatus rv;

    switch (thread_provider)
    {
    case thread_nspr:
        {
            PRThread *thread = PR_CreateThread(
                PR_USER_THREAD, start, arg,
                PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                PR_UNJOINABLE_THREAD, 0);
            rv = (NULL == thread) ? PR_FAILURE : PR_SUCCESS;
        }
        break;
    case thread_pthread:
#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
        {
            int rv;
            pthread_t id;
            pthread_attr_t tattr;
            StartObject *start_object;
            start_object = PR_NEW(StartObject);
            PR_ASSERT(NULL != start_object);
            start_object->start = start;
            start_object->arg = arg;

            rv = _PT_PTHREAD_ATTR_INIT(&tattr);
            PR_ASSERT(0 == rv);

            rv = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
            PR_ASSERT(0 == rv);

            rv = pthread_attr_setstacksize(&tattr, 64 * 1024);
            PR_ASSERT(0 == rv);

            rv = _PT_PTHREAD_CREATE(&id, tattr, pthread_start, start_object);
            (void)_PT_PTHREAD_ATTR_DESTROY(&tattr);
            return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
        }
#else
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        rv = PR_FAILURE;
        break;
#endif /* defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS) */

    case thread_sproc:
#if defined(IRIX) && !defined(_PR_PTHREADS)
        {
            PRInt32 pid;
            StartObject *start_object;
            start_object = PR_NEW(StartObject);
            PR_ASSERT(NULL != start_object);
            start_object->start = start;
            start_object->arg = arg;
            pid = sprocsp(
                sproc_start, PR_SALL, start_object, NULL, 64 * 1024);
            rv = (0 < pid) ? PR_SUCCESS : PR_FAILURE;
        }
#else
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        rv = PR_FAILURE;
#endif  /* defined(IRIX) && !defined(_PR_PTHREADS) */
        break;
    case thread_win32:
#if defined(WIN32)
        {
            void *th;
            PRUintn id;       
            StartObject *start_object;
            start_object = PR_NEW(StartObject);
            PR_ASSERT(NULL != start_object);
            start_object->start = start;
            start_object->arg = arg;
            th = (void*)_beginthreadex(
                NULL, /* LPSECURITY_ATTRIBUTES - pointer to thread security attributes */  
                0U, /* DWORD - initial thread stack size, in bytes */
                windows_start, /* LPTHREAD_START_ROUTINE - pointer to thread function */
                start_object, /* LPVOID - argument for new thread */
                0U, /*DWORD dwCreationFlags - creation flags */
                &id /* LPDWORD - pointer to returned thread identifier */ );

            rv = (NULL == th) ? PR_FAILURE : PR_SUCCESS;
        }
#else
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        rv = PR_FAILURE;
#endif
        break;
    default:
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        rv = PR_FAILURE;
    }
    return rv;
}  /* NSPRPUB_TESTS_CreateThread */

static void PR_CALLBACK lazyEntry(void *arg)
{
    PR_ASSERT(NULL == arg);
}  /* lazyEntry */


static void OneShot(void *arg)
{
    PRUintn pdkey;
    PRLock *lock;
    PRFileDesc *fd;
    PRDir *dir;
    PRFileDesc *pair[2];
    PRIntn test = (PRIntn)arg;

	for (test = 0; test < 12; ++test) {

    switch (test)
    {
        case 0:
            lock = PR_NewLock(); 
			DPRINTF((output,"Thread[0x%x] called PR_NewLock\n",
			PR_GetCurrentThread()));
            PR_DestroyLock(lock);
            break;
            
        case 1:
            (void)PR_SecondsToInterval(1);
			DPRINTF((output,"Thread[0x%x] called PR_SecondsToInterval\n",
			PR_GetCurrentThread()));
            break;
            
        case 2: (void)PR_CreateThread(
            PR_USER_THREAD, lazyEntry, NULL, PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0); 
			DPRINTF((output,"Thread[0x%x] called PR_CreateThread\n",
			PR_GetCurrentThread()));
            break;
            
        case 3:
            fd = PR_Open("foreign.tmp", PR_CREATE_FILE | PR_RDWR, 0666); 
			DPRINTF((output,"Thread[0x%x] called PR_Open\n",
			PR_GetCurrentThread()));
            PR_Close(fd);
            break;
            
        case 4:
            fd = PR_NewUDPSocket(); 
			DPRINTF((output,"Thread[0x%x] called PR_NewUDPSocket\n",
			PR_GetCurrentThread()));
            PR_Close(fd);
            break;
            
        case 5:
            fd = PR_NewTCPSocket(); 
			DPRINTF((output,"Thread[0x%x] called PR_NewTCPSocket\n",
			PR_GetCurrentThread()));
            PR_Close(fd);
            break;
            
        case 6:
#ifdef SYMBIAN
#define TEMP_DIR "c:\\data\\"
#else
#define TEMP_DIR "/tmp/"
#endif
            dir = PR_OpenDir(TEMP_DIR);
			DPRINTF((output,"Thread[0x%x] called PR_OpenDir\n",
			PR_GetCurrentThread()));
            PR_CloseDir(dir);
            break;
            
        case 7:
            (void)PR_NewThreadPrivateIndex(&pdkey, NULL);
			DPRINTF((output,"Thread[0x%x] called PR_NewThreadPrivateIndex\n",
			PR_GetCurrentThread()));
            break;
        
        case 8:
            (void)PR_GetEnv("PATH");
			DPRINTF((output,"Thread[0x%x] called PR_GetEnv\n",
			PR_GetCurrentThread()));
            break;
            
        case 9:
            (void)PR_NewTCPSocketPair(pair);
			DPRINTF((output,"Thread[0x%x] called PR_NewTCPSocketPair\n",
			PR_GetCurrentThread()));
            PR_Close(pair[0]);
            PR_Close(pair[1]);
            break;
            
        case 10:
            PR_SetConcurrency(2);
			DPRINTF((output,"Thread[0x%x] called PR_SetConcurrency\n",
			PR_GetCurrentThread()));
            break;

        case 11:
            PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_HIGH);
			DPRINTF((output,"Thread[0x%x] called PR_SetThreadPriority\n",
			PR_GetCurrentThread()));
            break;
            
        default: 
            break;
    } /* switch() */
	}
}  /* OneShot */

int main(int argc, char **argv)
{
    PRStatus rv;
	PRInt32	thread_cnt = DEFAULT_THREAD_COUNT;
	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dt:");

#if defined(WIN32)
	thread_provider = thread_win32;
#elif defined(_PR_PTHREADS)
	thread_provider = thread_pthread;
#elif defined(IRIX)
	thread_provider = thread_sproc;
#else
    thread_provider = thread_nspr;
#endif


	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
		if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
			_debug_on = 1;
            break;
        case 't':  /* thread count */
            thread_cnt = atoi(opt->value);
            break;
         default:
            break;
        }
    }
	PL_DestroyOptState(opt);

	PR_SetConcurrency(2);

	output = PR_GetSpecialFD(PR_StandardOutput);

    while (thread_cnt-- > 0)
    {
        rv = NSPRPUB_TESTS_CreateThread(OneShot, (void*)thread_cnt);
        PR_ASSERT(PR_SUCCESS == rv);
        PR_Sleep(PR_MillisecondsToInterval(5));
    }
    PR_Sleep(PR_SecondsToInterval(3));
    return (PR_SUCCESS == PR_Cleanup()) ? 0 : 1;
}  /* main */

/* foreign.c */
