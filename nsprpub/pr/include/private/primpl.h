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

#ifndef primpl_h___
#define primpl_h___

/*
 * HP-UX 10.10's pthread.h (DCE threads) includes dce/cma.h, which
 * has:
 *     #define sigaction _sigaction_sys
 * This macro causes chaos if signal.h gets included before pthread.h.
 * To be safe, we include pthread.h first.
 */

#if defined(_PR_PTHREADS)
#include <pthread.h>
#endif

#if defined(_PR_BTHREADS)
#include <kernel/OS.h>
#endif

#ifdef WINNT
/* Need to force service-pack 3 extensions to be defined by
** setting _WIN32_WINNT to NT 4.0 for winsock.h, winbase.h, winnt.h.
*/
#ifndef  _WIN32_WINNT
    #define _WIN32_WINNT 0x0400
#elif   (_WIN32_WINNT < 0x0400)
    #undef  _WIN32_WINNT
    #define _WIN32_WINNT 0x0400
#endif /* _WIN32_WINNT */
#endif /* WINNT */

#include "nspr.h"
#include "prpriv.h"

typedef struct PRSegment PRSegment;

#ifdef XP_MAC
#include "prosdep.h"
#include "probslet.h"
#else
#include "md/prosdep.h"
#include "obsolete/probslet.h"
#endif  /* XP_MAC */

/*************************************************************************
*****  A Word about Model Dependent Function Naming Convention ***********
*************************************************************************/

/*
NSPR 2.0 must implement its function across a range of platforms 
including: MAC, Windows/16, Windows/95, Windows/NT, and several
variants of Unix. Each implementation shares common code as well 
as having platform dependent portions. This standard describes how
the model dependent portions are to be implemented.

In header file pr/include/primpl.h, each publicly declared 
platform dependent function is declared as:

PR_EXTERN void _PR_MD_FUNCTION( long arg1, long arg2 );
#define _PR_MD_FUNCTION _MD_FUNCTION

In header file pr/include/md/<platform>/_<platform>.h, 
each #define'd macro is redefined as one of:

#define _MD_FUNCTION <blanks>
#define _MD_FUNCTION <expanded macro>
#define _MD_FUNCTION <osFunction>
#define _MD_FUNCTION <_MD_Function>

Where:

<blanks> is no definition at all. In this case, the function is not implemented 
and is never called for this platform. 
For example: 
#define _MD_INIT_CPUS()

<expanded macro> is a C language macro expansion. 
For example: 
#define        _MD_CLEAN_THREAD(_thread) \
    PR_BEGIN_MACRO \
        PR_DestroyCondVar(_thread->md.asyncIOCVar); \
        PR_DestroyLock(_thread->md.asyncIOLock); \
    PR_END_MACRO

<osFunction> is some function implemented by the host operating system. 
For example: 
#define _MD_EXIT        exit

<_MD_function> is the name of a function implemented for this platform in 
pr/src/md/<platform>/<soruce>.c file. 
For example: 
#define        _MD_GETFILEINFO         _MD_GetFileInfo

In <source>.c, the implementation is:
PR_IMPLEMENT(PRInt32) _MD_GetFileInfo(const char *fn, PRFileInfo *info);
*/

PR_BEGIN_EXTERN_C

typedef struct _MDLock _MDLock;
typedef struct _MDCVar _MDCVar;
typedef struct _MDSegment _MDSegment;
typedef struct _MDThread _MDThread;
typedef struct _MDThreadStack _MDThreadStack;
typedef struct _MDSemaphore _MDSemaphore;
typedef struct _MDDir _MDDir;
typedef struct _MDFileDesc _MDFileDesc;
typedef struct _MDProcess _MDProcess;
typedef struct _MDFileMap _MDFileMap;

#if defined(_PR_PTHREADS)

/*
** The following definitions are unique to implementing NSPR using pthreads.
** Since pthreads defines most of the thread and thread synchronization
** stuff, this is a pretty small set.
*/

#define PT_CV_NOTIFIED_LENGTH 6
typedef struct _PT_Notified _PT_Notified;
struct _PT_Notified
{
    PRIntn length;              /* # of used entries in this structure */
    struct
    {
        PRCondVar *cv;          /* the condition variable notified */
        PRIntn times;           /* and the number of times notified */
    } cv[PT_CV_NOTIFIED_LENGTH];
    _PT_Notified *link;         /* link to another of these | NULL */
};

/*
 * bits defined for pthreads 'state' field 
 */
#define PT_THREAD_DETACHED  0x01    /* thread can't be joined */
#define PT_THREAD_GLOBAL    0x02    /* a global thread (unlikely) */
#define PT_THREAD_SYSTEM    0x04    /* system (not user) thread */
#define PT_THREAD_PRIMORD   0x08    /* this is the primordial thread */
#define PT_THREAD_ABORTED   0x10    /* thread has been interrupted */
#define PT_THREAD_GCABLE    0x20    /* thread is garbage collectible */
#define PT_THREAD_SUSPENDED 0x40    /* thread has been suspended */
#define PT_THREAD_FOREIGN   0x80    /* thread is not one of ours */
#define PT_THREAD_BOUND     0x100    /* a bound-global thread */

/* 
** Possible values for thread's suspend field
** Note that the first two can be the same as they are really mutually exclusive,
** i.e. both cannot be happening at the same time. We have two symbolic names
** just as a mnemonic.
**/
#define PT_THREAD_RESUMED   0x80    /* thread has been resumed */
#define PT_THREAD_SETGCABLE 0x100   /* set the GCAble flag */

#if defined(DEBUG)

typedef struct PTDebug
{
    PRTime timeStarted;
    PRUintn predictionsFoiled;
    PRUintn pollingListMax;
    PRUintn continuationsServed;
    PRUintn recyclesNeeded;
    PRUintn quiescentIO;
    PRUintn locks_created, locks_destroyed;
    PRUintn locks_acquired, locks_released;
    PRUintn cvars_created, cvars_destroyed;
    PRUintn cvars_notified, delayed_cv_deletes;
} PTDebug;

PR_EXTERN(void) PT_GetStats(PTDebug* here);
PR_EXTERN(void) PT_FPrintStats(PRFileDesc *fd, const char *msg);

#else

typedef PRUintn PTDebug;
#define PT_GetStats(_p)
#define PT_FPrintStats(_fd, _msg)

#endif /* defined(DEBUG) */

#else /* defined(_PR_PTHREADS) */

typedef PRUintn PTDebug;
#define PT_GetStats(_p)
#define PT_FPrintStats(_fd, _msg)

/*
** This section is contains those parts needed to implement NSPR on
** platforms in general. One would assume that the pthreads implementation
** included lots of the same types, at least conceptually.
*/

/*
 * Local threads only.  No multiple CPU support and hence all the
 * following routines are no-op.
 */
#ifdef _PR_LOCAL_THREADS_ONLY

#define        _PR_MD_SUSPEND_THREAD(thread)        
#define        _PR_MD_RESUME_THREAD(thread)        
#define        _PR_MD_SUSPEND_CPU(cpu)        
#define        _PR_MD_RESUME_CPU(cpu)        
#define        _PR_MD_BEGIN_SUSPEND_ALL()        
#define        _PR_MD_END_SUSPEND_ALL()        
#define        _PR_MD_BEGIN_RESUME_ALL()        
#define        _PR_MD_END_RESUME_ALL()
#define _PR_MD_INIT_ATTACHED_THREAD(thread) PR_FAILURE

#endif

typedef struct _PRCPUQueue _PRCPUQueue;
typedef struct _PRCPU _PRCPU;
typedef struct _MDCPU _MDCPU;

struct _PRCPUQueue {
    _MDLock  runQLock;          /* lock for the run + wait queues */
    _MDLock  sleepQLock;        /* lock for the run + wait queues */
    _MDLock  miscQLock;         /* lock for the run + wait queues */

    PRCList runQ[PR_PRIORITY_LAST + 1]; /* run queue for this CPU */
    PRUint32  runQReadyMask;
    PRCList sleepQ;
    PRIntervalTime sleepQmax;
    PRCList pauseQ;
    PRCList suspendQ;
    PRCList waitingToJoinQ;

    PRUintn   numCPUs;          /* number of CPUs using this Q */
};

struct _PRCPU {
    PRCList links;              /* link list of CPUs */
    PRUint32 id;                /* id for this CPU */

    union {
        PRInt32 bits;
        PRUint8 missed[4];
    } u;
    PRIntn where;               /* index into u.missed */
    PRPackedBool paused;        /* cpu is paused */
    PRPackedBool exit;          /* cpu should exit */

    PRThread *thread;           /* native thread for this CPUThread */
    PRThread *idle_thread;      /* user-level idle thread for this CPUThread */

    PRIntervalTime last_clock;  /* the last time we went into 
                                 * _PR_ClockInterrupt() on this CPU
                                 */

    _PRCPUQueue *queue;

    _MDCPU md;
};

typedef struct _PRInterruptTable {
    const char *name;
    PRUintn missed_bit;
    void (*handler)(void);
} _PRInterruptTable;

#define _PR_CPU_PTR(_qp) \
    ((_PRCPU*) ((char*) (_qp) - offsetof(_PRCPU,links)))

#if !defined(IRIX) && !defined(WIN32)
#define _MD_GET_ATTACHED_THREAD()        (_PR_MD_CURRENT_THREAD())
#endif

