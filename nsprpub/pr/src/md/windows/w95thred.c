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

#include "primpl.h"
#include <process.h>  /* for _beginthreadex() */

extern void _PR_Win32InitTimeZone(void);  /* defined in ntmisc.c */

/* --- globals ------------------------------------------------ */
#ifdef _PR_USE_STATIC_TLS
__declspec(thread) struct PRThread  *_pr_thread_last_run;
__declspec(thread) struct PRThread  *_pr_currentThread;
__declspec(thread) struct _PRCPU    *_pr_currentCPU;
#else
DWORD _pr_currentThreadIndex;
DWORD _pr_lastThreadIndex;
DWORD _pr_currentCPUIndex;
#endif
int                           _pr_intsOff = 0; 
_PRInterruptTable             _pr_interruptTable[] = { { 0 } };

void
_PR_MD_EARLY_INIT()
{
    _PR_Win32InitTimeZone();

#ifndef _PR_USE_STATIC_TLS
    _pr_currentThreadIndex = TlsAlloc();
    _pr_lastThreadIndex = TlsAlloc();
    _pr_currentCPUIndex = TlsAlloc();
#endif
}

void _PR_MD_CLEANUP_BEFORE_EXIT(void)
{
    WSACleanup();

#ifndef _PR_USE_STATIC_TLS
    TlsFree(_pr_currentThreadIndex);
    TlsFree(_pr_lastThreadIndex);
    TlsFree(_pr_currentCPUIndex);
#endif
}

PRStatus
_PR_MD_INIT_THREAD(PRThread *thread)
{
    if (thread->flags & (_PR_PRIMORDIAL | _PR_ATTACHED)) {
        /*
        ** Warning:
        ** --------
        ** NSPR requires a real handle to every thread.
        ** GetCurrentThread() returns a pseudo-handle which
        ** is not suitable for some thread operations (e.g.,
        ** suspending).  Therefore, get a real handle from
        ** the pseudo handle via DuplicateHandle(...)
        */
        DuplicateHandle(
                GetCurrentProcess(),     /* Process of source handle */
                GetCurrentThread(),      /* Pseudo Handle to dup */
                GetCurrentProcess(),     /* Process of handle */
                &(thread->md.handle),    /* resulting handle */
                0L,                      /* access flags */
                FALSE,                   /* Inheritable */
                DUPLICATE_SAME_ACCESS);  /* Options */
    }

    /* Create the blocking IO semaphore */
    thread->md.blocked_sema = CreateSemaphore(NULL, 0, 1, NULL);
    if (thread->md.blocked_sema == NULL)
        return PR_FAILURE;
	else
		return PR_SUCCESS;
}

PRStatus 
_PR_MD_CREATE_THREAD(PRThread *thread, 
                  void (*start)(void *), 
                  PRThreadPriority priority, 
                  PRThreadScope scope, 
                  PRThreadState state, 
                  PRUint32 stackSize)
{

    thread->md.handle = (HANDLE) _beginthreadex(
                    NULL,
                    thread->stack->stackSize,
                    (unsigned (__stdcall *)(void *))start,
                    (void *)thread,
                    CREATE_SUSPENDED,
                    &(thread->id));
    if(!thread->md.handle) {
        return PR_FAILURE;
    }

    thread->md.id = thread->id;
    _PR_MD_SET_PRIORITY(&(thread->md), priority);

    /* Activate the thread */
    if ( ResumeThread( thread->md.handle ) != -1)
        return PR_SUCCESS;

    return PR_FAILURE;
}

void    
_PR_MD_YIELD(void)
{
    /* Can NT really yield at all? */
    Sleep(0);
}

void     
_PR_MD_SET_PRIORITY(_MDThread *thread, PRThreadPriority newPri)
{
    int nativePri;
    BOOL rv;

    if (newPri < PR_PRIORITY_FIRST) {
        newPri = PR_PRIORITY_FIRST;
    } else if (newPri > PR_PRIORITY_LAST) {
        newPri = PR_PRIORITY_LAST;
    }
    switch (newPri) {
        case PR_PRIORITY_LOW:
            nativePri = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case PR_PRIORITY_NORMAL:
            nativePri = THREAD_PRIORITY_NORMAL;
            break;
        case PR_PRIORITY_HIGH:
            nativePri = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case PR_PRIORITY_URGENT:
            nativePri = THREAD_PRIORITY_HIGHEST;
    }
    rv = SetThreadPriority(thread->handle, nativePri);
    PR_ASSERT(rv);
    if (!rv) {
	PR_LOG(_pr_thread_lm, PR_LOG_MIN,
                ("PR_SetThreadPriority: can't set thread priority\n"));
    }
    return;
}

void
_PR_MD_CLEAN_THREAD(PRThread *thread)
{
    BOOL rv;

    if (thread->md.blocked_sema) {
        rv = CloseHandle(thread->md.blocked_sema);
        PR_ASSERT(rv);
        thread->md.blocked_sema = 0;
    }

    if (thread->md.handle) {
        rv = CloseHandle(thread->md.handle);
        PR_ASSERT(rv);
        thread->md.handle = 0;
    }
}

void
_PR_MD_EXIT_THREAD(PRThread *thread)
{
    _PR_MD_CLEAN_THREAD(thread);
    _PR_MD_SET_CURRENT_THREAD(NULL);
}


void
_PR_MD_EXIT(PRIntn status)
{
    _exit(status);
}

PRInt32 _PR_MD_SETTHREADAFFINITYMASK(PRThread *thread, PRUint32 mask )
{
    int rv;

    rv = SetThreadAffinityMask(thread->md.handle, mask);

    return rv?0:-1;
}

PRInt32 _PR_MD_GETTHREADAFFINITYMASK(PRThread *thread, PRUint32 *mask)
{
    PRInt32 rv, system_mask;

    rv = GetProcessAffinityMask(GetCurrentProcess(), mask, &system_mask);
    
    return rv?0:-1;
}

void 
_PR_MD_SUSPEND_CPU(_PRCPU *cpu) 
{
    _PR_MD_SUSPEND_THREAD(cpu->thread);
}

void
_PR_MD_RESUME_CPU(_PRCPU *cpu)
{
    _PR_MD_RESUME_THREAD(cpu->thread);
}

void
_PR_MD_SUSPEND_THREAD(PRThread *thread)
{
    if (_PR_IS_NATIVE_THREAD(thread)) {
        DWORD previousSuspendCount;
        /* XXXMB - SuspendThread() is not a blocking call; how do we
         * know when the thread is *REALLY* suspended?
         */
        previousSuspendCount = SuspendThread(thread->md.handle);
        PR_ASSERT(previousSuspendCount == 0);
    }
}

void
_PR_MD_RESUME_THREAD(PRThread *thread)
{
    if (_PR_IS_NATIVE_THREAD(thread)) {
        DWORD previousSuspendCount;
        previousSuspendCount = ResumeThread(thread->md.handle);
        PR_ASSERT(previousSuspendCount == 1);
    }
}

PRThread*
_MD_CURRENT_THREAD(void)
{
PRThread *thread;

	thread = _MD_GET_ATTACHED_THREAD();

   	if (NULL == thread) {
		thread = _PRI_AttachThread(
            PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL, 0);
	}
	PR_ASSERT(thread != NULL);
	return thread;
}
