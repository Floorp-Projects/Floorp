/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <process.h>  /* for _beginthreadex() */

/* --- globals ------------------------------------------------ */
PRLock                       *_pr_schedLock = NULL;
_PRInterruptTable             _pr_interruptTable[] = { { 0 } };

BOOL _pr_use_static_tls = TRUE;
__declspec(thread) PRThread  *_pr_current_fiber;
__declspec(thread) PRThread  *_pr_fiber_last_run;
__declspec(thread) _PRCPU    *_pr_current_cpu;
__declspec(thread) PRUintn    _pr_ints_off;
DWORD _pr_currentFiberIndex;
DWORD _pr_lastFiberIndex;
DWORD _pr_currentCPUIndex;
DWORD _pr_intsOffIndex;

_MDLock                       _nt_idleLock;
PRCList                       _nt_idleList;
PRUint32                        _nt_idleCount;

extern __declspec(thread) PRThread *_pr_io_restarted_io;
extern DWORD _pr_io_restartedIOIndex;

/* Must check the restarted_io *before* decrementing no_sched to 0 */
#define POST_SWITCH_WORK() \
    PR_BEGIN_MACRO \
        PRThread *restarted_io = \
            (_pr_use_static_tls ? _pr_io_restarted_io \
            : (PRThread *) TlsGetValue(_pr_io_restartedIOIndex)); \
        if (restarted_io) { \
            _nt_handle_restarted_io(restarted_io); \
        } \
        _PR_MD_LAST_THREAD()->no_sched = 0; \
    PR_END_MACRO

void
_nt_handle_restarted_io(PRThread *restarted_io)
{
    /* After the switch we can resume an IO if needed.
     * XXXMB - this needs to be done in create thread, since that could
     * be the result for a context switch too..
     */
    PR_ASSERT(restarted_io->io_suspended == PR_TRUE);
    PR_ASSERT(restarted_io->md.thr_bound_cpu == restarted_io->cpu);

    _PR_THREAD_LOCK(restarted_io);
    if (restarted_io->io_pending == PR_FALSE) {

        /* The IO already completed, put us back on the runq. */
        int pri = restarted_io->priority;

        restarted_io->state = _PR_RUNNABLE;
        _PR_RUNQ_LOCK(restarted_io->cpu);
        _PR_ADD_RUNQ(restarted_io, restarted_io->cpu, pri);
        _PR_RUNQ_UNLOCK(restarted_io->cpu);
    } else {
        _PR_SLEEPQ_LOCK(restarted_io->cpu);
        _PR_ADD_SLEEPQ(restarted_io, restarted_io->sleep);
        _PR_SLEEPQ_UNLOCK(restarted_io->cpu);
    }
    restarted_io->io_suspended = PR_FALSE;
    restarted_io->md.thr_bound_cpu = NULL;

    _PR_THREAD_UNLOCK(restarted_io);

    if (_pr_use_static_tls) {
        _pr_io_restarted_io = NULL;
    } else {
        TlsSetValue(_pr_io_restartedIOIndex, NULL);
    }
}

void
_PR_MD_EARLY_INIT()
{
    _MD_NEW_LOCK( &_nt_idleLock );
    _nt_idleCount = 0;
    PR_INIT_CLIST(&_nt_idleList);

#if 0
    /* Make the clock tick at least once per millisecond */
    if ( timeBeginPeriod(1) == TIMERR_NOCANDO) {
        /* deep yoghurt; clock doesn't tick fast enough! */
        PR_ASSERT(0);
    }
#endif

    if (!_pr_use_static_tls) {
        _pr_currentFiberIndex = TlsAlloc();
        _pr_lastFiberIndex = TlsAlloc();
        _pr_currentCPUIndex = TlsAlloc();
        _pr_intsOffIndex = TlsAlloc();
        _pr_io_restartedIOIndex = TlsAlloc();
    }
}

void _PR_MD_CLEANUP_BEFORE_EXIT(void)
{
    _PR_NT_FreeSids();

    WSACleanup();

    if (!_pr_use_static_tls) {
        TlsFree(_pr_currentFiberIndex);
        TlsFree(_pr_lastFiberIndex);
        TlsFree(_pr_currentCPUIndex);
        TlsFree(_pr_intsOffIndex);
        TlsFree(_pr_io_restartedIOIndex);
    }
}

