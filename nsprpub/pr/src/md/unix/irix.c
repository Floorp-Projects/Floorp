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

#include "primpl.h"

#include <signal.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/immu.h>
#include <sys/utsname.h>
#include <sys/sysmp.h>
#include <sys/pda.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/procfs.h>
#include <task.h>
#include <dlfcn.h>

static void _MD_IrixIntervalInit(void);

#if defined(_PR_PTHREADS)
/*
 * for compatibility with classic nspr
 */
void _PR_IRIX_CHILD_PROCESS()
{
}
#else  /* defined(_PR_PTHREADS) */

static void irix_detach_sproc(void);
char *_nspr_sproc_private;    /* ptr. to private region in every sproc */

extern PRUintn    _pr_numCPU;

typedef struct nspr_arena {
	PRCList links;
	usptr_t *usarena;
} nspr_arena;

#define ARENA_PTR(qp) \
	((nspr_arena *) ((char*) (qp) - offsetof(nspr_arena , links)))

static usptr_t *alloc_new_arena(void);

PRCList arena_list = PR_INIT_STATIC_CLIST(&arena_list);
ulock_t arena_list_lock;
nspr_arena first_arena;
int	_nspr_irix_arena_cnt = 1;

PRCList sproc_list = PR_INIT_STATIC_CLIST(&sproc_list);
ulock_t sproc_list_lock;

typedef struct sproc_data {
	void (*entry) (void *, size_t);
	unsigned inh;
	void *arg;
	caddr_t sp;
	size_t len;
	int *pid;
	int creator_pid;
} sproc_data;

typedef struct sproc_params {
	PRCList links;
	sproc_data sd;
} sproc_params;

#define SPROC_PARAMS_PTR(qp) \
	((sproc_params *) ((char*) (qp) - offsetof(sproc_params , links)))

long	_nspr_irix_lock_cnt = 0;
long	_nspr_irix_sem_cnt = 0;
long	_nspr_irix_pollsem_cnt = 0;

usptr_t *_pr_usArena;
ulock_t _pr_heapLock;

usema_t *_pr_irix_exit_sem;
PRInt32 _pr_irix_exit_now = 0;
PRInt32 _pr_irix_process_exit_code = 0;	/* exit code for PR_ProcessExit */
PRInt32 _pr_irix_process_exit = 0; /* process exiting due to call to
										   PR_ProcessExit */

int _pr_irix_primoridal_cpu_fd[2] = { -1, -1 };
static void (*libc_exit)(int) = NULL;
static void *libc_handle = NULL;

#define _NSPR_DEF_INITUSERS		100	/* default value of CONF_INITUSERS */
#define _NSPR_DEF_INITSIZE		(4 * 1024 * 1024)	/* 4 MB */

int _irix_initusers = _NSPR_DEF_INITUSERS;
int _irix_initsize = _NSPR_DEF_INITSIZE;

PRIntn _pr_io_in_progress, _pr_clock_in_progress;

PRInt32 _pr_md_irix_sprocs_created, _pr_md_irix_sprocs_failed;
PRInt32 _pr_md_irix_sprocs = 1;
PRCList _pr_md_irix_sproc_list =
PR_INIT_STATIC_CLIST(&_pr_md_irix_sproc_list);

sigset_t ints_off;
extern sigset_t timer_set;

#if !defined(PR_SETABORTSIG)
#define PR_SETABORTSIG 18
#endif
/*
 * terminate the entire application if any sproc exits abnormally
 */
PRBool _nspr_terminate_on_error = PR_TRUE;

/*
 * exported interface to set the shared arena parameters
 */
void _PR_Irix_Set_Arena_Params(PRInt32 initusers, PRInt32 initsize)
{
    _irix_initusers = initusers;
    _irix_initsize = initsize;
}

static usptr_t *alloc_new_arena()
{
    return(usinit("/dev/zero"));
}

static PRStatus new_poll_sem(struct _MDThread *mdthr, int val)
{
PRIntn _is;
PRStatus rv = PR_SUCCESS;
usema_t *sem = NULL;
PRCList *qp;
nspr_arena *arena;
usptr_t *irix_arena;
PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(_is); 
	_PR_LOCK(arena_list_lock);
	for (qp = arena_list.next; qp != &arena_list; qp = qp->next) {
		arena = ARENA_PTR(qp);
		sem = usnewpollsema(arena->usarena, val);
		if (sem != NULL) {
			mdthr->cvar_pollsem = sem;
			mdthr->pollsem_arena = arena->usarena;
			break;
		}
	}
	if (sem == NULL) {
		/*
		 * If no space left in the arena allocate a new one.
		 */
		if (errno == ENOMEM) {
			arena = PR_NEWZAP(nspr_arena);
			if (arena != NULL) {
				irix_arena = alloc_new_arena();
				if (irix_arena) {
					PR_APPEND_LINK(&arena->links, &arena_list);
					_nspr_irix_arena_cnt++;
					arena->usarena = irix_arena;
					sem = usnewpollsema(arena->usarena, val);
					if (sem != NULL) {
						mdthr->cvar_pollsem = sem;
						mdthr->pollsem_arena = arena->usarena;
					} else
						rv = PR_FAILURE;
				} else {
					PR_DELETE(arena);
					rv = PR_FAILURE;
				}

			} else
				rv = PR_FAILURE;
		} else
			rv = PR_FAILURE;
	}
	_PR_UNLOCK(arena_list_lock);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(_is);
	if (rv == PR_SUCCESS)
		_MD_ATOMIC_INCREMENT(&_nspr_irix_pollsem_cnt);
	return rv;
}

static void free_poll_sem(struct _MDThread *mdthr)
{
PRIntn _is;
PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(_is); 
	usfreepollsema(mdthr->cvar_pollsem, mdthr->pollsem_arena);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(_is);
	_MD_ATOMIC_DECREMENT(&_nspr_irix_pollsem_cnt);
}

