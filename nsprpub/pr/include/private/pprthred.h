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

#ifndef pprthred_h___
#define pprthred_h___

/*
** API for PR private functions.  These calls are to be used by internal
** developers only.
*/
#include "nspr.h"


PR_BEGIN_EXTERN_C

/*---------------------------------------------------------------------------
** THREAD PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

/*
** Add an exit func to be evaluated when the thread exits. Each thread
** can have zero or more of these.
*/
typedef void (*PRThreadExit)(void *arg);
PR_EXTERN(PRStatus) PR_SetThreadExit(PRUintn index, PRThreadExit func, void *arg);

/*
** Return the "index"'th at-exit function associated with thread
** "t". Returns NULL if the index does not reference a valid at-exit
** function.
*/
PR_EXTERN(PRThreadExit) PR_GetThreadExit(PRUintn index, void **argp);

/*
** Associate a thread object with an existing native thread.
** 	"type" is the type of thread object to attach
** 	"priority" is the priority to assign to the thread
** 	"stack" defines the shape of the threads stack
**
** This can return NULL if some kind of error occurs, or if memory is
** tight. This call invokes "start(obj,arg)" and returns when the
** function returns. The thread object is automatically destroyed.
**
** This call is not normally needed unless you create your own native
** thread. PR_Init does this automatically for the primordial thread.
*/
PR_EXTERN(PRThread*) PR_AttachThread(PRThreadType type,
                                     PRThreadPriority priority,
				     PRThreadStack *stack);

/*
** Detach the nspr thread from the currently executing native thread.
** The thread object will be destroyed and all related data attached
** to it. The exit procs will be invoked.
**
** This call is not normally needed unless you create your own native
** thread. PR_Exit will automatially detach the nspr thread object
** created by PR_Init for the primordial thread.
**
** This call returns after the nspr thread object is destroyed.
*/
PR_EXTERN(void) PR_DetachThread(void);

/*
** Get the id of the named thread. Each thread is assigned a unique id
** when it is created or attached.
*/
PR_EXTERN(PRUint32) PR_GetThreadID(PRThread *thread);

/*
** Set the procedure that is called when a thread is dumped. The procedure
** will be applied to the argument, arg, when called. Setting the procedure
** to NULL effectively removes it.
*/
typedef void (*PRThreadDumpProc)(PRFileDesc *fd, PRThread *t, void *arg);
PR_EXTERN(void) PR_SetThreadDumpProc(
    PRThread* thread, PRThreadDumpProc dump, void *arg);

/*
** Get this thread's affinity mask.  The affinity mask is a 32 bit quantity
** marking a bit for each processor this process is allowed to run on.
** The processor mask is returned in the mask argument.
** The least-significant-bit represents processor 0.
**
** Returns 0 on success, -1 on failure.
*/
PR_EXTERN(PRInt32) PR_GetThreadAffinityMask(PRThread *thread, PRUint32 *mask);

/*
** Set this thread's affinity mask.  
**
** Returns 0 on success, -1 on failure.
*/
PR_EXTERN(PRInt32) PR_SetThreadAffinityMask(PRThread *thread, PRUint32 mask );

/*
** Set the default CPU Affinity mask.
**
*/
PR_EXTERN(PRInt32) PR_SetCPUAffinityMask(PRUint32 mask);

/*
** Show status of all threads to standard error output.
*/
PR_EXTERN(void) PR_ShowStatus(void);

/*
** Set thread recycle mode to on (1) or off (0)
*/
PR_EXTERN(void) PR_SetThreadRecycleMode(PRUint32 flag);


/*---------------------------------------------------------------------------
** THREAD PRIVATE FUNCTIONS FOR GARBAGE COLLECTIBLE THREADS           
---------------------------------------------------------------------------*/

/* 
** Only Garbage collectible threads participate in resume all, suspend all and 
** enumeration operations.  They are also different during creation when
** platform specific action may be needed (For example, all Solaris GC able
** threads are bound threads).
*/

/*
** Same as PR_CreateThread except that the thread is marked as garbage
** collectible.
*/
PR_EXTERN(PRThread*) PR_CreateThreadGCAble(PRThreadType type,
				     void (*start)(void *arg),
				     void *arg,
				     PRThreadPriority priority,
				     PRThreadScope scope,
				     PRThreadState state,
				     PRUint32 stackSize);

/*
** Same as PR_AttachThread except that the thread being attached is marked as 
** garbage collectible.
*/
PR_EXTERN(PRThread*) PR_AttachThreadGCAble(PRThreadType type,
					PRThreadPriority priority,
					PRThreadStack *stack);

/*
** Mark the thread as garbage collectible.
*/
PR_EXTERN(void) PR_SetThreadGCAble(void);

/*
** Unmark the thread as garbage collectible.
*/
PR_EXTERN(void) PR_ClearThreadGCAble(void);

/*
** This routine prevents all other GC able threads from running. This call is needed by 
** the garbage collector.
*/
PR_EXTERN(void) PR_SuspendAll(void);

/*
** This routine unblocks all other GC able threads that were suspended from running by 
** PR_SuspendAll(). This call is needed by the garbage collector.
*/
PR_EXTERN(void) PR_ResumeAll(void);

/*
** Return the thread stack pointer of the given thread. 
** Needed by the garbage collector.
*/
PR_EXTERN(void *) PR_GetSP(PRThread *thread);

