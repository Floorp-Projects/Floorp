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

#ifndef nspr_solaris_defs_h___
#define nspr_solaris_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"solaris"
#define _PR_SI_SYSNAME	"SOLARIS"
#ifdef sparc
#define _PR_SI_ARCHITECTURE	"sparc"
#elif defined(i386)
#define _PR_SI_ARCHITECTURE	"x86"
#else
#error unknown processor
#endif
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE		0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	(2*65536L)
#define _MD_MMAP_FLAGS          MAP_SHARED

#undef  HAVE_STACK_GROWING_UP

#ifndef HAVE_WEAK_IO_SYMBOLS
#define	HAVE_WEAK_IO_SYMBOLS
#endif

#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define	USE_DLFCN
#define NEED_STRFTIME_LOCK

/*
 * Intel x86 has atomic instructions.
 *
 * Sparc v8 does not have instructions to efficiently implement
 * atomic increment/decrement operations.  In the local threads
 * only and pthreads versions, we use the default atomic routine
 * implementation in pratom.c.  The obsolete global threads only
 * version uses a global mutex_t to implement the atomic routines
 * in solaris.c, which is actually equivalent to the default
 * implementation.
 *
 * 64-bit Solaris requires sparc v9, which has atomic instructions.
 */
#if defined(i386) || defined(_PR_GLOBAL_THREADS_ONLY) || defined(IS_64)
#define _PR_HAVE_ATOMIC_OPS
#endif

#if defined(_PR_GLOBAL_THREADS_ONLY) || defined(_PR_PTHREADS)
/*
 * We have assembly language implementation of atomic
 * stacks for the 32-bit sparc and x86 architectures only.
 *
 * Note: We ran into thread starvation problem with the
 * 32-bit sparc assembly language implementation of atomic
 * stacks, so we do not use it now. (Bugzilla bug 113740)
 */
#if !defined(sparc)
#define _PR_HAVE_ATOMIC_CAS
#endif
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
#if defined(_PR_GLOBAL_THREADS_ONLY) || defined(_PR_PTHREADS)
#define _PR_HAVE_GETHOST_R
#define _PR_HAVE_GETHOST_R_POINTER
#endif

#include "prinrval.h"
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

NSPR_API(void)		_MD_EarlyInit(void);

#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_PR_UnixInit

#elif defined(_PR_GLOBAL_THREADS_ONLY)

#include "prthread.h"

#include <ucontext.h>

/*
** Iinitialization Related definitions
*/

NSPR_API(void)		_MD_EarlyInit(void);

#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_PR_UnixInit

#define _MD_GET_SP(threadp)	threadp->md.sp

/*
** Clean-up the thread machine dependent data structure
*/
#define	_MD_INIT_THREAD				_MD_InitializeThread
#define	_MD_INIT_ATTACHED_THREAD	_MD_InitializeThread

NSPR_API(PRStatus) _MD_CreateThread(PRThread *thread, 
					void (*start)(void *), 
					PRThreadPriority priority,
					PRThreadScope scope, 
					PRThreadState state, 
					PRUint32 stackSize);
#define _MD_CREATE_THREAD _MD_CreateThread

#define	_PR_CONTEXT_TYPE	ucontext_t

#define CONTEXT(_thread) (&(_thread)->md.context)

#include <thread.h>
#include <sys/lwp.h>
#include <synch.h>

extern struct PRLock *_pr_schedLock;

/*
** Thread Local Storage 
*/

#define THREAD_KEY_T thread_key_t

extern struct PRThread *_pr_attached_thread_tls();
extern struct PRThread *_pr_current_thread_tls();
extern struct _PRCPU *_pr_current_cpu_tls();
extern struct PRThread *_pr_last_thread_tls();

extern THREAD_KEY_T threadid_key;
extern THREAD_KEY_T cpuid_key;
extern THREAD_KEY_T last_thread_key;

#define _MD_GET_ATTACHED_THREAD() _pr_attached_thread_tls()
#define _MD_CURRENT_THREAD() _pr_current_thread_tls()
#define _MD_CURRENT_CPU() _pr_current_cpu_tls()
#define _MD_LAST_THREAD() _pr_last_thread_tls()
	
