/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:            ptthread.c
** Descritpion:        Implemenation for threds using pthreds
** Exports:            ptthread.h
*/

#if defined(_PR_PTHREADS)

#include "prlog.h"
#include "primpl.h"
#include "prpdce.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>

#if defined(OPENBSD) || defined(FREEBSD) || defined(DRAGONFLY)
#include <pthread_np.h>
#endif

#if defined(ANDROID)
#include <sys/prctl.h>
#endif

#ifdef _PR_NICE_PRIORITY_SCHEDULING
#undef _POSIX_THREAD_PRIORITY_SCHEDULING
#include <sys/resource.h>
#ifndef HAVE_GETTID
#define gettid() (syscall(SYS_gettid))
#endif
#endif

/*
 * Record whether or not we have the privilege to set the scheduling
 * policy and priority of threads.  0 means that privilege is available.
 * EPERM means that privilege is not available.
 */

static PRIntn pt_schedpriv = 0;
extern PRLock *_pr_sleeplock;

static struct _PT_Bookeeping
{
    PRLock *ml;                 /* a lock to protect ourselves */
    PRCondVar *cv;              /* used to signal global things */
    PRInt32 system, user;       /* a count of the two different types */
    PRUintn this_many;          /* number of threads allowed for exit */
    pthread_key_t key;          /* thread private data key */
    PRBool keyCreated;          /* whether 'key' should be deleted */
    PRThread *first, *last;     /* list of threads we know about */
#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    PRInt32 minPrio, maxPrio;   /* range of scheduling priorities */
#endif
} pt_book = {0};

static void _pt_thread_death(void *arg);
static void _pt_thread_death_internal(void *arg, PRBool callDestructors);
static void init_pthread_gc_support(void);

#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
static PRIntn pt_PriorityMap(PRThreadPriority pri)
{
#ifdef NTO
    /* This priority algorithm causes lots of problems on Neutrino
     * for now I have just hard coded everything to run at priority 10
     * until I can come up with a new algorithm.
     *     Jerry.Kirk@Nexwarecorp.com
     */
    return 10;
#else
    return pt_book.minPrio +
           pri * (pt_book.maxPrio - pt_book.minPrio) / PR_PRIORITY_LAST;
#endif
}
#elif defined(_PR_NICE_PRIORITY_SCHEDULING)
/*
 * This functions maps higher priorities to lower nice values relative to the
 * nice value specified in the |nice| parameter. The corresponding relative
 * adjustments are:
 *
 * PR_PRIORITY_LOW    +1
 * PR_PRIORITY_NORMAL  0
 * PR_PRIORITY_HIGH   -1
 * PR_PRIORITY_URGENT -2
 */
static int pt_RelativePriority(int nice, PRThreadPriority pri)
{
    return nice + (1 - pri);
}
#endif

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
    pthread_t id = pthread_self();
#ifdef _PR_NICE_PRIORITY_SCHEDULING
    pid_t tid;
#endif

#ifdef _PR_NICE_PRIORITY_SCHEDULING
    /*
     * We need to know the kernel thread ID of each thread in order to
     * set its nice value hence we do it here instead of at creation time.
     */
    tid = gettid();
    errno = 0;
    rv = getpriority(PRIO_PROCESS, 0);

    /* If we cannot read the main thread's nice value don't try to change the
     * new thread's nice value. */
    if (errno == 0) {
        setpriority(PRIO_PROCESS, tid,
                    pt_RelativePriority(rv, thred->priority));
    }
#endif

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
    /*
     * Both the parent thread and this new thread set thred->id.
     * The new thread must ensure that thred->id is set before
     * it executes its startFunc.  The parent thread must ensure
     * that thred->id is set before PR_CreateThread() returns.
     * Both threads set thred->id while holding pt_book.ml and
     * use thred->idSet to ensure thred->id is written only once.
     */
    if (!thred->idSet)
    {
        thred->id = id;
        thred->idSet = PR_TRUE;
    }
    else
    {
        PR_ASSERT(pthread_equal(thred->id, id));
    }

#ifdef _PR_NICE_PRIORITY_SCHEDULING
    thred->tid = tid;
    PR_NotifyAllCondVar(pt_book.cv);
