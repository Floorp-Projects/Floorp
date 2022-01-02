/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Windows NT IO module
 *
 * This module handles IO for LOCAL_SCOPE and GLOBAL_SCOPE threads.
 * For LOCAL_SCOPE threads, we're using NT fibers.  For GLOBAL_SCOPE threads
 * we're using NT-native threads.
 *
 * When doing IO, we want to use completion ports for optimal performance
 * with fibers.  But if we use completion ports for all IO, it is difficult
 * to project a blocking model with GLOBAL_SCOPE threads.  To handle this
 * we create an extra thread for completing IO for GLOBAL_SCOPE threads.
 * We don't really want to complete IO on a separate thread for LOCAL_SCOPE
 * threads because it means extra context switches, which are really slow
 * on NT...  Since we're using a single completion port, some IO will
 * be incorrectly completed on the GLOBAL_SCOPE IO thread; this will mean
 * extra context switching; but I don't think there is anything I can do
 * about it.
 */

#include "primpl.h"
#include "pprmwait.h"
#include <direct.h>
#include <mbstring.h>

static HANDLE                _pr_completion_port;
static PRThread             *_pr_io_completion_thread;

#define RECYCLE_SIZE 512
static struct _MDLock        _pr_recycle_lock;
static PRInt32               _pr_recycle_INET_array[RECYCLE_SIZE];
static PRInt32               _pr_recycle_INET_tail = 0;
static PRInt32               _pr_recycle_INET6_array[RECYCLE_SIZE];
static PRInt32               _pr_recycle_INET6_tail = 0;

__declspec(thread) PRThread *_pr_io_restarted_io = NULL;
DWORD _pr_io_restartedIOIndex;  /* The thread local storage slot for each
                                 * thread is initialized to NULL. */

PRBool                       _nt_version_gets_lockfile_completion;

struct _MDLock               _pr_ioq_lock;
extern _MDLock               _nt_idleLock;
extern PRCList               _nt_idleList;
extern PRUint32              _nt_idleCount;

#define CLOSE_TIMEOUT   PR_SecondsToInterval(5)

/*
 * NSPR-to-NT access right mapping table for files.
 */
static DWORD fileAccessTable[] = {
    FILE_GENERIC_READ,
    FILE_GENERIC_WRITE,
    FILE_GENERIC_EXECUTE
};

/*
 * NSPR-to-NT access right mapping table for directories.
 */
static DWORD dirAccessTable[] = {
    FILE_GENERIC_READ,
    FILE_GENERIC_WRITE|FILE_DELETE_CHILD,
    FILE_GENERIC_EXECUTE
};

static PRBool IsPrevCharSlash(const char *str, const char *current);

#define _NEED_351_FILE_LOCKING_HACK
#ifdef _NEED_351_FILE_LOCKING_HACK
#define _PR_LOCAL_FILE 1
#define _PR_REMOTE_FILE 2
PRBool IsFileLocalInit();
PRInt32 IsFileLocal(HANDLE hFile);
#endif /* _NEED_351_FILE_LOCKING_HACK */

static PRInt32 _md_MakeNonblock(HANDLE);

static PROsfd _nt_nonblock_accept(PRFileDesc *fd, struct sockaddr *addr, int *addrlen, PRIntervalTime);
static PRInt32 _nt_nonblock_connect(PRFileDesc *fd, struct sockaddr *addr, int addrlen, PRIntervalTime);
static PRInt32 _nt_nonblock_recv(PRFileDesc *fd, char *buf, int len, int flags, PRIntervalTime);
static PRInt32 _nt_nonblock_send(PRFileDesc *fd, char *buf, int len, PRIntervalTime);
static PRInt32 _nt_nonblock_writev(PRFileDesc *fd, const PRIOVec *iov, int size, PRIntervalTime);
static PRInt32 _nt_nonblock_sendto(PRFileDesc *, const char *, int, const struct sockaddr *, int, PRIntervalTime);
static PRInt32 _nt_nonblock_recvfrom(PRFileDesc *, char *, int, struct sockaddr *, int *, PRIntervalTime);

/*
 * We cannot associate a fd (a socket) with an I/O completion port
 * if the fd is nonblocking or inheritable.
 *
 * Nonblocking socket I/O won't work if the socket is associated with
 * an I/O completion port.
 *
 * An inheritable fd cannot be associated with an I/O completion port
 * because the completion notification of async I/O initiated by the
 * child process is still posted to the I/O completion port in the
 * parent process.
 */
#define _NT_USE_NB_IO(fd) \
    ((fd)->secret->nonblocking || (fd)->secret->inheritable == _PR_TRI_TRUE)

/*
 * UDP support
 *
 * UDP is supported on NT by the continuation thread mechanism.
 * The code is borrowed from ptio.c in pthreads nspr, hence the
 * PT and pt prefixes.  This mechanism is in fact general and
 * not limited to UDP.  For now, only UDP's recvfrom and sendto
 * go through the continuation thread if they get WSAEWOULDBLOCK
 * on first try.  Recv and send on a connected UDP socket still
 * goes through asychronous io.
 */

#define PT_DEFAULT_SELECT_MSEC 100

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revent);

typedef enum pr_ContuationStatus
{
    pt_continuation_sumbitted,
    pt_continuation_inprogress,
    pt_continuation_abort,
    pt_continuation_done
} pr_ContuationStatus;

struct pt_Continuation
{
    /* These objects are linked in ascending timeout order */
    pt_Continuation *next, *prev;           /* self linked list of these things */

    /* The building of the continuation operation */
    ContinuationFn function;                /* what function to continue */
    union {
        SOCKET osfd;
    } arg1;            /* #1 - the op's fd */
    union {
        void* buffer;
    } arg2;           /* #2 - primary transfer buffer */
    union {
        PRIntn amount;
    } arg3;          /* #3 - size of 'buffer' */
    union {
        PRIntn flags;
    } arg4;           /* #4 - read/write flags */
    union {
        PRNetAddr *addr;
    } arg5;        /* #5 - send/recv address */

    PRIntervalTime timeout;                 /* representation of the timeout */

    PRIntn event;                           /* flags for select()'s events */

    /*
    ** The representation and notification of the results of the operation.
    ** These function can either return an int return code or a pointer to
    ** some object.
    */
    union {
        PRIntn code;
        void *object;
    } result;

    PRIntn syserrno;                        /* in case it failed, why (errno) */
    pr_ContuationStatus status;             /* the status of the operation */
    PRCondVar *complete;                    /* to notify the initiating thread */
};

static struct pt_TimedQueue
{
    PRLock *ml;                             /* a little protection */
    PRThread *thread;                       /* internal thread's identification */
    PRCondVar *new_op;                      /* new operation supplied */
    PRCondVar *finish_op;                   /* an existing operation finished */
    PRUintn op_count;                       /* number of operations in the list */
    pt_Continuation *head, *tail;           /* head/tail of list of operations */

    pt_Continuation *op;                    /* timed operation furthest in future */
    PRIntervalTime epoch;                   /* the epoch of 'timed' */
} pt_tq;

#if defined(DEBUG)
static struct pt_debug_s
{
    PRIntn predictionsFoiled;
    PRIntn pollingListMax;
    PRIntn continuationsServed;
} pt_debug;
#endif  /* DEBUG */

static void ContinuationThread(void *arg);
static PRInt32 pt_SendTo(
    SOCKET osfd, const void *buf,
    PRInt32 amount, PRInt32 flags, const PRNetAddr *addr,
    PRIntn addrlen, PRIntervalTime timeout);
static PRInt32 pt_RecvFrom(SOCKET osfd, void *buf, PRInt32 amount,
                           PRInt32 flags, PRNetAddr *addr, PRIntn *addr_len, PRIntervalTime timeout);


/* The key returned from GetQueuedCompletionStatus() is used to determine what
 * type of completion we have.  We differentiate between IO completions and
 * CVAR completions.
 */
#define KEY_IO              0xaaaaaaaa
#define KEY_CVAR            0xbbbbbbbb

PRInt32
_PR_MD_PAUSE_CPU(PRIntervalTime ticks)
{
    int awoken = 0;
    unsigned long bytes, key;
    int rv;
    LPOVERLAPPED olp;
    _MDOverlapped *mdOlp;
    PRUint32 timeout;

    if (_nt_idleCount > 0) {
        PRThread *deadThread;

        _MD_LOCK(&_nt_idleLock);
        while( !PR_CLIST_IS_EMPTY(&_nt_idleList) ) {
            deadThread = _PR_THREAD_PTR(PR_LIST_HEAD(&_nt_idleList));
            PR_REMOVE_LINK(&deadThread->links);

            PR_ASSERT(deadThread->state == _PR_DEAD_STATE);

            /* XXXMB - cleanup to do here? */
            if ( !_PR_IS_NATIVE_THREAD(deadThread) ) {
                /* Spinlock while user thread is still running.
                 * There is no way to use a condition variable here. The thread
                 * is dead, and we have to wait until we switch off the dead
                 * thread before we can kill the fiber completely.
                 */
                while ( deadThread->no_sched)
                    ;

                DeleteFiber(deadThread->md.fiber_id);
            }
            memset(deadThread, 0xa, sizeof(PRThread)); /* debugging */
            if (!deadThread->threadAllocatedOnStack) {
                PR_DELETE(deadThread);
            }
            _nt_idleCount--;
        }
        _MD_UNLOCK(&_nt_idleLock);
    }

    if (ticks == PR_INTERVAL_NO_TIMEOUT)
#if 0
        timeout = INFINITE;
#else
        /*
         * temporary hack to poll the runq every 5 seconds because of bug in
         * native threads creating user threads and not poking the right cpu.
         *
         * A local thread that was interrupted is bound to its current
         * cpu but there is no easy way for the interrupter to poke the
         * right cpu.  This is a hack to poll the runq every 5 seconds.
         */
        timeout = 5000;
#endif
    else {
        timeout = PR_IntervalToMilliseconds(ticks);
    }

    /*
     * The idea of looping here is to complete as many IOs as possible before
     * returning.  This should minimize trips to the idle thread.
     */
    while(1) {
        rv = GetQueuedCompletionStatus(
                 _pr_completion_port,
                 &bytes,
                 &key,
                 &olp,
                 timeout);
        if (rv == 0 && olp == NULL) {
            /* Error in GetQueuedCompetionStatus */
            if (GetLastError() != WAIT_TIMEOUT) {
                /* ARGH - what can we do here? Log an error? XXXMB */
                return -1;
            } else {
                /* If awoken == 0, then we just had a timeout */
                return awoken;
            }
        }

        if (olp == NULL) {
            return 0;
        }

        mdOlp = (_MDOverlapped *)olp;

        if (mdOlp->ioModel == _MD_MultiWaitIO) {
            PRRecvWait *desc;
            PRWaitGroup *group;
            PRThread *thred = NULL;
            PRMWStatus mwstatus;

            desc = mdOlp->data.mw.desc;
            PR_ASSERT(desc != NULL);
            mwstatus = rv ? PR_MW_SUCCESS : PR_MW_FAILURE;
            if (InterlockedCompareExchange((PVOID *)&desc->outcome,
                                           (PVOID)mwstatus, (PVOID)PR_MW_PENDING)
                == (PVOID)PR_MW_PENDING) {
                if (mwstatus == PR_MW_SUCCESS) {
                    desc->bytesRecv = bytes;
                } else {
                    mdOlp->data.mw.error = GetLastError();
                }
            }
            group = mdOlp->data.mw.group;
            PR_ASSERT(group != NULL);

            _PR_MD_LOCK(&group->mdlock);
            PR_APPEND_LINK(&mdOlp->data.mw.links, &group->io_ready);
            PR_ASSERT(desc->fd != NULL);
            NT_HashRemoveInternal(group, desc->fd);
            if (!PR_CLIST_IS_EMPTY(&group->wait_list)) {
                thred = _PR_THREAD_CONDQ_PTR(PR_LIST_HEAD(&group->wait_list));
                PR_REMOVE_LINK(&thred->waitQLinks);
            }
            _PR_MD_UNLOCK(&group->mdlock);

            if (thred) {
                if (!_PR_IS_NATIVE_THREAD(thred)) {
                    int pri = thred->priority;
                    _PRCPU *lockedCPU = _PR_MD_CURRENT_CPU();
                    _PR_THREAD_LOCK(thred);
                    if (thred->flags & _PR_ON_PAUSEQ) {
                        _PR_SLEEPQ_LOCK(thred->cpu);
                        _PR_DEL_SLEEPQ(thred, PR_TRUE);
                        _PR_SLEEPQ_UNLOCK(thred->cpu);
                        _PR_THREAD_UNLOCK(thred);
                        thred->cpu = lockedCPU;
                        thred->state = _PR_RUNNABLE;
                        _PR_RUNQ_LOCK(lockedCPU);
                        _PR_ADD_RUNQ(thred, lockedCPU, pri);
                        _PR_RUNQ_UNLOCK(lockedCPU);
                    } else {
                        /*
                         * The thread was just interrupted and moved
                         * from the pause queue to the run queue.
                         */
                        _PR_THREAD_UNLOCK(thred);
                    }
                } else {
                    _PR_THREAD_LOCK(thred);
                    thred->state = _PR_RUNNABLE;
                    _PR_THREAD_UNLOCK(thred);
                    ReleaseSemaphore(thred->md.blocked_sema, 1, NULL);
                }
            }
        } else {
            PRThread *completed_io;

            PR_ASSERT(mdOlp->ioModel == _MD_BlockingIO);
            completed_io = _PR_THREAD_MD_TO_PTR(mdOlp->data.mdThread);
            completed_io->md.blocked_io_status = rv;
            if (rv == 0) {
                completed_io->md.blocked_io_error = GetLastError();
            }
            completed_io->md.blocked_io_bytes = bytes;

            if ( !_PR_IS_NATIVE_THREAD(completed_io) ) {
                int pri = completed_io->priority;
                _PRCPU *lockedCPU = _PR_MD_CURRENT_CPU();

                /* The KEY_CVAR notification only occurs when a native thread
                 * is notifying a user thread.  For user-user notifications
                 * the wakeup occurs by having the notifier place the thread
                 * on the runq directly; for native-native notifications the
                 * wakeup occurs by calling ReleaseSemaphore.
                 */
                if ( key == KEY_CVAR ) {
                    PR_ASSERT(completed_io->io_pending == PR_FALSE);
                    PR_ASSERT(completed_io->io_suspended == PR_FALSE);
                    PR_ASSERT(completed_io->md.thr_bound_cpu == NULL);

                    /* Thread has already been deleted from sleepQ */

                    /* Switch CPU and add to runQ */
                    completed_io->cpu = lockedCPU;
                    completed_io->state = _PR_RUNNABLE;
                    _PR_RUNQ_LOCK(lockedCPU);
                    _PR_ADD_RUNQ(completed_io, lockedCPU, pri);
                    _PR_RUNQ_UNLOCK(lockedCPU);
                } else {
                    PR_ASSERT(key == KEY_IO);
                    PR_ASSERT(completed_io->io_pending == PR_TRUE);

                    _PR_THREAD_LOCK(completed_io);

                    completed_io->io_pending = PR_FALSE;

                    /* If io_suspended is true, then this IO has already resumed.
                     * We don't need to do anything; because the thread is
                     * already running.
                     */
                    if (completed_io->io_suspended == PR_FALSE) {
                        if (completed_io->flags & (_PR_ON_SLEEPQ|_PR_ON_PAUSEQ)) {
                            _PR_SLEEPQ_LOCK(completed_io->cpu);
                            _PR_DEL_SLEEPQ(completed_io, PR_TRUE);
                            _PR_SLEEPQ_UNLOCK(completed_io->cpu);

                            _PR_THREAD_UNLOCK(completed_io);

                            /*
                             * If an I/O operation is suspended, the thread
                             * must be running on the same cpu on which the
                             * I/O operation was issued.
                             */
                            PR_ASSERT(!completed_io->md.thr_bound_cpu ||
                                      (completed_io->cpu == completed_io->md.thr_bound_cpu));

                            if (!completed_io->md.thr_bound_cpu) {
                                completed_io->cpu = lockedCPU;
                            }
                            completed_io->state = _PR_RUNNABLE;
                            _PR_RUNQ_LOCK(completed_io->cpu);
                            _PR_ADD_RUNQ(completed_io, completed_io->cpu, pri);
                            _PR_RUNQ_UNLOCK(completed_io->cpu);
                        } else {
                            _PR_THREAD_UNLOCK(completed_io);
                        }
                    } else {
                        _PR_THREAD_UNLOCK(completed_io);
                    }
                }
            } else {
                /* For native threads, they are only notified through this loop
                 * when completing IO.  So, don't worry about this being a CVAR
                 * notification, because that is not possible.
                 */
                _PR_THREAD_LOCK(completed_io);
                completed_io->io_pending = PR_FALSE;
                if (completed_io->io_suspended == PR_FALSE) {
                    completed_io->state = _PR_RUNNABLE;
                    _PR_THREAD_UNLOCK(completed_io);
                    rv = ReleaseSemaphore(completed_io->md.blocked_sema,
                                          1, NULL);
                    PR_ASSERT(0 != rv);
                } else {
                    _PR_THREAD_UNLOCK(completed_io);
                }
            }
        }

        awoken++;
        timeout = 0;   /* Don't block on subsequent trips through the loop */
    }

    /* never reached */
    return 0;
}

