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

/*
** File:            ptthread.c
** Descritpion:        Implemenation for threds using pthreds
** Exports:            ptthread.h
*/

#if defined(_PR_PTHREADS) || defined(_PR_DCETHREADS)

#include "prlog.h"
#include "primpl.h"
#include "prpdce.h"

#include <pthread.h>
#include <string.h>
#include <signal.h>

/*
 * Record whether or not we have the privilege to set the scheduling
 * policy and priority of threads.  0 means that privilege is available.
 * EPERM means that privilege is not available.
 */

PRIntn pt_schedpriv;
extern PRLock *_pr_sleeplock;

struct _PT_Bookeeping pt_book = {0};

static void init_pthread_gc_support(void);

static PRIntn pt_PriorityMap(PRThreadPriority pri)
{
    return pt_book.minPrio +
	    pri * (pt_book.maxPrio - pt_book.minPrio) / PR_PRIORITY_LAST;
}

/*
** Initialize a stack for a native pthread thread
*/
static void _PR_InitializeStack(PRThreadStack *ts)
{
    if( ts && (ts->stackTop == 0) ) {
        ts->allocBase = (char *) &ts;
        ts->allocSize = ts->stackSize;

        /*
        ** Setup stackTop and stackBottom values.
        */
#ifdef HAVE_STACK_GROWING_UP
        ts->stackBottom = ts->allocBase + ts->stackSize;
        ts->stackTop = ts->allocBase;
#else
        ts->stackTop    = ts->allocBase;
        ts->stackBottom = ts->allocBase - ts->stackSize;
#endif
    }
}

static void *_pt_root(void *arg)
{
    PRIntn rv;
    PRThread *thred = (PRThread*)arg;
    PRBool detached = (thred->state & PT_THREAD_DETACHED) ? PR_TRUE : PR_FALSE;

    /*
     * Both the parent thread and this new thread set thred->id.
     * The new thread must ensure that thred->id is set before
     * it executes its startFunc.  The parent thread must ensure
     * that thred->id is set before PR_CreateThread() returns.
     * Both threads set thred->id without holding a lock.  Since
     * they are writing the same value, this unprotected double
     * write should be safe.
     */
    thred->id = pthread_self();

    /*
    ** DCE Threads can't detach during creation, so do it late.
    ** I would like to do it only here, but that doesn't seem
    ** to work.
    */
#if defined(_PR_DCETHREADS)
    if (detached)
    {
        /* pthread_detach() modifies its argument, so we must pass a copy */
        pthread_t self = thred->id;
        rv = pthread_detach(&self);
        PR_ASSERT(0 == rv);
    }
#endif /* defined(_PR_DCETHREADS) */

    /* Set up the thread stack information */
    _PR_InitializeStack(thred->stack);

    /*
     * Set within the current thread the pointer to our object.
     * This object will be deleted when the thread termintates,
     * whether in a join or detached (see _PR_InitThreads()).
     */
    rv = pthread_setspecific(pt_book.key, thred);
    PR_ASSERT(0 == rv);

    /* make the thread visible to the rest of the runtime */
    PR_Lock(pt_book.ml);

    /* If this is a GCABLE thread, set its state appropriately */
    if (thred->suspend & PT_THREAD_SETGCABLE)
	    thred->state |= PT_THREAD_GCABLE;
    thred->suspend = 0;

    thred->prev = pt_book.last;
    pt_book.last->next = thred;
    thred->next = NULL;
    pt_book.last = thred;
    PR_Unlock(pt_book.ml);

    thred->startFunc(thred->arg);  /* make visible to the client */

    /* unhook the thread from the runtime */
    PR_Lock(pt_book.ml);
    if (thred->state & PT_THREAD_SYSTEM)
        pt_book.system -= 1;
    else if (--pt_book.user == pt_book.this_many)
        PR_NotifyAllCondVar(pt_book.cv);
    thred->prev->next = thred->next;
    if (NULL == thred->next)
        pt_book.last = thred->prev;
    else
        thred->next->prev = thred->prev;

    /*
     * At this moment, PR_CreateThread() may not have set thred->id yet.
     * It is safe for a detached thread to free thred only after
     * PR_CreateThread() has set thred->id.
     */
    if (detached)
    {
        while (!thred->okToDelete)
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(pt_book.ml);

    /* last chance to delete this puppy if the thread is detached */
    if (detached)
    {
        if (NULL != thred->io_cv)
            PR_DestroyCondVar(thred->io_cv);
    	PR_DELETE(thred->stack);
#if defined(DEBUG)
        memset(thred, 0xaf, sizeof(PRThread));
#endif
        PR_DELETE(thred);
    }

    rv = pthread_setspecific(pt_book.key, NULL);
    PR_ASSERT(0 == rv);
    return NULL;
}  /* _pt_root */

static PRThread* _PR_CreateThread(
    PRThreadType type, void (*start)(void *arg),
    void *arg, PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize, PRBool isGCAble)
{
    int rv;
    PRThread *thred;
    pthread_attr_t tattr;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)priority)
        priority = PR_PRIORITY_FIRST;
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)priority)
        priority = PR_PRIORITY_LAST;

    rv = PTHREAD_ATTR_INIT(&tattr);
    PR_ASSERT(0 == rv);

    if (EPERM != pt_schedpriv)
    {
#if !defined(_PR_DCETHREADS) && !defined(FREEBSD)
        struct sched_param schedule;
#endif

#if !defined(FREEBSD)
        rv = pthread_attr_setinheritsched(&tattr, PTHREAD_EXPLICIT_SCHED);
        PR_ASSERT(0 == rv);
#endif

        /* Use the default scheduling policy */

#if defined(_PR_DCETHREADS)
        rv = pthread_attr_setprio(&tattr, pt_PriorityMap(priority));
        PR_ASSERT(0 == rv);
#elif !defined(FREEBSD)
        rv = pthread_attr_getschedparam(&tattr, &schedule);
        PR_ASSERT(0 == rv);
        schedule.sched_priority = pt_PriorityMap(priority);
        rv = pthread_attr_setschedparam(&tattr, &schedule);
        PR_ASSERT(0 == rv);
#endif /* !defined(_PR_DCETHREADS) */
    }

    /*
     * DCE threads can't set detach state before creating the thread.
     * AIX can't set detach late. Why can't we all just get along?
     */
