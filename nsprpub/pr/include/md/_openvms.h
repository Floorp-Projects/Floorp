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

/*
** This is the OpenVMS machine dependant configuration file. It is based
** on the OSF/1 machine dependant file.
*/

#ifndef nspr_openvms_defs_h___
#define nspr_openvms_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"OpenVMS"
#define _PR_SI_SYSNAME	"OpenVMS"
#ifdef __alpha
#define _PR_SI_ARCHITECTURE "alpha"
#else
#define _PR_SI_ARCHITECTURE "vax"
#endif
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE		0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	131072L

/*
** This is not defined on OpenVMS. I believe its only used in GC code, and
** isn't that only used in Java? Anyway, for now, let's keep the compiler
** happy.
*/
#define SA_RESTART 0

/*
** OpenVMS doesn't have these in socket.h.
** Does in later versions!
*/
#if 0
struct ip_mreq {
    struct in_addr  imr_multiaddr;      /* IP multicast address of group */
    struct in_addr  imr_interface;      /* local IP address of interface */
};
#endif

/*
 * OSF1 needs the MAP_FIXED flag to ensure that mmap returns a pointer
 * with the upper 32 bits zero.  This is because Java sticks a pointer
 * into an int.
 */
#define _MD_MMAP_FLAGS          MAP_PRIVATE|MAP_FIXED

#undef  HAVE_STACK_GROWING_UP
#undef 	HAVE_WEAK_IO_SYMBOLS
#undef 	HAVE_WEAK_MALLOC_SYMBOLS
#undef  HAVE_BSD_FLOCK

#define NEED_TIME_R

#define HAVE_DLL
#define USE_DLFCN

#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_STAT_HAS_ONLY_ST_ATIME
#define _PR_NO_LARGE_FILES

/* IPv6 support */
#define _PR_HAVE_GETIPNODEBYNAME
#define _PR_HAVE_GETIPNODEBYADDR
#define _PR_INET6_PROBE
#ifndef _PR_INET6
#define AF_INET6 26
#define AI_V4MAPPED 0x00000010
#define AI_ALL      0x00000008
#define AI_ADDRCONFIG 0x00000020
#endif

#undef  USE_SETJMP

#include <setjmp.h>

/*
 * A jmp_buf is actually a struct sigcontext.  The sc_sp field of
 * struct sigcontext is the stack pointer.
 */
#define _MD_GET_SP(_t) (((struct sigcontext *) (_t)->md.context)->sc_sp)
#define PR_NUM_GCREGS _JBLEN
#define CONTEXT(_th) ((_th)->md.context)

/*
** I am ifdef'ing these out because that's the way they are in FT.
*/
#ifndef __VMS

/*
** Initialize a thread context to run "_main()" when started
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
{									  \
        *status = PR_TRUE;              \
    if (setjmp(CONTEXT(_thread))) {				\
	(*_main)();						\
    }								\
    _MD_GET_SP(_thread) = (long) ((_sp) - 64);			\
    _MD_GET_SP(_thread) &= ~15;					\
}

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
    errno = (_thread)->md.errcode;     \
    _MD_SET_CURRENT_THREAD(_thread);	\
    longjmp(CONTEXT(_thread), 1);    \
}

#endif /* __VMS */

/* Machine-dependent (MD) data structures */

struct _MDThread {
    jmp_buf context;
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
#endif        /* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)                 ((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)         ((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)         ((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)                ((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)                ((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)    ((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)    ((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)         ((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)                ((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)                ((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)         ((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)    ((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)        32

struct _MDCPU {
	struct _MDCPU_Unix md_unix;
};

#ifndef _PR_PTHREADS
#define _MD_INIT_LOCKS()
#endif
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

/*
 * The following are copied from _sunos.h, _aix.h.  This means
 * some of them should probably be moved into _unixos.h.  But
 * _irix.h seems to be quite different in regard to these macros.
 */
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
extern PRIntervalTime _PR_UNIX_GetInterval(void);
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond
extern PRIntervalTime _PR_UNIX_TicksPerSecond(void);

#define _MD_EARLY_INIT		_MD_EarlyInit
void _MD_EarlyInit(void);
#define _MD_FINAL_INIT		_PR_UnixInit
#define _MD_INIT_RUNNING_CPU(cpu) _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD         _MD_InitializeThread
#ifdef _VMS_NOT_YET
NSPR_API(void) _PR_InitThreads(
  PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs);
#endif
#define _MD_EXIT_THREAD(thread)
#define	_MD_SUSPEND_THREAD(thread)
#define	_MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

/* The following defines unwrapped versions of select() and poll(). */
extern int __select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
#define _MD_SELECT              __select

#ifndef __VMS
#define _MD_POLL __poll
extern int __poll(struct pollfd filedes[], unsigned int nfds, int timeout);
#endif

#ifdef __VMS
NSPR_API(void) _PR_InitCPUs(void);
NSPR_API(void) _PR_MD_START_INTERRUPTS(void);
#endif

/*
 * Atomic operations
 */
#include <machine/builtins.h>
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_ADD(ptr,val) (__ATOMIC_ADD_LONG(ptr,val) + val)
#define _MD_ATOMIC_INCREMENT(val) (__ATOMIC_INCREMENT_LONG(val) + 1)
#define _MD_ATOMIC_DECREMENT(val) (__ATOMIC_DECREMENT_LONG(val) - 1)
#define _MD_ATOMIC_SET(val, newval) __ATOMIC_EXCH_LONG(val, newval)

extern int thread_suspend(PRThread *thr_id);
extern int thread_resume(PRThread *thr_id);

#endif /* nspr_openvms_defs_h___ */