#endif

    /* If this is a GCABLE thread, set its state appropriately */
    if (thred->suspend & PT_THREAD_SETGCABLE) {
        thred->state |= PT_THREAD_GCABLE;
    }
    thred->suspend = 0;

    thred->prev = pt_book.last;
    if (pt_book.last) {
        pt_book.last->next = thred;
    }
    else {
        pt_book.first = thred;
    }
    thred->next = NULL;
    pt_book.last = thred;
    PR_Unlock(pt_book.ml);

    thred->startFunc(thred->arg);  /* make visible to the client */

    /* unhook the thread from the runtime */
    PR_Lock(pt_book.ml);
    /*
     * At this moment, PR_CreateThread() may not have set thred->id yet.
     * It is safe for a detached thread to free thred only after
     * PR_CreateThread() has accessed thred->id and thred->idSet.
     */
    if (detached)
    {
        while (!thred->okToDelete) {
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
        }
    }

    if (thred->state & PT_THREAD_SYSTEM) {
        pt_book.system -= 1;
    }
    else if (--pt_book.user == pt_book.this_many) {
        PR_NotifyAllCondVar(pt_book.cv);
    }
    if (NULL == thred->prev) {
        pt_book.first = thred->next;
    }
    else {
        thred->prev->next = thred->next;
    }
    if (NULL == thred->next) {
        pt_book.last = thred->prev;
    }
    else {
        thred->next->prev = thred->prev;
    }
    PR_Unlock(pt_book.ml);

    /*
    * Here we set the pthread's backpointer to the PRThread to NULL.
    * Otherwise the destructor would get called eagerly as the thread
    * returns to the pthread runtime. The joining thread would them be
    * the proud possessor of a dangling reference. However, this is the
    * last chance to delete the object if the thread is detached, so
    * just let the destructor do the work.
    */
    if (PR_FALSE == detached)
    {
        /* Call TPD destructors on this thread. */
        _PR_DestroyThreadPrivate(thred);
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
    }

    return NULL;
}  /* _pt_root */

static PRThread* pt_AttachThread(void)
{
    PRThread *thred = NULL;

    /*
     * NSPR must have been initialized when PR_AttachThread is called.
     * We cannot have PR_AttachThread call implicit initialization
     * because if multiple threads call PR_AttachThread simultaneously,
     * NSPR may be initialized more than once.
     * We can't call any function that calls PR_GetCurrentThread()
     * either (e.g., PR_SetError()) as that will result in infinite
     * recursion.
     */
    if (!_pr_initialized) {
        return NULL;
    }

    /* PR_NEWZAP must not call PR_GetCurrentThread() */
    thred = PR_NEWZAP(PRThread);
    if (NULL != thred)
    {
        int rv;

        thred->priority = PR_PRIORITY_NORMAL;
        thred->id = pthread_self();
        thred->idSet = PR_TRUE;
#ifdef _PR_NICE_PRIORITY_SCHEDULING
        thred->tid = gettid();
#endif
        rv = pthread_setspecific(pt_book.key, thred);
        PR_ASSERT(0 == rv);

        thred->state = PT_THREAD_GLOBAL | PT_THREAD_FOREIGN;
        PR_Lock(pt_book.ml);

        /* then put it into the list */
        thred->prev = pt_book.last;
        if (pt_book.last) {
            pt_book.last->next = thred;
        }
        else {
            pt_book.first = thred;
        }
        thred->next = NULL;
        pt_book.last = thred;
        PR_Unlock(pt_book.ml);

    }
    return thred;  /* may be NULL */
}  /* pt_AttachThread */

