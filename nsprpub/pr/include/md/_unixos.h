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

#ifndef prunixos_h___
#define prunixos_h___

/*
 * If FD_SETSIZE is not defined on the command line, set the default value
 * before include select.h
 */
/*
 * Linux: FD_SETSIZE is defined in /usr/include/sys/select.h and should
 * not be redefined.
 */
#if !defined(LINUX) && !defined(RHAPSODY)
#ifndef FD_SETSIZE
#define FD_SETSIZE  4096
#endif
#endif

#include <unistd.h>
#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>

#include "prio.h"
#include "prmem.h"
#include "prclist.h"

/* To pick up fd_set */
#if defined(HPUX)
#include <sys/time.h>
#elif defined(OSF1) || defined(AIX) || defined(SOLARIS) || defined(IRIX) \
        || defined(UNIXWARE) || defined(NCR) || defined(SNI) || defined(NEC) \
        || defined(BSDI) || defined(SONY)
#include <sys/select.h>
#elif defined(SUNOS4) || defined(SCO) || defined(FREEBSD) || defined(RHAPSODY)
#include <sys/types.h>
#elif defined(LINUX)
#include <sys/time.h>
#include <sys/types.h>
#else
#error Find out what include file defines fd_set on this platform
#endif

#define PR_DIRECTORY_SEPARATOR		'/'
#define PR_DIRECTORY_SEPARATOR_STR	"/"
#define PR_PATH_SEPARATOR		':'
#define PR_PATH_SEPARATOR_STR		":"
#define GCPTR

typedef int (*FARPROC)();

/*
 * intervals at which GLOBAL threads wakeup to check for pending interrupt
 */
#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5
extern PRIntervalTime intr_timeout_ticks;

typedef struct _PRUnixPollDesc {
	PRInt32 osfd;
	PRInt16 in_flags;
	PRInt16 out_flags;
} _PRUnixPollDesc;

typedef struct PRPollQueue {
    PRCList links;        /* for linking PRPollQueue's together */
    _PRUnixPollDesc *pds;        /* array of poll descriptors */
    PRUintn npds;            /* length of the array */
    PRPackedBool on_ioq;    /* is this on the async i/o work q? */
    PRIntervalTime timeout;        /* timeout, in ticks */
    struct PRThread *thr;
} PRPollQueue;

#define _PR_POLLQUEUE_PTR(_qp) \
    ((PRPollQueue*) ((char*) (_qp) - offsetof(PRPollQueue,links)))


extern PRInt32 _PR_WaitForFD(PRInt32 osfd, PRUintn how,
					PRIntervalTime timeout);
extern void _PR_Unblock_IO_Wait(struct PRThread *thr);

#if defined(_PR_LOCAL_THREADS_ONLY) || defined(_PR_GLOBAL_THREADS_ONLY)
#define _MD_CHECK_FOR_EXIT()
#endif

extern fd_set _pr_md_read_set, _pr_md_write_set, _pr_md_exception_set;
extern PRInt16 _pr_md_read_cnt[], _pr_md_write_cnt[], _pr_md_exception_cnt[];
extern PRInt32 _pr_md_ioq_max_osfd;
extern PRUint32 _pr_md_ioq_timeout;

struct _MDFileDesc {
    int osfd;
};

struct _MDDir {
	DIR *d;
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
struct _PRCPU;
extern void _MD_unix_init_running_cpu(struct _PRCPU *cpu);

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


/*
** Make a redzone at both ends of the stack segment. Disallow access
** to those pages of memory. It's ok if the mprotect call's don't
** work - it just means that we don't really have a functional
** redzone.
*/
#if defined(DEBUG) && !defined(RHAPSODY)
#if !defined(SOLARIS)	
#include <string.h>  /* for memset() */
#define _MD_INIT_STACK(ts,REDZONE)					\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_NONE);	\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_NONE);				\
    /*									\
    ** Fill stack memory with something that turns into an illegal	\
    ** pointer value. This will sometimes find runtime references to	\
    ** uninitialized pointers. We don't do this for solaris because we	\
    ** can use purify instead.						\
    */									\
    if (_pr_debugStacks) {						\
	memset(ts->allocBase + REDZONE, 0xf7, ts->stackSize);		\
    }									\
    PR_END_MACRO
#else	/* !SOLARIS	*/
#define _MD_INIT_STACK(ts,REDZONE)					\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_NONE);	\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_NONE);				\
    PR_END_MACRO
#endif	/* !SOLARIS	*/

/*
 * _MD_CLEAR_STACK
 *	Allow access to the redzone pages; the access was turned off in
 *	_MD_INIT_STACK.
 */
#define _MD_CLEAR_STACK(ts)						\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_READ|PROT_WRITE);\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_READ|PROT_WRITE);			\
    PR_END_MACRO

#else	/* DEBUG */

#define _MD_INIT_STACK(ts,REDZONE)
#define _MD_CLEAR_STACK(ts)

#endif	/* DEBUG */

#if !defined(SOLARIS) 

#define PR_SET_INTSOFF(newval)

#endif

/************************************************************************/

extern void _PR_UnixInit(void);

/************************************************************************/

struct _MDProcess {
    pid_t pid;
};

struct PRProcess;
struct PRProcessAttr;

/* Create a new process (fork() + exec()) */
#define _MD_CREATE_PROCESS _MD_CreateUnixProcess
extern struct PRProcess * _MD_CreateUnixProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const struct PRProcessAttr *attr
);

