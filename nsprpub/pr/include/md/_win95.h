/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_win95_defs_h___
#define nspr_win95_defs_h___

#include "prio.h"

#include <windows.h>
#include <winsock.h>
#include <errno.h>

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH      "win32"
#define _PR_SI_SYSNAME        "WIN95"
#if defined(_M_IX86) || defined(_X86_)
#define _PR_SI_ARCHITECTURE   "x86"
#elif defined(_M_X64) || defined(_M_AMD64) || defined(_AMD64_)
#define _PR_SI_ARCHITECTURE   "x86-64"
#elif defined(_M_IA64) || defined(_IA64_)
#define _PR_SI_ARCHITECTURE   "ia64"
#elif defined(_M_ARM) || defined(_ARM_)
#define _PR_SI_ARCHITECTURE   "arm"
#else
#error unknown processor architecture
#endif

#define HAVE_DLL
#undef  HAVE_THREAD_AFFINITY
#define _PR_HAVE_GETADDRINFO
#define _PR_INET6_PROBE
#ifndef _PR_INET6
#define AF_INET6 23
/* newer ws2tcpip.h provides these */
#ifndef AI_CANONNAME
#define AI_CANONNAME 0x2
#define AI_NUMERICHOST 0x4
#define NI_NUMERICHOST 0x02
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
#endif
#define _PR_HAVE_MD_SOCKADDR_IN6
/* isomorphic to struct in6_addr on Windows */
struct _md_in6_addr {
    union {
        PRUint8  _S6_u8[16];
        PRUint16 _S6_u16[8];
    } _S6_un;
};
/* isomorphic to struct sockaddr_in6 on Windows */
struct _md_sockaddr_in6 {
    PRInt16 sin6_family;
    PRUint16 sin6_port;
    PRUint32 sin6_flowinfo;
    struct _md_in6_addr sin6_addr;
    PRUint32 sin6_scope_id;
};
#endif
#define _PR_HAVE_THREADSAFE_GETHOST
#define _PR_HAVE_ATOMIC_OPS
#define PR_HAVE_WIN32_NAMED_SHARED_MEMORY

/* --- Common User-Thread/Native-Thread Definitions --------------------- */

/* --- Globals --- */
extern struct PRLock                      *_pr_schedLock;

/* --- Typedefs --- */
typedef void (*FiberFunc)(void *);

#define PR_NUM_GCREGS           8
typedef PRInt32	                PR_CONTEXT_TYPE[PR_NUM_GCREGS];
#define GC_VMBASE               0x40000000
#define GC_VMLIMIT              0x00FFFFFF

#define _MD_MAGIC_THREAD	0x22222222
#define _MD_MAGIC_THREADSTACK	0x33333333
#define _MD_MAGIC_SEGMENT	0x44444444
#define _MD_MAGIC_DIR		0x55555555
#define _MD_MAGIC_CV        0x66666666

struct _MDCPU {
    int              unused;
};

struct _MDThread {
    HANDLE           blocked_sema;      /* Threads block on this when waiting
                                         * for IO or CondVar.
                                         */
    PRBool           inCVWaitQueue;     /* PR_TRUE if the thread is in the
                                         * wait queue of some cond var.
                                         * PR_FALSE otherwise.  */
    HANDLE           handle;            /* Win32 thread handle */
    PRUint32         id;
    void            *sp;                /* only valid when suspended */
    PRUint32         magic;             /* for debugging */
    PR_CONTEXT_TYPE  gcContext;         /* Thread context for GC */
    struct PRThread *prev, *next;       /* used by the cvar wait queue to
                                         * chain the PRThread structures
                                         * together */
    void (*start)(void *);              /* used by _PR_MD_CREATE_THREAD to
                                         * pass its 'start' argument to
                                         * pr_root. */
};

struct _MDThreadStack {
    PRUint32           magic;          /* for debugging */
};

struct _MDSegment {
    PRUint32           magic;          /* for debugging */
};

#undef PROFILE_LOCKS