static PRThread* _PR_CreateThread(
    PRThreadType type, void (*start)(void *arg),
    void *arg, PRThreadPriority priority, PRThreadScope scope,
    PRThreadState state, PRUint32 stackSize, PRBool isGCAble)
{
    int rv;
    PRThread *thred;
    pthread_attr_t tattr;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)priority) {
        priority = PR_PRIORITY_FIRST;
    }
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)priority) {
        priority = PR_PRIORITY_LAST;
    }

    rv = _PT_PTHREAD_ATTR_INIT(&tattr);
    PR_ASSERT(0 == rv);

    if (EPERM != pt_schedpriv)
    {
#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
        struct sched_param schedule;
#endif

#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
        rv = pthread_attr_setinheritsched(&tattr, PTHREAD_EXPLICIT_SCHED);
        PR_ASSERT(0 == rv);
#endif

        /* Use the default scheduling policy */

#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
        rv = pthread_attr_getschedparam(&tattr, &schedule);
        PR_ASSERT(0 == rv);
        schedule.sched_priority = pt_PriorityMap(priority);
        rv = pthread_attr_setschedparam(&tattr, &schedule);
        PR_ASSERT(0 == rv);
#ifdef NTO
        rv = pthread_attr_setschedpolicy(&tattr, SCHED_RR); /* Round Robin */
        PR_ASSERT(0 == rv);
#endif
#endif /* _POSIX_THREAD_PRIORITY_SCHEDULING > 0 */
    }

    rv = pthread_attr_setdetachstate(&tattr,
                                     ((PR_JOINABLE_THREAD == state) ?
                                      PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));
    PR_ASSERT(0 == rv);

    /*
     * If stackSize is 0, we use the default pthread stack size.
     */
    if (stackSize)
    {
#ifdef _MD_MINIMUM_STACK_SIZE
        if (stackSize < _MD_MINIMUM_STACK_SIZE) {
            stackSize = _MD_MINIMUM_STACK_SIZE;
        }
#endif
        rv = pthread_attr_setstacksize(&tattr, stackSize);
        PR_ASSERT(0 == rv);
    }

    thred = PR_NEWZAP(PRThread);
    if (NULL == thred)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, errno);
        goto done;
    }
    else
    {
        pthread_t id;

        thred->arg = arg;
        thred->startFunc = start;
        thred->priority = priority;
        if (PR_UNJOINABLE_THREAD == state) {
            thred->state |= PT_THREAD_DETACHED;
        }

        if (PR_LOCAL_THREAD == scope) {
            scope = PR_GLOBAL_THREAD;
        }

        if (PR_GLOBAL_BOUND_THREAD == scope) {
#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
            rv = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
            if (rv) {
                /*
                 * system scope not supported
                 */
                scope = PR_GLOBAL_THREAD;
                /*
                 * reset scope
                 */
                rv = pthread_attr_setscope(&tattr, PTHREAD_SCOPE_PROCESS);
                PR_ASSERT(0 == rv);
            }
#endif
        }
        if (PR_GLOBAL_THREAD == scope) {
            thred->state |= PT_THREAD_GLOBAL;
        }
        else if (PR_GLOBAL_BOUND_THREAD == scope) {
            thred->state |= (PT_THREAD_GLOBAL | PT_THREAD_BOUND);
        }
        else {  /* force it global */
            thred->state |= PT_THREAD_GLOBAL;
        }
        if (PR_SYSTEM_THREAD == type) {
            thred->state |= PT_THREAD_SYSTEM;
        }

        thred->suspend =(isGCAble) ? PT_THREAD_SETGCABLE : 0;

        thred->stack = PR_NEWZAP(PRThreadStack);
        if (thred->stack == NULL) {
            PRIntn oserr = errno;
            PR_Free(thred);  /* all that work ... poof! */
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, oserr);
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
        if (PR_SYSTEM_THREAD == type) {
            pt_book.system += 1;
        }
        else {
            pt_book.user += 1;
        }
        PR_Unlock(pt_book.ml);

        /*
         * We pass a pointer to a local copy (instead of thred->id)
         * to pthread_create() because who knows what wacky things
         * pthread_create() may be doing to its argument.
         */
        rv = _PT_PTHREAD_CREATE(&id, tattr, _pt_root, thred);

        if (EPERM == rv)
        {
            /* Remember that we don't have thread scheduling privilege. */
            pt_schedpriv = EPERM;
            PR_LOG(_pr_thread_lm, PR_LOG_MIN,
                   ("_PR_CreateThread: no thread scheduling privilege"));
            /* Try creating the thread again without setting priority. */
#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
            rv = pthread_attr_setinheritsched(&tattr, PTHREAD_INHERIT_SCHED);
            PR_ASSERT(0 == rv);
#endif
            rv = _PT_PTHREAD_CREATE(&id, tattr, _pt_root, thred);
        }

        if (0 != rv)
        {
            PRIntn oserr = rv;
            PR_Lock(pt_book.ml);
            if (thred->state & PT_THREAD_SYSTEM) {
                pt_book.system -= 1;
            }
            else if (--pt_book.user == pt_book.this_many) {
                PR_NotifyAllCondVar(pt_book.cv);
            }
            PR_Unlock(pt_book.ml);

            PR_Free(thred->stack);
            PR_Free(thred);  /* all that work ... poof! */
            PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, oserr);
            thred = NULL;  /* and for what? */
            goto done;
        }

        PR_Lock(pt_book.ml);
        /*
         * Both the parent thread and this new thread set thred->id.
         * The parent thread must ensure that thred->id is set before
         * PR_CreateThread() returns.  (See comments in _pt_root().)
         */
        if (!thred->idSet)
        {
            thred->id = id;
            thred->idSet = PR_TRUE;
        }
        else
        {
            PR_ASSERT(pthread_equal(thred->id, id));
        }

        /*
         * If the new thread is detached, tell it that PR_CreateThread() has
         * accessed thred->id and thred->idSet so it's ok to delete thred.
         */
        if (PR_UNJOINABLE_THREAD == state)
        {
            thred->okToDelete = PR_TRUE;
            PR_NotifyAllCondVar(pt_book.cv);
        }
        PR_Unlock(pt_book.ml);
    }

done:
    rv = _PT_PTHREAD_ATTR_DESTROY(&tattr);
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
    return PR_GetCurrentThread();
}  /* PR_AttachThread */


PR_IMPLEMENT(PRStatus) PR_JoinThread(PRThread *thred)
{
    int rv = -1;
    void *result = NULL;
    PR_ASSERT(thred != NULL);

    if ((0xafafafaf == thred->state)
        || (PT_THREAD_DETACHED == (PT_THREAD_DETACHED & thred->state))
        || (PT_THREAD_FOREIGN == (PT_THREAD_FOREIGN & thred->state)))
    {
        /*
         * This might be a bad address, but if it isn't, the state should
         * either be an unjoinable thread or it's already had the object
         * deleted. However, the client that called join on a detached
         * thread deserves all the rath I can muster....
         */
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        PR_LogPrint(
            "PR_JoinThread: %p not joinable | already smashed\n", thred);
    }
    else
    {
        pthread_t id = thred->id;
        rv = pthread_join(id, &result);
        PR_ASSERT(rv == 0 && result == NULL);
        if (0 == rv)
        {
            /*
             * PR_FALSE, because the thread already called the TPD
             * destructors before exiting _pt_root.
             */
            _pt_thread_death_internal(thred, PR_FALSE);
        }
        else
        {
            PRErrorCode prerror;
            switch (rv)
            {
                case EINVAL:  /* not a joinable thread */
                case ESRCH:   /* no thread with given ID */
                    prerror = PR_INVALID_ARGUMENT_ERROR;
                    break;
                case EDEADLK: /* a thread joining with itself */
                    prerror = PR_DEADLOCK_ERROR;
                    break;
                default:
                    prerror = PR_UNKNOWN_ERROR;
                    break;
            }
            PR_SetError(prerror, rv);
        }
    }
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_JoinThread */

PR_IMPLEMENT(void) PR_DetachThread(void)
{
    void *thred;
    int rv;

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL == thred) {
        return;
    }
    _pt_thread_death(thred);
    rv = pthread_setspecific(pt_book.key, NULL);
    PR_ASSERT(0 == rv);
}  /* PR_DetachThread */

