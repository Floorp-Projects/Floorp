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

#ifndef nspr_win16_defs_h___
#define nspr_win16_defs_h___

#include <windows.h>
#include <winsock.h>
#include <errno.h>
#include <direct.h>

#include "nspr.h"
/* $$ fix this */
#define Remind(x)

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH      "win16"
#define _PR_SI_SYSNAME        "WIN16"
#define _PR_SI_ARCHITECTURE   "x86"    /* XXXMB hardcode for now */

#define HAVE_DLL
#define _PR_NO_PREEMPT
#define _PR_LOCAL_THREADS_ONLY
#undef  _PR_GLOBAL_THREADS_ONLY
#undef  HAVE_THREAD_AFFINITY
#define HAVE_SOCKET_REUSEADDR
#define HAVE_SOCKET_KEEPALIVE
#define _PR_HAVE_ATOMIC_OPS

/* --- Common User-Thread/Native-Thread Definitions --------------------- */

extern struct PRLock        *_pr_schedLock;
extern char                 * _pr_top_of_task_stack;


/* --- Typedefs --- */

#define PR_NUM_GCREGS           9
typedef PRInt32	               PR_CONTEXT_TYPE[PR_NUM_GCREGS];

#define _MD_MAGIC_THREAD	0x22222222
#define _MD_MAGIC_THREADSTACK	0x33333333
#define _MD_MAGIC_SEGMENT	0x44444444
#define _MD_MAGIC_DIR		0x55555555
#define _MD_MAGIC_CV        0x66666666


typedef struct _PRWin16PollDesc
{
	PRInt32 osfd;
	PRInt16 in_flags;
	PRInt16 out_flags;
} _PRWin16PollDesc;

typedef struct PRPollQueue
{
    PRCList links;              /* for linking PRPollQueue's together */
    _PRWin16PollDesc *pds;      /* array of poll descriptors */
    PRUintn npds;				/* length of the array */
    PRPackedBool on_ioq;        /* is this on the async i/o work q? */
    PRIntervalTime timeout;     /* timeout, in ticks */
    struct PRThread *thr;
} PRPollQueue;

#define _PR_POLLQUEUE_PTR(_qp) \
    ((PRPollQueue *) ((char*) (_qp) - offsetof(PRPollQueue,links)))

PR_EXTERN(PRInt32) _PR_WaitForFD(PRInt32 osfd, PRUintn how,
					PRIntervalTime timeout);
PR_EXTERN(void) _PR_Unblock_IO_Wait(struct PRThread *thr);

#define _PR_MD_MAX_OSFD             FD_SETSIZE
#define _PR_IOQ(_cpu)				((_cpu)->md.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) 	PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)		((_cpu)->md.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)		((_cpu)->md.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)		((_cpu)->md.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)		((_cpu)->md.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)	((_cpu)->md.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)	((_cpu)->md.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)		((_cpu)->md.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)		((_cpu)->md.ioq_max_osfd)

struct _MDCPU {
    PRCList		ioQ;
    fd_set		fd_read_set, fd_write_set, fd_exception_set;
    PRInt16		fd_read_cnt[_PR_MD_MAX_OSFD],fd_write_cnt[_PR_MD_MAX_OSFD],
				fd_exception_cnt[_PR_MD_MAX_OSFD];
    PRUint32	ioq_timeout;
    PRInt32		ioq_max_osfd;
};

struct _MDThread {
    /* The overlapped structure must be first! */
    HANDLE           blocked_sema;      /* Threads block on this when waiting
                                         * for IO or CondVar.
                                         */
    PRInt32         errcode;        /* preserved errno for this thread */
    CATCHBUF        context;        /* thread context for Throw() */
    void           *SP;             /* Stack pointer, used only by GarbColl */
    int             threadNumber;   /* instrumentation: order of creation */
	_PRWin16PollDesc thr_pd;       /* poll descriptor for i/o */
	PRPollQueue		thr_pq;        /* i/o parameters			*/
    void           *exceptionContext; /* mfc exception context */
    char            guardBand[24];  /* don't overwrite this */
    PRUint32        magic;          /* self identifier, for debug */
};

struct _MDThreadStack {
    PRUint32           magic;          /* for debugging */
    PRIntn          cxByteCount;    /* number of stack bytes to move */
    char *          stackTop;       /* high address on stack */
};

