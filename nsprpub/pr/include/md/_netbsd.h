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

#ifndef nspr_netbsd_defs_h___
#define nspr_netbsd_defs_h___

#include <sys/syscall.h>
#include <sys/param.h>  /* for __NetBSD_Version__ */

#define PR_LINKER_ARCH	"netbsd"
#define _PR_SI_SYSNAME  "NetBSD"
#if defined(__i386__)
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__alpha__)
#define _PR_SI_ARCHITECTURE "alpha"
#elif defined(__m68k__)
#define _PR_SI_ARCHITECTURE "m68k"
#elif defined(__powerpc__)
#define _PR_SI_ARCHITECTURE "powerpc"
#elif defined(__sparc__)
#define _PR_SI_ARCHITECTURE "sparc"
#elif defined(__mips__)
#define _PR_SI_ARCHITECTURE "mips"
#elif defined(__arm32__)
#define _PR_SI_ARCHITECTURE "arm32"
#endif

#if defined(__ELF__)
#define PR_DLL_SUFFIX		".so"
#else
#define PR_DLL_SUFFIX		".so.1.0"
#endif

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define USE_DLFCN
#define _PR_HAVE_SOCKADDR_LEN
#define _PR_NO_LARGE_FILES
#define _PR_STAT_HAS_ST_ATIMESPEC
#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY

#if __NetBSD_Version__ >= 105000000
#define _PR_INET6
#define _PR_HAVE_INET_NTOP
#define _PR_HAVE_GETHOSTBYNAME2
#define _PR_HAVE_GETADDRINFO
#define _PR_INET6_PROBE
#endif

#define USE_SETJMP

#ifndef _PR_PTHREADS
#include <setjmp.h>

#define PR_CONTEXT_TYPE	sigjmp_buf

#define CONTEXT(_th) ((_th)->md.context)

/*
** Initialize a thread context to run "_main()" when started
*/
#ifdef __i386__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[2] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[0] = (int) _main;					\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[2]
#endif
#ifdef __sparc__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[2] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[3] = (int) _main;					\
    CONTEXT(_thread)[4] = (int) _main + 4;				\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[2]
#endif
#ifdef __powerpc__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[3] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[4] = (int) _main;					\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[3]
#endif
#ifdef __m68k__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[2] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[5] = (int) _main;					\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[2]
#endif
#ifdef __mips__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[32] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[2] = (int) _main;					\
    CONTEXT(_thread)[28] = (int) _main;					\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[32]
#endif
#ifdef __arm32__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[23] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[24] = (int) _main;					\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[23]
#endif
#ifdef __alpha__
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
{									\
    sigsetjmp(CONTEXT(_thread), 1);					\
    CONTEXT(_thread)[34] = (unsigned char*) ((_sp) - 128);		\
    CONTEXT(_thread)[2] = (long) _main;					\
    CONTEXT(_thread)[30] = (long) _main;				\
    CONTEXT(_thread)[31] = (long) _main;				\
    *status = PR_TRUE;							\
}
#define	_MD_GET_SP(_thread)	CONTEXT(_thread)[34]
#endif
#ifndef _MD_INIT_CONTEXT
#error "Need to define _MD_INIT_CONTEXT for this platform"
#endif

#define PR_NUM_GCREGS	_JBLEN

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!sigsetjmp(CONTEXT(_thread), 1)) {  \
        (_thread)->md.errcode = errno;  \
        _PR_Schedule();  \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{   \
    errno = (_thread)->md.errcode;  \
    _MD_SET_CURRENT_THREAD(_thread);  \
    siglongjmp(CONTEXT(_thread), 1);  \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    PR_CONTEXT_TYPE context;
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

#endif /* ! _PR_PTHREADS */

#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT			_PR_UnixInit
#define _MD_GET_INTERVAL                  _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC              _PR_UNIX_TicksPerSecond

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)
#if defined(_PR_POLL_AVAILABLE)
#include <poll.h>
#define _MD_POLL(fds,nfds,timeout) syscall(SYS_poll,fds,nfds,timeout)
#endif

#if NetBSD1_3 == 1L
typedef unsigned int nfds_t;
#endif

#endif /* nspr_netbsd_defs_h___ */
