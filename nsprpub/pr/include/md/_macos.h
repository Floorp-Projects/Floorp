/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef prmacos_h___
#define prmacos_h___

//
// This file contains all changes and additions which need to be made to the NSPR runtime 
// for the Macintosh platform (specifically the Metrowerks environment).  This file should 
// only be incluced in Macintosh builds.
//

#define PR_DLL_SUFFIX		""
#define _PR_LOCAL_THREADS_ONLY
#define _PR_NO_PREEMPT	1
#define _PR_HAVE_ATOMIC_OPS 1

#include "prinit.h"
#include "prio.h"
#include "prlong.h"
#include "prlock.h"
#include "prcvar.h"
#include "prsem.h"
#include "prthread.h"
#include "prtime.h"
#include "prproces.h"

#if !defined(MAC_NSPR_STANDALONE)
#include "macstdlibextras.h"
#endif

#include <stddef.h>
#include <setjmp.h>

#include <Errors.h>
#include <OpenTransport.h>
#include <DriverServices.h>

#define _PR_HAVE_PEEK_BUFFER
#define _PR_PEEK_BUFFER_MAX (16 * 1024)
#define _PR_FD_NEED_EMULATE_MSG_PEEK(fd) 1

struct _MDProcess {
    PRInt8 notused;
};

struct _MDThread {
    jmp_buf      jb;
    int          osErrCode;
	PRLock *     asyncIOLock;
	PRCondVar *  asyncIOCVar;
    PRBool       missedIONotify;
    PRBool       missedAsyncNotify;
    PRBool       asyncNotifyPending;
};

struct _MDThreadStack {
    PRInt8 notused;
};

struct _MDLock {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDSegment {
    PRInt8 notused;
};

struct _MDCPU {
    AbsoluteTime    lastThreadSwitch;
    AbsoluteTime    lastWakeUpProcess;
    PRBool          trackScheduling;
};

typedef struct _MDSocketCallerInfo {
	PRThread *	thread;
	void *		cookie;
} _MDSocketCallerInfo;

struct _MDFileDesc {
    PRInt32         osfd;
    PRPackedBool    orderlyDisconnect;
    PRPackedBool    readReady;
    PRPackedBool    writeReady;
    PRPackedBool    exceptReady;
    PRLock *        miscLock;

    /* Server sockets: listen bit tells the notifier func what to do */
    PRBool          doListen;

    /* stored error for non-blocking connects, as a Unix-style error code */
    OTReason        disconnectError;

    _MDSocketCallerInfo  misc;
    _MDSocketCallerInfo  read;
    _MDSocketCallerInfo  write;
};

/*
** Iinitialization Related definitions
*/

#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_MD_FinalInit

/*
** Interrupts Related definitions
*/

#define _MD_GET_INTSOFF()               (_pr_intsOff)

#define _MD_INTSOFF(_is)                                  \
    PR_BEGIN_MACRO                                        \
        ENTER_CRITICAL_REGION();                          \
        (_is) = _PR_MD_GET_INTSOFF();                     \
        _PR_MD_SET_INTSOFF(1);                            \
        LEAVE_CRITICAL_REGION();                          \
    PR_END_MACRO

#if TARGET_CARBON
extern void _MD_SetIntsOff(PRInt32 ints);
#define _MD_SET_INTSOFF(_val)           _MD_SetIntsOff(_val)
#else /* not TARGET_CARBON */
#define _MD_SET_INTSOFF(_val)           (_pr_intsOff = _val)
#endif /* TARGET_CARBON */

#define _MD_START_INTERRUPTS			_MD_StartInterrupts
#define _MD_STOP_INTERRUPTS	    		_MD_StopInterrupts
#define _MD_BLOCK_CLOCK_INTERRUPTS()
#define _MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _MD_DISABLE_CLOCK_INTERRUPTS()
#define _MD_ENABLE_CLOCK_INTERRUPTS()

/*
** CPU Related definitions
*/

#define _MD_PAUSE_CPU		_MD_PauseCPU
#define _MD_CLEANUP_BEFORE_EXIT()
#define _MD_EXIT(status)	exit(status)
#define _MD_INIT_CPUS()
#define _MD_INIT_RUNNING_CPU(cpu) _MD_InitRunningCPU(cpu)

/*
** Process Related definitions
*/

extern struct PRProcess * _MD_CreateProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);
#define _MD_CREATE_PROCESS _MD_CreateProcess