static PRStatus
_native_thread_md_wait(PRThread *thread, PRIntervalTime ticks)
{
    DWORD rv;
    PRUint32 msecs = (ticks == PR_INTERVAL_NO_TIMEOUT) ?
                     INFINITE : PR_IntervalToMilliseconds(ticks);

    /*
     * thread waiting for a cvar or a joining thread
     */
    rv = WaitForSingleObject(thread->md.blocked_sema, msecs);
    switch(rv) {
        case WAIT_OBJECT_0:
            return PR_SUCCESS;
            break;
        case WAIT_TIMEOUT:
            _PR_THREAD_LOCK(thread);
            PR_ASSERT (thread->state != _PR_IO_WAIT);
            if (thread->wait.cvar != NULL) {
                PR_ASSERT(thread->state == _PR_COND_WAIT);
                thread->wait.cvar = NULL;
                thread->state = _PR_RUNNING;
                _PR_THREAD_UNLOCK(thread);
            } else {
                /* The CVAR was notified just as the timeout
                 * occurred.  This left the semaphore in the
                 * signaled state.  Call WaitForSingleObject()
                 * to clear the semaphore.
                 */
                _PR_THREAD_UNLOCK(thread);
                rv = WaitForSingleObject(thread->md.blocked_sema, INFINITE);
                PR_ASSERT(rv == WAIT_OBJECT_0);
            }
            return PR_SUCCESS;
            break;
        default:
            return PR_FAILURE;
            break;
    }

    return PR_SUCCESS;
}

PRStatus
_PR_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    DWORD rv;

    if (_native_threads_only) {
        return(_native_thread_md_wait(thread, ticks));
    }
    if ( thread->flags & _PR_GLOBAL_SCOPE ) {
        PRUint32 msecs = (ticks == PR_INTERVAL_NO_TIMEOUT) ?
                         INFINITE : PR_IntervalToMilliseconds(ticks);
        rv = WaitForSingleObject(thread->md.blocked_sema, msecs);
        switch(rv) {
            case WAIT_OBJECT_0:
                return PR_SUCCESS;
                break;
            case WAIT_TIMEOUT:
                _PR_THREAD_LOCK(thread);
                if (thread->state == _PR_IO_WAIT) {
                    if (thread->io_pending == PR_TRUE) {
                        thread->state = _PR_RUNNING;
                        thread->io_suspended = PR_TRUE;
                        _PR_THREAD_UNLOCK(thread);
                    } else {
                        /* The IO completed just at the same time the timeout
                         * occurred.  This left the semaphore in the signaled
                         * state.  Call WaitForSingleObject() to clear the
                         * semaphore.
                         */
                        _PR_THREAD_UNLOCK(thread);
                        rv = WaitForSingleObject(thread->md.blocked_sema, INFINITE);
                        PR_ASSERT(rv == WAIT_OBJECT_0);
                    }
                } else {
                    if (thread->wait.cvar != NULL) {
                        PR_ASSERT(thread->state == _PR_COND_WAIT);
                        thread->wait.cvar = NULL;
                        thread->state = _PR_RUNNING;
                        _PR_THREAD_UNLOCK(thread);
                    } else {
                        /* The CVAR was notified just as the timeout
                         * occurred.  This left the semaphore in the
                         * signaled state.  Call WaitForSingleObject()
                         * to clear the semaphore.
                         */
                        _PR_THREAD_UNLOCK(thread);
                        rv = WaitForSingleObject(thread->md.blocked_sema, INFINITE);
                        PR_ASSERT(rv == WAIT_OBJECT_0);
                    }
                }
                return PR_SUCCESS;
                break;
            default:
                return PR_FAILURE;
                break;
        }
    } else {
        PRInt32 is;

        _PR_INTSOFF(is);
        _PR_MD_SWITCH_CONTEXT(thread);
    }

    return PR_SUCCESS;
}

static void
_native_thread_io_nowait(
    PRThread *thread,
    int rv,
    int bytes)
{
    int rc;

    PR_ASSERT(rv != 0);
    _PR_THREAD_LOCK(thread);
    if (thread->state == _PR_IO_WAIT) {
        PR_ASSERT(thread->io_suspended == PR_FALSE);
        PR_ASSERT(thread->io_pending == PR_TRUE);
        thread->state = _PR_RUNNING;
        thread->io_pending = PR_FALSE;
        _PR_THREAD_UNLOCK(thread);
    } else {
        /* The IO completed just at the same time the
         * thread was interrupted. This left the semaphore
         * in the signaled state. Call WaitForSingleObject()
         * to clear the semaphore.
         */
        PR_ASSERT(thread->io_suspended == PR_TRUE);
        PR_ASSERT(thread->io_pending == PR_TRUE);
        thread->io_pending = PR_FALSE;
        _PR_THREAD_UNLOCK(thread);
        rc = WaitForSingleObject(thread->md.blocked_sema, INFINITE);
        PR_ASSERT(rc == WAIT_OBJECT_0);
    }

    thread->md.blocked_io_status = rv;
    thread->md.blocked_io_bytes = bytes;
    rc = ResetEvent(thread->md.thr_event);
    PR_ASSERT(rc != 0);
    return;
}

static PRStatus
_native_thread_io_wait(PRThread *thread, PRIntervalTime ticks)
{
    DWORD rv, bytes;
#define _NATIVE_IO_WAIT_HANDLES     2
#define _NATIVE_WAKEUP_EVENT_INDEX  0
#define _NATIVE_IO_EVENT_INDEX      1

    HANDLE wait_handles[_NATIVE_IO_WAIT_HANDLES];

    PRUint32 msecs = (ticks == PR_INTERVAL_NO_TIMEOUT) ?
                     INFINITE : PR_IntervalToMilliseconds(ticks);

    PR_ASSERT(thread->flags & _PR_GLOBAL_SCOPE);

    wait_handles[0] = thread->md.blocked_sema;
    wait_handles[1] = thread->md.thr_event;
    rv = WaitForMultipleObjects(_NATIVE_IO_WAIT_HANDLES, wait_handles,
                                FALSE, msecs);

    switch(rv) {
        case WAIT_OBJECT_0 + _NATIVE_IO_EVENT_INDEX:
            /*
             * I/O op completed
             */
            _PR_THREAD_LOCK(thread);
            if (thread->state == _PR_IO_WAIT) {

                PR_ASSERT(thread->io_suspended == PR_FALSE);
                PR_ASSERT(thread->io_pending == PR_TRUE);
                thread->state = _PR_RUNNING;
                thread->io_pending = PR_FALSE;
                _PR_THREAD_UNLOCK(thread);
            } else {
                /* The IO completed just at the same time the
                 * thread was interrupted. This led to us being
                 * notified twice. Call WaitForSingleObject()
                 * to clear the semaphore.
                 */
                PR_ASSERT(thread->io_suspended == PR_TRUE);
                PR_ASSERT(thread->io_pending == PR_TRUE);
                thread->io_pending = PR_FALSE;
                _PR_THREAD_UNLOCK(thread);
                rv = WaitForSingleObject(thread->md.blocked_sema,
                                         INFINITE);
                PR_ASSERT(rv == WAIT_OBJECT_0);
            }

            rv = GetOverlappedResult((HANDLE) thread->io_fd,
                                     &thread->md.overlapped.overlapped, &bytes, FALSE);

            thread->md.blocked_io_status = rv;
            if (rv != 0) {
                thread->md.blocked_io_bytes = bytes;
            } else {
                thread->md.blocked_io_error = GetLastError();
                PR_ASSERT(ERROR_IO_PENDING != thread->md.blocked_io_error);
            }
            rv = ResetEvent(thread->md.thr_event);
            PR_ASSERT(rv != 0);
            break;
        case WAIT_OBJECT_0 + _NATIVE_WAKEUP_EVENT_INDEX:
            /*
             * I/O interrupted;
             */
#ifdef DEBUG
            _PR_THREAD_LOCK(thread);
            PR_ASSERT(thread->io_suspended == PR_TRUE);
            _PR_THREAD_UNLOCK(thread);
#endif
            break;
        case WAIT_TIMEOUT:
            _PR_THREAD_LOCK(thread);
            if (thread->state == _PR_IO_WAIT) {
                thread->state = _PR_RUNNING;
                thread->io_suspended = PR_TRUE;
                _PR_THREAD_UNLOCK(thread);
            } else {
                /*
                 * The thread was interrupted just as the timeout
                 * occurred. This left the semaphore in the signaled
                 * state. Call WaitForSingleObject() to clear the
                 * semaphore.
                 */
                PR_ASSERT(thread->io_suspended == PR_TRUE);
                _PR_THREAD_UNLOCK(thread);
                rv = WaitForSingleObject(thread->md.blocked_sema, INFINITE);
                PR_ASSERT(rv == WAIT_OBJECT_0);
            }
            break;
        default:
            return PR_FAILURE;
            break;
    }

    return PR_SUCCESS;
}


static PRStatus
_NT_IO_WAIT(PRThread *thread, PRIntervalTime timeout)
{
    PRBool fWait = PR_TRUE;

    if (_native_threads_only) {
        return(_native_thread_io_wait(thread, timeout));
    }
    if (!_PR_IS_NATIVE_THREAD(thread))  {

        _PR_THREAD_LOCK(thread);

        /* The IO may have already completed; if so, don't add to sleepQ,
         * since we are already on the runQ!
         */
        if (thread->io_pending == PR_TRUE) {
            _PR_SLEEPQ_LOCK(thread->cpu);
            _PR_ADD_SLEEPQ(thread, timeout);
            _PR_SLEEPQ_UNLOCK(thread->cpu);
        } else {
            fWait = PR_FALSE;
        }
        _PR_THREAD_UNLOCK(thread);
    }
    if (fWait) {
        return _PR_MD_WAIT(thread, timeout);
    }
    else {
        return PR_SUCCESS;
    }
}

/*
 * Unblock threads waiting for I/O
 * used when interrupting threads
 *
 * NOTE: The thread lock should held when this function is called.
 * On return, the thread lock is released.
 */
void _PR_Unblock_IO_Wait(PRThread *thr)
{
    PRStatus rv;
    _PRCPU *cpu = thr->cpu;

    PR_ASSERT(thr->state == _PR_IO_WAIT);
    /*
     * A thread for which an I/O timed out or was interrupted cannot be
     * in an IO_WAIT state except as a result of calling PR_Close or
     * PR_NT_CancelIo for the FD. For these two cases, _PR_IO_WAIT state
     * is not interruptible
     */
    if (thr->md.interrupt_disabled == PR_TRUE) {
        _PR_THREAD_UNLOCK(thr);
        return;
    }
    thr->io_suspended = PR_TRUE;
    thr->state = _PR_RUNNABLE;

    if (!_PR_IS_NATIVE_THREAD(thr)) {
        PRThread *me = _PR_MD_CURRENT_THREAD();
        PR_ASSERT(thr->flags & (_PR_ON_SLEEPQ | _PR_ON_PAUSEQ));
        _PR_SLEEPQ_LOCK(cpu);
        _PR_DEL_SLEEPQ(thr, PR_TRUE);
        _PR_SLEEPQ_UNLOCK(cpu);
        /*
         * this thread will continue to run on the same cpu until the
         * I/O is aborted by closing the FD or calling CancelIO
         */
        thr->md.thr_bound_cpu = cpu;

        PR_ASSERT(!(thr->flags & _PR_IDLE_THREAD));
        _PR_AddThreadToRunQ(me, thr);
    }
    _PR_THREAD_UNLOCK(thr);
    rv = _PR_MD_WAKEUP_WAITER(thr);
    PR_ASSERT(PR_SUCCESS == rv);
}

/* Resume an outstanding IO; requires that after the switch, we disable */
static PRStatus
_NT_ResumeIO(PRThread *thread, PRIntervalTime ticks)
{
    PRBool fWait = PR_TRUE;

    if (!_PR_IS_NATIVE_THREAD(thread)) {
        if (_pr_use_static_tls) {
            _pr_io_restarted_io = thread;
        } else {
            TlsSetValue(_pr_io_restartedIOIndex, thread);
        }
    } else {
        _PR_THREAD_LOCK(thread);
        if (!thread->io_pending) {
            fWait = PR_FALSE;
        }
        thread->io_suspended = PR_FALSE;

        _PR_THREAD_UNLOCK(thread);
    }
    /* We don't put ourselves back on the sleepQ yet; until we
     * set the suspended bit to false, we can't do that.  Just save
     * the sleep time here, and then continue.  The restarted_io handler
     * will add us to the sleepQ if needed.
     */
    thread->sleep = ticks;

    if (fWait) {
        if (!_PR_IS_NATIVE_THREAD(thread)) {
            return _PR_MD_WAIT(thread, ticks);
        }
        else {
            return _NT_IO_WAIT(thread, ticks);
        }
    }
    return PR_SUCCESS;
}

PRStatus
_PR_MD_WAKEUP_WAITER(PRThread *thread)
{
    if (thread == NULL) {
        /* If thread is NULL, we aren't waking a thread, we're just poking
         * idle thread
         */
        if ( PostQueuedCompletionStatus(_pr_completion_port, 0,
                                        KEY_CVAR, NULL) == FALSE) {
            return PR_FAILURE;
        }
        return PR_SUCCESS;
    }

    if ( _PR_IS_NATIVE_THREAD(thread) ) {
        if (ReleaseSemaphore(thread->md.blocked_sema, 1, NULL) == FALSE) {
            return PR_FAILURE;
        }
        else {
            return PR_SUCCESS;
        }
    } else {
        PRThread *me = _PR_MD_CURRENT_THREAD();

        /* When a Native thread has to awaken a user thread, it has to poke
         * the completion port because all user threads might be idle, and
         * thus the CPUs are just waiting for a completion.
         *
         * XXXMB - can we know when we are truely idle (and not checking
         *         the runq)?
         */
        if ((_PR_IS_NATIVE_THREAD(me) || (thread->cpu != me->cpu)) &&
            (!thread->md.thr_bound_cpu)) {
            /* The thread should not be in any queue */
            PR_ASSERT(thread->queueCount == 0);
            if ( PostQueuedCompletionStatus(_pr_completion_port, 0,
                                            KEY_CVAR, &(thread->md.overlapped.overlapped)) == FALSE) {
                return PR_FAILURE;
            }
        }
        return PR_SUCCESS;
    }
}

void
_PR_MD_INIT_IO()
{
    WORD WSAVersion = 0x0101;
    WSADATA WSAData;
    int err;
    OSVERSIONINFO OSversion;

    err = WSAStartup( WSAVersion, &WSAData );
    PR_ASSERT(0 == err);

    _pr_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                          NULL,
                          0,
                          0);

    _MD_NEW_LOCK(&_pr_recycle_lock);
    _MD_NEW_LOCK(&_pr_ioq_lock);

    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&OSversion)) {
        _nt_version_gets_lockfile_completion = PR_FALSE;
        if (OSversion.dwMajorVersion >= 4) {
            _nt_version_gets_lockfile_completion = PR_TRUE;
        }
    } else {
        PR_ASSERT(0);
    }

#ifdef _NEED_351_FILE_LOCKING_HACK
    IsFileLocalInit();
#endif /* _NEED_351_FILE_LOCKING_HACK */

    /*
     * UDP support: start up the continuation thread
     */

    pt_tq.op_count = 0;
    pt_tq.head = pt_tq.tail = NULL;
    pt_tq.ml = PR_NewLock();
    PR_ASSERT(NULL != pt_tq.ml);
    pt_tq.new_op = PR_NewCondVar(pt_tq.ml);
    PR_ASSERT(NULL != pt_tq.new_op);
#if defined(DEBUG)
    memset(&pt_debug, 0, sizeof(struct pt_debug_s));
#endif

    pt_tq.thread = PR_CreateThread(
                       PR_SYSTEM_THREAD, ContinuationThread, NULL,
                       PR_PRIORITY_URGENT, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);

    PR_ASSERT(NULL != pt_tq.thread);

#ifdef DEBUG
    /* Doublecheck _pr_filetime_offset's hard-coded value is correct. */
    {
        SYSTEMTIME systime;
        union {
            PRTime prt;
            FILETIME ft;
        } filetime;
        BOOL rv;

        systime.wYear = 1970;
        systime.wMonth = 1;
        /* wDayOfWeek is ignored */
        systime.wDay = 1;
        systime.wHour = 0;
        systime.wMinute = 0;
        systime.wSecond = 0;
        systime.wMilliseconds = 0;

        rv = SystemTimeToFileTime(&systime, &filetime.ft);
        PR_ASSERT(0 != rv);
        PR_ASSERT(filetime.prt == _pr_filetime_offset);
    }
#endif /* DEBUG */

    _PR_NT_InitSids();
}

/* --- SOCKET IO --------------------------------------------------------- */

/* _md_get_recycled_socket()
 * Get a socket from the recycle bin; if no sockets are in the bin,
 * create one.  The socket will be passed to AcceptEx() as the
 * second argument.
 */
static SOCKET
_md_get_recycled_socket(int af)
{
    SOCKET rv;

    _MD_LOCK(&_pr_recycle_lock);
    if (af == AF_INET && _pr_recycle_INET_tail) {
        _pr_recycle_INET_tail--;
        rv = _pr_recycle_INET_array[_pr_recycle_INET_tail];
        _MD_UNLOCK(&_pr_recycle_lock);
        return rv;
    }
    if (af == AF_INET6 && _pr_recycle_INET6_tail) {
        _pr_recycle_INET6_tail--;
        rv = _pr_recycle_INET6_array[_pr_recycle_INET6_tail];
        _MD_UNLOCK(&_pr_recycle_lock);
        return rv;
    }
    _MD_UNLOCK(&_pr_recycle_lock);

    rv = _PR_MD_SOCKET(af, SOCK_STREAM, 0);
    if (rv != INVALID_SOCKET && _md_Associate((HANDLE)rv) == 0) {
        closesocket(rv);
        return INVALID_SOCKET;
    }
    return rv;
}