#define _MD_DETACH_PROCESS _MD_DetachUnixProcess
extern PRStatus _MD_DetachUnixProcess(struct PRProcess *process);

/* Wait for a child process to terminate */
#define _MD_WAIT_PROCESS _MD_WaitUnixProcess
extern PRStatus _MD_WaitUnixProcess(struct PRProcess *process,
    PRInt32 *exitCode);

#define _MD_KILL_PROCESS _MD_KillUnixProcess
extern PRStatus _MD_KillUnixProcess(struct PRProcess *process);

/************************************************************************/

#define _MD_START_INTERRUPTS			_MD_StartInterrupts
#define _MD_STOP_INTERRUPTS				_MD_StopInterrupts
#define _MD_DISABLE_CLOCK_INTERRUPTS	_MD_DisableClockInterrupts
#define _MD_BLOCK_CLOCK_INTERRUPTS		_MD_BlockClockInterrupts
#define _MD_UNBLOCK_CLOCK_INTERRUPTS	_MD_UnblockClockInterrupts

/************************************************************************/

extern void		_MD_InitCPUS(void);
#define _MD_INIT_CPUS           _MD_InitCPUS

extern void		_MD_Wakeup_CPUs(void);
#define _MD_WAKEUP_CPUS _MD_Wakeup_CPUs

#define _MD_PAUSE_CPU			_MD_PauseCPU

#if defined(_PR_LOCAL_THREADS_ONLY) || defined(_PR_GLOBAL_THREADS_ONLY)
#define _MD_CLEANUP_BEFORE_EXIT()
#endif

#ifndef IRIX
#define _MD_EXIT(status)		_exit(status)
#endif

/************************************************************************/

#define _MD_GET_ENV				getenv
#define _MD_PUT_ENV				putenv

/************************************************************************/

extern void		_MD_MakeNonblock(PRFileDesc *fd);
#define _MD_MAKE_NONBLOCK			_MD_MakeNonblock		

/************************************************************************/

extern PRStatus	_MD_AllocSegment(PRSegment *seg, PRUint32 size,
				void *vaddr);
extern void		_MD_FreeSegment(PRSegment *seg);

#define _MD_INIT_SEGS			_MD_InitSegs
#define _MD_ALLOC_SEGMENT		_MD_AllocSegment
#define _MD_FREE_SEGMENT		_MD_FreeSegment

/************************************************************************/

#define _MD_INTERVAL_INIT()
#define _MD_INTERVAL_PER_MILLISEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000000)

/************************************************************************/

#define _MD_ERRNO()             	(errno)
#define _MD_GET_SOCKET_ERROR()		(errno)

/************************************************************************/

extern PRInt32 _MD_AvailableSocket(PRInt32 osfd);

#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

extern void _MD_InitSegs(void);
extern void _MD_StartInterrupts(void);
extern void _MD_StopInterrupts(void);
extern void _MD_DisableClockInterrupts(void);
extern void _MD_BlockClockInterrupts(void);
extern void _MD_UnblockClockInterrupts(void);
extern void _MD_PauseCPU(PRIntervalTime timeout);

extern PRStatus _MD_open_dir(struct _MDDir *, const char *);
extern PRInt32  _MD_close_dir(struct _MDDir *);
extern char *   _MD_read_dir(struct _MDDir *, PRIntn);
extern PRInt32  _MD_open(const char *name, PRIntn osflags, PRIntn mode);
extern PRInt32	_MD_delete(const char *name);
extern PRInt32	_MD_getfileinfo(const char *fn, PRFileInfo *info);
extern PRInt32  _MD_getfileinfo64(const char *fn, PRFileInfo64 *info);
extern PRInt32  _MD_getopenfileinfo(const PRFileDesc *fd, PRFileInfo *info);
extern PRInt32  _MD_getopenfileinfo64(const PRFileDesc *fd, PRFileInfo64 *info);
extern PRInt32	_MD_rename(const char *from, const char *to);
extern PRInt32	_MD_access(const char *name, PRIntn how);
extern PRInt32	_MD_mkdir(const char *name, PRIntn mode);
extern PRInt32	_MD_rmdir(const char *name);
extern PRInt32	_MD_accept_read(PRInt32 sock, PRInt32 *newSock,
				PRNetAddr **raddr, void *buf, PRInt32 amount);
