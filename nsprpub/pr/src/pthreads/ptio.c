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
** File:   ptio.c
** Descritpion:  Implemenation of I/O methods for pthreads
** Exports:   ptio.h
*/

#if defined(_PR_PTHREADS)

#include <string.h>  /* for memset() */
#include <sys/types.h>
#include <dirent.h>

/*
 * XXX: On Linux 2.0.27 sched.h uses this _P macro that seems to be undefined.
 * I suspect that it is a typo (should be __P).
 */
#if defined(LINUX)
#define _P(x) __P(x)
#endif
#include <pthread.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef SOLARIS
#include <sys/filio.h>  /* to pick up FIONREAD */
#endif
/* Linux (except glibc) and FreeBSD don't have poll */
#if !(defined(LINUX) && !(defined(__GLIBC__) && __GLIBC__ >= 2)) \
	&& !defined(FREEBSD)
#include <poll.h>
#endif
#ifdef AIX
/* To pick up sysconf() */
#include <unistd.h>
#else
/* To pick up getrlimit() etc. */
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "primpl.h"

/* On Alpha Linux, these are already defined in sys/socket.h */
#if !(defined(LINUX) && defined(__alpha))
#include <netinet/tcp.h>  /* TCP_NODELAY, TCP_MAXSEG */
#endif

#if defined(SOLARIS)
#define _PRSockOptVal_t char *
#elif defined(IRIX) || defined(OSF1) || defined(AIX) || defined(HPUX) \
	|| defined(LINUX) || defined(FREEBSD)
#define _PRSockOptVal_t void *
#else
#error "Cannot determine architecture"
#endif

#if (defined(HPUX) && !defined(HPUX10_30) && !defined(HPUX11))
#define _PRSelectFdSetArg_t int *
#elif defined(AIX4_1)
#define _PRSelectFdSetArg_t void *
#elif defined(IRIX) || (defined(AIX) && !defined(AIX4_1)) \
    || defined(OSF1) || defined(SOLARIS) \
    || defined(HPUX10_30) || defined(HPUX11) || defined(LINUX) \
    || defined(FREEBSD)
#define _PRSelectFdSetArg_t fd_set *
#else
#error "Cannot determine architecture"
#endif

static PRStatus pt_InitIOContinuation(void);
static PRFileDesc *pt_SetMethods(PRIntn osfd, PRDescType type);

static pthread_condattr_t _pt_cvar_attr;
static PRLock *_pr_flock_lock;  /* For PR_LockFile() etc. */
static PRLock *_pr_rename_lock;  /* For PR_Rename() */

extern struct _PT_Bookeeping pt_book;  /* defined in ptthread.c */
extern PRIntn pt_schedpriv;  /* defined in ptthread.c */

/*****************************************************************************/
/*****************************************************************************/
/************************** File descriptor caching **************************/
/*****************************************************************************/
/*****************************************************************************/

typedef struct _PT_Fd_Cache
{
    PRLock *ml;
    PRIntn count;
    PRIntn limit;
    PRFileDesc *fd;
} _PT_Fd_Cache;
static _PT_Fd_Cache pt_fd_cache;

/*
** Get a FileDescriptor from the cache if one exists. If not allocate
** a new one from the heap.
*/
static PRFileDesc *pt_Getfd(void)
{
    PRFileDesc *fd;
    do
    {
        fd = pt_fd_cache.fd;  /* quick, unsafe check */
        if (NULL == fd)
        {
            fd = PR_NEWZAP(PRFileDesc);
            if (NULL == fd) goto finished;
            fd->secret = PR_NEWZAP(PRFilePrivate);
            if (NULL == fd->secret)
            {
                PR_DELETE(fd);
                goto finished;
            }
        }
        else
        {
            PRFilePrivate *secret;
            PR_Lock(pt_fd_cache.ml);
            fd = pt_fd_cache.fd;  /* safer extraction */
            if (NULL != fd)
            {
                pt_fd_cache.count -= 1;
                pt_fd_cache.fd = fd->higher;
                fd->higher = NULL;
            }
            PR_Unlock(pt_fd_cache.ml);
            secret = fd->secret;
            memset(fd, 0, sizeof(PRFileDesc));
            memset(secret, 0, sizeof(PRFilePrivate));
            fd->secret = secret;
        }
    } while (NULL == fd);
finished:
    return fd;
}  /* pt_Getfd */

/*
** Return a file descriptor to the cache unless there are too many in
** there already. If put in cache, clear the fields first.
*/
static void pt_Putfd(PRFileDesc *fd)
{
    PR_ASSERT(_PR_FILEDESC_CLOSED == fd->secret->state);
    PR_ASSERT(pt_fd_cache.count < pt_fd_cache.limit);

    fd->secret->state = _PR_FILEDESC_FREED;
    if (pt_fd_cache.count > pt_fd_cache.limit)
    {
        PR_DELETE(fd->secret);
        PR_DELETE(fd);
    }
    else
    {
        PR_Lock(pt_fd_cache.ml);
        pt_fd_cache.count += 1;
        fd->higher = pt_fd_cache.fd;
        pt_fd_cache.fd = fd;
        PR_Unlock(pt_fd_cache.ml);
    }
}  /* pt_Putfd */
    
/*****************************************************************************/
/************************* I/O Continuation machinery ************************/
/*****************************************************************************/

/*
 * The polling interval defines the maximum amount of time that a thread
 * might hang up before an interrupt is noticed.
 */
#define PT_DEFAULT_POLL_MSEC 100

/*
 * Latest POSIX defines this type as socklen_t.  It may also be
 * size_t or int.
 */
#if (defined(LINUX) && defined(__GLIBC__) && __GLIBC__ >= 2 \
	&& !defined(__alpha))
typedef socklen_t pt_SockLen;
#elif defined(AIX) || (defined(LINUX) && defined(__alpha))
typedef PRSize pt_SockLen;
#else
typedef PRIntn pt_SockLen;
#endif

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revents);

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
    union { PRIntn osfd; } arg1;            /* #1 - the op's fd */
    union { void* buffer; } arg2;           /* #2 - primary transfer buffer */
    union {
        PRSize amount;                      /* #3 - size of 'buffer', or */
        pt_SockLen *addr_len;                  /*    - length of address */
#ifdef HPUX11
        /*
         * For sendfile()
         */
        off_t offset;                       /* offset in file to send */
#endif
    } arg3;
    union { PRIntn flags; } arg4;           /* #4 - read/write flags */
    union { PRNetAddr *addr; } arg5;        /* #5 - send/recv address */

#ifdef HPUX11
    /*
     * For sendfile()
     */
    int filedesc;                           /* descriptor of file to send */
    int nbytes_to_send;                     /* size of header and file */
#endif  /* HPUX11 */
    
    PRIntervalTime timeout;                 /* client (relative) timeout */
    PRIntervalTime absolute;                /* internal (absolute) timeout */

    PRInt16 event;                           /* flags for poll()'s events */

    /*
    ** The representation and notification of the results of the operation.
    ** These function can either return an int return code or a pointer to
    ** some object.
    */
    union { PRSize code; void *object; } result;

    PRIntn syserrno;                        /* in case it failed, why (errno) */
    pr_ContuationStatus status;             /* the status of the operation */
    PRCondVar complete;                     /* to notify the initiating thread */
};

static struct pt_TimedQueue
{
    PRCallOnceType once;                    /* controls the initialization
                                             * of this structure */
    PRLock *ml;                             /* a little protection */
    PRThread *thread;                       /* internal thread's identification */
    PRCondVar *new_op;                      /* new operation supplied */
    PRUintn op_count;                       /* number of operations in the list */
    pt_Continuation *head, *tail;           /* head/tail of list of operations */

    pt_Continuation *op;                    /* timed operation furthest in future */
    PRBool exitFlag;                        /* a Boolean flag for signaling the
                                             * continuation thread to exit */
} pt_tq;

#if defined(DEBUG)
static struct pt_debug_s
{
    PRUintn predictionsFoiled;
    PRUintn pollingListMax;
    PRUintn continuationsServed;
} pt_debug;
#endif  /* DEBUG */

/*
 * The following two functions, pt_InsertTimedInternal and
 * pt_FinishTimedInternal, are always called with the pt_tq.ml
 * lock held.  The "internal" in the functions' names come from
 * the Mesa programming language.  Internal functions are always
 * called from inside a monitor.
 */

static void pt_InsertTimedInternal(pt_Continuation *op)
{
    pt_Continuation *t_op = NULL;
    PRIntervalTime now = PR_IntervalNow();

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
     * The portion of this routine deals with timed ops.
     */
    op->absolute = now + op->timeout;  /* absolute ticks */
    if (NULL == pt_tq.op) pt_tq.op = op;
    else
    {
        /*
         * To find where in the list to put the new operation, based
         * on the absolute time the operation in question will expire.
         *
         * The new operation ('op') will expire at now() + op->timeout.
         *
         * This should be easy!
         */

        for (t_op = pt_tq.op; NULL != t_op; t_op = t_op->prev)
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
             * now() and when the previous intervals were computed.
             */
            if ((PRInt32)(op->absolute - t_op->absolute) >= 0)
            {
                if (t_op == pt_tq.op) pt_tq.op = op;
                break;
            }
        }
    }

done:

    /*
     * Insert 'op' into the queue just after t_op or if t_op is null,
     * at the head of the list.
     *
     * We need to set up the 'next' and 'prev' pointers of 'op'
     * correctly before inserting 'op' into the queue.  Also, we insert
     * 'op' by updating pt_tq.head or op->prev->next first, and then
     * updating op->next->prev.  We want to make sure that the 'next'
     * pointers are linked up correctly at all times so that we can
     * traverse the queue by starting with pt_tq.head and following
     * the 'next' pointers, without having to acquire the pt_tq.ml lock.
     * (We do that in ContinuationThread.)  We traverse the 'prev'
     * pointers only in this function, which is called with the lock held.
     *
     * Similar care is taken in pt_FinishTimedInternal where we remove
     * an op from the queue.
     */
    if (NULL == t_op)
    {
        op->prev = NULL;
        op->next = pt_tq.head;
        pt_tq.head = op;
        if (NULL == pt_tq.tail) pt_tq.tail = op;
        else op->next->prev = op;
    }
    else
    {
        op->prev = t_op;
        op->next = t_op->next;
        if (NULL != op->prev)
            op->prev->next = op;
        if (NULL != op->next)
            op->next->prev = op;
        if (t_op == pt_tq.tail)
            pt_tq.tail = op;
    }

    pt_tq.op_count += 1;

}  /* pt_InsertTimedInternal */

