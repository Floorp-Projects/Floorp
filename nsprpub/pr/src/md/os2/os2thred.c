/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"
#include <process.h>  /* for _beginthread() */

#ifdef XP_OS2_VACPP
#include <time.h>     /* for _tzset() */
#endif

#ifdef XP_OS2_EMX
#include <signal.h>
#endif

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

PRStatus
_PR_MD_CREATE_THREAD(PRThread *thread, 
                  void (*start)(void *), 
                  PRThreadPriority priority, 
                  PRThreadScope scope, 
                  PRThreadState state, 
                  PRUint32 stackSize)
{
    thread->md.handle = thread->id = (TID) _beginthread(start,
                                                        NULL, 
                                                        thread->stack->stackSize,
                                                        thread);
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