struct _MDSegment {
    PRUint32           magic;          /* for debugging */
};


struct _MDLock {
    PRUint32           magic;          /* for debugging */
    PRUint32           mutex;
};

struct _MDDir {
    PRUint32         magic;          /* for debugging */
    struct  dirent  *dir;
};

struct _MDCVar {
	PRUint32        magic;
};

struct _MDSemaphore {
	PRInt32			unused;
};

struct _MDFileDesc {
    PRInt32 osfd;
};

struct _MDProcess {
    HANDLE handle;
    DWORD id;
};

/*
** Microsoft 'struct _stat'
** ... taken directly from msvc 1.52c's header file sys/stat.h
** see PR_Stat() implemented in w16io.c
** See BugSplat: 98516
*/
#pragma pack(push)
#pragma pack(2)

typedef unsigned short _ino_t;
typedef short _dev_t;
typedef long _off_t;

typedef struct _MDMSStat {
    _dev_t st_dev;
    _ino_t st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    _dev_t st_rdev;
    _off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
} _MDMSStat;
#pragma pack(pop)

/* --- Errors --- */
    /* These are NSPR generated error codes which need to be unique from
     * OS error codes.
     */
#define _MD_UNIQUEBASE                 50000
#define _MD_EINTERRUPTED               _MD_UNIQUEBASE + 1
#define _MD_ETIMEDOUT                  _MD_UNIQUEBASE + 2
#define _MD_EIO                        _MD_UNIQUEBASE + 3

struct PRProcess;
struct PRProcessAttr;

/* --- Create a new process --- */
#define _MD_CREATE_PROCESS _PR_CreateWindowsProcess
extern struct PRProcess * _PR_CreateWindowsProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const struct PRProcessAttr *attr
);

#define _MD_DETACH_PROCESS _PR_DetachWindowsProcess
extern PRStatus _PR_DetachWindowsProcess(struct PRProcess *process);

/* --- Wait for a child process to terminate --- */
#define _MD_WAIT_PROCESS _PR_WaitWindowsProcess
extern PRStatus _PR_WaitWindowsProcess(struct PRProcess *process, 
    PRInt32 *exitCode);

#define _MD_KILL_PROCESS _PR_KillWindowsProcess
extern PRStatus _PR_KillWindowsProcess(struct PRProcess *process);


/* --- Misc stuff --- */

#define MD_ASSERTINT( x )             PR_ASSERT( (x) < 65535 )

/* --- IO stuff --- */
#define MAX_PATH    256
#define _MD_ERRNO()                   errno
#define GetLastError()                errno

#define _MD_GET_FILE_ERROR()          errno
#define _MD_SET_FILE_ERROR(_err)      errno = (_err)

#define _MD_OPEN                      _PR_MD_OPEN
#define _MD_READ                      _PR_MD_READ
#define _MD_WRITE                     _PR_MD_WRITE
#define _MD_WRITEV                    _PR_MD_WRITEV
#define _MD_LSEEK                     _PR_MD_LSEEK
#define _MD_LSEEK64                   _PR_MD_LSEEK64
#define _MD_CLOSE_FILE                _PR_MD_CLOSE_FILE
#define _MD_GETFILEINFO               _PR_MD_GETFILEINFO
#define _MD_GETOPENFILEINFO           _PR_MD_GETOPENFILEINFO
#define _MD_STAT                      _PR_MD_STAT
#define _MD_RENAME                    _PR_MD_RENAME     
#define _MD_ACCESS                    _PR_MD_ACCESS     
#define _MD_DELETE                    _PR_MD_DELETE     
#define _MD_MKDIR                     _PR_MD_MKDIR      
#define _MD_RMDIR                     _PR_MD_RMDIR      
#define _MD_LOCKFILE                  _PR_MD_LOCKFILE
#define _MD_TLOCKFILE                 _PR_MD_TLOCKFILE
#define _MD_UNLOCKFILE                _PR_MD_UNLOCKFILE


