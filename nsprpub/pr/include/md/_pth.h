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

#ifndef nspr_pth_defs_h_
#define nspr_pth_defs_h_

/*
** Appropriate definitions of entry points not used in a pthreads world
*/
#define _PR_MD_BLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_DISABLE_CLOCK_INTERRUPTS()
#define _PR_MD_ENABLE_CLOCK_INTERRUPTS()

/* In good standards fashion, the DCE threads (based on posix-4) are not
 * quite the same as newer posix implementations.  These are mostly name
 * changes and small differences, so macros usually do the trick
 */
#ifdef _PR_DCETHREADS
#define _PT_PTHREAD_MUTEXATTR_INIT        pthread_mutexattr_create
#define _PT_PTHREAD_MUTEXATTR_DESTROY     pthread_mutexattr_delete
#define _PT_PTHREAD_MUTEX_INIT(m, a)      pthread_mutex_init(&(m), a)
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    (0 == pthread_mutex_trylock(&(m)))
#define _PT_PTHREAD_CONDATTR_INIT         pthread_condattr_create
#define _PT_PTHREAD_COND_INIT(m, a)       pthread_cond_init(&(m), a)
#define _PT_PTHREAD_CONDATTR_DESTROY      pthread_condattr_delete

/* Notes about differences between DCE threads and pthreads 10:
 *   1. pthread_mutex_trylock returns 1 when it locks the mutex
 *      0 when it does not.  The latest pthreads has a set of errno-like
 *      return values.
 *   2. return values from pthread_cond_timedwait are different.
 *
 *
 *
 */
#elif defined(BSDI)
/*
 * Mutex and condition attributes are not supported.  The attr
 * argument to pthread_mutex_init() and pthread_cond_init() must
 * be passed as NULL.
 *
 * The memset calls in _PT_PTHREAD_MUTEX_INIT and _PT_PTHREAD_COND_INIT
 * are to work around BSDI's using a single bit to indicate a mutex
 * or condition variable is initialized.  This entire BSDI section
 * will go away when BSDI releases updated threads libraries for
 * BSD/OS 3.1 and 4.0.
 */
#define _PT_PTHREAD_MUTEXATTR_INIT(x)     0
#define _PT_PTHREAD_MUTEXATTR_DESTROY(x)  /* */
#define _PT_PTHREAD_MUTEX_INIT(m, a)      (memset(&(m), 0, sizeof(m)), \
                                      pthread_mutex_init(&(m), NULL))
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#define _PT_PTHREAD_CONDATTR_INIT(x)      0
#define _PT_PTHREAD_CONDATTR_DESTROY(x)   /* */
#define _PT_PTHREAD_COND_INIT(m, a)       (memset(&(m), 0, sizeof(m)), \
                                      pthread_cond_init(&(m), NULL))
#else
#define _PT_PTHREAD_MUTEXATTR_INIT        pthread_mutexattr_init
#define _PT_PTHREAD_MUTEXATTR_DESTROY     pthread_mutexattr_destroy
#define _PT_PTHREAD_MUTEX_INIT(m, a)      pthread_mutex_init(&(m), &(a))
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#if defined(DARWIN)
#define _PT_PTHREAD_CONDATTR_INIT(x)      0
#else
#define _PT_PTHREAD_CONDATTR_INIT         pthread_condattr_init
#endif
#define _PT_PTHREAD_CONDATTR_DESTROY      pthread_condattr_destroy
#define _PT_PTHREAD_COND_INIT(m, a)       pthread_cond_init(&(m), &(a))
#endif