#ifdef _PR_LOCAL_THREADS_ONLY 

PR_EXTERN(struct _PRCPU *)              _pr_currentCPU;
PR_EXTERN(PRThread *)                   _pr_currentThread;
PR_EXTERN(PRThread *)                   _pr_lastThread;
PR_EXTERN(PRInt32)                      _pr_intsOff;

#define _MD_CURRENT_CPU()               (_pr_currentCPU)
#define _MD_SET_CURRENT_CPU(_cpu)       (_pr_currentCPU = (_cpu))
#define _MD_CURRENT_THREAD()            (_pr_currentThread)
#define _MD_SET_CURRENT_THREAD(_thread) (_pr_currentThread = (_thread))
#define _MD_LAST_THREAD()               (_pr_lastThread)
#define _MD_SET_LAST_THREAD(t)          (_pr_lastThread = t)

#define _MD_GET_INTSOFF()               (_pr_intsOff)
#define _MD_SET_INTSOFF(_val)           (_pr_intsOff = _val)


/* The unbalanced curly braces in these two macros are intentional */
#define _PR_LOCK_HEAP() { PRIntn _is; if (_pr_currentCPU) _PR_INTSOFF(_is);
#define _PR_UNLOCK_HEAP() if (_pr_currentCPU) _PR_INTSON(_is); }

#endif /* _PR_LOCAL_THREADS_ONLY */

#if defined(_PR_GLOBAL_THREADS_ONLY)

#define _MD_GET_INTSOFF() 0
#define _MD_SET_INTSOFF(_val)
#define _PR_INTSOFF(_is)
#define _PR_FAST_INTSON(_is)
#define _PR_INTSON(_is)
#define _PR_THREAD_LOCK(_thread)
#define _PR_THREAD_UNLOCK(_thread)
#define _PR_RUNQ_LOCK(cpu)
#define _PR_RUNQ_UNLOCK(cpu)
#define _PR_SLEEPQ_LOCK(thread)
#define _PR_SLEEPQ_UNLOCK(thread)
#define _PR_MISCQ_LOCK(thread)
#define _PR_MISCQ_UNLOCK(thread)
#define _PR_CPU_LIST_LOCK()
#define _PR_CPU_LIST_UNLOCK()

#define _PR_ADD_RUNQ(_thread, _cpu, _pri)
#define _PR_DEL_RUNQ(_thread)
#define _PR_ADD_SLEEPQ(_thread, _timeout)
#define _PR_DEL_SLEEPQ(_thread, _propogate)
#define _PR_ADD_JOINQ(_thread, _cpu)
#define _PR_DEL_JOINQ(_thread)
#define _PR_ADD_SUSPENDQ(_thread, _cpu)
#define _PR_DEL_SUSPENDQ(_thread)

#define _PR_THREAD_SWITCH_CPU(_thread, _newCPU)

#define _PR_IS_NATIVE_THREAD(thread) 1
#define _PR_IS_NATIVE_THREAD_SUPPORTED() 1

#else

#define _PR_INTSOFF(_is) \
    PR_BEGIN_MACRO \
        (_is) = _PR_MD_GET_INTSOFF(); \
        _PR_MD_SET_INTSOFF(1); \
    PR_END_MACRO

#define _PR_FAST_INTSON(_is) \
    PR_BEGIN_MACRO \
        _PR_MD_SET_INTSOFF(_is); \
    PR_END_MACRO

#define _PR_INTSON(_is) \
    PR_BEGIN_MACRO \
        if ((_is == 0) && (_PR_MD_CURRENT_CPU())->u.bits) \
                _PR_IntsOn((_PR_MD_CURRENT_CPU())); \
        _PR_MD_SET_INTSOFF(_is); \
    PR_END_MACRO

#ifdef _PR_LOCAL_THREADS_ONLY 

#define _PR_IS_NATIVE_THREAD(thread) 0
#define _PR_THREAD_LOCK(_thread)
#define _PR_THREAD_UNLOCK(_thread)
#define _PR_RUNQ_LOCK(cpu)
#define _PR_RUNQ_UNLOCK(cpu)
#define _PR_SLEEPQ_LOCK(thread)
#define _PR_SLEEPQ_UNLOCK(thread)
#define _PR_MISCQ_LOCK(thread)
#define _PR_MISCQ_UNLOCK(thread)
#define _PR_CPU_LIST_LOCK()
#define _PR_CPU_LIST_UNLOCK()

#define _PR_ADD_RUNQ(_thread, _cpu, _pri) \
    PR_BEGIN_MACRO \
    PR_APPEND_LINK(&(_thread)->links, &_PR_RUNQ(_cpu)[_pri]); \
    _PR_RUNQREADYMASK(_cpu) |= (1L << _pri); \
    PR_END_MACRO

#define _PR_DEL_RUNQ(_thread) \
    PR_BEGIN_MACRO \
    _PRCPU *_cpu = _thread->cpu; \
    PRInt32 _pri = _thread->priority; \
    PR_REMOVE_LINK(&(_thread)->links); \
    if (PR_CLIST_IS_EMPTY(&_PR_RUNQ(_cpu)[_pri])) \
        _PR_RUNQREADYMASK(_cpu) &= ~(1L << _pri); \
    PR_END_MACRO

#define _PR_ADD_SLEEPQ(_thread, _timeout) \
    _PR_AddSleepQ(_thread, _timeout);   

#define _PR_DEL_SLEEPQ(_thread, _propogate) \
    _PR_DelSleepQ(_thread, _propogate);  

#define _PR_ADD_JOINQ(_thread, _cpu) \
    PR_APPEND_LINK(&(_thread)->links, &_PR_WAITINGTOJOINQ(_cpu));

#define _PR_DEL_JOINQ(_thread) \
    PR_REMOVE_LINK(&(_thread)->links);

#define _PR_ADD_SUSPENDQ(_thread, _cpu) \
    PR_APPEND_LINK(&(_thread)->links, &_PR_SUSPENDQ(_cpu));

#define _PR_DEL_SUSPENDQ(_thread) \
    PR_REMOVE_LINK(&(_thread)->links);

#define _PR_THREAD_SWITCH_CPU(_thread, _newCPU)

#define _PR_IS_NATIVE_THREAD_SUPPORTED() 0

#else        /* _PR_LOCAL_THREADS_ONLY */

/* These are for the "combined" thread model */

#define _PR_THREAD_LOCK(_thread) \
    _PR_MD_LOCK(&(_thread)->threadLock);

#define _PR_THREAD_UNLOCK(_thread) \
    _PR_MD_UNLOCK(&(_thread)->threadLock);

#define _PR_RUNQ_LOCK(_cpu) \
    PR_BEGIN_MACRO \
    _PR_MD_LOCK(&(_cpu)->queue->runQLock );\
    PR_END_MACRO
    
#define _PR_RUNQ_UNLOCK(_cpu) \
    PR_BEGIN_MACRO \
    _PR_MD_UNLOCK(&(_cpu)->queue->runQLock );\
    PR_END_MACRO

#define _PR_SLEEPQ_LOCK(_cpu) \
    _PR_MD_LOCK(&(_cpu)->queue->sleepQLock );

#define _PR_SLEEPQ_UNLOCK(_cpu) \
    _PR_MD_UNLOCK(&(_cpu)->queue->sleepQLock );

#define _PR_MISCQ_LOCK(_cpu) \
    _PR_MD_LOCK(&(_cpu)->queue->miscQLock );

#define _PR_MISCQ_UNLOCK(_cpu) \
    _PR_MD_UNLOCK(&(_cpu)->queue->miscQLock );

#define _PR_CPU_LIST_LOCK()                 _PR_MD_LOCK(&_pr_cpuLock)
#define _PR_CPU_LIST_UNLOCK()               _PR_MD_UNLOCK(&_pr_cpuLock)

#define QUEUE_RUN           0x1
#define QUEUE_SLEEP         0x2
#define QUEUE_JOIN          0x4
#define QUEUE_SUSPEND       0x8
#define QUEUE_LOCK          0x10

#define _PR_ADD_RUNQ(_thread, _cpu, _pri) \
    PR_BEGIN_MACRO \
    PR_APPEND_LINK(&(_thread)->links, &_PR_RUNQ(_cpu)[_pri]); \
    _PR_RUNQREADYMASK(_cpu) |= (1L << _pri); \
    PR_ASSERT((_thread)->queueCount == 0); \
    (_thread)->queueCount = QUEUE_RUN; \
    PR_END_MACRO

#define _PR_DEL_RUNQ(_thread) \
    PR_BEGIN_MACRO \
    _PRCPU *_cpu = _thread->cpu; \
    PRInt32 _pri = _thread->priority; \
    PR_REMOVE_LINK(&(_thread)->links); \
    if (PR_CLIST_IS_EMPTY(&_PR_RUNQ(_cpu)[_pri])) \
        _PR_RUNQREADYMASK(_cpu) &= ~(1L << _pri); \
    PR_ASSERT((_thread)->queueCount == QUEUE_RUN);\
    (_thread)->queueCount = 0; \
    PR_END_MACRO

#define _PR_ADD_SLEEPQ(_thread, _timeout) \
    PR_ASSERT((_thread)->queueCount == 0); \
    (_thread)->queueCount = QUEUE_SLEEP; \
    _PR_AddSleepQ(_thread, _timeout);  