struct _MDDir {
    HANDLE           d_hdl;
    WIN32_FIND_DATAA d_entry;
    PRBool           firstEntry;     /* Is this the entry returned
                                      * by FindFirstFile()? */
    PRUint32         magic;          /* for debugging */
};

#ifdef MOZ_UNICODE
struct _MDDirUTF16 {
    HANDLE           d_hdl;
    WIN32_FIND_DATAW d_entry;
    PRBool           firstEntry;     /* Is this the entry returned
                                      * by FindFirstFileW()? */
    PRUint32         magic;          /* for debugging */
};
#endif /* MOZ_UNICODE */

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
    CRITICAL_SECTION mutex;          /* this is recursive on NT */

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

struct _MDSemaphore {
    HANDLE           sem;
};

struct _MDFileDesc {
    PROsfd osfd;     /* The osfd can come from one of three spaces:
                      * - For stdin, stdout, and stderr, we are using
                      *   the libc file handle (0, 1, 2), which is an int.
                      * - For files and pipes, we are using Win32 HANDLE,
                      *   which is a void*.
                      * - For sockets, we are using Winsock SOCKET, which
                      *   is a u_int.
                      */
};

struct _MDProcess {
    HANDLE handle;
    DWORD id;
};

/* --- Misc stuff --- */
#define _MD_GET_SP(thread)            (thread)->md.gcContext[6]

/* --- NT security stuff --- */

extern void _PR_NT_InitSids(void);
extern void _PR_NT_FreeSids(void);
extern PRStatus _PR_NT_MakeSecurityDescriptorACL(
    PRIntn mode,
    DWORD accessTable[],
    PSECURITY_DESCRIPTOR *resultSD,
    PACL *resultACL
);
extern void _PR_NT_FreeSecurityDescriptorACL(
    PSECURITY_DESCRIPTOR pSD, PACL pACL);

/* --- IO stuff --- */

#define _MD_OPEN                      _PR_MD_OPEN
#define _MD_OPEN_FILE                 _PR_MD_OPEN_FILE
#define _MD_READ                      _PR_MD_READ
#define _MD_WRITE                     _PR_MD_WRITE
#define _MD_WRITEV                    _PR_MD_WRITEV
#define _MD_LSEEK                     _PR_MD_LSEEK
#define _MD_LSEEK64                   _PR_MD_LSEEK64
extern PRInt32 _MD_CloseFile(PROsfd osfd);
#define _MD_CLOSE_FILE                _MD_CloseFile
#define _MD_GETFILEINFO               _PR_MD_GETFILEINFO
#define _MD_GETFILEINFO64             _PR_MD_GETFILEINFO64
#define _MD_GETOPENFILEINFO           _PR_MD_GETOPENFILEINFO
#define _MD_GETOPENFILEINFO64         _PR_MD_GETOPENFILEINFO64
#define _MD_STAT                      _PR_MD_STAT
#define _MD_RENAME                    _PR_MD_RENAME     
#define _MD_ACCESS                    _PR_MD_ACCESS     
#define _MD_DELETE                    _PR_MD_DELETE     
#define _MD_MKDIR                     _PR_MD_MKDIR      
#define _MD_MAKE_DIR                  _PR_MD_MAKE_DIR
#define _MD_RMDIR                     _PR_MD_RMDIR      
#define _MD_LOCKFILE                  _PR_MD_LOCKFILE
#define _MD_TLOCKFILE                 _PR_MD_TLOCKFILE
#define _MD_UNLOCKFILE                _PR_MD_UNLOCKFILE

/* --- UTF16 IO stuff --- */
extern PRBool _pr_useUnicode;
#ifdef MOZ_UNICODE
#define _MD_OPEN_FILE_UTF16           _PR_MD_OPEN_FILE_UTF16
#define _MD_OPEN_DIR_UTF16            _PR_MD_OPEN_DIR_UTF16
#define _MD_READ_DIR_UTF16            _PR_MD_READ_DIR_UTF16
#define _MD_CLOSE_DIR_UTF16           _PR_MD_CLOSE_DIR_UTF16
#define _MD_GETFILEINFO64_UTF16       _PR_MD_GETFILEINFO64_UTF16
#endif /* MOZ_UNICODE */