#if !defined(_PR_DCETHREADS)
    rv = pthread_attr_setdetachstate(&tattr,
        ((PR_JOINABLE_THREAD == state) ?
            PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));
    PR_ASSERT(0 == rv);
#endif /* !defined(_PR_DCETHREADS) */

    /*
     * HPUX only supports PTHREAD_SCOPE_SYSTEM.
     * IRIX only supports PTHREAD_SCOPE_PROCESS.
     * OSF1 only supports PTHREAD_SCOPE_PROCESS.
     * AIX only supports PTHREAD_SCOPE_SYSTEM.
     */
#if defined(SOLARIS)
    rv = pthread_attr_setscope(&tattr,
	((PR_GLOBAL_THREAD == scope) ?
            PTHREAD_SCOPE_SYSTEM : PTHREAD_SCOPE_PROCESS));
    PR_ASSERT(0 == rv);
#endif

#if defined(IRIX)
    if ((16 * 1024) > stackSize) stackSize = (16 * 1024);  /* IRIX minimum */
    else
#endif
    if (0 == stackSize) stackSize = (64 * 1024);  /* default == 64K */
    /*
     * Linux doesn't have pthread_attr_setstacksize.
     */
#ifndef LINUX
    rv = pthread_attr_setstacksize(&tattr, stackSize);
    PR_ASSERT(0 == rv);
#endif

    thred = PR_NEWZAP(PRThread);
    if (thred != NULL)
    {
        pthread_t id;

        thred->arg = arg;
        thred->startFunc = start;
        thred->priority = priority;
        if (PR_UNJOINABLE_THREAD == state)
            thred->state |= PT_THREAD_DETACHED;
        if (PR_GLOBAL_THREAD == scope)
            thred->state |= PT_THREAD_GLOBAL;
        if (PR_SYSTEM_THREAD == type)
            thred->state |= PT_THREAD_SYSTEM;

        thred->suspend =(isGCAble) ? PT_THREAD_SETGCABLE : 0;

        thred->stack = PR_NEWZAP(PRThreadStack);
        if (thred->stack == NULL) {
            PR_DELETE(thred);  /* all that work ... poof! */
            thred = NULL;  /* and for what? */
            goto done;
        }
        thred->stack->stackSize = stackSize;
        thred->stack->thr = thred;

#ifdef PT_NO_SIGTIMEDWAIT
      pthread_mutex_init(&thred->suspendResumeMutex,NULL);
      pthread_cond_init(&thred->suspendResumeCV,NULL);
#endif

	    /* make the thread counted to the rest of the runtime */
	    PR_Lock(pt_book.ml);
	    if (thred->state & PT_THREAD_SYSTEM)
	        pt_book.system += 1;
	    else pt_book.user += 1;
	    PR_Unlock(pt_book.ml);

        /*
         * We pass a pointer to a local copy (instead of thred->id)
         * to pthread_create() because who knows what wacky things
         * pthread_create() may be doing to its argument.
         */
        if (PTHREAD_CREATE(&id, tattr, _pt_root, thred) != 0)
        {
            PR_Lock(pt_book.ml);
            if (thred->state & PT_THREAD_SYSTEM)
                pt_book.system -= 1;
            else if (--pt_book.user == pt_book.this_many)
                PR_NotifyAllCondVar(pt_book.cv);
            PR_Unlock(pt_book.ml);

            PR_DELETE(thred->stack);
            PR_DELETE(thred);  /* all that work ... poof! */
            thred = NULL;  /* and for what? */
            goto done;
        }

        /*
         * Both the parent thread and this new thread set thred->id.
         * The parent thread must ensure that thred->id is set before
         * PR_CreateThread() returns.  (See comments in _pt_root().)
         */
        thred->id = id;

        /*
         * If the new thread is detached, tell it that PR_CreateThread()
         * has set thred->id so it's ok to delete thred.
         */
        if (PR_UNJOINABLE_THREAD == state)
        {
            PR_Lock(pt_book.ml);
            thred->okToDelete = PR_TRUE;
            PR_NotifyAllCondVar(pt_book.cv);
            PR_Unlock(pt_book.ml);
        }
    }

done:
    rv = PTHREAD_ATTR_DESTROY(&tattr);
    PR_ASSERT(0 == rv);

    return thred;
}  /* _PR_CreateThread */

PR_IMPLEMENT(PRThread*) PR_CreateThread(
    PRThreadType type, void (*start)(void *arg), void *arg,
    PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize)
{
    return _PR_CreateThread(
        type, start, arg, priority, scope, state, stackSize, PR_FALSE);
} /* PR_CreateThread */

