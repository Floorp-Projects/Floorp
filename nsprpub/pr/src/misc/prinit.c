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

#include "primpl.h"
#include <string.h>

PRLogModuleInfo *_pr_clock_lm;
PRLogModuleInfo *_pr_cmon_lm;
PRLogModuleInfo *_pr_io_lm;
PRLogModuleInfo *_pr_cvar_lm;
PRLogModuleInfo *_pr_mon_lm;
PRLogModuleInfo *_pr_linker_lm;
PRLogModuleInfo *_pr_sched_lm;
PRLogModuleInfo *_pr_thread_lm;
PRLogModuleInfo *_pr_gc_lm;

PRFileDesc *_pr_stdin;
PRFileDesc *_pr_stdout;
PRFileDesc *_pr_stderr;

#if !defined(_PR_PTHREADS)

PRCList _pr_active_local_threadQ =
			PR_INIT_STATIC_CLIST(&_pr_active_local_threadQ);
PRCList _pr_active_global_threadQ =
			PR_INIT_STATIC_CLIST(&_pr_active_global_threadQ);

_MDLock  _pr_cpuLock;           /* lock for the CPU Q */
PRCList _pr_cpuQ = PR_INIT_STATIC_CLIST(&_pr_cpuQ);

PRUint32 _pr_utid;

PRUintn _pr_userActive;
PRUintn _pr_systemActive;
PRUintn _pr_maxPTDs;

#ifdef _PR_LOCAL_THREADS_ONLY

struct _PRCPU 	*_pr_currentCPU;
PRThread 	*_pr_currentThread;
PRThread 	*_pr_lastThread;
PRInt32 	_pr_intsOff;

#endif /* _PR_LOCAL_THREADS_ONLY */

/* Lock protecting all "termination" condition variables of all threads */
PRLock *_pr_terminationCVLock;

#endif /* !defined(_PR_PTHREADS) */

static void _PR_InitCallOnce(void);
static void _PR_InitStuff(void);

/************************************************************************/
/**************************IDENTITY AND VERSIONING***********************/
/************************************************************************/
PR_IMPLEMENT(PRBool) PR_VersionCheck(const char *importedVersion)
{
    /*
    ** This is the secret handshake algorithm. Right now it requires
    ** an exact match. Later it should get more tricky.
    */
    return ((0 == strcmp(importedVersion, PR_VERSION)) ? PR_TRUE : PR_FALSE);
}  /* PR_VersionCheck */


PRBool _pr_initialized = PR_FALSE;

PR_IMPLEMENT(PRBool) PR_Initialized(void)
{
    return _pr_initialized;
}