PRStatus
_PR_MD_INIT_THREAD(PRThread *thread)
{
    thread->md.overlapped.ioModel = _MD_BlockingIO;
    thread->md.overlapped.data.mdThread = &thread->md;

    if (thread->flags & _PR_GLOBAL_SCOPE) {
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
        if (thread->md.blocked_sema == NULL) {
            return PR_FAILURE;
        }
		if (_native_threads_only) {
			/* Create the blocking IO semaphore */
			thread->md.thr_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (thread->md.thr_event == NULL) {
				return PR_FAILURE;
			}
		}
    }

    return PR_SUCCESS;
}

static unsigned __stdcall
pr_root(void *arg)
{
    PRThread *thread = (PRThread *)arg;
    thread->md.start(thread);
    return 0;
}

PRStatus 
_PR_MD_CREATE_THREAD(PRThread *thread, 
                  void (*start)(void *), 
                  PRThreadPriority priority, 
                  PRThreadScope scope, 
                  PRThreadState state, 
                  PRUint32 stackSize)
{

    thread->md.start = start;
    thread->md.handle = (HANDLE) _beginthreadex(
                    NULL,
                    thread->stack->stackSize,
                    pr_root,
                    (void *)thread,
                    CREATE_SUSPENDED,
                    &(thread->id));
    if(!thread->md.handle) {
        PRErrorCode prerror;
        thread->md.fiber_last_error = GetLastError();
        switch (errno) {
            case ENOMEM:
                prerror = PR_OUT_OF_MEMORY_ERROR;
                break;
            case EAGAIN:
                prerror = PR_INSUFFICIENT_RESOURCES_ERROR;
                break;
            case EINVAL:
                prerror = PR_INVALID_ARGUMENT_ERROR;
                break;
            default:
                prerror = PR_UNKNOWN_ERROR;
        }
        PR_SetError(prerror, errno);
        return PR_FAILURE;
    }

    thread->md.id = thread->id;
    /*
     * On windows, a thread is created with a thread priority of
     * THREAD_PRIORITY_NORMAL.
     */
    if (priority != PR_PRIORITY_NORMAL) {
        _PR_MD_SET_PRIORITY(&(thread->md), priority);
    }

    /* Activate the thread */
    if ( ResumeThread( thread->md.handle ) != -1)
        return PR_SUCCESS;

    PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
    return PR_FAILURE;
}

void
_PR_MD_JOIN_THREAD(_MDThread *md)
{
    DWORD rv;

    rv = WaitForSingleObject(md->handle, INFINITE);
    PR_ASSERT(WAIT_OBJECT_0 == rv);
}