PR_IMPLEMENT(PRThread*) PR_CreateThreadGCAble(
    PRThreadType type, void (*start)(void *arg), void *arg, 
    PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize)
{
    return _PR_CreateThread(
        type, start, arg, priority, scope, state, stackSize, PR_TRUE);
}  /* PR_CreateThreadGCAble */

PR_IMPLEMENT(void*) GetExecutionEnvironment(PRThread *thred)
{
    return thred->environment;
}  /* GetExecutionEnvironment */
 
PR_IMPLEMENT(void) SetExecutionEnvironment(PRThread *thred, void *env)
{
    thred->environment = env;
}  /* SetExecutionEnvironment */

PR_IMPLEMENT(PRThread*) PR_AttachThread(
    PRThreadType type, PRThreadPriority priority, PRThreadStack *stack)
{
    PRThread *thred = NULL;
    void *privateData = NULL;

    /*
     * NSPR must have been initialized when PR_AttachThread is called.
     * We cannot have PR_AttachThread call implicit initialization
     * because if multiple threads call PR_AttachThread simultaneously,
     * NSPR may be initialized more than once.
     */
    if (!_pr_initialized) {
        /* Since NSPR is not initialized, we cannot call PR_SetError. */
        return NULL;
    }

    PTHREAD_GETSPECIFIC(pt_book.key, privateData);
    if (NULL != privateData)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }
    thred = PR_NEWZAP(PRThread);
    if (NULL == thred) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        int rv;

        if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)priority)
            priority = PR_PRIORITY_FIRST;
        else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)priority)
            priority = PR_PRIORITY_LAST;

        thred->priority = priority;
        thred->id = pthread_self();
        rv = pthread_setspecific(pt_book.key, thred);
        PR_ASSERT(0 == rv);

        PR_Lock(pt_book.ml);
        if (PR_SYSTEM_THREAD == type)
        {
            pt_book.system += 1;
            thred->state |= PT_THREAD_SYSTEM;
        }
        else pt_book.user += 1;

        /* then put it into the list */
        thred->prev = pt_book.last;
	    pt_book.last->next = thred;
        thred->next = NULL;
        pt_book.last = thred;
        PR_Unlock(pt_book.ml);

    }
    return thred;  /* may be NULL */
}  /* PR_AttachThread */

PR_IMPLEMENT(PRStatus) PR_JoinThread(PRThread *thred)
{
    int rv = -1;
    void *result = NULL;
    PR_ASSERT(thred != NULL);

    if ((0xafafafaf == thred->state)
    || (PT_THREAD_DETACHED & thred->state))
    {
        /*
         * This might be a bad address, but if it isn't, the state should
         * either be an unjoinable thread or it's already had the object
         * deleted. However, the client that called join on a detached
         * thread deserves all the rath I can muster....
         */
        PR_SetError(PR_ILLEGAL_ACCESS_ERROR, 0);
        PR_LogPrint(
            "PR_JoinThread: 0x%X not joinable | already smashed\n", thred);

        PR_ASSERT((0xafafafaf == thred->state)
        || (PT_THREAD_DETACHED & thred->state));
    }
    else
    {
        pthread_t id = thred->id;
        rv = pthread_join(id, &result);
        PR_ASSERT(rv == 0 && result == NULL);
        if (0 != rv)
            PR_SetError(PR_UNKNOWN_ERROR, errno);
        if (NULL != thred->io_cv)
            PR_DestroyCondVar(thred->io_cv);
    	PR_DELETE(thred->stack);
#if defined(DEBUG)
        memset(thred, 0xaf, sizeof(PRThread));
#endif
        PR_DELETE(thred);
    }
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_JoinThread */

PR_IMPLEMENT(void) PR_DetachThread()
{
    void *tmp;
    PTHREAD_GETSPECIFIC(pt_book.key, tmp);
    PR_ASSERT(NULL != tmp);

    if (NULL != tmp)
    {
        int rv;
        PRThread *thred = (PRThread*)tmp;

        PR_Lock(pt_book.ml);
        if (thred->state & PT_THREAD_SYSTEM)
            pt_book.system -= 1;
        else if (--pt_book.user == pt_book.this_many)
            PR_NotifyAllCondVar(pt_book.cv);
        thred->prev->next = thred->next;
        if (NULL == thred->next)
            pt_book.last = thred->prev;
        else
            thred->next->prev = thred->prev;
        PR_Unlock(pt_book.ml);

        if (NULL != thred->io_cv)
            PR_DestroyCondVar(thred->io_cv);
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
    	PR_DELETE(thred->stack);
#if defined(DEBUG)
        memset(thred, 0xaf, sizeof(PRThread));
#endif
        PR_DELETE(thred);
    }    
}  /* PR_DetachThread */

PR_IMPLEMENT(PRThread*) PR_GetCurrentThread()
{
    void *thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    PTHREAD_GETSPECIFIC(pt_book.key, thred);
    PR_ASSERT(NULL != thred);
    return (PRThread*)thred;
}  /* PR_GetCurrentThread */

PR_IMPLEMENT(PRThreadScope) PR_GetThreadScope(const PRThread *thred)
{
    return (thred->state & PT_THREAD_GLOBAL) ?
        PR_GLOBAL_THREAD : PR_LOCAL_THREAD;
}  /* PR_GetThreadScope() */

PR_IMPLEMENT(PRThreadType) PR_GetThreadType(const PRThread *thred)
{
    return (thred->state & PT_THREAD_SYSTEM) ?
        PR_SYSTEM_THREAD : PR_USER_THREAD;
}

PR_IMPLEMENT(PRThreadState) PR_GetThreadState(const PRThread *thred)
{
    return (thred->state & PT_THREAD_DETACHED) ?
        PR_UNJOINABLE_THREAD : PR_JOINABLE_THREAD;
}  /* PR_GetThreadState */

PR_IMPLEMENT(PRThreadPriority) PR_GetThreadPriority(const PRThread *thred)
{
    PR_ASSERT(thred != NULL);
    return thred->priority;
}  /* PR_GetThreadPriority */

PR_IMPLEMENT(void) PR_SetThreadPriority(PRThread *thred, PRThreadPriority newPri)
{
    PRIntn rv;

    PR_ASSERT(NULL != thred);

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)newPri)
        newPri = PR_PRIORITY_FIRST;
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)newPri)
        newPri = PR_PRIORITY_LAST;

