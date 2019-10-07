/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


sigset_t ints_off;
pthread_mutex_t _pr_heapLock;
pthread_key_t current_thread_key;
pthread_key_t current_cpu_key;
pthread_key_t last_thread_key;
pthread_key_t intsoff_key;


PRInt32 _pr_md_pthreads_created, _pr_md_pthreads_failed;
PRInt32 _pr_md_pthreads = 1;

void _MD_EarlyInit(void)
{
    extern PRInt32 _nspr_noclock;

    if (pthread_key_create(&current_thread_key, NULL) != 0) {
        perror("pthread_key_create failed");
        exit(1);
    }
    if (pthread_key_create(&current_cpu_key, NULL) != 0) {
        perror("pthread_key_create failed");
        exit(1);
    }
    if (pthread_key_create(&last_thread_key, NULL) != 0) {
        perror("pthread_key_create failed");
        exit(1);
    }
    if (pthread_key_create(&intsoff_key, NULL) != 0) {
        perror("pthread_key_create failed");
        exit(1);
    }

    sigemptyset(&ints_off);
    sigaddset(&ints_off, SIGALRM);
    sigaddset(&ints_off, SIGIO);
    sigaddset(&ints_off, SIGCLD);

    /*
     * disable clock interrupts
     */
    _nspr_noclock = 1;

}

void _MD_InitLocks()
{
    if (pthread_mutex_init(&_pr_heapLock, NULL) != 0) {
        perror("pthread_mutex_init failed");
        exit(1);
    }
}

PR_IMPLEMENT(void) _MD_FREE_LOCK(struct _MDLock *lockp)
{
    PRIntn _is;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (me && !_PR_IS_NATIVE_THREAD(me)) {
        _PR_INTSOFF(_is);
    }
    pthread_mutex_destroy(&lockp->mutex);
    if (me && !_PR_IS_NATIVE_THREAD(me)) {
        _PR_FAST_INTSON(_is);
    }
}



PR_IMPLEMENT(PRStatus) _MD_NEW_LOCK(struct _MDLock *lockp)
{
    PRStatus rv;
    PRIntn is;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (me && !_PR_IS_NATIVE_THREAD(me)) {
        _PR_INTSOFF(is);
    }
    rv = pthread_mutex_init(&lockp->mutex, NULL);
    if (me && !_PR_IS_NATIVE_THREAD(me)) {
        _PR_FAST_INTSON(is);
    }
    return (rv == 0) ? PR_SUCCESS : PR_FAILURE;
}


PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
        (void) setjmp(CONTEXT(t));
    }
    *np = sizeof(CONTEXT(t)) / sizeof(PRWord);
    return (PRWord *) CONTEXT(t);
}

PR_IMPLEMENT(void)
_MD_SetPriority(_MDThread *thread, PRThreadPriority newPri)
{
    /*
     * XXX - to be implemented
     */
    return;
}