/*
 * function: pt_FinishTimedInternal
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
    if (NULL == op->prev) pt_tq.head = op->next;
    else op->prev->next = op->next;
    if (NULL == op->next) pt_tq.tail = op->prev;
    else op->next->prev = op->prev;

    /* did we happen to hit the timed op? */
    if (op == pt_tq.op) pt_tq.op = op->prev;

    next = op->next;
    op->next = op->prev = NULL;
    op->status = pt_continuation_done;

    pt_tq.op_count -= 1;

#if defined(DEBUG)
    pt_debug.continuationsServed += 1;
#endif
    PR_NotifyCondVar(&op->complete);

    return next;
}  /* pt_FinishTimedInternal */

static void ContinuationThread(void *arg)
{
    /* initialization */
    PRInt32 msecs, mx_poll_ticks;
    struct pollfd *pollingList = 0;         /* list built for polling */
    PRIntn pollingListUsed;                 /* # entries used in the list */
    PRIntn pollingListNeeded;               /* # entries needed this time */
    PRIntn pollingSlotsAllocated = 0;       /* # entries available in list */
    
    mx_poll_ticks = (PRInt32)PR_MillisecondsToInterval(PT_DEFAULT_POLL_MSEC);

    /* do some real work */
    while (PR_TRUE)
    {
        PRIntn rv;
        PRInt32 timeout;
        PRIntn pollIndex;
        PRIntervalTime now;
        pt_Continuation *op;

        PR_Lock(pt_tq.ml);
        while (!pt_tq.exitFlag && (NULL == pt_tq.head))
            PR_WaitCondVar(pt_tq.new_op, PR_INTERVAL_NO_TIMEOUT);
        pollingListNeeded = pt_tq.op_count;
        PR_Unlock(pt_tq.ml);

        /* Okay. We're history */
        if (pt_tq.exitFlag) break;

    /*
     * We are not holding the pt_tq.ml lock now, so more items may
     * get added to pt_tq during this window of time.  We hope
     * that 10 more spaces in the polling list should be enough.
     */
        pollingListNeeded += 10;
        if (pollingListNeeded > pollingSlotsAllocated)
        {
            if (NULL != pollingList) PR_DELETE(pollingList);
            pollingList = (struct pollfd*)PR_Malloc(
                pollingListNeeded * sizeof(struct pollfd));
            PR_ASSERT(NULL != pollingList);
            pollingSlotsAllocated = pollingListNeeded;
        }

#if defined(DEBUG)
        if (pollingListNeeded > pt_debug.pollingListMax)
            pt_debug.pollingListMax = pollingListNeeded;
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
                op->syserrno = EINTR;
                op = pt_FinishTimedInternal(op);
            }
            else
            {
                if (pollingListUsed == pollingSlotsAllocated) break;
                PR_ASSERT(pt_continuation_done != op->status);
                op->status = pt_continuation_inprogress;
                pollingList[pollingListUsed].revents = 0;
                pollingList[pollingListUsed].fd = op->arg1.osfd;
                pollingList[pollingListUsed].events = op->event;
                pollingListUsed += 1;
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

        if (NULL == pt_tq.head) continue;  /* did list evaporate? */

        /*
         * We don't want to wait forever on this poll. So keep
         * the interval down. The operations, if they are timed,
         * still have to timeout, while those that are not timed
         * should persist forever. But they may be aborted. That's
         * what this anxiety is all about.
         */
        if (PR_INTERVAL_NO_TIMEOUT == pt_tq.head->timeout)
            msecs = PT_DEFAULT_POLL_MSEC;
        else
        {
            timeout = pt_tq.head->absolute - PR_IntervalNow();
            if (timeout <= 0) msecs = 0;  /* already timed out */
            else if (timeout >= mx_poll_ticks) msecs = PT_DEFAULT_POLL_MSEC;
            else msecs = (PRInt32)PR_IntervalToMilliseconds(timeout);
    }

        rv = poll(pollingList, pollingListUsed, msecs);
        
        if ((-1 == rv) && ((errno == EINTR) || (errno == EAGAIN)))
            continue; /* go around the loop again */

    if (rv > 0)
    {
            /*
             * poll() says that something in our list is ready for some more
             * action. Find it, load up the operation and see what happens.
             */

            /*
             * This may work out okay. The rule is that only this thread,
             * the continuation thread, can remove elements from the list.
             * Therefore, the list is at worst, longer than when we built
             * the polling list.
             */

            op = pt_tq.head;
            for (pollIndex = 0; pollIndex < pollingListUsed; ++pollIndex)
            {
                PR_ASSERT(NULL != op);
                if (0 != pollingList[pollIndex].revents)
                {
                    /*
                     * This one wants attention. Redo the operation.
                     * We know that there can only be more elements
                     * in the op list than we knew about when we created
                     * the poll list. Therefore, we might have to skip
                     * a few ops to find the right one to operate on.
                     */
                    while ((pollingList[pollIndex].fd != op->arg1.osfd)
                    || (pollingList[pollIndex].events != op->event))
                    {
                        PR_ASSERT(NULL != op->next);  /* it has to be in there */
                        op = op->next;  /* keep advancing down the list */
                    }

                    /*
                     * Skip over all those not in progress. They'll be
                     * pruned next time we build a polling list. Call
                     * the continuation function. If it reports completion,
                     * finish off the operation.
                     */
                    if ((pt_continuation_inprogress == op->status)
                    && (op->function(op, pollingList[pollIndex].revents)))
                    {
                        PR_Lock(pt_tq.ml);
                        op = pt_FinishTimedInternal(op);
                        PR_Unlock(pt_tq.ml);
                    }
                    continue;
                }
                op = op->next;  /* progress to next operation */
            }
        }

        /*
         * This is timeout processing. It is done after checking
         * for good completions. Those that just made it under the
         * wire are lucky, but none the less, valid.
         */
        if ((NULL != pt_tq.head)
        && (PR_INTERVAL_NO_TIMEOUT != pt_tq.head->timeout))
        {
            now = PR_IntervalNow();
            while ((PRInt32)(pt_tq.head->absolute - now) <= 0)
            {
                /* 
                 * The leading element of the timed queue has timed
                 * out. Get rid of it. In any case go around the
                 * loop again, computing the polling list, checking
                 * for interrupted operations.
                 */

                PR_Lock(pt_tq.ml);
                pt_tq.head->result.code = -1;
                pt_tq.head->syserrno = ETIMEDOUT;
                (void)pt_FinishTimedInternal(pt_tq.head);
                PR_Unlock(pt_tq.ml);
                if ((NULL == pt_tq.head)
                || (PR_INTERVAL_NO_TIMEOUT == pt_tq.head->timeout))
                    break;
            }
        }
    }
    if (NULL != pollingList) PR_DELETE(pollingList);
}  /* ContinuationThread */

static PRIntn pt_Continue(pt_Continuation *op)
{
    PRIntn rc;
    PRStatus rv;
#ifdef DEBUG
    PRBool waitcv_interrupted = PR_FALSE;
#endif /* DEBUG */

    PR_CallOnce(&pt_tq.once, pt_InitIOContinuation);

    /* Finish filling in the blank slots */
    /* op->complete = PR_NewCondVar(pt_tq.ml); */
    op->complete.lock = pt_tq.ml;
    rc = PTHREAD_COND_INIT(op->complete.cv, _pt_cvar_attr);  PR_ASSERT(0 == rc);
    op->status = pt_continuation_sumbitted;
    PR_Lock(pt_tq.ml);  /* we provide the locking */

    pt_InsertTimedInternal(op);  /* insert in the structure */

    PR_NotifyCondVar(pt_tq.new_op);  /* notify the continuation thread */

    while (pt_continuation_done != op->status)  /* wait for completion */
    {
        rv = PR_WaitCondVar(&op->complete, PR_INTERVAL_NO_TIMEOUT);
        /*
         * If we get interrupted, we set state the continuation thread will
         * see and allow it to finish the I/O operation w/ error. That way
         * the rule that only the continuation thread is removing elements
         * from the list is still valid.
         *
         * Don't call interrupt on the continuation thread. That'll just
         * irritate him. He's cycling around at least every mx_poll_ticks
         * anyhow and should notice the request in there.
         */
        if ((PR_FAILURE == rv)
        && (PR_PENDING_INTERRUPT_ERROR == PR_GetError()))
        {
#ifdef DEBUG
            waitcv_interrupted = PR_TRUE;
#endif /* DEBUG */
            if (pt_continuation_done != op->status)
            {
                /* tell the continuation thread to abort the operation */
                op->status = pt_continuation_abort;
            }
            else
            {
                op->result.code = -1;
                op->syserrno = EINTR;
            }
        }
    }

    PR_Unlock(pt_tq.ml);  /* we provide the locking */
    rc = pthread_cond_destroy(&op->complete.cv); PR_ASSERT(0 == rc);

    /* make sure that the continuation thread did abort the operation */
#ifdef DEBUG
    if (PR_TRUE == waitcv_interrupted)
    {
        PR_ASSERT(-1 == op->result.code);
        PR_ASSERT(EINTR == op->syserrno);
    }
#endif /* DEBUG */
    return op->result.code;  /* and the primary answer */
}  /* pt_Continue */

/*****************************************************************************/
/*********************** specific continuation functions *********************/
/*****************************************************************************/
static PRBool pt_connect_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = _MD_unix_get_nonblocking_connect_error(op->arg1.osfd);
    if (op->syserrno != 0) {
        op->result.code = -1;
    } else {
        op->result.code = 0;
    }
    return PR_TRUE; /* this one is cooked */
}  /* pt_connect_cont */

static PRBool pt_accept_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = 0;
    op->result.code = accept(
        op->arg1.osfd, op->arg2.buffer, op->arg3.addr_len);
    if (-1 == op->result.code)
    {
        op->syserrno = errno;
        if (EWOULDBLOCK == errno || EAGAIN == errno)  /* the only thing we allow */
            return PR_FALSE;  /* do nothing - this one ain't finished */
    }
    return PR_TRUE;
}  /* pt_accept_cont */