static PRStatus new_lock(struct _MDLock *lockp)
{
PRIntn _is;
PRStatus rv = PR_SUCCESS;
ulock_t lock = NULL;
PRCList *qp;
nspr_arena *arena;
usptr_t *irix_arena;
PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(_is); 
	_PR_LOCK(arena_list_lock);
	for (qp = arena_list.next; qp != &arena_list; qp = qp->next) {
		arena = ARENA_PTR(qp);
		lock = usnewlock(arena->usarena);
		if (lock != NULL) {
			lockp->lock = lock;
			lockp->arena = arena->usarena;
			break;
		}
	}
	if (lock == NULL) {
		/*
		 * If no space left in the arena allocate a new one.
		 */
		if (errno == ENOMEM) {
			arena = PR_NEWZAP(nspr_arena);
			if (arena != NULL) {
				irix_arena = alloc_new_arena();
				if (irix_arena) {
					PR_APPEND_LINK(&arena->links, &arena_list);
					_nspr_irix_arena_cnt++;
					arena->usarena = irix_arena;
					lock = usnewlock(irix_arena);
					if (lock != NULL) {
						lockp->lock = lock;
						lockp->arena = arena->usarena;
					} else
						rv = PR_FAILURE;
				} else {
					PR_DELETE(arena);
					rv = PR_FAILURE;
				}

			} else
				rv = PR_FAILURE;
		} else
			rv = PR_FAILURE;
	}
	_PR_UNLOCK(arena_list_lock);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(_is);
	if (rv == PR_SUCCESS)
		_MD_ATOMIC_INCREMENT(&_nspr_irix_lock_cnt);
	return rv;
}

static void free_lock(struct _MDLock *lockp)
{
PRIntn _is;
PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(_is); 
	usfreelock(lockp->lock, lockp->arena);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(_is);
	_MD_ATOMIC_DECREMENT(&_nspr_irix_lock_cnt);
}

void _MD_FREE_LOCK(struct _MDLock *lockp)
{
	PRIntn _is;
	PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(_is); 
	free_lock(lockp);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(_is);
}

/*
 * _MD_get_attached_thread
 *		Return the thread pointer of the current thread if it is attached.
 *
 *		This function is needed for Irix because the thread-local-storage is
 *		implemented by mmapin'g a page with the MAP_LOCAL flag. This causes the
 *		sproc-private page to inherit contents of the page of the caller of sproc().
 */
PRThread *_MD_get_attached_thread(void)
{

	if (_MD_GET_SPROC_PID() == get_pid())
		return _MD_THIS_THREAD();
	else
		return 0;
}

/*
 * _MD_get_current_thread
 *		Return the thread pointer of the current thread (attaching it if
 *		necessary)
 */
PRThread *_MD_get_current_thread(void)
{
PRThread *me;

	me = _MD_GET_ATTACHED_THREAD();
    if (NULL == me) {
        me = _PRI_AttachThread(
            PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL, 0);
    }
    PR_ASSERT(me != NULL);
	return(me);
}

/*
 * irix_detach_sproc
 *		auto-detach a sproc when it exits
 */
void irix_detach_sproc(void)
{
PRThread *me;

	me = _MD_GET_ATTACHED_THREAD();
	if ((me != NULL) && (me->flags & _PR_ATTACHED)) {
		_PRI_DetachThread();
	}
}


PRStatus _MD_NEW_LOCK(struct _MDLock *lockp)
{
    PRStatus rv;
    PRIntn is;
    PRThread *me = _MD_GET_ATTACHED_THREAD();	

	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(is);
	rv = new_lock(lockp);
	if (me && !_PR_IS_NATIVE_THREAD(me))
		_PR_FAST_INTSON(is);
	return rv;
}

static void
sigchld_handler(int sig)
{
    pid_t pid;
    int status;

    /*
     * If an sproc exited abnormally send a SIGKILL signal to all the
     * sprocs in the process to terminate the application
     */
    while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
        if (WIFSIGNALED(status) && ((WTERMSIG(status) == SIGSEGV) ||
            (WTERMSIG(status) == SIGBUS) ||
            (WTERMSIG(status) == SIGABRT) ||
            (WTERMSIG(status) == SIGILL))) {

				prctl(PR_SETEXITSIG, SIGKILL);
				_exit(status);
			}
    }
}

static void save_context_and_block(int sig)
{
PRThread *me = _PR_MD_CURRENT_THREAD();
_PRCPU *cpu = _PR_MD_CURRENT_CPU();

	/*
	 * save context
	 */
	(void) setjmp(me->md.jb);
	/*
	 * unblock the suspending thread
	 */
	if (me->cpu) {
		/*
		 * I am a cpu thread, not a user-created GLOBAL thread
		 */
		unblockproc(cpu->md.suspending_id);	
	} else {
		unblockproc(me->md.suspending_id);	
	}
	/*
	 * now, block current thread
	 */
	blockproc(getpid());
}

/*
** The irix kernel has a bug in it which causes async connect's which are
** interrupted by a signal to fail terribly (EADDRINUSE is returned). 
** We work around the bug by blocking signals during the async connect
** attempt.
*/
PRInt32 _MD_irix_connect(
    PRInt32 osfd, const PRNetAddr *addr, PRInt32 addrlen, PRIntervalTime timeout)
{
    PRInt32 rv;
    sigset_t oldset;

    sigprocmask(SIG_BLOCK, &ints_off, &oldset);
    rv = connect(osfd, addr, addrlen);
    sigprocmask(SIG_SETMASK, &oldset, 0);

    return(rv);
}

#include "prprf.h"

/********************************************************************/
/********************************************************************/
/*************** Various thread like things for IRIX ****************/
/********************************************************************/
/********************************************************************/

void *_MD_GetSP(PRThread *t)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    void *sp;

    if (me == t)
        (void) setjmp(t->md.jb);

    sp = (void *)(t->md.jb[JB_SP]);
    PR_ASSERT((sp >= (void *) t->stack->stackBottom) &&
        (sp <= (void *) (t->stack->stackBottom + t->stack->stackSize)));
    return(sp);
}

void _MD_InitLocks()
{
    char buf[200];
    char *init_users, *init_size;

    PR_snprintf(buf, sizeof(buf), "/dev/zero");

    if (init_users = getenv("_NSPR_IRIX_INITUSERS"))
        _irix_initusers = atoi(init_users);

    if (init_size = getenv("_NSPR_IRIX_INITSIZE"))
        _irix_initsize = atoi(init_size);

    usconfig(CONF_INITUSERS, _irix_initusers);
    usconfig(CONF_INITSIZE, _irix_initsize);
    usconfig(CONF_AUTOGROW, 1);
    usconfig(CONF_AUTORESV, 1);
	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0) {
		perror("PR_Init: unable to config mutex arena");
		exit(-1);
	}

    _pr_usArena = usinit(buf);
    if (!_pr_usArena) {
        fprintf(stderr,
            "PR_Init: Error - unable to create lock/monitor arena\n");
        exit(-1);
    }
    _pr_heapLock = usnewlock(_pr_usArena);
	_nspr_irix_lock_cnt++;

    arena_list_lock = usnewlock(_pr_usArena);
	_nspr_irix_lock_cnt++;

    sproc_list_lock = usnewlock(_pr_usArena);
	_nspr_irix_lock_cnt++;

	_pr_irix_exit_sem = usnewsema(_pr_usArena, 0);
	_nspr_irix_sem_cnt = 1;

	first_arena.usarena = _pr_usArena;
	PR_INIT_CLIST(&first_arena.links);
	PR_APPEND_LINK(&first_arena.links, &arena_list);
}