/* _md_put_recycled_socket()
 * Add a socket to the recycle bin.
 */
static void
_md_put_recycled_socket(SOCKET newsock, int af)
{
    PR_ASSERT(_pr_recycle_INET_tail >= 0);
    PR_ASSERT(_pr_recycle_INET6_tail >= 0);

    _MD_LOCK(&_pr_recycle_lock);
    if (af == AF_INET && _pr_recycle_INET_tail < RECYCLE_SIZE) {
        _pr_recycle_INET_array[_pr_recycle_INET_tail] = newsock;
        _pr_recycle_INET_tail++;
        _MD_UNLOCK(&_pr_recycle_lock);
    } else if (af == AF_INET6 && _pr_recycle_INET6_tail < RECYCLE_SIZE) {
        _pr_recycle_INET6_array[_pr_recycle_INET6_tail] = newsock;
        _pr_recycle_INET6_tail++;
        _MD_UNLOCK(&_pr_recycle_lock);
    } else {
        _MD_UNLOCK(&_pr_recycle_lock);
        closesocket(newsock);
    }

    return;
}

/* _md_Associate()
 * Associates a file with the completion port.
 * Returns 0 on failure, 1 on success.
 */
PRInt32
_md_Associate(HANDLE file)
{
    HANDLE port;

    if (!_native_threads_only) {
        port = CreateIoCompletionPort((HANDLE)file,
                                      _pr_completion_port,
                                      KEY_IO,
                                      0);

        /* XXX should map error codes on failures */
        return (port == _pr_completion_port);
    } else {
        return 1;
    }
}

/*
 * _md_MakeNonblock()
 * Make a socket nonblocking.
 * Returns 0 on failure, 1 on success.
 */
static PRInt32
_md_MakeNonblock(HANDLE file)
{
    int rv;
    u_long one = 1;

    rv = ioctlsocket((SOCKET)file, FIONBIO, &one);
    /* XXX should map error codes on failures */
    return (rv == 0);
}

static int missing_completions = 0;
static int max_wait_loops = 0;

static PRInt32
_NT_IO_ABORT(PROsfd sock)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRBool fWait;
    PRInt32 rv;
    int loop_count;

    /* This is a clumsy way to abort the IO, but it is all we can do.
     * It looks a bit racy, but we handle all the cases.
     * case 1:  IO completes before calling closesocket
     *     case 1a:  fWait is set to PR_FALSE
     *           This should e the most likely case.  We'll properly
     *           not wait call _NT_IO_WAIT, since the closesocket()
     *           won't be forcing a completion.
     *     case 1b: fWait is set to PR_TRUE
     *           This hopefully won't happen much.  When it does, this
     *           thread will timeout in _NT_IO_WAIT for CLOSE_INTERVAL
     *           before cleaning up.
     * case 2:  IO does not complete before calling closesocket
     *     case 2a: IO never completes
     *           This is the likely case.  We'll close it and wait
     *           for the completion forced by the close.  Return should
     *           be immediate.
     *     case 2b: IO completes just after calling closesocket
     *           Since the closesocket is issued, we'll either get a
     *           completion back for the real IO or for the close.  We
     *           don't really care.  It may not even be possible to get
     *           a real completion here.  In any event, we'll awaken
     *           from NT_IO_WAIT immediately.
     */

    _PR_THREAD_LOCK(me);
    fWait = me->io_pending;
    if (fWait) {
        /*
         * If there's still I/O pending, it should have already timed
         * out once before this function is called.
         */
        PR_ASSERT(me->io_suspended == PR_TRUE);

        /* Set up to wait for I/O completion again */
        me->state = _PR_IO_WAIT;
        me->io_suspended = PR_FALSE;
        me->md.interrupt_disabled = PR_TRUE;
    }
    _PR_THREAD_UNLOCK(me);

    /* Close the socket if there is one */
    if (sock != INVALID_SOCKET) {
        rv = closesocket((SOCKET)sock);
    }

    /* If there was I/O pending before the close, wait for it to complete */
    if (fWait) {

        /* Wait and wait for the I/O to complete */
        for (loop_count = 0; fWait; ++loop_count) {

            _NT_IO_WAIT(me, CLOSE_TIMEOUT);

            _PR_THREAD_LOCK(me);
            fWait = me->io_pending;
            if (fWait) {
                PR_ASSERT(me->io_suspended == PR_TRUE);
                me->state = _PR_IO_WAIT;
                me->io_suspended = PR_FALSE;
            }
            _PR_THREAD_UNLOCK(me);

            if (loop_count > max_wait_loops) {
                max_wait_loops = loop_count;
            }
        }

        if (loop_count > 1) {
            ++missing_completions;
        }

        me->md.interrupt_disabled = PR_FALSE;
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
    }

    PR_ASSERT(me->io_pending == PR_FALSE);
    me->md.thr_bound_cpu = NULL;
    me->io_suspended = PR_FALSE;

    return rv;
}


PROsfd
_PR_MD_SOCKET(int af, int type, int flags)
{
    SOCKET sock;

    sock = socket(af, type, flags);

    if (sock == INVALID_SOCKET) {
        _PR_MD_MAP_SOCKET_ERROR(WSAGetLastError());
    }

    return (PROsfd)sock;
}

struct connect_data_s {
    PRInt32 status;
    PRInt32 error;
    PROsfd  osfd;
    struct sockaddr *addr;
    PRUint32 addrlen;
    PRIntervalTime timeout;
};

void
_PR_MD_connect_thread(void *cdata)
{
    struct connect_data_s *cd = (struct connect_data_s *)cdata;

    cd->status = connect(cd->osfd, cd->addr, cd->addrlen);

    if (cd->status == SOCKET_ERROR) {
        cd->error = WSAGetLastError();
    }

    return;
}


PRInt32
_PR_MD_CONNECT(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen,
               PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    u_long nbio;
    PRInt32 rc;

    if (fd->secret->nonblocking) {
        if (!fd->secret->md.io_model_committed) {
            rv = _md_MakeNonblock((HANDLE)osfd);
            PR_ASSERT(0 != rv);
            fd->secret->md.io_model_committed = PR_TRUE;
        }

        if ((rv = connect(osfd, (struct sockaddr *) addr, addrlen)) == -1) {
            err = WSAGetLastError();
            _PR_MD_MAP_CONNECT_ERROR(err);
        }
        return rv;
    }

    /*
     * Temporarily make the socket non-blocking so that we can
     * initiate a non-blocking connect and wait for its completion
     * (with a timeout) in select.
     */
    PR_ASSERT(!fd->secret->md.io_model_committed);
    nbio = 1;
    rv = ioctlsocket((SOCKET)osfd, FIONBIO, &nbio);
    PR_ASSERT(0 == rv);

    rc = _nt_nonblock_connect(fd, (struct sockaddr *) addr, addrlen, timeout);

    /* Set the socket back to blocking. */
    nbio = 0;
    rv = ioctlsocket((SOCKET)osfd, FIONBIO, &nbio);
    PR_ASSERT(0 == rv);

    return rc;
}

PRInt32
_PR_MD_BIND(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen)
{
    PRInt32 rv;
#if 0
    int one = 1;
#endif

    rv = bind(fd->secret->md.osfd, (const struct sockaddr *)&(addr->inet), addrlen);

    if (rv == SOCKET_ERROR) {
        _PR_MD_MAP_BIND_ERROR(WSAGetLastError());
        return -1;
    }

#if 0
    /* Disable nagle- so far unknown if this is good or not...
     */
    rv = setsockopt(fd->secret->md.osfd,
                    SOL_SOCKET,
                    TCP_NODELAY,
                    (const char *)&one,
                    sizeof(one));
    PR_ASSERT(rv == 0);
#endif

    return 0;
}

void _PR_MD_UPDATE_ACCEPT_CONTEXT(PROsfd accept_sock, PROsfd listen_sock)
{
    /* Sockets accept()'d with AcceptEx need to call this setsockopt before
     * calling anything other than ReadFile(), WriteFile(), send(), recv(),
     * Transmitfile(), and closesocket().  In order to call any other
     * winsock functions, we have to make this setsockopt call.
     *
     * XXXMB - For the server, we *NEVER* need this in
     * the "normal" code path.  But now we have to call it.  This is a waste
     * of a system call.  We'd like to only call it before calling the
     * obscure socket calls, but since we don't know at that point what the
     * original socket was (or even if it is still alive) we can't do it
     * at that point...
     */
    setsockopt((SOCKET)accept_sock,
               SOL_SOCKET,
               SO_UPDATE_ACCEPT_CONTEXT,
               (char *)&listen_sock,
               sizeof(listen_sock));

}

#define INET_ADDR_PADDED (sizeof(PRNetAddr) + 16)
PROsfd
_PR_MD_FAST_ACCEPT(PRFileDesc *fd, PRNetAddr *raddr, PRUint32 *rlen,
                   PRIntervalTime timeout, PRBool fast,
                   _PR_AcceptTimeoutCallback callback, void *callbackArg)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    SOCKET accept_sock;
    int bytes;
    PRNetAddr *Laddr;
    PRNetAddr *Raddr;
    PRUint32 llen, err;
    int rv;

    if (_NT_USE_NB_IO(fd)) {
        if (!fd->secret->md.io_model_committed) {
            rv = _md_MakeNonblock((HANDLE)osfd);
            PR_ASSERT(0 != rv);
            fd->secret->md.io_model_committed = PR_TRUE;
        }
        /*
         * The accepted socket inherits the nonblocking and
         * inheritable (HANDLE_FLAG_INHERIT) attributes of
         * the listening socket.
         */
        accept_sock = _nt_nonblock_accept(fd, (struct sockaddr *)raddr, rlen, timeout);
        if (!fd->secret->nonblocking) {
            u_long zero = 0;

            rv = ioctlsocket(accept_sock, FIONBIO, &zero);
            PR_ASSERT(0 == rv);
        }
        return accept_sock;
    }

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return -1;
    }

    if (!fd->secret->md.io_model_committed) {
        rv = _md_Associate((HANDLE)osfd);
        PR_ASSERT(0 != rv);
        fd->secret->md.io_model_committed = PR_TRUE;
    }

    if (!me->md.acceptex_buf) {
        me->md.acceptex_buf = PR_MALLOC(2*INET_ADDR_PADDED);
        if (!me->md.acceptex_buf) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return -1;
        }
    }

    accept_sock = _md_get_recycled_socket(fd->secret->af);
    if (accept_sock == INVALID_SOCKET) {
        return -1;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));
    if (_native_threads_only) {
        me->md.overlapped.overlapped.hEvent = me->md.thr_event;
    }

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        closesocket(accept_sock);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);
    me->io_fd = osfd;

    rv = AcceptEx((SOCKET)osfd,
                  accept_sock,
                  me->md.acceptex_buf,
                  0,
                  INET_ADDR_PADDED,
                  INET_ADDR_PADDED,
                  &bytes,
                  &(me->md.overlapped.overlapped));

    if ( (rv == 0) && ((err = WSAGetLastError()) != ERROR_IO_PENDING))  {
        /* Argh! The IO failed */
        closesocket(accept_sock);
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return -1;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_ACCEPTEX_ERROR(err);
        return -1;
    }

    if (_native_threads_only && rv) {
        _native_thread_io_nowait(me, rv, bytes);
    } else if (_NT_IO_WAIT(me, timeout) == PR_FAILURE) {
        PR_ASSERT(0);
        closesocket(accept_sock);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

    if (me->io_suspended) {
        closesocket(accept_sock);
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        } else {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        }
        return -1;
    }

    if (me->md.blocked_io_status == 0) {
        closesocket(accept_sock);
        _PR_MD_MAP_ACCEPTEX_ERROR(me->md.blocked_io_error);
        return -1;
    }

    if (!fast) {
        _PR_MD_UPDATE_ACCEPT_CONTEXT((SOCKET)accept_sock, (SOCKET)osfd);
    }

    /* IO is done */
    GetAcceptExSockaddrs(
        me->md.acceptex_buf,
        0,
        INET_ADDR_PADDED,
        INET_ADDR_PADDED,
        (LPSOCKADDR *)&(Laddr),
        &llen,
        (LPSOCKADDR *)&(Raddr),
        (unsigned int *)rlen);

    if (raddr != NULL) {
        memcpy((char *)raddr, (char *)&Raddr->inet, *rlen);
    }

    PR_ASSERT(me->io_pending == PR_FALSE);

    return accept_sock;
}

PRInt32
_PR_MD_FAST_ACCEPT_READ(PRFileDesc *sd, PROsfd *newSock, PRNetAddr **raddr,
                        void *buf, PRInt32 amount, PRIntervalTime timeout,
                        PRBool fast, _PR_AcceptTimeoutCallback callback,
                        void *callbackArg)
{
    PROsfd sock = sd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    int bytes;
    PRNetAddr *Laddr;
    PRUint32 llen, rlen, err;
    int rv;
    PRBool isConnected;
    PRBool madeCallback = PR_FALSE;

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return -1;
    }

    if (!sd->secret->md.io_model_committed) {
        rv = _md_Associate((HANDLE)sock);
        PR_ASSERT(0 != rv);
        sd->secret->md.io_model_committed = PR_TRUE;
    }

    *newSock = _md_get_recycled_socket(sd->secret->af);
    if (*newSock == INVALID_SOCKET) {
        return -1;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));
    if (_native_threads_only) {
        me->md.overlapped.overlapped.hEvent = me->md.thr_event;
    }

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        closesocket(*newSock);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);
    me->io_fd = sock;

    rv = AcceptEx((SOCKET)sock,
                  *newSock,
                  buf,
                  amount,
                  INET_ADDR_PADDED,
                  INET_ADDR_PADDED,
                  &bytes,
                  &(me->md.overlapped.overlapped));

    if ( (rv == 0) && ((err = GetLastError()) != ERROR_IO_PENDING)) {
        closesocket(*newSock);
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return -1;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_ACCEPTEX_ERROR(err);
        return -1;
    }

    if (_native_threads_only && rv) {
        _native_thread_io_nowait(me, rv, bytes);
    } else if (_NT_IO_WAIT(me, timeout) == PR_FAILURE) {
        PR_ASSERT(0);
        closesocket(*newSock);
        return -1;
    }

retry:
    if (me->io_suspended) {
        PRInt32 err;
        INT seconds;
        INT bytes = sizeof(seconds);

        PR_ASSERT(timeout != PR_INTERVAL_NO_TIMEOUT);

        err = getsockopt(*newSock,
                         SOL_SOCKET,
                         SO_CONNECT_TIME,
                         (char *)&seconds,
                         (PINT)&bytes);
        if ( err == NO_ERROR ) {
            PRIntervalTime elapsed = PR_SecondsToInterval(seconds);

            if (seconds == 0xffffffff) {
                isConnected = PR_FALSE;
            }
            else {
                isConnected = PR_TRUE;
            }

            if (!isConnected) {
                if (madeCallback == PR_FALSE && callback) {
                    callback(callbackArg);
                }
                madeCallback = PR_TRUE;
                me->state = _PR_IO_WAIT;
                if (_NT_ResumeIO(me, timeout) == PR_FAILURE) {
                    closesocket(*newSock);
                    return -1;
                }
                goto retry;
            }

            if (elapsed < timeout) {
                /* Socket is connected but time not elapsed, RESUME IO */
                timeout -= elapsed;
                me->state = _PR_IO_WAIT;
                if (_NT_ResumeIO(me, timeout) == PR_FAILURE) {
                    closesocket(*newSock);
                    return -1;
                }
                goto retry;
            }
        } else {
            /*  What to do here? Assume socket not open?*/
            PR_ASSERT(0);
            isConnected = PR_FALSE;
        }

        rv = _NT_IO_ABORT(*newSock);

        PR_ASSERT(me->io_pending ==  PR_FALSE);
        PR_ASSERT(me->io_suspended ==  PR_FALSE);
        PR_ASSERT(me->md.thr_bound_cpu ==  NULL);
        /* If the IO is still suspended, it means we didn't get any
         * completion from NT_IO_WAIT.  This is not disasterous, I hope,
         * but it may mean we still have an IO outstanding...  Try to
         * recover by just allowing ourselves to continue.
         */
        me->io_suspended = PR_FALSE;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        } else {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        }
        me->state = _PR_RUNNING;
        closesocket(*newSock);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE);
    PR_ASSERT(me->io_suspended == PR_FALSE);
    PR_ASSERT(me->md.thr_bound_cpu == NULL);

    if (me->md.blocked_io_status == 0) {
        _PR_MD_MAP_ACCEPTEX_ERROR(me->md.blocked_io_error);
        closesocket(*newSock);
        return -1;
    }

    if (!fast) {
        _PR_MD_UPDATE_ACCEPT_CONTEXT((SOCKET)*newSock, (SOCKET)sock);
    }

    /* IO is done */
    GetAcceptExSockaddrs(
        buf,
        amount,
        INET_ADDR_PADDED,
        INET_ADDR_PADDED,
        (LPSOCKADDR *)&(Laddr),
        &llen,
        (LPSOCKADDR *)(raddr),
        (unsigned int *)&rlen);

    return me->md.blocked_io_bytes;
}