PR_IMPLEMENT(PRThread*) PR_GetCurrentThread(void)
{
    void *thred;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL == thred) {
        thred = pt_AttachThread();
    }
    PR_ASSERT(NULL != thred);
    return (PRThread*)thred;
}  /* PR_GetCurrentThread */

PR_IMPLEMENT(PRThreadScope) PR_GetThreadScope(const PRThread *thred)
{
    return (thred->state & PT_THREAD_BOUND) ?
           PR_GLOBAL_BOUND_THREAD : PR_GLOBAL_THREAD;
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

    if ((PRIntn)PR_PRIORITY_FIRST > (PRIntn)newPri) {
        newPri = PR_PRIORITY_FIRST;
    }
    else if ((PRIntn)PR_PRIORITY_LAST < (PRIntn)newPri) {
        newPri = PR_PRIORITY_LAST;
    }

#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
    if (EPERM != pt_schedpriv)
    {
        int policy;
        struct sched_param schedule;

        rv = pthread_getschedparam(thred->id, &policy, &schedule);
        if(0 == rv) {
            schedule.sched_priority = pt_PriorityMap(newPri);
            rv = pthread_setschedparam(thred->id, policy, &schedule);
            if (EPERM == rv)
            {
                pt_schedpriv = EPERM;
                PR_LOG(_pr_thread_lm, PR_LOG_MIN,
                       ("PR_SetThreadPriority: no thread scheduling privilege"));
            }
        }
        if (rv != 0) {
            rv = -1;
        }
    }
#elif defined(_PR_NICE_PRIORITY_SCHEDULING)
    PR_Lock(pt_book.ml);
    while (thred->tid == 0) {
        PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(pt_book.ml);

    errno = 0;
    rv = getpriority(PRIO_PROCESS, 0);

    /* Do not proceed unless we know the main thread's nice value. */
    if (errno == 0) {
        rv = setpriority(PRIO_PROCESS, thred->tid,
                         pt_RelativePriority(rv, newPri));

        if (rv == -1)
        {
            /* We don't set pt_schedpriv to EPERM in case errno == EPERM
             * because adjusting the nice value might be permitted for certain
             * ranges but not for others. */
            PR_LOG(_pr_thread_lm, PR_LOG_MIN,
                   ("PR_SetThreadPriority: setpriority failed with error %d",
                    errno));
        }
    }
#else
    (void)rv; /* rv is unused */
#endif

    thred->priority = newPri;
}  /* PR_SetThreadPriority */

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
    if (NULL == thred) {
        return PR_FAILURE;
    }

    thred->state |= PT_THREAD_ABORTED;

    cv = thred->waiting;
    if ((NULL != cv) && !thred->interrupt_blocked)
    {
        PRIntn rv;
        (void)PR_ATOMIC_INCREMENT(&cv->notify_pending);
        rv = pthread_cond_broadcast(&cv->cv);
        PR_ASSERT(0 == rv);
        if (0 > PR_ATOMIC_DECREMENT(&cv->notify_pending)) {
            PR_DestroyCondVar(cv);
        }
    }
    return PR_SUCCESS;
}  /* PR_Interrupt */

PR_IMPLEMENT(void) PR_ClearInterrupt(void)
{
    PRThread *me = PR_GetCurrentThread();
    me->state &= ~PT_THREAD_ABORTED;
}  /* PR_ClearInterrupt */

PR_IMPLEMENT(void) PR_BlockInterrupt(void)
{
    PRThread *me = PR_GetCurrentThread();
    _PT_THREAD_BLOCK_INTERRUPT(me);
}  /* PR_BlockInterrupt */

PR_IMPLEMENT(void) PR_UnblockInterrupt(void)
{
    PRThread *me = PR_GetCurrentThread();
    _PT_THREAD_UNBLOCK_INTERRUPT(me);
}  /* PR_UnblockInterrupt */

PR_IMPLEMENT(PRStatus) PR_Yield(void)
{
    static PRBool warning = PR_TRUE;
    if (warning) warning = _PR_Obsolete(
                                   "PR_Yield()", "PR_Sleep(PR_INTERVAL_NO_WAIT)");
    return PR_Sleep(PR_INTERVAL_NO_WAIT);
}

