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

#ifndef nspr_xhppa_defs_h___
#define nspr_xhppa_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH    "hpux"
#define _PR_SI_SYSNAME   "HPUX"
#define _PR_SI_ARCHITECTURE "hppa1.1"
#define PR_DLL_SUFFIX        ".sl"

#define _PR_VMBASE        0x30000000 
#define _PR_STACK_VMBASE    0x50000000
#define _MD_DEFAULT_STACK_SIZE    65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#define NEED_TIME_R

#define HAVE_STACK_GROWING_UP
#undef	HAVE_WEAK_IO_SYMBOLS
#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define USE_HPSHL
#ifndef HAVE_STRERROR
#define HAVE_STRERROR
#endif
#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_STAT_HAS_ONLY_ST_ATIME

#undef _PR_HAVE_ATOMIC_OPS

#if !defined(_PR_PTHREADS)

#include <syscall.h>
#include <setjmp.h>

#define USE_SETJMP

#define _MD_GET_SP(_t) (*((int *)((_t)->md.jb) + 1))
#define PR_NUM_GCREGS _JBLEN
/* Caveat: This makes jmp_buf full of doubles. */
#define CONTEXT(_th) ((_th)->md.jb)

    /* Stack needs two frames (64 bytes) at the bottom */ \
#define _MD_SET_THR_SP(_t, _sp)     ((_MD_GET_SP(_t)) = (int) (_sp + 64 *2))
#define SAVE_CONTEXT(_th)           _setjmp(CONTEXT(_th))
#define GOTO_CONTEXT(_th)           _longjmp(CONTEXT(_th), 1)

#if !defined(PTHREADS_USER)

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
{ \
    *(status) = PR_TRUE; \
    if (_setjmp(CONTEXT(_thread))) (*_main)(); \
    /* Stack needs two frames (64 bytes) at the bottom */ \
    (_MD_GET_SP(_thread)) = (int) ((_sp) + 64*2); \
}

#define _MD_SWITCH_CONTEXT(_thread) \
    if (!_setjmp(CONTEXT(_thread))) { \
    (_thread)->md.errcode = errno; \
    _PR_Schedule(); \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{ \
    errno = (_thread)->md.errcode; \
    _MD_SET_CURRENT_THREAD(_thread); \
    _longjmp(CONTEXT(_thread), 1); \
}

/* Machine-dependent (MD) data structures.  HP-UX has no native threads. */

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

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#define _MD_CLEAN_THREAD(_thread)

#else /* PTHREADS_USER	*/

#include "_nspr_pthread.h"

#endif /* PTHREADS_USER	*/

#endif  /* !defined(_PR_PTHREADS) */

#if !defined(PTHREADS_USER)
#define _MD_EARLY_INIT                 	_MD_EarlyInit
#define _MD_FINAL_INIT					_PR_UnixInit
#endif 

#if defined(HPUX_LW_TIMER)
extern void _PR_HPUX_LW_IntervalInit(void);
extern PRIntervalTime _PR_HPUX_LW_GetInterval(void);
#define _MD_INTERVAL_INIT                 _PR_HPUX_LW_IntervalInit
#define _MD_GET_INTERVAL                  _PR_HPUX_LW_GetInterval
#define _MD_INTERVAL_PER_SEC()            1000
#else
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond
#endif

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)

#include <poll.h>
#define _MD_POLL(fds,nfds,timeout) syscall(SYS_poll,fds,nfds,timeout)

#ifdef HPUX11
extern void _MD_hpux_map_sendfile_error(int err);
#if !defined(_PR_PTHREADS)
extern PRInt32 _PR_HPUXTransmitFile(PRFileDesc *sd, PRFileDesc *fd,
        const void *headers, PRInt32 hlen,
        PRTransmitFileFlags flags, PRIntervalTime timeout);
#endif /* !_PR_PTHREADS */
#endif /* HPUX11 */

#endif /* nspr_xhppa_defs_h___ */