PRInt32
_PR_MD_SENDFILE(PRFileDesc *sock, PRSendFileData *sfd,
                PRInt32 flags, PRIntervalTime timeout)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRInt32 tflags;
    int rv, err;

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return -1;
    }

    if (!sock->secret->md.io_model_committed) {
        rv = _md_Associate((HANDLE)sock->secret->md.osfd);
        PR_ASSERT(0 != rv);
        sock->secret->md.io_model_committed = PR_TRUE;
    }
    if (!me->md.xmit_bufs) {
        me->md.xmit_bufs = PR_NEW(TRANSMIT_FILE_BUFFERS);
        if (!me->md.xmit_bufs) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return -1;
        }
    }
    me->md.xmit_bufs->Head       = (void *)sfd->header;
    me->md.xmit_bufs->HeadLength = sfd->hlen;
    me->md.xmit_bufs->Tail       = (void *)sfd->trailer;
    me->md.xmit_bufs->TailLength = sfd->tlen;

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));
    me->md.overlapped.overlapped.Offset = sfd->file_offset;
    if (_native_threads_only) {
        me->md.overlapped.overlapped.hEvent = me->md.thr_event;
    }

    tflags = 0;
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        tflags = TF_DISCONNECT | TF_REUSE_SOCKET;
    }

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);
    me->io_fd = sock->secret->md.osfd;

    rv = TransmitFile((SOCKET)sock->secret->md.osfd,
                      (HANDLE)sfd->fd->secret->md.osfd,
                      (DWORD)sfd->file_nbytes,
                      (DWORD)0,
                      (LPOVERLAPPED)&(me->md.overlapped.overlapped),
                      (TRANSMIT_FILE_BUFFERS *)me->md.xmit_bufs,
                      (DWORD)tflags);
    if ( (rv == 0) && ((err = GetLastError()) != ERROR_IO_PENDING) ) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return -1;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_TRANSMITFILE_ERROR(err);
        return -1;
    }

    if (_NT_IO_WAIT(me, timeout) == PR_FAILURE) {
        PR_ASSERT(0);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

    if (me->io_suspended) {
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        } else {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        }
        return -1;
    }

    if (me->md.blocked_io_status == 0) {
        _PR_MD_MAP_TRANSMITFILE_ERROR(me->md.blocked_io_error);
        return -1;
    }

    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        _md_put_recycled_socket(sock->secret->md.osfd, sock->secret->af);
    }

    PR_ASSERT(me->io_pending == PR_FALSE);

    return me->md.blocked_io_bytes;
}

PRInt32
_PR_MD_RECV(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    int bytes;
    int rv, err;

    if (_NT_USE_NB_IO(fd)) {
        if (!fd->secret->md.io_model_committed) {
            rv = _md_MakeNonblock((HANDLE)osfd);
            PR_ASSERT(0 != rv);
            fd->secret->md.io_model_committed = PR_TRUE;
        }
        return _nt_nonblock_recv(fd, buf, amount, flags, timeout);
    }

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return -1;
    }

    if (!fd->secret->md.io_model_committed) {
        rv = _md_Associate((HANDLE)osfd);
        PR_ASSERT(0 != rv);
        fd->secret->md.io_model_committed = PR_TRUE;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));
    if (_native_threads_only) {
        me->md.overlapped.overlapped.hEvent = me->md.thr_event;
    }

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);
    me->io_fd = osfd;

    rv = ReadFile((HANDLE)osfd,
                  buf,
                  amount,
                  &bytes,
                  &(me->md.overlapped.overlapped));
    if ( (rv == 0) && (GetLastError() != ERROR_IO_PENDING) ) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return -1;
        }
        _PR_THREAD_UNLOCK(me);

        if ((err = GetLastError()) == ERROR_HANDLE_EOF) {
            return 0;
        }
        _PR_MD_MAP_READ_ERROR(err);
        return -1;
    }

    if (_native_threads_only && rv) {
        _native_thread_io_nowait(me, rv, bytes);
    } else if (_NT_IO_WAIT(me, timeout) == PR_FAILURE) {
        PR_ASSERT(0);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

    if (me->io_suspended) {
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        } else {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        }
        return -1;
    }

    if (me->md.blocked_io_status == 0) {
        if (me->md.blocked_io_error == ERROR_HANDLE_EOF) {
            return 0;
        }
        _PR_MD_MAP_READ_ERROR(me->md.blocked_io_error);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE);

    return me->md.blocked_io_bytes;
}

PRInt32
_PR_MD_SEND(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
            PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    int bytes;
    int rv, err;

    if (_NT_USE_NB_IO(fd)) {
        if (!fd->secret->md.io_model_committed) {
            rv = _md_MakeNonblock((HANDLE)osfd);
            PR_ASSERT(0 != rv);
            fd->secret->md.io_model_committed = PR_TRUE;
        }
        return _nt_nonblock_send(fd, (char *)buf, amount, timeout);
    }

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return -1;
    }

    if (!fd->secret->md.io_model_committed) {
        rv = _md_Associate((HANDLE)osfd);
        PR_ASSERT(0 != rv);
        fd->secret->md.io_model_committed = PR_TRUE;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));
    if (_native_threads_only) {
        me->md.overlapped.overlapped.hEvent = me->md.thr_event;
    }

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);
    me->io_fd = osfd;

    rv = WriteFile((HANDLE)osfd,
                   buf,
                   amount,
                   &bytes,
                   &(me->md.overlapped.overlapped));
    if ( (rv == 0) && ((err = GetLastError()) != ERROR_IO_PENDING) ) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return -1;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_WRITE_ERROR(err);
        return -1;
    }

    if (_native_threads_only && rv) {
        _native_thread_io_nowait(me, rv, bytes);
    } else if (_NT_IO_WAIT(me, timeout) == PR_FAILURE) {
        PR_ASSERT(0);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

    if (me->io_suspended) {
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        } else {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        }
        return -1;
    }

    if (me->md.blocked_io_status == 0) {
        _PR_MD_MAP_WRITE_ERROR(me->md.blocked_io_error);
        return -1;
    }

    PR_ASSERT(me->io_pending == PR_FALSE);

    return me->md.blocked_io_bytes;
}

PRInt32
_PR_MD_SENDTO(PRFileDesc *fd, const void *buf, PRInt32 amount, PRIntn flags,
              const PRNetAddr *addr, PRUint32 addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv;

    if (!fd->secret->md.io_model_committed) {
        rv = _md_MakeNonblock((HANDLE)osfd);
        PR_ASSERT(0 != rv);
        fd->secret->md.io_model_committed = PR_TRUE;
    }
    if (_NT_USE_NB_IO(fd)) {
        return _nt_nonblock_sendto(fd, buf, amount, (struct sockaddr *)addr, addrlen, timeout);
    }
    else {
        return pt_SendTo(osfd, buf, amount, flags, addr, addrlen, timeout);
    }
}

PRInt32
_PR_MD_RECVFROM(PRFileDesc *fd, void *buf, PRInt32 amount, PRIntn flags,
                PRNetAddr *addr, PRUint32 *addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv;

    if (!fd->secret->md.io_model_committed) {
        rv = _md_MakeNonblock((HANDLE)osfd);
        PR_ASSERT(0 != rv);
        fd->secret->md.io_model_committed = PR_TRUE;
    }
    if (_NT_USE_NB_IO(fd)) {
        return _nt_nonblock_recvfrom(fd, buf, amount, (struct sockaddr *)addr, addrlen, timeout);
    }
    else {
        return pt_RecvFrom(osfd, buf, amount, flags, addr, addrlen, timeout);
    }
}

/* XXXMB - for now this is a sockets call only */
PRInt32
_PR_MD_WRITEV(PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    int index;
    int sent = 0;
    int rv;

    if (_NT_USE_NB_IO(fd)) {
        if (!fd->secret->md.io_model_committed) {
            rv = _md_MakeNonblock((HANDLE)osfd);
            PR_ASSERT(0 != rv);
            fd->secret->md.io_model_committed = PR_TRUE;
        }
        return _nt_nonblock_writev(fd, iov, iov_size, timeout);
    }

    for (index=0; index<iov_size; index++) {
        rv = _PR_MD_SEND(fd, iov[index].iov_base, iov[index].iov_len, 0,
                         timeout);
        if (rv > 0) {
            sent += rv;
        }
        if ( rv != iov[index].iov_len ) {
            if (sent <= 0) {
                return -1;
            }
            return -1;
        }
    }

    return sent;
}

PRInt32
_PR_MD_LISTEN(PRFileDesc *fd, PRIntn backlog)
{
    PRInt32 rv;

    rv = listen(fd->secret->md.osfd, backlog);
    if (rv < 0) {
        _PR_MD_MAP_LISTEN_ERROR(WSAGetLastError());
    }
    return(rv);
}

PRInt32
_PR_MD_SHUTDOWN(PRFileDesc *fd, PRIntn how)
{
    PRInt32 rv;

    rv = shutdown(fd->secret->md.osfd, how);
    if (rv < 0) {
        _PR_MD_MAP_SHUTDOWN_ERROR(WSAGetLastError());
    }
    return(rv);
}

PRStatus
_PR_MD_GETSOCKNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    rv = getsockname((SOCKET)fd->secret->md.osfd, (struct sockaddr *)addr, len);
    if (rv==0) {
        return PR_SUCCESS;
    }
    else {
        _PR_MD_MAP_GETSOCKNAME_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_GETPEERNAME(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *len)
{
    PRInt32 rv;

    /*
     * NT has a bug that, when invoked on a socket accepted by
     * AcceptEx(), getpeername() returns an all-zero peer address.
     * To work around this bug, we store the peer's address (returned
     * by AcceptEx()) with the socket fd and use the cached peer
     * address if the socket is an accepted socket.
     */

    if (fd->secret->md.accepted_socket) {
        INT seconds;
        INT bytes = sizeof(seconds);

        /*
         * Determine if the socket is connected.
         */

        rv = getsockopt(fd->secret->md.osfd,
                        SOL_SOCKET,
                        SO_CONNECT_TIME,
                        (char *) &seconds,
                        (PINT) &bytes);
        if (rv == NO_ERROR) {
            if (seconds == 0xffffffff) {
                PR_SetError(PR_NOT_CONNECTED_ERROR, 0);
                return PR_FAILURE;
            }
            *len = PR_NETADDR_SIZE(&fd->secret->md.peer_addr);
            memcpy(addr, &fd->secret->md.peer_addr, *len);
            return PR_SUCCESS;
        } else {
            _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
            return PR_FAILURE;
        }
    } else {
        rv = getpeername((SOCKET)fd->secret->md.osfd,
                         (struct sockaddr *) addr, len);
        if (rv == 0) {
            return PR_SUCCESS;
        } else {
            _PR_MD_MAP_GETPEERNAME_ERROR(WSAGetLastError());
            return PR_FAILURE;
        }
    }
}

PRStatus
_PR_MD_GETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, char* optval, PRInt32* optlen)
{
    PRInt32 rv;

    rv = getsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0) {
        return PR_SUCCESS;
    }
    else {
        _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

PRStatus
_PR_MD_SETSOCKOPT(PRFileDesc *fd, PRInt32 level, PRInt32 optname, const char* optval, PRInt32 optlen)
{
    PRInt32 rv;

    rv = setsockopt((SOCKET)fd->secret->md.osfd, level, optname, optval, optlen);
    if (rv==0) {
        return PR_SUCCESS;
    }
    else {
        _PR_MD_MAP_SETSOCKOPT_ERROR(WSAGetLastError());
        return PR_FAILURE;
    }
}

/* --- FILE IO ----------------------------------------------------------- */

PROsfd
_PR_MD_OPEN(const char *name, PRIntn osflags, PRIntn mode)
{
    HANDLE file;
    PRInt32 access = 0;
    PRInt32 flags = 0;
    PRInt32 flag6 = 0;

    if (osflags & PR_SYNC) {
        flag6 = FILE_FLAG_WRITE_THROUGH;
    }

    if (osflags & PR_RDONLY || osflags & PR_RDWR) {
        access |= GENERIC_READ;
    }
    if (osflags & PR_WRONLY || osflags & PR_RDWR) {
        access |= GENERIC_WRITE;
    }

    if ( osflags & PR_CREATE_FILE && osflags & PR_EXCL ) {
        flags = CREATE_NEW;
    }
    else if (osflags & PR_CREATE_FILE) {
        flags = (0 != (osflags & PR_TRUNCATE)) ? CREATE_ALWAYS : OPEN_ALWAYS;
    }
    else if (osflags & PR_TRUNCATE) {
        flags = TRUNCATE_EXISTING;
    }
    else {
        flags = OPEN_EXISTING;
    }


    flag6 |= FILE_FLAG_OVERLAPPED;

    file = CreateFile(name,
                      access,
                      FILE_SHARE_READ|FILE_SHARE_WRITE,
                      NULL,
                      flags,
                      flag6,
                      NULL);
    if (file == INVALID_HANDLE_VALUE) {
        _PR_MD_MAP_OPEN_ERROR(GetLastError());
        return -1;
    }

    if (osflags & PR_APPEND) {
        if ( SetFilePointer(file, 0, 0, FILE_END) == 0xFFFFFFFF ) {
            _PR_MD_MAP_LSEEK_ERROR(GetLastError());
            CloseHandle(file);
            return -1;
        }
    }

    return (PROsfd)file;
}

PROsfd
_PR_MD_OPEN_FILE(const char *name, PRIntn osflags, PRIntn mode)
{
    HANDLE file;
    PRInt32 access = 0;
    PRInt32 flags = 0;
    PRInt32 flag6 = 0;
    SECURITY_ATTRIBUTES sa;
    LPSECURITY_ATTRIBUTES lpSA = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pACL = NULL;

    if (osflags & PR_SYNC) {
        flag6 = FILE_FLAG_WRITE_THROUGH;
    }

    if (osflags & PR_RDONLY || osflags & PR_RDWR) {
        access |= GENERIC_READ;
    }
    if (osflags & PR_WRONLY || osflags & PR_RDWR) {
        access |= GENERIC_WRITE;
    }

    if ( osflags & PR_CREATE_FILE && osflags & PR_EXCL ) {
        flags = CREATE_NEW;
    }
    else if (osflags & PR_CREATE_FILE) {
        flags = (0 != (osflags & PR_TRUNCATE)) ? CREATE_ALWAYS : OPEN_ALWAYS;
    }
    else if (osflags & PR_TRUNCATE) {
        flags = TRUNCATE_EXISTING;
    }
    else {
        flags = OPEN_EXISTING;
    }


    flag6 |= FILE_FLAG_OVERLAPPED;

    if (osflags & PR_CREATE_FILE) {
        if (_PR_NT_MakeSecurityDescriptorACL(mode, fileAccessTable,
                                             &pSD, &pACL) == PR_SUCCESS) {
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = pSD;
            sa.bInheritHandle = FALSE;
            lpSA = &sa;
        }
    }
    file = CreateFile(name,
                      access,
                      FILE_SHARE_READ|FILE_SHARE_WRITE,
                      lpSA,
                      flags,
                      flag6,
                      NULL);
    if (lpSA != NULL) {
        _PR_NT_FreeSecurityDescriptorACL(pSD, pACL);
    }
    if (file == INVALID_HANDLE_VALUE) {
        _PR_MD_MAP_OPEN_ERROR(GetLastError());
        return -1;
    }

    if (osflags & PR_APPEND) {
        if ( SetFilePointer(file, 0, 0, FILE_END) == 0xFFFFFFFF ) {
            _PR_MD_MAP_LSEEK_ERROR(GetLastError());
            CloseHandle(file);
            return -1;
        }
    }

    return (PROsfd)file;
}

PRInt32
_PR_MD_READ(PRFileDesc *fd, void *buf, PRInt32 len)
{
    PROsfd f = fd->secret->md.osfd;
    PRUint32 bytes;
    int rv, err;
    LONG hiOffset = 0;
    LONG loOffset;
    LARGE_INTEGER offset; /* use for a normalized add of len to offset */

    if (!fd->secret->md.sync_file_io) {
        PRThread *me = _PR_MD_CURRENT_THREAD();

        if (me->io_suspended) {
            PR_SetError(PR_INVALID_STATE_ERROR, 0);
            return -1;
        }

        memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));

        me->md.overlapped.overlapped.Offset = SetFilePointer((HANDLE)f, 0, &me->md.overlapped.overlapped.OffsetHigh, FILE_CURRENT);
        PR_ASSERT((me->md.overlapped.overlapped.Offset != 0xffffffff) || (GetLastError() == NO_ERROR));

        if (fd->secret->inheritable == _PR_TRI_TRUE) {
            rv = ReadFile((HANDLE)f,
                          (LPVOID)buf,
                          len,
                          &bytes,
                          &me->md.overlapped.overlapped);
            if (rv != 0) {
                loOffset = SetFilePointer((HANDLE)f, bytes, &hiOffset, FILE_CURRENT);
                PR_ASSERT((loOffset != 0xffffffff) || (GetLastError() == NO_ERROR));
                return bytes;
            }
            err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                rv = GetOverlappedResult((HANDLE)f,
                                         &me->md.overlapped.overlapped, &bytes, TRUE);
                if (rv != 0) {
                    loOffset = SetFilePointer((HANDLE)f, bytes, &hiOffset, FILE_CURRENT);
                    PR_ASSERT((loOffset != 0xffffffff) || (GetLastError() == NO_ERROR));
                    return bytes;
                }
                err = GetLastError();
            }
            if (err == ERROR_HANDLE_EOF) {
                return 0;
            } else {
                _PR_MD_MAP_READ_ERROR(err);
                return -1;
            }
        } else {
            if (!fd->secret->md.io_model_committed) {
                rv = _md_Associate((HANDLE)f);
                PR_ASSERT(rv != 0);
                fd->secret->md.io_model_committed = PR_TRUE;
            }

            if (_native_threads_only) {
                me->md.overlapped.overlapped.hEvent = me->md.thr_event;
            }

            _PR_THREAD_LOCK(me);
            if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                _PR_THREAD_UNLOCK(me);
                return -1;
            }
            me->io_pending = PR_TRUE;
            me->state = _PR_IO_WAIT;
            _PR_THREAD_UNLOCK(me);
            me->io_fd = f;

            rv = ReadFile((HANDLE)f,
                          (LPVOID)buf,
                          len,
                          &bytes,
                          &me->md.overlapped.overlapped);
            if ( (rv == 0) && ((err = GetLastError()) != ERROR_IO_PENDING) ) {
                _PR_THREAD_LOCK(me);
                me->io_pending = PR_FALSE;
                me->state = _PR_RUNNING;
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    _PR_THREAD_UNLOCK(me);
                    return -1;
                }
                _PR_THREAD_UNLOCK(me);

                if (err == ERROR_HANDLE_EOF) {
                    return 0;
                }
                _PR_MD_MAP_READ_ERROR(err);
                return -1;
            }

            if (_native_threads_only && rv) {
                _native_thread_io_nowait(me, rv, bytes);
            } else if (_NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
                PR_ASSERT(0);
                return -1;
            }

            PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

            if (me->io_suspended) {
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                } else {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                return -1;
            }

            if (me->md.blocked_io_status == 0) {
                if (me->md.blocked_io_error == ERROR_HANDLE_EOF) {
                    return 0;
                }
                _PR_MD_MAP_READ_ERROR(me->md.blocked_io_error);
                return -1;
            }

            /* Apply the workaround from bug 70765 (see _PR_MD_WRITE)
             * to the reading code, too. */

            offset.LowPart = me->md.overlapped.overlapped.Offset;
            offset.HighPart = me->md.overlapped.overlapped.OffsetHigh;
            offset.QuadPart += me->md.blocked_io_bytes;

            SetFilePointer((HANDLE)f, offset.LowPart, &offset.HighPart, FILE_BEGIN);

            PR_ASSERT(me->io_pending == PR_FALSE);

            return me->md.blocked_io_bytes;
        }
    } else {

        rv = ReadFile((HANDLE)f,
                      (LPVOID)buf,
                      len,
                      &bytes,
                      NULL);
        if (rv == 0) {
            err = GetLastError();
            /* ERROR_HANDLE_EOF can only be returned by async io */
            PR_ASSERT(err != ERROR_HANDLE_EOF);
            if (err == ERROR_BROKEN_PIPE) {
                /* The write end of the pipe has been closed. */
                return 0;
            }
            _PR_MD_MAP_READ_ERROR(err);
            return -1;
        }
        return bytes;
    }
}