PR_IMPLEMENT(PRStatus) PR_Sleep(PRIntervalTime ticks)
{
    PRStatus rv = PR_SUCCESS;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    if (PR_INTERVAL_NO_WAIT == ticks)
    {
        _PT_PTHREAD_YIELD();
    }
    else
    {
        PRCondVar *cv;
        PRIntervalTime timein;

        timein = PR_IntervalNow();
        cv = PR_NewCondVar(_pr_sleeplock);
        PR_ASSERT(cv != NULL);
        PR_Lock(_pr_sleeplock);
        do
        {
            PRIntervalTime now = PR_IntervalNow();
            PRIntervalTime delta = now - timein;
            if (delta > ticks) {
                break;
            }
            rv = PR_WaitCondVar(cv, ticks - delta);
        } while (PR_SUCCESS == rv);
        PR_Unlock(_pr_sleeplock);
        PR_DestroyCondVar(cv);
    }
    return rv;
}  /* PR_Sleep */

static void _pt_thread_death(void *arg)
{
    void *thred;
    int rv;

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL == thred)
    {
        /*
         * Have PR_GetCurrentThread return the expected value to the
         * destructors.
         */
        rv = pthread_setspecific(pt_book.key, arg);
        PR_ASSERT(0 == rv);
    }

    /* PR_TRUE for: call destructors */
    _pt_thread_death_internal(arg, PR_TRUE);

    if (NULL == thred)
    {
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
    }
}

static void _pt_thread_death_internal(void *arg, PRBool callDestructors)
{
    PRThread *thred = (PRThread*)arg;

    if (thred->state & (PT_THREAD_FOREIGN|PT_THREAD_PRIMORD))
    {
        PR_Lock(pt_book.ml);
        if (NULL == thred->prev) {
            pt_book.first = thred->next;
        }
        else {
            thred->prev->next = thred->next;
        }
        if (NULL == thred->next) {
            pt_book.last = thred->prev;
        }
        else {
            thred->next->prev = thred->prev;
        }
        PR_Unlock(pt_book.ml);
    }
    if (callDestructors) {
        _PR_DestroyThreadPrivate(thred);
    }
    PR_Free(thred->privateData);
    if (NULL != thred->errorString) {
        PR_Free(thred->errorString);
    }
    if (NULL != thred->name) {
        PR_Free(thred->name);
    }
    PR_Free(thred->stack);
    if (NULL != thred->syspoll_list) {
        PR_Free(thred->syspoll_list);
    }
#if defined(_PR_POLL_WITH_SELECT)
    if (NULL != thred->selectfd_list) {
        PR_Free(thred->selectfd_list);
    }
#endif
#if defined(DEBUG)
    memset(thred, 0xaf, sizeof(PRThread));
#endif /* defined(DEBUG) */
    PR_Free(thred);
}  /* _pt_thread_death */

void _PR_InitThreads(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
    int rv;
    PRThread *thred;

    PR_ASSERT(priority == PR_PRIORITY_NORMAL);

#ifdef _PR_NEED_PTHREAD_INIT
    /*
     * On BSD/OS (3.1 and 4.0), the pthread subsystem is lazily
     * initialized, but pthread_self() fails to initialize
     * pthreads and hence returns a null thread ID if invoked
     * by the primordial thread before any other pthread call.
     * So we explicitly initialize pthreads here.
     */
    pthread_init();
#endif

#if _POSIX_THREAD_PRIORITY_SCHEDULING > 0
#if defined(FREEBSD)
    {
        pthread_attr_t attr;
        int policy;
        /* get the min and max priorities of the default policy */
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_getschedpolicy(&attr, &policy);
        pt_book.minPrio = sched_get_priority_min(policy);
        PR_ASSERT(-1 != pt_book.minPrio);
        pt_book.maxPrio = sched_get_priority_max(policy);
        PR_ASSERT(-1 != pt_book.maxPrio);
        pthread_attr_destroy(&attr);
    }
#else
    /*
    ** These might be function evaluations
    */
    pt_book.minPrio = PT_PRIO_MIN;
    pt_book.maxPrio = PT_PRIO_MAX;
#endif
#endif

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
    thred->idSet = PR_TRUE;
#ifdef _PR_NICE_PRIORITY_SCHEDULING
    thred->tid = gettid();
#endif

    thred->state = (PT_THREAD_DETACHED | PT_THREAD_PRIMORD);
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
     * NBB: Oh really? I'm going to give it a spin - AOF 19 June 1998.
     * More info - the problem is that pthreads calls the destructor
     * eagerly as the thread returns from its root, rather than lazily
     * after the thread is joined. Therefore, threads that are joining
     * and holding PRThread references are actually holding pointers to
     * nothing.
     */
    rv = _PT_PTHREAD_KEY_CREATE(&pt_book.key, _pt_thread_death);
    if (0 != rv) {
        PR_Assert("0 == rv", __FILE__, __LINE__);
    }
    pt_book.keyCreated = PR_TRUE;
    rv = pthread_setspecific(pt_book.key, thred);
    PR_ASSERT(0 == rv);
}  /* _PR_InitThreads */

