/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_solaris_defs_h___
#define nspr_solaris_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH  "solaris"
#define _PR_SI_SYSNAME  "SOLARIS"
#ifdef sparc
#define _PR_SI_ARCHITECTURE "sparc"
#elif defined(__x86_64)
#define _PR_SI_ARCHITECTURE "x86-64"
#elif defined(i386)
#define _PR_SI_ARCHITECTURE "x86"
#else
#error unknown processor
#endif
#define PR_DLL_SUFFIX       ".so"

#define _PR_VMBASE      0x30000000
#define _PR_STACK_VMBASE    0x50000000
#define _MD_DEFAULT_STACK_SIZE  (2*65536L)
#define _MD_MMAP_FLAGS          MAP_SHARED

#undef  HAVE_STACK_GROWING_UP

#ifndef HAVE_WEAK_IO_SYMBOLS
#define HAVE_WEAK_IO_SYMBOLS
#endif

#undef  HAVE_WEAK_MALLOC_SYMBOLS
#define HAVE_DLL
#define USE_DLFCN
#define NEED_STRFTIME_LOCK

/*
 * Intel x86 has atomic instructions.
 *
 * Sparc v8 does not have instructions to efficiently implement
 * atomic increment/decrement operations.  We use the default
 * atomic routine implementation in pratom.c.
 *
 * 64-bit Solaris requires sparc v9, which has atomic instructions.
 */
#if defined(i386) || defined(IS_64)
#define _PR_HAVE_ATOMIC_OPS
#endif

#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_STAT_HAS_ST_ATIM
#ifdef SOLARIS2_5
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#else
#define _PR_HAVE_POSIX_SEMAPHORES
#define PR_HAVE_POSIX_NAMED_SHARED_MEMORY
#endif
#define _PR_HAVE_GETIPNODEBYNAME
#define _PR_HAVE_GETIPNODEBYADDR
#define _PR_HAVE_GETADDRINFO
#define _PR_INET6_PROBE
#define _PR_ACCEPT_INHERIT_NONBLOCK
#ifdef _PR_INET6
#define _PR_HAVE_INET_NTOP
#else
#define AF_INET6 26
struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
};
#define AI_CANONNAME 0x0010
#define AI_V4MAPPED 0x0001
#define AI_ALL      0x0002
#define AI_ADDRCONFIG   0x0004
#define _PR_HAVE_MD_SOCKADDR_IN6
/* isomorphic to struct in6_addr on Solaris 8 */
struct _md_in6_addr {
    union {
        PRUint8  _S6_u8[16];
        PRUint32 _S6_u32[4];
        PRUint32 __S6_align;
    } _S6_un;
};
/* isomorphic to struct sockaddr_in6 on Solaris 8 */
struct _md_sockaddr_in6 {
    PRUint16 sin6_family;
    PRUint16 sin6_port;
    PRUint32 sin6_flowinfo;
    struct _md_in6_addr sin6_addr;
    PRUint32 sin6_scope_id;
    PRUint32 __sin6_src_id;
};
#endif
#if defined(_PR_PTHREADS)
#define _PR_HAVE_GETHOST_R
#define _PR_HAVE_GETHOST_R_POINTER
#endif

#include "prinrval.h"
#define _MD_INTERVAL_INIT()
NSPR_API(PRIntervalTime) _MD_Solaris_GetInterval(void);
#define _MD_GET_INTERVAL                  _MD_Solaris_GetInterval
NSPR_API(PRIntervalTime) _MD_Solaris_TicksPerSecond(void);
#define _MD_INTERVAL_PER_SEC              _MD_Solaris_TicksPerSecond

#if defined(_PR_HAVE_ATOMIC_OPS)
/*
** Atomic Operations
*/
#define _MD_INIT_ATOMIC()

NSPR_API(PRInt32) _MD_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT _MD_AtomicIncrement

NSPR_API(PRInt32) _MD_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#define _MD_ATOMIC_ADD _MD_AtomicAdd

NSPR_API(PRInt32) _MD_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT _MD_AtomicDecrement

NSPR_API(PRInt32) _MD_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET _MD_AtomicSet
#endif /* _PR_HAVE_ATOMIC_OPS */

#if defined(_PR_PTHREADS)

NSPR_API(void)      _MD_EarlyInit(void);

#define _MD_EARLY_INIT      _MD_EarlyInit
#define _MD_FINAL_INIT      _PR_UnixInit

#else /* _PR_PTHREADS */

/*
 * _PR_LOCAL_THREADS_ONLY implementation on Solaris
 */

#include "prthread.h"

#include <errno.h>
#include <ucontext.h>
#include <sys/stack.h>
#include <synch.h>

/*
** Initialization Related definitions
*/

NSPR_API(void)              _MD_EarlyInit(void);
NSPR_API(void)              _MD_SolarisInit();
#define _MD_EARLY_INIT      _MD_EarlyInit
#define _MD_FINAL_INIT      _MD_SolarisInit
#define _MD_INIT_THREAD     _MD_InitializeThread