/* --- Socket IO stuff --- */
extern void _PR_MD_InitSockets(void);
extern void _PR_MD_CleanupSockets(void);
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
extern void _MD_MakeNonblock(PRFileDesc *f);
#define _MD_MAKE_NONBLOCK             _MD_MakeNonblock
#define _MD_INIT_FD_INHERITABLE       _PR_MD_INIT_FD_INHERITABLE
#define _MD_QUERY_FD_INHERITABLE      _PR_MD_QUERY_FD_INHERITABLE
#define _MD_SHUTDOWN                  _PR_MD_SHUTDOWN
#define _MD_LISTEN                    _PR_MD_LISTEN
extern PRInt32 _MD_CloseSocket(PROsfd osfd);
#define _MD_CLOSE_SOCKET              _MD_CloseSocket
#define _MD_SENDTO                    _PR_MD_SENDTO
#define _MD_RECVFROM                  _PR_MD_RECVFROM
#define _MD_SOCKETPAIR(s, type, proto, sv) -1
#define _MD_GETSOCKNAME               _PR_MD_GETSOCKNAME
#define _MD_GETPEERNAME               _PR_MD_GETPEERNAME
#define _MD_GETSOCKOPT                _PR_MD_GETSOCKOPT
#define _MD_SETSOCKOPT                _PR_MD_SETSOCKOPT
#define _MD_SET_FD_INHERITABLE        _PR_MD_SET_FD_INHERITABLE
#define _MD_SELECT                    select
#define _MD_FSYNC                     _PR_MD_FSYNC
#define READ_FD                       1
#define WRITE_FD                      2

#define _MD_INIT_ATOMIC()
#if defined(_M_IX86) || defined(_X86_)
#define _MD_ATOMIC_INCREMENT          _PR_MD_ATOMIC_INCREMENT
#define _MD_ATOMIC_ADD          	  _PR_MD_ATOMIC_ADD
#define _MD_ATOMIC_DECREMENT          _PR_MD_ATOMIC_DECREMENT
#else /* non-x86 processors */
#define _MD_ATOMIC_INCREMENT(x)       InterlockedIncrement((PLONG)x)
#define _MD_ATOMIC_ADD(ptr,val)    (InterlockedExchangeAdd((PLONG)ptr, (LONG)val) + val)
#define _MD_ATOMIC_DECREMENT(x)       InterlockedDecrement((PLONG)x)
#endif /* x86 */
#define _MD_ATOMIC_SET(x,y)           InterlockedExchange((PLONG)x, (LONG)y)

#define _MD_INIT_IO                   _PR_MD_INIT_IO


/* win95 doesn't have async IO */
#define _MD_SOCKET                    _PR_MD_SOCKET
extern PRInt32 _MD_SocketAvailable(PRFileDesc *fd);
#define _MD_SOCKETAVAILABLE           _MD_SocketAvailable
#define _MD_PIPEAVAILABLE             _PR_MD_PIPEAVAILABLE
#define _MD_CONNECT                   _PR_MD_CONNECT
extern PROsfd _MD_Accept(PRFileDesc *fd, PRNetAddr *raddr, PRUint32 *rlen,
        PRIntervalTime timeout);
#define _MD_ACCEPT                    _MD_Accept
#define _MD_BIND                      _PR_MD_BIND
#define _MD_RECV                      _PR_MD_RECV
#define _MD_SEND                      _PR_MD_SEND
#define _MD_PR_POLL                   _PR_MD_PR_POLL

/* --- Scheduler stuff --- */
// #define _MD_PAUSE_CPU                 _PR_MD_PAUSE_CPU
#define _MD_PAUSE_CPU

/* --- DIR stuff --- */
#define PR_DIRECTORY_SEPARATOR        '\\'
#define PR_DIRECTORY_SEPARATOR_STR    "\\"
#define PR_PATH_SEPARATOR		';'
#define PR_PATH_SEPARATOR_STR		";"
#define _MD_ERRNO()                   GetLastError()
#define _MD_OPEN_DIR                  _PR_MD_OPEN_DIR
#define _MD_CLOSE_DIR                 _PR_MD_CLOSE_DIR
#define _MD_READ_DIR                  _PR_MD_READ_DIR

