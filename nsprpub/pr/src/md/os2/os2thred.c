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
#include <process.h>  /* for _beginthread() */

#ifdef XP_OS2_VACPP
#include <time.h>     /* for _tzset() */
#endif

/* --- Declare these to avoid "implicit" warnings --- */
PR_EXTERN(void) _PR_MD_NEW_SEM(_MDSemaphore *md, PRUintn value);
PR_EXTERN(void) _PR_MD_DESTROY_SEM(_MDSemaphore *md);

/* --- globals ------------------------------------------------ */
_NSPR_TLS*        pThreadLocalStorage = 0;
_PRInterruptTable             _pr_interruptTable[] = { { 0 } };
APIRET (* APIENTRY QueryThreadContext)(TID, ULONG, PCONTEXTRECORD);

PR_IMPLEMENT(void)
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

PR_IMPLEMENT(void)
_PR_MD_EARLY_INIT()
{
   HMODULE hmod;

   if (DosLoadModule(NULL, 0, "DOSCALL1.DLL", &hmod) == 0)
       DosQueryProcAddr(hmod, 877, "DOSQUERYTHREADCONTEXT",
                        (PFN *)&QueryThreadContext);

#ifdef XP_OS2_VACPP
   _tzset();
#endif
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


PR_IMPLEMENT(PRStatus)
_PR_MD_INIT_THREAD(PRThread *thread)
{
   if (thread->flags & (_PR_PRIMORDIAL | _PR_ATTACHED)) {
      _pr_SetThreadMDHandle(thread);
   }

   /* Create the blocking IO semaphore */
   _PR_MD_NEW_SEM(&thread->md.blocked_sema, 1);
   return (thread->md.blocked_sema.sem != 0) ? PR_SUCCESS : PR_FAILURE;
}

PR_IMPLEMENT(PRStatus) 
_PR_MD_CREATE_THREAD(PRThread *thread, 
                  void (*start)(void *), 
                  PRThreadPriority priority, 
                  PRThreadScope scope, 
                  PRThreadState state, 
                  PRUint32 stackSize)
{
    thread->md.handle = thread->id = (TID) _beginthread(
                    (void(* _Optlink)(void*))start,
                    NULL, 
                    thread->stack->stackSize,
                    thread);
    if(thread->md.handle == -1) {
        return PR_FAILURE;
    }
    _PR_MD_SET_PRIORITY(&(thread->md), priority);

    return PR_SUCCESS;
}

PR_IMPLEMENT(void)    
_PR_MD_YIELD(void)
{
    /* Isn't there some problem with DosSleep(0) on OS/2? */
    DosSleep(0);
}

PR_IMPLEMENT(void)     
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
            nativePri = PRTYC_IDLETIME;
            break;
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

PR_IMPLEMENT(void)
_PR_MD_CLEAN_THREAD(PRThread *thread)
{
	if (&thread->md.blocked_sema) {
	  _PR_MD_DESTROY_SEM(&thread->md.blocked_sema);
	}
	
	if (thread->md.handle) {
	  DosKillThread(thread->md.handle);
	  thread->md.handle = 0;
	}
}

PR_IMPLEMENT(void)
_PR_MD_EXIT_THREAD(PRThread *thread)
{
    _PR_MD_DESTROY_SEM(&thread->md.blocked_sema);

    if (thread->md.handle) {
       /* DosKillThread will not kill a suspended thread, but it will mark it
        * for death; we must resume it after killing it to make sure it knows
        * it is about to die (pretty wicked, huh?).
        *
        * DosKillThread will not kill the current thread, instead we must use
        * DosExit.
        */
       if ( thread != _MD_CURRENT_THREAD() ) {
           DosKillThread( thread->md.handle );
           DosResumeThread( thread->md.handle );
       } else {
#ifndef XP_OS2_EMX
           _endthread();
#endif
       }
       thread->md.handle = 0;
    }

    _PR_MD_SET_CURRENT_THREAD(NULL);
}


PR_IMPLEMENT(void)
_PR_MD_EXIT(PRIntn status)
{
    _exit(status);
}

#ifdef HAVE_THREAD_AFFINITY
PR_EXTERN(PRInt32) 
_PR_MD_SETTHREADAFFINITYMASK(PRThread *thread, PRUint32 mask )
{
   /* Can we do this on OS/2?  Only on SMP versions? */
   PR_ASSERT(!"Not implemented");
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
   PR_ASSERT(!"Not implemented");
   return 0;

 /* This is what windows does:
    PRInt32 rv, system_mask;

    rv = GetProcessAffinityMask(GetCurrentProcess(), mask, &system_mask);
    
    return rv?0:-1;
  */
}
#endif /* HAVE_THREAD_AFFINITY */

PR_IMPLEMENT(void) 
_PR_MD_SUSPEND_CPU(_PRCPU *cpu) 
{
    _PR_MD_SUSPEND_THREAD(cpu->thread);
}

PR_IMPLEMENT(void)
_PR_MD_RESUME_CPU(_PRCPU *cpu)
{
    _PR_MD_RESUME_THREAD(cpu->thread);
}

PR_IMPLEMENT(void)
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

PR_IMPLEMENT(void)
_PR_MD_RESUME_THREAD(PRThread *thread)
{
    if (_PR_IS_NATIVE_THREAD(thread)) {
        DosResumeThread(thread->md.handle);
    }
}

