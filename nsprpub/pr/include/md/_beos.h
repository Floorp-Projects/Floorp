/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nspr_beos_defs_h___
#define nspr_beos_defs_h___

#include "prtypes.h"
#include "prio.h"
#include "prthread.h"
#include "prproces.h"
#include "prmem.h"
#include "obsolete/prsem.h"
#include <errno.h>

#include <support/SupportDefs.h>
#include <kernel/OS.h>
#include <dirent.h>

/*
 * Internal configuration macros
 */

#ifdef BONE_VERSION
#define _PR_HAVE_SOCKADDR_LEN
#endif

#define PR_LINKER_ARCH	"beos"
#define _PR_SI_SYSNAME  "BEOS"
#ifdef __powerpc__
#define _PR_SI_ARCHITECTURE "ppc"
#else
#define _PR_SI_ARCHITECTURE "x86"
#endif
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE              0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef	HAVE_STACK_GROWING_UP
#define HAVE_DLL
#define _PR_NO_CLOCK_TIMER

/*
 * The Atomic operations
 */

#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC _MD_AtomicInit
#define _MD_ATOMIC_INCREMENT _MD_AtomicIncrement
#define _MD_ATOMIC_ADD _MD_AtomicAdd
#define _MD_ATOMIC_DECREMENT _MD_AtomicDecrement
#define _MD_ATOMIC_SET _MD_AtomicSet

#define HAVE_CVAR_BUILT_ON_SEM
#define _PR_GLOBAL_THREADS_ONLY
#define _PR_BTHREADS
#define _PR_NEED_FAKE_POLL
#define _PR_HAVE_PEEK_BUFFER
#define _PR_PEEK_BUFFER_MAX (16 * 1024)
#define _PR_FD_NEED_EMULATE_MSG_PEEK(fd) 1
#define _PR_CONNECT_DOES_NOT_BIND

/* Define threading functions and objects as native BeOS */
struct _MDThread {
    thread_id	tid;	/* BeOS thread handle */
	sem_id		joinSem;	/* sems used to synchronzie joining */
	PRBool	is_joining;	/* TRUE if someone is currently waiting to
						   join this thread */
};

struct _MDThreadStack {
    PRInt8	notused;
};

/*
 * Lock and Semaphore related definitions
 */

struct _MDLock {
    sem_id semaphoreID;
    int32  benaphoreCount;
};

struct _MDCVar {
    sem_id sem1;
    sem_id sem2;
    int16  count;
};

struct _MDSemaphore {
    sem_id sid;
};

/*
** CPU-related definitions
*/
struct _MDCPU {
    int8		unused;
};

/*
** Process-related definitions
*/
struct _MDProcess {
    pid_t pid;
};

struct _MDSegment {
    PRInt8 notused;
};

/*
** File- and directory-related definitions
*/

#ifndef BONE_VERSION
#define BE_SOCK_SHUTDOWN_READ	0x01
#define BE_SOCK_SHUTDOWN_WRITE	0x02
#endif

struct _MDFileDesc {
    PRInt32	osfd;
    PRInt32	sock_state;
    PRBool	accepted_socket;
    PRNetAddr	peer_addr;
#ifndef BONE_VERSION
    PRBool	connectValueValid;
    int		connectReturnValue;
    int		connectReturnError;
#endif
};

struct _MDDir {
    DIR		*d;
};

#define PR_DIRECTORY_SEPARATOR		'/'
#define PR_DIRECTORY_SEPARATOR_STR	"/"
#define PR_PATH_SEPARATOR		':'
#define PR_PATH_SEPARATOR_STR		":"

#define GETTIMEOFDAY(tp)	gettimeofday((tp), NULL)

/* --- Memory-mapped files stuff --- not implemented on BeOS */

struct _MDFileMap {
    PRInt8 unused;
};

/*
 * Network related definitions.
 */

#ifndef BONE_VERSION
#define IPPROTO_IP 0
#define AF_UNIX 2
#define TCP_NODELAY SO_NONBLOCK
#define SO_LINGER -1
#define SO_ERROR 4
#endif

#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5

#ifndef BONE_VERSION
/* these aren't actually used. if they are, we're screwed */
struct  protoent {
    char    *p_name;        /* official protocol name */
    char    **p_aliases;    /* alias list */
    int     p_proto;        /* protocol # */
};

struct protoent* getprotobyname(const char* name);
struct protoent* getprotobynumber(int number);
#endif