static void _PR_InitStuff()
{
    if (_pr_initialized) return;
    _pr_initialized = PR_TRUE;

    (void) PR_GetPageSize();

	_pr_clock_lm = PR_NewLogModule("clock");
	_pr_cmon_lm = PR_NewLogModule("cmon");
	_pr_io_lm = PR_NewLogModule("io");
	_pr_mon_lm = PR_NewLogModule("mon");
	_pr_linker_lm = PR_NewLogModule("linker");
	_pr_cvar_lm = PR_NewLogModule("cvar");
	_pr_sched_lm = PR_NewLogModule("sched");
	_pr_thread_lm = PR_NewLogModule("thread");
	_pr_gc_lm = PR_NewLogModule("gc");
      
    /* NOTE: These init's cannot depend on _PR_MD_CURRENT_THREAD() */ 
    _PR_MD_EARLY_INIT();

    _PR_InitAtomic();
    _PR_InitLocks();
    _PR_InitSegs();
    _PR_InitStacks();
	_PR_InitTPD();
    _PR_InitEnv();
    _PR_InitLayerCache();

    _PR_InitThreads(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    
#ifdef WIN16
	{
	PRInt32 top;   /* artificial top of stack, win16 */
    _pr_top_of_task_stack = (char *) &top;
	}
#endif    

#ifndef _PR_GLOBAL_THREADS_ONLY
    _PR_InitCPUs();
#endif

/*
 * XXX: call _PR_InitMem only on those platforms for which nspr implements
 *	malloc, for now.
 */
#ifdef _PR_OVERRIDE_MALLOC
    _PR_InitMem();
#endif

    _PR_InitCMon();
    _PR_InitIO();
    _PR_InitNet();
    _PR_InitClock();
#ifdef PR_LOGGING
    _PR_InitLog();
#endif
    _PR_InitLinker();
    _PR_InitCallOnce();
    _PR_InitDtoa();

    _PR_MD_FINAL_INIT();
}

void _PR_ImplicitInitialization()
{
	_PR_InitStuff();

    /* Enable interrupts */
#ifndef _PR_GLOBAL_THREADS_ONLY
    _PR_MD_START_INTERRUPTS();
#endif

}

PR_IMPLEMENT(void) PR_DisableClockInterrupts(void)
{
#if !defined(_PR_PTHREADS)
	if (!_pr_initialized) {
		_PR_InitStuff();
	} else {
    	_PR_MD_DISABLE_CLOCK_INTERRUPTS();
	}
#endif
}

PR_IMPLEMENT(void) PR_BlockClockInterrupts(void)
{
#if !defined(_PR_PTHREADS)
    	_PR_MD_BLOCK_CLOCK_INTERRUPTS();
#endif
}

PR_IMPLEMENT(void) PR_UnblockClockInterrupts(void)
{
#if !defined(_PR_PTHREADS)
    	_PR_MD_UNBLOCK_CLOCK_INTERRUPTS();
#endif
}

PR_IMPLEMENT(void) PR_Init(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
#if defined(XP_MAC)
#pragma unused (type, priority, maxPTDs)
#endif

    _PR_ImplicitInitialization();
}

PR_IMPLEMENT(PRIntn) PR_Initialize(
    PRPrimordialFn prmain, PRIntn argc, char **argv, PRUintn maxPTDs)
{
#if defined(XP_MAC)
#pragma unused (maxPTDs)
#endif

    PRIntn rv;
    _PR_ImplicitInitialization();
    rv = prmain(argc, argv);
	PR_Cleanup();
    return rv;
}  /* PR_Initialize */

/*
 *-----------------------------------------------------------------------
 *
 * _PR_CleanupBeforeExit --
 *
 *   Perform the cleanup work before exiting the process. 
 *   We first do the cleanup generic to all platforms.  Then
 *   we call _PR_MD_CLEANUP_BEFORE_EXIT(), where platform-dependent
 *   cleanup is done.  This function is used by PR_Cleanup().
 *
 * See also: PR_Cleanup().
 *
 *-----------------------------------------------------------------------
 */
#if defined(_PR_PTHREADS)
    /* see ptthread.c */
#else
static void
_PR_CleanupBeforeExit(void)
{
/* 
Do not make any calls here other than to destroy resources.  For example,
do not make any calls that eventually may end up in PR_Lock.  Because the
thread is destroyed, can not access current thread any more.
*/
    _PR_CleanupTPD();
    if (_pr_terminationCVLock)
    /*
     * In light of the comment above, this looks real suspicious.
     * I'd go so far as to say it's just a problem waiting to happen.
     */
        PR_DestroyLock(_pr_terminationCVLock);

    _PR_MD_CLEANUP_BEFORE_EXIT();
}
#endif /* defined(_PR_PTHREADS) */

/*
 *----------------------------------------------------------------------
 *
 * PR_Cleanup --
 *
 *   Perform a graceful shutdown of the NSPR runtime.  PR_Cleanup() may
 *   only be called from the primordial thread, typically at the
 *   end of the main() function.  It returns when it has completed
 *   its platform-dependent duty and the process must not make any other
 *   NSPR library calls prior to exiting from main().
 *
 *   PR_Cleanup() first blocks the primordial thread until all the
 *   other user (non-system) threads, if any, have terminated.
 *   Then it performs cleanup in preparation for exiting the process.
 *   PR_Cleanup() does not exit the primordial thread (which would
 *   in turn exit the process).
 *   
 *   PR_Cleanup() only responds when it is called by the primordial
 *   thread. Calls by any other thread are silently ignored.
 *
 * See also: PR_ExitProcess()
 *
 *----------------------------------------------------------------------
 */
#if defined(_PR_PTHREADS)
    /* see ptthread.c */
#else

PR_IMPLEMENT(PRStatus) PR_Cleanup()
{
    PRThread *me = PR_GetCurrentThread();
    PR_ASSERT((NULL != me) && (me->flags & _PR_PRIMORDIAL));
    if ((NULL != me) && (me->flags & _PR_PRIMORDIAL))
    {

        PR_LOG(_pr_thread_lm, PR_LOG_MIN, ("PR_Cleanup: shutting down NSPR"));

        /*
         * No more recycling of threads
         */
        _pr_recycleThreads = 0;

        /*
         * Wait for all other user (non-system/daemon) threads
         * to terminate.
         */
        PR_Lock(_pr_activeLock);
        while (_pr_userActive > _pr_primordialExitCount) {
            PR_WaitCondVar(_pr_primordialExitCVar, PR_INTERVAL_NO_TIMEOUT);
        }
        PR_Unlock(_pr_activeLock);

#if defined(WIN16)
		_PR_ShutdownLinker();
#endif
        /* Release the primordial thread's private data, etc. */
        _PR_CleanupThread(me);

        _PR_MD_STOP_INTERRUPTS();

	    PR_LOG(_pr_thread_lm, PR_LOG_MIN,
	            ("PR_Cleanup: clean up before destroying thread"));
	#ifdef PR_LOGGING
	    _PR_LogCleanup();
	#endif

        /*
         * This part should look like the end of _PR_NativeRunThread
         * and _PR_UserRunThread.
         */
        if (_PR_IS_NATIVE_THREAD(me)) {
            _PR_MD_EXIT_THREAD(me);
            _PR_NativeDestroyThread(me);
        } else {
            _PR_UserDestroyThread(me);
            PR_DELETE(me->stack);
            PR_DELETE(me);
        }

        /*
         * XXX: We are freeing the heap memory here so that Purify won't
         * complain, but we should also free other kinds of resources
         * that are allocated by the _PR_InitXXX() functions.
         * Ideally, for each _PR_InitXXX(), there should be a corresponding
         * _PR_XXXCleanup() that we can call here.
         */
        _PR_CleanupBeforeExit();
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}
#endif /* defined(_PR_PTHREADS) */

/*
 *------------------------------------------------------------------------
 * PR_ProcessExit --
 * 
 *   Cause an immediate, nongraceful, forced termination of the process.
 *   It takes a PRIntn argument, which is the exit status code of the
 *   process.
 *   
 * See also: PR_Cleanup()
 *
 *------------------------------------------------------------------------
 */

#if defined(_PR_PTHREADS)
    /* see ptthread.c */
#else
PR_IMPLEMENT(void) PR_ProcessExit(PRIntn status)
{
    _PR_MD_EXIT(status);
}

#endif /* defined(_PR_PTHREADS) */

PR_IMPLEMENT(PRProcessAttr *)
PR_NewProcessAttr(void)
{
    PRProcessAttr *attr;

    attr = PR_NEWZAP(PRProcessAttr);
    if (!attr) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return attr;
}

PR_IMPLEMENT(void)
PR_ResetProcessAttr(PRProcessAttr *attr)
{
    memset(attr, 0, sizeof(*attr));
}

PR_IMPLEMENT(void)
PR_DestroyProcessAttr(PRProcessAttr *attr)
{
    PR_DELETE(attr);
}

PR_IMPLEMENT(void)
PR_SetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd
)
{
    switch (stdioFd) {
        case PR_StandardInput:
            attr->stdinFd = redirectFd;
            break;
        case PR_StandardOutput:
            attr->stdoutFd = redirectFd;
            break;
        case PR_StandardError:
            attr->stderrFd = redirectFd;
            break;
        default:
            PR_ASSERT(0);
    }
}

PR_IMPLEMENT(PRProcess*) PR_CreateProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    return _PR_MD_CREATE_PROCESS(path, argv, envp, attr);
}  /* PR_CreateProcess */