#define _PR_DEL_SLEEPQ(_thread, _propogate) \
    PR_ASSERT((_thread)->queueCount == QUEUE_SLEEP);\
    (_thread)->queueCount = 0; \
    _PR_DelSleepQ(_thread, _propogate);  

#define _PR_ADD_JOINQ(_thread, _cpu) \
    PR_ASSERT((_thread)->queueCount == 0); \
    (_thread)->queueCount = QUEUE_JOIN; \
    PR_APPEND_LINK(&(_thread)->links, &_PR_WAITINGTOJOINQ(_cpu));

#define _PR_DEL_JOINQ(_thread) \
    PR_ASSERT((_thread)->queueCount == QUEUE_JOIN);\
    (_thread)->queueCount = 0; \
    PR_REMOVE_LINK(&(_thread)->links);

#define _PR_ADD_SUSPENDQ(_thread, _cpu) \
    PR_ASSERT((_thread)->queueCount == 0); \
    (_thread)->queueCount = QUEUE_SUSPEND; \
    PR_APPEND_LINK(&(_thread)->links, &_PR_SUSPENDQ(_cpu));

#define _PR_DEL_SUSPENDQ(_thread) \
    PR_ASSERT((_thread)->queueCount == QUEUE_SUSPEND);\
    (_thread)->queueCount = 0; \
    PR_REMOVE_LINK(&(_thread)->links);

#define _PR_THREAD_SWITCH_CPU(_thread, _newCPU) \
    (_thread)->cpu = (_newCPU);

#define _PR_IS_NATIVE_THREAD(thread) (thread->flags & _PR_GLOBAL_SCOPE)
#define _PR_IS_NATIVE_THREAD_SUPPORTED() 1

#endif /* _PR_LOCAL_THREADS_ONLY */

#endif /* _PR_GLOBAL_THREADS_ONLY */

#define _PR_SET_RESCHED_FLAG() _PR_MD_CURRENT_CPU()->u.missed[3] = 1
#define _PR_CLEAR_RESCHED_FLAG() _PR_MD_CURRENT_CPU()->u.missed[3] = 0

extern _PRInterruptTable _pr_interruptTable[];

/* Bits for _pr_interruptState.u.missed[0,1] */
#define _PR_MISSED_CLOCK    0x1
#define _PR_MISSED_IO        0x2
#define _PR_MISSED_CHILD    0x4

extern void _PR_IntsOn(_PRCPU *cpu);

PR_EXTERN(void) _PR_WakeupCPU(void);
PR_EXTERN(void) _PR_PauseCPU(void);

/************************************************************************/

#define _PR_LOCK_LOCK(_lock) \
    _PR_MD_LOCK(&(_lock)->ilock);
#define _PR_LOCK_UNLOCK(_lock) \
    _PR_MD_UNLOCK(&(_lock)->ilock);
    
extern PRThread * _PR_AssignLock(PRLock *lock);

#define _PR_LOCK_PTR(_qp) \
    ((PRLock*) ((char*) (_qp) - offsetof(PRLock,links)))

/************************************************************************/

#define _PR_CVAR_LOCK(_cvar) \
    _PR_MD_LOCK(&(_cvar)->ilock); 
#define _PR_CVAR_UNLOCK(_cvar) \
    _PR_MD_UNLOCK(&(_cvar)->ilock);

extern PRStatus _PR_WaitCondVar(
    PRThread *thread, PRCondVar *cvar, PRLock *lock, PRIntervalTime timeout);
extern PRUint32 _PR_CondVarToString(PRCondVar *cvar, char *buf, PRUint32 buflen);

PR_EXTERN(void) _PR_Notify(PRMonitor *mon, PRBool all, PRBool sticky);

typedef struct _PRPerThreadExit {
    PRThreadExit func;
    void *arg;
} _PRPerThreadExit;

/* PRThread.flags */
#define _PR_SYSTEM          0x01
#define _PR_INTERRUPT       0x02
#define _PR_ATTACHED        0x04        /* created via PR_AttachThread */
#define _PR_PRIMORDIAL      0x08        /* the thread that called PR_Init */
#define _PR_ON_SLEEPQ       0x10        /* thread is on the sleepQ */
#define _PR_ON_PAUSEQ       0x20        /* thread is on the pauseQ */
#define _PR_SUSPENDING      0x40        /* thread wants to suspend */
#define _PR_GLOBAL_SCOPE    0x80        /* thread is global scope */
#define _PR_IDLE_THREAD     0x200       /* this is an idle thread        */
#define _PR_GCABLE_THREAD   0x400       /* this is a collectable thread */
#define _PR_BOUND_THREAD    0x800       /* a bound thread */

/* PRThread.state */
#define _PR_UNBORN       0
#define _PR_RUNNABLE     1
#define _PR_RUNNING      2
#define _PR_LOCK_WAIT    3
#define _PR_COND_WAIT    4
#define _PR_JOIN_WAIT    5
#define _PR_IO_WAIT      6
#define _PR_SUSPENDED    7
#define _PR_DEAD_STATE   8  /* for debugging */

/* PRThreadStack.flags */
#define _PR_STACK_VM            0x1    /* using vm instead of malloc */
#define _PR_STACK_MAPPED        0x2    /* vm is mapped */
#define _PR_STACK_PRIMORDIAL    0x4    /* stack for primordial thread */

/* 
** If the default stcksize from the client is zero, we need to pick a machine
** dependent value.  This is only for standard user threads.  For custom threads,
** 0 has a special meaning.
** Adjust stackSize. Round up to a page boundary.
*/
#if (!defined(HAVE_CUSTOM_USER_THREADS))
#define        _PR_ADJUST_STACKSIZE(stackSize) \
        PR_BEGIN_MACRO \
    if (stackSize == 0) \
                stackSize = _MD_DEFAULT_STACK_SIZE; \
    stackSize = (stackSize + (1 << _pr_pageShift) - 1) >> _pr_pageShift; \
    stackSize <<= _pr_pageShift; \
        PR_END_MACRO
#else
#define        _PR_ADJUST_STACKSIZE(stackSize)
#endif

#define _PR_PENDING_INTERRUPT(_thread) ((_thread)->flags & _PR_INTERRUPT)

#define _PR_THREAD_PTR(_qp) \
    ((PRThread*) ((char*) (_qp) - offsetof(PRThread,links)))

#define _PR_ACTIVE_THREAD_PTR(_qp) \
    ((PRThread*) ((char*) (_qp) - offsetof(PRThread,active)))

#define _PR_THREAD_CONDQ_PTR(_qp) \
    ((PRThread*) ((char*) (_qp) - offsetof(PRThread,waitQLinks)))

#define _PR_THREAD_MD_TO_PTR(_md) \
    ((PRThread*) ((char*) (_md) - offsetof(PRThread,md)))

#define _PR_THREAD_STACK_TO_PTR(_stack) \
    ((PRThread*) (_stack->thr))

extern PRCList _pr_active_local_threadQ;
extern PRCList _pr_active_global_threadQ;
extern PRCList _pr_cpuQ;
extern _MDLock  _pr_cpuLock;
extern PRInt32 _pr_md_idle_cpus;

#define _PR_ACTIVE_LOCAL_THREADQ()          _pr_active_local_threadQ
#define _PR_ACTIVE_GLOBAL_THREADQ()         _pr_active_global_threadQ
#define _PR_CPUQ()                          _pr_cpuQ
#define _PR_RUNQ(_cpu)                      ((_cpu)->queue->runQ)
#define _PR_RUNQREADYMASK(_cpu)             ((_cpu)->queue->runQReadyMask)
#define _PR_SLEEPQ(_cpu)                    ((_cpu)->queue->sleepQ)
#define _PR_SLEEPQMAX(_cpu)                 ((_cpu)->queue->sleepQmax)
#define _PR_PAUSEQ(_cpu)                    ((_cpu)->queue->pauseQ)
#define _PR_SUSPENDQ(_cpu)                  ((_cpu)->queue->suspendQ)
#define _PR_WAITINGTOJOINQ(_cpu)            ((_cpu)->queue->waitingToJoinQ)

extern PRUint32 _pr_recycleThreads;   /* Flag for behavior on thread cleanup */
extern PRLock *_pr_deadQLock;
extern PRUint32 _pr_numNativeDead;
extern PRUint32 _pr_numUserDead;
extern PRCList _pr_deadNativeQ;
extern PRCList _pr_deadUserQ;
#define _PR_DEADNATIVEQ     _pr_deadNativeQ
#define _PR_DEADUSERQ       _pr_deadUserQ
#define _PR_DEADQ_LOCK      PR_Lock(_pr_deadQLock);
#define _PR_DEADQ_UNLOCK    PR_Unlock(_pr_deadQLock);
#define _PR_INC_DEADNATIVE  (_pr_numNativeDead++)
#define _PR_DEC_DEADNATIVE  (_pr_numNativeDead--)
#define _PR_NUM_DEADNATIVE  (_pr_numNativeDead)
#define _PR_INC_DEADUSER    (_pr_numUserDead++)
#define _PR_DEC_DEADUSER    (_pr_numUserDead--)
#define _PR_NUM_DEADUSER    (_pr_numUserDead)

extern PRUint32 _pr_utid;

extern struct _PRCPU  *_pr_primordialCPU;