/*
 * malloc() related definitions.
 */

#undef _PR_OVERRIDE_MALLOC

/* Miscellaneous */

#define _MD_ERRNO()             (errno)

#define _MD_CLEANUP_BEFORE_EXIT _MD_cleanup_before_exit
#define _MD_EXIT _MD_exit

#define _MD_GET_ENV getenv
#define _MD_PUT_ENV putenv

#define _MD_EARLY_INIT _MD_early_init
#define _MD_FINAL_INIT _MD_final_init

/* CPU Stuff */

#define _MD_INIT_CPUS _MD_init_cpus
#define _MD_WAKEUP_CPUS _MD_wakeup_cpus
#define _MD_START_INTERRUPTS _MD_start_interrupts
#define _MD_STOP_INTERRUPTS _MD_stop_interrupts
#define _MD_DISABLE_CLOCK_INTERRUPTS _MD_disable_clock_interrupts
#define _MD_BLOCK_CLOCK_INTERRUPTS _MD_block_clock_interrupts
#define _MD_UNBLOCK_CLOCK_INTERRUPTS _MD_unblock_clock_interrupts
#define _MD_CLOCK_INTERRUPT _MD_clock_interrupt
#define _MD_INIT_STACK _MD_init_stack
#define _MD_CLEAR_STACK _MD_clear_stack
// #define _MD_GET_INTSOFF _MD_get_intsoff
// #define _MD_SET_INTSOFF _MD_set_intsoff
#define _MD_CURRENT_CPU _MD_current_cpu
#define _MD_SET_CURRENT_CPU _MD_set_current_cpu
#define _MD_INIT_RUNNING_CPU _MD_init_running_cpu
#define _MD_PAUSE_CPU _MD_pause_cpu

/* Thread stuff */

#define _MD_CURRENT_THREAD() PR_GetCurrentThread()
// #define _MD_GET_ATTACHED_THREAD _MD_get_attached_thread
#define _MD_LAST_THREAD _MD_last_thread
#define _MD_SET_CURRENT_THREAD _MD_set_current_THREAD
#define _MD_SET_LAST_THREAD _MD_set_last_thread
#define _MD_INIT_THREAD _MD_init_thread
#define _MD_EXIT_THREAD _MD_exit_thread
#define _MD_INIT_ATTACHED_THREAD _MD_init_attached_thread

#define _MD_SUSPEND_THREAD _MD_suspend_thread
#define _MD_RESUME_THREAD _MD_resume_thread
#define _MD_SUSPEND_CPU _MD_suspend_cpu
#define _MD_RESUME_CPU _MD_resume_cpu
#define _MD_BEGIN_SUSPEND_ALL _MD_begin_suspend_all
#define _MD_END_SUSPEND_ALL _MD_end_suspend_all
#define _MD_BEGIN_RESUME_ALL _MD_begin_resume_all
#define _MD_END_RESUME_ALL _MD_end_resume_all

#define _MD_GET_SP _MD_get_sp

#define _MD_CLEAN_THREAD _MD_clean_thread
#define _MD_CREATE_PRIMORDIAL_USER_THREAD _MD_create_primordial_user_thread
#define _MD_CREATE_USER_THREAD _MD_create_user_thread
#define _MD_INIT_PRIMORDIAL_THREAD _MD_init_primordial_thread
#define _MD_CREATE_THREAD _MD_create_thread
#define _MD_YIELD _MD_yield
#define _MD_SET_PRIORITY _MD_set_priority

#define _MD_SUSPENDALL _MD_suspendall
#define _MD_RESUMEALL _MD_resumeall

#define _MD_SWITCH_CONTEXT _MD_switch_context
#define _MD_RESTORE_CONTEXT _MD_restore_context

#define _MD_WAIT _MD_wait
#define _MD_WAKEUP_WAITER _MD_wakeup_waiter

#define _MD_SETTHREADAFFINITYMASK _MD_setthreadaffinitymask
#define _MD_GETTHREADAFFINITYMASK _MD_getthreadaffinitymask

/* Thread Synchronization */