#if defined(_PR_DCETHREADS)
    rv = pthread_setprio(thred->id, pt_PriorityMap(newPri));
    /* pthread_setprio returns the old priority */
    PR_ASSERT(-1 != rv);
#elif !defined(FREEBSD)
    if (EPERM != pt_schedpriv)
    {
        int policy;
        struct sched_param schedule;

        rv = pthread_getschedparam(thred->id, &policy, &schedule);
        PR_ASSERT(0 == rv);
        schedule.sched_priority = pt_PriorityMap(newPri);
        rv = pthread_setschedparam(thred->id, policy, &schedule);
        PR_ASSERT(0 == rv);
    }
#endif

    thred->priority = newPri;
}  /* PR_SetThreadPriority */

PR_IMPLEMENT(PRStatus) PR_NewThreadPrivateIndex(
    PRUintn *newIndex, PRThreadPrivateDTOR destructor)
{
    int rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    rv = PTHREAD_KEY_CREATE((pthread_key_t*)newIndex, destructor);

    if (0 == rv)
    {
        PR_Lock(pt_book.ml);
        if (*newIndex >= pt_book.highwater)
            pt_book.highwater = *newIndex + 1;
        PR_Unlock(pt_book.ml);
        return PR_SUCCESS;
    }
    PR_SetError(PR_UNKNOWN_ERROR, rv);
    return PR_FAILURE;
}  /* PR_NewThreadPrivateIndex */

PR_IMPLEMENT(PRStatus) PR_SetThreadPrivate(PRUintn index, void *priv)
{
    PRIntn rv;
    if ((pthread_key_t)index >= pt_book.highwater)
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
        return PR_FAILURE;
    }
    rv = pthread_setspecific((pthread_key_t)index, priv);
    PR_ASSERT(0 == rv);
    if (0 == rv) return PR_SUCCESS;

    PR_SetError(PR_UNKNOWN_ERROR, rv);
    return PR_FAILURE;
}  /* PR_SetThreadPrivate */

PR_IMPLEMENT(void*) PR_GetThreadPrivate(PRUintn index)
{
    void *result = NULL;
    if ((pthread_key_t)index < pt_book.highwater)
        PTHREAD_GETSPECIFIC((pthread_key_t)index, result);
    return result;
}  /* PR_GetThreadPrivate */

PR_IMPLEMENT(PRStatus) PR_Interrupt(PRThread *thred)
{
    /*
    ** If the target thread indicates that it's waiting,
    ** find the condition and broadcast to it. Broadcast
    ** since we don't know which thread (if there are more
    ** than one). This sounds risky, but clients must
    ** test their invariants when resumed from a wait and
    ** I don't expect very many threads to be waiting on
    ** a single condition and I don't expect interrupt to
    ** be used very often.
    **
    ** I don't know why I thought this would work. Must have
    ** been one of those weaker momements after I'd been
    ** smelling the vapors.
    **
    ** Even with the followng changes it is possible that
    ** the pointer to the condition variable is pointing
    ** at a bogus value. Will the unerlying code detect
    ** that?
    */
    PRCondVar *cv;
    PR_ASSERT(NULL != thred);
    if (NULL == thred) return PR_FAILURE;

    thred->state |= PT_THREAD_ABORTED;

    cv = thred->waiting;
    if (NULL != cv)
    {
        PRIntn rv = pthread_cond_broadcast(&cv->cv);
        PR_ASSERT(0 == rv);
    }
    return PR_SUCCESS;
}  /* PR_Interrupt */

PR_IMPLEMENT(void) PR_ClearInterrupt()
{
    PRThread *me = PR_CurrentThread();
    me->state &= ~PT_THREAD_ABORTED;
}  /* PR_ClearInterrupt */

PR_IMPLEMENT(PRStatus) PR_Yield()
{
    static PRBool warning = PR_TRUE;
    if (warning) warning = _PR_Obsolete(
        "PR_Yield()", "PR_Sleep(PR_INTERVAL_NO_WAIT)");
    return PR_Sleep(PR_INTERVAL_NO_WAIT);
}

PR_IMPLEMENT(PRStatus) PR_Sleep(PRIntervalTime ticks)
{
    PRStatus rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (PR_INTERVAL_NO_WAIT == ticks)
    {
        PTHREAD_YIELD();
        rv = PR_SUCCESS;
    }
    else
    {
        PRCondVar *cv = PR_NewCondVar(_pr_sleeplock);
        PR_ASSERT(cv != NULL);
        PR_Lock(_pr_sleeplock);
        rv = PR_WaitCondVar(cv, ticks);
        PR_Unlock(_pr_sleeplock);
        PR_DestroyCondVar(cv);
    }
    return rv;
}  /* PR_Sleep */