PR_IMPLEMENT(PRStatus) _MD_InitThread(struct PRThread *thread)
{
    struct sigaction sigact;

    if (thread->flags & _PR_GLOBAL_SCOPE) {
        thread->md.pthread = pthread_self();
#if 0
        /*
         * set up SIGUSR1 handler; this is used to save state
         * during PR_SuspendAll
         */
        sigact.sa_handler = save_context_and_block;
        sigact.sa_flags = SA_RESTART;
        /*
         * Must mask clock interrupts
         */
        sigact.sa_mask = timer_set;
        sigaction(SIGUSR1, &sigact, 0);
#endif
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(void) _MD_ExitThread(struct PRThread *thread)
{
    if (thread->flags & _PR_GLOBAL_SCOPE) {
        _MD_CLEAN_THREAD(thread);
        _MD_SET_CURRENT_THREAD(NULL);
    }
}

PR_IMPLEMENT(void) _MD_CleanThread(struct PRThread *thread)
{
    if (thread->flags & _PR_GLOBAL_SCOPE) {
        pthread_mutex_destroy(&thread->md.pthread_mutex);
        pthread_cond_destroy(&thread->md.pthread_cond);
    }
}

PR_IMPLEMENT(void) _MD_SuspendThread(struct PRThread *thread)
{
    PRInt32 rv;

    PR_ASSERT((thread->flags & _PR_GLOBAL_SCOPE) &&
              _PR_IS_GCABLE_THREAD(thread));
#if 0
    thread->md.suspending_id = getpid();
    rv = kill(thread->md.id, SIGUSR1);
    PR_ASSERT(rv == 0);
    /*
     * now, block the current thread/cpu until woken up by the suspended
     * thread from it's SIGUSR1 signal handler
     */
    blockproc(getpid());
#endif
}

PR_IMPLEMENT(void) _MD_ResumeThread(struct PRThread *thread)
{
    PRInt32 rv;

    PR_ASSERT((thread->flags & _PR_GLOBAL_SCOPE) &&
              _PR_IS_GCABLE_THREAD(thread));
#if 0
    rv = unblockproc(thread->md.id);
#endif
}

PR_IMPLEMENT(void) _MD_SuspendCPU(struct _PRCPU *thread)
{
    PRInt32 rv;

#if 0
    cpu->md.suspending_id = getpid();
    rv = kill(cpu->md.id, SIGUSR1);
    PR_ASSERT(rv == 0);
    /*
     * now, block the current thread/cpu until woken up by the suspended
     * thread from it's SIGUSR1 signal handler
     */
    blockproc(getpid());
#endif
}

PR_IMPLEMENT(void) _MD_ResumeCPU(struct _PRCPU *thread)
{
#if 0
    unblockproc(cpu->md.id);
#endif
}


#define PT_NANOPERMICRO 1000UL
#define PT_BILLION 1000000000UL

PR_IMPLEMENT(PRStatus)
_pt_wait(PRThread *thread, PRIntervalTime timeout)
{
    int rv;
    struct timeval now;
    struct timespec tmo;
    PRUint32 ticks = PR_TicksPerSecond();


    if (timeout != PR_INTERVAL_NO_TIMEOUT) {
        tmo.tv_sec = timeout / ticks;
        tmo.tv_nsec = timeout - (tmo.tv_sec * ticks);
        tmo.tv_nsec = PR_IntervalToMicroseconds(PT_NANOPERMICRO *
                                                tmo.tv_nsec);

        /* pthreads wants this in absolute time, off we go ... */
        (void)GETTIMEOFDAY(&now);
        /* that one's usecs, this one's nsecs - grrrr! */
        tmo.tv_sec += now.tv_sec;
        tmo.tv_nsec += (PT_NANOPERMICRO * now.tv_usec);
        tmo.tv_sec += tmo.tv_nsec / PT_BILLION;
        tmo.tv_nsec %= PT_BILLION;
    }

    pthread_mutex_lock(&thread->md.pthread_mutex);
    thread->md.wait--;
    if (thread->md.wait < 0) {
        if (timeout != PR_INTERVAL_NO_TIMEOUT) {
            rv = pthread_cond_timedwait(&thread->md.pthread_cond,
                                        &thread->md.pthread_mutex, &tmo);
        }
        else
            rv = pthread_cond_wait(&thread->md.pthread_cond,
                                   &thread->md.pthread_mutex);
        if (rv != 0) {
            thread->md.wait++;
        }
    } else {
        rv = 0;
    }
    pthread_mutex_unlock(&thread->md.pthread_mutex);

    return (rv == 0) ? PR_SUCCESS : PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
_MD_wait(PRThread *thread, PRIntervalTime ticks)
{
    if ( thread->flags & _PR_GLOBAL_SCOPE ) {
        _MD_CHECK_FOR_EXIT();
        if (_pt_wait(thread, ticks) == PR_FAILURE) {
            _MD_CHECK_FOR_EXIT();
            /*
             * wait timed out
             */
            _PR_THREAD_LOCK(thread);
            if (thread->wait.cvar) {
                /*
                 * The thread will remove itself from the waitQ
                 * of the cvar in _PR_WaitCondVar
                 */
                thread->wait.cvar = NULL;
                thread->state =  _PR_RUNNING;
                _PR_THREAD_UNLOCK(thread);
            }  else {
                _pt_wait(thread, PR_INTERVAL_NO_TIMEOUT);
                _PR_THREAD_UNLOCK(thread);
            }
        }
    } else {
        _PR_MD_SWITCH_CONTEXT(thread);
    }
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
_MD_WakeupWaiter(PRThread *thread)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 pid, rv;
    PRIntn is;

    PR_ASSERT(_pr_md_idle_cpus >= 0);
    if (thread == NULL) {
        if (_pr_md_idle_cpus) {
            _MD_Wakeup_CPUs();
        }
    } else if (!_PR_IS_NATIVE_THREAD(thread)) {
        /*
         * If the thread is on my cpu's runq there is no need to
         * wakeup any cpus
         */
        if (!_PR_IS_NATIVE_THREAD(me)) {
            if (me->cpu != thread->cpu) {
                if (_pr_md_idle_cpus) {
                    _MD_Wakeup_CPUs();
                }
            }
        } else {
            if (_pr_md_idle_cpus) {
                _MD_Wakeup_CPUs();
            }
        }
    } else {
        PR_ASSERT(_PR_IS_NATIVE_THREAD(thread));
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_INTSOFF(is);
        }

        pthread_mutex_lock(&thread->md.pthread_mutex);
        thread->md.wait++;
        rv = pthread_cond_signal(&thread->md.pthread_cond);
        PR_ASSERT(rv == 0);
        pthread_mutex_unlock(&thread->md.pthread_mutex);

        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
    }
    return PR_SUCCESS;
}

/* These functions should not be called for AIX */
PR_IMPLEMENT(void)
_MD_YIELD(void)
{
    PR_NOT_REACHED("_MD_YIELD should not be called for AIX.");
}

PR_IMPLEMENT(PRStatus)
_MD_CreateThread(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize)
{
    PRIntn is;
    int rv;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    pthread_attr_t attr;

    if (!_PR_IS_NATIVE_THREAD(me)) {
        _PR_INTSOFF(is);
    }

    if (pthread_mutex_init(&thread->md.pthread_mutex, NULL) != 0) {
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
        return PR_FAILURE;
    }

    if (pthread_cond_init(&thread->md.pthread_cond, NULL) != 0) {
        pthread_mutex_destroy(&thread->md.pthread_mutex);
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
        return PR_FAILURE;
    }
    thread->flags |= _PR_GLOBAL_SCOPE;

    pthread_attr_init(&attr); /* initialize attr with default attributes */
    if (pthread_attr_setstacksize(&attr, (size_t) stackSize) != 0) {
        pthread_mutex_destroy(&thread->md.pthread_mutex);
        pthread_cond_destroy(&thread->md.pthread_cond);
        pthread_attr_destroy(&attr);
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
        return PR_FAILURE;
    }

    thread->md.wait = 0;
    rv = pthread_create(&thread->md.pthread, &attr, start, (void *)thread);
    if (0 == rv) {
        _MD_ATOMIC_INCREMENT(&_pr_md_pthreads_created);
        _MD_ATOMIC_INCREMENT(&_pr_md_pthreads);
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
        return PR_SUCCESS;
    } else {
        pthread_mutex_destroy(&thread->md.pthread_mutex);
        pthread_cond_destroy(&thread->md.pthread_cond);
        pthread_attr_destroy(&attr);
        _MD_ATOMIC_INCREMENT(&_pr_md_pthreads_failed);
        if (!_PR_IS_NATIVE_THREAD(me)) {
            _PR_FAST_INTSON(is);
        }
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, rv);
        return PR_FAILURE;
    }
}

PR_IMPLEMENT(void)
_MD_InitRunningCPU(struct _PRCPU *cpu)
{
    extern int _pr_md_pipefd[2];

    _MD_unix_init_running_cpu(cpu);
    cpu->md.pthread = pthread_self();
    if (_pr_md_pipefd[0] >= 0) {
        _PR_IOQ_MAX_OSFD(cpu) = _pr_md_pipefd[0];
#ifndef _PR_USE_POLL
        FD_SET(_pr_md_pipefd[0], &_PR_FD_READ_SET(cpu));
#endif
    }
}


void
_MD_CleanupBeforeExit(void)
{
#if 0
    extern PRInt32    _pr_cpus_exit;

    _pr_irix_exit_now = 1;
    if (_pr_numCPU > 1) {
        /*
         * Set a global flag, and wakeup all cpus which will notice the flag
         * and exit.
         */
        _pr_cpus_exit = getpid();
        _MD_Wakeup_CPUs();
        while(_pr_numCPU > 1) {
            _PR_WAIT_SEM(_pr_irix_exit_sem);
            _pr_numCPU--;
        }
    }
    /*
     * cause global threads on the recycle list to exit
     */
    _PR_DEADQ_LOCK;
    if (_PR_NUM_DEADNATIVE != 0) {
        PRThread *thread;
        PRCList *ptr;

        ptr = _PR_DEADNATIVEQ.next;
        while( ptr != &_PR_DEADNATIVEQ ) {
            thread = _PR_THREAD_PTR(ptr);
            _MD_CVAR_POST_SEM(thread);
            ptr = ptr->next;
        }
    }
    _PR_DEADQ_UNLOCK;
    while(_PR_NUM_DEADNATIVE > 1) {
        _PR_WAIT_SEM(_pr_irix_exit_sem);
        _PR_DEC_DEADNATIVE;
    }
#endif
}