#define _MD_INIT_LOCKS _MD_init_locks
#define _MD_NEW_LOCK _MD_new_lock
#define _MD_FREE_LOCK _MD_free_lock
#define _MD_LOCK _MD_lock
#define _MD_TEST_AND_LOCK _MD_test_and_lock
#define _MD_UNLOCK _MD_unlock
#define _MD_IOQ_LOCK _MD_ioq_lock
#define _MD_IOQ_UNLOCK _MD_ioq_unlock
#define _MD_NEW_SEM _MD_new_sem
#define _MD_DESTROY_SEM _MD_destroy_sem
#define _MD_TIMED_WAIT_SEM _MD_timed_wait_sem
#define _MD_WAIT_SEM _MD_wait_sem
#define _MD_POST_SEM _MD_post_sem
// #define _MD_NEW_CV _MD_new_cv
// #define _MD_FREE_CV _MD_free_cv
// #define _MD_WAIT_CV _MD_wait_cv
// #define _MD_NOTIFY_CV _MD_notify_cv
// #define _MD_NOTIFYALL_CV _MD_notifyall_cv

/* File I/O */

/* don't need any I/O initializations */
#define _MD_INIT_IO()
#define _MD_INIT_FILEDESC(fd)

#define _MD_OPEN_DIR _MD_open_dir
#define _MD_READ_DIR _MD_read_dir
#define _MD_CLOSE_DIR _MD_close_dir
#define _MD_MAKE_NONBLOCK _MD_make_nonblock
#define _MD_SET_FD_INHERITABLE _MD_set_fd_inheritable
#define _MD_INIT_FD_INHERITABLE _MD_init_fd_inheritable
#define _MD_QUERY_FD_INHERITABLE _MD_query_fd_inheritable
#define _MD_OPEN _MD_open
#define _MD_OPEN_FILE _MD_open
#define _MD_CLOSE_FILE _MD_close_file
#define _MD_READ _MD_read
#define _MD_WRITE _MD_write
#define _MD_WRITEV _MD_writev
#define _MD_LSEEK _MD_lseek
#define _MD_LSEEK64 _MD_lseek64
#define _MD_FSYNC _MD_fsync
#define _MD_DELETE _MD_delete
#define _MD_GETFILEINFO _MD_getfileinfo
#define _MD_GETFILEINFO64 _MD_getfileinfo64
#define _MD_GETOPENFILEINFO _MD_getopenfileinfo
#define _MD_GETOPENFILEINFO64 _MD_getopenfileinfo64
#define _MD_RENAME _MD_rename
#define _MD_ACCESS _MD_access
#define _MD_STAT stat
#define _MD_MKDIR _MD_mkdir
#define _MD_MAKE_DIR _MD_mkdir
#define _MD_RMDIR _MD_rmdir
#define _MD_PR_POLL _MD_pr_poll

/* Network I/O */

#define _MD_CLOSE_SOCKET _MD_close_socket
#define _MD_CONNECT _MD_connect
#define _MD_ACCEPT _MD_accept
#define _MD_BIND _MD_bind
#define _MD_LISTEN _MD_listen
#define _MD_SHUTDOWN _MD_shutdown
#define _MD_RECV _MD_recv
#define _MD_SEND _MD_send
#define _MD_ACCEPT_READ _MD_accept_read
#define _MD_GETSOCKNAME _MD_getsockname
#define _MD_GETPEERNAME _MD_getpeername
#define _MD_GETSOCKOPT _MD_getsockopt
#define _MD_SETSOCKOPT _MD_setsockopt
#define _MD_RECVFROM _MD_recvfrom
#define _MD_SENDTO _MD_sendto
#define _MD_SOCKETPAIR _MD_socketpair
#define _MD_SOCKET _MD_socket
#define _MD_SOCKETAVAILABLE _MD_socketavailable
#define _MD_PIPEAVAILABLE _MD_socketavailable

#define _MD_GET_SOCKET_ERROR()	(errno)
#define _MD_GETHOSTNAME _MD_gethostname

#define _MD_SELECT select

/* Process management */

#define _MD_CREATE_PROCESS _MD_create_process
#define _MD_DETACH_PROCESS _MD_detach_process
#define _MD_WAIT_PROCESS _MD_wait_process
#define _MD_KILL_PROCESS _MD_kill_process

/* Atomic data operations */

// #define _MD_INIT_ATOMIC _MD_init_atomic
// #define _MD_ATOMIC_INCREMENT _MD_atomic_increment
// #define _MD_ATOMIC_DECREMENT _MD_atomic_decrement
// #define _MD_ATOMIC_SET _MD_atomic_set

/* memory management */

#define _MD_INIT_SEGS _MD_init_segs
#define _MD_ALLOC_SEGMENT _MD_alloc_segment
#define _MD_FREE_SEGMENT _MD_free_segment

