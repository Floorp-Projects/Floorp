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
#define PTHREAD_MUTEXATTR_INIT        pthread_mutexattr_create
#define PTHREAD_MUTEXATTR_DESTROY     pthread_mutexattr_delete
#define PTHREAD_MUTEX_INIT(m, a)      pthread_mutex_init(&(m), a)
#define PTHREAD_MUTEX_IS_LOCKED(m)    (0 == pthread_mutex_trylock(&(m)))
#define PTHREAD_CONDATTR_INIT         pthread_condattr_create
#define PTHREAD_COND_INIT(m, a)       pthread_cond_init(&(m), a)
#define PTHREAD_CONDATTR_DESTROY      pthread_condattr_delete

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
 * The memset calls in PTHREAD_MUTEX_INIT and PTHREAD_COND_INIT
 * are to work around BSDI's using a single bit to indicate a mutex
 * or condition variable is initialized.  This entire BSDI section
 * will go away when BSDI releases updated threads libraries for
 * BSD/OS 3.1 and 4.0.
 */
#define PTHREAD_MUTEXATTR_INIT(x)     0
#define PTHREAD_MUTEXATTR_DESTROY(x)  /* */
#define PTHREAD_MUTEX_INIT(m, a)      (memset(&(m), 0, sizeof(m)), \
                                      pthread_mutex_init(&(m), NULL))
#define PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#define PTHREAD_CONDATTR_INIT(x)      0
#define PTHREAD_CONDATTR_DESTROY(x)   /* */
#define PTHREAD_COND_INIT(m, a)       (memset(&(m), 0, sizeof(m)), \
                                      pthread_cond_init(&(m), NULL))
#else
#define PTHREAD_MUTEXATTR_INIT        pthread_mutexattr_init
#define PTHREAD_MUTEXATTR_DESTROY     pthread_mutexattr_destroy
#define PTHREAD_MUTEX_INIT(m, a)      pthread_mutex_init(&(m), &(a))
#define PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#define PTHREAD_CONDATTR_INIT         pthread_condattr_init
#define PTHREAD_CONDATTR_DESTROY      pthread_condattr_destroy
#define PTHREAD_COND_INIT(m, a)       pthread_cond_init(&(m), &(a))
#endif

/* The pthread_t handle used to identify a thread can vary in size
 * with different implementations of pthreads.  This macro defines
 * a way to get a unique identifier from the handle.  This may be
 * more of a problem as we adapt to more pthreads implementations.
 */
#if defined(_PR_DCETHREADS)
#define PTHREAD_ZERO_THR_HANDLE(t)        memset(&(t), 0, sizeof(pthread_t))
#define PTHREAD_THR_HANDLE_IS_ZERO(t) \
	(!memcmp(&(t), &pt_zero_tid, sizeof(pthread_t)))
#define PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#elif defined(IRIX) || defined(OSF1) || defined(AIX) || defined(SOLARIS) \
	|| defined(HPUX) || defined(LINUX) || defined(FREEBSD) \
	|| defined(NETBSD) || defined(OPENBSD) || defined(BSDI)
#define PTHREAD_ZERO_THR_HANDLE(t)        (t) = 0
#define PTHREAD_THR_HANDLE_IS_ZERO(t)     (t) == 0
#define PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#else 
#error "pthreads is not supported for this architecture"
#endif

#if defined(_PR_DCETHREADS)
#define PTHREAD_ATTR_INIT            pthread_attr_create
#define PTHREAD_ATTR_DESTROY         pthread_attr_delete
#define PTHREAD_CREATE(t, a, f, r)   pthread_create(t, a, f, r) 
#define PTHREAD_KEY_CREATE           pthread_keycreate
#define PTHREAD_ATTR_SETSCHEDPOLICY  pthread_attr_setsched
#define PTHREAD_ATTR_GETSTACKSIZE(a, s) \
                                     (*(s) = pthread_attr_getstacksize(*(a)), 0)
#define PTHREAD_GETSPECIFIC(k, r) \
		pthread_getspecific((k), (pthread_addr_t *) &(r))
#elif defined(_PR_PTHREADS)
#define PTHREAD_ATTR_INIT            pthread_attr_init
#define PTHREAD_ATTR_DESTROY         pthread_attr_destroy
#define PTHREAD_CREATE(t, a, f, r)   pthread_create(t, &a, f, r) 
#define PTHREAD_KEY_CREATE           pthread_key_create
#define PTHREAD_ATTR_SETSCHEDPOLICY  pthread_attr_setschedpolicy
#define PTHREAD_ATTR_GETSTACKSIZE(a, s) pthread_attr_getstacksize(a, s)
#define PTHREAD_GETSPECIFIC(k, r)    (r) = pthread_getspecific(k)
#else
#error "Cannot determine pthread strategy"
#endif

#if defined(_PR_DCETHREADS)
#define PTHREAD_EXPLICIT_SCHED      PTHREAD_DEFAULT_SCHED
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
 * These platforms don't have pthread_atfork()
 */
#if defined(_PR_DCETHREADS) || defined(FREEBSD) \
	|| (defined(LINUX) && defined(__alpha)) \
	|| defined(NETBSD) || defined(OPENBSD)
#define PT_NO_ATFORK
#endif

/*
 * These platforms don't have sigtimedwait()
 */
#if (defined(AIX) && !defined(AIX4_3)) || defined(LINUX) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI)
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

#if defined(_PR_DCETHREADS)
#define PT_PRIO_MIN            PRI_OTHER_MIN
#define PT_PRIO_MAX            PRI_OTHER_MAX
#else /* defined(_PR_DCETHREADS) */
#include <sys/sched.h>
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#endif /* defined(_PR_DCETHREADS) */

#elif defined(LINUX)
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#elif defined(SOLARIS)
/*
 * Solaris doesn't seem to have macros for the min/max priorities.
 * The range of 0-127 is mentioned in the pthread_setschedparam(3T)
 * man pages, and pthread_setschedparam indeed allows 0-127.  However,
 * pthread_attr_setschedparam does not allow 0; it allows 1-127.
 */
#define PT_PRIO_MIN            1
#define PT_PRIO_MAX            127
#elif defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) /* XXX */
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            126
#else
#error "pthreads is not supported for this architecture"
#endif

/*
 * The PTHREAD_YIELD function is called from a signal handler.
 * Needed for garbage collection -- Look at PR_Suspend/PR_Resume
 * implementation.
 */
#if defined(_PR_DCETHREADS)
#define PTHREAD_YIELD()            	pthread_yield()
#elif defined(OSF1)
/*
 * sched_yield can't be called from a signal handler.  Must use
 * the _np version.
 */
#define PTHREAD_YIELD()            	pthread_yield_np()
#elif defined(AIX)
extern int (*_PT_aix_yield_fcn)();
#define PTHREAD_YIELD()			(*_PT_aix_yield_fcn)()
#elif defined(IRIX)
#include <time.h>
#define PTHREAD_YIELD() \
    PR_BEGIN_MACRO               				\
		struct timespec onemillisec = {0};		\
		onemillisec.tv_nsec = 1000000L;			\
        nanosleep(&onemillisec,NULL);			\
    PR_END_MACRO
#elif defined(HPUX) || defined(LINUX) || defined(SOLARIS) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI)
#define PTHREAD_YIELD()            	sched_yield()
#else
#error "Need to define PTHREAD_YIELD for this platform"
#endif

#endif /* nspr_pth_defs_h_ */