extern PRStatus _MD_DetachProcess(PRProcess *process);
#define _MD_DETACH_PROCESS _MD_DetachProcess

extern PRStatus _MD_WaitProcess(PRProcess *process, PRInt32 *exitCode);
#define _MD_WAIT_PROCESS _MD_WaitProcess

extern PRStatus _MD_KillProcess(PRProcess *process);
#define _MD_KILL_PROCESS _MD_KillProcess

/*
** Memory Segments Related definitions
*/

#define _MD_INIT_SEGS()

/*
** Thread Stacks Debugging Related definitions
*/

#define _MD_INIT_STACK		_MD_InitStack
#define _MD_CLEAR_STACK		_MD_ClearStack

/*
** Locks Related definitions
*/

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(lock) (PR_SUCCESS)
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)

/*
** Thread Related definitions
*/

NSPR_API(PRThread *) PR_GetPrimaryThread();

#if defined(powerc) || defined(__powerc)
#define _MD_GET_PC(_t) (*((PRUint32 *)((_t)->md.jb)))
#define _MD_GET_SP(_t) (*((PRUint32 *)((_t)->md.jb) + 2))
#define _MD_GET_TOC(_t) (*((PRUint32 *)((_t)->md.jb) + 3))
#define INIT_STACKPTR(stackTop) ((unsigned char*)stackTop - 128)
#define PR_NUM_GCREGS 70
#else
#define _MD_GET_PC(_t) (*((PRUint32 *)((_t)->md.jb) + 6))
#define _MD_GET_SP(_t) (*((PRUint32 *)((_t)->md.jb) + 12))
#define INIT_STACKPTR(stackTop) ((unsigned char*)stackTop - 4)
#define PR_NUM_GCREGS 13
#endif

#define _MD_DEFAULT_STACK_SIZE (58 * 1024)
#define _MD_MINIMUM_STACK_SIZE (58 * 1024)

/*
** Initialize the thread machine dependent data structure
*/
extern PRStatus _MD_InitThread(PRThread *thread);
#define _MD_INIT_THREAD	_MD_InitThread

/*
** Clean-up the thread machine dependent data structure
*/
#define	_MD_CLEAN_THREAD(_thread)						\
    PR_BEGIN_MACRO				      					\
	PR_DestroyCondVar(_thread->md.asyncIOCVar);			\
	PR_DestroyLock(_thread->md.asyncIOLock);			\
    PR_END_MACRO


/*
** Initialize the thread context preparing it to execute _main.
** *sp = 0 zeros out the sp for the first stack frame so that
** stack walking code can find the top of the stack.
*/
#if defined(powerc) || defined(__powerc)
#define _MD_INIT_CONTEXT(_thread, _sp, _main, _status)	\
    PR_BEGIN_MACRO				      					\
 	unsigned char *sp;									\
 	unsigned long *tvect;								\
	long **jb = (_thread)->md.jb;		      			\
    *((PRBool *)_status) = PR_TRUE;              		\
	(void) setjmp(jb);			      					\
    sp = INIT_STACKPTR(_sp);							\
    *sp = 0;                                            \
    (_MD_GET_SP(_thread)) = (long) sp;   				\
	tvect = (unsigned long *)_main;						\
    (_MD_GET_PC(_thread)) = (int) *tvect;   			\
    (_MD_GET_TOC(_thread)) = (int) *(tvect+1);   		\
	_thread->no_sched = 0;                              \
    PR_END_MACRO
