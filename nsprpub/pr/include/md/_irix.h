/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#ifndef nspr_irix_defs_h___
#define nspr_irix_defs_h___

#include "prclist.h"
#include "prthread.h"
#include <sys/ucontext.h>

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH          "irix"
#define _PR_SI_SYSNAME          "IRIX"
#define _PR_SI_ARCHITECTURE     "mips"
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE        0x50000000
#define _PR_NUM_GCREGS          9
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#define _MD_DEFAULT_STACK_SIZE  65536L
#define _MD_MIN_STACK_SIZE      16384L

#undef  HAVE_STACK_GROWING_UP
#define HAVE_WEAK_IO_SYMBOLS
#define HAVE_WEAK_MALLOC_SYMBOLS
#define HAVE_DLL
#define USE_DLFCN
#define _PR_HAVE_ATOMIC_OPS

/* Initialization entry points */
PR_EXTERN(void) _MD_EarlyInit(void);
#define _MD_EARLY_INIT _MD_EarlyInit

PR_EXTERN(void) _MD_IrixInit(void);
#define _MD_FINAL_INIT _MD_IrixInit

#define _MD_INIT_IO()

/* Timer operations */
PR_EXTERN(PRIntervalTime) _MD_GetInterval(void);
#define _MD_GET_INTERVAL _MD_GetInterval

PR_EXTERN(PRIntervalTime) _MD_IntervalPerSec(void);
#define _MD_INTERVAL_PER_SEC _MD_IntervalPerSec

/* GC operations */
PR_EXTERN(void *) _MD_GetSP(PRThread *thread);
#define    _MD_GET_SP _MD_GetSP

/* The atomic operations */
#include <mutex.h>
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val) add_then_test((unsigned long*)val, 1)
#define _MD_ATOMIC_DECREMENT(val) add_then_test((unsigned long*)val, 0xffffffff)
#define _MD_ATOMIC_SET(val, newval) test_and_set((unsigned long*)val, newval)

#if defined(_PR_PTHREADS)
#else /* defined(_PR_PTHREADS) */

/************************************************************************/

#include <setjmp.h>
#include <errno.h>
#include <unistd.h>
#include <bstring.h>
#include <sys/time.h>
#include <ulocks.h>
#include <sys/prctl.h>


/*
 * Data region private to each sproc. This region is setup by calling
 * mmap(...,MAP_LOCAL,...). The private data is mapped at the same
 * address in every sproc, but every sproc gets a private mapping.
 *
 * Just make sure that this structure fits in a page, as only one page
 * is allocated for the private region.
 */
struct sproc_private_data {
    struct PRThread *me;
    struct _PRCPU *cpu;
    struct PRThread *last;
    PRUintn intsOff;
	int		sproc_pid;
};

extern char *_nspr_sproc_private;

#define _PR_PRDA() ((struct sproc_private_data *) _nspr_sproc_private)
#define _MD_CURRENT_THREAD() (_PR_PRDA()->me)
#define _MD_SET_CURRENT_THREAD(_thread) _PR_PRDA()->me = (_thread)
#define _MD_LAST_THREAD() (_PR_PRDA()->last)
#define _MD_SET_LAST_THREAD(_thread) _PR_PRDA()->last = (_thread)
#define _MD_CURRENT_CPU() (_PR_PRDA()->cpu)
#define _MD_SET_CURRENT_CPU(_cpu) _PR_PRDA()->cpu = (_cpu)
#define _MD_SET_INTSOFF(_val) (_PR_PRDA()->intsOff = _val)
#define _MD_GET_INTSOFF() (_PR_PRDA()->intsOff)

#define _MD_SET_SPROC_PID(_val) (_PR_PRDA()->sproc_pid = _val)
#define _MD_GET_SPROC_PID() (_PR_PRDA()->sproc_pid)

PR_EXTERN(struct PRThread*) _MD_get_attached_thread(void);
#define _MD_GET_ATTACHED_THREAD()	_MD_get_attached_thread()

#define _MD_CHECK_FOR_EXIT() {					\
		if (_pr_irix_exit_now) {			\
			_PR_POST_SEM(_pr_irix_exit_sem);	\
			exit(0);				\
		}						\
	}
		
#define _MD_ATTACH_THREAD(threadp)

#define _MD_SAVE_ERRNO(_thread)
#define _MD_RESTORE_ERRNO(_thread)

extern struct _PRCPU  *_pr_primordialCPU;
extern usema_t *_pr_irix_exit_sem;
extern PRInt32 _pr_irix_exit_now;