extern PRLock *_pr_activeLock;          /* lock for userActive and systemActive */
extern PRInt32 _pr_userActive;          /* number of active user threads */
extern PRInt32 _pr_systemActive;        /* number of active system threads */
extern PRInt32 _pr_primordialExitCount; /* number of user threads left
                                         * before the primordial thread
                                         * can exit.  */
extern PRCondVar *_pr_primordialExitCVar; /* the condition variable for
                                           * notifying the primordial thread
                                           * when all other user threads
                                           * have terminated.  */

extern PRUintn _pr_maxPTDs;

extern PRLock *_pr_terminationCVLock;

/*************************************************************************
* Internal routines either called by PR itself or from machine-dependent *
* code.                                                                  *
*************************************************************************/

extern void _PR_ClockInterrupt(void);

extern void _PR_Schedule(void);
extern void _PR_SetThreadPriority(
    PRThread* thread, PRThreadPriority priority);
PR_EXTERN(void) _PR_Unlock(PRLock *lock);

PR_EXTERN(void) _PR_SuspendThread(PRThread *t);
PR_EXTERN(void) _PR_ResumeThread(PRThread *t);

extern PRThreadStack * _PR_NewStack(PRUint32 stackSize);
extern void _PR_FreeStack(PRThreadStack *stack);
extern PRBool _PR_NotifyThread (PRThread *thread, PRThread *me);
extern void _PR_NotifyLockedThread (PRThread *thread);

PR_EXTERN(void) _PR_AddSleepQ(PRThread *thread, PRIntervalTime timeout);
PR_EXTERN(void) _PR_DelSleepQ(PRThread *thread, PRBool propogate_time);

extern void _PR_AddThreadToRunQ(PRThread *me, PRThread *thread);

PR_EXTERN(PRThread*) _PR_CreateThread(PRThreadType type,
                                     void (*start)(void *arg),
                                     void *arg,
                                     PRThreadPriority priority,
                                     PRThreadScope scope,
                                     PRThreadState state,
                                     PRUint32 stackSize,
                     PRUint32 flags);

extern void _PR_NativeDestroyThread(PRThread *thread);
extern void _PR_UserDestroyThread(PRThread *thread);

extern PRThread* _PRI_AttachThread(
    PRThreadType type, PRThreadPriority priority,
    PRThreadStack *stack, PRUint32 flags);

extern void _PRI_DetachThread(void);


#define _PR_IO_PENDING(_thread) ((_thread)->io_pending)

PR_EXTERN(void) _PR_MD_INIT_CPUS();
#define    _PR_MD_INIT_CPUS _MD_INIT_CPUS

PR_EXTERN(void) _PR_MD_WAKEUP_CPUS();
#define    _PR_MD_WAKEUP_CPUS _MD_WAKEUP_CPUS

/* Interrupts related */

PR_EXTERN(void) _PR_MD_START_INTERRUPTS(void);
#define    _PR_MD_START_INTERRUPTS _MD_START_INTERRUPTS

PR_EXTERN(void) _PR_MD_STOP_INTERRUPTS(void);
#define    _PR_MD_STOP_INTERRUPTS _MD_STOP_INTERRUPTS

PR_EXTERN(void) _PR_MD_ENABLE_CLOCK_INTERRUPTS(void);
#define    _PR_MD_ENABLE_CLOCK_INTERRUPTS _MD_ENABLE_CLOCK_INTERRUPTS

PR_EXTERN(void) _PR_MD_DISABLE_CLOCK_INTERRUPTS(void);
#define    _PR_MD_DISABLE_CLOCK_INTERRUPTS _MD_DISABLE_CLOCK_INTERRUPTS

PR_EXTERN(void) _PR_MD_BLOCK_CLOCK_INTERRUPTS(void);
#define    _PR_MD_BLOCK_CLOCK_INTERRUPTS _MD_BLOCK_CLOCK_INTERRUPTS

PR_EXTERN(void) _PR_MD_UNBLOCK_CLOCK_INTERRUPTS(void);
#define    _PR_MD_UNBLOCK_CLOCK_INTERRUPTS _MD_UNBLOCK_CLOCK_INTERRUPTS

/* The _PR_MD_WAIT_LOCK and _PR_MD_WAKEUP_WAITER functions put to sleep and
 * awaken a thread which is waiting on a lock or cvar.
 */
extern PRStatus _PR_MD_WAIT(PRThread *, PRIntervalTime timeout);
#define    _PR_MD_WAIT _MD_WAIT

extern PRStatus _PR_MD_WAKEUP_WAITER(PRThread *);
#define    _PR_MD_WAKEUP_WAITER _MD_WAKEUP_WAITER

#ifndef _PR_LOCAL_THREADS_ONLY /* not if only local threads supported */
PR_EXTERN(void) _PR_MD_CLOCK_INTERRUPT(void);
#define    _PR_MD_CLOCK_INTERRUPT _MD_CLOCK_INTERRUPT
#endif

/* Stack debugging */
PR_EXTERN(void) _PR_MD_INIT_STACK(PRThreadStack *ts, PRIntn redzone);
#define    _PR_MD_INIT_STACK _MD_INIT_STACK

PR_EXTERN(void) _PR_MD_CLEAR_STACK(PRThreadStack* ts);
#define    _PR_MD_CLEAR_STACK _MD_CLEAR_STACK

/* CPU related */
PR_EXTERN(PRInt32) _PR_MD_GET_INTSOFF(void);
#define    _PR_MD_GET_INTSOFF _MD_GET_INTSOFF

PR_EXTERN(void) _PR_MD_SET_INTSOFF(PRInt32 _val);
#define    _PR_MD_SET_INTSOFF _MD_SET_INTSOFF

PR_EXTERN(_PRCPU*) _PR_MD_CURRENT_CPU(void);
#define    _PR_MD_CURRENT_CPU _MD_CURRENT_CPU

PR_EXTERN(void) _PR_MD_SET_CURRENT_CPU(_PRCPU *cpu);
#define    _PR_MD_SET_CURRENT_CPU _MD_SET_CURRENT_CPU

PR_EXTERN(void) _PR_MD_INIT_RUNNING_CPU(_PRCPU *cpu);
#define    _PR_MD_INIT_RUNNING_CPU _MD_INIT_RUNNING_CPU

/*
 * Returns the number of threads awoken or 0 if a timeout occurred;
 */
extern PRInt32 _PR_MD_PAUSE_CPU(PRIntervalTime timeout);
#define    _PR_MD_PAUSE_CPU _MD_PAUSE_CPU

extern void _PR_MD_CLEANUP_BEFORE_EXIT(void);
#define _PR_MD_CLEANUP_BEFORE_EXIT _MD_CLEANUP_BEFORE_EXIT

extern void _PR_MD_EXIT(PRIntn status);
#define    _PR_MD_EXIT _MD_EXIT

/* Locks related */

PR_EXTERN(void) _PR_MD_INIT_LOCKS(void);
#define    _PR_MD_INIT_LOCKS _MD_INIT_LOCKS

PR_EXTERN(PRStatus) _PR_MD_NEW_LOCK(_MDLock *md);
#define    _PR_MD_NEW_LOCK _MD_NEW_LOCK

PR_EXTERN(void) _PR_MD_FREE_LOCK(_MDLock *md);
#define    _PR_MD_FREE_LOCK _MD_FREE_LOCK

PR_EXTERN(void) _PR_MD_LOCK(_MDLock *md);
#define    _PR_MD_LOCK _MD_LOCK

PR_EXTERN(PRBool) _PR_MD_TEST_AND_LOCK(_MDLock *md);
#define    _PR_MD_TEST_AND_LOCK _MD_TEST_AND_LOCK

PR_EXTERN(void) _PR_MD_UNLOCK(_MDLock *md);
#define    _PR_MD_UNLOCK _MD_UNLOCK

PR_EXTERN(void) _PR_MD_IOQ_LOCK(void);
#define    _PR_MD_IOQ_LOCK _MD_IOQ_LOCK

PR_EXTERN(void) _PR_MD_IOQ_UNLOCK(void);
#define    _PR_MD_IOQ_UNLOCK _MD_IOQ_UNLOCK

#ifndef _PR_LOCAL_THREADS_ONLY /* not if only local threads supported */
/* Semaphore related -- only for native threads */
#ifdef HAVE_CVAR_BUILT_ON_SEM
PR_EXTERN(void) _PR_MD_NEW_SEM(_MDSemaphore *md, PRUintn value);
#define _PR_MD_NEW_SEM _MD_NEW_SEM

PR_EXTERN(void) _PR_MD_DESTROY_SEM(_MDSemaphore *md);
#define _PR_MD_DESTROY_SEM _MD_DESTROY_SEM

PR_EXTERN(PRStatus) _PR_MD_TIMED_WAIT_SEM(
    _MDSemaphore *md, PRIntervalTime timeout);
#define _PR_MD_TIMED_WAIT_SEM _MD_TIMED_WAIT_SEM

PR_EXTERN(PRStatus) _PR_MD_WAIT_SEM(_MDSemaphore *md);
#define _PR_MD_WAIT_SEM _MD_WAIT_SEM

PR_EXTERN(void) _PR_MD_POST_SEM(_MDSemaphore *md);
#define _PR_MD_POST_SEM _MD_POST_SEM
#endif /* HAVE_CVAR_BUILT_ON_SEM */

#endif

/* Condition Variables related -- only for native threads */