#else
#define _MD_INIT_CONTEXT(_thread, _sp, _main, _status)	\
    PR_BEGIN_MACRO				      					\
 	unsigned char *sp;									\
	long **jb = (_thread)->md.jb;		      			\
    *((PRBool *)_status) = PR_TRUE;              		\
	(void) setjmp(jb);			      					\
    sp = INIT_STACKPTR(_sp);							\
    (_MD_GET_SP(_thread)) = (long) sp;   				\
    (_MD_GET_PC(_thread)) = (int) _main;   				\
	_thread->no_sched = 0;                              \
    PR_END_MACRO
#endif

/*
** Switch away from the current thread context by saving its state and
** calling the thread scheduler. Reload cpu when we come back from the
** context switch because it might have changed.
*/
/*    	ResetTimer();	before _PR_Schedule()		   		*/


#define _MD_SWITCH_CONTEXT(_thread)                                 \
    PR_BEGIN_MACRO                                                  \
    PR_ASSERT(_thread->no_sched);                                   \
    if (!setjmp(_thread->md.jb)) {                                  \
        _MD_SET_LAST_THREAD(_thread);                               \
        if (_PR_MD_CURRENT_CPU()->md.trackScheduling)               \
            _PR_MD_CURRENT_CPU()->md.lastThreadSwitch = UpTime();   \
        _PR_Schedule();                                             \
    } else {                                                        \
        PR_ASSERT(_MD_LAST_THREAD() !=_MD_CURRENT_THREAD());        \
        _MD_LAST_THREAD()->no_sched = 0;                            \
    }                                                               \
    PR_END_MACRO

/*
** Restore a thread context that was saved by _MD_SWITCH_CONTEXT or
** initialized by _MD_INIT_CONTEXT.
*/
#define _MD_RESTORE_CONTEXT(_newThread)	    \
    PR_BEGIN_MACRO			    			\
	long **jb = (_newThread)->md.jb;	    \
	_MD_SET_CURRENT_THREAD(_newThread); 	\
    _newThread->no_sched = 1;		        \
	longjmp(jb, 1);			    			\
    PR_END_MACRO


#define _MD_ERRNO()	 _MD_CURRENT_THREAD()->md.osErrCode

extern PRStatus _MD_wait(PRThread *thread, PRIntervalTime timeout);
#define _MD_WAIT	_MD_wait

/*
** Combined thread model related definitions
*/

#define _MD_CREATE_THREAD(a,b,c,d,e,f) (PR_SUCCESS)
#define _MD_WAKEUP_WAITER(a)
#define _MD_SET_PRIORITY(a,b)

/*
** File I/O Related definitions
*/

extern PRInt32 _PR_MD_WRITE_SYNC(PRFileDesc *fd, void *buf, PRInt32 amount);
#define    _PR_MD_WRITE_SYNC _MD_WRITE_SYNC

struct _MDDir {
	short		ioVRefNum;
	long		ioDirID;
	short		ioFDirIndex;
	char		*currentEntryName;
};

#define PR_DIRECTORY_SEPARATOR		'/'
#define PR_DIRECTORY_SEPARATOR_STR	"/"
#define PR_PATH_SEPARATOR		':'
#define PR_PATH_SEPARATOR_STR		":"

typedef enum IOOperation {
    READ_ASYNC,
    WRITE_ASYNC
} IOOperation;


#define _MD_INIT_IO()

#define _MD_OPEN 					_MD_Open
#define _MD_OPEN_FILE 				_MD_Open
#define _MD_CLOSE_FILE 				FSClose
#define _MD_READ(fd,buf,amount) 	ReadWriteProc(fd,buf,amount,READ_ASYNC)
#define _MD_WRITE(fd,buf,amount) 	ReadWriteProc(fd,buf,amount,WRITE_ASYNC)
#define _MD_WRITE_SYNC(fd,buf,amount) WriteSyncProc(fd,buf,amount)
#define _MD_GET_FILE_ERROR() 		_PR_MD_CURRENT_THREAD()->md.osErrCode
#define _MD_LSEEK 					_MD_LSeek
#define _MD_FSYNC 					_MD_FSync

/* to be implemented */
#define _MD_LSEEK64(a,b,c)           LL_ZERO
#define _MD_GETOPENFILEINFO64(fd,info)   -1
#define _MD_GETFILEINFO64(fd,info)   -1

#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