void _PR_InitThreads(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
    int rv;
    PRThread *thred;

    /*
    ** These might be function evaluations
    */
    pt_book.minPrio = PT_PRIO_MIN;
    pt_book.maxPrio = PT_PRIO_MAX;
    
    PR_ASSERT(NULL == pt_book.ml);
    pt_book.ml = PR_NewLock();
    PR_ASSERT(NULL != pt_book.ml);
    pt_book.cv = PR_NewCondVar(pt_book.ml);
    PR_ASSERT(NULL != pt_book.cv);
    thred = PR_NEWZAP(PRThread);
    PR_ASSERT(NULL != thred);
    thred->arg = NULL;
    thred->startFunc = NULL;
    thred->priority = priority;
    thred->id = pthread_self();

    thred->state |= (PT_THREAD_DETACHED | PT_THREAD_PRIMORD);
    if (PR_SYSTEM_THREAD == type)
    {
        thred->state |= PT_THREAD_SYSTEM;
        pt_book.system += 1;
	    pt_book.this_many = 0;
    }
    else
    {
	    pt_book.user += 1;
	    pt_book.this_many = 1;
    }
    thred->next = thred->prev = NULL;
    pt_book.first = pt_book.last = thred;

    thred->stack = PR_NEWZAP(PRThreadStack);
    PR_ASSERT(thred->stack != NULL);
    thred->stack->stackSize = 0;
    thred->stack->thr = thred;
	_PR_InitializeStack(thred->stack);

    /*
     * Create a key for our use to store a backpointer in the pthread
     * to our PRThread object. This object gets deleted when the thread
     * returns from its root in the case of a detached thread. Other
     * threads delete the objects in Join.
     *
     * NB: The destructor logic seems to have a bug so it isn't used.
     */
    rv = PTHREAD_KEY_CREATE(&pt_book.key, NULL);
    PR_ASSERT(0 == rv);
    rv = pthread_setspecific(pt_book.key, thred);
    PR_ASSERT(0 == rv);
    
    pt_schedpriv = PT_PRIVCHECK();
    PR_SetThreadPriority(thred, priority);

    /*
     * Linux pthreads use SIGUSR1 and SIGUSR2 internally, which
     * conflict with the use of these two signals in our GC support.
     * So we don't know how to support GC on Linux pthreads.
     */
#if !defined(LINUX2_0) && !defined(FREEBSD)
	init_pthread_gc_support();
#endif

}  /* _PR_InitThreads */