/* --- Segment stuff --- */
#define _MD_INIT_SEGS()
#define _MD_ALLOC_SEGMENT(seg, size, vaddr)   0
#define _MD_FREE_SEGMENT(seg)

/* --- Environment Stuff --- */
#define _MD_GET_ENV                 _PR_MD_GET_ENV
#define _MD_PUT_ENV                 _PR_MD_PUT_ENV

/* --- Threading Stuff --- */
#define _MD_DEFAULT_STACK_SIZE            0
#define _MD_INIT_THREAD             _PR_MD_INIT_THREAD
#define _MD_INIT_ATTACHED_THREAD    _PR_MD_INIT_THREAD
#define _MD_CREATE_THREAD           _PR_MD_CREATE_THREAD
#define _MD_YIELD                   _PR_MD_YIELD
#define _MD_SET_PRIORITY            _PR_MD_SET_PRIORITY
#define _MD_SET_CURRENT_THREAD_NAME _PR_MD_SET_CURRENT_THREAD_NAME
#define _MD_CLEAN_THREAD            _PR_MD_CLEAN_THREAD
#define _MD_SETTHREADAFFINITYMASK   _PR_MD_SETTHREADAFFINITYMASK
#define _MD_GETTHREADAFFINITYMASK   _PR_MD_GETTHREADAFFINITYMASK
#define _MD_EXIT_THREAD             _PR_MD_EXIT_THREAD
#define _MD_EXIT                    _PR_MD_EXIT
#define _MD_SUSPEND_THREAD          _PR_MD_SUSPEND_THREAD
#define _MD_RESUME_THREAD           _PR_MD_RESUME_THREAD
#define _MD_SUSPEND_CPU             _PR_MD_SUSPEND_CPU
#define _MD_RESUME_CPU              _PR_MD_RESUME_CPU
#define _MD_BEGIN_SUSPEND_ALL()
#define _MD_BEGIN_RESUME_ALL()
#define _MD_END_SUSPEND_ALL()
#define _MD_END_RESUME_ALL()

/* --- Lock stuff --- */
#define _PR_LOCK                      _MD_LOCK
#define _PR_UNLOCK					  _MD_UNLOCK

#define _MD_NEW_LOCK(lock)            (InitializeCriticalSection(&((lock)->mutex)),(lock)->notified.length=0,(lock)->notified.link=NULL,PR_SUCCESS)
#define _MD_FREE_LOCK(lock)           DeleteCriticalSection(&((lock)->mutex))
#define _MD_LOCK(lock)                EnterCriticalSection(&((lock)->mutex))
#define _MD_TEST_AND_LOCK(lock)       (EnterCriticalSection(&((lock)->mutex)),0)
#define _MD_UNLOCK                    _PR_MD_UNLOCK

/* --- lock and cv waiting --- */
#define _MD_WAIT                      _PR_MD_WAIT
#define _MD_WAKEUP_WAITER             _PR_MD_WAKEUP_WAITER

/* --- CVar ------------------- */
#define _MD_WAIT_CV					  _PR_MD_WAIT_CV
#define _MD_NEW_CV					  _PR_MD_NEW_CV
#define _MD_FREE_CV					  _PR_MD_FREE_CV
#define _MD_NOTIFY_CV				  _PR_MD_NOTIFY_CV	
#define _MD_NOTIFYALL_CV			  _PR_MD_NOTIFYALL_CV

   /* XXXMB- the IOQ stuff is certainly not working correctly yet. */
// extern  struct _MDLock              _pr_ioq_lock;
#define _MD_IOQ_LOCK()                
#define _MD_IOQ_UNLOCK()              


/* --- Initialization stuff --- */
#define _MD_START_INTERRUPTS()
#define _MD_STOP_INTERRUPTS()
#define _MD_DISABLE_CLOCK_INTERRUPTS()
#define _MD_ENABLE_CLOCK_INTERRUPTS()
#define _MD_BLOCK_CLOCK_INTERRUPTS()
#define _MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _MD_EARLY_INIT                _PR_MD_EARLY_INIT
#define _MD_FINAL_INIT()
#define _MD_EARLY_CLEANUP()
#define _MD_INIT_CPUS()
#define _MD_INIT_RUNNING_CPU(cpu)