void
_PR_MD_END_THREAD(void)
{
    _endthreadex(0);
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

const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void
_PR_MD_SET_CURRENT_THREAD_NAME(const char *name)
{
#ifdef _MSC_VER
   THREADNAME_INFO info;

   if (!IsDebuggerPresent())
      return;

   info.dwType = 0x1000;
   info.szName = (char*) name;
   info.dwThreadID = -1;
   info.dwFlags = 0;

   __try {
      RaiseException(MS_VC_EXCEPTION,
                     0,
                     sizeof(info) / sizeof(ULONG_PTR),
                     (ULONG_PTR*)&info);
   } __except(EXCEPTION_CONTINUE_EXECUTION) {
   }
#endif
}

void
_PR_MD_CLEAN_THREAD(PRThread *thread)
{
    BOOL rv;

    if (thread->md.acceptex_buf) {
        PR_DELETE(thread->md.acceptex_buf);
    }

    if (thread->md.xmit_bufs) {
        PR_DELETE(thread->md.xmit_bufs);
    }

    if (thread->md.blocked_sema) {
        rv = CloseHandle(thread->md.blocked_sema);
        PR_ASSERT(rv);
        thread->md.blocked_sema = 0;
    }
	if (_native_threads_only) {
		if (thread->md.thr_event) {
			rv = CloseHandle(thread->md.thr_event);
			PR_ASSERT(rv);
			thread->md.thr_event = 0;
		}
	}

    if (thread->md.handle) {
        rv = CloseHandle(thread->md.handle);
        PR_ASSERT(rv);
        thread->md.handle = 0;
    }

    /* Don't call DeleteFiber on current fiber or we'll kill the whole thread.
     * Don't call free(thread) until we've switched off the thread.
     * So put this fiber (or thread) on a list to be deleted by the idle
     * fiber next time we have a chance.
     */
    if (!(thread->flags & (_PR_ATTACHED|_PR_GLOBAL_SCOPE))) {
        _MD_LOCK(&_nt_idleLock);
        _nt_idleCount++;
        PR_APPEND_LINK(&thread->links, &_nt_idleList);
        _MD_UNLOCK(&_nt_idleLock);
    }
}

void
_PR_MD_EXIT_THREAD(PRThread *thread)
{
    BOOL rv;

    if (thread->md.acceptex_buf) {
        PR_DELETE(thread->md.acceptex_buf);
    }

    if (thread->md.xmit_bufs) {
        PR_DELETE(thread->md.xmit_bufs);
    }

    if (thread->md.blocked_sema) {
        rv = CloseHandle(thread->md.blocked_sema);
        PR_ASSERT(rv);
        thread->md.blocked_sema = 0;
    }

	if (_native_threads_only) {
		if (thread->md.thr_event) {
			rv = CloseHandle(thread->md.thr_event);
			PR_ASSERT(rv);
			thread->md.thr_event = 0;
		}
	}

    if (thread->md.handle) {
        rv = CloseHandle(thread->md.handle);
        PR_ASSERT(rv);
        thread->md.handle = 0;
    }

    if (thread->flags & _PR_GLOBAL_SCOPE) {
        _MD_SET_CURRENT_THREAD(NULL);
    }
}


void
_PR_MD_EXIT(PRIntn status)
{
    _exit(status);
}

#ifdef HAVE_FIBERS

void
_pr_fiber_mainline(void *unused) 
{
    PRThread *fiber = _PR_MD_CURRENT_THREAD();

    POST_SWITCH_WORK();

    fiber->md.fiber_fn(fiber->md.fiber_arg);
}

PRThread *_PR_MD_CREATE_USER_THREAD(
    PRUint32 stacksize, void (*start)(void *), void *arg)
{
    PRThread *thread;

    if ( (thread = PR_NEW(PRThread)) == NULL ) {
        return NULL;
    }
    
    memset(thread, 0, sizeof(PRThread));
    thread->md.fiber_fn = start;
    thread->md.fiber_arg = arg;
    thread->md.fiber_stacksize = stacksize;
    return thread;
}

void
_PR_MD_CREATE_PRIMORDIAL_USER_THREAD(PRThread *thread)
{
    thread->md.fiber_id = ConvertThreadToFiber(NULL);
    PR_ASSERT(thread->md.fiber_id);
    _MD_SET_CURRENT_THREAD(thread);
    _MD_SET_LAST_THREAD(thread);
    thread->no_sched = 1;
    return;
}

void
_PR_MD_INIT_CONTEXT(PRThread *thread, char *top, void (*start) (void), PRBool *status)
{
    thread->md.fiber_fn = (void (*)(void *))start;
    thread->md.fiber_id = CreateFiber(thread->md.fiber_stacksize, 
        (LPFIBER_START_ROUTINE)_pr_fiber_mainline, NULL);
    if (thread->md.fiber_id != 0)
        *status = PR_TRUE;
    else {
        DWORD oserror = GetLastError();
        PRErrorCode prerror;
        if (oserror == ERROR_NOT_ENOUGH_MEMORY) {
            prerror = PR_OUT_OF_MEMORY_ERROR;
        } else {
            prerror = PR_UNKNOWN_ERROR;
        }
        PR_SetError(prerror, oserror);
        *status = PR_FALSE;
    }
}

void
_PR_MD_SWITCH_CONTEXT(PRThread *thread)
{
    PR_ASSERT( !_PR_IS_NATIVE_THREAD(thread) );

    thread->md.fiber_last_error = GetLastError();
    _PR_Schedule();
}

void
_PR_MD_RESTORE_CONTEXT(PRThread *thread)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();

    PR_ASSERT( !_PR_IS_NATIVE_THREAD(thread) );

    /* The user-level code for yielding will happily add ourselves to the runq
     * and then switch to ourselves; the NT fibers can't handle switching to 
     * ourselves.
     */
    if (thread != me) {
        SetLastError(thread->md.fiber_last_error);
        _MD_SET_CURRENT_THREAD(thread);
        _PR_MD_SET_LAST_THREAD(me);
        thread->no_sched = 1;
        SwitchToFiber(thread->md.fiber_id);
        POST_SWITCH_WORK();
    }
}


#endif /* HAVE_FIBERS */

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
        /*
        ** There seems to be some doubt about whether or not SuspendThread
        ** is a synchronous function. The test afterwards is to help veriry
        ** that it is, which is what Microsoft says it is.
        */
        PRUintn rv = SuspendThread(thread->md.handle);
        PR_ASSERT(0xffffffffUL != rv);
    }
}

void
_PR_MD_RESUME_THREAD(PRThread *thread)
{
    if (_PR_IS_NATIVE_THREAD(thread)) {
        ResumeThread(thread->md.handle);
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