/* --- Socket IO stuff --- */
#define _MD_EACCES                WSAEACCES
#define _MD_EADDRINUSE            WSAEADDRINUSE
#define _MD_EADDRNOTAVAIL         WSAEADDRNOTAVAIL
#define _MD_EAFNOSUPPORT          WSAEAFNOSUPPORT
#define _MD_EAGAIN                WSAEWOULDBLOCK
#define _MD_EALREADY              WSAEALREADY
#define _MD_EBADF                 WSAEBADF
#define _MD_ECONNREFUSED          WSAECONNREFUSED
#define _MD_ECONNRESET            WSAECONNRESET
#define _MD_EFAULT                WSAEFAULT
#define _MD_EINPROGRESS           WSAEINPROGRESS
#define _MD_EINTR                 WSAEINTR
#define _MD_EINVAL                EINVAL
#define _MD_EISCONN               WSAEISCONN
#define _MD_ENETUNREACH           WSAENETUNREACH
#define _MD_ENOENT                ENOENT
#define _MD_ENOTCONN              WSAENOTCONN
#define _MD_ENOTSOCK              WSAENOTSOCK
#define _MD_EOPNOTSUPP            WSAEOPNOTSUPP
#define _MD_EWOULDBLOCK           WSAEWOULDBLOCK
#define _MD_GET_SOCKET_ERROR()    WSAGetLastError()
#define _MD_SET_SOCKET_ERROR(_err) WSASetLastError(_err)

#define _MD_INIT_FILEDESC(fd)
#define _MD_MAKE_NONBLOCK             _PR_MD_MAKE_NONBLOCK
#define _MD_SHUTDOWN                  _PR_MD_SHUTDOWN
#define _MD_LISTEN                      _PR_MD_LISTEN
#define _MD_CLOSE_SOCKET              _PR_MD_CLOSE_SOCKET
#define _MD_SENDTO                    _PR_MD_SENDTO
#define _MD_RECVFROM                  _PR_MD_RECVFROM
#define _MD_SOCKETPAIR(s, type, proto, sv) -1
#define _MD_GETSOCKNAME               _PR_MD_GETSOCKNAME
#define _MD_GETPEERNAME               _PR_MD_GETPEERNAME
#define _MD_GETSOCKOPT                _PR_MD_GETSOCKOPT
#define _MD_SETSOCKOPT                _PR_MD_SETSOCKOPT
#define _MD_SELECT                    select
#define _MD_FSYNC                     _PR_MD_FSYNC
#define _MD_SOCKETAVAILABLE           _PR_MD_SOCKETAVAILABLE

#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(x)       (*x++)
#define _MD_ATOMIC_ADD(ptr, val)      ((*x) += val)
#define _MD_ATOMIC_DECREMENT(x)       (*x--)
#define _MD_ATOMIC_SET(x,y)           (*x, y)

#define _MD_INIT_IO                   _PR_MD_INIT_IO
#define _MD_TRANSMITFILE              _PR_MD_TRANSMITFILE

/* win95 doesn't have async IO */
#define _MD_SOCKET                    _PR_MD_SOCKET
#define _MD_CONNECT                   _PR_MD_CONNECT
#define _MD_ACCEPT                    _PR_MD_ACCEPT
#define _MD_BIND                      _PR_MD_BIND
#define _MD_RECV                      _PR_MD_RECV
#define _MD_SEND                      _PR_MD_SEND

#define _MD_CHECK_FOR_EXIT()

/* --- Scheduler stuff --- */
#define _MD_PAUSE_CPU                 _PR_MD_PAUSE_CPU

/* --- DIR stuff --- */
#define PR_DIRECTORY_SEPARATOR        '\\'
#define PR_DIRECTORY_SEPARATOR_STR    "\\"
#define PR_PATH_SEPARATOR		';'
#define PR_PATH_SEPARATOR_STR		";"
#define _MD_OPEN_DIR                  _PR_MD_OPEN_DIR
#define _MD_CLOSE_DIR                 _PR_MD_CLOSE_DIR
#define _MD_READ_DIR                  _PR_MD_READ_DIR

/* --- Segment stuff --- */
#define _MD_INIT_SEGS()
#define _MD_ALLOC_SEGMENT           _MD_AllocSegment
#define _MD_FREE_SEGMENT            _MD_FreeSegment

/* --- Environment Stuff --- */
#define _MD_GET_ENV                 _PR_MD_GET_ENV
#define _MD_PUT_ENV                 _PR_MD_PUT_ENV