/* Thread operations */
#define _PR_LOCK_HEAP()	{						\
			PRIntn _is;					\
				if (_pr_primordialCPU) {		\
				if (_PR_MD_CURRENT_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_CURRENT_THREAD()))	\
						_PR_INTSOFF(_is); 	\
					_PR_LOCK(_pr_heapLock);		\
				}

#define _PR_UNLOCK_HEAP() 	if (_pr_primordialCPU)	{		\
					_PR_UNLOCK(_pr_heapLock);	\
				if (_PR_MD_CURRENT_THREAD() && 		\
					!_PR_IS_NATIVE_THREAD( 		\
					_PR_MD_CURRENT_THREAD()))	\
						_PR_INTSON(_is);	\
				}					\
			  }

#define _PR_OPEN_POLL_SEM(_sem)  usopenpollsema(_sem, 0666)
#define _PR_WAIT_SEM(_sem) uspsema(_sem)
#define _PR_POST_SEM(_sem) usvsema(_sem)

#define _MD_CVAR_POST_SEM(threadp)	usvsema((threadp)->md.cvar_pollsem)

#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

struct _MDLock {
    ulock_t lock;
	usptr_t *arena;
};

/*
 * disable pre-emption for the LOCAL threads when calling the arena lock
 * routines
 */

#define _PR_LOCK(lock) {						\
		PRIntn _is;						\
		PRThread *me = _PR_MD_CURRENT_THREAD();			\
		if (me && !_PR_IS_NATIVE_THREAD(me))			\
			_PR_INTSOFF(_is); 				\
		ussetlock(lock);					\
		if (me && !_PR_IS_NATIVE_THREAD(me))			\
			_PR_FAST_INTSON(_is); 				\
	}

#define _PR_UNLOCK(lock) {						\
		PRIntn _is;						\
		PRThread *me = _PR_MD_CURRENT_THREAD();			\
		if (me && !_PR_IS_NATIVE_THREAD(me))			\
			_PR_INTSOFF(_is); 				\
		usunsetlock(lock);					\
		if (me && !_PR_IS_NATIVE_THREAD(me))			\
			_PR_FAST_INTSON(_is); 				\
	}

PR_EXTERN(PRStatus) _MD_NEW_LOCK(struct _MDLock *md);
PR_EXTERN(void) _MD_FREE_LOCK(struct _MDLock *lockp);

#define _MD_LOCK(_lockp) _PR_LOCK((_lockp)->lock)
#define _MD_UNLOCK(_lockp) _PR_UNLOCK((_lockp)->lock)
#define _MD_TEST_AND_LOCK(_lockp) uscsetlock((_lockp)->lock, 1)

extern ulock_t _pr_heapLock;

struct _MDThread {
    jmp_buf jb;
    usptr_t     *pollsem_arena;
    usema_t     *cvar_pollsem;
    PRInt32     cvar_pollsemfd;
    PRInt32     cvar_pollsem_select;    /* acquire sem by calling select */
    PRInt32     cvar_wait;              /* if 1, thread is waiting on cvar Q */
    PRInt32	id;
    PRInt32	suspending_id;
    int errcode;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDSemaphore {
    usema_t *sem;
};

struct _MDCVar {
    ulock_t mdcvar_lock;
};

struct _MDSegment {
    PRInt8 notused;
};

struct _MDCPU {
    PRInt32 id;
    PRInt32 suspending_id;
    struct _MDCPU_Unix md_unix;
};

/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)	      \
    PR_BEGIN_MACRO				      \
	int *jb = (_thread)->md.jb;		      \
    *status = PR_TRUE;              \
	(void) setjmp(jb);			      \
	(_thread)->md.jb[JB_SP] = (int) ((_sp) - 64); \
	(_thread)->md.jb[JB_PC] = (int) _main;	      \
	_thread->no_sched = 0; \
    PR_END_MACRO

/*
** Switch away from the current thread context by saving its state and
** calling the thread scheduler. Reload cpu when we come back from the
** context switch because it might have changed.
*
*  XXX RUNQ lock needed before clearing _PR_NO_SCHED flag, because the
*      thread may be unr RUNQ?
*/
#define _MD_SWITCH_CONTEXT(_thread) \
    PR_BEGIN_MACRO    \
    PR_ASSERT(_thread->no_sched); \
    if (!setjmp(_thread->md.jb)) { \
        _MD_SAVE_ERRNO(_thread) \
        _MD_SET_LAST_THREAD(_thread); \
        _PR_Schedule(); \
    } else {      \
        PR_ASSERT(_MD_LAST_THREAD() !=_MD_CURRENT_THREAD()); \
            _MD_LAST_THREAD()->no_sched = 0;			\
    }             \
    PR_END_MACRO