static PRBool pt_read_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
    op->result.code = read(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_read_cont */

static PRBool pt_recv_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
    op->result.code = recv(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recv_cont */

static PRBool pt_send_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    PRIntn bytes = send(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
    op->syserrno = errno;
    if (bytes > 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else return ((-1 == bytes) &&
        (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ? 
        PR_FALSE : PR_TRUE;
}  /* pt_send_cont */

static PRBool pt_write_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    PRIntn bytes = write(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    if (bytes > 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else return ((-1 == bytes) &&
        (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ? 
        PR_FALSE : PR_TRUE;
}  /* pt_write_cont */

static PRBool pt_writev_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Same rules as write, but continuing seems to be a bit more
     * complicated. As the number of bytes sent grows, we have to
     * redefine the vector we're pointing at. We might have to
     * modify an individual vector parms or we might have to eliminate
     * a pair altogether.
     */
    PRIntn bytes = writev(
        op->arg1.osfd, (struct iovec*)op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    if (bytes > 0)  /* this is progress */
    {
        PRIntn iov_index;
        struct iovec *iov = (struct iovec*)op->arg2.buffer;
    op->result.code += bytes;  /* accumulate the number sent */
        for (iov_index = 0; iov_index < op->arg3.amount; ++iov_index)
        {
            /* how much progress did we make in the i/o vector? */
            if (bytes < iov[iov_index].iov_len)
            {
                /* this element's not done yet */
                char **bp = (char**)&(iov[iov_index].iov_base);
                iov[iov_index].iov_len -= bytes;  /* there's that much left */
                *bp += bytes;  /* starting there */
                break;  /* go off and do that */
            }
            bytes -= iov[iov_index].iov_len;  /* that element's consumed */
        }
        op->arg2.buffer = &iov[iov_index];  /* new start of array */
        op->arg3.amount -= iov_index;  /* and array length */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else return ((-1 == bytes) &&
        (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ? 
        PR_FALSE : PR_TRUE;
}  /* pt_writev_cont */

static PRBool pt_sendto_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes = sendto(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags,
        (struct sockaddr*)op->arg5.addr, PR_NETADDR_SIZE(op->arg5.addr));
    op->syserrno = errno;
    if (bytes > 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else return ((-1 == bytes) && 
        (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
    PR_FALSE : PR_TRUE;
}  /* pt_sendto_cont */

static PRBool pt_recvfrom_cont(pt_Continuation *op, PRInt16 revents)
{
    pt_SockLen addr_len = sizeof(PRNetAddr);
    op->result.code = recvfrom(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount,
        op->arg4.flags, (struct sockaddr*)op->arg5.addr, &addr_len);
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recvfrom_cont */

#ifdef HPUX11
static PRBool pt_hpux_transmitfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct iovec *hdtrl = (struct iovec *) op->arg2.buffer;
    int count;

    count = sendfile(op->arg1.osfd, op->filedesc, op->arg3.offset, 0,
            hdtrl, op->arg4.flags);
    PR_ASSERT(count <= op->nbytes_to_send);

    if (count != -1) {
        op->result.code += count;
    } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
        op->result.code = -1;
    } else {
        return PR_FALSE;
    }

    if (count != -1 && count < op->nbytes_to_send) {
        if (hdtrl[0].iov_len == 0) {
            PR_ASSERT(hdtrl[0].iov_base == NULL);
            op->arg3.offset += count;
        } else if (count < hdtrl[0].iov_len) {
            PR_ASSERT(op->arg3.offset == 0);
            hdtrl[0].iov_base = (char *) hdtrl[0].iov_base + count;
            hdtrl[0].iov_len -= count;
        } else {
            op->arg3.offset = count - hdtrl[0].iov_len;
            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;
        }
        op->nbytes_to_send -= count;
        return PR_FALSE;
    }

    return PR_TRUE;
}
#endif  /* HPUX11 */

#if !defined(PT_NO_ATFORK)

static void pt_BeforeFork()
{
    PRStatus rv;
    PRThread *thred = pt_tq.thread;

    /*
     * We shut down the continuation thread cleanly only if there are
     * no other threads in the process when fork() is called.
     * If there are other threads, they won't be duplicated in the child
     * process.  Then the child process may already be unclean, so there
     * is no point for us to try to be clean.
     */

    if ((NULL != thred) && (2 == pt_book.user + pt_book.system))
    {
        /* pt_tq should be empty */
        PR_ASSERT((0 == pt_tq.op_count)
            && (NULL == pt_tq.head)
            && (NULL == pt_tq.tail)
            && (NULL == pt_tq.op));
        pt_tq.exitFlag = PR_TRUE;
        rv = PR_Interrupt(thred);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(thred);
        PR_ASSERT(PR_SUCCESS == rv);
        pt_tq.exitFlag = PR_FALSE;
        /* Indicates that the continuation thread is shut down cleanly */
        pt_tq.thread = NULL;
        memset(&pt_tq.once, 0, sizeof(pt_tq.once));
        PR_ASSERT(1 == pt_book.user + pt_book.system);
    }
}  /* pt_BeforeFork */

static void pt_AfterForkParent(void)
{
}  /* pt_AfterForkParent */

static void pt_AfterForkChild(void)
{
    /*
     * pt_tq may be in a corrupted state if the continuation thread
     * existed and was not terminated cleanly before fork.  In this
     * case, we expect the child process to call exec immediately.
     */
}  /* pt_AfterForkChild */

#endif  /* PT_NO_ATFORK */

void _PR_InitIO()
{
    _pr_stdin = pt_SetMethods(0, PR_DESC_FILE);
    _pr_stdout = pt_SetMethods(1, PR_DESC_FILE);
    _pr_stderr = pt_SetMethods(2, PR_DESC_FILE);

    PR_ASSERT(_pr_stdin && _pr_stdout && _pr_stderr);

    _pr_flock_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_flock_lock);
    _pr_rename_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_rename_lock);

    pt_fd_cache.ml = PR_NewLock();
    PR_ASSERT(NULL != pt_fd_cache.ml);
    pt_fd_cache.limit = FD_SETSIZE;
}  /* _PR_InitIO */

static PRStatus pt_InitIOContinuation()
{
    PRIntn rv;

    PR_ASSERT((0 == pt_tq.op_count)
        && (NULL == pt_tq.head)
        && (NULL == pt_tq.tail)
        && (NULL == pt_tq.op)
        && (PR_FALSE == pt_tq.exitFlag));

    if (NULL == pt_tq.ml)
    {
        /* The very first time */
        pt_tq.ml = PR_NewLock();
        PR_ASSERT(NULL != pt_tq.ml);
        pt_tq.new_op = PR_NewCondVar(pt_tq.ml);
        PR_ASSERT(NULL != pt_tq.new_op);
        rv = PTHREAD_CONDATTR_INIT(&_pt_cvar_attr);
        PR_ASSERT(0 == rv);

#if !defined(PT_NO_ATFORK)
	rv = pthread_atfork(pt_BeforeFork,
            pt_AfterForkParent, pt_AfterForkChild);
        PR_ASSERT(0 == rv);
#endif
    }

    pt_tq.thread = PR_CreateThread(
        PR_SYSTEM_THREAD, ContinuationThread, NULL,
        PR_PRIORITY_URGENT, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    PR_ASSERT(NULL != pt_tq.thread);

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRFileDesc*) PR_GetSpecialFD(PRSpecialFD osfd)
{
    PRFileDesc *result = NULL;
    PR_ASSERT(osfd >= PR_StandardInput && osfd <= PR_StandardError);

    if (!_pr_initialized) _PR_ImplicitInitialization();
    
    switch (osfd)
    {
        case PR_StandardInput: result = _pr_stdin; break;
        case PR_StandardOutput: result = _pr_stdout; break;
        case PR_StandardError: result = _pr_stderr; break;
        default:
            (void)PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    }
    return result;
}  /* PR_GetSpecialFD */

/*****************************************************************************/
/***************************** I/O private methods ***************************/
/*****************************************************************************/

static PRBool pt_TestAbort(void)
{
    PRThread *me = PR_CurrentThread();
    if(me->state & PT_THREAD_ABORTED)
    {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        me->state &= ~PT_THREAD_ABORTED;
        return PR_TRUE;
    }
    return PR_FALSE;
}  /* pt_TestAbort */

static void pt_MapError(void (*mapper)(PRIntn), PRIntn syserrno)
{
    switch (syserrno)
    {
        case EINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0); break;
        case ETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0); break;
        default:
            mapper(syserrno);
    }
}  /* pt_MapError */

static PRStatus pt_Close(PRFileDesc *fd)
{
    PRIntn syserrno, rv = 0;
    if ((NULL == fd) || (NULL == fd->secret))
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if (pt_TestAbort()) return PR_FAILURE;

    if (_PR_FILEDESC_OPEN == fd->secret->state)
    {
        fd->secret->state = _PR_FILEDESC_CLOSED;
        rv = close(fd->secret->md.osfd);
        syserrno = errno;
    }

    pt_Putfd(fd);
    if (-1 == rv)
    {
        pt_MapError(_PR_MD_MAP_CLOSE_ERROR, syserrno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Close */

static PRInt32 pt_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    bytes = read(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.function = pt_read_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_READ_ERROR, syserrno);
    return bytes;
}  /* pt_Read */

static PRInt32 pt_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    if (pt_TestAbort()) return PR_FAILURE;

    bytes = write(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        buf = (char *) buf + bytes;
        amount -= bytes;
        fNeedContinue = PR_TRUE;
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        bytes = 0;
        fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_write_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_WRITE_ERROR, syserrno);
    return bytes;
}  /* pt_Write */

static PRInt32 pt_Writev(
    PRFileDesc *fd, PRIOVec *iov, PRInt32 iov_len, PRIntervalTime timeout)
{
    PRIntn iov_index = 0;
    PRBool fNeedContinue = PR_FALSE;
    PRInt32 syserrno, bytes = -1, rv = -1;

    if (pt_TestAbort()) return rv;

    /*
     * The first shot at this can use the client's iov directly.
     * Only if we have to continue the operation do we have to
     * make a copy that we can modify.
     */
    rv = bytes = writev(fd->secret->md.osfd, (struct iovec*)iov, iov_len);
    syserrno = errno;

    /*
     * If we moved some bytes (ie., bytes > 0) how does that implicate
     * the i/o vector list. In other words, exactly where are we within
     * that array? What are the parameters for resumption? Maybe we're
     * done!
     */
    if ((bytes > 0) && (!fd->secret->nonblocking))
    {
        for (iov_index = 0; iov_index < iov_len; ++iov_index)
        {
            if (bytes < iov[iov_index].iov_len) break; /* continue w/ what's left */
            bytes -= iov[iov_index].iov_len;  /* this one's done cooked */
        }
    }

    if ((bytes >= 0) && (iov_index < iov_len) && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout)
        {
            rv = -1;
            syserrno = ETIMEDOUT;
        }
        else fNeedContinue = PR_TRUE;
    }
    else if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            rv = bytes = 0;
            fNeedContinue = PR_TRUE;
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        /*
         * Okay. Now we need a modifiable copy of the array.
         * Allocate some storage and copy the (already) modified
         * bits into the new vector. The is copying only the
         * part of the array that's still valid. The array may
         * have a new length and the first element of the array may
         * have different values.
         */
        struct iovec *osiov = NULL, *tiov;
        PRIntn osiov_len = iov_len - iov_index;  /* recompute */
        osiov = (struct iovec*)PR_Malloc(osiov_len * sizeof(struct iovec));
        PR_ASSERT(NULL != osiov);
        for (tiov = osiov; iov_index < iov_len; ++iov_index)
        {
            tiov->iov_base = iov[iov_index].iov_base;
            tiov->iov_len = iov[iov_index].iov_len;
            tiov += 1;
        }
        osiov->iov_len -= bytes;  /* that may be partially done */
        /* so advance the description */
        osiov->iov_base = (char*)osiov->iov_base + bytes;

        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)osiov;
        op.arg3.amount = osiov_len;
        op.timeout = timeout;
        op.result.code = rv;
        op.function = pt_writev_cont;
        op.event = POLLOUT | POLLPRI;
        rv = pt_Continue(&op);
        syserrno = op.syserrno;
        PR_DELETE(osiov);
    }
    if (rv == -1) pt_MapError(_PR_MD_MAP_WRITEV_ERROR, syserrno);
    return rv;
}  /* pt_Writev */

static PRInt32 pt_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    PRIntn how;
    off_t pos = -1;
    
    if (pt_TestAbort()) return pos;

    switch (whence)
    {
        case PR_SEEK_SET: how = SEEK_SET; break;
        case PR_SEEK_CUR: how = SEEK_CUR; break;
        case PR_SEEK_END: how = SEEK_END; break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return pos;
    }
    pos = lseek(fd->secret->md.osfd, offset, how);
    if (pos == -1)
        pt_MapError(_PR_MD_MAP_LSEEK_ERROR, errno);
    return pos;
}  /* pt_Seek */

static PRInt64 pt_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    PRInt64 on;
    PRInt32 off, position = -1;
    LL_L2I(off, offset);  /* possible loss of bits */
    LL_I2L(on, off);  /* get back original or notice we didn't */
    if (LL_EQ(on, offset)) position = pt_Seek(fd, off, whence);
    LL_I2L(on, position);  /* might not have worked */
    return on;
}  /* pt_Seek64 */

static PRInt32 pt_Available(PRFileDesc *fd)
{
    PRInt32 rv, bytes = -1;
    if (pt_TestAbort()) return bytes;

    rv = ioctl(fd->secret->md.osfd, FIONREAD, &bytes);

    if (rv == -1)
        pt_MapError(_PR_MD_MAP_SOCKETAVAILABLE_ERROR, errno);
    return bytes;
}  /* pt_Available */

static PRInt64 pt_Available64(PRFileDesc *fd)
{
    PRInt64 rv;
    PRInt32 avail = pt_Available(fd);
    LL_I2L(rv, avail);
    return rv;
}  /* pt_Available64 */

static PRStatus pt_Synch(PRFileDesc *fd)
{
    return (NULL == fd) ? PR_FAILURE : PR_SUCCESS;
} /* pt_Synch */

static PRStatus pt_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PRInt32 rv;
    struct stat sb;
    PRInt64 s, s2us;
 
    if ((rv = fstat(fd->secret->md.osfd, &sb)) == 0 )
    {
        if (info)
        {
            if (S_IFREG & sb.st_mode)
                info->type = PR_FILE_FILE ;
            else if (S_IFDIR & sb.st_mode)
                info->type = PR_FILE_DIRECTORY;
            else
                info->type = PR_FILE_OTHER;
            info->size = sb.st_size;
#if defined(IRIX) && defined(HAVE_LONG_LONG)
            info->modifyTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_mtim.tv_sec);
            info->creationTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_ctim.tv_sec);
            info->modifyTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_mtime);
            info->creationTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_ctime);
#else
            LL_I2L(s, sb.st_mtime);
            LL_I2L(s2us, PR_USEC_PER_SEC);
            LL_MUL(s, s, s2us);
            info->modifyTime = s;
            LL_I2L(s, sb.st_ctime);
            LL_MUL(s, s, s2us);
            info->creationTime = s;
#endif
        }
    }
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* pt_FileInfo */

static PRStatus pt_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PRFileInfo info32;
    PRStatus rv = pt_FileInfo(fd, &info32);
    if (PR_SUCCESS == rv)
    {
        info->type = info32.type;
        info->creationTime = info32.creationTime;
        info->modifyTime = info32.modifyTime;
        LL_I2L(info->size, info32.size);
    }
    return rv;
}  /* pt_FileInfo64 */