/* _PR_IRIX_CHILD_PROCESS is a private API for Server group */
void _PR_IRIX_CHILD_PROCESS()
{
extern PRUint32 _pr_global_threads;

    PR_ASSERT(_PR_MD_CURRENT_CPU() == _pr_primordialCPU);
    PR_ASSERT(_pr_numCPU == 1);
    PR_ASSERT(_pr_global_threads == 0);
    /*
     * save the new pid
     */
    _pr_primordialCPU->md.id = getpid();
	_MD_SET_SPROC_PID(getpid());	
}

static PRStatus pr_cvar_wait_sem(PRThread *thread, PRIntervalTime timeout)
{
    int rv;

#ifdef _PR_USE_POLL
	struct pollfd pfd;
	int msecs;

	if (timeout == PR_INTERVAL_NO_TIMEOUT)
		msecs = -1;
	else
		msecs  = PR_IntervalToMilliseconds(timeout);
#else
    struct timeval tv, *tvp;
    fd_set rd;

	if(timeout == PR_INTERVAL_NO_TIMEOUT)
		tvp = NULL;
	else {
		tv.tv_sec = PR_IntervalToSeconds(timeout);
		tv.tv_usec = PR_IntervalToMicroseconds(
		timeout - PR_SecondsToInterval(tv.tv_sec));
		tvp = &tv;
	}
	FD_ZERO(&rd);
	FD_SET(thread->md.cvar_pollsemfd, &rd);
#endif

    /*
     * call uspsema only if a previous select call on this semaphore
     * did not timeout
     */
    if (!thread->md.cvar_pollsem_select) {
        rv = _PR_WAIT_SEM(thread->md.cvar_pollsem);
		PR_ASSERT(rv >= 0);
	} else
        rv = 0;
again:
    if(!rv) {
#ifdef _PR_USE_POLL
		pfd.events = POLLIN;
		pfd.fd = thread->md.cvar_pollsemfd;
		rv = _MD_POLL(&pfd, 1, msecs);
#else
		rv = _MD_SELECT(thread->md.cvar_pollsemfd + 1, &rd, NULL,NULL,tvp);
#endif
        if ((rv == -1) && (errno == EINTR)) {
			rv = 0;
			goto again;
		}
		PR_ASSERT(rv >= 0);
	}

    if (rv > 0) {
        /*
         * acquired the semaphore, call uspsema next time
         */
        thread->md.cvar_pollsem_select = 0;
        return PR_SUCCESS;
    } else {
        /*
         * select timed out; must call select, not uspsema, when trying
         * to acquire the semaphore the next time
         */
        thread->md.cvar_pollsem_select = 1;
        return PR_FAILURE;
    }
}

PRStatus _MD_wait(PRThread *thread, PRIntervalTime ticks)
{
    if ( thread->flags & _PR_GLOBAL_SCOPE ) {
	_MD_CHECK_FOR_EXIT();
        if (pr_cvar_wait_sem(thread, ticks) == PR_FAILURE) {
	    _MD_CHECK_FOR_EXIT();
            /*
             * wait timed out
             */
            _PR_THREAD_LOCK(thread);
            if (thread->wait.cvar) {
                /*
                 * The thread will remove itself from the waitQ
                 * of the cvar in _PR_WaitCondVar
                 */
                thread->wait.cvar = NULL;
                thread->state =  _PR_RUNNING;
                _PR_THREAD_UNLOCK(thread);
            }  else {
                _PR_THREAD_UNLOCK(thread);
                /*
             * This thread was woken up by a notifying thread
             * at the same time as a timeout; so, consume the
             * extra post operation on the semaphore
             */
	        _MD_CHECK_FOR_EXIT();
            pr_cvar_wait_sem(thread, PR_INTERVAL_NO_TIMEOUT);
            }
	    _MD_CHECK_FOR_EXIT();
        }
    } else {
        _PR_MD_SWITCH_CONTEXT(thread);
    }
    return PR_SUCCESS;
}

PRStatus _MD_WakeupWaiter(PRThread *thread)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRIntn is;

	PR_ASSERT(_pr_md_idle_cpus >= 0);
    if (thread == NULL) {
		if (_pr_md_idle_cpus)
        	_MD_Wakeup_CPUs();
    } else if (!_PR_IS_NATIVE_THREAD(thread)) {
		if (_pr_md_idle_cpus)
       		_MD_Wakeup_CPUs();
    } else {
		PR_ASSERT(_PR_IS_NATIVE_THREAD(thread));
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_INTSOFF(is);
		_MD_CVAR_POST_SEM(thread);
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_FAST_INTSON(is);
    } 
    return PR_SUCCESS;
}

void create_sproc (void (*entry) (void *, size_t), unsigned inh,
					void *arg, caddr_t sp, size_t len, int *pid)
{
sproc_params sparams;
char data;
int rv;
PRThread *me = _PR_MD_CURRENT_THREAD();

	if (!_PR_IS_NATIVE_THREAD(me) && (_PR_MD_CURRENT_CPU()->id == 0)) {
		*pid = sprocsp(entry,		/* startup func		*/
						inh,        /* attribute flags	*/
						arg,     	/* thread param		*/
						sp,         /* stack address	*/
						len);       /* stack size		*/
	} else {
		sparams.sd.entry = entry;
		sparams.sd.inh = inh;
		sparams.sd.arg = arg;
		sparams.sd.sp = sp;
		sparams.sd.len = len;
		sparams.sd.pid = pid;
		sparams.sd.creator_pid = getpid();
		_PR_LOCK(sproc_list_lock);
		PR_APPEND_LINK(&sparams.links, &sproc_list);
		rv = write(_pr_irix_primoridal_cpu_fd[1], &data, 1);
		PR_ASSERT(rv == 1);
		_PR_UNLOCK(sproc_list_lock);
		blockproc(getpid());
	}
}

/*
 * _PR_MD_WAKEUP_PRIMORDIAL_CPU
 *
 *		wakeup cpu 0
 */