PR_IMPLEMENT(PRStatus) PR_Cleanup()
{
    PRThread *me = PR_CurrentThread();
    PR_LOG(_pr_thread_lm, PR_LOG_MIN, ("PR_Cleanup: shutting down NSPR"));
    PR_ASSERT(me->state & PT_THREAD_PRIMORD);
    if (me->state & PT_THREAD_PRIMORD)
    {
        PR_Lock(pt_book.ml);
        while (pt_book.user > pt_book.this_many)
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
        PR_Unlock(pt_book.ml);

        /*
         * I am not sure if it's safe to delete the cv and lock here,
         * since there may still be "system" threads around. If this
         * call isn't immediately prior to exiting, then there's a
         * problem.
         */
        if (0 == pt_book.system)
        {
            PR_DestroyCondVar(pt_book.cv); pt_book.cv = NULL;
            PR_DestroyLock(pt_book.ml); pt_book.ml = NULL;
        }
        PR_DELETE(me->stack);
        PR_DELETE(me);
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}  /* PR_Cleanup */

PR_IMPLEMENT(void) PR_ProcessExit(PRIntn status)
{
    _exit(status);
}

/*
 * $$$
 * The following two thread-to-processor affinity functions are not
 * yet implemented for pthreads.  By the way, these functions should return
 * PRStatus rather than PRInt32 to indicate the success/failure status.
 * $$$
 */

PR_IMPLEMENT(PRInt32) PR_GetThreadAffinityMask(PRThread *thread, PRUint32 *mask)
{
    return 0;  /* not implemented */
}

PR_IMPLEMENT(PRInt32) PR_SetThreadAffinityMask(PRThread *thread, PRUint32 mask )
{
    return 0;  /* not implemented */
}

PR_IMPLEMENT(void)
PR_SetThreadDumpProc(PRThread* thread, PRThreadDumpProc dump, void *arg)
{
    thread->dump = dump;
    thread->dumpArg = arg;
}

/* 
 * Garbage collection support follows.
 */

#if defined(_PR_DCETHREADS)

/*
 * statics for Garbage Collection support.  We don't need to protect these
 * signal masks since the garbage collector itself is protected by a lock
 * and multiple threads will not be garbage collecting at the same time.
 */
static sigset_t javagc_vtalarm_sigmask;
static sigset_t javagc_intsoff_sigmask;

#else /* defined(_PR_DCETHREADS) */

/* a bogus signal mask for forcing a timed wait */
/* Not so bogus in AIX as we really do a sigwait */
static sigset_t sigwait_set;

static struct timespec onemillisec = {0, 1000000L};
static struct timespec hundredmillisec = {0, 100000000L};

#endif /* defined(_PR_DCETHREADS) */

static void suspend_signal_handler(PRIntn sig);

#ifdef PT_NO_SIGTIMEDWAIT
static void null_signal_handler(PRIntn sig);
#endif

static void init_pthread_gc_support()
{
    PRIntn rv;

#if defined(_PR_DCETHREADS)
	rv = sigemptyset(&javagc_vtalarm_sigmask);
    PR_ASSERT(0 == rv);
	rv = sigaddset(&javagc_vtalarm_sigmask, SIGVTALRM);
    PR_ASSERT(0 == rv);
#else  /* defined(_PR_DCETHREADS) */
	{
	    struct sigaction sigact_usr2 = {0};

	    sigact_usr2.sa_handler = suspend_signal_handler;
	    sigact_usr2.sa_flags = SA_RESTART;
	    sigemptyset (&sigact_usr2.sa_mask);

        rv = sigaction (SIGUSR2, &sigact_usr2, NULL);
        PR_ASSERT(0 == rv);

        sigemptyset (&sigwait_set);
#if defined(PT_NO_SIGTIMEDWAIT)
        sigaddset (&sigwait_set, SIGUSR1);
#else
        sigaddset (&sigwait_set, SIGUSR2);
#endif  /* defined(PT_NO_SIGTIMEDWAIT) */
	}
#if defined(PT_NO_SIGTIMEDWAIT)
	{
	    struct sigaction sigact_null = {0};
	    sigact_null.sa_handler = null_signal_handler;
	    sigact_null.sa_flags = SA_RESTART;
	    sigemptyset (&sigact_null.sa_mask);
        rv = sigaction (SIGUSR1, &sigact_null, NULL);
	    PR_ASSERT(0 ==rv); 
    }
#endif  /* defined(PT_NO_SIGTIMEDWAIT) */
#endif /* defined(_PR_DCETHREADS) */
}

PR_IMPLEMENT(void) PR_SetThreadGCAble()
{
    PR_Lock(pt_book.ml);
	PR_CurrentThread()->state |= PT_THREAD_GCABLE;
    PR_Unlock(pt_book.ml);
}

PR_IMPLEMENT(void) PR_ClearThreadGCAble()
{
    PR_Lock(pt_book.ml);
	PR_CurrentThread()->state &= (~PT_THREAD_GCABLE);
    PR_Unlock(pt_book.ml);
}

#if defined(DEBUG)
static PRBool suspendAllOn = PR_FALSE;
#endif

static PRBool suspendAllSuspended = PR_FALSE;

/* Are all GCAble threads (except gc'ing thread) suspended? */
PR_IMPLEMENT(PRBool) PR_SuspendAllSuspended()
{
	return suspendAllSuspended;
} /* PR_SuspendAllSuspended */

PR_IMPLEMENT(PRStatus) PR_EnumerateThreads(PREnumerator func, void *arg)
{
    PRIntn count = 0;
    PRStatus rv = PR_SUCCESS;
    PRThread* thred = pt_book.first;
    PRThread *me = PR_CurrentThread();

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_EnumerateThreads\n"));
    /*
     * $$$
     * Need to suspend all threads other than me before doing this.
     * This is really a gross and disgusting thing to do. The only
     * good thing is that since all other threads are suspended, holding
     * the lock during a callback seems like child's play.
     * $$$
     */
    PR_ASSERT(suspendAllOn);

    while (thred != NULL)
    {
        /* Steve Morse, 4-23-97: Note that we can't walk a queue by taking
         * qp->next after applying the function "func".  In particular, "func"
         * might remove the thread from the queue and put it into another one in
         * which case qp->next no longer points to the next entry in the original
         * queue.
         *
         * To get around this problem, we save qp->next in qp_next before applying
         * "func" and use that saved value as the next value after applying "func".
         */
        PRThread* next = thred->next;

        if (thred->state & PT_THREAD_GCABLE)
        {
#if !defined(_PR_DCETHREADS)
            PR_ASSERT((thred == me) || (thred->suspend & PT_THREAD_SUSPENDED));
#endif
            PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
                   ("In PR_EnumerateThreads callback thread %X thid = %X\n", 
                    thred, thred->id));

            rv = func(thred, count++, arg);
            if (rv != PR_SUCCESS)
                return rv;
        }
        thred = next;
    }
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("End PR_EnumerateThreads count = %d \n", count));
    return rv;
}  /* PR_EnumerateThreads */

/*
 * PR_SuspendAll and PR_ResumeAll are called during garbage collection.  The strategy 
 * we use is to send a SIGUSR2 signal to every gc able thread that we intend to suspend.
 * The signal handler will record the stack pointer and will block until resumed by
 * the resume call.  Since the signal handler is the last routine called for the
 * suspended thread, the stack pointer will also serve as a place where all the
 * registers have been saved on the stack for the previously executing routines.
 *
 * Through global variables, we also make sure that PR_Suspend and PR_Resume does not
 * proceed until the thread is suspended or resumed.
 */

#if !defined(_PR_DCETHREADS)

/*
 * In the signal handler, we can not use condition variable notify or wait.
 * This does not work consistently across all pthread platforms.  We also can not 
 * use locking since that does not seem to work reliably across platforms.
 * Only thing we can do is yielding while testing for a global condition
 * to change.  This does work on pthread supported platforms.  We may have
 * to play with priortities if there are any problems detected.
 */

 /* 
  * In AIX, you cannot use ANY pthread calls in the signal handler except perhaps
  * pthread_yield. But that is horribly inefficient. Hence we use only sigwait, no
  * sigtimedwait is available. We need to use another user signal, SIGUSR1. Actually
  * SIGUSR1 is also used by exec in Java. So our usage here breaks the exec in Java,
  * for AIX. You cannot use pthread_cond_wait or pthread_delay_np in the signal
  * handler as all synchronization mechanisms just break down. 
  */