#define _MD_SET_CURRENT_THREAD(newval) 			\
	PR_BEGIN_MACRO					\
	thr_setspecific(threadid_key, (void *)newval);	\
	PR_END_MACRO

#define _MD_SET_CURRENT_CPU(newval) 			\
	PR_BEGIN_MACRO					\
	thr_setspecific(cpuid_key, (void *)newval);	\
	PR_END_MACRO

#define _MD_SET_LAST_THREAD(newval)	 			\
	PR_BEGIN_MACRO						\
	thr_setspecific(last_thread_key, (void *)newval);	\
	PR_END_MACRO
	
#define	_MD_CLEAN_THREAD(_thread)	_MD_cleanup_thread(_thread)
extern void _MD_exit_thread(PRThread *thread);
#define _MD_EXIT_THREAD(thread)		_MD_exit_thread(thread)

#define	_MD_SUSPEND_THREAD(thread)	_MD_Suspend(thread)
#define	_MD_RESUME_THREAD(thread)	thr_continue((thread)->md.handle)

/* XXXX Needs to be defined - Prashant */
#define _MD_SUSPEND_CPU(cpu)
#define _MD_RESUME_CPU(cpu)

extern void _MD_Begin_SuspendAll(void);
extern void _MD_End_SuspendAll(void);
extern void _MD_End_ResumeAll(void);
#define _MD_BEGIN_SUSPEND_ALL()		_MD_Begin_SuspendAll()
#define _MD_BEGIN_RESUME_ALL()		
#define	_MD_END_SUSPEND_ALL()		_MD_End_SuspendAll()
#define	_MD_END_RESUME_ALL()		_MD_End_ResumeAll()

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(md_lockp) (mutex_init(&((md_lockp)->lock),USYNC_THREAD,NULL) ? PR_FAILURE : PR_SUCCESS)
#define _MD_FREE_LOCK(md_lockp) mutex_destroy(&((md_lockp)->lock))
#define _MD_UNLOCK(md_lockp) mutex_unlock(&((md_lockp)->lock))
#define _MD_TEST_AND_LOCK(md_lockp) mutex_trylock(&((md_lockp)->lock))
struct _MDLock;
NSPR_API(void) _MD_lock(struct _MDLock *md_lock);
#undef PROFILE_LOCKS
#ifndef PROFILE_LOCKS
#define _MD_LOCK(md_lockp) _MD_lock(md_lockp)
#else
#define _MD_LOCK(md_lockp)                 \
    PR_BEGIN_MACRO \
    int rv = _MD_TEST_AND_LOCK(md_lockp); \
    if (rv == 0) { \
        (md_lockp)->hitcount++; \
    } else { \
        (md_lockp)->misscount++; \
        _MD_lock(md_lockp); \
    } \
    PR_END_MACRO
#endif

#define _PR_LOCK_HEAP() if (_pr_heapLock) _MD_LOCK(&_pr_heapLock->md)
#define _PR_UNLOCK_HEAP() if (_pr_heapLock) _MD_UNLOCK(&_pr_heapLock->md)

#define _MD_ATTACH_THREAD(threadp)


#define THR_KEYCREATE thr_keycreate
#define THR_SELF thr_self
#define _MD_NEW_CV(condp) cond_init(&((condp)->cv), USYNC_THREAD, 0)
#define COND_WAIT(condp, mutexp) cond_wait(condp, mutexp)
#define COND_TIMEDWAIT(condp, mutexp, tspec) \
                                     cond_timedwait(condp, mutexp, tspec)
#define _MD_NOTIFY_CV(condp, lockp) cond_signal(&((condp)->cv))
#define _MD_NOTIFYALL_CV(condp,unused) cond_broadcast(&((condp)->cv))	
#define _MD_FREE_CV(condp) cond_destroy(&((condp)->cv))
#define _MD_YIELD() thr_yield()
#include <time.h>
/* 
 * Because clock_gettime() on Solaris/x86 2.4 always generates a
 * segmentation fault, we use an emulated version _pr_solx86_clock_gettime(),
 * which is implemented using gettimeofday().
 */