void _PR_MD_WAKEUP_PRIMORDIAL_CPU()
{
char data = '0';
int rv;

	rv = write(_pr_irix_primoridal_cpu_fd[1], &data, 1);
	PR_ASSERT(rv == 1);
}

/*
 * _PR_MD_primordial_cpu
 *
 *		process events that need to executed by the primordial cpu on each
 *		iteration through the idle loop
 */

void _PR_MD_primordial_cpu()
{
PRCList *qp;
sproc_params *sp;
int pid;

	_PR_LOCK(sproc_list_lock);
	while ((qp = sproc_list.next) != &sproc_list) {
		sp = SPROC_PARAMS_PTR(qp);
		PR_REMOVE_LINK(&sp->links);
		pid = sp->sd.creator_pid;
		(*(sp->sd.pid)) = sprocsp(sp->sd.entry,		/* startup func    */
							sp->sd.inh,            	/* attribute flags     */
							sp->sd.arg,     		/* thread param     */
							sp->sd.sp,             	/* stack address    */
							sp->sd.len);         	/* stack size     */
		unblockproc(pid);
	}
	_PR_UNLOCK(sproc_list_lock);
}

PRStatus _MD_CreateThread(PRThread *thread, 
void (*start)(void *), 
PRThreadPriority priority, 
PRThreadScope scope, 
PRThreadState state, 
PRUint32 stackSize)
{
    typedef void (*SprocEntry) (void *, size_t);
    SprocEntry spentry = (SprocEntry)start;
    PRIntn is;
	PRThread *me = _PR_MD_CURRENT_THREAD();	
	PRInt32 pid;
	PRStatus rv;

	if (!_PR_IS_NATIVE_THREAD(me))
		_PR_INTSOFF(is);
    thread->md.cvar_pollsem_select = 0;
    thread->flags |= _PR_GLOBAL_SCOPE;

	thread->md.cvar_pollsemfd = -1;
	if (new_poll_sem(&thread->md,0) == PR_FAILURE) {
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_FAST_INTSON(is);
		return PR_FAILURE;
	}
	thread->md.cvar_pollsemfd =
		_PR_OPEN_POLL_SEM(thread->md.cvar_pollsem);
	if ((thread->md.cvar_pollsemfd < 0)) {
		free_poll_sem(&thread->md);
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_FAST_INTSON(is);
		return PR_FAILURE;
	}

    create_sproc(spentry,            /* startup func    */
    			PR_SALL,            /* attribute flags     */
    			(void *)thread,     /* thread param     */
    			NULL,               /* stack address    */
    			stackSize, &pid);         /* stack size     */
    if (pid > 0) {
        _MD_ATOMIC_INCREMENT(&_pr_md_irix_sprocs_created);
        _MD_ATOMIC_INCREMENT(&_pr_md_irix_sprocs);
		rv = PR_SUCCESS;
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_FAST_INTSON(is);
        return rv;
    } else {
        close(thread->md.cvar_pollsemfd);
        thread->md.cvar_pollsemfd = -1;
		free_poll_sem(&thread->md);
        thread->md.cvar_pollsem = NULL;
        _MD_ATOMIC_INCREMENT(&_pr_md_irix_sprocs_failed);
		if (!_PR_IS_NATIVE_THREAD(me))
			_PR_FAST_INTSON(is);
        return PR_FAILURE;
    }
}

void _MD_CleanThread(PRThread *thread)
{
    if (thread->flags & _PR_GLOBAL_SCOPE) {
        close(thread->md.cvar_pollsemfd);
        thread->md.cvar_pollsemfd = -1;
		free_poll_sem(&thread->md);
        thread->md.cvar_pollsem = NULL;
    }
}

void _MD_SetPriority(_MDThread *thread, PRThreadPriority newPri)
{
    return;
}

extern void _MD_unix_terminate_waitpid_daemon(void);

void
_MD_CleanupBeforeExit(void)
{
    extern PRInt32    _pr_cpus_exit;

    _MD_unix_terminate_waitpid_daemon();

	_pr_irix_exit_now = 1;
    if (_pr_numCPU > 1) {
        /*
         * Set a global flag, and wakeup all cpus which will notice the flag
         * and exit.
         */
        _pr_cpus_exit = getpid();
        _MD_Wakeup_CPUs();
        while(_pr_numCPU > 1) {
            _PR_WAIT_SEM(_pr_irix_exit_sem);
            _pr_numCPU--;
        }
    }
    /*
     * cause global threads on the recycle list to exit
     */
     _PR_DEADQ_LOCK;
     if (_PR_NUM_DEADNATIVE != 0) {
	PRThread *thread;
    	PRCList *ptr;

        ptr = _PR_DEADNATIVEQ.next;
        while( ptr != &_PR_DEADNATIVEQ ) {
        	thread = _PR_THREAD_PTR(ptr);
		_MD_CVAR_POST_SEM(thread);
                ptr = ptr->next;
        } 
     }
     _PR_DEADQ_UNLOCK;
     while(_PR_NUM_DEADNATIVE > 1) {
	_PR_WAIT_SEM(_pr_irix_exit_sem);
	_PR_DEC_DEADNATIVE;
     }
}

#ifdef _PR_HAVE_SGI_PRDA_PROCMASK
extern void __sgi_prda_procmask(int);
#endif

PRStatus
_MD_InitAttachedThread(PRThread *thread, PRBool wakeup_parent)
{
	PRStatus rv = PR_SUCCESS;

    if (thread->flags & _PR_GLOBAL_SCOPE) {
		if (new_poll_sem(&thread->md,0) == PR_FAILURE) {
			return PR_FAILURE;
		}
		thread->md.cvar_pollsemfd =
			_PR_OPEN_POLL_SEM(thread->md.cvar_pollsem);
		if ((thread->md.cvar_pollsemfd < 0)) {
			free_poll_sem(&thread->md);
			return PR_FAILURE;
		}
		if (_MD_InitThread(thread, PR_FALSE) == PR_FAILURE) {
			close(thread->md.cvar_pollsemfd);
			thread->md.cvar_pollsemfd = -1;
			free_poll_sem(&thread->md);
			thread->md.cvar_pollsem = NULL;
			return PR_FAILURE;
		}
    }
	return rv;
}