static PRStatus pt_Fsync(PRFileDesc *fd)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = fsync(fd->secret->md.osfd);
    if (rv < 0) {
        pt_MapError(_PR_MD_MAP_FSYNC_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Fsync */

static PRStatus pt_Connect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PRIntn rv = -1, syserrno;
    PRSize addr_len = PR_NETADDR_SIZE(addr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = connect(fd->secret->md.osfd, (struct sockaddr*)addr, addr_len);
    syserrno = errno;
    if ((-1 == rv) && (EINPROGRESS == syserrno) && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
            op.arg2.buffer = (void*)addr;
            op.arg3.amount = addr_len;
            op.timeout = timeout;
            op.function = pt_connect_cont;
            op.event = POLLOUT | POLLPRI;
            rv = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (-1 == rv) {
        pt_MapError(_PR_MD_MAP_CONNECT_ERROR, syserrno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Connect */

PR_IMPLEMENT(PRStatus) PR_GetConnectStatus(const PRPollDesc *pd)
{
    PRInt32 osfd;
    PRFileDesc *bottom = pd->fd;
    int err;

    if (pd->out_flags & PR_POLL_NVAL) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((pd->out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR)) == 0) {
        PR_ASSERT(pd->out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
    }

    while (bottom->lower != NULL) {
        bottom = bottom->lower;
    }
    osfd = bottom->secret->md.osfd;

    err = _MD_unix_get_nonblocking_connect_error(osfd);
    if (err != 0) {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static PRFileDesc* pt_Accept(
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRFileDesc *newfd = NULL;
    PRIntn syserrno, osfd = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return newfd;

    osfd = accept(fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
    syserrno = errno;

    if (osfd == -1)
    {
        if (fd->secret->nonblocking) goto failed;

        if (EWOULDBLOCK != syserrno && EAGAIN != syserrno) goto failed;
        else
        {
            if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
            else
            {
                pt_Continuation op;
                op.arg1.osfd = fd->secret->md.osfd;
                op.arg2.buffer = addr;
                op.arg3.addr_len = &addr_len;
                op.timeout = timeout;
                op.function = pt_accept_cont;
                op.event = POLLIN | POLLPRI;
                osfd = pt_Continue(&op);
                syserrno = op.syserrno;
            }
            if (osfd < 0) goto failed;
        }
    }
#ifdef AIX
    /* mask off the first byte of struct sockaddr (the length field) */
    if (addr)
    addr->inet.family &= 0x00ff;
#endif
    newfd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP);
    if (newfd == NULL) close(osfd);  /* $$$ whoops! this doesn't work $$$ */
    else
    {
        PR_ASSERT((NULL == addr) || (PR_NETADDR_SIZE(addr) == addr_len));
#if defined(_PR_INET6)
        PR_ASSERT((NULL == addr) || (addr->raw.family == AF_INET)
                || (addr->raw.family == AF_INET6));
#else
        PR_ASSERT((NULL == addr) || (addr->raw.family == AF_INET));
#endif
    }
    return newfd;

failed:
    pt_MapError(_PR_MD_MAP_ACCEPT_ERROR, syserrno);
    return NULL;
}  /* pt_Accept */

static PRStatus pt_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    PRIntn rv;
    PRInt32 one = 1;

    if (pt_TestAbort()) return PR_FAILURE;

#if defined(_PR_INET6)
    PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
    PR_ASSERT(addr->raw.family == AF_INET);
#endif

    rv = setsockopt(
        fd->secret->md.osfd, SOL_SOCKET, SO_REUSEADDR,
        (_PRSockOptVal_t) &one, sizeof(one));
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SETSOCKOPT_ERROR, errno);
        return PR_FAILURE;
    }

    rv = bind(fd->secret->md.osfd, (struct sockaddr*)addr, PR_NETADDR_SIZE(addr));

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_BIND_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Bind */

static PRStatus pt_Listen(PRFileDesc *fd, PRIntn backlog)
{
    PRIntn rv;

    if (pt_TestAbort()) return PR_FAILURE;

    rv = listen(fd->secret->md.osfd, backlog);
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_LISTEN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Listen */

static PRStatus pt_Shutdown(PRFileDesc *fd, PRIntn how)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = shutdown(fd->secret->md.osfd, how);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SHUTDOWN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Shutdown */

static PRInt32 pt_Recv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    bytes = recv(fd->secret->md.osfd, buf, amount, flags);
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
            op.arg2.buffer = buf;
            op.arg3.amount = amount;
            op.arg4.flags = flags;
            op.timeout = timeout;
            op.function = pt_recv_cont;
            op.event = POLLIN | POLLPRI;
            bytes = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_RECV_ERROR, syserrno);
    return bytes;
}  /* pt_Recv */

static PRInt32 pt_Send(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    /*
     * Under HP-UX DCE threads, pthread.h includes dce/cma_ux.h,
     * which has the following:
     *     #  define send		cma_send
     *     extern int  cma_send (int , void *, int, int );
     * So we need to cast away the 'const' of argument #2 for send().
     */
#if defined (HPUX) && defined(_PR_DCETHREADS)
#define PT_SENDBUF_CAST (void *)
#else
#define PT_SENDBUF_CAST
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    bytes = send(fd->secret->md.osfd, PT_SENDBUF_CAST buf, amount, flags);
    syserrno = errno;

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout)
        {
            bytes = -1;
            syserrno = ETIMEDOUT;
        }
        else
        {
            buf = (char *) buf + bytes;
            amount -= bytes;
            fNeedContinue = PR_TRUE;
        }
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            bytes = 0;
            fNeedContinue = PR_TRUE;
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.timeout = timeout;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_send_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_SEND_ERROR, syserrno);
    return bytes;
}  /* pt_Send */

static PRInt32 pt_SendTo(
    PRFileDesc *fd, const void *buf,
    PRInt32 amount, PRIntn flags, const PRNetAddr *addr,
    PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    if (pt_TestAbort()) return PR_FAILURE;

#if defined(_PR_INET6)
    PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
    PR_ASSERT(addr->raw.family == AF_INET);
#endif
    bytes = sendto(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)addr, PR_NETADDR_SIZE(addr));
    syserrno = errno;
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else fNeedContinue = PR_TRUE;
    }
    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = (PRNetAddr*)addr;
        op.timeout = timeout;
        op.result.code = 0;  /* initialize the number sent */
        op.function = pt_sendto_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_SENDTO_ERROR, syserrno);
    return bytes;
}  /* pt_SendTo */

static PRInt32 pt_RecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRBool fNeedContinue = PR_FALSE;
    PRInt32 syserrno, bytes = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    bytes = recvfrom(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)addr, &addr_len);
    syserrno = errno;

    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = addr;
        op.timeout = timeout;
        op.function = pt_recvfrom_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