/*
** File Manipulation definitions
*/

#define	_MD_RENAME 			_MD_Rename
#define	_MD_ACCESS 			_MD_Access

#define	_MD_GETFILEINFO 	_MD_GetFileInfo
#define	_MD_GETOPENFILEINFO _MD_GetOpenFileInfo

#define	_MD_STAT 			_MD_Stat

#define	_MD_DELETE 			_MD_Delete

extern PRStatus _MD_LockFile(PRInt32 osfd);
#define	_MD_LOCKFILE 		_MD_LockFile
extern PRStatus _MD_TLockFile(PRInt32 osfd);
#define	_MD_TLOCKFILE 		_MD_TLockFile
extern PRStatus _MD_UnlockFile(PRInt32 osfd);
#define	_MD_UNLOCKFILE		_MD_UnlockFile

/* 
** Directory enumeration related definitions 
*/

extern PRStatus _MD_OpenDir(struct _MDDir *md,const char *name);
#define	_MD_OPEN_DIR 		_MD_OpenDir

extern char* _MD_ReadDir(struct _MDDir *md,PRIntn flags);
#define	_MD_READ_DIR 		_MD_ReadDir

#define	_MD_CLOSE_DIR 		_MD_CloseDir

#define	_MD_MKDIR 			_MD_MkDir
#define	_MD_MAKE_DIR		_MD_MkDir
#define	_MD_RMDIR 			_MD_Delete

/*
** Pipe I/O Related definitions (not implemented)
*/

#define _MD_PIPEAVAILABLE(fd) -1

/*
** Socket I/O Related definitions
*/

#if UNIVERSAL_INTERFACES_VERSION >= 0x0330
/* In Universal Interfaces 3.3 and later, these are enums. */
#define IP_TTL IP_TTL
#define IP_TOS IP_TOS
#define IP_ADD_MEMBERSHIP IP_ADD_MEMBERSHIP
#define IP_DROP_MEMBERSHIP IP_DROP_MEMBERSHIP
#define IP_MULTICAST_IF IP_MULTICAST_IF
#define IP_MULTICAST_TTL IP_MULTICAST_TTL
#define IP_MULTICAST_LOOP IP_MULTICAST_LOOP
#define TCP_NODELAY TCP_NODELAY
#define TCP_MAXSEG TCP_MAXSEG
#endif

#define _MD_SOCKET 			_MD_socket
#define _MD_BIND			_MD_bind
#define _MD_LISTEN			_MD_listen
#define _MD_GETSOCKNAME		_MD_getsockname

extern PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen);
#define _MD_GETSOCKOPT		_MD_getsockopt

extern PRStatus _MD_setsockopt(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen);
#define _MD_SETSOCKOPT		_MD_setsockopt

#define _MD_SOCKETAVAILABLE	_MD_socketavailable
#define _MD_ACCEPT			_MD_accept
#define _MD_CONNECT			_MD_connect
#define _MD_SEND			_MD_send
#define _MD_RECV			_MD_recv
#define _MD_CLOSE_SOCKET	_MD_closesocket
#define _MD_SENDTO			_MD_sendto
#define _MD_RECVFROM		_MD_recvfrom
#define _MD_PR_POLL			_MD_poll
#define _MD_INIT_FILEDESC	_MD_initfiledesc
#define _MD_FREE_FILEDESC	_MD_freefiledesc
#define _MD_MAKE_NONBLOCK	_MD_makenonblock
#define _MD_INIT_FD_INHERITABLE _MD_initfdinheritable
#define _MD_QUERY_FD_INHERITABLE _MD_queryfdinheritable

#define _MD_GET_SOCKET_ERROR() 		_PR_MD_CURRENT_THREAD()->md.osErrCode

#define _PR_MD_MAP_SELECT_ERROR(x) 	(x)
/*
** Netdb Related definitions
*/
extern PRStatus _MD_gethostname(char *name, int namelen);
#define _MD_GETHOSTNAME		_MD_gethostname
#define _PR_GET_HOST_ADDR_AS_NAME