#ifdef __GNUC__
/*
 * GCC supports the constructor and destructor attributes as of
 * version 2.5.
 */
#if defined(DARWIN)
/*
 * The dynamic linker on OSX doesn't execute __attribute__((destructor))
 * destructors in the right order wrt non-__attribute((destructor)) destructors
 * in other libraries. So use atexit() instead, which does.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1399746#c99
 */
static void _PR_Fini(void);

__attribute__ ((constructor))
static void _register_PR_Fini() {
    atexit(_PR_Fini);
}
#else
static void _PR_Fini(void) __attribute__ ((destructor));
#endif

#elif defined(__SUNPRO_C)
/*
 * Sun Studio compiler
 */
#pragma fini(_PR_Fini)
static void _PR_Fini(void);
#elif defined(HPUX)
/*
 * Current versions of HP C compiler define __HP_cc.
 * HP C compiler A.11.01.20 doesn't define __HP_cc.
 */
#if defined(__ia64) || defined(_LP64)
#pragma FINI "_PR_Fini"
static void _PR_Fini(void);
#else
/*
 * Only HP-UX 10.x style initializers are supported in 32-bit links.
 * Need to use the +I PR_HPUX10xInit linker option.
 */
#include <dl.h>

static void _PR_Fini(void);

void PR_HPUX10xInit(shl_t handle, int loading)
{
    /*
     * This function is called when a shared library is loaded as well
     * as when the shared library is unloaded.  Note that it may not
     * be called when the user's program terminates.
     *
     * handle is the shl_load API handle for the shared library being
     * initialized.
     *
     * loading is non-zero at startup and zero at termination.
     */
    if (loading) {
        /* ... do some initializations ... */
    } else {
        _PR_Fini();
    }
}
#endif
#elif defined(AIX)
/* Need to use the -binitfini::_PR_Fini linker option. */
#endif

void _PR_Fini(void)
{
    void *thred;
    int rv;

    if (!_pr_initialized) {
        /* Either NSPR was never successfully initialized or
         * PR_Cleanup has been called already. */
        if (pt_book.keyCreated)
        {
            rv = pthread_key_delete(pt_book.key);
            PR_ASSERT(0 == rv);
            pt_book.keyCreated = PR_FALSE;
        }
        return;
    }

    _PT_PTHREAD_GETSPECIFIC(pt_book.key, thred);
    if (NULL != thred)
    {
        /*
         * PR_FALSE, because it is unsafe to call back to the
         * thread private data destructors at final cleanup.
         */
        _pt_thread_death_internal(thred, PR_FALSE);
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
    }
    rv = pthread_key_delete(pt_book.key);
    PR_ASSERT(0 == rv);
    pt_book.keyCreated = PR_FALSE;
    /* TODO: free other resources used by NSPR */
    /* _pr_initialized = PR_FALSE; */
}  /* _PR_Fini */