#ifndef _PR_LOCAL_THREADS_ONLY /* not if only local threads supported */
PR_EXTERN(PRInt32) _PR_MD_NEW_CV(_MDCVar *md);
#define    _PR_MD_NEW_CV _MD_NEW_CV

PR_EXTERN(void) _PR_MD_FREE_CV(_MDCVar *md);
#define    _PR_MD_FREE_CV _MD_FREE_CV

PR_EXTERN(void) _PR_MD_WAIT_CV(
    _MDCVar *mdCVar,_MDLock *mdLock,PRIntervalTime timeout);
#define    _PR_MD_WAIT_CV _MD_WAIT_CV

PR_EXTERN(void) _PR_MD_NOTIFY_CV(_MDCVar *md, _MDLock *lock);
#define    _PR_MD_NOTIFY_CV _MD_NOTIFY_CV

PR_EXTERN(void) _PR_MD_NOTIFYALL_CV(_MDCVar *md, _MDLock *lock);
#define    _PR_MD_NOTIFYALL_CV _MD_NOTIFYALL_CV
#endif /* _PR_LOCAL_THREADS_ONLY */

/* Threads related */
PR_EXTERN(PRThread*) _PR_MD_CURRENT_THREAD(void);
#define    _PR_MD_CURRENT_THREAD _MD_CURRENT_THREAD

PR_EXTERN(PRThread*) _PR_MD_GET_ATTACHED_THREAD(void);
#define    _PR_MD_GET_ATTACHED_THREAD _MD_GET_ATTACHED_THREAD

PR_EXTERN(PRThread*) _PR_MD_LAST_THREAD(void);
#define    _PR_MD_LAST_THREAD _MD_LAST_THREAD

PR_EXTERN(void) _PR_MD_SET_CURRENT_THREAD(PRThread *thread);
#define    _PR_MD_SET_CURRENT_THREAD _MD_SET_CURRENT_THREAD

PR_EXTERN(void) _PR_MD_SET_LAST_THREAD(PRThread *thread);
#define    _PR_MD_SET_LAST_THREAD _MD_SET_LAST_THREAD

extern PRStatus _PR_MD_INIT_THREAD(PRThread *thread);
#define    _PR_MD_INIT_THREAD _MD_INIT_THREAD

extern void _PR_MD_EXIT_THREAD(PRThread *thread);
#define    _PR_MD_EXIT_THREAD _MD_EXIT_THREAD

#ifndef _PR_LOCAL_THREADS_ONLY /* not if only local threads supported */

PR_EXTERN(PRStatus) _PR_MD_INIT_ATTACHED_THREAD(PRThread *thread);
#define    _PR_MD_INIT_ATTACHED_THREAD _MD_INIT_ATTACHED_THREAD

extern void _PR_MD_SUSPEND_THREAD(PRThread *thread);
#define    _PR_MD_SUSPEND_THREAD _MD_SUSPEND_THREAD

extern void _PR_MD_RESUME_THREAD(PRThread *thread);
#define    _PR_MD_RESUME_THREAD _MD_RESUME_THREAD

extern void _PR_MD_SUSPEND_CPU(_PRCPU  *cpu);
#define    _PR_MD_SUSPEND_CPU _MD_SUSPEND_CPU

extern void _PR_MD_RESUME_CPU(_PRCPU  *cpu);
#define    _PR_MD_RESUME_CPU _MD_RESUME_CPU

extern void _PR_MD_BEGIN_SUSPEND_ALL(void);
#define    _PR_MD_BEGIN_SUSPEND_ALL _MD_BEGIN_SUSPEND_ALL

extern void _PR_MD_END_SUSPEND_ALL(void);
#define    _PR_MD_END_SUSPEND_ALL _MD_END_SUSPEND_ALL

extern void _PR_MD_BEGIN_RESUME_ALL(void);
#define    _PR_MD_BEGIN_RESUME_ALL _MD_BEGIN_RESUME_ALL

extern void _PR_MD_END_RESUME_ALL(void);
#define    _PR_MD_END_RESUME_ALL _MD_END_RESUME_ALL

#if defined(IRIX) 
PR_EXTERN(void) _PR_IRIX_CHILD_PROCESS(void);
#endif        /* IRIX */

#endif        /* !_PR_LOCAL_THREADS_ONLY */

extern void _PR_MD_CLEAN_THREAD(PRThread *thread);
#define    _PR_MD_CLEAN_THREAD _MD_CLEAN_THREAD

#ifdef HAVE_CUSTOM_USER_THREADS
extern void _PR_MD_CREATE_PRIMORDIAL_USER_THREAD(PRThread *);
#define    _PR_MD_CREATE_PRIMORDIAL_USER_THREAD _MD_CREATE_PRIMORDIAL_USER_THREAD

extern PRThread* _PR_MD_CREATE_USER_THREAD(
                        PRUint32 stacksize,
                        void (*start)(void *),
                        void *arg);
#define    _PR_MD_CREATE_USER_THREAD _MD_CREATE_USER_THREAD
#endif

extern PRStatus _PR_MD_CREATE_THREAD(
                        PRThread *thread, 
                        void (*start) (void *), 
                        PRThreadPriority priority,                      
                        PRThreadScope scope,
                        PRThreadState state,
                        PRUint32 stackSize);
#define    _PR_MD_CREATE_THREAD _MD_CREATE_THREAD

extern void _PR_MD_YIELD(void);
#define    _PR_MD_YIELD _MD_YIELD

extern void _PR_MD_SET_PRIORITY(_MDThread *md, PRThreadPriority newPri);
#define    _PR_MD_SET_PRIORITY _MD_SET_PRIORITY

PR_EXTERN(void) _PR_MD_SUSPENDALL(void);
#define    _PR_MD_SUSPENDALL _MD_SUSPENDALL

PR_EXTERN(void) _PR_MD_RESUMEALL(void);
#define    _PR_MD_RESUMEALL _MD_RESUMEALL

extern void _PR_MD_INIT_CONTEXT(
    PRThread *thread, char *top, void (*start) (void), PRBool *status);
#define    _PR_MD_INIT_CONTEXT _MD_INIT_CONTEXT

extern void _PR_MD_SWITCH_CONTEXT(PRThread *thread);
#define    _PR_MD_SWITCH_CONTEXT _MD_SWITCH_CONTEXT

extern void _PR_MD_RESTORE_CONTEXT(PRThread *thread);
#define    _PR_MD_RESTORE_CONTEXT _MD_RESTORE_CONTEXT

/* Directory enumeration related */
extern PRStatus _PR_MD_OPEN_DIR(_MDDir *md,const char *name);
#define    _PR_MD_OPEN_DIR _MD_OPEN_DIR

extern char * _PR_MD_READ_DIR(_MDDir *md, PRIntn flags);
#define    _PR_MD_READ_DIR _MD_READ_DIR

extern PRInt32 _PR_MD_CLOSE_DIR(_MDDir *md);
#define    _PR_MD_CLOSE_DIR _MD_CLOSE_DIR

/* I/O related */
extern void _PR_MD_INIT_FILEDESC(PRFileDesc *fd);
#define    _PR_MD_INIT_FILEDESC _MD_INIT_FILEDESC

#ifdef XP_MAC
extern void _PR_MD_FREE_FILEDESC(PRFileDesc *fd);
#define    _PR_MD_FREE_FILEDESC _MD_FREE_FILEDESC
#endif

extern void _PR_MD_MAKE_NONBLOCK(PRFileDesc *fd);
#define    _PR_MD_MAKE_NONBLOCK _MD_MAKE_NONBLOCK

/* File I/O related */
extern PRInt32 _PR_MD_OPEN(const char *name, PRIntn osflags, PRIntn mode);
#define    _PR_MD_OPEN _MD_OPEN

extern PRInt32 _PR_MD_CLOSE_FILE(PRInt32 osfd);
#define    _PR_MD_CLOSE_FILE _MD_CLOSE_FILE

extern PRInt32 _PR_MD_READ(PRFileDesc *fd, void *buf, PRInt32 amount);
#define    _PR_MD_READ _MD_READ

extern PRInt32 _PR_MD_WRITE(PRFileDesc *fd, const void *buf, PRInt32 amount);
#define    _PR_MD_WRITE _MD_WRITE

extern PRInt32 _PR_MD_WRITEV(
    PRFileDesc *fd, const struct PRIOVec *iov,
    PRInt32 iov_size, PRIntervalTime timeout);
#define    _PR_MD_WRITEV _MD_WRITEV

extern PRInt32 _PR_MD_FSYNC(PRFileDesc *fd);
#define    _PR_MD_FSYNC _MD_FSYNC

extern PRInt32 _PR_MD_DELETE(const char *name);
#define        _PR_MD_DELETE _MD_DELETE

extern PRInt32 _PR_MD_RENAME(const char *from, const char *to);
#define _PR_MD_RENAME _MD_RENAME

extern PRInt32 _PR_MD_ACCESS(const char *name, PRAccessHow how);
#define _PR_MD_ACCESS _MD_ACCESS

extern PRInt32 _PR_MD_STAT(const char *name, struct stat *buf);
#define _PR_MD_STAT _MD_STAT

extern PRInt32 _PR_MD_MKDIR(const char *name, PRIntn mode);
#define _PR_MD_MKDIR _MD_MKDIR

extern PRInt32 _PR_MD_RMDIR(const char *name);
#define _PR_MD_RMDIR _MD_RMDIR