#if defined(i386) && defined(SOLARIS2_4)
extern int _pr_solx86_clock_gettime(clockid_t clock_id, struct timespec *tp);
#define GETTIME(tt) _pr_solx86_clock_gettime(CLOCK_REALTIME, (tt))
#else
#define GETTIME(tt) clock_gettime(CLOCK_REALTIME, (tt))
#endif  /* i386 && SOLARIS2_4 */

#define MUTEX_T mutex_t
#define COND_T cond_t

#define _MD_NEW_SEM(md_semp,_val)  sema_init(&((md_semp)->sem),_val,USYNC_THREAD,NULL)
#define _MD_DESTROY_SEM(md_semp) sema_destroy(&((md_semp)->sem))
#define _MD_WAIT_SEM(md_semp) sema_wait(&((md_semp)->sem))
#define _MD_POST_SEM(md_semp) sema_post(&((md_semp)->sem))

#define _MD_SAVE_ERRNO(_thread)
#define _MD_RESTORE_ERRNO(_thread)
#define _MD_INIT_RUNNING_CPU(cpu) _MD_unix_init_running_cpu(cpu)

extern struct _MDLock _pr_ioq_lock;
#define _MD_IOQ_LOCK()		_MD_LOCK(&_pr_ioq_lock)
#define _MD_IOQ_UNLOCK()	_MD_UNLOCK(&_pr_ioq_lock)

extern PRStatus _MD_wait(struct PRThread *, PRIntervalTime timeout);
#define _MD_WAIT _MD_wait

extern PRStatus _MD_WakeupWaiter(struct PRThread *);
#define _MD_WAKEUP_WAITER _MD_WakeupWaiter

NSPR_API(void) _MD_InitIO(void);
#define _MD_INIT_IO _MD_InitIO

#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
    PR_BEGIN_MACRO \
    *status = PR_TRUE; \
    PR_END_MACRO
#define _MD_SWITCH_CONTEXT(_thread)
#define _MD_RESTORE_CONTEXT(_newThread)

struct _MDLock {
    MUTEX_T lock;
#ifdef PROFILE_LOCKS
    PRInt32 hitcount;
    PRInt32 misscount;
#endif
};

struct _MDCVar {
    COND_T cv;
};

struct _MDSemaphore {
    sema_t sem;
};

struct _MDThread {
    _PR_CONTEXT_TYPE context;
    thread_t handle;
    lwpid_t lwpid;
    uint_t sp;		/* stack pointer */
    uint_t threadID;	/* ptr to solaris-internal thread id structures */
    struct _MDSemaphore waiter_sem;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDSegment {
    PRInt8 notused;
};

/*
 * md-specific cpu structure field, common to all Unix platforms
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

/* The following defines the unwrapped versions of select() and poll(). */
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout);
#define _MD_SELECT	_select

#include <poll.h>
#define _MD_POLL _poll
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);

PR_BEGIN_EXTERN_C

/*
** Missing function prototypes
*/
extern int gethostname (char *name, int namelen);

PR_END_EXTERN_C

#else /* _PR_GLOBAL_THREADS_ONLY */

/*
 * LOCAL_THREADS_ONLY implementation on Solaris
 */

#include "prthread.h"

#include <errno.h>
#include <ucontext.h>
#include <sys/stack.h>
#include <synch.h>

/*
** Iinitialization Related definitions
*/

NSPR_API(void)				_MD_EarlyInit(void);
NSPR_API(void)				_MD_SolarisInit();
#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_MD_SolarisInit
#define	_MD_INIT_THREAD		_MD_InitializeThread

#ifdef USE_SETJMP

#include <setjmp.h>

#define _PR_CONTEXT_TYPE	jmp_buf

#ifdef sparc
#define _MD_GET_SP(_t)		(_t)->md.context[2]
#else
#define _MD_GET_SP(_t)		(_t)->md.context[4]
#endif

