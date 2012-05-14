/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_pthread_defs_h___
#define nspr_pthread_defs_h___

#include <pthread.h>
#include "prthread.h"

#if defined(PTHREADS_USER)
/*
** Thread Local Storage 
*/
extern pthread_key_t current_thread_key;
extern pthread_key_t current_cpu_key;
extern pthread_key_t last_thread_key;
extern pthread_key_t intsoff_key;

#define _MD_CURRENT_THREAD() 			\
			((struct PRThread *) pthread_getspecific(current_thread_key))
#define _MD_CURRENT_CPU() 				\
			((struct _PRCPU *) pthread_getspecific(current_cpu_key))
#define _MD_LAST_THREAD()				\
			((struct PRThread *) pthread_getspecific(last_thread_key))
	
#define _MD_SET_CURRENT_THREAD(newval) 			\
	pthread_setspecific(current_thread_key, (void *)newval)

#define _MD_SET_CURRENT_CPU(newval) 			\
	pthread_setspecific(current_cpu_key, (void *)newval)

#define _MD_SET_LAST_THREAD(newval)	 			\
	pthread_setspecific(last_thread_key, (void *)newval)

#define _MD_SET_INTSOFF(_val)
#define _MD_GET_INTSOFF()	1
	
/*
** Initialize the thread context preparing it to execute _main.
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)			\
    PR_BEGIN_MACRO				      							\
        *status = PR_TRUE;              						\
		if (SAVE_CONTEXT(_thread)) {							\
	    	(*_main)();											\
		}														\
		_MD_SET_THR_SP(_thread, _sp);							\
		_thread->no_sched = 0; 									\
    PR_END_MACRO

#define _MD_SWITCH_CONTEXT(_thread)  								\
    PR_BEGIN_MACRO 													\
	PR_ASSERT(_thread->no_sched);									\
	if (!SAVE_CONTEXT(_thread)) {									\
		(_thread)->md.errcode = errno;  							\
		_MD_SET_LAST_THREAD(_thread);								\
		_PR_Schedule();		     									\
    } else {														\
		 (_MD_LAST_THREAD())->no_sched = 0;							\
	}																\
    PR_END_MACRO

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread)								\
    PR_BEGIN_MACRO 													\
    errno = (_thread)->md.errcode; 									\
    _MD_SET_CURRENT_THREAD(_thread); 								\
	_thread->no_sched = 1;											\
    GOTO_CONTEXT(_thread); 											\
    PR_END_MACRO


/* Machine-dependent (MD) data structures */

struct _MDThread {
    jmp_buf 		jb;
    int				id;
    int				errcode;
	pthread_t		pthread;
	pthread_mutex_t	pthread_mutex;
	pthread_cond_t	pthread_cond;
	int				wait;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDLock {
	pthread_mutex_t mutex;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDCVar {
	pthread_mutex_t mutex;
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
    jmp_buf 			jb;
	pthread_t 			pthread;
	struct _MDCPU_Unix 	md_unix;
};

/*
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
*/

extern pthread_mutex_t _pr_heapLock;

#define _PR_LOCK(lock) pthread_mutex_lock(lock)

#define _PR_UNLOCK(lock) pthread_mutex_unlock(lock)


#define _PR_LOCK_HEAP()	{									\
				if (_pr_primordialCPU) {					\
					_PR_LOCK(_pr_heapLock);					\
				}

#define _PR_UNLOCK_HEAP() 	if (_pr_primordialCPU)	{		\
					_PR_UNLOCK(_pr_heapLock);				\
				}											\
			  }

NSPR_API(PRStatus) _MD_NEW_LOCK(struct _MDLock *md);
NSPR_API(void) _MD_FREE_LOCK(struct _MDLock *lockp);

#define _MD_LOCK(_lockp) _PR_LOCK(&(_lockp)->mutex)
#define _MD_UNLOCK(_lockp) _PR_UNLOCK(&(_lockp)->mutex)

#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()
#define _MD_CHECK_FOR_EXIT()

NSPR_API(PRStatus) _MD_InitThread(struct PRThread *thread);
#define _MD_INIT_THREAD _MD_InitThread
#define _MD_INIT_ATTACHED_THREAD _MD_InitThread

NSPR_API(void) _MD_ExitThread(struct PRThread *thread);
#define _MD_EXIT_THREAD _MD_ExitThread

NSPR_API(void) _MD_SuspendThread(struct PRThread *thread);
#define _MD_SUSPEND_THREAD _MD_SuspendThread

NSPR_API(void) _MD_ResumeThread(struct PRThread *thread);
#define _MD_RESUME_THREAD _MD_ResumeThread

NSPR_API(void) _MD_SuspendCPU(struct _PRCPU *thread);
#define _MD_SUSPEND_CPU _MD_SuspendCPU

NSPR_API(void) _MD_ResumeCPU(struct _PRCPU *thread);
#define _MD_RESUME_CPU _MD_ResumeCPU

#define _MD_BEGIN_SUSPEND_ALL()
#define _MD_END_SUSPEND_ALL()
#define _MD_BEGIN_RESUME_ALL()
#define _MD_END_RESUME_ALL()

NSPR_API(void) _MD_EarlyInit(void);
#define _MD_EARLY_INIT _MD_EarlyInit

#define _MD_FINAL_INIT _PR_UnixInit

NSPR_API(void) _MD_InitLocks(void);
#define _MD_INIT_LOCKS _MD_InitLocks

NSPR_API(void) _MD_CleanThread(struct PRThread *thread);
#define _MD_CLEAN_THREAD _MD_CleanThread

NSPR_API(PRStatus) _MD_CreateThread(
                        struct PRThread *thread,
                        void (*start) (void *),
                        PRThreadPriority priority,
                        PRThreadScope scope,
                        PRThreadState state,
                        PRUint32 stackSize);
#define _MD_CREATE_THREAD _MD_CreateThread

extern void _MD_CleanupBeforeExit(void);
#define _MD_CLEANUP_BEFORE_EXIT _MD_CleanupBeforeExit

NSPR_API(void) _MD_InitRunningCPU(struct _PRCPU *cpu);
#define    _MD_INIT_RUNNING_CPU _MD_InitRunningCPU

/* The _PR_MD_WAIT_LOCK and _PR_MD_WAKEUP_WAITER functions put to sleep and
 * awaken a thread which is waiting on a lock or cvar.
 */
NSPR_API(PRStatus) _MD_wait(struct PRThread *, PRIntervalTime timeout);
#define _MD_WAIT _MD_wait

NSPR_API(PRStatus) _MD_WakeupWaiter(struct PRThread *);
#define _MD_WAKEUP_WAITER _MD_WakeupWaiter

NSPR_API(void) _MD_SetPriority(struct _MDThread *thread,
	PRThreadPriority newPri);
#define _MD_SET_PRIORITY _MD_SetPriority

#endif /* PTHREADS_USER */

#endif /* nspr_pthread_defs_h___ */
