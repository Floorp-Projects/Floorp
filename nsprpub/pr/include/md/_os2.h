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

#ifndef nspr_os2_defs_h___
#define nspr_os2_defs_h___

#define INCL_DOS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_WPS
#include <os2.h>
#include <sys/select.h>

#include "prio.h"

#include <errno.h>

#ifdef XP_OS2_VACPP
/* TODO RAMSEMs need to be written for GCC/EMX */
#define USE_RAMSEM
#endif

#ifdef USE_RAMSEM
#pragma pack(4)

#pragma pack(2)
typedef struct _RAMSEM
{
   ULONG   ulTIDPID;
   ULONG   hevSem;
   ULONG   cLocks;
   USHORT  cWaiting;
   USHORT  cPosts;
} RAMSEM, *PRAMSEM;

typedef struct _CRITICAL_SECTION
{
    ULONG ulReserved[4]; /* Same size as RAMSEM */
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;
#pragma pack(4)

APIRET _Optlink SemRequest486(PRAMSEM, ULONG);
APIRET _Optlink SemReleasex86(PRAMSEM, ULONG);
#endif

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH      "os2"
#define _PR_SI_SYSNAME        "OS2"
#define _PR_SI_ARCHITECTURE   "x86"    /* XXXMB hardcode for now */

#define HAVE_DLL
#define _PR_GLOBAL_THREADS_ONLY
#undef  HAVE_THREAD_AFFINITY
#define _PR_HAVE_THREADSAFE_GETHOST
#define _PR_HAVE_ATOMIC_OPS

#define HANDLE unsigned long
#define HINSTANCE HMODULE

/* --- Common User-Thread/Native-Thread Definitions --------------------- */

/* --- Globals --- */
extern struct PRLock                      *_pr_schedLock;

/* --- Typedefs --- */
typedef void (*FiberFunc)(void *);

#define PR_NUM_GCREGS           8
typedef PRInt32	                PR_CONTEXT_TYPE[PR_NUM_GCREGS];
#define GC_VMBASE               0x40000000
#define GC_VMLIMIT              0x00FFFFFF
typedef int (*FARPROC)();

#define _MD_MAGIC_THREAD	0x22222222
#define _MD_MAGIC_THREADSTACK	0x33333333
#define _MD_MAGIC_SEGMENT	0x44444444
#define _MD_MAGIC_DIR		0x55555555
#define _MD_MAGIC_CV        0x66666666

struct _MDSemaphore {
   HEV sem;
};

struct _MDCPU {
    int              unused;
}; 

struct _MDThread {
    HEV              blocked_sema;      /* Threads block on this when waiting
                                         * for IO or CondVar.
                                         */
    PRBool           inCVWaitQueue;     /* PR_TRUE if the thread is in the
                                         * wait queue of some cond var.
                                         * PR_FALSE otherwise.  */
    TID              handle;            /* OS/2 thread handle */
    void            *sp;                /* only valid when suspended */
    PRUint32         magic;             /* for debugging */
    PR_CONTEXT_TYPE  gcContext;         /* Thread context for GC */
    struct PRThread *prev, *next;       /* used by the cvar wait queue to
                                         * chain the PRThread structures
                                         * together */
};

struct _MDThreadStack {
    PRUint32           magic;          /* for debugging */
};

struct _MDSegment {
    PRUint32           magic;          /* for debugging */
};

#undef PROFILE_LOCKS

struct _MDDir {
    HDIR           d_hdl;
    FILEFINDBUF3  d_entry;
    PRBool           firstEntry;     /* Is this the entry returned
                                      * by FindFirstFile()? */
    PRUint32         magic;          /* for debugging */
};

struct _MDCVar {
    PRUint32 magic;
    struct PRThread *waitHead, *waitTail;  /* the wait queue: a doubly-
                                            * linked list of threads
                                            * waiting on this condition
                                            * variable */
    PRIntn nwait;                          /* number of threads in the
                                            * wait queue */
};

#define _MD_CV_NOTIFIED_LENGTH 6
typedef struct _MDNotified _MDNotified;
struct _MDNotified {
    PRIntn length;                     /* # of used entries in this
                                        * structure */
    struct {
        struct _MDCVar *cv;            /* the condition variable notified */
        PRIntn times;                  /* and the number of times notified */
        struct PRThread *notifyHead;   /* list of threads to wake up */
    } cv[_MD_CV_NOTIFIED_LENGTH];
    _MDNotified *link;                 /* link to another of these, or NULL */
};

struct _MDLock {
#ifdef USE_RAMSEM
    CRITICAL_SECTION mutex;            /* this is recursive on NT */
#else
    HMTX mutex;                        /* this is recursive on NT */
#endif

    /*
     * When notifying cvars, there is no point in actually
     * waking up the threads waiting on the cvars until we've
     * released the lock.  So, we temporarily record the cvars.
     * When doing an unlock, we'll then wake up the waiting threads.
     */
    struct _MDNotified notified;     /* array of conditions notified */
#ifdef PROFILE_LOCKS
    PRInt32 hitcount;
    PRInt32 misscount;
#endif
};

struct _MDFileDesc {
    PRInt32 osfd;    /* The osfd can come from one of three spaces:
                      * - For stdin, stdout, and stderr, we are using
                      *   the libc file handle (0, 1, 2), which is an int.
                      * - For files and pipes, we are using OS/2 handles,
                      *   which is a void*.
                      * - For sockets, we are using int
                      */
};

struct _MDProcess {
   PID pid;
};

/* --- Misc stuff --- */
#define _MD_GET_SP(thread)            (thread)->md.gcContext[6]

/* --- IO stuff --- */

#define _MD_OPEN                      (_PR_MD_OPEN)
#define _MD_OPEN_FILE                 (_PR_MD_OPEN)
#define _MD_READ                      (_PR_MD_READ)
#define _MD_WRITE                     (_PR_MD_WRITE)
#define _MD_WRITEV                    (_PR_MD_WRITEV)
#define _MD_LSEEK                     (_PR_MD_LSEEK)
#define _MD_LSEEK64                   (_PR_MD_LSEEK64)
extern PRInt32 _MD_CloseFile(PRInt32 osfd);
#define _MD_CLOSE_FILE                _MD_CloseFile
#define _MD_GETFILEINFO               (_PR_MD_GETFILEINFO)
#define _MD_GETFILEINFO64             (_PR_MD_GETFILEINFO64)
#define _MD_GETOPENFILEINFO           (_PR_MD_GETOPENFILEINFO)
#define _MD_GETOPENFILEINFO64         (_PR_MD_GETOPENFILEINFO64)
#define _MD_STAT                      (_PR_MD_STAT)
#define _MD_RENAME                    (_PR_MD_RENAME)
#define _MD_ACCESS                    (_PR_MD_ACCESS)
#define _MD_DELETE                    (_PR_MD_DELETE)
#define _MD_MKDIR                     (_PR_MD_MKDIR)
#define _MD_MAKE_DIR                  (_PR_MD_MKDIR)
#define _MD_RMDIR                     (_PR_MD_RMDIR)
#define _MD_LOCKFILE                  (_PR_MD_LOCKFILE)
#define _MD_TLOCKFILE                 (_PR_MD_TLOCKFILE)
#define _MD_UNLOCKFILE                (_PR_MD_UNLOCKFILE)

/* --- Socket IO stuff --- */

#define TCPV40HDRS
#define BSD_SELECT

/* The ones that don't map directly may need to be re-visited... */
#ifdef XP_OS2_VACPP
#define EPIPE                     EBADF
#define EIO                       ECONNREFUSED
#endif
#define _MD_EACCES                EACCES
#define _MD_EADDRINUSE            EADDRINUSE
#define _MD_EADDRNOTAVAIL         EADDRNOTAVAIL
#define _MD_EAFNOSUPPORT          EAFNOSUPPORT
#define _MD_EAGAIN                EWOULDBLOCK
#define _MD_EALREADY              EALREADY
#define _MD_EBADF                 EBADF
#define _MD_ECONNREFUSED          ECONNREFUSED
#define _MD_ECONNRESET            ECONNRESET
#define _MD_EFAULT                SOCEFAULT
#define _MD_EINPROGRESS           EINPROGRESS
#define _MD_EINTR                 EINTR
#define _MD_EINVAL                EINVAL
#define _MD_EISCONN               EISCONN
#define _MD_ENETUNREACH           ENETUNREACH
#define _MD_ENOENT                ENOENT
#define _MD_ENOTCONN              ENOTCONN
#define _MD_ENOTSOCK              ENOTSOCK
#define _MD_EOPNOTSUPP            EOPNOTSUPP
#define _MD_EWOULDBLOCK           EWOULDBLOCK
#define _MD_GET_SOCKET_ERROR()    sock_errno()
#ifndef INADDR_LOOPBACK /* For some reason this is not defined in OS2 tcpip */
/*  #define INADDR_LOOPBACK         INADDR_ANY */
#endif  

#define _MD_INIT_FILEDESC(fd)
extern void _MD_MakeNonblock(PRFileDesc *f);
#define _MD_MAKE_NONBLOCK             _MD_MakeNonblock
#define _MD_INIT_FD_INHERITABLE       (_PR_MD_INIT_FD_INHERITABLE)
#define _MD_QUERY_FD_INHERITABLE      (_PR_MD_QUERY_FD_INHERITABLE)
#define _MD_SHUTDOWN                  (_PR_MD_SHUTDOWN)
#define _MD_LISTEN                    _PR_MD_LISTEN
extern PRInt32 _MD_CloseSocket(PRInt32 osfd);
#define _MD_CLOSE_SOCKET              _MD_CloseSocket
#define _MD_SENDTO                    (_PR_MD_SENDTO)
#define _MD_RECVFROM                  (_PR_MD_RECVFROM)
#define _MD_SOCKETPAIR                (_PR_MD_SOCKETPAIR)
#define _MD_GETSOCKNAME               (_PR_MD_GETSOCKNAME)
#define _MD_GETPEERNAME               (_PR_MD_GETPEERNAME)
#define _MD_GETSOCKOPT                (_PR_MD_GETSOCKOPT)
#define _MD_SETSOCKOPT                (_PR_MD_SETSOCKOPT)

#define _MD_FSYNC                     _PR_MD_FSYNC
#define _MD_SET_FD_INHERITABLE        (_PR_MD_SET_FD_INHERITABLE)

#ifdef _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT          _PR_MD_ATOMIC_INCREMENT
#define _MD_ATOMIC_ADD                _PR_MD_ATOMIC_ADD
#define _MD_ATOMIC_DECREMENT          _PR_MD_ATOMIC_DECREMENT
#define _MD_ATOMIC_SET                _PR_MD_ATOMIC_SET
#endif

#define _MD_INIT_IO                   (_PR_MD_INIT_IO)
#define _MD_PR_POLL                   (_PR_MD_PR_POLL)

#define _MD_SOCKET                    (_PR_MD_SOCKET)
extern PRInt32 _MD_SocketAvailable(PRFileDesc *fd);
#define _MD_SOCKETAVAILABLE           _MD_SocketAvailable
#define _MD_PIPEAVAILABLE             _MD_SocketAvailable
#define _MD_CONNECT                   (_PR_MD_CONNECT)
extern PRInt32 _MD_Accept(PRFileDesc *fd, PRNetAddr *raddr, PRUint32 *rlen,
        PRIntervalTime timeout);
#define _MD_ACCEPT                    _MD_Accept
#define _MD_BIND                      (_PR_MD_BIND)
#define _MD_RECV                      (_PR_MD_RECV)
#define _MD_SEND                      (_PR_MD_SEND)

/* --- Scheduler stuff --- */
/* #define _MD_PAUSE_CPU                 _PR_MD_PAUSE_CPU */
#define _MD_PAUSE_CPU

/* --- DIR stuff --- */
#define PR_DIRECTORY_SEPARATOR        '\\'
#define PR_DIRECTORY_SEPARATOR_STR    "\\"
#define PR_PATH_SEPARATOR		';'
#define PR_PATH_SEPARATOR_STR		";"
#define _MD_ERRNO()                   errno
#define _MD_OPEN_DIR                  (_PR_MD_OPEN_DIR)
#define _MD_CLOSE_DIR                 (_PR_MD_CLOSE_DIR)
#define _MD_READ_DIR                  (_PR_MD_READ_DIR)

/* --- Segment stuff --- */
#define _MD_INIT_SEGS()
#define _MD_ALLOC_SEGMENT(seg, size, vaddr)   0
#define _MD_FREE_SEGMENT(seg)

/* --- Environment Stuff --- */
#define _MD_GET_ENV                 (_PR_MD_GET_ENV)
#define _MD_PUT_ENV                 (_PR_MD_PUT_ENV)

/* --- Threading Stuff --- */
#define _MD_DEFAULT_STACK_SIZE      65536L
#define _MD_INIT_THREAD             (_PR_MD_INIT_THREAD)
#define _MD_INIT_ATTACHED_THREAD    (_PR_MD_INIT_THREAD)
#define _MD_CREATE_THREAD           (_PR_MD_CREATE_THREAD)
#define _MD_YIELD                   (_PR_MD_YIELD)
#define _MD_SET_PRIORITY            (_PR_MD_SET_PRIORITY)
#define _MD_CLEAN_THREAD            (_PR_MD_CLEAN_THREAD)
#define _MD_SETTHREADAFFINITYMASK   (_PR_MD_SETTHREADAFFINITYMASK)
#define _MD_GETTHREADAFFINITYMASK   (_PR_MD_GETTHREADAFFINITYMASK)
#define _MD_EXIT_THREAD             (_PR_MD_EXIT_THREAD)
#define _MD_SUSPEND_THREAD          (_PR_MD_SUSPEND_THREAD)
#define _MD_RESUME_THREAD           (_PR_MD_RESUME_THREAD)
#define _MD_SUSPEND_CPU             (_PR_MD_SUSPEND_CPU)
#define _MD_RESUME_CPU              (_PR_MD_RESUME_CPU)
#define _MD_WAKEUP_CPUS             (_PR_MD_WAKEUP_CPUS)
#define _MD_BEGIN_SUSPEND_ALL()
#define _MD_BEGIN_RESUME_ALL()
#define _MD_END_SUSPEND_ALL()
#define _MD_END_RESUME_ALL()

/* --- Lock stuff --- */
#define _PR_LOCK                      _MD_LOCK
#define _PR_UNLOCK					  _MD_UNLOCK

#ifdef USE_RAMSEM
#define _MD_NEW_LOCK                  (_PR_MD_NEW_LOCK)
#define _MD_FREE_LOCK(lock)           (DosCloseEventSem(((PRAMSEM)(&((lock)->mutex)))->hevSem))
#define _MD_LOCK(lock)                (SemRequest486(&((lock)->mutex), -1))
#define _MD_TEST_AND_LOCK(lock)       (SemRequest486(&((lock)->mutex), -1),0)
#define _MD_UNLOCK(lock)              \
    PR_BEGIN_MACRO \
    if (0 != (lock)->notified.length) { \
        md_UnlockAndPostNotifies((lock), NULL, NULL); \
    } else { \
        SemReleasex86( &(lock)->mutex, 0 ); \
    } \
    PR_END_MACRO
#else
#define _MD_NEW_LOCK                  (_PR_MD_NEW_LOCK)
#define _MD_FREE_LOCK(lock)           (DosCloseMutexSem((lock)->mutex))
#define _MD_LOCK(lock)                (DosRequestMutexSem((lock)->mutex, SEM_INDEFINITE_WAIT))
#define _MD_TEST_AND_LOCK(lock)       (DosRequestMutexSem((lock)->mutex, SEM_INDEFINITE_WAIT),0)
#define _MD_UNLOCK(lock)              \
    PR_BEGIN_MACRO \
    if (0 != (lock)->notified.length) { \
        md_UnlockAndPostNotifies((lock), NULL, NULL); \
    } else { \
        DosReleaseMutexSem((lock)->mutex); \
    } \
    PR_END_MACRO
#endif

/* --- lock and cv waiting --- */
#define _MD_WAIT                      (_PR_MD_WAIT)
#define _MD_WAKEUP_WAITER             (_PR_MD_WAKEUP_WAITER)

/* --- CVar ------------------- */
#define _MD_WAIT_CV					  (_PR_MD_WAIT_CV)
#define _MD_NEW_CV					  (_PR_MD_NEW_CV)
#define _MD_FREE_CV					  (_PR_MD_FREE_CV)
#define _MD_NOTIFY_CV				  (_PR_MD_NOTIFY_CV	)
#define _MD_NOTIFYALL_CV			  (_PR_MD_NOTIFYALL_CV)

   /* XXXMB- the IOQ stuff is certainly not working correctly yet. */
/* extern  struct _MDLock              _pr_ioq_lock; */
#define _MD_IOQ_LOCK()                
#define _MD_IOQ_UNLOCK()              


/* --- Initialization stuff --- */
#define _MD_START_INTERRUPTS()
#define _MD_STOP_INTERRUPTS()
#define _MD_DISABLE_CLOCK_INTERRUPTS()
#define _MD_ENABLE_CLOCK_INTERRUPTS()
#define _MD_BLOCK_CLOCK_INTERRUPTS()
#define _MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _MD_EARLY_INIT                (_PR_MD_EARLY_INIT)
#define _MD_FINAL_INIT()
#define _MD_INIT_CPUS()
#define _MD_INIT_RUNNING_CPU(cpu)

struct PRProcess;
struct PRProcessAttr;

#define _MD_CREATE_PROCESS _PR_CreateOS2Process
extern struct PRProcess * _PR_CreateOS2Process(
    const char *path,
    char *const *argv,
    char *const *envp,
    const struct PRProcessAttr *attr
);

#define _MD_DETACH_PROCESS _PR_DetachOS2Process
extern PRStatus _PR_DetachOS2Process(struct PRProcess *process);

/* --- Wait for a child process to terminate --- */
#define _MD_WAIT_PROCESS _PR_WaitOS2Process
extern PRStatus _PR_WaitOS2Process(struct PRProcess *process, 
    PRInt32 *exitCode);

#define _MD_KILL_PROCESS _PR_KillOS2Process
extern PRStatus _PR_KillOS2Process(struct PRProcess *process);

#define _MD_CLEANUP_BEFORE_EXIT()
#define _MD_EXIT                          (_PR_MD_EXIT)
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
    PR_BEGIN_MACRO \
    *status = PR_TRUE; \
    PR_END_MACRO
#define _MD_SWITCH_CONTEXT
#define _MD_RESTORE_CONTEXT

/* --- Intervals --- */
#define _MD_INTERVAL_INIT                 (_PR_MD_INTERVAL_INIT)
#define _MD_GET_INTERVAL                  (_PR_MD_GET_INTERVAL)
#define _MD_INTERVAL_PER_SEC              (_PR_MD_INTERVAL_PER_SEC)
#define _MD_INTERVAL_PER_MILLISEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000000)

/* --- Native-Thread Specific Definitions ------------------------------- */

typedef struct __NSPR_TLS
{
    struct PRThread  *_pr_thread_last_run;
    struct PRThread  *_pr_currentThread;
    struct _PRCPU    *_pr_currentCPU;
} _NSPR_TLS;

extern _NSPR_TLS*  pThreadLocalStorage;
NSPR_API(void) _PR_MD_ENSURE_TLS(void);

#define _MD_GET_ATTACHED_THREAD() pThreadLocalStorage->_pr_currentThread
extern struct PRThread * _MD_CURRENT_THREAD(void);
#define _MD_SET_CURRENT_THREAD(_thread) _PR_MD_ENSURE_TLS(); pThreadLocalStorage->_pr_currentThread = (_thread)

#define _MD_LAST_THREAD() pThreadLocalStorage->_pr_thread_last_run
#define _MD_SET_LAST_THREAD(_thread) _PR_MD_ENSURE_TLS(); pThreadLocalStorage->_pr_thread_last_run = (_thread)

#define _MD_CURRENT_CPU() pThreadLocalStorage->_pr_currentCPU
#define _MD_SET_CURRENT_CPU(_cpu) _PR_MD_ENSURE_TLS(); pThreadLocalStorage->_pr_currentCPU = (_cpu)

/* lth. #define _MD_SET_INTSOFF(_val) (_pr_ints_off = (_val)) */
/* lth. #define _MD_GET_INTSOFF() _pr_ints_off */
/* lth. #define _MD_INCREMENT_INTSOFF() (_pr_ints_off++) */
/* lth. #define _MD_DECREMENT_INTSOFF() (_pr_ints_off--) */

/* --- Scheduler stuff --- */
#define LOCK_SCHEDULER()                 0
#define UNLOCK_SCHEDULER()               0
#define _PR_LockSched()                	 0
#define _PR_UnlockSched()                0

/* --- Initialization stuff --- */
#define _MD_INIT_LOCKS()

/* --- Stack stuff --- */
#define _MD_INIT_STACK(stack, redzone)
#define _MD_CLEAR_STACK(stack)

/* --- Memory-mapped files stuff --- not implemented on OS/2 */

struct _MDFileMap {
    PRInt8 unused;
};

extern PRStatus _MD_CreateFileMap(struct PRFileMap *fmap, PRInt64 size);
#define _MD_CREATE_FILE_MAP _MD_CreateFileMap

extern PRInt32 _MD_GetMemMapAlignment(void);
#define _MD_GET_MEM_MAP_ALIGNMENT _MD_GetMemMapAlignment

extern void * _MD_MemMap(struct PRFileMap *fmap, PRInt64 offset,
        PRUint32 len);
#define _MD_MEM_MAP _MD_MemMap

extern PRStatus _MD_MemUnmap(void *addr, PRUint32 size);
#define _MD_MEM_UNMAP _MD_MemUnmap

extern PRStatus _MD_CloseFileMap(struct PRFileMap *fmap);
#define _MD_CLOSE_FILE_MAP _MD_CloseFileMap

/* Some stuff for setting up thread contexts */
typedef ULONG DWORD, *PDWORD;

/* The following definitions and two structures are new in OS/2 Warp 4.0.
 */
#ifndef CONTEXT_CONTROL
#define CONTEXT_CONTROL        0x00000001
#define CONTEXT_INTEGER        0x00000002
#define CONTEXT_SEGMENTS       0x00000004
#define CONTEXT_FLOATING_POINT 0x00000008
#define CONTEXT_FULL           0x0000000F

#pragma pack(2)
typedef struct _FPREG {
    ULONG      losig;    /*  Low 32-bits of the significand. */
    ULONG      hisig;    /*  High 32-bits of the significand. */
    USHORT     signexp;  /*  Sign and exponent. */
} FPREG;
typedef struct _CONTEXTRECORD {
    ULONG     ContextFlags;
    ULONG     ctx_env[7];
    FPREG     ctx_stack[8];
    ULONG     ctx_SegGs;     /*  GS register. */
    ULONG     ctx_SegFs;     /*  FS register. */
    ULONG     ctx_SegEs;     /*  ES register. */
    ULONG     ctx_SegDs;     /*  DS register. */
    ULONG     ctx_RegEdi;    /*  EDI register. */
    ULONG     ctx_RegEsi;    /*  ESI register. */
    ULONG     ctx_RegEax;    /*  EAX register. */
    ULONG     ctx_RegEbx;    /*  EBX register. */
    ULONG     ctx_RegEcx;    /*  ECX register. */
    ULONG     ctx_RegEdx;    /*  EDX register. */
    ULONG     ctx_RegEbp;    /*  EBP register. */
    ULONG     ctx_RegEip;    /*  EIP register. */
    ULONG     ctx_SegCs;     /*  CS register. */
    ULONG     ctx_EFlags;    /*  EFLAGS register. */
    ULONG     ctx_RegEsp;    /*  ESP register. */
    ULONG     ctx_SegSs;     /*  SS register. */
} CONTEXTRECORD, *PCONTEXTRECORD;
#pragma pack()
#endif

extern APIRET (* APIENTRY QueryThreadContext)(TID, ULONG, PCONTEXTRECORD);
unsigned long _System _DLL_InitTerm( unsigned long mod_handle, unsigned long flag);

/*
#define _pr_tid            (((PTIB2)_getTIBvalue(offsetof(TIB, tib_ptib2)))->tib2_ultid)
#define _pr_current_Thread (_system_tls[_pr_tid-1].__pr_current_thread)
*/

/* Some simple mappings of Windows API's to OS/2 API's to make our lives a
 * little bit easier.  Only add one here if it is a DIRECT mapping.  We are
 * not emulating anything.  Just mapping.
 */
#define FreeLibrary(x) DosFreeModule((HMODULE)x)
#define OutputDebugString(x)
                               
extern int _MD_os2_get_nonblocking_connect_error(int osfd);

#endif /* nspr_os2_defs_h___ */