/* Socket I/O related */
extern void _PR_MD_INIT_IO(void);
#define    _PR_MD_INIT_IO _MD_INIT_IO

extern PRInt32 _PR_MD_CLOSE_SOCKET(PRInt32 osfd);
#define    _PR_MD_CLOSE_SOCKET _MD_CLOSE_SOCKET

extern PRInt32 _PR_MD_CONNECT(
    PRFileDesc *fd, const PRNetAddr *addr,
    PRUint32 addrlen, PRIntervalTime timeout);
#define    _PR_MD_CONNECT _MD_CONNECT

extern PRInt32 _PR_MD_ACCEPT(
    PRFileDesc *fd, PRNetAddr *addr,
    PRUint32 *addrlen, PRIntervalTime timeout);
#define    _PR_MD_ACCEPT _MD_ACCEPT

extern PRInt32 _PR_MD_BIND(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen);
#define    _PR_MD_BIND _MD_BIND

extern PRInt32 _PR_MD_LISTEN(PRFileDesc *fd, PRIntn backlog);
#define    _PR_MD_LISTEN _MD_LISTEN

extern PRInt32 _PR_MD_SHUTDOWN(PRFileDesc *fd, PRIntn how);
#define    _PR_MD_SHUTDOWN _MD_SHUTDOWN

extern PRInt32 _PR_MD_RECV(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout);
#define    _PR_MD_RECV _MD_RECV

extern PRInt32 _PR_MD_SEND(
    PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags, 
    PRIntervalTime timeout);
#define    _PR_MD_SEND _MD_SEND

extern PRInt32 _PR_MD_ACCEPT_READ(PRFileDesc *sd, PRInt32 *newSock, 
                                PRNetAddr **raddr, void *buf, PRInt32 amount,
                                PRIntervalTime timeout);
#define _PR_MD_ACCEPT_READ _MD_ACCEPT_READ
extern PRInt32 _PR_EmulateAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
              PRNetAddr **raddr, void *buf, PRInt32 amount,
              PRIntervalTime timeout);

#ifdef WIN32
extern PRInt32 _PR_MD_FAST_ACCEPT(PRFileDesc *fd, PRNetAddr *addr, 
                                PRUint32 *addrlen, PRIntervalTime timeout,
                                PRBool fast,
                                _PR_AcceptTimeoutCallback callback,
                                void *callbackArg);

extern PRInt32 _PR_MD_FAST_ACCEPT_READ(PRFileDesc *sd, PRInt32 *newSock, 
                                PRNetAddr **raddr, void *buf, PRInt32 amount,
                                PRIntervalTime timeout, PRBool fast,
                                _PR_AcceptTimeoutCallback callback,
                                void *callbackArg);

extern void _PR_MD_UPDATE_ACCEPT_CONTEXT(PRInt32 s, PRInt32 ls);
#define _PR_MD_UPDATE_ACCEPT_CONTEXT _MD_UPDATE_ACCEPT_CONTEXT
#endif /* WIN32 */

extern PRInt32 _PR_MD_TRANSMITFILE(
    PRFileDesc *sock, PRFileDesc *file, 
    const void *headers, PRInt32 hlen, PRInt32 flags,
    PRIntervalTime timeout);
#define _PR_MD_TRANSMITFILE _MD_TRANSMITFILE
extern PRInt32 _PR_EmulateTransmitFile(PRFileDesc *sd, PRFileDesc *fd,
              const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
              PRIntervalTime timeout);

extern PRStatus _PR_MD_GETSOCKNAME(
    PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen);
#define    _PR_MD_GETSOCKNAME _MD_GETSOCKNAME

extern PRStatus _PR_MD_GETPEERNAME(
    PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen);
#define    _PR_MD_GETPEERNAME _MD_GETPEERNAME

extern PRStatus _PR_MD_GETSOCKOPT(
    PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen);
#define    _PR_MD_GETSOCKOPT _MD_GETSOCKOPT

extern PRStatus _PR_MD_SETSOCKOPT(
    PRFileDesc *fd, PRInt32 level, PRInt32 optname,
    const char* optval, PRInt32 optlen);
#define    _PR_MD_SETSOCKOPT _MD_SETSOCKOPT

extern PRStatus PR_CALLBACK _PR_SocketGetSocketOption(
    PRFileDesc *fd, PRSocketOptionData *data);

extern PRStatus PR_CALLBACK _PR_SocketSetSocketOption(
    PRFileDesc *fd, const PRSocketOptionData *data);

extern PRInt32 _PR_MD_RECVFROM(
    PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
    PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout);
#define    _PR_MD_RECVFROM _MD_RECVFROM

extern PRInt32 _PR_MD_SENDTO(
    PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
    const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout);
#define    _PR_MD_SENDTO _MD_SENDTO

extern PRInt32 _PR_MD_SOCKETPAIR(int af, int type, int flags, PRInt32 *osfd);
#define    _PR_MD_SOCKETPAIR _MD_SOCKETPAIR

extern PRInt32 _PR_MD_SOCKET(int af, int type, int flags);
#define    _PR_MD_SOCKET _MD_SOCKET

extern PRInt32 _PR_MD_SOCKETAVAILABLE(PRFileDesc *fd);
#define    _PR_MD_SOCKETAVAILABLE _MD_SOCKETAVAILABLE

extern PRInt32 _PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds,
                                                                                        PRIntervalTime timeout);
#define    _PR_MD_PR_POLL _MD_PR_POLL

extern PRStatus _PR_MD_SET_FD_INHERITABLE(PRFileDesc *fd, PRBool inheritable);
#define    _PR_MD_SET_FD_INHERITABLE _MD_SET_FD_INHERITABLE


#define _PR_PROCESS_TIMEOUT_INTERRUPT_ERRORS(me) \
        if (_PR_PENDING_INTERRUPT(me)) { \
                me->flags &= ~_PR_INTERRUPT; \
                PR_SetError( PR_PENDING_INTERRUPT_ERROR, 0); \
        } else { \
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0); \
        }                                                        
                
extern void *_PR_MD_GET_SP(PRThread *thread);
#define    _PR_MD_GET_SP _MD_GET_SP

#endif /* defined(_PR_PTHREADS) */

/************************************************************************/
/*************************************************************************
** The remainder of the definitions are shared by pthreads and the classic
** NSPR code. These too may be conditionalized.
*************************************************************************/
/************************************************************************/

extern PRInt32 _PR_MD_LSEEK(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence);
#define    _PR_MD_LSEEK _MD_LSEEK

extern PRInt64 _PR_MD_LSEEK64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence);
#define    _PR_MD_LSEEK64 _MD_LSEEK64

extern PRInt32 _PR_MD_GETFILEINFO(const char *fn, PRFileInfo *info);
#define _PR_MD_GETFILEINFO _MD_GETFILEINFO

extern PRInt32 _PR_MD_GETFILEINFO64(const char *fn, PRFileInfo64 *info);
#define _PR_MD_GETFILEINFO64 _MD_GETFILEINFO64

extern PRInt32 _PR_MD_GETOPENFILEINFO(const PRFileDesc *fd, PRFileInfo *info);
#define _PR_MD_GETOPENFILEINFO _MD_GETOPENFILEINFO

extern PRInt32 _PR_MD_GETOPENFILEINFO64(const PRFileDesc *fd, PRFileInfo64 *info);
#define _PR_MD_GETOPENFILEINFO64 _MD_GETOPENFILEINFO64


/*****************************************************************************/
/************************** File descriptor caching **************************/
/*****************************************************************************/
extern void _PR_InitFdCache(void);
extern void _PR_CleanupFdCache(void);
extern PRFileDesc *_PR_Getfd(void);
extern void _PR_Putfd(PRFileDesc *fd);

/*
 * These flags are used by NSPR temporarily in the poll
 * descriptor's out_flags field to record the mapping of
 * NSPR's poll flags to the system poll flags.
 *
 * If _PR_POLL_READ_SYS_WRITE bit is set, it means the
 * PR_POLL_READ flag specified by the topmost layer is
 * mapped to the WRITE flag at the system layer.  Similarly
 * for the other three _PR_POLL_XXX_SYS_YYY flags.  It is
 * assumed that the PR_POLL_EXCEPT flag doesn't get mapped
 * to other flags.
 */
#define _PR_POLL_READ_SYS_READ     0x1
#define _PR_POLL_READ_SYS_WRITE    0x2
#define _PR_POLL_WRITE_SYS_READ    0x4
#define _PR_POLL_WRITE_SYS_WRITE   0x8

/*
** These methods are coerced into file descriptor methods table
** when the intended service is inappropriate for the particular
** type of file descriptor.
*/
extern PRIntn _PR_InvalidInt(void);
extern PRInt16 _PR_InvalidInt16(void);
extern PRInt64 _PR_InvalidInt64(void);
extern PRStatus _PR_InvalidStatus(void);
extern PRFileDesc *_PR_InvalidDesc(void);

extern PRIOMethods _pr_faulty_methods;

extern PRStatus _PR_MapOptionName(
    PRSockOption optname, PRInt32 *level, PRInt32 *name);
extern void _PR_InitThreads(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs);