/* Memory mapped file I/O */

#define _MD_CREATE_FILE_MAP _MD_create_file_map
#define _MD_GET_MEM_MAP_ALIGNMENT _MD_get_mem_map_alignment
#define _MD_MEM_MAP _MD_mem_map
#define _MD_MEM_UNMAP _MD_mem_unmap
#define _MD_CLOSE_FILE_MAP _MD_close_file_map

/* Time related */

#define _MD_NOW _MD_now
#define _MD_INTERVAL_INIT _MD_interval_init
#define _MD_GET_INTERVAL _MD_get_interval
#define _MD_INTERVAL_PER_SEC _MD_interval_per_sec

/* File locking */

#define _MD_LOCKFILE _MD_lockfile
#define _MD_TLOCKFILE _MD_tlockfile
#define _MD_UNLOCKFILE _MD_unlockfile

/**
 * Prototypes for machine dependent function implementations. (Too bad
 * NSPR's MD system blows so much that we have to reiterate every stinking
 * thing we implement here in our MD header file.)
 */

/* Miscellaneous */

NSPR_API(void) _MD_cleanup_before_exit(void);
NSPR_API(void) _MD_exit(PRIntn status);

NSPR_API(char*) _MD_get_env(const char *name);
NSPR_API(PRIntn) _MD_put_env(const char *name);

NSPR_API(void) _MD_early_init(void);
NSPR_API(void) _MD_final_init(void);

/* CPU Stuff */

NSPR_API(void) _MD_init_cpus();
NSPR_API(void) _MD_wakeup_cpus();
NSPR_API(void) _MD_start_interrupts(void);
NSPR_API(void) _MD_stop_interrupts(void);
NSPR_API(void) _MD_disable_clock_interrupts(void);
NSPR_API(void) _MD_block_clock_interrupts(void);
NSPR_API(void) _MD_unblock_clock_interrupts(void);
NSPR_API(void) _MD_clock_interrupt(void);
// NSPR_API(void) _MD_init_stack(PRThreadStack *ts, PRIntn redzone);
// NSPR_API(void) _MD_clear_stack(PRThreadStack* ts);
// NSPR_API(PRInt32) _MD_get_intsoff(void);
// NSPR_API(void) _MD_set_intsoff(PRInt32 _val);
// NSPR_API(_PRCPU*) _MD_current_cpu(void);
// NSPR_API(void) _MD_set_current_cpu(_PRCPU *cpu);
// NSPR_API(void) _MD_init_running_cpu(_PRCPU *cpu);
NSPR_API(PRInt32) _MD_pause_cpu(PRIntervalTime timeout);

/* Thread stuff */

// NSPR_API(PRThread*) _MD_current_thread(void);
NSPR_API(PRThread*) _MD_get_attached_thread(void);
NSPR_API(PRThread*) _MD_last_thread(void);
NSPR_API(void) _MD_set_current_thread(PRThread *thread);
NSPR_API(void) _MD_set_last_thread(PRThread *thread);
NSPR_API(PRStatus) _MD_init_thread(PRThread *thread);
NSPR_API(void) _MD_exit_thread(PRThread *thread);
NSPR_API(PRStatus) _MD_init_attached_thread(PRThread *thread);

NSPR_API(void) _MD_suspend_thread(PRThread *thread);
NSPR_API(void) _MD_resume_thread(PRThread *thread);
// NSPR_API(void) _MD_suspend_cpu(_PRCPU  *cpu);
// NSPR_API(void) _MD_resume_cpu(_PRCPU  *cpu);
NSPR_API(void) _MD_begin_suspend_all(void);
NSPR_API(void) _MD_end_suspend_all(void);
NSPR_API(void) _MD_begin_resume_all(void);
NSPR_API(void) _MD_end_resume_all(void);

NSPR_API(void *) _MD_get_sp(PRThread *thread);

NSPR_API(void) _MD_clean_thread(PRThread *thread);
NSPR_API(void) _MD_create_primordial_user_thread(PRThread *);
NSPR_API(PRThread*) _MD_create_user_thread(PRUint32 stacksize, void (*start)(void *), void *arg);
NSPR_API(void) _MD_init_primordial_thread(PRThread *thread);
NSPR_API(PRStatus) _MD_create_thread(PRThread *thread, void (*start)(void *), PRThreadPriority priority, PRThreadScope scope, PRThreadState state, PRUint32 stackSize);
NSPR_API(void) _MD_yield(void);
NSPR_API(void) _MD_set_priority(struct _MDThread *md, PRThreadPriority newPri);