PRStatus
_MD_InitThread(PRThread *thread, PRBool wakeup_parent)
{
    struct sigaction sigact;
	PRStatus rv = PR_SUCCESS;

    if (thread->flags & _PR_GLOBAL_SCOPE) {
		thread->md.id = getpid();
        setblockproccnt(thread->md.id, 0);
		_MD_SET_SPROC_PID(getpid());	
#ifdef _PR_HAVE_SGI_PRDA_PROCMASK
		/*
		 * enable user-level processing of sigprocmask(); this is an
		 * undocumented feature available in Irix 6.2, 6.3, 6.4 and 6.5
		 */
		__sgi_prda_procmask(USER_LEVEL);
#endif
		/*
		 * set up SIGUSR1 handler; this is used to save state
		 */
		sigact.sa_handler = save_context_and_block;
		sigact.sa_flags = SA_RESTART;
		/*
		 * Must mask clock interrupts
		 */
		sigact.sa_mask = timer_set;
		sigaction(SIGUSR1, &sigact, 0);


		/*
		 * PR_SETABORTSIG is a new command implemented in a patch to
		 * Irix 6.2, 6.3 and 6.4. This causes a signal to be sent to all
		 * sprocs in the process when one of them terminates abnormally
		 *
		 */
		if (prctl(PR_SETABORTSIG, SIGKILL) < 0) {
			/*
			 *  if (errno == EINVAL)
			 *
			 *	PR_SETABORTSIG not supported under this OS.
			 *	You may want to get a recent kernel rollup patch that
			 *	supports this feature.
			 */
		}
		/*
		 * SIGCLD handler for detecting abormally-terminating
		 * sprocs and for reaping sprocs
		 */
		sigact.sa_handler = sigchld_handler;
		sigact.sa_flags = SA_RESTART;
		sigact.sa_mask = ints_off;
		sigaction(SIGCLD, &sigact, NULL);
    }
	return rv;
}

/*
 * PR_Cleanup should be executed on the primordial sproc; migrate the thread
 * to the primordial cpu
 */

void _PR_MD_PRE_CLEANUP(PRThread *me)
{
PRIntn is;
_PRCPU *cpu = _pr_primordialCPU;

	PR_ASSERT(cpu);

	me->flags |= _PR_BOUND_THREAD;	

	if (me->cpu->id != 0) {
		_PR_INTSOFF(is);
		_PR_RUNQ_LOCK(cpu);
		me->cpu = cpu;
		me->state = _PR_RUNNABLE;
		_PR_ADD_RUNQ(me, cpu, me->priority);
		_PR_RUNQ_UNLOCK(cpu);
		_MD_Wakeup_CPUs();

		_PR_MD_SWITCH_CONTEXT(me);

		_PR_FAST_INTSON(is);
		PR_ASSERT(me->cpu->id == 0);
	}
}

/*
 * process exiting
 */
PR_EXTERN(void ) _MD_exit(PRIntn status)
{
PRThread *me = _PR_MD_CURRENT_THREAD();

	/*
	 * the exit code of the process is the exit code of the primordial
	 * sproc
	 */
	if (!_PR_IS_NATIVE_THREAD(me) && (_PR_MD_CURRENT_CPU()->id == 0)) {
		/*
		 * primordial sproc case: call _exit directly
		 * Cause SIGKILL to be sent to other sprocs
		 */
		prctl(PR_SETEXITSIG, SIGKILL);
		_exit(status);
	} else {
		int rv;
		char data;
		sigset_t set;

		/*
		 * non-primordial sproc case: cause the primordial sproc, cpu 0,
		 * to wakeup and call _exit
		 */
		_pr_irix_process_exit = 1;
		_pr_irix_process_exit_code = status;
		rv = write(_pr_irix_primoridal_cpu_fd[1], &data, 1);
		PR_ASSERT(rv == 1);
		/*
		 * block all signals and wait for SIGKILL to terminate this sproc
		 */
		sigfillset(&set);
		sigsuspend(&set);
		/*
		 * this code doesn't (shouldn't) execute
		 */
		prctl(PR_SETEXITSIG, SIGKILL);
		_exit(status);
	}
}

/*
 * Override the exit() function in libc to cause the process to exit
 * when the primodial/main nspr thread calls exit. Calls to exit by any
 * other thread simply result in a call to the exit function in libc.
 * The exit code of the process is the exit code of the primordial
 * sproc.
 */

void exit(int status)
{
PRThread *me, *thr;
PRCList *qp;

	if (!_pr_initialized)  {
		if (!libc_exit) {

			if (!libc_handle)
				libc_handle = dlopen("libc.so",RTLD_NOW);
			if (libc_handle)
				libc_exit = (void (*)(int)) dlsym(libc_handle, "exit");
		}
		if (libc_exit)
			(*libc_exit)(status);
		else
			_exit(status);
	}

	me = _PR_MD_CURRENT_THREAD();

	if (me == NULL) 		/* detached thread */
		(*libc_exit)(status);

	PR_ASSERT(_PR_IS_NATIVE_THREAD(me) ||
						(_PR_MD_CURRENT_CPU())->id == me->cpu->id);

	if (me->flags & _PR_PRIMORDIAL) {

		me->flags |= _PR_BOUND_THREAD;	

		PR_ASSERT((_PR_MD_CURRENT_CPU())->id == me->cpu->id);
		if (me->cpu->id != 0) {
			_PRCPU *cpu = _pr_primordialCPU;
			PRIntn is;

			_PR_INTSOFF(is);
			_PR_RUNQ_LOCK(cpu);
			me->cpu = cpu;
			me->state = _PR_RUNNABLE;
			_PR_ADD_RUNQ(me, cpu, me->priority);
			_PR_RUNQ_UNLOCK(cpu);
			_MD_Wakeup_CPUs();

			_PR_MD_SWITCH_CONTEXT(me);

			_PR_FAST_INTSON(is);
		}

		PR_ASSERT((_PR_MD_CURRENT_CPU())->id == 0);

		if (prctl(PR_GETNSHARE) > 1) {
#define SPROC_EXIT_WAIT_TIME 5
			int sleep_cnt = SPROC_EXIT_WAIT_TIME;

			/*
			 * sprocs still running; caue cpus and recycled global threads
			 * to exit
			 */
			_pr_irix_exit_now = 1;
			if (_pr_numCPU > 1) {
				_MD_Wakeup_CPUs();
			}
			 _PR_DEADQ_LOCK;
			 if (_PR_NUM_DEADNATIVE != 0) {
				PRThread *thread;
				PRCList *ptr;

				ptr = _PR_DEADNATIVEQ.next;
				while( ptr != &_PR_DEADNATIVEQ ) {
					thread = _PR_THREAD_PTR(ptr);
					_MD_CVAR_POST_SEM(thread);
					ptr = ptr->next;
				} 
			 }

			while (sleep_cnt-- > 0) {
				if (waitpid(0, NULL, WNOHANG) >= 0) 
					sleep(1);
				else
					break;
			}
			prctl(PR_SETEXITSIG, SIGKILL);
		}
		(*libc_exit)(status);
	} else {
		/*
		 * non-primordial thread; simply call exit in libc.
		 */
		(*libc_exit)(status);
	}
}