#ifdef AIX
    /* mask off the first byte of struct sockaddr (the length field) */
    if (addr) addr->inet.family &= 0x00ff;
#endif
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_RECVFROM_ERROR, syserrno);
    return bytes;
}  /* pt_RecvFrom */

#ifdef HPUX11
/*
 * pt_HPUXTransmitFile
 *
 *	Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *	bytes of headers is sent before sending the file.
 *
 *	PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *	
 *	return number of bytes sent or -1 on error
 *
 *      This implementation takes advantage of the sendfile() system
 *      call available in HP-UX B.11.00.
 */

static PRInt32 pt_HPUXTransmitFile(PRFileDesc *sd, PRFileDesc *fd, 
        const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
        PRIntervalTime timeout)
{
    struct stat statbuf;
    size_t nbytes_to_send;
    struct iovec hdtrl[2];  /* optional header and trailer buffers */
    int send_flags;
    PRInt32 count;
    int syserrno;

    /* Get file size */
    if (fstat(fd->secret->md.osfd, &statbuf) == -1) {
        _PR_MD_MAP_FSTAT_ERROR(errno);
        return -1;
    }
    nbytes_to_send = hlen + statbuf.st_size;

    hdtrl[0].iov_base = (void *) headers;  /* cast away the 'const' */
    hdtrl[0].iov_len = hlen;
    hdtrl[1].iov_base = NULL;
    hdtrl[1].iov_base = 0;
    /*
     * SF_DISCONNECT seems to close the socket even if sendfile()
     * only does a partial send on a nonblocking socket.  This
     * would prevent the subsequent sendfile() calls on that socket
     * from working.  So we don't use the SD_DISCONNECT flag.
     */
    send_flags = 0;

    do {
        count = sendfile(sd->secret->md.osfd, fd->secret->md.osfd,
                0, 0, hdtrl, send_flags);
        PR_ASSERT(count <= nbytes_to_send);
    } while (count == -1 && (syserrno = errno) == EINTR);

    if (count == -1 && (syserrno == EAGAIN || syserrno == EWOULDBLOCK)) {
        count = 0;
    }
    if (count != -1 && count < nbytes_to_send) {
        pt_Continuation op;

        if (count < hlen) {
            hdtrl[0].iov_base = ((char *) headers) + count;
            hdtrl[0].iov_len = hlen - count;
            op.arg3.offset = 0;
        } else {
            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;
            op.arg3.offset = count - hlen;
        }

        op.arg1.osfd = sd->secret->md.osfd;
        op.filedesc = fd->secret->md.osfd;
        op.arg2.buffer = hdtrl;
        op.arg4.flags = send_flags;
        op.nbytes_to_send = nbytes_to_send - count;
        op.result.code = count;
        op.timeout = timeout;
        op.function = pt_hpux_transmitfile_cont;
        op.event = POLLOUT | POLLPRI;
        count = pt_Continue(&op);
        syserrno = op.syserrno;
    }

    if (count == -1) {
        _MD_hpux_map_sendfile_error(syserrno);
	return -1;
    }
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        PR_Close(sd);
    }
    return count;
}
#endif  /* HPUX11 */

static PRInt32 pt_TransmitFile(
    PRFileDesc *sd, PRFileDesc *fd, const void *headers,
    PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (pt_TestAbort()) return -1;

#ifdef HPUX11
    return pt_HPUXTransmitFile(sd, fd, headers, hlen, flags, timeout);
#else
    return _PR_UnixTransmitFile(sd, fd, headers, hlen, flags, timeout);
#endif
}  /* pt_TransmitFile */

/*
 * XXX: When IPv6 is running, we need to see if this code works
 * with a PRNetAddr structure that supports both IPv4 and IPv6.
 */
static PRInt32 pt_AcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    PRInt32 rv = -1;
    PRNetAddr remote;
    PRFileDesc *accepted = NULL;
    PRIntervalTime start, elapsed;

    if (pt_TestAbort()) return rv;

    if (PR_INTERVAL_NO_TIMEOUT != timeout) start = PR_IntervalNow();
    if ((accepted = PR_Accept(sd, &remote, timeout)) == NULL) return rv;

    if (PR_INTERVAL_NO_TIMEOUT != timeout)
    {
        elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
        if (elapsed > timeout)
        {
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
            goto failed;
        }
        else timeout = timeout - elapsed;
    }

    rv = PR_Recv(accepted, buf, amount, 0, timeout);
    if (rv >= 0)
    {
        /* copy the new info out where caller can see it */
        *nd = accepted;
        *raddr = (PRNetAddr *)((char*)buf + amount);
        memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
        return rv;
    }

failed:
    PR_Close(accepted);
    return rv;
}  /* pt_AcceptRead */

static PRStatus pt_GetSockName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getsockname(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
#ifdef AIX
    /* mask off the first byte of struct sockaddr (the length field) */
    if (addr) addr->inet.family &= 0x00ff;
#endif
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETSOCKNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
        PR_ASSERT(addr_len == PR_NETADDR_SIZE(addr));
#if defined(_PR_INET6)
        PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif
        return PR_SUCCESS;
    }
}  /* pt_GetSockName */

static PRStatus pt_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getpeername(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);

#ifdef AIX
    /* mask off the first byte of struct sockaddr (the length field) */
    if (addr) addr->inet.family &= 0x00ff;
#endif
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETPEERNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
        PR_ASSERT(addr_len == PR_NETADDR_SIZE(addr));
#if defined(_PR_INET6)
        PR_ASSERT(addr->raw.family == AF_INET || addr->raw.family == AF_INET6);
#else
	PR_ASSERT(addr->raw.family == AF_INET);
#endif
        return PR_SUCCESS;
    }
}  /* pt_GetPeerName */

static PRStatus pt_GetSockOpt(
    PRFileDesc *fd, PRSockOption optname, void* optval, PRInt32* optlen)
{
    PRIntn rv = -1;
    PRInt32 level, name;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a getsockopt() call.
     */
    if (PR_SockOpt_Nonblocking == optname)
    {
        PR_ASSERT(sizeof(PRIntn) <= *optlen);
        *((PRIntn *) optval) = (PRIntn) fd->secret->nonblocking;
        *optlen = sizeof(PRIntn);
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(optname, &level, &name);
    if (0 == rv)
    {
        if (PR_SockOpt_Linger == optname)
        {
            struct linger linger;
            pt_SockLen len = sizeof(linger);
            rv = getsockopt(fd->secret->md.osfd, level, name,
                (char *) &linger, &len);
            if (0 == rv)
            {
                ((PRLinger*)(optval))->polarity = linger.l_onoff
                    ? PR_TRUE : PR_FALSE;
                ((PRLinger*)(optval))->linger = PR_SecondsToInterval(
                    linger.l_linger);
                *optlen = sizeof(PRLinger);
            }
        }
        else
        {
            /* Make sure the pointer type cast below is safe */
            PR_ASSERT(sizeof(PRInt32) == sizeof(PRIntn));
            rv = getsockopt(fd->secret->md.osfd, level, name,
                optval, (pt_SockLen*)optlen);
        }
    }

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETSOCKOPT_ERROR, errno);
        return PR_FAILURE;
    } else {
        return PR_SUCCESS;
    }
}  /* pt_GetSockOpt */

static PRStatus pt_SetSockOpt(
    PRFileDesc *fd, PRSockOption optname, const void* optval, PRInt32 optlen)
{
    PRIntn rv = -1;
    PRInt32 level, name;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a setsockopt() call.
     */
    if (PR_SockOpt_Nonblocking == optname)
    {
        PR_ASSERT(sizeof(PRIntn) == optlen);
        fd->secret->nonblocking = *((PRIntn *) optval) ? PR_TRUE : PR_FALSE;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(optname, &level, &name);
    if (0 == rv)
    {
        if (PR_SockOpt_Linger == optname)
        {
            struct linger linger;
            linger.l_onoff = ((PRLinger*)(optval))->polarity;
            linger.l_linger = PR_IntervalToSeconds(
                ((PRLinger*)(optval))->linger);
            rv = setsockopt(fd->secret->md.osfd, level, name,
                (char *) &linger, sizeof(linger));
        }
        else
        {
            rv = setsockopt(fd->secret->md.osfd, level, name,
                optval, optlen);
        }
    }

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SETSOCKOPT_ERROR, errno);
        return PR_FAILURE;
    } else {
        return PR_SUCCESS;
    }
}  /* pt_SetSockOpt */

static PRStatus pt_GetSocketOption(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRIntn rv;
    pt_SockLen length;
    PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a getsockopt() call
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        data->value.non_blocking = fd->secret->nonblocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                length = sizeof(linger);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char *) &linger, &length);
                PR_ASSERT((-1 == rv) || (sizeof(linger) == length));
                data->value.linger.polarity =
                    (linger.l_onoff) ? PR_TRUE : PR_FALSE;
                data->value.linger.linger =
                    PR_SecondsToInterval(linger.l_linger);
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.reuse_addr = (0 == value) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 boolean;
                length = sizeof(boolean);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&boolean, &length);
                PR_ASSERT((-1 == rv) || (sizeof(boolean) == length));
                data->value.mcast_loopback = (0 == boolean) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.recv_buffer_size = value;
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                length = sizeof(PRUintn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl;
                length = sizeof(ttl);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, &length);
                PR_ASSERT((-1 == rv) || (sizeof(ttl) == length));
                data->value.mcast_ttl = ttl;
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                length = sizeof(mreq);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&mreq, &length);
                PR_ASSERT((-1 == rv) || (sizeof(mreq) == length));
                data->value.add_member.mcaddr.inet.ip =
                    mreq.imr_multiaddr.s_addr;
                data->value.add_member.ifaddr.inet.ip =
                    mreq.imr_interface.s_addr;
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                length = sizeof(data->value.mcast_if.inet.ip);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip, &length);
                PR_ASSERT((-1 == rv)
                    || (sizeof(data->value.mcast_if.inet.ip) == length));
                break;
            }
            default:
                PR_NOT_REACHED("Unknown socket option");
                break;
        }
        if (-1 == rv) _PR_MD_MAP_GETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_GetSocketOption */