#if defined(PT_NO_SIGTIMEDWAIT)
static void null_signal_handler(PRIntn sig)
{
	return;
}
#endif

static void suspend_signal_handler(PRIntn sig)
{
	PRThread *me = PR_CurrentThread();

	PR_ASSERT(me != NULL);
	PR_ASSERT(me->state & PT_THREAD_GCABLE);
	PR_ASSERT((me->suspend & PT_THREAD_SUSPENDED) == 0);

	PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
        ("Begin suspend_signal_handler thred %X thread id = %X\n", 
		me, me->id));

	/*
	 * save stack pointer
	 */
	me->sp = &me;

	/* 
	   At this point, the thread's stack pointer has been saved,
	   And it is going to enter a wait loop until it is resumed.
	   So it is _really_ suspended 
	*/

	me->suspend |= PT_THREAD_SUSPENDED;

	/*
	 * now, block current thread
	 */
#if defined(PT_NO_SIGTIMEDWAIT)
	pthread_cond_signal(&me->suspendResumeCV);
	while (me->suspend & PT_THREAD_SUSPENDED)
	{
#if !defined(FREEBSD)  /*XXX*/
        PRIntn rv;
	    sigwait(&sigwait_set, &rv);
#endif
	}
	me->suspend |= PT_THREAD_RESUMED;
	pthread_cond_signal(&me->suspendResumeCV);
#else /* defined(PT_NO_SIGTIMEDWAIT) */
	while (me->suspend & PT_THREAD_SUSPENDED)
	{
		PRIntn rv = sigtimedwait(&sigwait_set, NULL, &hundredmillisec);
    	PR_ASSERT(-1 == rv);
	}
	me->suspend |= PT_THREAD_RESUMED;
#endif

    /*
     * At this point, thread has been resumed, so set a global condition.
     * The ResumeAll needs to know that this has really been resumed. 
     * So the signal handler sets a flag which PR_ResumeAll will reset. 
     * The PR_ResumeAll must reset this flag ...
     */

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
        ("End suspend_signal_handler thred = %X tid = %X\n", me, me->id));
}  /* suspend_signal_handler */

static void PR_SuspendSet(PRThread *thred)
{
    PRIntn rv;

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("PR_SuspendSet thred %X thread id = %X\n", thred, thred->id));


    /*
     * Check the thread state and signal the thread to suspend
     */

    PR_ASSERT((thred->suspend & PT_THREAD_SUSPENDED) == 0);

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("doing pthread_kill in PR_SuspendSet thred %X tid = %X\n",
	   thred, thred->id));
    rv = pthread_kill (thred->id, SIGUSR2);
    PR_ASSERT(0 == rv);
}

static void PR_SuspendTest(PRThread *thred)
{
    PRIntn rv;

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("Begin PR_SuspendTest thred %X thread id = %X\n", thred, thred->id));


    /*
     * Wait for the thread to be really suspended. This happens when the
     * suspend signal handler stores the stack pointer and sets the state
     * to suspended. 
     */

#if defined(PT_NO_SIGTIMEDWAIT)
    pthread_mutex_lock(&thred->suspendResumeMutex);
    while ((thred->suspend & PT_THREAD_SUSPENDED) == 0)
    {
	    pthread_cond_timedwait(
	        &thred->suspendResumeCV, &thred->suspendResumeMutex, &onemillisec);
	}
	pthread_mutex_unlock(&thred->suspendResumeMutex);
#else
    while ((thred->suspend & PT_THREAD_SUSPENDED) == 0)
    {
		rv = sigtimedwait(&sigwait_set, NULL, &onemillisec);
    	PR_ASSERT(-1 == rv);
	}
#endif

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
        ("End PR_SuspendTest thred %X tid %X\n", thred, thred->id));
}  /* PR_SuspendTest */

PR_IMPLEMENT(void) PR_ResumeSet(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("PR_ResumeSet thred %X thread id = %X\n", thred, thred->id));

    /*
     * Clear the global state and set the thread state so that it will
     * continue past yield loop in the suspend signal handler
     */

    PR_ASSERT(thred->suspend & PT_THREAD_SUSPENDED);


    thred->suspend &= ~PT_THREAD_SUSPENDED;

#if defined(PT_NO_SIGTIMEDWAIT)
	pthread_kill(thred->id, SIGUSR1);
#endif

}  /* PR_ResumeSet */

PR_IMPLEMENT(void) PR_ResumeTest(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	   ("Begin PR_ResumeTest thred %X thread id = %X\n", thred, thred->id));

    /*
     * Wait for the threads resume state to change
     * to indicate it is really resumed 
     */
#if defined(PT_NO_SIGTIMEDWAIT)
    pthread_mutex_lock(&thred->suspendResumeMutex);
    while ((thred->suspend & PT_THREAD_RESUMED) == 0)
    {
	    pthread_cond_timedwait(
	        &thred->suspendResumeCV, &thred->suspendResumeMutex, &onemillisec);
    }
    pthread_mutex_unlock(&thred->suspendResumeMutex);
#else
    while ((thred->suspend & PT_THREAD_RESUMED) == 0) {
		PRIntn rv = sigtimedwait(&sigwait_set, NULL, &onemillisec);
    	PR_ASSERT(-1 == rv);
	}