PR_IMPLEMENT(PRStatus) PR_CreateProcessDetached(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    PRProcess *process;
    PRStatus rv;

    process = PR_CreateProcess(path, argv, envp, attr);
    if (NULL == process) {
	return PR_FAILURE;
    }
    rv = PR_DetachProcess(process);
    PR_ASSERT(PR_SUCCESS == rv);
    if (rv == PR_FAILURE) {
	PR_DELETE(process);
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus) PR_DetachProcess(PRProcess *process)
{
    return _PR_MD_DETACH_PROCESS(process);
}

PR_IMPLEMENT(PRStatus) PR_WaitProcess(PRProcess *process, PRInt32 *exitCode)
{
    return _PR_MD_WAIT_PROCESS(process, exitCode);
}  /* PR_WaitProcess */

PR_IMPLEMENT(PRStatus) PR_KillProcess(PRProcess *process)
{
    return _PR_MD_KILL_PROCESS(process);
}

/*
 ********************************************************************
 *
 * Module initialization
 *
 ********************************************************************
 */

static struct {
    PRLock *ml;
    PRCondVar *cv;
} mod_init;

static void _PR_InitCallOnce() {
    mod_init.ml = PR_NewLock();
    PR_ASSERT(NULL != mod_init.ml);
    mod_init.cv = PR_NewCondVar(mod_init.ml);
    PR_ASSERT(NULL != mod_init.cv);
}


PR_IMPLEMENT(PRStatus) PR_CallOnce(
    PRCallOnceType *once,
    PRCallOnceFN    func)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (!once->initialized) {
	if (PR_AtomicSet(&once->inProgress, 1) == 0) {
	    once->status = (*func)();
	    PR_Lock(mod_init.ml);
	    once->initialized = 1;
	    PR_NotifyAllCondVar(mod_init.cv);
	    PR_Unlock(mod_init.ml);
	} else {
	    PR_Lock(mod_init.ml);
	    while (!once->initialized) {
		PR_WaitCondVar(mod_init.cv, PR_INTERVAL_NO_TIMEOUT);
            }
	    PR_Unlock(mod_init.ml);
	}
    }
    return once->status;
}

PRBool _PR_Obsolete(const char *obsolete, const char *preferred)
{
#if defined(DEBUG)
#ifndef XP_MAC
    PR_fprintf(
        PR_STDERR, "'%s' is obsolete. Use '%s' instead.\n",
        obsolete, (NULL == preferred) ? "something else" : preferred);
    return PR_FALSE;
#else
#pragma unused (obsolete, preferred)
    return PR_FALSE;
#endif
#endif
}  /* _PR_Obsolete */

/* prinit.c */