/* The pthreads standard does not specify an invalid value for the
 * pthread_t handle.  (0 is usually an invalid pthread identifier
 * but there are exceptions, for example, DG/UX.)  These macros
 * define a way to set the handle to or compare the handle with an
 * invalid identifier.  These macros are not portable and may be
 * more of a problem as we adapt to more pthreads implementations.
 * They are only used in the PRMonitor functions.  Do not use them
 * in new code.
 *
 * Unfortunately some of our clients depend on certain properties
 * of our PRMonitor implementation, preventing us from replacing
 * it by a portable implementation.
 * - High-performance servers like the fact that PR_EnterMonitor
 *   only calls PR_Lock and PR_ExitMonitor only calls PR_Unlock.
 *   (A portable implementation would use a PRLock and a PRCondVar
 *   to implement the recursive lock in a monitor and call both
 *   PR_Lock and PR_Unlock in PR_EnterMonitor and PR_ExitMonitor.)
 *   Unfortunately this forces us to read the monitor owner field
 *   without holding a lock.
 * - One way to make it safe to read the monitor owner field
 *   without holding a lock is to make that field a PRThread*
 *   (one should be able to read a pointer with a single machine
 *   instruction).  However, PR_GetCurrentThread calls calloc if
 *   it is called by a thread that was not created by NSPR.  The
 *   malloc tracing tools in the Mozilla client use PRMonitor for
 *   locking in their malloc, calloc, and free functions.  If
 *   PR_EnterMonitor calls any of these functions, infinite
 *   recursion ensues.
 */
#if defined(_PR_DCETHREADS)
#define _PT_PTHREAD_INVALIDATE_THR_HANDLE(t) \
	memset(&(t), 0, sizeof(pthread_t))
#define _PT_PTHREAD_THR_HANDLE_IS_INVALID(t) \
	(!memcmp(&(t), &pt_zero_tid, sizeof(pthread_t)))
#define _PT_PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#elif defined(IRIX) || defined(OSF1) || defined(AIX) || defined(SOLARIS) \
	|| defined(HPUX) || defined(LINUX) || defined(FREEBSD) \
	|| defined(NETBSD) || defined(OPENBSD) || defined(BSDI) \
	|| defined(VMS) || defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE)
#define _PT_PTHREAD_INVALIDATE_THR_HANDLE(t)  (t) = 0
#define _PT_PTHREAD_THR_HANDLE_IS_INVALID(t)  (t) == 0
#define _PT_PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#else 
#error "pthreads is not supported for this architecture"
#endif

#if defined(_PR_DCETHREADS)
#define _PT_PTHREAD_ATTR_INIT            pthread_attr_create
#define _PT_PTHREAD_ATTR_DESTROY         pthread_attr_delete
#define _PT_PTHREAD_CREATE(t, a, f, r)   pthread_create(t, a, f, r) 
#define _PT_PTHREAD_KEY_CREATE           pthread_keycreate
#define _PT_PTHREAD_ATTR_SETSCHEDPOLICY  pthread_attr_setsched
#define _PT_PTHREAD_ATTR_GETSTACKSIZE(a, s) \
                                     (*(s) = pthread_attr_getstacksize(*(a)), 0)
#define _PT_PTHREAD_GETSPECIFIC(k, r) \
		pthread_getspecific((k), (pthread_addr_t *) &(r))
#elif defined(_PR_PTHREADS)
#define _PT_PTHREAD_ATTR_INIT            pthread_attr_init
#define _PT_PTHREAD_ATTR_DESTROY         pthread_attr_destroy
#define _PT_PTHREAD_CREATE(t, a, f, r)   pthread_create(t, &a, f, r) 
#define _PT_PTHREAD_KEY_CREATE           pthread_key_create
#define _PT_PTHREAD_ATTR_SETSCHEDPOLICY  pthread_attr_setschedpolicy
#define _PT_PTHREAD_ATTR_GETSTACKSIZE(a, s) pthread_attr_getstacksize(a, s)
#define _PT_PTHREAD_GETSPECIFIC(k, r)    (r) = pthread_getspecific(k)
#else
#error "Cannot determine pthread strategy"
#endif

#if defined(_PR_DCETHREADS)
#define _PT_PTHREAD_EXPLICIT_SCHED      _PT_PTHREAD_DEFAULT_SCHED
#endif

/*
 * pthread_mutex_trylock returns different values in DCE threads and
 * pthreads.
 */