extern PRInt32 	_PR_UnixTransmitFile(PRFileDesc *sd, PRFileDesc *fd,
			const void *headers, PRInt32 hlen,
			PRTransmitFileFlags flags, PRIntervalTime timeout);

extern PRStatus _MD_LockFile(PRInt32 osfd);
extern PRStatus _MD_TLockFile(PRInt32 osfd);
extern PRStatus _MD_UnlockFile(PRInt32 osfd);

#define _MD_OPEN_DIR(dir, name)		    _MD_open_dir(dir, name)
#define _MD_CLOSE_DIR(dir)		        _MD_close_dir(dir)
#define _MD_READ_DIR(dir, flags)	    _MD_read_dir(dir, flags)
#define _MD_OPEN(name, osflags, mode    )	_MD_open(name, osflags, mode)
extern PRInt32 _MD_read(PRFileDesc *fd, void *buf, PRInt32 amount);
#define _MD_READ(fd,buf,amount)		    _MD_read(fd,buf,amount)
extern PRInt32 _MD_write(PRFileDesc *fd, const void *buf, PRInt32 amount);
#define _MD_WRITE(fd,buf,amount)	    _MD_write(fd,buf,amount)
#define _MD_DELETE(name)		        _MD_delete(name)
#define _MD_GETFILEINFO(fn, info)	    _MD_getfileinfo(fn, info)
#define _MD_GETFILEINFO64(fn, info)	    _MD_getfileinfo64(fn, info)
#define _MD_GETOPENFILEINFO(fd, info)	_MD_getopenfileinfo(fd, info)
#define _MD_GETOPENFILEINFO64(fd, info)	_MD_getopenfileinfo64(fd, info)
#define _MD_RENAME(from, to)		    _MD_rename(from, to)
#define _MD_ACCESS(name, how)		    _MD_access(name, how)
#define _MD_MKDIR(name, mode)		    _MD_mkdir(name, mode)
#define _MD_RMDIR(name)			        _MD_rmdir(name)
#define _MD_ACCEPT_READ(sock, newSock, raddr, buf, amount)	_MD_accept_read(sock, newSock, raddr, buf, amount)

#define _MD_LOCKFILE _MD_LockFile
#define _MD_TLOCKFILE _MD_TLockFile
#define _MD_UNLOCKFILE _MD_UnlockFile


extern PRInt32		_MD_socket(int af, int type, int flags);
#define _MD_SOCKET	_MD_socket
extern PRInt32		_MD_connect(PRFileDesc *fd, const PRNetAddr *addr,
								PRUint32 addrlen, PRIntervalTime timeout);
#define _MD_CONNECT	_MD_connect
extern PRInt32		_MD_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen,
													PRIntervalTime timeout);
#define _MD_ACCEPT	_MD_accept
extern PRInt32		_MD_bind(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen);
#define _MD_BIND	_MD_bind
extern PRInt32		_MD_listen(PRFileDesc *fd, PRIntn backlog);
#define _MD_LISTEN	_MD_listen
extern PRInt32		_MD_shutdown(PRFileDesc *fd, PRIntn how);
#define _MD_SHUTDOWN	_MD_shutdown

extern PRInt32		_MD_recv(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout);
#define _MD_RECV	_MD_recv
extern PRInt32		_MD_send(PRFileDesc *fd, const void *buf, PRInt32 amount,
									PRIntn flags, PRIntervalTime timeout);
#define _MD_SEND	_MD_send
extern PRInt32		_MD_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount,
						PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen,
											PRIntervalTime timeout);
#define _MD_RECVFROM	_MD_recvfrom
extern PRInt32 _MD_sendto(PRFileDesc *fd, const void *buf, PRInt32 amount,
							PRIntn flags, const PRNetAddr *addr, PRUint32 addrlen,
												PRIntervalTime timeout);
#define _MD_SENDTO	_MD_sendto
extern PRInt32		_MD_writev(PRFileDesc *fd, struct PRIOVec *iov,
								PRInt32 iov_size, PRIntervalTime timeout);
#define _MD_WRITEV	_MD_writev

extern PRInt32		_MD_socketavailable(PRFileDesc *fd);
#define	_MD_SOCKETAVAILABLE		_MD_socketavailable
extern PRInt64		_MD_socketavailable64(PRFileDesc *fd);
#define	_MD_SOCKETAVAILABLE64		_MD_socketavailable64