struct PRLock {
#if defined(_PR_PTHREADS)
    pthread_mutex_t mutex;          /* the underlying lock */
    _PT_Notified notified;          /* array of conditions notified */
    pthread_t owner;                /* current lock owner */
#elif defined(_PR_BTHREADS)
    sem_id	semaphoreID;	    /* the underlying lock */
    int32	benaphoreCount;	    /* number of people in lock */
    thread_id	owner;		    /* current lock owner */
#else /* not pthreads or Be threads */
    PRCList links;                  /* linkage for PRThread.lockList */
    struct PRThread *owner;         /* current lock owner */
    PRCList waitQ;                  /* list of threads waiting for lock */
    PRThreadPriority priority;      /* priority of lock */ 
    PRThreadPriority boostPriority; /* boosted priority of lock owner */
    _MDLock ilock;                  /* Internal Lock to protect user-level fields */
#endif
};

extern void _PR_InitLocks(void);

struct PRCondVar {
    PRLock *lock;               /* associated lock that protects the condition */
#if defined(_PR_PTHREADS)
    pthread_cond_t cv;          /* underlying pthreads condition */
    PRInt32 notify_pending;     /* CV has destroy pending notification */
#elif defined(_PR_BTHREADS)
    sem_id	isem;		/* Semaphore used to lock threadQ */
    int32	benaphoreCount; /* Number of people in lock */
#else /* not pthreads or Be threads */
    PRCList condQ;              /* Condition variable wait Q */
    _MDLock ilock;              /* Internal Lock to protect condQ */
    _MDCVar md;
#endif
};

/************************************************************************/

struct PRMonitor {
    const char* name;           /* monitor name for debugging */
#if defined(_PR_PTHREADS)
    PRLock lock;                /* the lock struture structure */
    pthread_t owner;            /* the owner of the lock or zero */
    PRCondVar *cvar;            /* condition variable queue */
#else  /* defined(_PR_PTHREADS) */
    PRCondVar *cvar;            /* associated lock and condition variable queue */
#endif /* defined(_PR_PTHREADS) */
    PRUint32 entryCount;        /* # of times re-entered */
};

/************************************************************************/

struct PRSemaphore {
#if defined(_PR_BTHREADS)
    sem_id  sem;
    int32   benaphoreCount;
#else
    PRCondVar *cvar;        /* associated lock and condition variable queue */
    PRUintn count;            /* the value of the counting semaphore */
    PRUint32 waiters;            /* threads waiting on the semaphore */
#if defined(_PR_PTHREADS)
#else  /* defined(_PR_PTHREADS) */
    _MDSemaphore md;
#endif /* defined(_PR_PTHREADS) */
#endif /* defined(_PR_BTHREADS) */
};

PR_EXTERN(void) _PR_InitSem(void);

/************************************************************************/

/* XXX this needs to be exported (sigh) */
struct PRThreadStack {
    PRCList links;
    PRUintn flags;

    char *allocBase;            /* base of stack's allocated memory */
    PRUint32 allocSize;         /* size of stack's allocated memory */
    char *stackBottom;          /* bottom of stack from C's point of view */
    char *stackTop;             /* top of stack from C's point of view */
    PRUint32 stackSize;         /* size of usable portion of the stack */

    PRSegment *seg;
        PRThread* thr;          /* back pointer to thread owning this stack */

#if defined(_PR_PTHREADS)
#else /* defined(_PR_PTHREADS) */
    _MDThreadStack md;
#endif /* defined(_PR_PTHREADS) */
};

/*
 * Thread private data destructor array
 * There is a destructor (or NULL) associated with each key and
 *    applied to all threads known to the system.
 *  Storage allocated in prtpd.c.
 */
extern PRThreadPrivateDTOR *_pr_tpd_destructors;
extern void _PR_DestroyThreadPrivate(PRThread*);

typedef void (PR_CALLBACK *_PRStartFn)(void *);

struct PRThread {
    PRUint32 state;                 /* thread's creation state */
    PRThreadPriority priority;      /* apparent priority, loosly defined */

    void *arg;                      /* argument to the client's entry point */
    _PRStartFn startFunc;           /* the root of the client's thread */

    PRThreadStack *stack;           /* info about thread's stack (for GC) */
    void *environment;              /* pointer to execution environment */

    PRThreadDumpProc dump;          /* dump thread info out */
    void *dumpArg;                  /* argument for the dump function */

    /*
    ** Per thread private data
    */
    PRUint32 tpdLength;             /* thread's current vector length */
    void **privateData;             /* private data vector or NULL */
    PRInt32 errorStringSize;        /* byte length of current error string | zero */
    PRErrorCode errorCode;          /* current NSPR error code | zero */
    PRInt32 osErrorCode;            /* mapping of errorCode | zero */
    char *errorString;              /* current error string | NULL */

#if defined(_PR_PTHREADS)
    pthread_t id;                   /* pthread identifier for the thread */
    PRBool okToDelete;              /* ok to delete the PRThread struct? */
    PRCondVar *io_cv;               /* a condition used to run i/o */
    PRCondVar *waiting;             /* where the thread is waiting | NULL */
    void *sp;                       /* recorded sp for garbage collection */
    PRThread *next, *prev;          /* simple linked list of all threads */
    PRUint32 suspend;               /* used to store suspend and resume flags */
#ifdef PT_NO_SIGTIMEDWAIT
    pthread_mutex_t suspendResumeMutex;
    pthread_cond_t suspendResumeCV;
#endif
#elif defined(_PR_BTHREADS)
    PRUint32 flags;
    _MDThread md;
    PRBool io_pending;
    PRInt32 io_fd;
    PRBool io_suspended;
#else /* not pthreads or Be threads */
    _MDLock threadLock;             /* Lock to protect thread state variables.
                                     * Protects the following fields:
                                     *     state
                                     *     priority
                                     *     links
                                     *     wait
                                     *     cpu
                                     */
    PRUint32 queueCount;
    PRUint32 waitCount;

    PRCList active;                 /* on list of all active threads        */
    PRCList links;
    PRCList waitQLinks;             /* when thread is PR_Wait'ing */
    PRCList lockList;               /* list of locks currently holding */
    PRIntervalTime sleep;           /* sleep time when thread is sleeping */
    struct _wait {
        struct PRLock *lock;
        struct PRCondVar *cvar;
    } wait;

    PRUint32 id;
    PRUint32 flags;
    PRUint32 no_sched;              /* Don't schedule the thread to run.
                                     * This flag has relevance only when
                                     * multiple NSPR CPUs are created.
                                     * When a thread is de-scheduled, there
                                     * is a narrow window of time in which
                                     * the thread is put on the run queue
                                     * but the scheduler is actually using
                                     * the stack of this thread.  It is safe
                                     * to run this thread on a different CPU
                                     * only when its stack is not in use on
                                     * any other CPU.  The no_sched flag is
                                     * set during this interval to prevent
                                     * the thread from being scheduled on a
                                     * different CPU.
                                     */
    PRUint32 numExits;
    _PRPerThreadExit *ptes;

    /* thread termination condition variable for join */
    PRCondVar *term;

    _PRCPU *cpu;                    /* cpu to which this thread is bound    */
    PRUint32 threadAllocatedOnStack;/* boolean */

    /* When an async IO is in progress and a second async IO cannot be 
     * initiated, the io_pending flag is set to true.  Some platforms will
     * not use the io_pending flag.  If the io_pending flag is true, then
     * io_fd is the OS-file descriptor on which IO is pending.
     */
    PRBool io_pending;
    PRInt32 io_fd;
 
    /* If a timeout occurs or if an outstanding IO is interrupted and the
     * OS doesn't support a real cancellation (NT or MAC), then the 
     * io_suspended flag will be set to true.  The thread will be resumed
     * but may run into trouble issuing additional IOs until the io_pending
     * flag can be cleared 
     */
    PRBool io_suspended;

    _MDThread md;
#endif
};

struct PRProcessAttr {
    PRFileDesc *stdinFd;
    PRFileDesc *stdoutFd;
    PRFileDesc *stderrFd;
    char *currentDirectory;
    char *fdInheritBuffer;
    PRSize fdInheritBufferSize;
    PRSize fdInheritBufferUsed;
};

struct PRProcess {
    _MDProcess md;
};

struct PRFileMap {
    PRFileDesc *fd;
    PRFileMapProtect prot;
    _MDFileMap md;
};

/************************************************************************/

struct PRFilePrivate {
    PRInt32 state;
    PRBool nonblocking;
    PRBool inheritable;
    PRFileDesc *next;
    PRIntn lockCount;
    _MDFileDesc md;
};

struct PRDir {
    PRDirEntry d;
    _MDDir md;
};

extern void _PR_InitSegs(void);
extern void _PR_InitStacks(void);
extern void _PR_InitTPD(void);
extern void _PR_InitMem(void);
extern void _PR_InitEnv(void);
extern void _PR_InitCMon(void);
extern void _PR_InitIO(void);
extern void _PR_InitLog(void);
extern void _PR_InitNet(void);
extern void _PR_InitClock(void);
extern void _PR_InitLinker(void);
extern void _PR_InitAtomic(void);
extern void _PR_InitCPUs(void);
extern void _PR_InitDtoa(void);
extern void _PR_InitMW(void);
extern void _PR_InitRWLocks(void);
extern void _PR_NotifyCondVar(PRCondVar *cvar, PRThread *me);
extern void _PR_CleanupThread(PRThread *thread);
extern void _PR_CleanupTPD(void);
extern void _PR_Cleanup(void);
extern void _PR_LogCleanup(void);
extern void _PR_InitLayerCache(void);

