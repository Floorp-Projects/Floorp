
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <process.h>  /* for _beginthreadex() */

#if defined(_MSC_VER) && _MSC_VER <= 1200
/*
 * VC++ 6.0 doesn't have DWORD_PTR.
 */

typedef DWORD DWORD_PTR;
#endif /* _MSC_VER <= 1200 */

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

typedef HRESULT (WINAPI *SETTHREADDESCRIPTION)(HANDLE, PCWSTR);
static SETTHREADDESCRIPTION sSetThreadDescription = NULL;

void
_PR_MD_EARLY_INIT()
{
    HMODULE hModule;

#ifndef _PR_USE_STATIC_TLS
    _pr_currentThreadIndex = TlsAlloc();
    _pr_lastThreadIndex = TlsAlloc();
    _pr_currentCPUIndex = TlsAlloc();
#endif

#if defined(_WIN64) && defined(WIN95)
    _fd_waiting_for_overlapped_done_lock = PR_NewLock();
#endif

    // SetThreadDescription is Windows 10 build 1607+
    hModule = GetModuleHandleW(L"kernel32.dll");
    if (hModule) {
        sSetThreadDescription =
            (SETTHREADDESCRIPTION) GetProcAddress(
                hModule,
                "SetThreadDescription");
    }
}

void _PR_MD_CLEANUP_BEFORE_EXIT(void)
{
    _PR_NT_FreeSids();

    _PR_MD_CleanupSockets();

    WSACleanup();

#ifndef _PR_USE_STATIC_TLS
    TlsFree(_pr_currentThreadIndex);
    TlsFree(_pr_lastThreadIndex);
    TlsFree(_pr_currentCPUIndex);
#endif

#if defined(_WIN64) && defined(WIN95)
    // For each iteration check if TFO overlapped IOs are down.
    if (_fd_waiting_for_overlapped_done_lock) {
        PRIntervalTime delay = PR_MillisecondsToInterval(1000);
        PRFileDescList *cur;
        do {
            CheckOverlappedPendingSocketsAreDone();

            PR_Lock(_fd_waiting_for_overlapped_done_lock);
            cur = _fd_waiting_for_overlapped_done;
            PR_Unlock(_fd_waiting_for_overlapped_done_lock);
#if defined(DO_NOT_WAIT_FOR_CONNECT_OVERLAPPED_OPERATIONS)
            cur = NULL;
#endif
            if (cur) {
                PR_Sleep(delay); // wait another 1s.
            }
        } while (cur);

        PR_DestroyLock(_fd_waiting_for_overlapped_done_lock);
    }
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
        BOOL ok = DuplicateHandle(
                      GetCurrentProcess(),     /* Process of source handle */
                      GetCurrentThread(),      /* Pseudo Handle to dup */
                      GetCurrentProcess(),     /* Process of handle */
                      &(thread->md.handle),    /* resulting handle */
                      0L,                      /* access flags */
                      FALSE,                   /* Inheritable */
                      DUPLICATE_SAME_ACCESS);  /* Options */
        if (!ok) {
            return PR_FAILURE;
        }
        thread->id = GetCurrentThreadId();
        thread->md.id = thread->id;
    }

    /* Create the blocking IO semaphore */
    thread->md.blocked_sema = CreateSemaphore(NULL, 0, 1, NULL);
    if (thread->md.blocked_sema == NULL) {
        return PR_FAILURE;
    }
    else {
        return PR_SUCCESS;
    }
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
                            CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION,
                            &(thread->id));
    if(!thread->md.handle) {
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
    if ( ResumeThread( thread->md.handle ) != -1) {
        return PR_SUCCESS;
    }

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
#endif

    if (sSetThreadDescription) {
        WCHAR wideName[MAX_PATH];
        if (MultiByteToWideChar(CP_ACP, 0, name, -1, wideName, MAX_PATH)) {
            sSetThreadDescription(GetCurrentThread(), wideName);
        }
    }

#ifdef _MSC_VER
    if (!IsDebuggerPresent()) {
        return;
    }

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
#ifdef WINCE
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return -1;
#else
    DWORD_PTR rv;

    rv = SetThreadAffinityMask(thread->md.handle, mask);

    return rv?0:-1;
#endif
}

PRInt32 _PR_MD_GETTHREADAFFINITYMASK(PRThread *thread, PRUint32 *mask)
{
#ifdef WINCE
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return -1;
#else
    BOOL rv;
    DWORD_PTR process_mask;
    DWORD_PTR system_mask;

    rv = GetProcessAffinityMask(GetCurrentProcess(),
                                &process_mask, &system_mask);
    if (rv) {
        *mask = (PRUint32)process_mask;
    }

    return rv?0:-1;
#endif
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
