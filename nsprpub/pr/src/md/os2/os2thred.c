/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <process.h>  /* for _beginthread() */
#include <signal.h>
#include <float.h>

/* --- globals ------------------------------------------------ */
_NSPR_TLS*        pThreadLocalStorage = 0;
_PRInterruptTable             _pr_interruptTable[] = { { 0 } };
APIRET (* APIENTRY QueryThreadContext)(TID, ULONG, PCONTEXTRECORD);

void
_PR_MD_ENSURE_TLS(void)
{
   if(!pThreadLocalStorage)
   {
      /* Allocate thread local storage (TLS).  Note, that only 32 bytes can
       * be allocated at a time. 
       */
      int rc = DosAllocThreadLocalMemory(sizeof(_NSPR_TLS) / 4, (PULONG*)&pThreadLocalStorage);
      PR_ASSERT(rc == NO_ERROR);
      memset(pThreadLocalStorage, 0, sizeof(_NSPR_TLS));
   }
}

void
_PR_MD_EARLY_INIT()
{
   HMODULE hmod;

   if (DosLoadModule(NULL, 0, "DOSCALL1", &hmod) == 0)
       DosQueryProcAddr(hmod, 877, "DOSQUERYTHREADCONTEXT",
                        (PFN *)&QueryThreadContext);
}

static void
_pr_SetThreadMDHandle(PRThread *thread)
{
   PTIB ptib;
   PPIB ppib;
   PRUword rc;

   rc = DosGetInfoBlocks(&ptib, &ppib);

   thread->md.handle = ptib->tib_ptib2->tib2_ultid;
}

/* On OS/2, some system function calls seem to change the FPU control word,
 * such that we crash with a floating underflow exception.  The FIX_FPU() call
 * in jsnum.c does not always work, as sometimes FIX_FPU() is called BEFORE the
 * OS/2 system call that horks the FPU control word.  So, we set an exception
 * handler that covers any floating point exceptions and resets the FPU CW to
 * the required value.
 */
static ULONG
_System OS2_FloatExcpHandler(PEXCEPTIONREPORTRECORD p1,
                             PEXCEPTIONREGISTRATIONRECORD p2,
                             PCONTEXTRECORD p3,
                             PVOID pv)
{
#ifdef DEBUG_pedemonte
    printf("Entering exception handler; ExceptionNum = %x\n", p1->ExceptionNum);
    switch(p1->ExceptionNum) {
        case XCPT_FLOAT_DENORMAL_OPERAND:
            printf("got XCPT_FLOAT_DENORMAL_OPERAND\n");
            break;
        case XCPT_FLOAT_DIVIDE_BY_ZERO:
            printf("got XCPT_FLOAT_DIVIDE_BY_ZERO\n");
            break;
        case XCPT_FLOAT_INEXACT_RESULT:
            printf("got XCPT_FLOAT_INEXACT_RESULT\n");
            break;
        case XCPT_FLOAT_INVALID_OPERATION:
            printf("got XCPT_FLOAT_INVALID_OPERATION\n");
            break;
        case XCPT_FLOAT_OVERFLOW:
            printf("got XCPT_FLOAT_OVERFLOW\n");
            break;
        case XCPT_FLOAT_STACK_CHECK:
            printf("got XCPT_FLOAT_STACK_CHECK\n");
            break;
        case XCPT_FLOAT_UNDERFLOW:
            printf("got XCPT_FLOAT_UNDERFLOW\n");
            break;
    }
#endif

    switch(p1->ExceptionNum) {
        case XCPT_FLOAT_DENORMAL_OPERAND:
        case XCPT_FLOAT_DIVIDE_BY_ZERO:
        case XCPT_FLOAT_INEXACT_RESULT:
        case XCPT_FLOAT_INVALID_OPERATION:
        case XCPT_FLOAT_OVERFLOW:
        case XCPT_FLOAT_STACK_CHECK:
        case XCPT_FLOAT_UNDERFLOW:
        {
            unsigned cw = p3->ctx_env[0];
            if ((cw & MCW_EM) != MCW_EM) {
                /* Mask out all floating point exceptions */
                p3->ctx_env[0] |= MCW_EM;
                /* Following two lines set precision to 53 bit mantissa.  See jsnum.c */
                p3->ctx_env[0] &= ~MCW_PC;
                p3->ctx_env[0] |= PC_53;
                return XCPT_CONTINUE_EXECUTION;
            }
        }
    }
    return XCPT_CONTINUE_SEARCH;
}

PR_IMPLEMENT(void)
PR_OS2_SetFloatExcpHandler(EXCEPTIONREGISTRATIONRECORD* excpreg)
{
    /* setup the exception handler for the thread */
    APIRET rv;
    excpreg->ExceptionHandler = OS2_FloatExcpHandler;
    excpreg->prev_structure = NULL;
    rv = DosSetExceptionHandler(excpreg);
    PR_ASSERT(rv == NO_ERROR);
}

PR_IMPLEMENT(void)
PR_OS2_UnsetFloatExcpHandler(EXCEPTIONREGISTRATIONRECORD* excpreg)
{
    /* unset exception handler */
    APIRET rv = DosUnsetExceptionHandler(excpreg);
    PR_ASSERT(rv == NO_ERROR);
}