#ifdef USE_SETJMP

#include <setjmp.h>

#define _PR_CONTEXT_TYPE    jmp_buf

#ifdef sparc
#define _MD_GET_SP(_t)      (_t)->md.context[2]
#else
#define _MD_GET_SP(_t)      (_t)->md.context[4]
#endif

#define PR_NUM_GCREGS       _JBLEN
#define CONTEXT(_thread)    (_thread)->md.context

#else  /* ! USE_SETJMP */

#ifdef sparc
#define _PR_CONTEXT_TYPE    ucontext_t
#define _MD_GET_SP(_t)      (_t)->md.context.uc_mcontext.gregs[REG_SP]
/*
** Sparc's use register windows. the _MD_GetRegisters for the sparc's
** doesn't actually store anything into the argument buffer; instead the
** register windows are homed to the stack. I assume that the stack
** always has room for the registers to spill to...
*/
#define PR_NUM_GCREGS       0
#else
#define _PR_CONTEXT_TYPE    unsigned int edi; sigset_t oldMask, blockMask; ucontext_t
#define _MD_GET_SP(_t)      (_t)->md.context.uc_mcontext.gregs[USP]
#define PR_NUM_GCREGS       _JBLEN
#endif

#define CONTEXT(_thread)    (&(_thread)->md.context)

#endif /* ! USE_SETJMP */

#include <time.h>
/*
 * Because clock_gettime() on Solaris/x86 always generates a
 * segmentation fault, we use an emulated version _pr_solx86_clock_gettime(),
 * which is implemented using gettimeofday().
 */
#ifdef i386
#define GETTIME(tt) _pr_solx86_clock_gettime(CLOCK_REALTIME, (tt))
#else
#define GETTIME(tt) clock_gettime(CLOCK_REALTIME, (tt))
#endif  /* i386 */

#define _MD_SAVE_ERRNO(_thread)         (_thread)->md.errcode = errno;
#define _MD_RESTORE_ERRNO(_thread)      errno = (_thread)->md.errcode;

#ifdef sparc

#ifdef USE_SETJMP
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)         \
    PR_BEGIN_MACRO                    \
    int *context = (_thread)->md.context;         \
    *status = PR_TRUE;              \
    (void) setjmp(context);               \
    (_thread)->md.context[1] = (int) ((_sp) - 64); \
    (_thread)->md.context[2] = (int) _main;       \
    (_thread)->md.context[3] = (int) _main + 4; \
    _thread->no_sched = 0; \
    PR_END_MACRO

#define _MD_SWITCH_CONTEXT(_thread)    \
    if (!setjmp(CONTEXT(_thread))) { \
    _MD_SAVE_ERRNO(_thread)    \
    _MD_SET_LAST_THREAD(_thread);    \
    _MD_SET_CURRENT_THREAD(_thread);     \
    _PR_Schedule();          \
    }

#define _MD_RESTORE_CONTEXT(_newThread)     \
{                    \
    _MD_RESTORE_ERRNO(_newThread)       \
    _MD_SET_CURRENT_THREAD(_newThread); \
    longjmp(CONTEXT(_newThread), 1);    \
}

#else
/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)                   \
    PR_BEGIN_MACRO                                                      \
    ucontext_t *uc = CONTEXT(_thread);                                  \
    *status = PR_TRUE;                                                  \
    getcontext(uc);                                                     \
    uc->uc_stack.ss_sp = (char *) ((unsigned long)(_sp - WINDOWSIZE - SA(MINFRAME)) & 0xfffffff8);  \
    uc->uc_stack.ss_size = _thread->stack->stackSize;                   \
    uc->uc_stack.ss_flags = 0;              /* ? */                     \
    uc->uc_mcontext.gregs[REG_SP] = (unsigned int) uc->uc_stack.ss_sp;  \
    uc->uc_mcontext.gregs[REG_PC] = (unsigned int) _main;               \
    uc->uc_mcontext.gregs[REG_nPC] = (unsigned int) ((char*)_main)+4;   \
    uc->uc_flags = UC_ALL;                                              \
    _thread->no_sched = 0;                                              \
    PR_END_MACRO

/*
** Switch away from the current thread context by saving its state and
** calling the thread scheduler. Reload cpu when we come back from the
** context switch because it might have changed.
*/
#define _MD_SWITCH_CONTEXT(_thread)                 \
    PR_BEGIN_MACRO                                  \
        if (!getcontext(CONTEXT(_thread))) {        \
            _MD_SAVE_ERRNO(_thread);                \
            _MD_SET_LAST_THREAD(_thread);           \
            _PR_Schedule();                         \
        }                                           \
    PR_END_MACRO

/*
** Restore a thread context that was saved by _MD_SWITCH_CONTEXT or
** initialized by _MD_INIT_CONTEXT.
*/
#define _MD_RESTORE_CONTEXT(_newThread)                     \
    PR_BEGIN_MACRO                                          \
        ucontext_t *uc = CONTEXT(_newThread);               \
        uc->uc_mcontext.gregs[11] = 1;                      \
        _MD_RESTORE_ERRNO(_newThread);                      \
        _MD_SET_CURRENT_THREAD(_newThread);                 \
        setcontext(uc);                                     \
    PR_END_MACRO
