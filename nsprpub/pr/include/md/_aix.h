/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_aix_defs_h___
#define nspr_aix_defs_h___

#include <sys/types.h>
#if defined(_PR_PTHREADS) || defined(PTHREADS_USER)
#include <pthread.h>
#endif

/*
 * To pick up fd_set and the poll events.
 */
#include <sys/select.h>
#include <sys/poll.h>

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH          "aix"
#define _PR_SI_SYSNAME		    "AIX"
#define _PR_SI_ARCHITECTURE	    "rs6000"
#define PR_DLL_SUFFIX		    ".so"

#define _PR_VMBASE	 	        0x30000000
#define _PR_STACK_VMBASE	    0x50000000
#define _MD_DEFAULT_STACK_SIZE	(2*65536L)
#define _MD_MINIMUM_STACK_SIZE	(2*65536L)
#define _MD_MMAP_FLAGS		    MAP_PRIVATE

#define NEED_TIME_R
#undef  HAVE_STACK_GROWING_UP
#undef	HAVE_WEAK_IO_SYMBOLS
#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define	USE_DLFCN
#define _PR_HAVE_SOCKADDR_LEN
#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_STAT_HAS_ONLY_ST_ATIME
#ifdef _PR_INET6
#define _PR_HAVE_INET_NTOP
#define _PR_HAVE_GETHOSTBYNAME2
#ifdef _AIX51 /* AIX 4.3.3 does not have AI_NUMERICHOST. */
#define _PR_HAVE_GETADDRINFO
#endif
#endif
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#define _PR_ACCEPT_INHERIT_NONBLOCK

/* Timer operations */
#if defined(AIX_TIMERS)
#define _MD_INTERVAL_INIT()

extern PRIntervalTime _MD_AixGetInterval(void);
#define _MD_GET_INTERVAL _MD_AixGetInterval

extern PRIntervalTime _MD_AixIntervalPerSec(void);
#define _MD_INTERVAL_PER_SEC _MD_AixIntervalPerSec

#else  /* defined(AIX_TIMERS) */
#define _MD_INTERVAL_USE_GTOD
#endif  /* defined(AIX_TIMERS) */

#ifdef AIX_HAVE_ATOMIC_OP_H
/* The atomic operations */
#include <sys/atomic_op.h>
#define _PR_HAVE_ATOMIC_OPS
#ifndef IS_64
#define _PR_HAVE_ATOMIC_CAS
#endif
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val)   ((PRInt32)fetch_and_add((atomic_p)val, 1) + 1)
#define _MD_ATOMIC_ADD(ptr, val)   ((PRInt32)fetch_and_add((atomic_p)ptr, val) + val)
#define _MD_ATOMIC_DECREMENT(val)   ((PRInt32)fetch_and_add((atomic_p)val, -1) - 1)
#define _MD_ATOMIC_SET(val, newval) _AIX_AtomicSet(val, newval)
#endif /* AIX_HAVE_ATOMIC_OP_H */

#define USE_SETJMP

#include <setjmp.h>

#define _MD_GET_SP(_t)				(_t)->md.jb[3]
#define _MD_SET_THR_SP(_t, _sp)		((_t)->md.jb[3] = (int) (_sp - 2 * 64))
#define PR_NUM_GCREGS				_JBLEN

#define CONTEXT(_th) 				((_th)->md.jb)
#define SAVE_CONTEXT(_th)			_setjmp(CONTEXT(_th))
#define GOTO_CONTEXT(_th)			_longjmp(CONTEXT(_th), 1)

#ifdef PTHREADS_USER
#include "_nspr_pthread.h"
#else

/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
    PR_BEGIN_MACRO				      \
        *status = PR_TRUE;              \
	if (setjmp(CONTEXT(_thread))) {	\
	    (*_main)();			\
	}				\
	_MD_GET_SP(_thread) = (int) (_sp - 2 * 64);		\
    PR_END_MACRO

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!setjmp(CONTEXT(_thread))) { \
	(_thread)->md.errcode = errno;  \
	_PR_Schedule();		     \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{				     \
    errno = (_thread)->md.errcode; \
    _MD_SET_CURRENT_THREAD(_thread); \
    longjmp(CONTEXT(_thread), 1); \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    jmp_buf jb;
    int id;
    int errcode;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDLock {
    PRInt8 notused;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDSegment {
    PRInt8 notused;
};

/*
 * md-specific cpu structure field
 */
#define _PR_MD_MAX_OSFD FD_SETSIZE

struct _MDCPU_Unix {
    PRCList ioQ;
    PRUint32 ioq_timeout;
    PRInt32 ioq_max_osfd;
    PRInt32 ioq_osfd_cnt;
#ifndef _PR_USE_POLL
    fd_set fd_read_set, fd_write_set, fd_exception_set;
    PRInt16 fd_read_cnt[_PR_MD_MAX_OSFD],fd_write_cnt[_PR_MD_MAX_OSFD],
				fd_exception_cnt[_PR_MD_MAX_OSFD];
#else
	struct pollfd *ioq_pollfds;
	int ioq_pollfds_size;
#endif	/* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)			((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)		((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)		((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)		((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)		((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)	((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)	((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)		((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)		((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)		((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)		((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)	((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)	32

struct _MDCPU {
    struct _MDCPU_Unix md_unix;
};

#if !defined(_PR_PTHREADS)
#define _MD_INIT_LOCKS()
#endif

#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_EARLY_INIT          	_MD_EarlyInit
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu)	_MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD			_MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)
#endif /* PTHREADS_USER */

#ifdef AIX_RENAME_SELECT
#define _MD_SELECT	select
#define _MD_POLL	poll
#endif

extern void _MD_aix_map_sendfile_error(int err);

#endif /* nspr_aix_defs_h___ */