void
_MD_InitRunningCPU(_PRCPU *cpu)
{
    extern int _pr_md_pipefd[2];

    _MD_unix_init_running_cpu(cpu);
    cpu->md.id = getpid();
	_MD_SET_SPROC_PID(getpid());	
	if (_pr_md_pipefd[0] >= 0) {
    	_PR_IOQ_MAX_OSFD(cpu) = _pr_md_pipefd[0];
#ifndef _PR_USE_POLL
    	FD_SET(_pr_md_pipefd[0], &_PR_FD_READ_SET(cpu));
#endif
	}
}

void
_MD_ExitThread(PRThread *thread)
{
    if (thread->flags & _PR_GLOBAL_SCOPE) {
        _MD_ATOMIC_DECREMENT(&_pr_md_irix_sprocs);
        _MD_CLEAN_THREAD(thread);
        _MD_SET_CURRENT_THREAD(NULL);
    }
}

void
_MD_SuspendCPU(_PRCPU *cpu)
{
    PRInt32 rv;

	cpu->md.suspending_id = getpid();
	rv = kill(cpu->md.id, SIGUSR1);
	PR_ASSERT(rv == 0);
	/*
	 * now, block the current thread/cpu until woken up by the suspended
	 * thread from it's SIGUSR1 signal handler
	 */
	blockproc(getpid());

}

void
_MD_ResumeCPU(_PRCPU *cpu)
{
    unblockproc(cpu->md.id);
}

#if 0
/*
 * save the register context of a suspended sproc
 */
void get_context(PRThread *thr)
{
    int len, fd;
    char pidstr[24];
    char path[24];

    /*
     * open the file corresponding to this process in procfs
     */
    sprintf(path,"/proc/%s","00000");
    len = strlen(path);
    sprintf(pidstr,"%d",thr->md.id);
    len -= strlen(pidstr);
    sprintf(path + len,"%s",pidstr);
    fd = open(path,O_RDONLY);
    if (fd >= 0) {
        (void) ioctl(fd, PIOCGREG, thr->md.gregs);
        close(fd);
    }
    return;
}
#endif	/* 0 */

void
_MD_SuspendThread(PRThread *thread)
{
    PRInt32 rv;

    PR_ASSERT((thread->flags & _PR_GLOBAL_SCOPE) &&
        (thread->flags & _PR_GCABLE_THREAD));

	thread->md.suspending_id = getpid();
	rv = kill(thread->md.id, SIGUSR1);
	PR_ASSERT(rv == 0);
	/*
	 * now, block the current thread/cpu until woken up by the suspended
	 * thread from it's SIGUSR1 signal handler
	 */
	blockproc(getpid());
}

void
_MD_ResumeThread(PRThread *thread)
{
    PR_ASSERT((thread->flags & _PR_GLOBAL_SCOPE) &&
        (thread->flags & _PR_GCABLE_THREAD));
    (void)unblockproc(thread->md.id);
}

/*
 * return the set of processors available for scheduling procs in the
 * "mask" argument
 */
PRInt32 _MD_GetThreadAffinityMask(PRThread *unused, PRUint32 *mask)
{
    PRInt32 nprocs, rv;
    struct pda_stat *pstat;
#define MAX_PROCESSORS    32

    nprocs = sysmp(MP_NPROCS);
    if (nprocs < 0)
        return(-1);
    pstat = (struct pda_stat*)PR_MALLOC(sizeof(struct pda_stat) * nprocs);
    if (pstat == NULL)
        return(-1);
    rv = sysmp(MP_STAT, pstat);
    if (rv < 0) {
        PR_DELETE(pstat);
        return(-1);
    }
    /*
     * look at the first 32 cpus
     */
    nprocs = (nprocs > MAX_PROCESSORS) ? MAX_PROCESSORS : nprocs;
    *mask = 0;
    while (nprocs) {
        if ((pstat->p_flags & PDAF_ENABLED) &&
            !(pstat->p_flags & PDAF_ISOLATED)) {
            *mask |= (1 << pstat->p_cpuid);
        }
        nprocs--;
        pstat++;
    }
    return 0;
}

static char *_thr_state[] = {
    "UNBORN",
    "RUNNABLE",
    "RUNNING",
    "LOCK_WAIT",
    "COND_WAIT",
    "JOIN_WAIT",
    "IO_WAIT",
    "SUSPENDED",
    "DEAD"
};