PR_IMPLEMENT(PRStatus) PR_Cleanup(void)
{
    PRThread *me = PR_GetCurrentThread();
    int rv;
    PR_LOG(_pr_thread_lm, PR_LOG_MIN, ("PR_Cleanup: shutting down NSPR"));
    PR_ASSERT(me->state & PT_THREAD_PRIMORD);
    if (me->state & PT_THREAD_PRIMORD)
    {
        PR_Lock(pt_book.ml);
        while (pt_book.user > pt_book.this_many) {
            PR_WaitCondVar(pt_book.cv, PR_INTERVAL_NO_TIMEOUT);
        }
        if (me->state & PT_THREAD_SYSTEM) {
            pt_book.system -= 1;
        }
        else {
            pt_book.user -= 1;
        }
        PR_Unlock(pt_book.ml);

        _PR_MD_EARLY_CLEANUP();

        _PR_CleanupMW();
        _PR_CleanupTime();
        _PR_CleanupDtoa();
        _PR_CleanupCallOnce();
        _PR_ShutdownLinker();
        _PR_LogCleanup();
        _PR_CleanupNet();
        /* Close all the fd's before calling _PR_CleanupIO */
        _PR_CleanupIO();
        _PR_CleanupCMon();

        _pt_thread_death(me);
        rv = pthread_setspecific(pt_book.key, NULL);
        PR_ASSERT(0 == rv);
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
        PR_DestroyLock(_pr_sleeplock);
        _pr_sleeplock = NULL;
        _PR_CleanupLayerCache();
        _PR_CleanupEnv();
#ifdef _PR_ZONE_ALLOCATOR
        _PR_DestroyZones();
#endif
        _pr_initialized = PR_FALSE;
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}  /* PR_Cleanup */

PR_IMPLEMENT(void) PR_ProcessExit(PRIntn status)
{
    _exit(status);
}

PR_IMPLEMENT(PRUint32) PR_GetThreadID(PRThread *thred)
{
    return (PRUint32)thred->id;  /* and I don't know what they will do with it */
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

/* a bogus signal mask for forcing a timed wait */
/* Not so bogus in AIX as we really do a sigwait */
static sigset_t sigwait_set;

static struct timespec onemillisec = {0, 1000000L};
#ifndef PT_NO_SIGTIMEDWAIT
static struct timespec hundredmillisec = {0, 100000000L};
#endif

static void suspend_signal_handler(PRIntn sig);

#ifdef PT_NO_SIGTIMEDWAIT
static void null_signal_handler(PRIntn sig);
#endif

/*
 * Linux pthreads use SIGUSR1 and SIGUSR2 internally, which
 * conflict with the use of these two signals in our GC support.
 * So we don't know how to support GC on Linux pthreads.
 */
static void init_pthread_gc_support(void)
{
    PRIntn rv;

    {
        struct sigaction sigact_usr2;

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
        struct sigaction sigact_null;
        sigact_null.sa_handler = null_signal_handler;
        sigact_null.sa_flags = SA_RESTART;
        sigemptyset (&sigact_null.sa_mask);
        rv = sigaction (SIGUSR1, &sigact_null, NULL);
        PR_ASSERT(0 ==rv);
    }
#endif  /* defined(PT_NO_SIGTIMEDWAIT) */
}

PR_IMPLEMENT(void) PR_SetThreadGCAble(void)
{
    PR_Lock(pt_book.ml);
    PR_GetCurrentThread()->state |= PT_THREAD_GCABLE;
    PR_Unlock(pt_book.ml);
}

PR_IMPLEMENT(void) PR_ClearThreadGCAble(void)
{
    PR_Lock(pt_book.ml);
    PR_GetCurrentThread()->state &= (~PT_THREAD_GCABLE);
    PR_Unlock(pt_book.ml);
}

#if defined(DEBUG)
static PRBool suspendAllOn = PR_FALSE;
#endif

static PRBool suspendAllSuspended = PR_FALSE;

PR_IMPLEMENT(PRStatus) PR_EnumerateThreads(PREnumerator func, void *arg)
{
    PRIntn count = 0;
    PRStatus rv = PR_SUCCESS;
    PRThread* thred = pt_book.first;

#if defined(DEBUG) || defined(FORCE_PR_ASSERT)
    PRThread *me = PR_GetCurrentThread();
#endif

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

        if (_PT_IS_GCABLE_THREAD(thred))
        {
            PR_ASSERT((thred == me) || (thred->suspend & PT_THREAD_SUSPENDED));
            PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
                   ("In PR_EnumerateThreads callback thread %p thid = %X\n",
                    thred, thred->id));

            rv = func(thred, count++, arg);
            if (rv != PR_SUCCESS) {
                return rv;
            }
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
    PRThread *me = PR_GetCurrentThread();

    PR_ASSERT(me != NULL);
    PR_ASSERT(_PT_IS_GCABLE_THREAD(me));
    PR_ASSERT((me->suspend & PT_THREAD_SUSPENDED) == 0);

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("Begin suspend_signal_handler thred %p thread id = %X\n",
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
#if !defined(FREEBSD) && !defined(NETBSD) && !defined(OPENBSD) \
    && !defined(BSDI) && !defined(UNIXWARE) \
    && !defined(DARWIN) && !defined(RISCOS)
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
           ("End suspend_signal_handler thred = %p tid = %X\n", me, me->id));
}  /* suspend_signal_handler */

static void pt_SuspendSet(PRThread *thred)
{
    PRIntn rv;

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("pt_SuspendSet thred %p thread id = %X\n", thred, thred->id));


    /*
     * Check the thread state and signal the thread to suspend
     */

    PR_ASSERT((thred->suspend & PT_THREAD_SUSPENDED) == 0);

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("doing pthread_kill in pt_SuspendSet thred %p tid = %X\n",
            thred, thred->id));
    rv = pthread_kill (thred->id, SIGUSR2);
    PR_ASSERT(0 == rv);
}

static void pt_SuspendTest(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("Begin pt_SuspendTest thred %p thread id = %X\n", thred, thred->id));


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
        PRIntn rv = sigtimedwait(&sigwait_set, NULL, &onemillisec);
        PR_ASSERT(-1 == rv);
    }
#endif

    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("End pt_SuspendTest thred %p tid %X\n", thred, thred->id));
}  /* pt_SuspendTest */

static void pt_ResumeSet(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("pt_ResumeSet thred %p thread id = %X\n", thred, thred->id));

    /*
     * Clear the global state and set the thread state so that it will
     * continue past yield loop in the suspend signal handler
     */

    PR_ASSERT(thred->suspend & PT_THREAD_SUSPENDED);


    thred->suspend &= ~PT_THREAD_SUSPENDED;

#if defined(PT_NO_SIGTIMEDWAIT)
    pthread_kill(thred->id, SIGUSR1);
#endif

}  /* pt_ResumeSet */

static void pt_ResumeTest(PRThread *thred)
{
    PR_LOG(_pr_gc_lm, PR_LOG_ALWAYS,
           ("Begin pt_ResumeTest thred %p thread id = %X\n", thred, thred->id));

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
               "End pt_ResumeTest thred %p tid %X\n", thred, thred->id));
}  /* pt_ResumeTest */