static PRStatus pt_SetSocketOption(PRFileDesc *fd, const PRSocketOptionData *data)
{
    PRIntn rv;
	PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a setsockopt call.
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        fd->secret->nonblocking = data->value.non_blocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                linger.l_onoff = data->value.linger.polarity;
                linger.l_linger = PR_IntervalToSeconds(data->value.linger.linger);
                rv = setsockopt(
                    fd->secret->md.osfd, level, name, (char*)&linger, sizeof(linger));
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            {
                PRIntn value = (data->value.reuse_addr) ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 boolean = data->value.mcast_loopback ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&boolean, sizeof(boolean));
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value = data->value.recv_buffer_size;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, sizeof(PRUintn));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl = data->value.mcast_ttl;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, sizeof(ttl));
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                mreq.imr_multiaddr.s_addr =
                    data->value.add_member.mcaddr.inet.ip;
                mreq.imr_interface.s_addr =
                    data->value.add_member.ifaddr.inet.ip;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&mreq, sizeof(mreq));
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip,
                    sizeof(data->value.mcast_if.inet.ip));
                break;
            }
            default:
                PR_NOT_REACHED("Unknown socket option");
                break;
        }
        if (-1 == rv) _PR_MD_MAP_SETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_SetSocketOption */

/*****************************************************************************/
/****************************** I/O method objects ***************************/
/*****************************************************************************/

static PRIOMethods _pr_file_methods = {
    PR_DESC_FILE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available,
    pt_Available64,
    pt_Fsync,
    pt_Seek,
    pt_Seek64,
    pt_FileInfo,
    pt_FileInfo64,
    (PRWritevFN)_PR_InvalidInt,		
    (PRConnectFN)_PR_InvalidStatus,		
    (PRAcceptFN)_PR_InvalidDesc,		
    (PRBindFN)_PR_InvalidStatus,		
    (PRListenFN)_PR_InvalidStatus,		
    (PRShutdownFN)_PR_InvalidStatus,	
    (PRRecvFN)_PR_InvalidInt,		
    (PRSendFN)_PR_InvalidInt,		
    (PRRecvfromFN)_PR_InvalidInt,	
    (PRSendtoFN)_PR_InvalidInt,		
    (PRPollFN)0,         
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,	
    (PRGetpeernameFN)_PR_InvalidStatus,	
    (PRGetsockoptFN)_PR_InvalidStatus,	
    (PRSetsockoptFN)_PR_InvalidStatus,	
};

static PRIOMethods _pr_tcp_methods = {
    PR_DESC_SOCKET_TCP,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available,
    pt_Available64,
    pt_Synch,
	(PRSeekFN)_PR_InvalidInt,
	(PRSeek64FN)_PR_InvalidInt64,
	(PRFileInfoFN)_PR_InvalidStatus,
	(PRFileInfo64FN)_PR_InvalidStatus,
    pt_Writev,
    pt_Connect,
    pt_Accept,
    pt_Bind,
    pt_Listen,
    pt_Shutdown,
    pt_Recv,
    pt_Send,
	(PRRecvfromFN)_PR_InvalidInt,
	(PRSendtoFN)_PR_InvalidInt,
	(PRPollFN)0,
    pt_AcceptRead,
    pt_TransmitFile,
    pt_GetSockName,
    pt_GetPeerName,
    pt_GetSockOpt,
    pt_SetSockOpt,
    pt_GetSocketOption,
    pt_SetSocketOption
};

static PRIOMethods _pr_udp_methods = {
    PR_DESC_SOCKET_UDP,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available,
    pt_Available64,
    pt_Synch,
	(PRSeekFN)_PR_InvalidInt,
	(PRSeek64FN)_PR_InvalidInt64,
	(PRFileInfoFN)_PR_InvalidStatus,
	(PRFileInfo64FN)_PR_InvalidStatus,
    pt_Writev,
    pt_Connect,
	(PRAcceptFN)_PR_InvalidDesc,
    pt_Bind,
    pt_Listen,
    pt_Shutdown,
    pt_Recv,
    pt_Send,
    pt_RecvFrom,
    pt_SendTo,
	(PRPollFN)0,
	(PRAcceptreadFN)_PR_InvalidInt,
	(PRTransmitfileFN)_PR_InvalidInt,
    pt_GetSockName,
    pt_GetPeerName,
    pt_GetSockOpt,
    pt_SetSockOpt,
    pt_GetSocketOption,
    pt_SetSocketOption
};

#if defined(_PR_FCNTL_FLAGS)
#undef _PR_FCNTL_FLAGS
#endif

#if defined(HPUX) || defined(OSF1) || defined(SOLARIS) || defined (IRIX) \
    || defined(AIX) || defined(LINUX) || defined(FREEBSD)
#define _PR_FCNTL_FLAGS O_NONBLOCK
#else
#error "Can't determine architecture"
#endif

static PRFileDesc *pt_SetMethods(PRIntn osfd, PRDescType type)
{
    PRInt32 flags, one = 1;
    PRFileDesc *fd = pt_Getfd();
    
    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->state = _PR_FILEDESC_OPEN;
        switch (type)
        {
            case PR_DESC_FILE:
                fd->methods = PR_GetFileMethods();
                break;
            case PR_DESC_SOCKET_TCP:
                fd->methods = PR_GetTCPMethods();
                flags = fcntl(osfd, F_GETFL, 0);
                flags |= _PR_FCNTL_FLAGS;
                (void)fcntl(osfd, F_SETFL, flags);
                (void)setsockopt(osfd, SOL_SOCKET, SO_KEEPALIVE,
                    (_PRSockOptVal_t) &one, sizeof(one));
                break;
            case PR_DESC_SOCKET_UDP:
                fd->methods = PR_GetUDPMethods();
                flags = fcntl(osfd, F_GETFL, 0);
                flags |= _PR_FCNTL_FLAGS;
                (void)fcntl(osfd, F_SETFL, flags);
                break;
            default:
                break;
        }
    }
    return fd;
}  /* pt_SetMethods */

PR_IMPLEMENT(PRIOMethods*) PR_GetFileMethods()
{
    return &_pr_file_methods;
}  /* PR_GetFileMethods */

PR_IMPLEMENT(PRIOMethods*) PR_GetTCPMethods()
{
    return &_pr_tcp_methods;
}  /* PR_GetTCPMethods */

PR_IMPLEMENT(PRIOMethods*) PR_GetUDPMethods()
{
    return &_pr_udp_methods;
}  /* PR_GetUDPMethods */

PR_IMPLEMENT(PRFileDesc*) PR_AllocFileDesc(PRInt32 osfd, PRIOMethods *methods)
{
    PRFileDesc *fd = pt_Getfd();

    /*
     * Assert that the file descriptor is small enough to fit in the
     * fd_set passed to select
     */
    PR_ASSERT(osfd < FD_SETSIZE);
    if (NULL == fd) goto failed;

    fd->methods = methods;
    fd->secret->md.osfd = osfd;
    /* Make fd non-blocking */
    if (osfd > 2)
    {
        /* Don't mess around with stdin, stdout or stderr */
        PRIntn flags;
        flags = fcntl(osfd, F_GETFL, 0);
        fcntl(osfd, F_SETFL, flags | _PR_FCNTL_FLAGS);
    }
    fd->secret->state = _PR_FILEDESC_OPEN;
    return fd;
    
failed:
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return fd;
}  /* PR_AllocFileDesc */

PR_IMPLEMENT(PRFileDesc*) PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
    PRIntn osfd;
    PRDescType ftype;
    PRFileDesc *fd = NULL;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (PF_INET != domain
#if defined(_PR_INET6)
	&& PF_INET6 != domain
#endif
	)
    {
        PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
        return fd;
    }
    if (type == SOCK_STREAM) ftype = PR_DESC_SOCKET_TCP;
    else if (type == SOCK_DGRAM) ftype = PR_DESC_SOCKET_UDP;
    else
    {
        (void)PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
        return fd;
    }

    osfd = socket(domain, type, proto);
    if (osfd == -1) pt_MapError(_PR_MD_MAP_SOCKET_ERROR, errno);
    else
    {
        fd = pt_SetMethods(osfd, ftype);
        if (fd == NULL) close(osfd);
    }
    return fd;
}  /* PR_Socket */

/*****************************************************************************/
/****************************** I/O public methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_Open(const char *name, PRIntn flags, PRIntn mode)
{
    PRFileDesc *fd = NULL;
    PRIntn syserrno, osfd = -1, osflags = 0;;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (flags & PR_RDONLY) osflags |= O_RDONLY;
    if (flags & PR_WRONLY) osflags |= O_WRONLY;
    if (flags & PR_RDWR) osflags |= O_RDWR;
    if (flags & PR_APPEND) osflags |= O_APPEND;
    if (flags & PR_TRUNCATE) osflags |= O_TRUNC;

    /*
    ** We have to hold the lock across the creation in order to
    ** enforce the sematics of PR_Rename(). (see the latter for
    ** more details)
    */
    if (flags & PR_CREATE_FILE)
    {
        osflags |= O_CREAT;
        if (NULL !=_pr_rename_lock)
            PR_Lock(_pr_rename_lock);
    }

    osfd = open(name, osflags, mode);
    syserrno = errno;

    if ((flags & PR_CREATE_FILE) && (NULL !=_pr_rename_lock))
        PR_Unlock(_pr_rename_lock);

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_OPEN_ERROR, syserrno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_FILE);
        if (fd == NULL) close(osfd);  /* $$$ whoops! this is bad $$$ */
    }
    return fd;
}  /* PR_Open */

PR_IMPLEMENT(PRStatus) PR_Delete(const char *name)
{
    PRIntn rv = -1;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return PR_FAILURE;

    rv = unlink(name);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_UNLINK_ERROR, errno);
        return PR_FAILURE;
    } else
        return PR_SUCCESS;
}  /* PR_Delete */

PR_IMPLEMENT(PRStatus) PR_Access(const char *name, PRAccessHow how)
{
    PRIntn rv;

    if (pt_TestAbort()) return PR_FAILURE;

    switch (how)
    {
    case PR_ACCESS_READ_OK:
        rv =  access(name, R_OK);
        break;
    case PR_ACCESS_WRITE_OK:
        rv = access(name, W_OK);
        break;
    case PR_ACCESS_EXISTS:
    default:
        rv = access(name, F_OK);
    }
    if (0 == rv) return PR_SUCCESS;
    pt_MapError(_PR_MD_MAP_ACCESS_ERROR, errno);
    return PR_FAILURE;
    
}  /* PR_Access */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo(const char *fn, PRFileInfo *info)
{
    PRInt32 rv;
    struct stat sb;
    PRInt64 s, s2us;

    if (pt_TestAbort()) return PR_FAILURE;

    if ((rv = stat(fn, &sb)) == 0 )
    {
        if (info)
        {
            if (S_IFREG & sb.st_mode)
                info->type = PR_FILE_FILE ;
            else if (S_IFDIR & sb.st_mode)
                info->type = PR_FILE_DIRECTORY;
            else
                info->type = PR_FILE_OTHER;
            info->size = sb.st_size;
#if defined(IRIX) && defined(HAVE_LONG_LONG)
            info->modifyTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_mtim.tv_sec);
            info->creationTime = (PR_USEC_PER_SEC * (PRInt64)sb.st_ctim.tv_sec);
#else
            LL_I2L(s, sb.st_mtime);
            LL_I2L(s2us, PR_USEC_PER_SEC);
            LL_MUL(s, s, s2us);
            info->modifyTime = s;
            LL_I2L(s, sb.st_ctime);
            LL_MUL(s, s, s2us);
            info->creationTime = s;
#endif
        }
    }
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo */

PR_IMPLEMENT(PRStatus) PR_Rename(const char *from, const char *to)
{
    PRIntn rv = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
    ** We have to acquire a lock here to stiffle anybody trying to create
    ** a new file at the same time. And we have to hold that lock while we
    ** test to see if the file exists and do the rename. The other place
    ** where the lock is held is in PR_Open() when possibly creating a 
    ** new file.
    */

    PR_Lock(_pr_rename_lock);
    rv = access(to, F_OK);
    if (0 == rv)
    {
        PR_SetError(PR_FILE_EXISTS_ERROR, 0);
        rv = -1;
    }
    else
    {
        rv = rename(from, to);
        if (rv == -1)
            pt_MapError(_PR_MD_MAP_RENAME_ERROR, errno);
    }
    PR_Unlock(_pr_rename_lock);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* PR_Rename */

PR_IMPLEMENT(PRStatus) PR_CloseDir(PRDir *dir)
{
    if (pt_TestAbort()) return PR_FAILURE;

    if (NULL != dir->md.d)
    {
        closedir(dir->md.d);
        dir->md.d = NULL;
    }
    return PR_SUCCESS;
}  /* PR_CloseDir */

PR_IMPLEMENT(PRStatus) PR_MkDir(const char *name, PRIntn mode)
{
    PRInt32 rv = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
    ** This lock is used to enforce rename semantics as described
    ** in PR_Rename.
    */
    if (NULL !=_pr_rename_lock)
        PR_Lock(_pr_rename_lock);
    rv = mkdir(name, mode);
    if (-1 == rv)
        pt_MapError(_PR_MD_MAP_MKDIR_ERROR, errno);
    if (NULL !=_pr_rename_lock)
        PR_Unlock(_pr_rename_lock);

    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* PR_Mkdir */

PR_IMPLEMENT(PRStatus) PR_RmDir(const char *name)
{
    PRInt32 rv;

    if (pt_TestAbort()) return PR_FAILURE;

    rv = rmdir(name);
    if (0 == rv) {
    return PR_SUCCESS;
    } else {
    pt_MapError(_PR_MD_MAP_RMDIR_ERROR, errno);
    return PR_FAILURE;
    }
}  /* PR_Rmdir */


PR_IMPLEMENT(PRDir*) PR_OpenDir(const char *name)
{
    DIR *osdir;
    PRDir *dir = NULL;

    if (pt_TestAbort()) return dir;

    osdir = opendir(name);
    if (osdir == NULL)
        pt_MapError(_PR_MD_MAP_OPENDIR_ERROR, errno);
    else
    {
        dir = PR_NEWZAP(PRDir);
        dir->md.d = osdir;
    }
    return dir;
}  /* PR_OpenDir */

PR_IMPLEMENT(PRInt32) PR_Poll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 ready = 0;
    /*
     * For restarting poll() if it is interrupted by a signal.
     * We use these variables to figure out how much time has
     * elapsed and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    if (0 == npds) PR_Sleep(timeout);
    else
    {
        PRIntn index, msecs;
        struct pollfd *syspoll = NULL;
        syspoll = (struct pollfd*)PR_Malloc(npds * sizeof(struct pollfd));
        for (index = 0; index < npds; ++index)
        {
            PRFileDesc *bottom = pds[index].fd;

            if (bottom == NULL)
            {
                /* make poll() ignore this entry */
                syspoll[index].fd = -1;
                continue;
            }

            while (bottom->lower != NULL) bottom = bottom->lower;
            syspoll[index].fd = bottom->secret->md.osfd;

            syspoll[index].events = 0;
            if (pds[index].in_flags & PR_POLL_READ)
                syspoll[index].events |= POLLIN;
            if (pds[index].in_flags & PR_POLL_WRITE)
                syspoll[index].events |= POLLOUT;
            if (pds[index].in_flags & PR_POLL_EXCEPT)
                syspoll[index].events |= POLLPRI;
            if (_PR_FILEDESC_OPEN == bottom->secret->state)
                pds[index].out_flags = 0;  /* init the result */
            else
            {
                ready += 1;  /* this will cause an abrupt return */
                pds[index].out_flags = POLLNVAL;  /* bogii */
            }
        }
        if (0 == ready)
        {
            switch (timeout)
            {
            case PR_INTERVAL_NO_WAIT: msecs = 0; break;
            case PR_INTERVAL_NO_TIMEOUT: msecs = -1; break;
            default:
                msecs = PR_IntervalToMilliseconds(timeout);
                start = PR_IntervalNow();
            }

retry:
            ready = poll(syspoll, npds, msecs);
            if (-1 == ready)
            {
                PRIntn oserror = errno;
                PRErrorCode prerror;

                switch (oserror) {
                    case EAGAIN:
                        prerror = PR_INSUFFICIENT_RESOURCES_ERROR;
                        break;
                    case EINTR:
                        if (timeout == PR_INTERVAL_NO_TIMEOUT)
                            goto retry;
                        else if (timeout == PR_INTERVAL_NO_WAIT)
                            ready = 0;  /* don't retry, just time out */
                        {
                            elapsed = (PRIntervalTime) (PR_IntervalNow()
                                    - start);
                            if (elapsed > timeout)
                                ready = 0;  /* timed out */
                            else
                            {
                                remaining = timeout - elapsed;
                                msecs = PR_IntervalToMilliseconds(remaining);
                                goto retry;
                            }
                        }
                        break;
                case EINVAL:
                        prerror = PR_INVALID_ARGUMENT_ERROR;
                        break;
                    case EFAULT:
                        prerror = PR_ACCESS_FAULT_ERROR;
                        break;
                    default:
                        prerror = PR_UNKNOWN_ERROR;
                        break;
                } 
                PR_SetError(prerror, oserror);
            }
            else if (ready > 0)
            {
                for (index = 0; index < npds; ++index)
                {
                    if (pds[index].fd == NULL) continue;
                    PR_ASSERT(0 == pds[index].out_flags);
                    if (0 != syspoll[index].revents)
                    {
                        if (syspoll[index].revents & POLLIN)
                            pds[index].out_flags |= PR_POLL_READ;
                        if (syspoll[index].revents & POLLOUT)
                            pds[index].out_flags |= PR_POLL_WRITE;
                        if (syspoll[index].revents & POLLPRI)
                            pds[index].out_flags |= PR_POLL_EXCEPT;
                        if (syspoll[index].revents & POLLERR)
                            pds[index].out_flags |= PR_POLL_ERR;
                        if (syspoll[index].revents & POLLNVAL)
                            pds[index].out_flags |= PR_POLL_NVAL;
                    }
                }
            }
        }

        PR_DELETE(syspoll);

    }
    return ready;

} /* PR_Poll */

PR_IMPLEMENT(PRDirEntry*) PR_ReadDir(PRDir *dir, PRDirFlags flags)
{
    struct dirent *dp;

    if (pt_TestAbort()) return NULL;

    for (;;)
    {
        dp = readdir(dir->md.d);
        if (NULL == dp) return NULL;
        if ((flags & PR_SKIP_DOT)
            && ('.' == dp->d_name[0])
            && (0 == dp->d_name[1])) continue;
        if ((flags & PR_SKIP_DOT_DOT)
            && ('.' == dp->d_name[0])
            && ('.' == dp->d_name[1])
            && (0 == dp->d_name[2])) continue;
        if ((flags & PR_SKIP_HIDDEN) && ('.' == dp->d_name[0]))
			continue;
        break;
    }
    dir->d.name = dp->d_name;
    return &dir->d;
}  /* PR_ReadDir */

PR_IMPLEMENT(PRFileDesc*) PR_NewUDPSocket()
{
    PRFileDesc *fd = NULL;
    PRIntn osfd = -1, syserrno;
    PRIntn domain = PF_INET;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

#if defined(_PR_INET6)
    if (_pr_ipv6_enabled)
        domain = PF_INET6;
#endif
    osfd = socket(domain, SOCK_DGRAM, 0);
    syserrno = errno;

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_SOCKET_ERROR, syserrno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_SOCKET_UDP);
        if (fd == NULL) close(osfd);
    }
    return fd;
}  /* PR_NewUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_NewTCPSocket()
{
    PRIntn osfd = -1;
    PRFileDesc *fd = NULL;
    PRIntn domain = PF_INET;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

#if defined(_PR_INET6)
    if (_pr_ipv6_enabled)
        domain = PF_INET6;
#endif
    osfd = socket(domain, SOCK_STREAM, 0);

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_SOCKET_ERROR, errno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP);
        if (fd == NULL) close(osfd);
    }
    return fd;
}  /* PR_NewTCPSocket */