NSPR_API(void) _MD_suspendall(void);
NSPR_API(void) _MD_resumeall(void);

NSPR_API(void) _MD_init_context(PRThread *thread, char *top, void (*start) (void), PRBool *status);
NSPR_API(void) _MD_switch_context(PRThread *thread);
NSPR_API(void) _MD_restore_context(PRThread *thread);

NSPR_API(PRStatus) _MD_wait(PRThread *, PRIntervalTime timeout);
NSPR_API(PRStatus) _MD_wakeup_waiter(PRThread *);

NSPR_API(PRInt32) _MD_setthreadaffinitymask(PRThread *thread, PRUint32 mask );
NSPR_API(PRInt32) _MD_getthreadaffinitymask(PRThread *thread, PRUint32 *mask);

/* Thread Synchronization */

NSPR_API(void) _MD_init_locks(void);
NSPR_API(PRStatus) _MD_new_lock(struct _MDLock *md);
NSPR_API(void) _MD_free_lock(struct _MDLock *md);
NSPR_API(void) _MD_lock(struct _MDLock *md);
NSPR_API(PRIntn) _MD_test_and_lock(struct _MDLock *md);
NSPR_API(void) _MD_unlock(struct _MDLock *md);
NSPR_API(void) _MD_ioq_lock(void);
NSPR_API(void) _MD_ioq_unlock(void);
NSPR_API(void) _MD_new_sem(struct _MDSemaphore *md, PRUintn value);
NSPR_API(void) _MD_destroy_sem(struct _MDSemaphore *md);
NSPR_API(PRStatus) _MD_timed_wait_sem(struct _MDSemaphore *md, PRIntervalTime timeout);
NSPR_API(PRStatus) _MD_wait_sem(struct _MDSemaphore *md);
NSPR_API(void) _MD_post_sem(struct _MDSemaphore *md);
// NSPR_API(PRInt32) _MD_new_cv(struct _MDCVar *md);
// NSPR_API(void) _MD_free_cv(struct _MDCVar *md);
// NSPR_API(void) _MD_wait_cv(struct _MDCVar *mdCVar, struct _MDLock *mdLock, PRIntervalTime timeout);
// NSPR_API(void) _MD_notify_cv(struct _MDCVar *md, struct _MDLock *lock);
// NSPR_API(void) _MD_notifyall_cv(struct _MDCVar *md, struct _MDLock *lock);

/* File I/O */

// NSPR_API(void) _MD_init_io(void);
NSPR_API(PRStatus) _MD_open_dir(struct _MDDir *md,const char *name);
NSPR_API(char *) _MD_read_dir(struct _MDDir *md, PRIntn flags);
NSPR_API(PRInt32) _MD_close_dir(struct _MDDir *md);
NSPR_API(void) _MD_make_nonblock(PRFileDesc *fd);
NSPR_API(void) _MD_init_fd_inheritable(PRFileDesc *fd, PRBool imported);
NSPR_API(void) _MD_query_fd_inheritable(PRFileDesc *fd);
NSPR_API(PRInt32) _MD_open(const char *name, PRIntn osflags, PRIntn mode);
NSPR_API(PRInt32) _MD_close_file(PRInt32 osfd);
NSPR_API(PRInt32) _MD_read(PRFileDesc *fd, void *buf, PRInt32 amount);
NSPR_API(PRInt32) _MD_write(PRFileDesc *fd, const void *buf, PRInt32 amount);
NSPR_API(PRInt32) _MD_writev(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_lseek(PRFileDesc *fd, PRInt32 offset, int whence);
NSPR_API(PRInt64) _MD_lseek64(PRFileDesc *fd, PRInt64 offset, int whence);
NSPR_API(PRInt32) _MD_fsync(PRFileDesc *fd);
NSPR_API(PRInt32) _MD_delete(const char *name);
NSPR_API(PRInt32) _MD_getfileinfo(const char *fn, PRFileInfo *info);
NSPR_API(PRInt32) _MD_getfileinfo64(const char *fn, PRFileInfo64 *info);
NSPR_API(PRInt32) _MD_getopenfileinfo(const PRFileDesc *fd, PRFileInfo *info);
NSPR_API(PRInt32) _MD_getopenfileinfo64(const PRFileDesc *fd, PRFileInfo64 *info);
NSPR_API(PRInt32) _MD_rename(const char *from, const char *to);
NSPR_API(PRInt32) _MD_access(const char *name, PRIntn how);
NSPR_API(PRInt32) _MD_stat(const char *name, struct stat *buf);
NSPR_API(PRInt32) _MD_mkdir(const char *name, PRIntn mode);
NSPR_API(PRInt32) _MD_rmdir(const char *name);
NSPR_API(PRInt32) _MD_pr_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout);