static pthread_once_t pt_gc_support_control = PTHREAD_ONCE_INIT;

PR_IMPLEMENT(void) PR_SuspendAll(void)
{
#ifdef DEBUG
    PRIntervalTime stime, etime;
#endif
    PRThread* thred = pt_book.first;
    PRThread *me = PR_GetCurrentThread();
    int rv;

    rv = pthread_once(&pt_gc_support_control, init_pthread_gc_support);
    PR_ASSERT(0 == rv);
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
        if ((thred != me) && _PT_IS_GCABLE_THREAD(thred)) {
            pt_SuspendSet(thred);
        }
        thred = thred->next;
    }

    /* Wait till they are really suspended */
    thred = pt_book.first;
    while (thred != NULL)
    {
        if ((thred != me) && _PT_IS_GCABLE_THREAD(thred)) {
            pt_SuspendTest(thred);
        }
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

PR_IMPLEMENT(void) PR_ResumeAll(void)
{
#ifdef DEBUG
    PRIntervalTime stime, etime;
#endif
    PRThread* thred = pt_book.first;
    PRThread *me = PR_GetCurrentThread();
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
        if ((thred != me) && _PT_IS_GCABLE_THREAD(thred)) {
            pt_ResumeSet(thred);
        }
        thred = thred->next;
    }

    thred = pt_book.first;
    while (thred != NULL)
    {
        if ((thred != me) && _PT_IS_GCABLE_THREAD(thred)) {
            pt_ResumeTest(thred);
        }
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
           ("in PR_GetSP thred %p thid = %X, sp = %p\n",
            thred, thred->id, thred->sp));
    return thred->sp;
}  /* PR_GetSP */

PR_IMPLEMENT(PRStatus) PR_SetCurrentThreadName(const char *name)
{
    PRThread *thread;
    size_t nameLen;
    int result = 0;

    if (!name) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    thread = PR_GetCurrentThread();
    if (!thread) {
        return PR_FAILURE;
    }

    PR_Free(thread->name);
    nameLen = strlen(name);
    thread->name = (char *)PR_Malloc(nameLen + 1);
    if (!thread->name) {
        return PR_FAILURE;
    }
    memcpy(thread->name, name, nameLen + 1);

#if defined(OPENBSD) || defined(FREEBSD) || defined(DRAGONFLY)
    pthread_set_name_np(thread->id, name);
#elif defined(ANDROID)
    prctl(PR_SET_NAME, (unsigned long)(name));
#elif defined(NETBSD)
    result = pthread_setname_np(thread->id, "%s", (void *)name);
#else /* not BSD */
    /*
     * On OSX, pthread_setname_np is only available in 10.6 or later, so test
     * for it at runtime.  It also may not be available on all linux distros.
     */
#if defined(DARWIN)
    int (*dynamic_pthread_setname_np)(const char*);
#else
    int (*dynamic_pthread_setname_np)(pthread_t, const char*);
#endif

    *(void**)(&dynamic_pthread_setname_np) =
        dlsym(RTLD_DEFAULT, "pthread_setname_np");
    if (!dynamic_pthread_setname_np) {
        return PR_SUCCESS;
    }

#if defined(DARWIN)
    /* Mac OS X has a length limit of 63 characters, but there is no API
     * exposing it.
     */
#define SETNAME_LENGTH_CONSTRAINT 63
#else
    /*
     * The 15-character name length limit is an experimentally determined
     * length of a null-terminated string that most linux distros accept
     * as an argument to pthread_setname_np.  Otherwise the E2BIG
     * error is returned by the function.
     */
#define SETNAME_LENGTH_CONSTRAINT 15
#endif
#define SETNAME_FRAGMENT1_LENGTH (SETNAME_LENGTH_CONSTRAINT >> 1)
#define SETNAME_FRAGMENT2_LENGTH \
    (SETNAME_LENGTH_CONSTRAINT - SETNAME_FRAGMENT1_LENGTH - 1)
    char name_dup[SETNAME_LENGTH_CONSTRAINT + 1];
    if (nameLen > SETNAME_LENGTH_CONSTRAINT) {
        memcpy(name_dup, name, SETNAME_FRAGMENT1_LENGTH);
        name_dup[SETNAME_FRAGMENT1_LENGTH] = '~';
        /* Note that this also copies the null terminator. */
        memcpy(name_dup + SETNAME_FRAGMENT1_LENGTH + 1,
               name + nameLen - SETNAME_FRAGMENT2_LENGTH,
               SETNAME_FRAGMENT2_LENGTH + 1);
        name = name_dup;
    }

#if defined(DARWIN)
    result = dynamic_pthread_setname_np(name);
#else
    result = dynamic_pthread_setname_np(thread->id, name);
#endif
#endif /* not BSD */

    if (result) {
        PR_SetError(PR_UNKNOWN_ERROR, result);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PR_IMPLEMENT(const char *) PR_GetThreadName(const PRThread *thread)
{
    if (!thread) {
        return NULL;
    }
    return thread->name;
}

#endif  /* defined(_PR_PTHREADS) */

/* ptthread.c */
