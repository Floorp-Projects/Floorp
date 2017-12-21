/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_pth_defs_h_
#define nspr_pth_defs_h_

/*
** Appropriate definitions of entry points not used in a pthreads world
*/
#define _PR_MD_BLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_DISABLE_CLOCK_INTERRUPTS()
#define _PR_MD_ENABLE_CLOCK_INTERRUPTS()

#if defined(BSDI)
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
#if defined(FREEBSD)
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    pt_pthread_mutex_is_locked(&(m))
#else
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#endif
#if defined(ANDROID)
/* Conditional attribute init and destroy aren't implemented in bionic. */
#define _PT_PTHREAD_CONDATTR_INIT(x)      0
#define _PT_PTHREAD_CONDATTR_DESTROY(x)   /* */
#else
#define _PT_PTHREAD_CONDATTR_INIT         pthread_condattr_init
#define _PT_PTHREAD_CONDATTR_DESTROY      pthread_condattr_destroy
#endif
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
#if defined(IRIX) || defined(OSF1) || defined(AIX) || defined(SOLARIS) \
	|| defined(LINUX) || defined(__GNU__) || defined(__GLIBC__) \
	|| defined(HPUX) || defined(FREEBSD) \
	|| defined(NETBSD) || defined(OPENBSD) || defined(BSDI) \
	|| defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE) || defined(RISCOS)	|| defined(SYMBIAN)
#define _PT_PTHREAD_INVALIDATE_THR_HANDLE(t)  (t) = 0
#define _PT_PTHREAD_THR_HANDLE_IS_INVALID(t)  (t) == 0
#define _PT_PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#else 
#error "pthreads is not supported for this architecture"
#endif

#if defined(_PR_PTHREADS)
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

/*
 * These platforms don't have sigtimedwait()
 */
#if (defined(AIX) && !defined(AIX4_3_PLUS)) \
	|| defined(LINUX) || defined(__GNU__)|| defined(__GLIBC__) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(UNIXWARE) \
	|| defined(DARWIN) || defined(SYMBIAN)
#define PT_NO_SIGTIMEDWAIT
#endif

#if defined(OSF1)
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
#include <sys/sched.h>
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#elif defined(LINUX) || defined(__GNU__) || defined(__GLIBC__) \
	|| defined(FREEBSD) || defined(SYMBIAN)
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
	|| defined(BSDI) || defined(DARWIN) || defined(UNIXWARE) \
	|| defined(RISCOS) /* XXX */
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
#if defined(OSF1)
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
#elif defined(HPUX) || defined(SOLARIS) \
	|| defined(LINUX) || defined(__GNU__) || defined(__GLIBC__) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE) || defined(RISCOS) || defined(SYMBIAN)
#define _PT_PTHREAD_YIELD()            	sched_yield()
#else
#error "Need to define _PT_PTHREAD_YIELD for this platform"
#endif

#endif /* nspr_pth_defs_h_ */