#define PR_NUM_GCREGS		_JBLEN
#define CONTEXT(_thread)	(_thread)->md.context

#else  /* ! USE_SETJMP */

#ifdef sparc
#define	_PR_CONTEXT_TYPE	ucontext_t
#define _MD_GET_SP(_t)		(_t)->md.context.uc_mcontext.gregs[REG_SP]
/*
** Sparc's use register windows. the _MD_GetRegisters for the sparc's
** doesn't actually store anything into the argument buffer; instead the
** register windows are homed to the stack. I assume that the stack
** always has room for the registers to spill to...
*/
#define PR_NUM_GCREGS		0
#else
#define _PR_CONTEXT_TYPE	unsigned int edi; sigset_t oldMask, blockMask; ucontext_t
#define _MD_GET_SP(_t)		(_t)->md.context.uc_mcontext.gregs[USP]
#define PR_NUM_GCREGS		_JBLEN
#endif

#define CONTEXT(_thread)	(&(_thread)->md.context)

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

#define _MD_SAVE_ERRNO(_thread)			(_thread)->md.errcode = errno;
#define _MD_RESTORE_ERRNO(_thread)		errno = (_thread)->md.errcode;

#ifdef sparc

#ifdef USE_SETJMP
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
    PR_BEGIN_MACRO				      \
	int *context = (_thread)->md.context;	      \
    *status = PR_TRUE;              \
	(void) setjmp(context);			      \
	(_thread)->md.context[1] = (int) ((_sp) - 64); \
	(_thread)->md.context[2] = (int) _main;	      \
	(_thread)->md.context[3] = (int) _main + 4; \
    _thread->no_sched = 0; \
    PR_END_MACRO

#define _MD_SWITCH_CONTEXT(_thread)    \
    if (!setjmp(CONTEXT(_thread))) { \
	_MD_SAVE_ERRNO(_thread)    \
	_MD_SET_LAST_THREAD(_thread);	 \
    _MD_SET_CURRENT_THREAD(_thread);	 \
	_PR_Schedule();		     \
    }

#define _MD_RESTORE_CONTEXT(_newThread)	    \
{				     \
	_MD_RESTORE_ERRNO(_newThread)	    \
	_MD_SET_CURRENT_THREAD(_newThread); \
    longjmp(CONTEXT(_newThread), 1);    \
}

#else
/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)					\
    PR_BEGIN_MACRO				      									\
    ucontext_t *uc = CONTEXT(_thread);									\
    *status = PR_TRUE;													\
    getcontext(uc);														\
    uc->uc_stack.ss_sp = (char *) ((unsigned long)(_sp - WINDOWSIZE - SA(MINFRAME)) & 0xfffffff8);	\
    uc->uc_stack.ss_size = _thread->stack->stackSize; 					\
    uc->uc_stack.ss_flags = 0; 				/* ? */		        		\
    uc->uc_mcontext.gregs[REG_SP] = (unsigned int) uc->uc_stack.ss_sp;	\
    uc->uc_mcontext.gregs[REG_PC] = (unsigned int) _main;				\
    uc->uc_mcontext.gregs[REG_nPC] = (unsigned int) ((char*)_main)+4;	\
    uc->uc_flags = UC_ALL;												\
    _thread->no_sched = 0;												\
    PR_END_MACRO

/*
** Switch away from the current thread context by saving its state and
** calling the thread scheduler. Reload cpu when we come back from the
** context switch because it might have changed.
*/
#define _MD_SWITCH_CONTEXT(_thread)    				\
    PR_BEGIN_MACRO                     				\
		if (!getcontext(CONTEXT(_thread))) { 		\
			_MD_SAVE_ERRNO(_thread);    			\
			_MD_SET_LAST_THREAD(_thread);	 		\
			_PR_Schedule();			 				\
		}					 						\
    PR_END_MACRO