/* 
	XXX _MD_WRITEV, _MD_SHUTDOWN & _MD_GETPEERNAME not done yet!!!
*/
#define _MD_WRITEV			_MD_writev
#define _MD_SHUTDOWN		_MD_shutdown
#define _MD_GETPEERNAME		_MD_getpeername


#ifdef OLD_MACSOCK_LIBRARY
#define _MD_SOCKET 			macsock_socket
#define _MD_LISTEN			macsock_listen
#define _MD_SEND(fd,buf,amount,flags,timeout)	macsock_send(fd->secret->md.osfd,buf,amount,flags)
#define _MD_SENDTO(fd,buf,amount,flags,addr,addrlen,timeout)	macsock_sendto(fd->secret->md.osfd,buf,amount,flags,(struct sockaddr *)addr,addrlen)
#define _MD_RECV(fd,buf,amount,flags,timeout)	macsock_recv(fd->secret->md.osfd,buf,amount,flags)
#define _MD_RECVFROM(fd,buf,amount,flags,addr,addrlen,timeout)	macsock_recvfrom(fd->secret->md.osfd,buf,amount,flags,(struct sockaddr *)addr,addrlen)
#define _MD_CLOSE_SOCKET	macsock_close
#define _MD_SHUTDOWN(a,b)	(0)

#define _MD_ACCEPT(fd,addr,addrlen,timeout)	macsock_accept(fd->secret->md.osfd,(struct sockaddr *)addr,addrlen)
#define _MD_CONNECT(fd,name,namelen,timeout)	macsock_connect(fd->secret->md.osfd,(struct sockaddr *)name,namelen)
#define _MD_BIND(fd,name,namelen)				macsock_bind(fd->secret->md.osfd,(struct sockaddr *)name,namelen)
#define _MD_GETSOCKNAME(fd,name,namelen)		macsock_getsockname(fd->secret->md.osfd,(struct sockaddr *)name,namelen)
#define _MD_GETPEERNAME(fd,name,namelen)		macsock_getpeername(fd->secret->md.osfd,(struct sockaddr *)name,namelen)
#define _MD_GETSOCKOPT(fd,level,optname,optval,optlen)		macsock_getsockopt(fd->secret->md.osfd,level,optname,optval,optlen)
#define _MD_SETSOCKOPT(fd,level,optname,optval,optlen)		macsock_setsockopt(fd->secret->md.osfd,level,optname,optval,optlen)
#define _MD_SOCKETAVAILABLE(fd,bytes)		macsock_socketavailable(fd->secret->md.osfd,bytes)
#endif

/*
** Memory Segements Related definitions
*/

#define _MD_INIT_SEGS()		
#define _MD_ALLOC_SEGMENT	_MD_AllocSegment
#define _MD_FREE_SEGMENT	_MD_FreeSegment

/*
** Time Related definitions
*/

#define _MD_GET_INTERVAL 				_MD_GetInterval
#define _MD_INTERVAL_PER_SEC() 			PR_MSEC_PER_SEC
#define _MD_INTERVAL_INIT()

/*
** Environemnt Related definitions
*/

extern char *_MD_GetEnv(const char *name);
#define _MD_GET_ENV			_MD_GetEnv

extern int _MD_PutEnv(const char *variableCopy);
#define _MD_PUT_ENV			_MD_PutEnv

/*
** Following is old stuff to be looked at.
*/

#define GCPTR
#define CALLBACK
typedef int (*FARPROC)();


#define MAX_NON_PRIMARY_TIME_SLICES 	6

extern long gTimeSlicesOnNonPrimaryThread;
extern struct PRThread *gPrimaryThread;

// Errors not found in the Mac StdCLib
#define EACCES  		13      	// Permission denied
#define ENOENT			-43			// No such file or directory
#define _OS_INVALID_FD_VALUE -1

#define	STDERR_FILENO	2

#if !defined(MAC_NSPR_STANDALONE)
#define PATH_SEPARATOR 					':'
#define PATH_SEPARATOR_STR		        ":"
#define DIRECTORY_SEPARATOR				'/'
#define DIRECTORY_SEPARATOR_STR			"/"
#endif