struct PRProcess;
struct PRProcessAttr;

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

#define _MD_CLEANUP_BEFORE_EXIT           _PR_MD_CLEANUP_BEFORE_EXIT
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status) \
    PR_BEGIN_MACRO \
    *status = PR_TRUE; \
    PR_END_MACRO
#define _MD_SWITCH_CONTEXT
#define _MD_RESTORE_CONTEXT

/* --- Intervals --- */
#define _MD_INTERVAL_INIT                 _PR_MD_INTERVAL_INIT
#define _MD_GET_INTERVAL                  _PR_MD_GET_INTERVAL
#define _MD_INTERVAL_PER_SEC              _PR_MD_INTERVAL_PER_SEC
#define _MD_INTERVAL_PER_MILLISEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()       (_PR_MD_INTERVAL_PER_SEC() / 1000000)

/* --- Time --- */
extern void _PR_FileTimeToPRTime(const FILETIME *filetime, PRTime *prtm);

#ifdef WINCE
extern void _MD_InitTime(void);
extern void _MD_CleanupTime(void);
#endif

/* --- Native-Thread Specific Definitions ------------------------------- */

extern struct PRThread * _MD_CURRENT_THREAD(void);

#ifdef _PR_USE_STATIC_TLS
extern __declspec(thread) struct PRThread *_pr_currentThread;
#define _MD_GET_ATTACHED_THREAD() _pr_currentThread
#define _MD_SET_CURRENT_THREAD(_thread) (_pr_currentThread = (_thread))

extern __declspec(thread) struct PRThread *_pr_thread_last_run;
#define _MD_LAST_THREAD() _pr_thread_last_run
#define _MD_SET_LAST_THREAD(_thread) (_pr_thread_last_run = 0)

extern __declspec(thread) struct _PRCPU *_pr_currentCPU;
#define _MD_CURRENT_CPU() _pr_currentCPU
#define _MD_SET_CURRENT_CPU(_cpu) (_pr_currentCPU = 0)
#else /* _PR_USE_STATIC_TLS */
extern DWORD _pr_currentThreadIndex;
#define _MD_GET_ATTACHED_THREAD() ((PRThread *) TlsGetValue(_pr_currentThreadIndex))
#define _MD_SET_CURRENT_THREAD(_thread) TlsSetValue(_pr_currentThreadIndex, (_thread))

extern DWORD _pr_lastThreadIndex;
#define _MD_LAST_THREAD() ((PRThread *) TlsGetValue(_pr_lastThreadIndex))
#define _MD_SET_LAST_THREAD(_thread) TlsSetValue(_pr_lastThreadIndex, 0)

extern DWORD _pr_currentCPUIndex;
#define _MD_CURRENT_CPU() ((struct _PRCPU *) TlsGetValue(_pr_currentCPUIndex))
#define _MD_SET_CURRENT_CPU(_cpu) TlsSetValue(_pr_currentCPUIndex, 0)
#endif /* _PR_USE_STATIC_TLS */

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

/* --- Memory-mapped files stuff --- */

struct _MDFileMap {
    HANDLE hFileMap;
    DWORD dwAccess;
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

/* --- Named semaphores stuff --- */
#define _PR_HAVE_NAMED_SEMAPHORES
#define _MD_OPEN_SEMAPHORE            _PR_MD_OPEN_SEMAPHORE
#define _MD_WAIT_SEMAPHORE            _PR_MD_WAIT_SEMAPHORE
#define _MD_POST_SEMAPHORE            _PR_MD_POST_SEMAPHORE
#define _MD_CLOSE_SEMAPHORE           _PR_MD_CLOSE_SEMAPHORE
#define _MD_DELETE_SEMAPHORE(name)    PR_SUCCESS  /* no op */

#endif /* nspr_win32_defs_h___ */