/* Network I/O */
NSPR_API(PRInt32) _MD_close_socket(PRInt32 osfd);
NSPR_API(PRInt32) _MD_connect(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_bind(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen);
NSPR_API(PRInt32) _MD_listen(PRFileDesc *fd, PRIntn backlog);
NSPR_API(PRInt32) _MD_shutdown(PRFileDesc *fd, PRIntn how);
NSPR_API(PRInt32) _MD_recv(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_send(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_accept_read(PRFileDesc *sd, PRInt32 *newSock, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout);
// NSPR_API(PRInt32) _MD_fast_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout, PRBool fast, _PR_AcceptTimeoutCallback callback, void *callbackArg);
// NSPR_API(PRInt32) _MD_fast_accept_read(PRFileDesc *sd, PRInt32 *newSock, PRNetAddr **raddr, void *buf, PRInt32 amount, PRIntervalTime timeout, PRBool fast, _PR_AcceptTimeoutCallback callback, void *callbackArg);
// NSPR_API(void) _MD_update_accept_context(PRInt32 s, PRInt32 ls);
NSPR_API(PRStatus) _MD_getsockname(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen);
NSPR_API(PRStatus) _MD_getpeername(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen);
NSPR_API(PRStatus) _MD_getsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen);
NSPR_API(PRStatus) _MD_setsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen);
NSPR_API(PRInt32) _MD_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_sendto(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout);
NSPR_API(PRInt32) _MD_socketpair(int af, int type, int flags, PRInt32 *osfd);
NSPR_API(PRInt32) _MD_socket(int af, int type, int flags);
NSPR_API(PRInt32) _MD_socketavailable(PRFileDesc *fd);

// NSPR_API(PRInt32) _MD_get_socket_error(void);
NSPR_API(PRStatus) _MD_gethostname(char *name, PRUint32 namelen);

/* Process management */

NSPR_API(PRProcess *) _MD_create_process(const char *path, char *const *argv, char *const *envp, const PRProcessAttr *attr);
NSPR_API(PRStatus) _MD_detach_process(PRProcess *process);
NSPR_API(PRStatus) _MD_wait_process(PRProcess *process, PRInt32 *exitCode);
NSPR_API(PRStatus) _MD_kill_process(PRProcess *process);

/* Atomic data operations */

// NSPR_API(void) _MD_init_atomic(void);
// NSPR_API(PRInt32) _MD_atomic_increment(PRInt32 *);
// NSPR_API(PRInt32) _MD_atomic_decrement(PRInt32 *);
// NSPR_API(PRInt32) _MD_atomic_set(PRInt32 *, PRInt32);

/* Memory management */

NSPR_API(void) _MD_init_segs(void);
NSPR_API(PRStatus) _MD_alloc_segment(PRSegment *seg, PRUint32 size, void *vaddr);
NSPR_API(void) _MD_free_segment(PRSegment *seg);

/* Memory mapped file I/O */

NSPR_API(PRStatus) _MD_create_file_map(PRFileMap *fmap, PRInt64 size);
NSPR_API(PRInt32) _MD_get_mem_map_alignment(void);
NSPR_API(void *) _MD_mem_map(PRFileMap *fmap, PRInt64 offset, PRUint32 len);
NSPR_API(PRStatus) _MD_mem_unmap(void *addr, PRUint32 size);
NSPR_API(PRStatus) _MD_close_file_map(PRFileMap *fmap);

/* Time related */

NSPR_API(PRTime) _MD_now(void);
NSPR_API(void) _MD_interval_init(void);
NSPR_API(PRIntervalTime) _MD_get_interval(void);
NSPR_API(PRIntervalTime) _MD_interval_per_sec(void);

/* File locking */

NSPR_API(PRStatus) _MD_lockfile(PRInt32 osfd);
NSPR_API(PRStatus) _MD_tlockfile(PRInt32 osfd);
NSPR_API(PRStatus) _MD_unlockfile(PRInt32 osfd);

#endif /* _nspr_beos_defs_h___*/