/* --- Threading Stuff --- */
#define _MD_DEFAULT_STACK_SIZE      32767L
#define _MD_INIT_THREAD             _PR_MD_INIT_THREAD
#define _MD_CREATE_THREAD(t,f,p,sc,st,stsiz) (PR_SUCCESS)
#define _MD_YIELD                   _PR_MD_YIELD
#define _MD_SET_PRIORITY(t,p)            
#define _MD_CLEAN_THREAD(t)
#define _MD_SETTHREADAFFINITYMASK   _PR_MD_SETTHREADAFFINITYMASK
#define _MD_GETTHREADAFFINITYMASK   _PR_MD_GETTHREADAFFINITYMASK
#define _MD_EXIT_THREAD
#define _MD_SUSPEND_THREAD          _PR_MD_SUSPEND_THREAD
#define _MD_RESUME_THREAD           _PR_MD_RESUME_THREAD
#define _MD_SUSPEND_CPU             _PR_MD_SUSPEND_CPU
#define _MD_RESUME_CPU              _PR_MD_RESUME_CPU
#define _MD_BEGIN_SUSPEND_ALL()
#define _MD_BEGIN_RESUME_ALL()
#define _MD_END_SUSPEND_ALL()
#define _MD_END_RESUME_ALL()

/* --- Lock stuff --- */
/*
** Win16 does not need MD locks.
*/
#define _PR_LOCK                    _MD_LOCK
#define _PR_UNLOCK                  _MD_UNLOCK

#define _MD_NEW_LOCK(l)             (PR_SUCCESS)
#define _MD_FREE_LOCK(l)
#define _MD_LOCK(l)
#define _MD_TEST_AND_LOCK(l)        (PR_FAILURE)
#define _MD_UNLOCK(l)

/* --- lock and cv waiting --- */
#define _MD_WAIT                      _PR_MD_WAIT
#define _MD_WAKEUP_WAITER(a)
#define _MD_WAKEUP_CPUS               _PR_MD_WAKEUP_CPUS

/* --- CVar ------------------- */
#define _MD_WAIT_CV					  _PR_MD_WAIT_CV
#define _MD_NEW_CV					  _PR_MD_NEW_CV
#define _MD_FREE_CV					  _PR_MD_FREE_CV
#define _MD_NOTIFY_CV				  _PR_MD_NOTIFY_CV	
#define _MD_NOTIFYALL_CV			  _PR_MD_NOTIFYALL_CV

   /* XXXMB- the IOQ stuff is certainly not working correctly yet. */
#define _MD_IOQ_LOCK()                
#define _MD_IOQ_UNLOCK()              


/* --- Initialization stuff --- */
PR_EXTERN(void) _MD_INIT_RUNNING_CPU(struct _PRCPU *cpu );
#define _MD_START_INTERRUPTS()
#define _MD_STOP_INTERRUPTS()
#define _MD_DISABLE_CLOCK_INTERRUPTS()
#define _MD_ENABLE_CLOCK_INTERRUPTS()
#define _MD_BLOCK_CLOCK_INTERRUPTS()
#define _MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _MD_EARLY_INIT                  _PR_MD_EARLY_INIT
#define _MD_FINAL_INIT                  _PR_MD_FINAL_INIT
#define _MD_INIT_CPUS()

/* --- User Threading stuff --- */
#define _MD_EXIT

#define _MD_CLEANUP_BEFORE_EXIT              _PR_MD_CLEANUP_BEFORE_EXIT

/* --- Intervals --- */
#define _MD_INTERVAL_INIT                 _PR_MD_INTERVAL_INIT
#define _MD_GET_INTERVAL                  _PR_MD_GET_INTERVAL
#define _MD_INTERVAL_PER_SEC              _PR_MD_INTERVAL_PER_SEC
#define _MD_INTERVAL_PER_MILLISEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000000)

/* --- Scheduler stuff --- */
#define LOCK_SCHEDULER()                 0
#define UNLOCK_SCHEDULER()               0
#define _PR_LockSched()                	 0
#define _PR_UnlockSched()                0

/* --- Initialization stuff --- */
#define _MD_INIT_LOCKS()

/* --- Stack stuff --- */
#define _MD_INIT_STACK                   _PR_MD_INIT_STACK
#define _MD_CLEAR_STACK(stack)

/*
** Watcom needs to see this to make the linker work.
**
*/
PR_EXTERN(void) _PR_NativeDestroyThread(PRThread *thread);
PR_EXTERN(void) _PR_UserDestroyThread(PRThread *thread);


