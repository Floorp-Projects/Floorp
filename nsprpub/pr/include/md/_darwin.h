/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_darwin_defs_h___
#define nspr_darwin_defs_h___

#include "prthread.h"

#include <libkern/OSAtomic.h>
#include <sys/syscall.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

#define PR_LINKER_ARCH  "darwin"
#define _PR_SI_SYSNAME  "DARWIN"
#ifdef __i386__
#define _PR_SI_ARCHITECTURE "x86"
#elif defined(__x86_64__)
#define _PR_SI_ARCHITECTURE "x86-64"
#elif defined(__ppc__)
#define _PR_SI_ARCHITECTURE "ppc"
#elif defined(__arm__)
#define _PR_SI_ARCHITECTURE "arm"
#elif defined(__aarch64__)
#define _PR_SI_ARCHITECTURE "aarch64"
#else
#error "Unknown CPU architecture"
#endif
#define PR_DLL_SUFFIX       ".dylib"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE    0x50000000
#define _MD_DEFAULT_STACK_SIZE  65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef  HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define USE_DLFCN
#define _PR_HAVE_SOCKADDR_LEN
#define _PR_STAT_HAS_ST_ATIMESPEC
#define _PR_HAVE_LARGE_OFF_T
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY

#define _PR_INET6
/*
 * I'd prefer to use getipnodebyname and getipnodebyaddr but the
 * getipnodebyname(3) man page on Mac OS X 10.2 says they are not
 * thread-safe.  AI_V4MAPPED|AI_ADDRCONFIG doesn't work either.
 */
#define _PR_HAVE_GETHOSTBYNAME2
#define _PR_HAVE_GETADDRINFO
/*
 * On Mac OS X 10.2, gethostbyaddr fails with h_errno=NO_RECOVERY
 * if you pass an IPv4-mapped IPv6 address to it.
 */
#define _PR_GHBA_DISALLOW_V4MAPPED
#ifdef __APPLE__
#if !defined(MAC_OS_X_VERSION_10_3) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
/*
 * socket(AF_INET6) fails with EPROTONOSUPPORT on Mac OS X 10.1.
 * IPv6 under OS X 10.2 and below is not complete (see bug 222031).
 */
#define _PR_INET6_PROBE
#endif /* DT < 10.3 */
#if defined(MAC_OS_X_VERSION_10_2) && \
    MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_2
/* Mac OS X 10.2 has inet_ntop and inet_pton. */
#define _PR_HAVE_INET_NTOP
#endif /* DT >= 10.2 */
#endif /* __APPLE__ */
#define _PR_IPV6_V6ONLY_PROBE
/* The IPV6_V6ONLY socket option is not defined on Mac OS X 10.1. */
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif

#ifdef __ppc__
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_DarwinPPC_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT(val)   _PR_DarwinPPC_AtomicIncrement(val)
extern PRInt32 _PR_DarwinPPC_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT(val)   _PR_DarwinPPC_AtomicDecrement(val)
extern PRInt32 _PR_DarwinPPC_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET(val, newval) _PR_DarwinPPC_AtomicSet(val, newval)
extern PRInt32 _PR_DarwinPPC_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD(ptr, val)    _PR_DarwinPPC_AtomicAdd(ptr, val)
#endif /* __ppc__ */

#ifdef __i386__
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_Darwin_x86_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT(val)   _PR_Darwin_x86_AtomicIncrement(val)
extern PRInt32 _PR_Darwin_x86_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT(val)   _PR_Darwin_x86_AtomicDecrement(val)
extern PRInt32 _PR_Darwin_x86_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET(val, newval) _PR_Darwin_x86_AtomicSet(val, newval)
extern PRInt32 _PR_Darwin_x86_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD(ptr, val)    _PR_Darwin_x86_AtomicAdd(ptr, val)
#endif /* __i386__ */

#ifdef __x86_64__
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
extern PRInt32 _PR_Darwin_x86_64_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT(val)   _PR_Darwin_x86_64_AtomicIncrement(val)
extern PRInt32 _PR_Darwin_x86_64_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT(val)   _PR_Darwin_x86_64_AtomicDecrement(val)
extern PRInt32 _PR_Darwin_x86_64_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET(val, newval) _PR_Darwin_x86_64_AtomicSet(val, newval)
extern PRInt32 _PR_Darwin_x86_64_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD(ptr, val)    _PR_Darwin_x86_64_AtomicAdd(ptr, val)
#endif /* __x86_64__ */

#ifdef __aarch64__
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val)   __sync_add_and_fetch(val, 1)
#define _MD_ATOMIC_DECREMENT(val)   __sync_sub_and_fetch(val, 1)
#define _MD_ATOMIC_SET(val, newval) __sync_lock_test_and_set(val, newval)
#define _MD_ATOMIC_ADD(ptr, val)    __sync_add_and_fetch(ptr, val)
#endif /* __aarch64__ */

#if defined(__arm__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val)   OSAtomicIncrement32(val)
#define _MD_ATOMIC_DECREMENT(val)   OSAtomicDecrement32(val)
static inline PRInt32 _MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
{
    PRInt32 oldval;
    do {
        oldval = *val;
    } while (!OSAtomicCompareAndSwap32(oldval, newval, val));
    return oldval;
}
#define _MD_ATOMIC_ADD(ptr, val)    OSAtomicAdd32(val, ptr)
#endif /* __arm__ */

#define USE_SETJMP

#if !defined(_PR_PTHREADS)

#include <setjmp.h>

#define PR_CONTEXT_TYPE jmp_buf

#define CONTEXT(_th)       ((_th)->md.context)
#define _MD_GET_SP(_th)    (((struct sigcontext *) (_th)->md.context)->sc_onstack)
#define PR_NUM_GCREGS       _JBLEN

/*
** Initialize a thread context to run "_main()" when started
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    if (setjmp(CONTEXT(_thread))) {  \
        _main();  \
    }  \
    _MD_GET_SP(_thread) = (unsigned char*) ((_sp) - 64); \
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!setjmp(CONTEXT(_thread))) {  \
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
    longjmp(CONTEXT(_thread), 1);  \
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
#endif  /* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)           ((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)       ((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)       ((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)      ((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)      ((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)  ((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)  ((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)       ((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)      ((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)      ((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)       ((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)  ((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)  32

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

extern PRStatus _MD_InitializeThread(PRThread *thread);

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#define _MD_CLEAN_THREAD(_thread)

extern PRStatus _MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize);
extern void _MD_SET_PRIORITY(struct _MDThread *thread, PRUintn newPri);
extern PRStatus _MD_WAIT(PRThread *, PRIntervalTime timeout);
extern PRStatus _MD_WAKEUP_WAITER(PRThread *);
extern void _MD_YIELD(void);

#endif /* ! _PR_PTHREADS */

#define _MD_EARLY_INIT          _MD_EarlyInit
#define _MD_FINAL_INIT          _PR_UnixInit
#define _MD_INTERVAL_INIT       _PR_Mach_IntervalInit
#define _MD_GET_INTERVAL        _PR_Mach_GetInterval
#define _MD_INTERVAL_PER_SEC    _PR_Mach_TicksPerSecond

extern void             _MD_EarlyInit(void);
extern void             _PR_Mach_IntervalInit(void);
extern PRIntervalTime   _PR_Mach_GetInterval(void);
extern PRIntervalTime   _PR_Mach_TicksPerSecond(void);

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#define _MD_SELECT(nfds,r,w,e,tv) syscall(SYS_select,nfds,r,w,e,tv)

/* For writev() */
#include <sys/uio.h>

#endif /* nspr_darwin_defs_h___ */