PRInt32
_PR_MD_WRITE(PRFileDesc *fd, const void *buf, PRInt32 len)
{
    PROsfd f = fd->secret->md.osfd;
    PRInt32 bytes;
    int rv, err;
    LONG hiOffset = 0;
    LONG loOffset;
    LARGE_INTEGER offset; /* use for the calculation of the new offset */

    if (!fd->secret->md.sync_file_io) {
        PRThread *me = _PR_MD_CURRENT_THREAD();

        if (me->io_suspended) {
            PR_SetError(PR_INVALID_STATE_ERROR, 0);
            return -1;
        }

        memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));

        me->md.overlapped.overlapped.Offset = SetFilePointer((HANDLE)f, 0, &me->md.overlapped.overlapped.OffsetHigh, FILE_CURRENT);
        PR_ASSERT((me->md.overlapped.overlapped.Offset != 0xffffffff) || (GetLastError() == NO_ERROR));

        if (fd->secret->inheritable == _PR_TRI_TRUE) {
            rv = WriteFile((HANDLE)f,
                           (LPVOID)buf,
                           len,
                           &bytes,
                           &me->md.overlapped.overlapped);
            if (rv != 0) {
                loOffset = SetFilePointer((HANDLE)f, bytes, &hiOffset, FILE_CURRENT);
                PR_ASSERT((loOffset != 0xffffffff) || (GetLastError() == NO_ERROR));
                return bytes;
            }
            err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                rv = GetOverlappedResult((HANDLE)f,
                                         &me->md.overlapped.overlapped, &bytes, TRUE);
                if (rv != 0) {
                    loOffset = SetFilePointer((HANDLE)f, bytes, &hiOffset, FILE_CURRENT);
                    PR_ASSERT((loOffset != 0xffffffff) || (GetLastError() == NO_ERROR));
                    return bytes;
                }
                err = GetLastError();
            }
            _PR_MD_MAP_READ_ERROR(err);
            return -1;
        } else {
            if (!fd->secret->md.io_model_committed) {
                rv = _md_Associate((HANDLE)f);
                PR_ASSERT(rv != 0);
                fd->secret->md.io_model_committed = PR_TRUE;
            }
            if (_native_threads_only) {
                me->md.overlapped.overlapped.hEvent = me->md.thr_event;
            }

            _PR_THREAD_LOCK(me);
            if (_PR_PENDING_INTERRUPT(me)) {
                me->flags &= ~_PR_INTERRUPT;
                PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                _PR_THREAD_UNLOCK(me);
                return -1;
            }
            me->io_pending = PR_TRUE;
            me->state = _PR_IO_WAIT;
            _PR_THREAD_UNLOCK(me);
            me->io_fd = f;

            rv = WriteFile((HANDLE)f,
                           buf,
                           len,
                           &bytes,
                           &(me->md.overlapped.overlapped));
            if ( (rv == 0) && ((err = GetLastError()) != ERROR_IO_PENDING) ) {
                _PR_THREAD_LOCK(me);
                me->io_pending = PR_FALSE;
                me->state = _PR_RUNNING;
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    _PR_THREAD_UNLOCK(me);
                    return -1;
                }
                _PR_THREAD_UNLOCK(me);

                _PR_MD_MAP_WRITE_ERROR(err);
                return -1;
            }

            if (_native_threads_only && rv) {
                _native_thread_io_nowait(me, rv, bytes);
            } else if (_NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
                PR_ASSERT(0);
                return -1;
            }

            PR_ASSERT(me->io_pending == PR_FALSE || me->io_suspended == PR_TRUE);

            if (me->io_suspended) {
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                } else {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                }
                return -1;
            }

            if (me->md.blocked_io_status == 0) {
                _PR_MD_MAP_WRITE_ERROR(me->md.blocked_io_error);
                return -1;
            }

            /*
             * Moving the file pointer by a relative offset (FILE_CURRENT)
             * does not work with a file on a network drive exported by a
             * Win2K system.  We still don't know why.  A workaround is to
             * move the file pointer by an absolute offset (FILE_BEGIN).
             * (Bugzilla bug 70765)
             */
            offset.LowPart = me->md.overlapped.overlapped.Offset;
            offset.HighPart = me->md.overlapped.overlapped.OffsetHigh;
            offset.QuadPart += me->md.blocked_io_bytes;

            SetFilePointer((HANDLE)f, offset.LowPart, &offset.HighPart, FILE_BEGIN);

            PR_ASSERT(me->io_pending == PR_FALSE);

            return me->md.blocked_io_bytes;
        }
    } else {
        rv = WriteFile((HANDLE)f,
                       buf,
                       len,
                       &bytes,
                       NULL);
        if (rv == 0) {
            _PR_MD_MAP_WRITE_ERROR(GetLastError());
            return -1;
        }
        return bytes;
    }
}

PRInt32
_PR_MD_SOCKETAVAILABLE(PRFileDesc *fd)
{
    PRInt32 result;

    if (ioctlsocket(fd->secret->md.osfd, FIONREAD, &result) < 0) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, WSAGetLastError());
        return -1;
    }
    return result;
}

PRInt32
_PR_MD_PIPEAVAILABLE(PRFileDesc *fd)
{
    if (NULL == fd) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    }
    else {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    }
    return -1;
}

PROffset32
_PR_MD_LSEEK(PRFileDesc *fd, PROffset32 offset, PRSeekWhence whence)
{
    DWORD moveMethod;
    PROffset32 rv;

    switch (whence) {
        case PR_SEEK_SET:
            moveMethod = FILE_BEGIN;
            break;
        case PR_SEEK_CUR:
            moveMethod = FILE_CURRENT;
            break;
        case PR_SEEK_END:
            moveMethod = FILE_END;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return -1;
    }

    rv = SetFilePointer((HANDLE)fd->secret->md.osfd, offset, NULL, moveMethod);

    /*
     * If the lpDistanceToMoveHigh argument (third argument) is
     * NULL, SetFilePointer returns 0xffffffff on failure.
     */
    if (-1 == rv) {
        _PR_MD_MAP_LSEEK_ERROR(GetLastError());
    }
    return rv;
}

PROffset64
_PR_MD_LSEEK64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence whence)
{
    DWORD moveMethod;
    LARGE_INTEGER li;
    DWORD err;

    switch (whence) {
        case PR_SEEK_SET:
            moveMethod = FILE_BEGIN;
            break;
        case PR_SEEK_CUR:
            moveMethod = FILE_CURRENT;
            break;
        case PR_SEEK_END:
            moveMethod = FILE_END;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return -1;
    }

    li.QuadPart = offset;
    li.LowPart = SetFilePointer((HANDLE)fd->secret->md.osfd,
                                li.LowPart, &li.HighPart, moveMethod);

    if (0xffffffff == li.LowPart && (err = GetLastError()) != NO_ERROR) {
        _PR_MD_MAP_LSEEK_ERROR(err);
        li.QuadPart = -1;
    }
    return li.QuadPart;
}

/*
 * This is documented to succeed on read-only files, but Win32's
 * FlushFileBuffers functions fails with "access denied" in such a
 * case.  So we only signal an error if the error is *not* "access
 * denied".
 */
PRInt32
_PR_MD_FSYNC(PRFileDesc *fd)
{
    /*
     * From the documentation:
     *
     *     On Windows NT, the function FlushFileBuffers fails if hFile
     *     is a handle to console output. That is because console
     *     output is not buffered. The function returns FALSE, and
     *     GetLastError returns ERROR_INVALID_HANDLE.
     *
     * On the other hand, on Win95, it returns without error.  I cannot
     * assume that 0, 1, and 2 are console, because if someone closes
     * System.out and then opens a file, they might get file descriptor
     * 1.  An error on *that* version of 1 should be reported, whereas
     * an error on System.out (which was the original 1) should be
     * ignored.  So I use isatty() to ensure that such an error was
     * because of this, and if it was, I ignore the error.
     */

    BOOL ok = FlushFileBuffers((HANDLE)fd->secret->md.osfd);

    if (!ok) {
        DWORD err = GetLastError();

        if (err != ERROR_ACCESS_DENIED) {   /* from winerror.h */
            _PR_MD_MAP_FSYNC_ERROR(err);
            return -1;
        }
    }
    return 0;
}

PRInt32
_PR_MD_CLOSE(PROsfd osfd, PRBool socket)
{
    PRInt32 rv;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (socket)  {
        rv = closesocket((SOCKET)osfd);
        if (rv < 0) {
            _PR_MD_MAP_CLOSE_ERROR(WSAGetLastError());
        }
    } else {
        rv = CloseHandle((HANDLE)osfd)?0:-1;
        if (rv < 0) {
            _PR_MD_MAP_CLOSE_ERROR(GetLastError());
        }
    }

    if (rv == 0 && me->io_suspended) {
        if (me->io_fd == osfd) {
            PRBool fWait;

            _PR_THREAD_LOCK(me);
            me->state = _PR_IO_WAIT;
            /* The IO could have completed on another thread just after
             * calling closesocket while the io_suspended flag was true.
             * So we now grab the lock to do a safe check on io_pending to
             * see if we need to wait or not.
             */
            fWait = me->io_pending;
            me->io_suspended = PR_FALSE;
            me->md.interrupt_disabled = PR_TRUE;
            _PR_THREAD_UNLOCK(me);

            if (fWait) {
                _NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT);
            }
            PR_ASSERT(me->io_suspended ==  PR_FALSE);
            PR_ASSERT(me->io_pending ==  PR_FALSE);
            /*
             * I/O operation is no longer pending; the thread can now
             * run on any cpu
             */
            _PR_THREAD_LOCK(me);
            me->md.interrupt_disabled = PR_FALSE;
            me->md.thr_bound_cpu = NULL;
            me->io_suspended = PR_FALSE;
            me->io_pending = PR_FALSE;
            me->state = _PR_RUNNING;
            _PR_THREAD_UNLOCK(me);
        }
    }
    return rv;
}