/*
** Restore a thread context that was saved by _MD_SWITCH_CONTEXT or
** initialized by _MD_INIT_CONTEXT.
*/
#define _MD_RESTORE_CONTEXT(_newThread)	    				\
    PR_BEGIN_MACRO			    							\
    	ucontext_t *uc = CONTEXT(_newThread); 				\
    	uc->uc_mcontext.gregs[11] = 1;     					\
		_MD_RESTORE_ERRNO(_newThread);	    				\
		_MD_SET_CURRENT_THREAD(_newThread); 				\
    	setcontext(uc);		       							\
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
        _PR_Schedule();	\
    }

#define _MD_RESTORE_CONTEXT(_newThread) \
{ \
    _MD_RESTORE_ERRNO(_newThread) \
    _MD_SET_CURRENT_THREAD(_newThread); \
    longjmp(CONTEXT(_newThread), 1); \
}

#else /* USE_SETJMP */

#define WINDOWSIZE		0
 
int getedi(void);
void setedi(int);
 
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
	PR_BEGIN_MACRO					\
	ucontext_t *uc = CONTEXT(_thread);		\
        *status = PR_TRUE;              \
	getcontext(uc);					\
	/* Force sp to be double aligned! */		\
    	uc->uc_mcontext.gregs[USP] = (int) ((unsigned long)(_sp - WINDOWSIZE - SA(MINFRAME)) & 0xfffffff8);	\
	uc->uc_mcontext.gregs[PC] = (int) _main;	\
	(_thread)->no_sched = 0; \
	PR_END_MACRO

/* getcontext() may return 1, contrary to what the man page says */
#define _MD_SWITCH_CONTEXT(_thread)			\
	PR_BEGIN_MACRO					\
	ucontext_t *uc = CONTEXT(_thread);		\
	PR_ASSERT(_thread->no_sched);			\
	sigfillset(&((_thread)->md.blockMask));		\
	sigprocmask(SIG_BLOCK, &((_thread)->md.blockMask),	\
		&((_thread)->md.oldMask));		\
	(_thread)->md.edi = getedi();			\
	if (! getcontext(uc)) {				\
		sigprocmask(SIG_SETMASK, &((_thread)->md.oldMask), NULL); \
		uc->uc_mcontext.gregs[EDI] = (_thread)->md.edi;	\
		_MD_SAVE_ERRNO(_thread)    		\
	        _MD_SET_LAST_THREAD(_thread);	        \
		_PR_Schedule();				\
	} else {					\
		sigprocmask(SIG_SETMASK, &((_thread)->md.oldMask), NULL); \
		setedi((_thread)->md.edi);		\
		PR_ASSERT(_MD_LAST_THREAD() !=_MD_CURRENT_THREAD()); \
		_MD_LAST_THREAD()->no_sched = 0;	\
	}						\
	PR_END_MACRO

/*
** Restore a thread context, saved by _PR_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_newthread)			\
	PR_BEGIN_MACRO					\
	ucontext_t *uc = CONTEXT(_newthread);		\
	uc->uc_mcontext.gregs[EAX] = 1;			\
	_MD_RESTORE_ERRNO(_newthread)  			\
	_MD_SET_CURRENT_THREAD(_newthread);		\
	(_newthread)->no_sched = 1;			\
	setcontext(uc);					\
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

#ifndef _PR_PTHREADS
#define _MD_INIT_LOCKS()
#endif
#define _MD_NEW_LOCK(lock)				PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#define _MD_INIT_RUNNING_CPU(cpu)		_MD_unix_init_running_cpu(cpu)
#define _MD_INIT_THREAD					_MD_InitializeThread
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

NSPR_API(PRIntervalTime)				_MD_Solaris_GetInterval(void);
#define _MD_GET_INTERVAL				_MD_Solaris_GetInterval
NSPR_API(PRIntervalTime)				_MD_Solaris_TicksPerSecond(void);
#define _MD_INTERVAL_PER_SEC			_MD_Solaris_TicksPerSecond

/* The following defines the unwrapped versions of select() and poll(). */
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout);
#define _MD_SELECT	_select

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

#endif /* _PR_GLOBAL_THREADS_ONLY */

extern void _MD_solaris_map_sendfile_error(int err);

#endif /* nspr_solaris_defs_h___ */