extern PRInt32 _MD_pr_poll(PRPollDesc *pds, PRIntn npds,
												PRIntervalTime timeout);
#define _MD_PR_POLL		_MD_pr_poll

extern PRInt32		_MD_close(PRInt32 osfd);
#define _MD_CLOSE_FILE	_MD_close
extern PRInt32		_MD_lseek(PRFileDesc*, PRInt32, PRSeekWhence);
#define _MD_LSEEK	_MD_lseek
extern PRInt64		_MD_lseek64(PRFileDesc*, PRInt64, PRSeekWhence);
#define _MD_LSEEK64	_MD_lseek64
extern PRInt32		_MD_fsync(PRFileDesc *fd);
#define _MD_FSYNC	_MD_fsync

extern PRInt32 _MD_socketpair(int af, int type, int flags, PRInt32 *osfd);
#define _MD_SOCKETPAIR		_MD_socketpair

#define _MD_CLOSE_SOCKET	_MD_close

#ifndef NO_NSPR_10_SUPPORT
#define _MD_STAT	stat
#endif

extern PRStatus _MD_getpeername(PRFileDesc *fd, PRNetAddr *addr,
											PRUint32 *addrlen);
#define _MD_GETPEERNAME _MD_getpeername
extern PRStatus _MD_getsockname(PRFileDesc *fd, PRNetAddr *addr,
											PRUint32 *addrlen);
#define _MD_GETSOCKNAME _MD_getsockname

extern PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level,
						PRInt32 optname, char* optval, PRInt32* optlen);
#define _MD_GETSOCKOPT		_MD_getsockopt
extern PRStatus _MD_setsockopt(PRFileDesc *fd, PRInt32 level,
					PRInt32 optname, const char* optval, PRInt32 optlen);
#define _MD_SETSOCKOPT		_MD_setsockopt

extern PRStatus _MD_gethostname(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME		_MD_gethostname

extern int _MD_unix_get_nonblocking_connect_error(int osfd);

#define HAVE_SOCKET_REUSEADDR
#define HAVE_SOCKET_KEEPALIVE

/* Memory-mapped files */

struct _MDFileMap {
    PRIntn prot;
    PRIntn flags;
};

extern PRStatus _MD_CreateFileMap(struct PRFileMap *fmap, PRInt64 size);
#define _MD_CREATE_FILE_MAP _MD_CreateFileMap

extern void * _MD_MemMap(struct PRFileMap *fmap, PRInt64 offset,
        PRUint32 len);
#define _MD_MEM_MAP _MD_MemMap

extern PRStatus _MD_MemUnmap(void *addr, PRUint32 size);
#define _MD_MEM_UNMAP _MD_MemUnmap

extern PRStatus _MD_CloseFileMap(struct PRFileMap *fmap);
#define _MD_CLOSE_FILE_MAP _MD_CloseFileMap

/*
 * The standard (XPG4) gettimeofday() (from BSD) takes two arguments.
 * On some SVR4 derivatives, gettimeofday() takes only one argument.
 * The GETTIMEOFDAY macro is intended to hide this difference.
 */
#ifdef HAVE_SVID_GETTOD
#define GETTIMEOFDAY(tp) gettimeofday(tp)
#else
#define GETTIMEOFDAY(tp) gettimeofday((tp), NULL)
#endif

#if defined(LINUX) && defined(_PR_PTHREADS) && !(__GLIBC__ >= 2)
#define _PR_NEED_FAKE_POLL
#endif

#if defined(_PR_NEED_FAKE_POLL)

/*
 * Some platforms don't have poll(), but our pthreads code calls poll().
 * As a temporary measure, I implemented a fake poll() using select().
 * Here are the struct and macro definitions copied from sys/poll.h
 * on Solaris 2.5.
 */

struct pollfd {
    int fd;
    short events;
    short revents;
};

/* poll events */

#define	POLLIN		0x0001		/* fd is readable */
#define	POLLPRI		0x0002		/* high priority info at fd */
#define	POLLOUT		0x0004		/* fd is writeable (won't block) */
#define	POLLRDNORM	0x0040		/* normal data is readable */
#define	POLLWRNORM	POLLOUT
#define	POLLRDBAND	0x0080		/* out-of-band data is readable */
#define	POLLWRBAND	0x0100		/* out-of-band data is writeable */

#define	POLLNORM	POLLRDNORM

#define	POLLERR		0x0008		/* fd has error condition */
#define	POLLHUP		0x0010		/* fd has been hung up on */
#define	POLLNVAL	0x0020		/* invalid pollfd entry */

extern int poll(struct pollfd *, unsigned long, int);

#endif /* _PR_NEED_FAKE_POLL */

#endif /* prunixos_h___ */