PRStatus
_PR_MD_SET_FD_INHERITABLE(PRFileDesc *fd, PRBool inheritable)
{
    BOOL rv;

    if (fd->secret->md.io_model_committed) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    rv = SetHandleInformation(
             (HANDLE)fd->secret->md.osfd,
             HANDLE_FLAG_INHERIT,
             inheritable ? HANDLE_FLAG_INHERIT : 0);
    if (0 == rv) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

void
_PR_MD_INIT_FD_INHERITABLE(PRFileDesc *fd, PRBool imported)
{
    if (imported) {
        fd->secret->inheritable = _PR_TRI_UNKNOWN;
    } else {
        fd->secret->inheritable = _PR_TRI_FALSE;
    }
}

void
_PR_MD_QUERY_FD_INHERITABLE(PRFileDesc *fd)
{
    DWORD flags;

    PR_ASSERT(_PR_TRI_UNKNOWN == fd->secret->inheritable);
    if (fd->secret->md.io_model_committed) {
        return;
    }
    if (GetHandleInformation((HANDLE)fd->secret->md.osfd, &flags)) {
        if (flags & HANDLE_FLAG_INHERIT) {
            fd->secret->inheritable = _PR_TRI_TRUE;
        } else {
            fd->secret->inheritable = _PR_TRI_FALSE;
        }
    }
}


/* --- DIR IO ------------------------------------------------------------ */
#define GetFileFromDIR(d)       (d)->d_entry.cFileName
#define FileIsHidden(d)       ((d)->d_entry.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)

void FlipSlashes(char *cp, int len)
{
    while (--len >= 0) {
        if (cp[0] == '/') {
            cp[0] = PR_DIRECTORY_SEPARATOR;
        }
        cp = _mbsinc(cp);
    }
} /* end FlipSlashes() */

/*
**
** Local implementations of standard Unix RTL functions which are not provided
** by the VC RTL.
**
*/

PRInt32
_PR_MD_CLOSE_DIR(_MDDir *d)
{
    if ( d ) {
        if (FindClose( d->d_hdl )) {
            d->magic = (PRUint32)-1;
            return 0;
        } else {
            _PR_MD_MAP_CLOSEDIR_ERROR(GetLastError());
            return -1;
        }
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
}


PRStatus
_PR_MD_OPEN_DIR(_MDDir *d, const char *name)
{
    char filename[ MAX_PATH ];
    int len;

    len = strlen(name);
    /* Need 5 bytes for \*.* and the trailing null byte. */
    if (len + 5 > MAX_PATH) {
        PR_SetError(PR_NAME_TOO_LONG_ERROR, 0);
        return PR_FAILURE;
    }
    strcpy(filename, name);

    /*
     * If 'name' ends in a slash or backslash, do not append
     * another backslash.
     */
    if (IsPrevCharSlash(filename, filename + len)) {
        len--;
    }
    strcpy(&filename[len], "\\*.*");
    FlipSlashes( filename, strlen(filename) );

    d->d_hdl = FindFirstFile( filename, &(d->d_entry) );
    if ( d->d_hdl == INVALID_HANDLE_VALUE ) {
        _PR_MD_MAP_OPENDIR_ERROR(GetLastError());
        return PR_FAILURE;
    }
    d->firstEntry = PR_TRUE;
    d->magic = _MD_MAGIC_DIR;
    return PR_SUCCESS;
}

char *
_PR_MD_READ_DIR(_MDDir *d, PRIntn flags)
{
    PRInt32 err;
    BOOL rv;
    char *fileName;

    if ( d ) {
        while (1) {
            if (d->firstEntry) {
                d->firstEntry = PR_FALSE;
                rv = 1;
            } else {
                rv = FindNextFile(d->d_hdl, &(d->d_entry));
            }
            if (rv == 0) {
                break;
            }
            fileName = GetFileFromDIR(d);
            if ( (flags & PR_SKIP_DOT) &&
                 (fileName[0] == '.') && (fileName[1] == '\0')) {
                continue;
            }
            if ( (flags & PR_SKIP_DOT_DOT) &&
                 (fileName[0] == '.') && (fileName[1] == '.') &&
                 (fileName[2] == '\0')) {
                continue;
            }
            if ( (flags & PR_SKIP_HIDDEN) && FileIsHidden(d)) {
                continue;
            }
            return fileName;
        }
        err = GetLastError();
        PR_ASSERT(NO_ERROR != err);
        _PR_MD_MAP_READDIR_ERROR(err);
        return NULL;
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return NULL;
}

PRInt32
_PR_MD_DELETE(const char *name)
{
    if (DeleteFile(name)) {
        return 0;
    } else {
        _PR_MD_MAP_DELETE_ERROR(GetLastError());
        return -1;
    }
}

void
_PR_FileTimeToPRTime(const FILETIME *filetime, PRTime *prtm)
{
    PR_ASSERT(sizeof(FILETIME) == sizeof(PRTime));
    CopyMemory(prtm, filetime, sizeof(PRTime));
#ifdef __GNUC__
    *prtm = (*prtm - _pr_filetime_offset) / 10LL;
#else
    *prtm = (*prtm - _pr_filetime_offset) / 10i64;
#endif

#ifdef DEBUG
    /* Doublecheck our calculation. */
    {
        SYSTEMTIME systime;
        PRExplodedTime etm;
        PRTime cmp; /* for comparison */
        BOOL rv;

        rv = FileTimeToSystemTime(filetime, &systime);
        PR_ASSERT(0 != rv);

        /*
         * PR_ImplodeTime ignores wday and yday.
         */
        etm.tm_usec = systime.wMilliseconds * PR_USEC_PER_MSEC;
        etm.tm_sec = systime.wSecond;
        etm.tm_min = systime.wMinute;
        etm.tm_hour = systime.wHour;
        etm.tm_mday = systime.wDay;
        etm.tm_month = systime.wMonth - 1;
        etm.tm_year = systime.wYear;
        /*
         * It is not well-documented what time zone the FILETIME's
         * are in.  WIN32_FIND_DATA is documented to be in UTC (GMT).
         * But BY_HANDLE_FILE_INFORMATION is unclear about this.
         * By our best judgement, we assume that FILETIME is in UTC.
         */
        etm.tm_params.tp_gmt_offset = 0;
        etm.tm_params.tp_dst_offset = 0;
        cmp = PR_ImplodeTime(&etm);

        /*
         * SYSTEMTIME is in milliseconds precision, so we convert PRTime's
         * microseconds to milliseconds before doing the comparison.
         */
        PR_ASSERT((cmp / PR_USEC_PER_MSEC) == (*prtm / PR_USEC_PER_MSEC));
    }
#endif /* DEBUG */
}

PRInt32
_PR_MD_STAT(const char *fn, struct stat *info)
{
    PRInt32 rv;

    rv = _stat(fn, (struct _stat *)info);
    if (-1 == rv) {
        /*
         * Check for MSVC runtime library _stat() bug.
         * (It's really a bug in FindFirstFile().)
         * If a pathname ends in a backslash or slash,
         * e.g., c:\temp\ or c:/temp/, _stat() will fail.
         * Note: a pathname ending in a slash (e.g., c:/temp/)
         * can be handled by _stat() on NT but not on Win95.
         *
         * We remove the backslash or slash at the end and
         * try again.
         */

        int len = strlen(fn);
        if (len > 0 && len <= _MAX_PATH
            && IsPrevCharSlash(fn, fn + len)) {
            char newfn[_MAX_PATH + 1];

            strcpy(newfn, fn);
            newfn[len - 1] = '\0';
            rv = _stat(newfn, (struct _stat *)info);
        }
    }

    if (-1 == rv) {
        _PR_MD_MAP_STAT_ERROR(errno);
    }
    return rv;
}

#define _PR_IS_SLASH(ch) ((ch) == '/' || (ch) == '\\')

static PRBool
IsPrevCharSlash(const char *str, const char *current)
{
    const char *prev;

    if (str >= current) {
        return PR_FALSE;
    }
    prev = _mbsdec(str, current);
    return (prev == current - 1) && _PR_IS_SLASH(*prev);
}

/*
 * IsRootDirectory --
 *
 * Return PR_TRUE if the pathname 'fn' is a valid root directory,
 * else return PR_FALSE.  The char buffer pointed to by 'fn' must
 * be writable.  During the execution of this function, the contents
 * of the buffer pointed to by 'fn' may be modified, but on return
 * the original contents will be restored.  'buflen' is the size of
 * the buffer pointed to by 'fn'.
 *
 * Root directories come in three formats:
 * 1. / or \, meaning the root directory of the current drive.
 * 2. C:/ or C:\, where C is a drive letter.
 * 3. \\<server name>\<share point name>\ or
 *    \\<server name>\<share point name>, meaning the root directory
 *    of a UNC (Universal Naming Convention) name.
 */

static PRBool
IsRootDirectory(char *fn, size_t buflen)
{
    char *p;
    PRBool slashAdded = PR_FALSE;
    PRBool rv = PR_FALSE;

    if (_PR_IS_SLASH(fn[0]) && fn[1] == '\0') {
        return PR_TRUE;
    }

    if (isalpha(fn[0]) && fn[1] == ':' && _PR_IS_SLASH(fn[2])
        && fn[3] == '\0') {
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        return rv;
    }

    /* The UNC root directory */

    if (_PR_IS_SLASH(fn[0]) && _PR_IS_SLASH(fn[1])) {
        /* The 'server' part should have at least one character. */
        p = &fn[2];
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the next slash */
        do {
            p = _mbsinc(p);
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (*p == '\0') {
            return PR_FALSE;
        }

        /* The 'share' part should have at least one character. */
        p++;
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the final slash */
        do {
            p = _mbsinc(p);
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (_PR_IS_SLASH(*p) && p[1] != '\0') {
            return PR_FALSE;
        }
        if (*p == '\0') {
            /*
             * GetDriveType() doesn't work correctly if the
             * path is of the form \\server\share, so we add
             * a final slash temporarily.
             */
            if ((p + 1) < (fn + buflen)) {
                *p++ = '\\';
                *p = '\0';
                slashAdded = PR_TRUE;
            } else {
                return PR_FALSE; /* name too long */
            }
        }
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        /* restore the 'fn' buffer */
        if (slashAdded) {
            *--p = '\0';
        }
    }
    return rv;
}

PRInt32
_PR_MD_GETFILEINFO64(const char *fn, PRFileInfo64 *info)
{
    WIN32_FILE_ATTRIBUTE_DATA findFileData;

    if (NULL == fn || '\0' == *fn) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    if (!GetFileAttributesEx(fn, GetFileExInfoStandard, &findFileData)) {
        _PR_MD_MAP_OPENDIR_ERROR(GetLastError());
        return -1;
    }

    if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    } else {
        info->type = PR_FILE_FILE;
    }

    info->size = findFileData.nFileSizeHigh;
    info->size = (info->size << 32) + findFileData.nFileSizeLow;

    _PR_FileTimeToPRTime(&findFileData.ftLastWriteTime, &info->modifyTime);

    if (0 == findFileData.ftCreationTime.dwLowDateTime &&
        0 == findFileData.ftCreationTime.dwHighDateTime) {
        info->creationTime = info->modifyTime;
    } else {
        _PR_FileTimeToPRTime(&findFileData.ftCreationTime,
                             &info->creationTime);
    }

    return 0;
}

PRInt32
_PR_MD_GETFILEINFO(const char *fn, PRFileInfo *info)
{
    PRFileInfo64 info64;
    PRInt32 rv = _PR_MD_GETFILEINFO64(fn, &info64);
    if (0 == rv)
    {
        info->type = info64.type;
        info->size = (PRUint32) info64.size;
        info->modifyTime = info64.modifyTime;
        info->creationTime = info64.creationTime;
    }
    return rv;
}

PRInt32
_PR_MD_GETOPENFILEINFO64(const PRFileDesc *fd, PRFileInfo64 *info)
{
    int rv;

    BY_HANDLE_FILE_INFORMATION hinfo;

    rv = GetFileInformationByHandle((HANDLE)fd->secret->md.osfd, &hinfo);
    if (rv == FALSE) {
        _PR_MD_MAP_FSTAT_ERROR(GetLastError());
        return -1;
    }

    if (hinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    }
    else {
        info->type = PR_FILE_FILE;
    }

    info->size = hinfo.nFileSizeHigh;
    info->size = (info->size << 32) + hinfo.nFileSizeLow;

    _PR_FileTimeToPRTime(&hinfo.ftLastWriteTime, &(info->modifyTime) );
    _PR_FileTimeToPRTime(&hinfo.ftCreationTime, &(info->creationTime) );

    return 0;
}

PRInt32
_PR_MD_GETOPENFILEINFO(const PRFileDesc *fd, PRFileInfo *info)
{
    int rv;

    BY_HANDLE_FILE_INFORMATION hinfo;

    rv = GetFileInformationByHandle((HANDLE)fd->secret->md.osfd, &hinfo);
    if (rv == FALSE) {
        _PR_MD_MAP_FSTAT_ERROR(GetLastError());
        return -1;
    }

    if (hinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    }
    else {
        info->type = PR_FILE_FILE;
    }

    info->size = hinfo.nFileSizeLow;

    _PR_FileTimeToPRTime(&hinfo.ftLastWriteTime, &(info->modifyTime) );
    _PR_FileTimeToPRTime(&hinfo.ftCreationTime, &(info->creationTime) );

    return 0;
}

PRInt32
_PR_MD_RENAME(const char *from, const char *to)
{
    /* Does this work with dot-relative pathnames? */
    if (MoveFile(from, to)) {
        return 0;
    } else {
        _PR_MD_MAP_RENAME_ERROR(GetLastError());
        return -1;
    }
}

PRInt32
_PR_MD_ACCESS(const char *name, PRAccessHow how)
{
    PRInt32 rv;

    switch (how) {
        case PR_ACCESS_WRITE_OK:
            rv = _access(name, 02);
            break;
        case PR_ACCESS_READ_OK:
            rv = _access(name, 04);
            break;
        case PR_ACCESS_EXISTS:
            rv = _access(name, 00);
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return -1;
    }
    if (rv < 0) {
        _PR_MD_MAP_ACCESS_ERROR(errno);
    }
    return rv;
}

PRInt32
_PR_MD_MKDIR(const char *name, PRIntn mode)
{
    /* XXXMB - how to translate the "mode"??? */
    if (CreateDirectory(name, NULL)) {
        return 0;
    } else {
        _PR_MD_MAP_MKDIR_ERROR(GetLastError());
        return -1;
    }
}

PRInt32
_PR_MD_MAKE_DIR(const char *name, PRIntn mode)
{
    BOOL rv;
    SECURITY_ATTRIBUTES sa;
    LPSECURITY_ATTRIBUTES lpSA = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pACL = NULL;

    if (_PR_NT_MakeSecurityDescriptorACL(mode, dirAccessTable,
                                         &pSD, &pACL) == PR_SUCCESS) {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = pSD;
        sa.bInheritHandle = FALSE;
        lpSA = &sa;
    }
    rv = CreateDirectory(name, lpSA);
    if (lpSA != NULL) {
        _PR_NT_FreeSecurityDescriptorACL(pSD, pACL);
    }
    if (rv) {
        return 0;
    } else {
        _PR_MD_MAP_MKDIR_ERROR(GetLastError());
        return -1;
    }
}

PRInt32
_PR_MD_RMDIR(const char *name)
{
    if (RemoveDirectory(name)) {
        return 0;
    } else {
        _PR_MD_MAP_RMDIR_ERROR(GetLastError());
        return -1;
    }
}

PRStatus
_PR_MD_LOCKFILE(PROsfd f)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return PR_FAILURE;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);

    rv = LockFileEx((HANDLE)f,
                    LOCKFILE_EXCLUSIVE_LOCK,
                    0,
                    0x7fffffff,
                    0,
                    &me->md.overlapped.overlapped);

    if (_native_threads_only) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return PR_FAILURE;
        }
        _PR_THREAD_UNLOCK(me);

        if (rv == FALSE) {
            err = GetLastError();
            PR_ASSERT(err != ERROR_IO_PENDING);
            _PR_MD_MAP_LOCKF_ERROR(err);
            return PR_FAILURE;
        }
        return PR_SUCCESS;
    }

    /* HACK AROUND NT BUG
     * NT 3.51 has a bug.  In NT 3.51, if LockFileEx returns true, you
     * don't get any completion on the completion port.  This is a bug.
     *
     * They fixed it on NT4.0 so that you do get a completion.
     *
     * If we pretend we won't get a completion, NSPR gets confused later
     * when the unexpected completion arrives.  If we assume we do get
     * a completion, we hang on 3.51.  Worse, Microsoft informs me that the
     * behavior varies on 3.51 depending on if you are using a network
     * file system or a local disk!
     *
     * Solution:  For now, _nt_version_gets_lockfile_completion is set
     * depending on whether or not this system is EITHER
     *      - running NT 4.0
     *      - running NT 3.51 with a service pack greater than 5.
     *
     * In the meantime, this code may not work on network file systems.
     *
     */

    if ( rv == FALSE && ((err = GetLastError()) != ERROR_IO_PENDING)) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return PR_FAILURE;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_LOCKF_ERROR(err);
        return PR_FAILURE;
    }
#ifdef _NEED_351_FILE_LOCKING_HACK
    else if (rv)  {
        /* If this is NT 3.51 and the file is local, then we won't get a
         * completion back from LockFile when it succeeded.
         */
        if (_nt_version_gets_lockfile_completion == PR_FALSE) {
            if ( IsFileLocal((HANDLE)f) == _PR_LOCAL_FILE) {
                me->io_pending = PR_FALSE;
                me->state = _PR_RUNNING;
                return PR_SUCCESS;
            }
        }
    }
#endif /* _NEED_351_FILE_LOCKING_HACK */

    if (_NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        _PR_THREAD_UNLOCK(me);
        return PR_FAILURE;
    }

    if (me->md.blocked_io_status == 0) {
        _PR_MD_MAP_LOCKF_ERROR(me->md.blocked_io_error);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PRStatus
_PR_MD_TLOCKFILE(PROsfd f)
{
    PRInt32 rv, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return PR_FAILURE;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));

    _PR_THREAD_LOCK(me);
    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        _PR_THREAD_UNLOCK(me);
        return -1;
    }
    me->io_pending = PR_TRUE;
    me->state = _PR_IO_WAIT;
    _PR_THREAD_UNLOCK(me);

    rv = LockFileEx((HANDLE)f,
                    LOCKFILE_FAIL_IMMEDIATELY|LOCKFILE_EXCLUSIVE_LOCK,
                    0,
                    0x7fffffff,
                    0,
                    &me->md.overlapped.overlapped);
    if (_native_threads_only) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return PR_FAILURE;
        }
        _PR_THREAD_UNLOCK(me);

        if (rv == FALSE) {
            err = GetLastError();
            PR_ASSERT(err != ERROR_IO_PENDING);
            _PR_MD_MAP_LOCKF_ERROR(err);
            return PR_FAILURE;
        }
        return PR_SUCCESS;
    }
    if ( rv == FALSE && ((err = GetLastError()) != ERROR_IO_PENDING)) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return PR_FAILURE;
        }
        _PR_THREAD_UNLOCK(me);

        _PR_MD_MAP_LOCKF_ERROR(err);
        return PR_FAILURE;
    }
#ifdef _NEED_351_FILE_LOCKING_HACK
    else if (rv)  {
        /* If this is NT 3.51 and the file is local, then we won't get a
         * completion back from LockFile when it succeeded.
         */
        if (_nt_version_gets_lockfile_completion == PR_FALSE) {
            if ( IsFileLocal((HANDLE)f) == _PR_LOCAL_FILE) {
                _PR_THREAD_LOCK(me);
                me->io_pending = PR_FALSE;
                me->state = _PR_RUNNING;
                if (_PR_PENDING_INTERRUPT(me)) {
                    me->flags &= ~_PR_INTERRUPT;
                    PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                    _PR_THREAD_UNLOCK(me);
                    return PR_FAILURE;
                }
                _PR_THREAD_UNLOCK(me);

                return PR_SUCCESS;
            }
        }
    }
#endif /* _NEED_351_FILE_LOCKING_HACK */

    if (_NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
        _PR_THREAD_LOCK(me);
        me->io_pending = PR_FALSE;
        me->state = _PR_RUNNING;
        if (_PR_PENDING_INTERRUPT(me)) {
            me->flags &= ~_PR_INTERRUPT;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            _PR_THREAD_UNLOCK(me);
            return PR_FAILURE;
        }
        _PR_THREAD_UNLOCK(me);

        return PR_FAILURE;
    }

    if (me->md.blocked_io_status == 0) {
        _PR_MD_MAP_LOCKF_ERROR(me->md.blocked_io_error);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


PRStatus
_PR_MD_UNLOCKFILE(PROsfd f)
{
    PRInt32 rv;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (me->io_suspended) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return PR_FAILURE;
    }

    memset(&(me->md.overlapped.overlapped), 0, sizeof(OVERLAPPED));

    rv = UnlockFileEx((HANDLE)f,
                      0,
                      0x7fffffff,
                      0,
                      &me->md.overlapped.overlapped);

    if (rv) {
        return PR_SUCCESS;
    }
    else {
        int err = GetLastError();
        _PR_MD_MAP_LOCKF_ERROR(err);
        return PR_FAILURE;
    }
}

void
_PR_MD_MAKE_NONBLOCK(PRFileDesc *f)
{
    /*
     * On NT, we either call _md_Associate() or _md_MakeNonblock(),
     * depending on whether the socket is blocking or not.
     *
     * Once we associate a socket with the io completion port,
     * there is no way to disassociate it from the io completion
     * port.  So we have to call _md_Associate/_md_MakeNonblock
     * lazily.
     */
}