#endif

    thred->suspend &= ~PT_THREAD_RESUMED;

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, (
        "End PR_ResumeTest thred %X tid %X\n", thred, thred->id));
}  /* PR_ResumeTest */

PR_IMPLEMENT(void) PR_SuspendAll()
{
#ifdef DEBUG
    PRIntervalTime stime, etime;
#endif
    PRThread* thred = pt_book.first;
    PRThread *me = PR_CurrentThread();
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_SuspendAll\n"));
    /*
     * Stop all threads which are marked GC able.
     */
    PR_Lock(pt_book.ml);
#ifdef DEBUG
    suspendAllOn = PR_TRUE;
    stime = PR_IntervalNow();
#endif
    while (thred != NULL)
    {
	    if ((thred != me) && (thred->state & PT_THREAD_GCABLE))
    		PR_SuspendSet(thred);
        thred = thred->next;
    }

    /* Wait till they are really suspended */
    thred = pt_book.first;
    while (thred != NULL)
    {
	    if ((thred != me) && (thred->state & PT_THREAD_GCABLE))
            PR_SuspendTest(thred);
        thred = thred->next;
    }

    suspendAllSuspended = PR_TRUE;

#ifdef DEBUG
    etime = PR_IntervalNow();
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,\
        ("End PR_SuspendAll (time %dms)\n",
        PR_IntervalToMilliseconds(etime - stime)));
#endif
}  /* PR_SuspendAll */

PR_IMPLEMENT(void) PR_ResumeAll()
{
#ifdef DEBUG
    PRIntervalTime stime, etime;
#endif
    PRThread* thred = pt_book.first;
    PRThread *me = PR_CurrentThread();
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_ResumeAll\n"));
    /*
     * Resume all previously suspended GC able threads.
     */
    suspendAllSuspended = PR_FALSE;
#ifdef DEBUG
    stime = PR_IntervalNow();
#endif

    while (thred != NULL)
    {
	    if ((thred != me) && (thred->state & PT_THREAD_GCABLE))
    	    PR_ResumeSet(thred);
        thred = thred->next;
    }

    thred = pt_book.first;
    while (thred != NULL)
    {
	    if ((thred != me) && (thred->state & PT_THREAD_GCABLE))
    	    PR_ResumeTest(thred);
        thred = thred->next;
    }

    PR_Unlock(pt_book.ml);
#ifdef DEBUG
    suspendAllOn = PR_FALSE;
    etime = PR_IntervalNow();
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
        ("End PR_ResumeAll (time %dms)\n",
        PR_IntervalToMilliseconds(etime - stime)));
#endif
}  /* PR_ResumeAll */

/* Return the stack pointer for the given thread- used by the GC */
PR_IMPLEMENT(void *)PR_GetSP(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, 
	    ("in PR_GetSP thred %X thid = %X, sp = %X \n", 
	    thred, thred->id, thred->sp));
    return thred->sp;
}  /* PR_GetSP */

#else /* !defined(_PR_DCETHREADS) */

/*
 * For DCE threads, there is no pthread_kill or a way of suspending or resuming a
 * particular thread.  We will just disable the preemption (virtual timer alarm) and
 * let the executing thread finish the garbage collection.  This stops all other threads
 * (GC able or not) and is very inefficient but there is no other choice.
 */
PR_IMPLEMENT(void) PR_SuspendAll()
{
    PRIntn rv;

#ifdef DEBUG
    suspendAllOn = PR_TRUE;
#endif
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_SuspendAll\n"));
    /* 
     * turn off preemption - i.e add virtual alarm signal to the set of 
     * blocking signals 
     */
    rv = sigprocmask(
        SIG_BLOCK, &javagc_vtalarm_sigmask, &javagc_intsoff_sigmask);
    PR_ASSERT(0 == rv);
    suspendAllSuspended = PR_TRUE;
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("End PR_SuspendAll\n"));
}  /* PR_SuspendAll */

PR_IMPLEMENT(void) PR_ResumeAll()
{
    PRIntn rv;
    
    suspendAllSuspended = PR_FALSE;
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_ResumeAll\n"));
    /* turn on preemption - i.e re-enable virtual alarm signal */

    rv = sigprocmask(SIG_SETMASK, &javagc_intsoff_sigmask, (sigset_t *)NULL);
    PR_ASSERT(0 == rv);
#ifdef DEBUG
    suspendAllOn = PR_FALSE;
#endif

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("End PR_ResumeAll\n"));
}  /* PR_ResumeAll */

/* Return the stack pointer for the given thread- used by the GC */
PR_IMPLEMENT(void*)PR_GetSP(PRThread *thred)
{
	pthread_t tid = thred->id;
	char *thread_tcb, *top_sp;

	/*
	 * For HPUX DCE threads, pthread_t is a struct with the
	 * following three fields (see pthread.h, dce/cma.h):
	 *     cma_t_address       field1;
	 *     short int           field2;
	 *     short int           field3;
	 * where cma_t_address is typedef'd to be either void*
	 * or char*.
	 */
	PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("Begin PR_GetSP\n"));
	thread_tcb = (char*)tid.field1;
	top_sp = *(char**)(thread_tcb + 128);
	PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS, ("End PR_GetSP %X \n", top_sp));
	return top_sp;
}  /* PR_GetSP */

#endif /* !defined(_PR_DCETHREADS) */

#endif  /* defined(_PR_PTHREADS) || defined(_PR_DCETHREADS) */

/* ptthread.c */