extern PRBool _pr_initialized;
extern void _PR_ImplicitInitialization(void);
extern PRBool _PR_Obsolete(const char *obsolete, const char *preferred);

/************************************************************************/

struct PRSegment {
    void *vaddr;
    PRUint32 size;
    PRUintn flags;
#if defined(_PR_PTHREADS)
#else  /* defined(_PR_PTHREADS) */
    _MDSegment md;
#endif /* defined(_PR_PTHREADS) */
};

/* PRSegment.flags */
#define _PR_SEG_VM    0x1

/***********************************************************************
** FUNCTION:	_PR_NewSegment()
** DESCRIPTION:
**   Allocate a memory segment. The "size" value is rounded up to the
**   native system page size and a page aligned portion of memory is
**   returned.  This memory is not part of the malloc heap. If "vaddr" is
**   not NULL then PR tries to allocate the segment at the desired virtual
**   address.
** INPUTS:	size:  size of the desired memory segment
**          vaddr:  address at which the newly aquired segment is to be
**                  mapped into memory.
** OUTPUTS:	a memory segment is allocated, a PRSegment is allocated
** RETURN:	pointer to PRSegment
***********************************************************************/
extern PRSegment* _PR_NewSegment(PRUint32 size, void *vaddr);

/***********************************************************************
** FUNCTION:	_PR_DestroySegment()
** DESCRIPTION:
**   The memory segment and the PRSegment are freed
** INPUTS:	seg:  pointer to PRSegment to be freed
** OUTPUTS:	the the PRSegment and its associated memory segment are freed
** RETURN:	void
***********************************************************************/
extern void _PR_DestroySegment(PRSegment *seg);

/************************************************************************/

extern PRInt32 _pr_pageSize;
extern PRInt32 _pr_pageShift;

extern PRLogModuleInfo *_pr_clock_lm;
extern PRLogModuleInfo *_pr_cmon_lm;
extern PRLogModuleInfo *_pr_io_lm;
extern PRLogModuleInfo *_pr_cvar_lm;
extern PRLogModuleInfo *_pr_mon_lm;
extern PRLogModuleInfo *_pr_linker_lm;
extern PRLogModuleInfo *_pr_sched_lm;
extern PRLogModuleInfo *_pr_thread_lm;
extern PRLogModuleInfo *_pr_gc_lm;

extern PRFileDesc *_pr_stdin;
extern PRFileDesc *_pr_stdout;
extern PRFileDesc *_pr_stderr;

#if defined(_PR_INET6)
extern PRBool _pr_ipv6_enabled;  /* defined in prnetdb.c */
#endif

/* Overriding malloc, free, etc. */
#if !defined(_PR_NO_PREEMPT) && defined(XP_UNIX) \
        && !defined(_PR_PTHREADS) && !defined(_PR_GLOBAL_THREADS_ONLY) \
        && !defined(PURIFY) \
        && !defined(RHAPSODY) \
        && !defined(NEXTSTEP) \
        && !defined(QNX) \
        && !(defined (UNIXWARE) && defined (USE_SVR4_THREADS))
#define _PR_OVERRIDE_MALLOC
#endif

/*************************************************************************
* External machine-dependent code provided by each OS.                     *                                                                     *
*************************************************************************/

/* Initialization related */
extern void _PR_MD_EARLY_INIT(void);
#define    _PR_MD_EARLY_INIT _MD_EARLY_INIT

extern void _PR_MD_INTERVAL_INIT(void);
#define    _PR_MD_INTERVAL_INIT _MD_INTERVAL_INIT

PR_EXTERN(void) _PR_MD_INIT_SEGS(void);
#define    _PR_MD_INIT_SEGS _MD_INIT_SEGS

PR_EXTERN(void) _PR_MD_FINAL_INIT(void);
#define    _PR_MD_FINAL_INIT _MD_FINAL_INIT

/* Process control */

extern PRProcess * _PR_MD_CREATE_PROCESS(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);
#define    _PR_MD_CREATE_PROCESS _MD_CREATE_PROCESS

extern PRStatus _PR_MD_DETACH_PROCESS(PRProcess *process);
#define    _PR_MD_DETACH_PROCESS _MD_DETACH_PROCESS

extern PRStatus _PR_MD_WAIT_PROCESS(PRProcess *process, PRInt32 *exitCode);
#define    _PR_MD_WAIT_PROCESS _MD_WAIT_PROCESS

extern PRStatus _PR_MD_KILL_PROCESS(PRProcess *process);
#define    _PR_MD_KILL_PROCESS _MD_KILL_PROCESS        

/* Current Time */
PR_EXTERN(PRTime) _PR_MD_NOW(void);
#define    _PR_MD_NOW _MD_NOW

/* Environment related */
extern char* _PR_MD_GET_ENV(const char *name);
#define    _PR_MD_GET_ENV _MD_GET_ENV

extern PRIntn _PR_MD_PUT_ENV(const char *name);
#define    _PR_MD_PUT_ENV _MD_PUT_ENV

/* Atomic operations */

extern void _PR_MD_INIT_ATOMIC(void);
#define    _PR_MD_INIT_ATOMIC _MD_INIT_ATOMIC

extern PRInt32 _PR_MD_ATOMIC_INCREMENT(PRInt32 *);
#define    _PR_MD_ATOMIC_INCREMENT _MD_ATOMIC_INCREMENT

extern PRInt32 _PR_MD_ATOMIC_ADD(PRInt32 *, PRInt32);
#define    _PR_MD_ATOMIC_ADD _MD_ATOMIC_ADD

extern PRInt32 _PR_MD_ATOMIC_DECREMENT(PRInt32 *);
#define    _PR_MD_ATOMIC_DECREMENT _MD_ATOMIC_DECREMENT

extern PRInt32 _PR_MD_ATOMIC_SET(PRInt32 *, PRInt32);
#define    _PR_MD_ATOMIC_SET _MD_ATOMIC_SET

/* Segment related */
PR_EXTERN(PRStatus) _PR_MD_ALLOC_SEGMENT(PRSegment *seg, PRUint32 size, void *vaddr);
#define    _PR_MD_ALLOC_SEGMENT _MD_ALLOC_SEGMENT

PR_EXTERN(void) _PR_MD_FREE_SEGMENT(PRSegment *seg);
#define    _PR_MD_FREE_SEGMENT _MD_FREE_SEGMENT

/* Garbage collection */

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
*/
extern PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np);

/* Time intervals */

extern PRIntervalTime _PR_MD_GET_INTERVAL(void);
#define _PR_MD_GET_INTERVAL _MD_GET_INTERVAL

extern PRIntervalTime _PR_MD_INTERVAL_PER_SEC(void);
#define _PR_MD_INTERVAL_PER_SEC _MD_INTERVAL_PER_SEC

/* Affinity masks */

extern PRInt32 _PR_MD_SETTHREADAFFINITYMASK(PRThread *thread, PRUint32 mask );
#define _PR_MD_SETTHREADAFFINITYMASK _MD_SETTHREADAFFINITYMASK

extern PRInt32 _PR_MD_GETTHREADAFFINITYMASK(PRThread *thread, PRUint32 *mask);
#define _PR_MD_GETTHREADAFFINITYMASK _MD_GETTHREADAFFINITYMASK

/* File locking */

extern PRStatus _PR_MD_LOCKFILE(PRInt32 osfd);
#define    _PR_MD_LOCKFILE _MD_LOCKFILE

extern PRStatus _PR_MD_TLOCKFILE(PRInt32 osfd);
#define    _PR_MD_TLOCKFILE _MD_TLOCKFILE

extern PRStatus _PR_MD_UNLOCKFILE(PRInt32 osfd);
#define    _PR_MD_UNLOCKFILE _MD_UNLOCKFILE

/* Memory-mapped files */

extern PRStatus _PR_MD_CREATE_FILE_MAP(PRFileMap *fmap, PRInt64 size);
#define _PR_MD_CREATE_FILE_MAP _MD_CREATE_FILE_MAP

extern void * _PR_MD_MEM_MAP(
    PRFileMap *fmap,
    PRInt64 offset,
    PRUint32 len);
#define _PR_MD_MEM_MAP _MD_MEM_MAP

extern PRStatus _PR_MD_MEM_UNMAP(void *addr, PRUint32 size);
#define _PR_MD_MEM_UNMAP _MD_MEM_UNMAP

extern PRStatus _PR_MD_CLOSE_FILE_MAP(PRFileMap *fmap);
#define _PR_MD_CLOSE_FILE_MAP _MD_CLOSE_FILE_MAP

/* Socket call error code */

PR_EXTERN(PRInt32) _PR_MD_GET_SOCKET_ERROR(void);
#define    _PR_MD_GET_SOCKET_ERROR _MD_GET_SOCKET_ERROR

/* Get name of current host */
extern PRStatus _PR_MD_GETHOSTNAME(char *name, PRUint32 namelen);
#define    _PR_MD_GETHOSTNAME _MD_GETHOSTNAME

#ifdef XP_BEOS

extern PRLock *_connectLock;

typedef struct _ConnectListNode {
	PRInt32		osfd;
	PRNetAddr	addr;
	PRUint32	addrlen;
	PRIntervalTime	timeout;
} ConnectListNode;

extern ConnectListNode connectList[64];

extern PRUint32 connectCount;

#endif /* XP_BEOS */

PR_END_EXTERN_C

#endif /* primpl_h___ */