/*
** Save the registers that the GC would find interesting into the thread
** "t". isCurrent will be non-zero if the thread state that is being
** saved is the currently executing thread. Return the address of the
** first register to be scanned as well as the number of registers to
** scan in "np".
**
** If "isCurrent" is non-zero then it is allowed for the thread context
** area to be used as scratch storage to hold just the registers
** necessary for scanning.
**
** This function simply calls the internal function _MD_HomeGCRegisters().
*/
PR_EXTERN(PRWord *) PR_GetGCRegisters(PRThread *t, int isCurrent, int *np);

/*
** (Get|Set)ExecutionEnvironent
**
** Used by Java to associate it's execution environment so garbage collector
** can find it. If return is NULL, then it's probably not a collectable thread.
**
** There's no locking required around these calls.
*/
PR_EXTERN(void*) GetExecutionEnvironment(PRThread *thread);
PR_EXTERN(void) SetExecutionEnvironment(PRThread* thread, void *environment);

/*
** Enumeration function that applies "func(thread,i,arg)" to each active
** thread in the process. The enumerator returns PR_SUCCESS if the enumeration
** should continue, any other value is considered failure, and enumeration
** stops, returning the failure value from PR_EnumerateThreads.
** Needed by the garbage collector.
*/
typedef PRStatus (PR_CALLBACK *PREnumerator)(PRThread *t, int i, void *arg);
PR_EXTERN(PRStatus) PR_EnumerateThreads(PREnumerator func, void *arg);

/* 
** Signature of a thread stack scanning function. It is applied to every
** contiguous group of potential pointers within a thread. Count denotes the
** number of pointers. 
*/
typedef PRStatus 
(PR_CALLBACK *PRScanStackFun)(PRThread* t,
			      void** baseAddr, PRUword count, void* closure);

/*
** Applies scanFun to all contiguous groups of potential pointers 
** within a thread. This includes the stack, registers, and thread-local
** data. If scanFun returns a status value other than PR_SUCCESS the scan
** is aborted, and the status value is returned. 
*/
PR_EXTERN(PRStatus)
PR_ThreadScanStackPointers(PRThread* t,
                           PRScanStackFun scanFun, void* scanClosure);

/* 
** Calls PR_ThreadScanStackPointers for every thread.
*/
PR_EXTERN(PRStatus)
PR_ScanStackPointers(PRScanStackFun scanFun, void* scanClosure);

/*
** Returns a conservative estimate on the amount of stack space left
** on a thread in bytes, sufficient for making decisions about whether 
** to continue recursing or not.
*/
PR_EXTERN(PRUword)
PR_GetStackSpaceLeft(PRThread* t);

/*---------------------------------------------------------------------------
** THREAD CPU PRIVATE FUNCTIONS             
---------------------------------------------------------------------------*/

/*
** Get a pointer to the primordial CPU.
*/
PR_EXTERN(struct _PRCPU *) _PR_GetPrimordialCPU(void);

/*---------------------------------------------------------------------------
** THREAD SYNCHRONIZATION PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

/*
** Create a new named monitor (named for debugging purposes).
**  Monitors are re-entrant locks with a built-in condition variable.
**
** This may fail if memory is tight or if some operating system resource
** is low.
*/
PR_EXTERN(PRMonitor*) PR_NewNamedMonitor(const char* name);

/*
** Test and then lock the lock if it's not already locked by some other
** thread. Return PR_FALSE if some other thread owned the lock at the
** time of the call.
*/
PR_EXTERN(PRBool) PR_TestAndLock(PRLock *lock);

/*
** Test and then enter the mutex associated with the monitor if it's not
** already entered by some other thread. Return PR_FALSE if some other
** thread owned the mutex at the time of the call.
*/
PR_EXTERN(PRBool) PR_TestAndEnterMonitor(PRMonitor *mon);

/*
** Return the number of times that the current thread has entered the
** mutex. Returns zero if the current thread has not entered the mutex.
*/
PR_EXTERN(PRIntn) PR_GetMonitorEntryCount(PRMonitor *mon);

/*
** Just like PR_CEnterMonitor except that if the monitor is owned by
** another thread NULL is returned.
*/
PR_EXTERN(PRMonitor*) PR_CTestAndEnterMonitor(void *address);


/*---------------------------------------------------------------------------
** PLATFORM-SPECIFIC INITIALIZATION FUNCTIONS
---------------------------------------------------------------------------*/

#if defined(IRIX)
/*
** Irix specific initialization funtion to be called before PR_Init
** is called by the application. Sets the CONF_INITUSERS and CONF_INITSIZE
** attributes of the shared arena set up by nspr.
**
** The environment variables _NSPR_IRIX_INITUSERS and _NSPR_IRIX_INITSIZE
** can also be used to set these arena attributes. If _NSPR_IRIX_INITUSERS
** is set, but not _NSPR_IRIX_INITSIZE, the value of the CONF_INITSIZE
** attribute of the nspr arena is scaled as a function of the
** _NSPR_IRIX_INITUSERS value.
** 
** If the _PR_Irix_Set_Arena_Params() is called in addition to setting the
** environment variables, the values of the environment variables are used.
** 
*/
PR_EXTERN(void) _PR_Irix_Set_Arena_Params(PRInt32 initusers, PRInt32 initsize);

#endif /* IRIX */

/* I think PR_GetMonitorEntryCount is useless. All you really want is this... */
#define PR_InMonitor(m)		(PR_GetMonitorEntryCount(m) > 0)

/*---------------------------------------------------------------------------
** Special X-Lock hack for client
---------------------------------------------------------------------------*/

#ifdef XP_UNIX
extern void PR_XLock(void);
extern void PR_XUnlock(void);
extern PRBool PR_XIsLocked(void);
#endif /* XP_UNIX */

PR_END_EXTERN_C

#endif /* pprthred_h___ */
