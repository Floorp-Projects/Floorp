/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bruce Robson <bns_robson@hotmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

#if HAVE_LIBPTHREAD
pthread_cond_t threadCond;
pthread_mutex_t threadMutex;
#endif

#if HAVE_LIBTHREAD
mutex_t threadMutex;
#endif

#if WINDOWS
HANDLE threadCond = NULL;
HANDLE threadMutex = NULL;
#endif

void
threadCancel(Thread thr)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_cancel(thr);
	if (ret)
	{
		fprintf(stderr, "pthread_cancel %d\n", ret);
	}
#endif

#if HAVE_LIBTHREAD
	thr_kill(thr, SIGKILL);
#endif

#if WINDOWS
	BOOL	ret;

	ret = TerminateThread(thr, 0);
	if (!ret)
	{
		fprintf(stderr, "TerminateThread failed\n");
	}
#endif
}

void
threadCondBroadcast(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_cond_broadcast(&threadCond);
	if (ret)
	{
		fprintf(stderr, "pthread_cond_broadcast %d\n", ret);
	}
#endif

#if HAVE_LIBTHREAD
	thr_cond_broadcast();
#endif

#if WINDOWS
	BOOL	ret;

	ret = SetEvent(threadCond);
	if (!ret)
	{
		fprintf(stderr, "SetEvent failed\n");
	}
#endif
}

void
threadCondSignal(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_cond_signal(&threadCond);
	if (ret)
	{
		fprintf(stderr, "pthread_cond_signal %d\n", ret);
	}
#endif

#if HAVE_LIBTHREAD
	thr_cond_signal();
#endif

#if WINDOWS
	BOOL	ret;

	ret = SetEvent(threadCond);
	if (!ret)
	{
		fprintf(stderr, "SetEvent failed\n");
	}
#endif
}

void
threadCondWait(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_cond_wait(&threadCond, &threadMutex);
	if (ret)
	{
		fprintf(stderr, "pthread_cond_wait %d\n", ret);
	}
#endif

#if HAVE_LIBTHREAD
	thr_cond_wait();
#endif

#if WINDOWS
	BOOL	ret;
	DWORD	wait;

	ret = ReleaseMutex(threadMutex);
	if (!ret)
	{
		fprintf(stderr, "ReleaseMutex failed\n");
		return;
	}
	wait = WaitForSingleObject(threadCond, INFINITE);
	if (wait == WAIT_FAILED)
	{
		fprintf(stderr, "WaitForSingleObject(cond) failed\n");
	}
	wait = WaitForSingleObject(threadMutex, INFINITE);
	if (wait == WAIT_FAILED)
	{
		fprintf(stderr, "WaitForSingleObject(mutex) failed\n");
	}
#endif
}

int
threadCreate(Thread *thr, void *(*start)(void *), void *arg)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_create(thr, NULL, start, arg);
	if (ret)
	{
		fprintf(stderr, "pthread_create %d\n", ret);
		exit(0);
		return 1;
	}
#endif

#if HAVE_LIBTHREAD
	int	ret;

	ret = thr_create(NULL, 0, start, arg, 0, thr);
	if (ret)
	{
		fprintf(stderr, "thr_create %d\n", ret);
		exit(0);
		return 1;
	}
#endif

#if WINDOWS
	*thr = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) start, arg, 0,
		NULL);
	if (!*thr)
	{
		fprintf(stderr, "CreateThread failed\n");
		exit(0);
		return 1;
	}
#endif

	return 0;
}

int
threadInit(void)
{
#if HAVE_LIBPTHREAD
	int			ret;
	pthread_mutexattr_t	attr;

	ret = pthread_cond_init(&threadCond, NULL);
	if (ret)
	{
		fprintf(stderr, "pthread_cond_init %d\n", ret);
		return 0;
	}
	pthread_mutexattr_init(&attr);
	ret = pthread_mutex_init(&threadMutex, &attr);
	if (ret)
	{
		fprintf(stderr, "pthread_mutex_init %d\n", ret);
		return 0;
	}
#endif

#if HAVE_LIBTHREAD
	int	ret;

	cond_init();
	ret = mutex_init(&threadMutex, USYNC_THREAD, NULL);
	if (ret)
	{
		fprintf(stderr, "mutex_init failed\n");
		return 0;
	}
#endif

#if WINDOWS
	threadCond = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!threadCond)
	{
		fprintf(stderr, "CreateEvent failed\n");
		return 0;
	}
        threadMutex = CreateMutex(NULL, FALSE, NULL);
	if (!threadMutex)
	{
		fprintf(stderr, "CreateMutex failed\n");
		return 0;
	}
#endif

	return 1;
}

void
threadJoin(Thread thr)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_join(thr, NULL);
	if (ret)
	{
		fprintf(stderr, "pthread_join %d\n", ret);
	}
#endif

#if HAVE_LIBTHREAD
	int	ret;

	ret = thr_join();
	if (ret)
	{
		fprintf(stderr, "thr_join failed\n");
	}
#endif

#if WINDOWS
	DWORD	wait;

	wait = WaitForSingleObject(thr, INFINITE);
	if (wait == WAIT_FAILED)
	{
		fprintf(stderr, "WaitForSingleObject(thr) failed\n");
	}
#endif
}

void
threadMutexLock(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_mutex_lock(&threadMutex);
	if (ret)
	{
		fprintf(stderr, "pthread_mutex_lock failed\n");
	}
#endif

#if HAVE_LIBTHREAD
	mutex_lock(&threadMutex);
#endif

#if WINDOWS
	DWORD	wait;

	wait = WaitForSingleObject(threadMutex, INFINITE);
	if (wait == WAIT_FAILED)
	{
		fprintf(stderr, "WaitForSingleObject(mutex) failed\n");
	}
#endif
}

void
threadMutexUnlock(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = pthread_mutex_unlock(&threadMutex);
	if (ret)
	{
		fprintf(stderr, "pthread_mutex_lock failed\n");
	}
#endif

#if HAVE_LIBTHREAD
	mutex_unlock(&threadMutex);
#endif

#if WINDOWS
	BOOL	ret;

	ret = ReleaseMutex(threadMutex);
	if (!ret)
	{
		fprintf(stderr, "ReleaseMutex failed\n");
	}
#endif
}

void
threadYield(void)
{
#if HAVE_LIBPTHREAD
	int	ret;

	ret = sched_yield();
	if (ret)
	{
		fprintf(stderr, "sched_yield failed\n");
	}
#endif

#if HAVE_LIBTHREAD
	thr_yield();
#endif

#if WINDOWS
	(void) SwitchToThread();
#endif
}