/*
 * _PR_ShutdownLinker unloads all dlls loaded by the application via
 * calls to PR_LoadLibrary
 */
void _PR_ShutdownLinker(void);


/*
** If thread emulation is used, then setjmp/longjmp stores the register
** state of each thread.
**
** CatchBuf layout:
**  context[0] - IP
**  context[1] - CS
**  context[2] - SP
**  context[3] - BP
**  context[4] - SI
**  context[5] - DI
**  context[6] - DS
**  context[7] - ?? (maybe flags)
**  context[8] - SS
*/
#define PR_CONTEXT_TYPE     CATCHBUF
#define PR_NUM_GCREGS       9

#define _MD_GET_SP(thread)  ((thread)->md.SP)
#define CONTEXT(_t)  ((_t)->md.context)

/*
** Initialize a thread context to run "e(o,a)" when started
*/
#define _MD_INIT_CONTEXT(_t, sp, epa, stat )   \
{                                              \
     *(stat) = PR_TRUE;                        \
     Catch((_t)->md.context );             \
     (_t)->md.context[0] = OFFSETOF(epa);         \
     (_t)->md.context[1] = SELECTOROF(epa);                       \
     (_t)->md.context[2] = OFFSETOF(_pr_top_of_task_stack - 64);  \
     (_t)->md.context[3]  = 0;                 \
}

#define _MD_SWITCH_CONTEXT(_t)                 \
    if (!Catch((_t)->md.context)) {            \
        int     garbCollPlaceHolder;           \
        (_t)->md.errcode = errno;              \
        (_t)->md.SP = &garbCollPlaceHolder;    \
        _PR_Schedule();                        \
    }

#define _MD_SAVE_CONTEXT(_t)                    \
    {                                           \
        int     garbCollPlaceHolder;            \
        Catch((_t)->md.context);                \
        (_t)->md.errcode = errno;               \
        (_t)->md.SP = &garbCollPlaceHolder;     \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _PR_MD_RESTORE_CONTEXT  _MD_RESTORE_CONTEXT

/*
 * Memory-mapped files
 */

struct _MDFileMap {
    PRInt8 unused;
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


/* --- Error mapping ----------------------------------- */
extern void _PR_MD_map_error( int err );

#define _PR_MD_MAP_OPENDIR_ERROR        _PR_MD_map_error
#define _PR_MD_MAP_CLOSEDIR_ERROR       _PR_MD_map_error
#define _PR_MD_MAP_READDIR_ERROR        _PR_MD_map_error
#define _PR_MD_MAP_DELETE_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_STAT_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_FSTAT_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_RENAME_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_ACCESS_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_MKDIR_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_RMDIR_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_READ_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_TRANSMITFILE_ERROR   _PR_MD_map_error
#define _PR_MD_MAP_WRITE_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_LSEEK_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_FSYNC_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_CLOSE_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_SOCKET_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_RECV_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_RECVFROM_ERROR       _PR_MD_map_error
#define _PR_MD_MAP_SEND_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_SENDTO_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_ACCEPT_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_ACCEPTEX_ERROR       _PR_MD_map_error
#define _PR_MD_MAP_CONNECT_ERROR        _PR_MD_map_error
#define _PR_MD_MAP_BIND_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_LISTEN_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_SHUTDOWN_ERROR       _PR_MD_map_error
#define _PR_MD_MAP_GETSOCKNAME_ERROR    _PR_MD_map_error
#define _PR_MD_MAP_GETPEERNAME_ERROR    _PR_MD_map_error
#define _PR_MD_MAP_GETSOCKOPT_ERROR     _PR_MD_map_error
#define _PR_MD_MAP_SETSOCKOPT_ERROR     _PR_MD_map_error
#define _PR_MD_MAP_OPEN_ERROR           _PR_MD_map_error
#define _PR_MD_MAP_GETHOSTNAME_ERROR    _PR_MD_map_error
#define _PR_MD_MAP_SELECT_ERROR         _PR_MD_map_error
#define _PR_MD_MAP_LOCKF_ERROR          _PR_MD_map_error
#define _PR_MD_MAP_WSASTARTUP_ERROR     _PR_MD_map_error

#endif /* nspr_win16_defs_h___ */