#if defined(_PR_DCETHREADS)
#define PT_TRYLOCK_SUCCESS 1
#define PT_TRYLOCK_BUSY    0
#else
#define PT_TRYLOCK_SUCCESS 0
#define PT_TRYLOCK_BUSY    EBUSY
#endif

/*
 * These platforms don't have sigtimedwait()
 */
#if (defined(AIX) && !defined(AIX4_3_PLUS)) || defined(LINUX) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(VMS) || defined(UNIXWARE) \
	|| defined(DARWIN)
#define PT_NO_SIGTIMEDWAIT
#endif

/*
 * These platforms don't have pthread_kill()
 */
#if defined(DARWIN)
#define pthread_kill(thread, sig) ENOSYS
#endif

#if defined(OSF1) || defined(VMS)
#define PT_PRIO_MIN            PRI_OTHER_MIN
#define PT_PRIO_MAX            PRI_OTHER_MAX
#elif defined(IRIX)
#include <sys/sched.h>
#define PT_PRIO_MIN            PX_PRIO_MIN
#define PT_PRIO_MAX            PX_PRIO_MAX
#elif defined(AIX)
#include <sys/priv.h>
#include <sys/sched.h>
#ifndef PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_JOINABLE     PTHREAD_CREATE_UNDETACHED
#endif
#define PT_PRIO_MIN            DEFAULT_PRIO
#define PT_PRIO_MAX            DEFAULT_PRIO
#elif defined(HPUX)

#if defined(_PR_DCETHREADS)
#define PT_PRIO_MIN            PRI_OTHER_MIN
#define PT_PRIO_MAX            PRI_OTHER_MAX
#else /* defined(_PR_DCETHREADS) */
#include <sys/sched.h>
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#endif /* defined(_PR_DCETHREADS) */

#elif defined(LINUX) || defined(FREEBSD)
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#elif defined(NTO)
/*
 * Neutrino has functions that return the priority range but
 * they return invalid numbers, so I just hard coded these here
 * for now.  Jerry.Kirk@Nexarecorp.com
 */
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            30
#elif defined(SOLARIS)
/*
 * Solaris doesn't seem to have macros for the min/max priorities.
 * The range of 0-127 is mentioned in the pthread_setschedparam(3T)
 * man pages, and pthread_setschedparam indeed allows 0-127.  However,
 * pthread_attr_setschedparam does not allow 0; it allows 1-127.
 */
#define PT_PRIO_MIN            1
#define PT_PRIO_MAX            127
#elif defined(OPENBSD)
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            31
#elif defined(NETBSD) \
	|| defined(BSDI) || defined(DARWIN) || defined(UNIXWARE) /* XXX */
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            126
#else
#error "pthreads is not supported for this architecture"
#endif

/*
 * The _PT_PTHREAD_YIELD function is called from a signal handler.
 * Needed for garbage collection -- Look at PR_Suspend/PR_Resume
 * implementation.
 */
#if defined(_PR_DCETHREADS)
#define _PT_PTHREAD_YIELD()            	pthread_yield()
#elif defined(OSF1) || defined(VMS)
/*
 * sched_yield can't be called from a signal handler.  Must use
 * the _np version.
 */
#define _PT_PTHREAD_YIELD()            	pthread_yield_np()
#elif defined(AIX)
extern int (*_PT_aix_yield_fcn)();
#define _PT_PTHREAD_YIELD()			(*_PT_aix_yield_fcn)()
#elif defined(IRIX)
#include <time.h>
#define _PT_PTHREAD_YIELD() \
    PR_BEGIN_MACRO               				\
		struct timespec onemillisec = {0};		\
		onemillisec.tv_nsec = 1000000L;			\
        nanosleep(&onemillisec,NULL);			\
    PR_END_MACRO
#elif defined(HPUX) || defined(LINUX) || defined(SOLARIS) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE)
#define _PT_PTHREAD_YIELD()            	sched_yield()
#else
#error "Need to define _PT_PTHREAD_YIELD for this platform"
#endif

#endif /* nspr_pth_defs_h_ */