#ifdef _NEED_351_FILE_LOCKING_HACK
/***************
**
** Lockfile hacks
**
** The following code is a hack to work around a microsoft bug with lockfile.
** The problem is that on NT 3.51, if LockFileEx() succeeds, you never
** get a completion back for files that are on local disks.  So, we need to
** know if a file is local or remote so we can tell if we should expect
** a completion.
**
** The only way to check if a file is local or remote based on the handle is
** to get the serial number for the volume it is mounted on and then to
** compare that with mounted drives.  This code caches the volume numbers of
** fixed disks and does a relatively quick check.
**
** Locking:  Since the only thing we ever do when multithreaded is a 32bit
**           assignment, we probably don't need locking.  It is included just
**           case anyway.
**
** Limitations:  Does not work on floppies because they are too slow
**               Unknown if it will work on wierdo 3rd party file systems
**
****************
*/

/* There can only be 26 drive letters on NT */
#define _PR_MAX_DRIVES 26

_MDLock cachedVolumeLock;
DWORD dwCachedVolumeSerialNumbers[_PR_MAX_DRIVES] = {0};
DWORD dwLastCachedDrive = 0;
DWORD dwRemoveableDrivesToCheck = 0; /* bitmask for removeable drives */

PRBool IsFileLocalInit()
{
    TCHAR lpBuffer[_PR_MAX_DRIVES*5];
    DWORD nBufferLength = _PR_MAX_DRIVES*5;
    DWORD nBufferNeeded = GetLogicalDriveStrings(0, NULL);
    DWORD dwIndex = 0;
    DWORD dwDriveType;
    DWORD dwVolumeSerialNumber;
    DWORD dwDriveIndex = 0;
    DWORD oldmode = (DWORD) -1;

    _MD_NEW_LOCK(&cachedVolumeLock);

    nBufferNeeded = GetLogicalDriveStrings(nBufferLength, lpBuffer);
    if (nBufferNeeded == 0 || nBufferNeeded > nBufferLength) {
        return PR_FALSE;
    }

    // Calling GetVolumeInformation on a removeable drive where the
    // disk is currently removed will cause a dialog box to the
    // console.  This is not good.
    // Temporarily disable the SEM_FAILCRITICALERRORS to avoid the
    // damn dialog.

    dwCachedVolumeSerialNumbers[dwDriveIndex] = 0;
    oldmode = SetErrorMode(SEM_FAILCRITICALERRORS);

    // now loop through the logical drives
    while(lpBuffer[dwIndex] != TEXT('\0'))
    {
        // skip the floppy drives.  This is *SLOW*
        if ((lpBuffer[dwIndex] == TEXT('A')) || (lpBuffer[dwIndex] == TEXT('B')))
            /* Skip over floppies */;
        else
        {
            dwDriveIndex = (lpBuffer[dwIndex] - TEXT('A'));

            dwDriveType = GetDriveType(&lpBuffer[dwIndex]);

            switch(dwDriveType)
            {
                // Ignore these drive types
                case 0:
                case 1:
                case DRIVE_REMOTE:
                default: // If the drive type is unknown, ignore it.
                    break;

                // Removable media drives can have different serial numbers
                // at different times, so cache the current serial number
                // but keep track of them so they can be rechecked if necessary.
                case DRIVE_REMOVABLE:

                // CDROM is a removable media
                case DRIVE_CDROM:

                // no idea if ramdisks can change serial numbers or not
                // but it doesn't hurt to treat them as removable.

                case DRIVE_RAMDISK:


                    // Here is where we keep track of removable drives.
                    dwRemoveableDrivesToCheck |= 1 << dwDriveIndex;

                // removable drives fall through to fixed drives and get cached.

                case DRIVE_FIXED:

                    // cache volume serial numbers.
                    if (GetVolumeInformation(
                            &lpBuffer[dwIndex],
                            NULL, 0,
                            &dwVolumeSerialNumber,
                            NULL, NULL, NULL, 0)
                       )
                    {
                        if (dwLastCachedDrive < dwDriveIndex) {
                            dwLastCachedDrive = dwDriveIndex;
                        }
                        dwCachedVolumeSerialNumbers[dwDriveIndex] = dwVolumeSerialNumber;
                    }

                    break;
            }
        }

        dwIndex += lstrlen(&lpBuffer[dwIndex]) +1;
    }

    if (oldmode != (DWORD) -1) {
        SetErrorMode(oldmode);
        oldmode = (DWORD) -1;
    }

    return PR_TRUE;
}

PRInt32 IsFileLocal(HANDLE hFile)
{
    DWORD dwIndex = 0, dwMask;
    BY_HANDLE_FILE_INFORMATION Info;
    TCHAR szDrive[4] = TEXT("C:\\");
    DWORD dwVolumeSerialNumber;
    DWORD oldmode = (DWORD) -1;
    int rv = _PR_REMOTE_FILE;

    if (!GetFileInformationByHandle(hFile, &Info)) {
        return -1;
    }

    // look to see if the volume serial number has been cached.
    _MD_LOCK(&cachedVolumeLock);
    while(dwIndex <= dwLastCachedDrive)
        if (dwCachedVolumeSerialNumbers[dwIndex++] == Info.dwVolumeSerialNumber)
        {
            _MD_UNLOCK(&cachedVolumeLock);
            return _PR_LOCAL_FILE;
        }
    _MD_UNLOCK(&cachedVolumeLock);

    // volume serial number not found in the cache.  Check removable files.
    // removable drives are noted as a bitmask.  If the bit associated with
    // a specific drive is set, then we should query its volume serial number
    // as its possible it has changed.
    dwMask = dwRemoveableDrivesToCheck;
    dwIndex = 0;

    while(dwMask)
    {
        while(!(dwMask & 1))
        {
            dwIndex++;
            dwMask = dwMask >> 1;
        }

        szDrive[0] = TEXT('A')+ (TCHAR) dwIndex;

        // Calling GetVolumeInformation on a removeable drive where the
        // disk is currently removed will cause a dialog box to the
        // console.  This is not good.
        // Temporarily disable the SEM_FAILCRITICALERRORS to avoid the
        // dialog.

        oldmode = SetErrorMode(SEM_FAILCRITICALERRORS);

        if (GetVolumeInformation(
                szDrive,
                NULL, 0,
                &dwVolumeSerialNumber,
                NULL, NULL, NULL, 0)
           )
        {
            if (dwVolumeSerialNumber == Info.dwVolumeSerialNumber)
            {
                _MD_LOCK(&cachedVolumeLock);
                if (dwLastCachedDrive < dwIndex) {
                    dwLastCachedDrive = dwIndex;
                }
                dwCachedVolumeSerialNumbers[dwIndex] = dwVolumeSerialNumber;
                _MD_UNLOCK(&cachedVolumeLock);
                rv = _PR_LOCAL_FILE;
            }
        }
        if (oldmode != (DWORD) -1) {
            SetErrorMode(oldmode);
            oldmode = (DWORD) -1;
        }

        if (rv == _PR_LOCAL_FILE) {
            return _PR_LOCAL_FILE;
        }

        dwIndex++;
        dwMask = dwMask >> 1;
    }

    return _PR_REMOTE_FILE;
}
#endif /* _NEED_351_FILE_LOCKING_HACK */

PR_IMPLEMENT(PRStatus) PR_NT_CancelIo(PRFileDesc *fd)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRBool fWait;
    PRFileDesc *bottom;

    bottom = PR_GetIdentitiesLayer(fd, PR_NSPR_IO_LAYER);
    if (!me->io_suspended || (NULL == bottom) ||
        (me->io_fd != bottom->secret->md.osfd)) {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return PR_FAILURE;
    }
    /*
     * The CancelIO operation has to be issued by the same NT thread that
     * issued the I/O operation
     */
    PR_ASSERT(_PR_IS_NATIVE_THREAD(me) || (me->cpu == me->md.thr_bound_cpu));
    if (me->io_pending) {
        if (!CancelIo((HANDLE)bottom->secret->md.osfd)) {
            PR_SetError(PR_INVALID_STATE_ERROR, GetLastError());
            return PR_FAILURE;
        }
    }
    _PR_THREAD_LOCK(me);
    fWait = me->io_pending;
    me->io_suspended = PR_FALSE;
    me->state = _PR_IO_WAIT;
    me->md.interrupt_disabled = PR_TRUE;
    _PR_THREAD_UNLOCK(me);
    if (fWait) {
        _NT_IO_WAIT(me, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_ASSERT(me->io_suspended ==  PR_FALSE);
    PR_ASSERT(me->io_pending ==  PR_FALSE);

    _PR_THREAD_LOCK(me);
    me->md.interrupt_disabled = PR_FALSE;
    me->md.thr_bound_cpu = NULL;
    me->io_suspended = PR_FALSE;
    me->io_pending = PR_FALSE;
    me->state = _PR_RUNNING;
    _PR_THREAD_UNLOCK(me);
    return PR_SUCCESS;
}

static PROsfd _nt_nonblock_accept(PRFileDesc *fd, struct sockaddr *addr, int *addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    SOCKET sock;
    PRInt32 rv, err;
    fd_set rd;
    struct timeval tv, *tvp;

    FD_ZERO(&rd);
    FD_SET((SOCKET)osfd, &rd);
    if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        while ((sock = accept(osfd, addr, addrlen)) == -1) {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking)) {
                if ((rv = _PR_NTFiberSafeSelect(0, &rd, NULL, NULL,
                                                NULL)) == -1) {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    break;
                }
            } else {
                _PR_MD_MAP_ACCEPT_ERROR(err);
                break;
            }
        }
    } else if (timeout == PR_INTERVAL_NO_WAIT) {
        if ((sock = accept(osfd, addr, addrlen)) == -1) {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking)) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            } else {
                _PR_MD_MAP_ACCEPT_ERROR(err);
            }
        }
    } else {
retry:
        if ((sock = accept(osfd, addr, addrlen)) == -1) {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking)) {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;

                rv = _PR_NTFiberSafeSelect(0, &rd, NULL, NULL, tvp);
                if (rv > 0) {
                    goto retry;
                } else if (rv == 0) {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                } else {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                }
            } else {
                _PR_MD_MAP_ACCEPT_ERROR(err);
            }
        }
    }
    return (PROsfd)sock;
}

static PRInt32 _nt_nonblock_connect(PRFileDesc *fd, struct sockaddr *addr, int addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv;
    int err;
    fd_set wr, ex;
    struct timeval tv, *tvp;
    int len;

    if ((rv = connect(osfd, addr, addrlen)) == -1) {
        if ((err = WSAGetLastError()) == WSAEWOULDBLOCK) {
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) {
                tvp = NULL;
            } else {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&wr);
            FD_ZERO(&ex);
            FD_SET((SOCKET)osfd, &wr);
            FD_SET((SOCKET)osfd, &ex);
            if ((rv = _PR_NTFiberSafeSelect(0, NULL, &wr, &ex,
                                            tvp)) == -1) {
                _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return rv;
            }
            if (rv == 0) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                return -1;
            }
            /* Call Sleep(0) to work around a Winsock timeing bug. */
            Sleep(0);
            if (FD_ISSET((SOCKET)osfd, &ex)) {
                len = sizeof(err);
                if (getsockopt(osfd, SOL_SOCKET, SO_ERROR,
                               (char *) &err, &len) == SOCKET_ERROR) {
                    _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
                    return -1;
                }
                _PR_MD_MAP_CONNECT_ERROR(err);
                return -1;
            }
            PR_ASSERT(FD_ISSET((SOCKET)osfd, &wr));
            rv = 0;
        } else {
            _PR_MD_MAP_CONNECT_ERROR(err);
        }
    }
    return rv;
}

static PRInt32 _nt_nonblock_recv(PRFileDesc *fd, char *buf, int len, int flags, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set rd;
    int osflags;

    if (0 == flags) {
        osflags = 0;
    } else {
        PR_ASSERT(PR_MSG_PEEK == flags);
        osflags = MSG_PEEK;
    }
    while ((rv = recv(osfd,buf,len,osflags)) == -1) {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
            && (!fd->secret->nonblocking)) {
            FD_ZERO(&rd);
            FD_SET((SOCKET)osfd, &rd);
            if (timeout == PR_INTERVAL_NO_TIMEOUT) {
                tvp = NULL;
            } else {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            if ((rv = _PR_NTFiberSafeSelect(0, &rd, NULL, NULL,
                                            tvp)) == -1) {
                _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                break;
            } else if (rv == 0) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                rv = -1;
                break;
            }
        } else {
            _PR_MD_MAP_RECV_ERROR(err);
            break;
        }
    }
    return(rv);
}

static PRInt32 _nt_nonblock_send(PRFileDesc *fd, char *buf, int len, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set wd;
    PRInt32 bytesSent = 0;

    while(bytesSent < len) {
        while ((rv = send(osfd,buf,len,0)) == -1) {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking)) {
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) {
                    tvp = NULL;
                } else {
                    tv.tv_sec = PR_IntervalToSeconds(timeout);
                    tv.tv_usec = PR_IntervalToMicroseconds(
                                     timeout - PR_SecondsToInterval(tv.tv_sec));
                    tvp = &tv;
                }
                FD_ZERO(&wd);
                FD_SET((SOCKET)osfd, &wd);
                if ((rv = _PR_NTFiberSafeSelect(0, NULL, &wd, NULL,
                                                tvp)) == -1) {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    return -1;
                }
                if (rv == 0) {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                    return -1;
                }
            } else {
                _PR_MD_MAP_SEND_ERROR(err);
                return -1;
            }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking) {
            break;
        }
        if (bytesSent < len) {
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) {
                tvp = NULL;
            } else {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&wd);
            FD_SET((SOCKET)osfd, &wd);
            if ((rv = _PR_NTFiberSafeSelect(0, NULL, &wd, NULL,
                                            tvp)) == -1) {
                _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return -1;
            }
            if (rv == 0) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                return -1;
            }
        }
    }
    return bytesSent;
}

static PRInt32 _nt_nonblock_writev(PRFileDesc *fd, const PRIOVec *iov, int size, PRIntervalTime timeout)
{
    int index;
    int sent = 0;
    int rv;

    for (index=0; index<size; index++) {
        rv = _nt_nonblock_send(fd, iov[index].iov_base, iov[index].iov_len, timeout);
        if (rv > 0) {
            sent += rv;
        }
        if ( rv != iov[index].iov_len ) {
            if (rv < 0) {
                if (fd->secret->nonblocking
                    && (PR_GetError() == PR_WOULD_BLOCK_ERROR)
                    && (sent > 0)) {
                    return sent;
                } else {
                    return -1;
                }
            }
            /* Only a nonblocking socket can have partial sends */
            PR_ASSERT(fd->secret->nonblocking);
            return sent;
        }
    }

    return sent;
}

static PRInt32 _nt_nonblock_sendto(
    PRFileDesc *fd, const char *buf, int len,
    const struct sockaddr *addr, int addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set wd;
    PRInt32 bytesSent = 0;

    while(bytesSent < len) {
        while ((rv = sendto(osfd,buf,len,0, addr, addrlen)) == -1) {
            if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
                && (!fd->secret->nonblocking)) {
                if ( timeout == PR_INTERVAL_NO_TIMEOUT ) {
                    tvp = NULL;
                } else {
                    tv.tv_sec = PR_IntervalToSeconds(timeout);
                    tv.tv_usec = PR_IntervalToMicroseconds(
                                     timeout - PR_SecondsToInterval(tv.tv_sec));
                    tvp = &tv;
                }
                FD_ZERO(&wd);
                FD_SET((SOCKET)osfd, &wd);
                if ((rv = _PR_NTFiberSafeSelect(0, NULL, &wd, NULL,
                                                tvp)) == -1) {
                    _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                    return -1;
                }
                if (rv == 0) {
                    PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                    return -1;
                }
            } else {
                _PR_MD_MAP_SENDTO_ERROR(err);
                return -1;
            }
        }
        bytesSent += rv;
        if (fd->secret->nonblocking) {
            break;
        }
        if (bytesSent < len) {
            if ( timeout == PR_INTERVAL_NO_TIMEOUT ) {
                tvp = NULL;
            } else {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&wd);
            FD_SET((SOCKET)osfd, &wd);
            if ((rv = _PR_NTFiberSafeSelect(0, NULL, &wd, NULL,
                                            tvp)) == -1) {
                _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                return -1;
            }
            if (rv == 0) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                return -1;
            }
        }
    }
    return bytesSent;
}

static PRInt32 _nt_nonblock_recvfrom(PRFileDesc *fd, char *buf, int len, struct sockaddr *addr, int *addrlen, PRIntervalTime timeout)
{
    PROsfd osfd = fd->secret->md.osfd;
    PRInt32 rv, err;
    struct timeval tv, *tvp;
    fd_set rd;

    while ((rv = recvfrom(osfd,buf,len,0,addr, addrlen)) == -1) {
        if (((err = WSAGetLastError()) == WSAEWOULDBLOCK)
            && (!fd->secret->nonblocking)) {
            if (timeout == PR_INTERVAL_NO_TIMEOUT) {
                tvp = NULL;
            } else {
                tv.tv_sec = PR_IntervalToSeconds(timeout);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                 timeout - PR_SecondsToInterval(tv.tv_sec));
                tvp = &tv;
            }
            FD_ZERO(&rd);
            FD_SET((SOCKET)osfd, &rd);
            if ((rv = _PR_NTFiberSafeSelect(0, &rd, NULL, NULL,
                                            tvp)) == -1) {
                _PR_MD_MAP_SELECT_ERROR(WSAGetLastError());
                break;
            } else if (rv == 0) {
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                rv = -1;
                break;
            }
        } else {
            _PR_MD_MAP_RECVFROM_ERROR(err);
            break;
        }
    }
    return(rv);
}