void _PR_List_Threads()
{
    PRThread *thr;
    void *handle;
    struct _PRCPU *cpu;
    PRCList *qp;
    int len, fd;
    char pidstr[24];
    char path[24];
    prpsinfo_t pinfo;


    printf("\n%s %-s\n"," ","LOCAL Threads");
    printf("%s %-s\n"," ","----- -------");
    printf("%s %-14s %-10s %-12s %-3s %-10s %-10s %-12s\n\n"," ",
        "Thread", "State", "Wait-Handle",
        "Cpu","Stk-Base","Stk-Sz","SP");
    for (qp = _PR_ACTIVE_LOCAL_THREADQ().next;
        qp != &_PR_ACTIVE_LOCAL_THREADQ(); qp = qp->next) {
        thr = _PR_ACTIVE_THREAD_PTR(qp);
        printf("%s 0x%-12x %-10s "," ",thr,_thr_state[thr->state]);
        if (thr->state == _PR_LOCK_WAIT)
            handle = thr->wait.lock;
        else if (thr->state == _PR_COND_WAIT)
            handle = thr->wait.cvar;
        else
            handle = NULL;
        if (handle)
            printf("0x%-10x ",handle);
        else
            printf("%-12s "," ");
        printf("%-3d ",thr->cpu->id);
        printf("0x%-8x ",thr->stack->stackBottom);
        printf("0x%-8x ",thr->stack->stackSize);
        printf("0x%-10x\n",thr->md.jb[JB_SP]);
    }

    printf("\n%s %-s\n"," ","GLOBAL Threads");
    printf("%s %-s\n"," ","------ -------");
    printf("%s %-14s %-6s %-12s %-12s %-12s %-12s\n\n"," ","Thread",
        "Pid","State","Wait-Handle",
        "Stk-Base","Stk-Sz");

    for (qp = _PR_ACTIVE_GLOBAL_THREADQ().next;
        qp != &_PR_ACTIVE_GLOBAL_THREADQ(); qp = qp->next) {
        thr = _PR_ACTIVE_THREAD_PTR(qp);
        if (thr->cpu != NULL)
            continue;        /* it is a cpu thread */
        printf("%s 0x%-12x %-6d "," ",thr,thr->md.id);
        /*
         * check if the sproc is still running
         * first call prctl(PR_GETSHMASK,pid) to check if
         * the process is part of the share group (the pid
         * could have been recycled by the OS)
         */
        if (prctl(PR_GETSHMASK,thr->md.id) < 0) {
            printf("%-12s\n","TERMINATED");
            continue;
        }
        /*
         * Now, check if the sproc terminated and is in zombie
         * state
         */
        sprintf(path,"/proc/pinfo/%s","00000");
        len = strlen(path);
        sprintf(pidstr,"%d",thr->md.id);
        len -= strlen(pidstr);
        sprintf(path + len,"%s",pidstr);
        fd = open(path,O_RDONLY);
        if (fd >= 0) {
            if (ioctl(fd, PIOCPSINFO, &pinfo) < 0)
                printf("%-12s ","TERMINATED");
            else if (pinfo.pr_zomb)
                printf("%-12s ","TERMINATED");
            else
                printf("%-12s ",_thr_state[thr->state]);
            close(fd);
        } else {
            printf("%-12s ","TERMINATED");
        }

        if (thr->state == _PR_LOCK_WAIT)
            handle = thr->wait.lock;
        else if (thr->state == _PR_COND_WAIT)
            handle = thr->wait.cvar;
        else
            handle = NULL;
        if (handle)
            printf("%-12x ",handle);
        else
            printf("%-12s "," ");
        printf("0x%-10x ",thr->stack->stackBottom);
        printf("0x%-10x\n",thr->stack->stackSize);
    }

    printf("\n%s %-s\n"," ","CPUs");
    printf("%s %-s\n"," ","----");
    printf("%s %-14s %-6s %-12s \n\n"," ","Id","Pid","State");


    for (qp = _PR_CPUQ().next; qp != &_PR_CPUQ(); qp = qp->next) {
        cpu = _PR_CPU_PTR(qp);
        printf("%s %-14d %-6d "," ",cpu->id,cpu->md.id);
        /*
         * check if the sproc is still running
         * first call prctl(PR_GETSHMASK,pid) to check if
         * the process is part of the share group (the pid
         * could have been recycled by the OS)
         */
        if (prctl(PR_GETSHMASK,cpu->md.id) < 0) {
            printf("%-12s\n","TERMINATED");
            continue;
        }
        /*
         * Now, check if the sproc terminated and is in zombie
         * state
         */
        sprintf(path,"/proc/pinfo/%s","00000");
        len = strlen(path);
        sprintf(pidstr,"%d",cpu->md.id);
        len -= strlen(pidstr);
        sprintf(path + len,"%s",pidstr);
        fd = open(path,O_RDONLY);
        if (fd >= 0) {
            if (ioctl(fd, PIOCPSINFO, &pinfo) < 0)
                printf("%-12s\n","TERMINATED");
            else if (pinfo.pr_zomb)
                printf("%-12s\n","TERMINATED");
            else
                printf("%-12s\n","RUNNING");
            close(fd);
        } else {
            printf("%-12s\n","TERMINATED");
        }

    }
    fflush(stdout);
}
#endif /* defined(_PR_PTHREADS) */ 

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
#if !defined(_PR_PTHREADS)
    if (isCurrent) {
        (void) setjmp(t->md.jb);
    }
    *np = sizeof(t->md.jb) / sizeof(PRWord);
    return (PRWord *) (t->md.jb);
#else
	*np = 0;
	return NULL;
#endif
}

void _MD_EarlyInit(void)
{
#if !defined(_PR_PTHREADS)
    char *eval;
    int fd;
	extern int __ateachexit(void (*func)(void));

    sigemptyset(&ints_off);
    sigaddset(&ints_off, SIGALRM);
    sigaddset(&ints_off, SIGIO);
    sigaddset(&ints_off, SIGCLD);

    if (eval = getenv("_NSPR_TERMINATE_ON_ERROR"))
        _nspr_terminate_on_error = (0 == atoi(eval) == 0) ? PR_FALSE : PR_TRUE;

    fd = open("/dev/zero",O_RDWR , 0);
    if (fd < 0) {
        perror("open /dev/zero failed");
        exit(1);
    }
    /*
     * Set up the sproc private data area.
     * This region exists at the same address, _nspr_sproc_private, for
     * every sproc, but each sproc gets a private copy of the region.
     */
    _nspr_sproc_private = (char*)mmap(0, _pr_pageSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE| MAP_LOCAL, fd, 0);
    if (_nspr_sproc_private == (void*)-1) {
        perror("mmap /dev/zero failed");
        exit(1);
    }
	_MD_SET_SPROC_PID(getpid());	
    close(fd);
	__ateachexit(irix_detach_sproc);
#endif
    _MD_IrixIntervalInit();
}  /* _MD_EarlyInit */