PRStatus
_PR_MD_INIT_THREAD(PRThread *thread)
{
   APIRET rv;

   if (thread->flags & (_PR_PRIMORDIAL | _PR_ATTACHED)) {
      _pr_SetThreadMDHandle(thread);
   }

   /* Create the blocking IO semaphore */
   rv = DosCreateEventSem(NULL, &(thread->md.blocked_sema), 0, 0);
   return (rv == NO_ERROR) ? PR_SUCCESS : PR_FAILURE;
}

typedef struct param_store
{
    void (*start)(void *);
    PRThread* thread;
} PARAMSTORE;

/* This is a small intermediate function that sets/unsets the exception
   handler before calling the initial thread function */
static void
ExcpStartFunc(void* arg)
{
    EXCEPTIONREGISTRATIONRECORD excpreg;
    PARAMSTORE params, *pParams = arg;

    PR_OS2_SetFloatExcpHandler(&excpreg);

    params = *pParams;
    PR_Free(pParams);
    params.start(params.thread);

    PR_OS2_UnsetFloatExcpHandler(&excpreg);
}

PRStatus
_PR_MD_CREATE_THREAD(PRThread *thread, 
                  void (*start)(void *), 
                  PRThreadPriority priority, 
                  PRThreadScope scope, 
                  PRThreadState state, 
                  PRUint32 stackSize)
{
    PARAMSTORE* params = PR_Malloc(sizeof(PARAMSTORE));
    params->start = start;
    params->thread = thread;
    thread->md.handle = thread->id = (TID) _beginthread(ExcpStartFunc,
                                                        NULL, 
                                                        thread->stack->stackSize,
                                                        params);
    if(thread->md.handle == -1) {
        return PR_FAILURE;
    }

    /*
     * On OS/2, a thread is created with a thread priority of
     * THREAD_PRIORITY_NORMAL
     */

    if (priority != PR_PRIORITY_NORMAL) {
        _PR_MD_SET_PRIORITY(&(thread->md), priority);
    }

    return PR_SUCCESS;
}

void
_PR_MD_YIELD(void)
{
    /* Isn't there some problem with DosSleep(0) on OS/2? */
    DosSleep(0);
}

void
_PR_MD_SET_PRIORITY(_MDThread *thread, PRThreadPriority newPri)
{
    int nativePri = PRTYC_NOCHANGE;
    BOOL rv;

    if (newPri < PR_PRIORITY_FIRST) {
        newPri = PR_PRIORITY_FIRST;
    } else if (newPri > PR_PRIORITY_LAST) {
        newPri = PR_PRIORITY_LAST;
    }
    switch (newPri) {
        case PR_PRIORITY_LOW:
        case PR_PRIORITY_NORMAL:
            nativePri = PRTYC_REGULAR;
            break;
        case PR_PRIORITY_HIGH:
            nativePri = PRTYC_FOREGROUNDSERVER;
            break;
        case PR_PRIORITY_URGENT:
            nativePri = PRTYC_TIMECRITICAL;
    }
    rv = DosSetPriority(PRTYS_THREAD, nativePri, 0, thread->handle);
    PR_ASSERT(rv == NO_ERROR);
    if (rv != NO_ERROR) {
        PR_LOG(_pr_thread_lm, PR_LOG_MIN,
                ("PR_SetThreadPriority: can't set thread priority\n"));
    }
    return;
}

void
_PR_MD_CLEAN_THREAD(PRThread *thread)
{
    APIRET rv;

    if (thread->md.blocked_sema) {
        rv = DosCloseEventSem(thread->md.blocked_sema);
        PR_ASSERT(rv == NO_ERROR);
        thread->md.blocked_sema = 0;
    }

    if (thread->md.handle) {
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

#ifdef HAVE_THREAD_AFFINITY
PR_EXTERN(PRInt32) 
_PR_MD_SETTHREADAFFINITYMASK(PRThread *thread, PRUint32 mask )
{
   /* Can we do this on OS/2?  Only on SMP versions? */
   PR_NOT_REACHED("Not implemented");
   return 0;

 /* This is what windows does:
    int rv;

    rv = SetThreadAffinityMask(thread->md.handle, mask);

    return rv?0:-1;
  */
}

PR_EXTERN(PRInt32)
_PR_MD_GETTHREADAFFINITYMASK(PRThread *thread, PRUint32 *mask)
{
   /* Can we do this on OS/2?  Only on SMP versions? */
   PR_NOT_REACHED("Not implemented");
   return 0;

 /* This is what windows does:
    PRInt32 rv, system_mask;

    rv = GetProcessAffinityMask(GetCurrentProcess(), mask, &system_mask);
    
    return rv?0:-1;
  */
}
#endif /* HAVE_THREAD_AFFINITY */

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
       APIRET rc;

        /* XXXMB - DosSuspendThread() is not a blocking call; how do we
         * know when the thread is *REALLY* suspended?
         */
       rc = DosSuspendThread(thread->md.handle);
       PR_ASSERT(rc == NO_ERROR);
    }
}

void
_PR_MD_RESUME_THREAD(PRThread *thread)
{
    if (_PR_IS_NATIVE_THREAD(thread)) {
        DosResumeThread(thread->md.handle);
    }
}


PRThread*
_MD_CURRENT_THREAD(void)
{
    PRThread *thread;

    thread = _MD_GET_ATTACHED_THREAD();

    if (NULL == thread) {
        thread = _PRI_AttachThread(PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL, 0);
    }

    PR_ASSERT(thread != NULL);
    return thread;
}