#define UNIX_THIS_DIRECTORY_STR			"./"
#define UNIX_PARENT_DIRECTORY_STR		"../"


// Alias a few names
#define getenv	PR_GetEnv
#define putenv	_MD_PutEnv

#if defined(MAC_NSPR_STANDALONE)
typedef unsigned char (*MemoryCacheFlusherProc)(size_t size);
typedef void (*PreAllocationHookProc)(void);

extern char *strdup(const char *source);

extern void InstallPreAllocationHook(PreAllocationHookProc newHook);
extern void InstallMemoryCacheFlusher(MemoryCacheFlusherProc newFlusher);
#endif

extern char *PR_GetDLLSearchPath(void);

#if defined(MAC_NSPR_STANDALONE)
extern int strcmp(const char *str1, const char *str2);
extern int strcasecmp(const char *str1, const char *str2);
#endif

extern void MapFullToPartialMacFile(char *);
extern char *MapPartialToFullMacFile(const char *);

extern void ResetTimer(void);
extern void PR_PeriodicIdle(void);
extern void ActivateTimer(void);
extern void DeactivateTimer(void);
extern void PR_InitMemory(void);

extern struct hostent *gethostbyaddr(const void *addr, int addrlen, int type);

extern short GetVolumeRefNumFromName(const char *);

#include <stdio.h>			// Needed to get FILE typedef
extern FILE *_OS_FOPEN(const char *filename, const char *mode);
//
//	Macintosh only private parts.
//

#define	dprintTrace			";dprintf;doTrace"
#define	dprintNoTrace		";dprintf"
extern void dprintf(const char *format, ...);


// Entry into the memory system's cache flushing
#if defined(MAC_NSPR_STANDALONE)
extern PRUint8 CallCacheFlushers(size_t blockSize);
#endif

#if defined(MAC_NSPR_STANDALONE)
extern void* reallocSmaller(void* block, size_t newSize);
#endif


/*
** PR_GetSystemInfo related definitions
*/
#define _PR_SI_SYSNAME          "MacOS"
#define _PR_SI_ARCHITECTURE     "PowerPC"

/*
 * Memory-mapped files
 */

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

extern void SetLogFileTypeCreator(const char *logFile);
extern int _MD_mac_get_nonblocking_connect_error(PRFileDesc* fd);


/*
 * Critical section support
 */

#define MAC_CRITICAL_REGIONS  TARGET_CARBON

#if MAC_CRITICAL_REGIONS

extern void InitCriticalRegion();
extern void TermCriticalRegion();

extern void EnterCritialRegion();
extern void LeaveCritialRegion();

#define INIT_CRITICAL_REGION()     InitCriticalRegion()
#define TERM_CRITICAL_REGION()     TermCriticalRegion()

#define ENTER_CRITICAL_REGION()     EnterCritialRegion()
#define LEAVE_CRITICAL_REGION()     LeaveCritialRegion()

#else

#define INIT_CRITICAL_REGION()
#define TERM_CRITICAL_REGION()

#define ENTER_CRITICAL_REGION()
#define LEAVE_CRITICAL_REGION()

#endif



/*
 * CPU Idle support
 */

extern void InitIdleSemaphore();
extern void TermIdleSemaphore();

extern void WaitOnIdleSemaphore();
extern void SignalIdleSemaphore();


/*
 * Atomic operations
 */
#ifdef _PR_HAVE_ATOMIC_OPS

extern PRInt32 _MD_AtomicSet(PRInt32 *val, PRInt32 newval);

#define _MD_INIT_ATOMIC()
#define _MD_ATOMIC_INCREMENT(val)   OTAtomicAdd32(1, (SInt32 *)val)
#define _MD_ATOMIC_ADD(ptr, val)    OTAtomicAdd32(val, (SInt32 *)ptr)
#define _MD_ATOMIC_DECREMENT(val)   OTAtomicAdd32(-1, (SInt32 *)val)
#define _MD_ATOMIC_SET(val, newval) _MD_AtomicSet(val, newval)

#endif /* _PR_HAVE_ATOMIC_OPS */


#endif /* prmacos_h___ */