PR_IMPLEMENT(PRStatus) PR_NewTCPSocketPair(PRFileDesc *fds[2])
{
    PRInt32 osfd[2];

    if (pt_TestAbort()) return PR_FAILURE;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, osfd) == -1) {
    pt_MapError(_PR_MD_MAP_SOCKETPAIR_ERROR, errno);
    return PR_FAILURE;
    }

    fds[0] = pt_SetMethods(osfd[0], PR_DESC_SOCKET_TCP);
    if (fds[0] == NULL) {
        close(osfd[0]);
        close(osfd[1]);
        return PR_FAILURE;
    }
    fds[1] = pt_SetMethods(osfd[1], PR_DESC_SOCKET_TCP);
    if (fds[1] == NULL) {
        PR_Close(fds[0]);
        close(osfd[1]);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* PR_NewTCPSocketPair */

PR_IMPLEMENT(PRStatus) PR_CreatePipe(
    PRFileDesc **readPipe,
    PRFileDesc **writePipe
)
{
    int pipefd[2];
    int flags;

    if (pt_TestAbort()) return PR_FAILURE;

    if (pipe(pipefd) == -1)
    {
	/* XXX map pipe error */
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return PR_FAILURE;
    }
    *readPipe = pt_SetMethods(pipefd[0], PR_DESC_FILE);
    if (NULL == *readPipe)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    flags = fcntl(pipefd[0], F_GETFL, 0);
    flags |= _PR_FCNTL_FLAGS;
    (void)fcntl(pipefd[0], F_SETFL, flags);
    *writePipe = pt_SetMethods(pipefd[1], PR_DESC_FILE);
    if (NULL == *writePipe)
    {
        PR_Close(*readPipe);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    flags = fcntl(pipefd[1], F_GETFL, 0);
    flags |= _PR_FCNTL_FLAGS;
    (void)fcntl(pipefd[1], F_SETFL, flags);
    return PR_SUCCESS;
}

/*****************************************************************************/
/***************************** I/O friends methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_ImportFile(PRInt32 osfd)
{
    PRFileDesc *fd = pt_SetMethods(osfd, PR_DESC_FILE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportFile */

PR_IMPLEMENT(PRFileDesc*) PR_ImportTCPSocket(PRInt32 osfd)
{
    PRFileDesc *fd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportTCPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_ImportUDPSocket(PRInt32 osfd)
{
    PRFileDesc *fd = pt_SetMethods(osfd, PR_DESC_SOCKET_UDP);
    if (NULL != fd) close(osfd);
    return fd;
}  /* PR_ImportUDPSocket */

PR_IMPLEMENT(PRInt32) PR_FileDesc2NativeHandle(PRFileDesc *fd)
{
    if (fd)
    {
        /*
         * The fd may be layered.  Chase the links to the
         * bottom layer to get the osfd.
         */
        PRFileDesc *bottom = fd;
        while (bottom->lower != NULL) {
            bottom = bottom->lower;
        }
        return bottom->secret->md.osfd;
    }
    else
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
}  /* PR_FileDesc2NativeHandle */

PR_IMPLEMENT(void) PR_ChangeFileDescNativeHandle(PRFileDesc *fd,
    PRInt32 handle)
{
    if (fd) fd->secret->md.osfd = handle;
}  /*  PR_ChangeFileDescNativeHandle*/

PR_IMPLEMENT(PRStatus) PR_LockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    if (0 == fd->secret->lockCount)
    {
        status = _PR_MD_LOCKFILE(fd->secret->md.osfd);
        if (PR_SUCCESS == status) fd->secret->lockCount = 1;
    }
    else fd->secret->lockCount += 1;
    PR_Unlock(_pr_flock_lock);
 
    return status;
}  /* PR_LockFile */

PR_IMPLEMENT(PRStatus) PR_TLockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    if (0 == fd->secret->lockCount)
    {
        status = _PR_MD_TLOCKFILE(fd->secret->md.osfd);
        if (PR_SUCCESS == status) fd->secret->lockCount = 1;
    }
    else fd->secret->lockCount += 1;
    PR_Unlock(_pr_flock_lock);
 
    return status;
}  /* PR_TLockFile */

PR_IMPLEMENT(PRStatus) PR_UnlockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    if (fd->secret->lockCount == 1)
    {
        status = _PR_MD_UNLOCKFILE(fd->secret->md.osfd);
        if (PR_SUCCESS == status) fd->secret->lockCount = 0;
    }
    else fd->secret->lockCount -= 1;
    PR_Unlock(_pr_flock_lock);

    return status;
}

/*
 * The next two entry points should not be in the API, but they are
 * defined here for historical (or hysterical) reasons.
 */

PRInt32 PR_GetSysfdTableMax(void)
{
#if defined(XP_UNIX) && !defined(AIX)
    struct rlimit rlim;

    if ( getrlimit(RLIMIT_NOFILE, &rlim) < 0) 
       return -1;

    return rlim.rlim_max;
#elif defined(AIX)
    return sysconf(_SC_OPEN_MAX);
#endif
}

PRInt32 PR_SetSysfdTableSize(PRIntn table_size)
{
#if defined(XP_UNIX) && !defined(AIX)
    struct rlimit rlim;
    PRInt32 tableMax = PR_GetSysfdTableMax();

    if (tableMax < 0) return -1;
    rlim.rlim_max = tableMax;

    /* Grow as much as we can; even if too big */
    if ( rlim.rlim_max < table_size )
        rlim.rlim_cur = rlim.rlim_max;
    else
        rlim.rlim_cur = table_size;

    if ( setrlimit(RLIMIT_NOFILE, &rlim) < 0) 
        return -1;

    return rlim.rlim_cur;
#elif defined(AIX)
    return -1;
#endif
}

/*
 * PR_Stat is supported for backward compatibility; some existing Java
 * code uses it.  New code should use PR_GetFileInfo.
 */

#ifndef NO_NSPR_10_SUPPORT
PR_IMPLEMENT(PRInt32) PR_Stat(const char *name, struct stat *buf)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_Stat", "PR_GetFileInfo");

    if (pt_TestAbort()) return PR_FAILURE;

    if (-1 == stat(name, buf)) {
        pt_MapError(_PR_MD_MAP_STAT_ERROR, errno);
        return PR_FAILURE;
    } else {
        return PR_SUCCESS;
    }
}
#endif /* ! NO_NSPR_10_SUPPORT */


PR_IMPLEMENT(void) PR_FD_ZERO(PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_ZERO (PR_Select)", "PR_Poll");
    memset(set, 0, sizeof(PR_fd_set));
}

PR_IMPLEMENT(void) PR_FD_SET(PRFileDesc *fh, PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_SET (PR_Select)", "PR_Poll");
    PR_ASSERT( set->hsize < PR_MAX_SELECT_DESC );

    set->harray[set->hsize++] = fh;
}

PR_IMPLEMENT(void) PR_FD_CLR(PRFileDesc *fh, PR_fd_set *set)
{
    PRUint32 index, index2;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_CLR (PR_Select)", "PR_Poll");

    for (index = 0; index<set->hsize; index++)
       if (set->harray[index] == fh) {
           for (index2=index; index2 < (set->hsize-1); index2++) {
               set->harray[index2] = set->harray[index2+1];
           }
           set->hsize--;
           break;
       }
}

PR_IMPLEMENT(PRInt32) PR_FD_ISSET(PRFileDesc *fh, PR_fd_set *set)
{
    PRUint32 index;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_ISSET (PR_Select)", "PR_Poll");
    for (index = 0; index<set->hsize; index++)
       if (set->harray[index] == fh) {
           return 1;
       }
    return 0;
}

PR_IMPLEMENT(void) PR_FD_NSET(PRInt32 fd, PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NSET (PR_Select)", "PR_Poll");
    PR_ASSERT( set->nsize < PR_MAX_SELECT_DESC );

    set->narray[set->nsize++] = fd;
}

PR_IMPLEMENT(void) PR_FD_NCLR(PRInt32 fd, PR_fd_set *set)
{
    PRUint32 index, index2;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NCLR (PR_Select)", "PR_Poll");

    for (index = 0; index<set->nsize; index++)
       if (set->narray[index] == fd) {
           for (index2=index; index2 < (set->nsize-1); index2++) {
               set->narray[index2] = set->narray[index2+1];
           }
           set->nsize--;
           break;
       }
}

PR_IMPLEMENT(PRInt32) PR_FD_NISSET(PRInt32 fd, PR_fd_set *set)
{
    PRUint32 index;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NISSET (PR_Select)", "PR_Poll");
    for (index = 0; index<set->nsize; index++)
       if (set->narray[index] == fd) {
           return 1;
       }
    return 0;
}

#include <sys/types.h>
#include <sys/time.h>
#if !defined(SUNOS4) && !defined(HPUX) && !defined(LINUX)
#include <sys/select.h>
#endif

static PRInt32
_PR_getset(PR_fd_set *pr_set, fd_set *set)
{
    PRUint32 index;
    PRInt32 max = 0;

    if (!pr_set)
        return 0;
   
    FD_ZERO(set);

    /* First set the pr file handle osfds */
    for (index=0; index<pr_set->hsize; index++) {
        FD_SET(pr_set->harray[index]->secret->md.osfd, set);
        if (pr_set->harray[index]->secret->md.osfd > max)
            max = pr_set->harray[index]->secret->md.osfd;
    }
    /* Second set the native osfds */
    for (index=0; index<pr_set->nsize; index++) {
        FD_SET(pr_set->narray[index], set);
        if (pr_set->narray[index] > max)
            max = pr_set->narray[index];
    }
    return max;
}

static void
_PR_setset(PR_fd_set *pr_set, fd_set *set)
{
    PRUint32 index, last_used;

    if (!pr_set)
        return;

    for (last_used=0, index=0; index<pr_set->hsize; index++) {
        if ( FD_ISSET(pr_set->harray[index]->secret->md.osfd, set) ) {
            pr_set->harray[last_used++] = pr_set->harray[index];
        }
    }
    pr_set->hsize = last_used;

    for (last_used=0, index=0; index<pr_set->nsize; index++) {
        if ( FD_ISSET(pr_set->narray[index], set) ) {
            pr_set->narray[last_used++] = pr_set->narray[index];
        }
    }
    pr_set->nsize = last_used;
}

PR_IMPLEMENT(PRInt32) PR_Select(
    PRInt32 unused, PR_fd_set *pr_rd, PR_fd_set *pr_wr, 
    PR_fd_set *pr_ex, PRIntervalTime timeout)
{
    fd_set rd, wr, ex;
    struct timeval tv, *tvp;
    PRInt32 max, max_fd;
    PRInt32 rv;
    /*
     * For restarting select() if it is interrupted by a Unix signal.
     * We use these variables to figure out how much time has elapsed
     * and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete( "PR_Select", "PR_Poll");

    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);

    max_fd = _PR_getset(pr_rd, &rd);
    max_fd = (max = _PR_getset(pr_wr, &wr))>max_fd?max:max_fd;
    max_fd = (max = _PR_getset(pr_ex, &ex))>max_fd?max:max_fd;

    if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        tvp = NULL;
    } else {
        tv.tv_sec = (PRInt32)PR_IntervalToSeconds(timeout);
        tv.tv_usec = (PRInt32)PR_IntervalToMicroseconds(
                timeout - PR_SecondsToInterval(tv.tv_sec));
        tvp = &tv;
        start = PR_IntervalNow();
    }

retry:
    rv = select(max_fd + 1, (_PRSelectFdSetArg_t) &rd,
        (_PRSelectFdSetArg_t) &wr, (_PRSelectFdSetArg_t) &ex, tvp);

    if (rv == -1 && errno == EINTR) {
        if (timeout == PR_INTERVAL_NO_TIMEOUT) {
            goto retry;
        } else {
            elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
            if (elapsed > timeout) {
                rv = 0;  /* timed out */
            } else {
                remaining = timeout - elapsed;
                tv.tv_sec = (PRInt32)PR_IntervalToSeconds(remaining);
                tv.tv_usec = (PRInt32)PR_IntervalToMicroseconds(
                        remaining - PR_SecondsToInterval(tv.tv_sec));
                goto retry;
            }
        }
    }

    if (rv > 0) {
        _PR_setset(pr_rd, &rd);
        _PR_setset(pr_wr, &wr);
        _PR_setset(pr_ex, &ex);
    } else if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SELECT_ERROR, errno);
    }
    return rv;
}

#endif /* defined(_PR_PTHREADS) */

/* ptio.c */