/*
** Restore a thread context that was saved by _MD_SWITCH_CONTEXT or
** initialized by _MD_INIT_CONTEXT.
*/
#define _MD_RESTORE_CONTEXT(_newThread) \
    PR_BEGIN_MACRO \
    int *jb = (_newThread)->md.jb; \
    _MD_RESTORE_ERRNO(_newThread) \
    _MD_SET_CURRENT_THREAD(_newThread); \
    _newThread->no_sched = 1;		\
    longjmp(jb, 1); \
    PR_END_MACRO

PR_EXTERN(PRStatus) _MD_InitThread(struct PRThread *thread, PRBool wakeup_parent);
#define _MD_INIT_THREAD(thread) 			_MD_InitThread(thread, PR_TRUE)
#define _MD_INIT_ATTACHED_THREAD(thread)	_MD_InitThread(thread, PR_FALSE)

PR_EXTERN(void) _MD_ExitThread(struct PRThread *thread);
#define _MD_EXIT_THREAD _MD_ExitThread

PR_EXTERN(void) _MD_SuspendThread(struct PRThread *thread);
#define _MD_SUSPEND_THREAD _MD_SuspendThread

PR_EXTERN(void) _MD_ResumeThread(struct PRThread *thread);
#define _MD_RESUME_THREAD _MD_ResumeThread

PR_EXTERN(void) _MD_SuspendCPU(struct _PRCPU *thread);
#define _MD_SUSPEND_CPU _MD_SuspendCPU

PR_EXTERN(void) _MD_ResumeCPU(struct _PRCPU *thread);
#define _MD_RESUME_CPU _MD_ResumeCPU

#define _MD_BEGIN_SUSPEND_ALL()
#define _MD_END_SUSPEND_ALL()
#define _MD_BEGIN_RESUME_ALL()
#define _MD_END_RESUME_ALL()

PR_EXTERN(void) _MD_InitLocks(void);
#define _MD_INIT_LOCKS _MD_InitLocks

PR_EXTERN(void) _MD_CleanThread(struct PRThread *thread);
#define _MD_CLEAN_THREAD _MD_CleanThread

#define _MD_INIT_PRIMORDIAL_THREAD(threadp)
#define _MD_YIELD()    sginap(0)

/* The _PR_MD_WAIT_LOCK and _PR_MD_WAKEUP_WAITER functions put to sleep and
 * awaken a thread which is waiting on a lock or cvar.
 */
PR_EXTERN(PRStatus) _MD_wait(struct PRThread *, PRIntervalTime timeout);
#define _MD_WAIT _MD_wait

PR_EXTERN(PRStatus) _MD_WakeupWaiter(struct PRThread *);
#define _MD_WAKEUP_WAITER _MD_WakeupWaiter

PR_EXTERN(void ) _MD_exit(PRIntn status);
#define _MD_EXIT	_MD_exit

#include "prthread.h"

PR_EXTERN(void) _MD_SetPriority(struct _MDThread *thread,
	PRThreadPriority newPri);
#define _MD_SET_PRIORITY _MD_SetPriority

PR_EXTERN(PRStatus) _MD_CreateThread(
                        struct PRThread *thread,
                        void (*start) (void *),
                        PRThreadPriority priority,
                        PRThreadScope scope,
                        PRThreadState state,
                        PRUint32 stackSize);
#define _MD_CREATE_THREAD _MD_CreateThread

extern void _MD_CleanupBeforeExit(void);
#define _MD_CLEANUP_BEFORE_EXIT _MD_CleanupBeforeExit


/* The following defines the unwrapped versions of select() and poll(). */
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout);
#define _MD_SELECT	_select

#include <stropts.h>
#include <poll.h>
#define _MD_POLL _poll
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);


#define HAVE_THREAD_AFFINITY 1

PR_EXTERN(PRInt32) _MD_GetThreadAffinityMask(PRThread *unused, PRUint32 *mask);
#define _MD_GETTHREADAFFINITYMASK _MD_GetThreadAffinityMask

PR_EXTERN(void) _MD_InitRunningCPU(struct _PRCPU *cpu);
#define    _MD_INIT_RUNNING_CPU _MD_InitRunningCPU

#endif  /* defined(_PR_PTHREADS) */

#endif /* nspr_irix_defs_h___ */