/*
 * UDP support: the continuation thread functions and recvfrom and sendto.
 */

static void pt_InsertTimedInternal(pt_Continuation *op)
{
    PRInt32 delta = 0;
    pt_Continuation *t_op = NULL;
    PRIntervalTime now = PR_IntervalNow(), op_tmo, qd_tmo;

    /*
     * If this element operation isn't timed, it gets queued at the
     * end of the list (just after pt_tq.tail) and we're
     * finishd early.
     */
    if (PR_INTERVAL_NO_TIMEOUT == op->timeout)
    {
        t_op = pt_tq.tail;  /* put it at the end */
        goto done;
    }

    /*
     * The rest of this routine actaully deals with timed ops.
     */

    if (NULL != pt_tq.op)
    {
        /*
         * To find where in the list to put the new operation, form
         * the absolute time the operations in question will expire.
         *
         * The new operation ('op') will expire at now() + op->timeout.
         *
         * The operation that will time out furthest in the future will
         * do so at pt_tq.epoch + pt_tq.op->timeout.
         *
         * Subsequently earlier timeouts are computed based on the latter
         * knowledge by subracting the timeout deltas that are stored in
         * the operation list. There are operation[n]->timeout ticks
         * between the expiration of operation[n-1] and operation[n].e e
         *
         * Therefore, the operation[n-1] will expire operation[n]->timeout
         * ticks prior to operation[n].
         *
         * This should be easy!
         */
        t_op = pt_tq.op;  /* running pointer to queued op */
        op_tmo = now + op->timeout;  /* that's in absolute ticks */
        qd_tmo = pt_tq.epoch + t_op->timeout;  /* likewise */

        do
        {
            /*
             * If 'op' expires later than t_op, then insert 'op' just
             * ahead of t_op. Otherwise, compute when operation[n-1]
             * expires and try again.
             *
             * The actual different between the expiriation of 'op'
             * and the current operation what becomes the new operaton's
             * timeout interval. That interval is also subtracted from
             * the interval of the operation immediately following where
             * we stick 'op' (unless the next one isn't timed). The new
             * timeout assigned to 'op' takes into account the values of
             * now() and when the previous intervals were compured.
             */
            delta = op_tmo - qd_tmo;
            if (delta >= 0)
            {
                op->timeout += (now - pt_tq.epoch);
                goto done;
            }

            qd_tmo -= t_op->timeout;  /* previous operaton expiration */
            t_op = t_op->prev;  /* point to previous operation */
            if (NULL != t_op) {
                qd_tmo += t_op->timeout;
            }
        } while (NULL != t_op);

        /*
         * If we got here we backed off the head of the list. That means that
         * this timed entry has to go at the head of the list. This is just
         * about like having an empty timer list.
         */
        delta = op->timeout;  /* $$$ is this right? */
    }

done:

    /*
     * Insert 'op' into the queue just after t_op or if t_op is null,
     * at the head of the list.
     *
     * If t_op is NULL, the list is currently empty and this is pretty
     * easy.
     */
    if (NULL == t_op)
    {
        op->prev = NULL;
        op->next = pt_tq.head;
        pt_tq.head = op;
        if (NULL == pt_tq.tail) {
            pt_tq.tail = op;
        }
        else {
            op->next->prev = op;
        }
    }
    else
    {
        op->prev = t_op;
        op->next = t_op->next;
        if (NULL != op->prev) {
            op->prev->next = op;
        }
        if (NULL != op->next) {
            op->next->prev = op;
        }
        if (t_op == pt_tq.tail) {
            pt_tq.tail = op;
        }
    }

    /*
     * Are we adjusting our epoch, etc? Are we replacing
     * what was previously the element due to expire furthest
     * out in the future? Is this even a timed operation?
     */
    if (PR_INTERVAL_NO_TIMEOUT != op->timeout)
    {
        if ((NULL == pt_tq.op)  /* we're the one and only */
            || (t_op == pt_tq.op))  /* we're replacing */
        {
            pt_tq.op = op;
            pt_tq.epoch = now;
        }
    }

    pt_tq.op_count += 1;

}  /* pt_InsertTimedInternal */

/*
 * function: pt_FinishTimed
 *
 * Takes the finished operation out of the timed queue. It
 * notifies the initiating thread that the opertions is
 * complete and returns to the caller the value of the next
 * operation in the list (or NULL).
 */
static pt_Continuation *pt_FinishTimedInternal(pt_Continuation *op)
{
    pt_Continuation *next;

    /* remove this one from the list */
    if (NULL == op->prev) {
        pt_tq.head = op->next;
    }
    else {
        op->prev->next = op->next;
    }
    if (NULL == op->next) {
        pt_tq.tail = op->prev;
    }
    else {
        op->next->prev = op->prev;
    }

    /* did we happen to hit the timed op? */
    if (op == pt_tq.op) {
        pt_tq.op = op->prev;
    }

    next = op->next;
    op->next = op->prev = NULL;
    op->status = pt_continuation_done;

    pt_tq.op_count -= 1;
#if defined(DEBUG)
    pt_debug.continuationsServed += 1;
#endif
    PR_NotifyCondVar(op->complete);

    return next;
}  /* pt_FinishTimedInternal */

static void ContinuationThread(void *arg)
{
    /* initialization */
    fd_set readSet, writeSet, exceptSet;
    struct timeval tv;
    SOCKET *pollingList = 0;                /* list built for polling */
    PRIntn pollingListUsed;                 /* # entries used in the list */
    PRIntn pollingListNeeded;               /* # entries needed this time */
    PRIntn pollingSlotsAllocated = 0;       /* # entries available in list */
    PRIntervalTime mx_select_ticks = PR_MillisecondsToInterval(PT_DEFAULT_SELECT_MSEC);

    /* do some real work */
    while (1)
    {
        PRIntn rv;
        PRStatus status;
        PRIntn pollIndex;
        pt_Continuation *op;
        PRIntervalTime now = PR_IntervalNow();
        PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;

        PR_Lock(pt_tq.ml);
        while (NULL == pt_tq.head)
        {
            status = PR_WaitCondVar(pt_tq.new_op, PR_INTERVAL_NO_TIMEOUT);
            if ((PR_FAILURE == status)
                && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) {
                break;
            }
        }
        pollingListNeeded = pt_tq.op_count;
        PR_Unlock(pt_tq.ml);

        /* Okay. We're history */
        if ((PR_FAILURE == status)
            && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) {
            break;
        }

        /*
         * We are not holding the pt_tq.ml lock now, so more items may
         * get added to pt_tq during this window of time.  We hope
         * that 10 more spaces in the polling list should be enough.
         */

        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        pollingListNeeded += 10;
        if (pollingListNeeded > pollingSlotsAllocated)
        {
            if (NULL != pollingList) {
                PR_DELETE(pollingList);
            }
            pollingList = PR_MALLOC(pollingListNeeded * sizeof(PRPollDesc));
            PR_ASSERT(NULL != pollingList);
            pollingSlotsAllocated = pollingListNeeded;
        }

#if defined(DEBUG)
        if (pollingListNeeded > pt_debug.pollingListMax) {
            pt_debug.pollingListMax = pollingListUsed;
        }
#endif

        /*
         * Build up a polling list.
         * This list is sorted on time. Operations that have been
         * interrupted are completed and not included in the list.
         * There is an assertion that the operation is in progress.
         */
        pollingListUsed = 0;
        PR_Lock(pt_tq.ml);

        for (op = pt_tq.head; NULL != op;)
        {
            if (pt_continuation_abort == op->status)
            {
                op->result.code = -1;
                op->syserrno = WSAEINTR;
                op = pt_FinishTimedInternal(op);
            }
            else
            {
                PR_ASSERT(pt_continuation_done != op->status);
                op->status = pt_continuation_inprogress;
                if (op->event & PR_POLL_READ) {
                    FD_SET(op->arg1.osfd, &readSet);
                }
                if (op->event & PR_POLL_WRITE) {
                    FD_SET(op->arg1.osfd, &writeSet);
                }
                if (op->event & PR_POLL_EXCEPT) {
                    FD_SET(op->arg1.osfd, &exceptSet);
                }
                pollingList[pollingListUsed] = op->arg1.osfd;
                pollingListUsed += 1;
                if (pollingListUsed == pollingSlotsAllocated) {
                    break;
                }
                op = op->next;
            }
        }

        PR_Unlock(pt_tq.ml);

        /*
         * If 'op' isn't NULL at this point, then we didn't get to
         * the end of the list. That means that more items got added
         * to the list than we anticipated. So, forget this iteration,
         * go around the horn again.
         * One would hope this doesn't happen all that often.
         */
        if (NULL != op)
        {
#if defined(DEBUG)
            pt_debug.predictionsFoiled += 1;  /* keep track */
#endif
            continue;  /* make it rethink things */
        }

        /* there's a chance that all ops got blown away */
        if (NULL == pt_tq.head) {
            continue;
        }
        /* if not, we know this is the shortest timeout */
        timeout = pt_tq.head->timeout;

        /*
         * We don't want to wait forever on this poll. So keep
         * the interval down. The operations, if they are timed,
         * still have to timeout, while those that are not timed
         * should persist forever. But they may be aborted. That's
         * what this anxiety is all about.
         */
        if (timeout > mx_select_ticks) {
            timeout = mx_select_ticks;
        }

        if (PR_INTERVAL_NO_TIMEOUT != pt_tq.head->timeout) {
            pt_tq.head->timeout -= timeout;
        }
        tv.tv_sec = PR_IntervalToSeconds(timeout);
        tv.tv_usec = PR_IntervalToMicroseconds(timeout) % PR_USEC_PER_SEC;

        rv = select(0, &readSet, &writeSet, &exceptSet, &tv);

        if (0 == rv)  /* poll timed out - what about leading op? */
        {
            if (0 == pt_tq.head->timeout)
            {
                /*
                 * The leading element of the timed queue has timed
                 * out. Get rid of it. In any case go around the
                 * loop again, computing the polling list, checking
                 * for interrupted operations.
                 */
                PR_Lock(pt_tq.ml);
                do
                {
                    pt_tq.head->result.code = -1;
                    pt_tq.head->syserrno = WSAETIMEDOUT;
                    op = pt_FinishTimedInternal(pt_tq.head);
                } while ((NULL != op) && (0 == op->timeout));
                PR_Unlock(pt_tq.ml);
            }
            continue;
        }

        if (-1 == rv && (WSAGetLastError() == WSAEINTR
                         || WSAGetLastError() == WSAEINPROGRESS))
        {
            continue;               /* go around the loop again */
        }

        /*
         * select() says that something in our list is ready for some more
         * action or is an invalid fd. Find it, load up the operation and
         * see what happens.
         */

        PR_ASSERT(rv > 0 || WSAGetLastError() == WSAENOTSOCK);


        /*
         * $$$ There's a problem here. I'm running the operations list
         * and I'm not holding any locks. I don't want to hold the lock
         * and do the operation, so this is really messed up..
         *
         * This may work out okay. The rule is that only this thread,
         * the continuation thread, can remove elements from the list.
         * Therefore, the list is at worst, longer than when we built
         * the polling list.
         */
        op = pt_tq.head;
        for (pollIndex = 0; pollIndex < pollingListUsed; ++pollIndex)
        {
            PRInt16 revents = 0;

            PR_ASSERT(NULL != op);

            /*
             * This one wants attention. Redo the operation.
             * We know that there can only be more elements
             * in the op list than we knew about when we created
             * the poll list. Therefore, we might have to skip
             * a few ops to find the right one to operation on.
             */
            while (pollingList[pollIndex] != op->arg1.osfd )
            {
                op = op->next;
                PR_ASSERT(NULL != op);
            }

            if (FD_ISSET(op->arg1.osfd, &readSet)) {
                revents |= PR_POLL_READ;
            }
            if (FD_ISSET(op->arg1.osfd, &writeSet)) {
                revents |= PR_POLL_WRITE;
            }
            if (FD_ISSET(op->arg1.osfd, &exceptSet)) {
                revents |= PR_POLL_EXCEPT;
            }

            /*
             * Sip over all those not in progress. They'll be
             * pruned next time we build a polling list. Call
             * the continuation function. If it reports completion,
             * finish off the operation.
             */
            if (revents && (pt_continuation_inprogress == op->status)
                && (op->function(op, revents)))
            {
                PR_Lock(pt_tq.ml);
                op = pt_FinishTimedInternal(op);
                PR_Unlock(pt_tq.ml);
            }
        }
    }
    if (NULL != pollingList) {
        PR_DELETE(pollingList);
    }
}  /* ContinuationThread */

static int pt_Continue(pt_Continuation *op)
{
    PRStatus rv;
    /* Finish filling in the blank slots */
    op->status = pt_continuation_sumbitted;
    op->complete = PR_NewCondVar(pt_tq.ml);

    PR_Lock(pt_tq.ml);  /* we provide the locking */

    pt_InsertTimedInternal(op);  /* insert in the structure */

    PR_NotifyCondVar(pt_tq.new_op);  /* notify the continuation thread */

    while (pt_continuation_done != op->status)  /* wait for completion */
    {
        rv = PR_WaitCondVar(op->complete, PR_INTERVAL_NO_TIMEOUT);
        /*
         * If we get interrupted, we set state the continuation thread will
         * see and allow it to finish the I/O operation w/ error. That way
         * the rule that only the continuation thread is removing elements
         * from the list is still valid.
         *
         * Don't call interrupt on the continuation thread. That'll just
         * piss him off. He's cycling around at least every mx_select_ticks
         * anyhow and should notice the request in there.
         */
        if ((PR_FAILURE == rv)
            && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) {
            op->status = pt_continuation_abort;    /* our status */
        }
    }

    PR_Unlock(pt_tq.ml);  /* we provide the locking */

    PR_DestroyCondVar(op->complete);

    return op->result.code;  /* and the primary answer */
}  /* pt_Continue */

static PRBool pt_sendto_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes = sendto(
                       op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags,
                       (struct sockaddr*)op->arg5.addr, sizeof(*(op->arg5.addr)));
    op->syserrno = WSAGetLastError();
    if (bytes > 0)  /* this is progress */
    {
        char *bp = op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else return ((-1 == bytes) && (WSAEWOULDBLOCK == op->syserrno)) ?
                    PR_FALSE : PR_TRUE;
}  /* pt_sendto_cont */

static PRBool pt_recvfrom_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn addr_len = sizeof(*(op->arg5.addr));
    op->result.code = recvfrom(
                          op->arg1.osfd, op->arg2.buffer, op->arg3.amount,
                          op->arg4.flags, (struct sockaddr*)op->arg5.addr, &addr_len);
    op->syserrno = WSAGetLastError();
    return ((-1 == op->result.code) && (WSAEWOULDBLOCK == op->syserrno)) ?
           PR_FALSE : PR_TRUE;
}  /* pt_recvfrom_cont */

static PRInt32 pt_SendTo(
    SOCKET osfd, const void *buf,
    PRInt32 amount, PRInt32 flags, const PRNetAddr *addr,
    PRIntn addrlen, PRIntervalTime timeout)
{
    PRInt32 bytes = -1, err;
    PRBool fNeedContinue = PR_FALSE;

    bytes = sendto(
                osfd, buf, amount, flags,
                (struct sockaddr*)addr, PR_NETADDR_SIZE(addr));
    if (bytes == -1) {
        if ((err = WSAGetLastError()) == WSAEWOULDBLOCK) {
            fNeedContinue = PR_TRUE;
        }
        else {
            _PR_MD_MAP_SENDTO_ERROR(err);
        }
    }
    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = (PRNetAddr*)addr;
        op.timeout = timeout;
        op.result.code = 0;  /* initialize the number sent */
        op.function = pt_sendto_cont;
        op.event = PR_POLL_WRITE | PR_POLL_EXCEPT;
        bytes = pt_Continue(&op);
        if (bytes < 0) {
            WSASetLastError(op.syserrno);
            _PR_MD_MAP_SENDTO_ERROR(op.syserrno);
        }
    }
    return bytes;
}  /* pt_SendTo */

static PRInt32 pt_RecvFrom(SOCKET osfd, void *buf, PRInt32 amount,
                           PRInt32 flags, PRNetAddr *addr, PRIntn *addr_len, PRIntervalTime timeout)
{
    PRInt32 bytes = -1, err;
    PRBool fNeedContinue = PR_FALSE;

    bytes = recvfrom(
                osfd, buf, amount, flags,
                (struct sockaddr*)addr, addr_len);
    if (bytes == -1) {
        if ((err = WSAGetLastError()) == WSAEWOULDBLOCK) {
            fNeedContinue = PR_TRUE;
        }
        else {
            _PR_MD_MAP_RECVFROM_ERROR(err);
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = addr;
        op.timeout = timeout;
        op.function = pt_recvfrom_cont;
        op.event = PR_POLL_READ | PR_POLL_EXCEPT;
        bytes = pt_Continue(&op);
        if (bytes < 0) {
            WSASetLastError(op.syserrno);
            _PR_MD_MAP_RECVFROM_ERROR(op.syserrno);
        }
    }
    return bytes;
}  /* pt_RecvFrom */