#endif

#else  /* x86 solaris */

#ifdef USE_SETJMP

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
    PR_BEGIN_MACRO \
    *status = PR_TRUE; \
    if (setjmp(CONTEXT(_thread))) _main(); \
    _MD_GET_SP(_thread) = (int) ((_sp) - 64); \
    PR_END_MACRO

#define _MD_SWITCH_CONTEXT(_thread) \
    if (!setjmp(CONTEXT(_thread))) { \
        _MD_SAVE_ERRNO(_thread) \
        _PR_Schedule(); \
    }

#define _MD_RESTORE_CONTEXT(_newThread) \
{ \
    _MD_RESTORE_ERRNO(_newThread) \
    _MD_SET_CURRENT_THREAD(_newThread); \
    longjmp(CONTEXT(_newThread), 1); \
}

#else /* USE_SETJMP */

#define WINDOWSIZE      0

int getedi(void);
void setedi(int);

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)         \
    PR_BEGIN_MACRO                  \
    ucontext_t *uc = CONTEXT(_thread);      \
        *status = PR_TRUE;              \
    getcontext(uc);                 \
    /* Force sp to be double aligned! */        \
        uc->uc_mcontext.gregs[USP] = (int) ((unsigned long)(_sp - WINDOWSIZE - SA(MINFRAME)) & 0xfffffff8); \
    uc->uc_mcontext.gregs[PC] = (int) _main;    \
    (_thread)->no_sched = 0; \
    PR_END_MACRO

/* getcontext() may return 1, contrary to what the man page says */
#define _MD_SWITCH_CONTEXT(_thread)         \
    PR_BEGIN_MACRO                  \
    ucontext_t *uc = CONTEXT(_thread);      \
    PR_ASSERT(_thread->no_sched);           \
    sigfillset(&((_thread)->md.blockMask));     \
    sigprocmask(SIG_BLOCK, &((_thread)->md.blockMask),  \
        &((_thread)->md.oldMask));      \
    (_thread)->md.edi = getedi();           \
    if (! getcontext(uc)) {             \
        sigprocmask(SIG_SETMASK, &((_thread)->md.oldMask), NULL); \
        uc->uc_mcontext.gregs[EDI] = (_thread)->md.edi; \
        _MD_SAVE_ERRNO(_thread)         \
            _MD_SET_LAST_THREAD(_thread);           \
        _PR_Schedule();             \
    } else {                    \
        sigprocmask(SIG_SETMASK, &((_thread)->md.oldMask), NULL); \
        setedi((_thread)->md.edi);      \
        PR_ASSERT(_MD_LAST_THREAD() !=_MD_CURRENT_THREAD()); \
        _MD_LAST_THREAD()->no_sched = 0;    \
    }                       \
    PR_END_MACRO

/*
** Restore a thread context, saved by _PR_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_newthread)         \
    PR_BEGIN_MACRO                  \
    ucontext_t *uc = CONTEXT(_newthread);       \
    uc->uc_mcontext.gregs[EAX] = 1;         \
    _MD_RESTORE_ERRNO(_newthread)           \
    _MD_SET_CURRENT_THREAD(_newthread);     \
    (_newthread)->no_sched = 1;         \
    setcontext(uc);                 \
    PR_END_MACRO
#endif /* USE_SETJMP */

#endif /* sparc */

struct _MDLock {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDThread {
    _PR_CONTEXT_TYPE context;
    int errcode;
    int id;
};

struct _MDThreadStack {
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
#define _MD_NEW_LOCK(lock)              PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD                 _MD_InitializeThread
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)
#define _MD_RESUME_THREAD(thread)
#define _MD_CLEAN_THREAD(_thread)

extern PRStatus _MD_WAIT(struct PRThread *, PRIntervalTime timeout);
extern PRStatus _MD_WAKEUP_WAITER(struct PRThread *);
extern void     _MD_YIELD(void);
extern PRStatus _MD_InitializeThread(PRThread *thread);
extern void     _MD_SET_PRIORITY(struct _MDThread *thread,
                                 PRThreadPriority newPri);
extern PRStatus _MD_CREATE_THREAD(PRThread *thread, void (*start) (void *),
                                  PRThreadPriority priority, PRThreadScope scope, PRThreadState state,
                                  PRUint32 stackSize);

/* The following defines the unwrapped versions of select() and poll(). */
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, struct timeval *timeout);
#define _MD_SELECT  _select

#include <stropts.h>
#include <poll.h>
#define _MD_POLL _poll
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);

PR_BEGIN_EXTERN_C

/*
** Missing function prototypes
*/
extern int gethostname (char *name, int namelen);

PR_END_EXTERN_C

#endif /* _PR_PTHREADS */

extern void _MD_solaris_map_sendfile_error(int err);

#endif /* nspr_solaris_defs_h___ */