void _MD_IrixInit()
{
#if !defined(_PR_PTHREADS)
    struct sigaction sigact;
    PRThread *me = _PR_MD_CURRENT_THREAD();
	int rv;

#ifdef _PR_HAVE_SGI_PRDA_PROCMASK
	/*
	 * enable user-level processing of sigprocmask(); this is an undocumented
	 * feature available in Irix 6.2, 6.3, 6.4 and 6.5
	 */
	__sgi_prda_procmask(USER_LEVEL);
#endif

	/*
	 * set up SIGUSR1 handler; this is used to save state
	 * during PR_SuspendAll
	 */
	sigact.sa_handler = save_context_and_block;
	sigact.sa_flags = SA_RESTART;
	sigact.sa_mask = ints_off;
	sigaction(SIGUSR1, &sigact, 0);

    /*
     * Change the name of the core file from core to core.pid,
     * This is inherited by the sprocs created by this process
     */
#ifdef PR_COREPID
    prctl(PR_COREPID, 0, 1);
#endif
    /*
     * Irix-specific terminate on error processing
     */
	/*
	 * PR_SETABORTSIG is a new command implemented in a patch to
	 * Irix 6.2, 6.3 and 6.4. This causes a signal to be sent to all
	 * sprocs in the process when one of them terminates abnormally
	 *
	 */
	if (prctl(PR_SETABORTSIG, SIGKILL) < 0) {
		/*
		 *  if (errno == EINVAL)
		 *
		 *	PR_SETABORTSIG not supported under this OS.
		 *	You may want to get a recent kernel rollup patch that
		 *	supports this feature.
		 *
		 */
	}
	/*
	 * PR_SETEXITSIG -  send the SIGCLD signal to the parent
	 *            sproc when any sproc terminates
	 *
	 *    This is used to cause the entire application to
	 *    terminate when    any sproc terminates abnormally by
	 *     receipt of a SIGSEGV, SIGBUS or SIGABRT signal.
	 *    If this is not done, the application may seem
	 *     "hung" to the user because the other sprocs may be
	 *    waiting for resources held by the
	 *    abnormally-terminating sproc.
	 */
	prctl(PR_SETEXITSIG, 0);

	sigact.sa_handler = sigchld_handler;
	sigact.sa_flags = SA_RESTART;
	sigact.sa_mask = ints_off;
	sigaction(SIGCLD, &sigact, NULL);

    /*
     * setup stack fields for the primordial thread
     */
    me->stack->stackSize = prctl(PR_GETSTACKSIZE);
    me->stack->stackBottom = me->stack->stackTop - me->stack->stackSize;

    rv = pipe(_pr_irix_primoridal_cpu_fd);
    PR_ASSERT(rv == 0);
#ifndef _PR_USE_POLL
    _PR_IOQ_MAX_OSFD(me->cpu) = _pr_irix_primoridal_cpu_fd[0];
    FD_SET(_pr_irix_primoridal_cpu_fd[0], &_PR_FD_READ_SET(me->cpu));
#endif

	libc_handle = dlopen("libc.so",RTLD_NOW);
	PR_ASSERT(libc_handle != NULL);
	libc_exit = (void (*)(int)) dlsym(libc_handle, "exit");
	PR_ASSERT(libc_exit != NULL);
	/* dlclose(libc_handle); */

#endif /* _PR_PTHREADS */

    _PR_UnixInit();
}

/**************************************************************************/
/************** code and such for NSPR 2.0's interval times ***************/
/**************************************************************************/

#define PR_PSEC_PER_SEC 1000000000000ULL  /* 10^12 */

#ifndef SGI_CYCLECNTR_SIZE
#define SGI_CYCLECNTR_SIZE      165     /* Size user needs to use to read CC */
#endif

static PRIntn mmem_fd = -1;
static PRIntn clock_width = 0;
static void *iotimer_addr = NULL;
static PRUint32 pr_clock_mask = 0;
static PRUint32 pr_clock_shift = 0;
static PRIntervalTime pr_ticks = 0;
static PRUint32 pr_clock_granularity = 1;
static PRUint32 pr_previous = 0, pr_residual = 0;
static PRUint32 pr_ticks_per_second = 0;

extern PRIntervalTime _PR_UNIX_GetInterval(void);
extern PRIntervalTime _PR_UNIX_TicksPerSecond(void);

static void _MD_IrixIntervalInit()
{
    /*
     * As much as I would like, the service available through this
     * interface on R3000's (aka, IP12) just isn't going to make it.
     * The register is only 24 bits wide, and rolls over at a verocious
     * rate.
     */
    PRUint32 one_tick = 0;
    struct utsname utsinfo;
    uname(&utsinfo);
    if ((strncmp("IP12", utsinfo.machine, 4) != 0)
        && ((mmem_fd = open("/dev/mmem", O_RDONLY)) != -1))
    {
        int poffmask = getpagesize() - 1;
        __psunsigned_t phys_addr, raddr, cycleval;

        phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
        raddr = phys_addr & ~poffmask;
        iotimer_addr = mmap(
            0, poffmask, PROT_READ, MAP_PRIVATE, mmem_fd, (__psint_t)raddr);

        clock_width = syssgi(SGI_CYCLECNTR_SIZE);
        if (clock_width < 0)
        {
            /* 
             * We must be executing on a 6.0 or earlier system, since the
             * SGI_CYCLECNTR_SIZE call is not supported.
             * 
             * The only pre-6.1 platforms with 64-bit counters are
             * IP19 and IP21 (Challenge, PowerChallenge, Onyx).
             */
            if (!strncmp(utsinfo.machine, "IP19", 4) ||
                !strncmp(utsinfo.machine, "IP21", 4))
                clock_width = 64;
            else
                clock_width = 32;
        }

        /*
         * 'cycleval' is picoseconds / increment of the counter.
         * I'm pushing for a tick to be 100 microseconds, 10^(-4).
         * That leaves 10^(-8) left over, or 10^8 / cycleval.
         * Did I do that right?
         */

        one_tick =  100000000UL / cycleval ;  /* 100 microseconds */

        while (0 != one_tick)
        {
            pr_clock_shift += 1;
            one_tick = one_tick >> 1;
            pr_clock_granularity = pr_clock_granularity << 1;
        }
        pr_clock_mask = pr_clock_granularity - 1;  /* to make a mask out of it */
        pr_ticks_per_second = PR_PSEC_PER_SEC
                / ((PRUint64)pr_clock_granularity * (PRUint64)cycleval);
            
        iotimer_addr = (void*)
            ((__psunsigned_t)iotimer_addr + (phys_addr & poffmask));
    }
    else
    {
        pr_ticks_per_second = _PR_UNIX_TicksPerSecond();
    }
}  /* _MD_IrixIntervalInit */

PRIntervalTime _MD_IrixIntervalPerSec()
{
    return pr_ticks_per_second;
}

PRIntervalTime _MD_IrixGetInterval()
{
    if (mmem_fd != -1)
    {
        if (64 == clock_width)
        {
            PRUint64 temp = *(PRUint64*)iotimer_addr;
            pr_ticks = (PRIntervalTime)(temp >> pr_clock_shift);
        }
        else
        {
            PRIntervalTime ticks = pr_ticks;
            PRUint32 now = *(PRUint32*)iotimer_addr, temp;
            PRUint32 residual = pr_residual, previous = pr_previous;

            temp = now - previous + residual;
            residual = temp & pr_clock_mask;
            ticks += temp >> pr_clock_shift;

            pr_previous = now;
            pr_residual = residual;
            pr_ticks = ticks;
        }
    }
    else
    {
        /*
         * No fast access. Use the time of day clock. This isn't the
         * right answer since this clock can get set back, tick at odd
         * rates, and it's expensive to acqurie.
         */
        pr_ticks = _PR_UNIX_GetInterval();
    }
    return pr_ticks;
}  /* _MD_IrixGetInterval */

