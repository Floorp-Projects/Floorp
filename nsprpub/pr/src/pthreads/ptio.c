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

#include <pthread.h>
#include <string.h>  /* for memset() */
#include <sys/types.h>
#include <dirent.h>
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
#ifdef _PR_POLL_AVAILABLE
#include <poll.h>
#endif
#ifdef AIX
/* To pick up sysconf() */
#include <unistd.h>
#include <dlfcn.h>  /* for dlopen */
#else
/* To pick up getrlimit() etc. */
#include <sys/time.h>
#include <sys/resource.h>
#endif

/*
 * The send_file() system call is available in AIX 4.3.2 or later.
 * If this file is compiled on an older AIX system, it attempts to
 * look up the send_file symbol at run time to determine whether
 * we can use the faster PR_TransmitFile implementation based on
 * send_file().  On AIX 4.3.2 or later, we can safely skip this
 * runtime function dispatching and just use the send_file based
 * implementation.
 */
#ifdef AIX
#ifdef SF_CLOSE
#define HAVE_SEND_FILE
#endif

#ifdef HAVE_SEND_FILE

#define AIX_SEND_FILE(a, b, c) send_file(a, b, c)

#else /* HAVE_SEND_FILE */

/*
 * The following definitions match those in <sys/socket.h>
 * on AIX 4.3.2.
 */

/*
 * Structure for the send_file() system call
 */
struct sf_parms {
    /* --------- header parms ---------- */
    void      *header_data;         /* Input/Output. Points to header buf */
    uint_t    header_length;        /* Input/Output. Length of the header */
    /* --------- file parms ------------ */
    int       file_descriptor;      /* Input. File descriptor of the file */
    unsigned long long file_size;   /* Output. Size of the file */
    unsigned long long file_offset; /* Input/Output. Starting offset */
    long long file_bytes;           /* Input/Output. no. of bytes to send */
    /* --------- trailer parms --------- */
    void      *trailer_data;        /* Input/Output. Points to trailer buf */
    uint_t    trailer_length;       /* Input/Output. Length of the trailer */
    /* --------- return info ----------- */
    unsigned long long bytes_sent;  /* Output. no. of bytes sent */
};

/*
 * Flags for the send_file() system call
 */
#define SF_CLOSE        0x00000001      /* close the socket after completion */
#define SF_REUSE        0x00000002      /* reuse socket. not supported */
#define SF_DONT_CACHE   0x00000004      /* don't apply network buffer cache */
#define SF_SYNC_CACHE   0x00000008      /* sync/update network buffer cache */

/*
 * prototype: size_t send_file(int *, struct sf_parms *, uint_t);
 */
static ssize_t (*pt_aix_sendfile_fptr)() = NULL;

#define AIX_SEND_FILE(a, b, c) (*pt_aix_sendfile_fptr)(a, b, c)

#endif /* HAVE_SEND_FILE */
#endif /* AIX */

#include "primpl.h"

/* On Alpha Linux, these are already defined in sys/socket.h */
#if !(defined(LINUX) && defined(__alpha))
#include <netinet/tcp.h>  /* TCP_NODELAY, TCP_MAXSEG */
#endif

#if defined(SOLARIS)
#define _PRSockOptVal_t char *
#elif defined(IRIX) || defined(OSF1) || defined(AIX) || defined(HPUX) \
    || defined(LINUX) || defined(FREEBSD) || defined(BSDI)
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
    || defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
    || defined(BSDI)
#define _PRSelectFdSetArg_t fd_set *
#else
#error "Cannot determine architecture"
#endif

static PRFileDesc *pt_SetMethods(PRIntn osfd, PRDescType type);

static PRLock *_pr_flock_lock;  /* For PR_LockFile() etc. */
static PRLock *_pr_rename_lock;  /* For PR_Rename() */

/**************************************************************************/

/* These two functions are only used in assertions. */
#if defined(DEBUG)

static PRBool IsValidNetAddr(const PRNetAddr *addr)
{
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
#ifdef _PR_INET6
            && (addr->raw.family != AF_INET6)
#endif
            && (addr->raw.family != AF_INET)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

static PRBool IsValidNetAddrLen(const PRNetAddr *addr, PRInt32 addr_len)
{
    /*
     * The definition of the length of a Unix domain socket address
     * is not uniform, so we don't check it.
     */
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
            && (PR_NETADDR_SIZE(addr) != addr_len)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

#endif /* DEBUG */

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
#if defined(HAVE_SOCKLEN_T) \
    || (defined(LINUX) && defined(__GLIBC__) && __GLIBC__ >= 2 \
    && !defined(__alpha))
typedef socklen_t pt_SockLen;
#elif (defined(AIX) && !defined(AIX4_1)) \
    || (defined(LINUX) && defined(__alpha))
typedef PRSize pt_SockLen;
#else
typedef PRIntn pt_SockLen;
#endif

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revents);

typedef enum pr_ContuationStatus
{
    pt_continuation_pending,
    pt_continuation_recycle,
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
    PRCondVar *complete;                    /* to notify the initiating thread */
};

static struct pt_TimedQueue
{
    PRLock *ml;                             /* a little protection */
    PRThread *thread;                       /* internal thread's identification */
    PRUintn op_count;                       /* number of operations in the list */
    pt_Continuation *head, *tail;           /* head/tail of list of operations */

    pt_Continuation *op;                    /* timed operation furthest in future */
} pt_tq;

#if defined(DEBUG)

PTDebug pt_debug;  /* this is shared between several modules */

PR_IMPLEMENT(void) PT_GetStats(PTDebug* here) { *here = pt_debug; }

PR_IMPLEMENT(void) PT_FPrintStats(PRFileDesc *debug_out, const char *msg)
{
    PTDebug stats;
    char buffer[100];
    PRExplodedTime tod;
    PRInt64 elapsed, aMil;
    PT_GetStats(&stats);  /* a copy */
    PR_ExplodeTime(stats.timeStarted, PR_LocalTimeParameters, &tod);
    (void)PR_FormatTime(buffer, sizeof(buffer), "%T", &tod);

    LL_SUB(elapsed, PR_Now(), stats.timeStarted);
    LL_I2L(aMil, 1000000);
    LL_DIV(elapsed, elapsed, aMil);
    
    if (NULL != msg) PR_fprintf(debug_out, "%s", msg);
    PR_fprintf(
        debug_out, "\tstarted: %s[%lld]\n", buffer, elapsed);
    PR_fprintf(
        debug_out, "\tmissed predictions: %u\n", stats.predictionsFoiled);
    PR_fprintf(
        debug_out, "\tpollingList max: %u\n", stats.pollingListMax);
    PR_fprintf(
        debug_out, "\tcontinuations served: %u\n", stats.continuationsServed);
    PR_fprintf(
        debug_out, "\trecycles needed: %u\n", stats.recyclesNeeded);
    PR_fprintf(
        debug_out, "\tquiescent IO: %u\n", stats.quiescentIO);
    PR_fprintf(
        debug_out, "\tlocks [created: %u, destroyed: %u]\n",
        stats.locks_created, stats.locks_destroyed);
    PR_fprintf(
        debug_out, "\tlocks [acquired: %u, released: %u]\n",
        stats.locks_acquired, stats.locks_released);
    PR_fprintf(
        debug_out, "\tcvars [created: %u, destroyed: %u]\n",
        stats.cvars_created, stats.cvars_destroyed);
    PR_fprintf(
        debug_out, "\tcvars [notified: %u, delayed_delete: %u]\n",
        stats.cvars_notified, stats.delayed_cv_deletes);
}  /* PT_FPrintStats */

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

#if defined(DEBUG)
    {
        PRIntn count;
        pt_Continuation *tmp;
        PR_ASSERT((pt_tq.head == NULL) == (pt_tq.tail == NULL));
        PR_ASSERT((pt_tq.head == NULL) == (pt_tq.op_count == 0));
        for (tmp = pt_tq.head, count = 0; tmp != NULL; tmp = tmp->next) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
        for (tmp = pt_tq.tail, count = 0; tmp != NULL; tmp = tmp->prev) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
    }
#endif /* defined(DEBUG) */

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
     * (we do that in pt_ContinuationThreadInternal).  We traverse the 'prev'
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

#if defined(DEBUG)
    {
        PRIntn count;
        pt_Continuation *tmp;
        PR_ASSERT(pt_tq.head != NULL);
        PR_ASSERT(pt_tq.tail != NULL);
        PR_ASSERT(pt_tq.op_count != 0);
        PR_ASSERT(pt_tq.head->prev == NULL);
        PR_ASSERT(pt_tq.tail->next == NULL);
        if (pt_tq.op_count > 1)
        {
            PR_ASSERT(pt_tq.head->next != NULL);
            PR_ASSERT(pt_tq.tail->prev != NULL);
        }
        else
        {
            PR_ASSERT(pt_tq.head->next == NULL);
            PR_ASSERT(pt_tq.tail->prev == NULL);
        }
        for (tmp = pt_tq.head, count = 0; tmp != NULL; tmp = tmp->next) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
        for (tmp = pt_tq.tail, count = 0; tmp != NULL; tmp = tmp->prev) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
    }
#endif /* defined(DEBUG) */

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

#if defined(DEBUG)
    {
        PRIntn count;
        pt_Continuation *tmp;
        PR_ASSERT(pt_tq.head != NULL);
        PR_ASSERT(pt_tq.tail != NULL);
        PR_ASSERT(pt_tq.op_count != 0);
        PR_ASSERT(pt_tq.head->prev == NULL);
        PR_ASSERT(pt_tq.tail->next == NULL);
        if (pt_tq.op_count > 1)
        {
            PR_ASSERT(pt_tq.head->next != NULL);
            PR_ASSERT(pt_tq.tail->prev != NULL);
        }
        else
        {
            PR_ASSERT(pt_tq.head->next == NULL);
            PR_ASSERT(pt_tq.tail->prev == NULL);
        }
        for (tmp = pt_tq.head, count = 0; tmp != NULL; tmp = tmp->next) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
        for (tmp = pt_tq.tail, count = 0; tmp != NULL; tmp = tmp->prev) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
    }
#endif /* defined(DEBUG) */

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
    PR_NotifyCondVar(op->complete);

#if defined(DEBUG)
    {
        PRIntn count;
        pt_Continuation *tmp;
        PR_ASSERT((pt_tq.head == NULL) == (pt_tq.tail == NULL));
        PR_ASSERT((pt_tq.head == NULL) == (pt_tq.op_count == 0));
        for (tmp = pt_tq.head, count = 0; tmp != NULL; tmp = tmp->next) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
        for (tmp = pt_tq.tail, count = 0; tmp != NULL; tmp = tmp->prev) count += 1;
        PR_ASSERT(count == pt_tq.op_count);
    }
#endif /* defined(DEBUG) */

    return next;
}  /* pt_FinishTimedInternal */

static void pt_ContinuationThreadInternal(pt_Continuation *my_op)
{
    /* initialization */
    PRInt32 msecs, mx_poll_ticks;
    PRThreadPriority priority;              /* used to save caller's prio */
    PRIntn pollingListUsed;                 /* # entries used in the list */
    PRIntn pollingListNeeded;               /* # entries needed this time */
    static struct pollfd *pollingList = 0;  /* list built for polling */
    static PRIntn pollingSlotsAllocated = 0;/* # entries available in list */
    static pt_Continuation **pollingOps = 0;/* list paralleling polling list */
    
    PR_Unlock(pt_tq.ml);  /* don't need that silly lock for a bit */

    priority = PR_GetThreadPriority(pt_tq.thread);
    PR_SetThreadPriority(pt_tq.thread, PR_PRIORITY_HIGH);

    mx_poll_ticks = (PRInt32)PR_MillisecondsToInterval(PT_DEFAULT_POLL_MSEC);

    /* do some real work */
    while (PR_TRUE)
    {
        PRIntn rv;
        PRInt32 timeout;
        PRIntn pollIndex;
        PRIntervalTime now;
        pt_Continuation *op, *next_op;

        PR_ASSERT(NULL != pt_tq.head);

        pollingListNeeded = pt_tq.op_count;

    /*
     * We are not holding the pt_tq.ml lock now, so more items may
     * get added to pt_tq during this window of time.  We hope
     * that 10 more spaces in the polling list should be enough.
     *
     * The space allocated is for both a vector that parallels the
     * polling list, providing pointers directly into the operation's
     * table and the polling list itself. There is a guard element
     * between the two structures.
     */
        pollingListNeeded += 10;
        if (pollingListNeeded > pollingSlotsAllocated)
        {
            if (NULL != pollingOps) PR_Free(pollingOps);
            pollingOps = (pt_Continuation**)PR_Malloc(
                sizeof(pt_Continuation**) + pollingListNeeded * 
                    (sizeof(struct pollfd) + sizeof(pt_Continuation*)));
            PR_ASSERT(NULL != pollingOps);
            pollingSlotsAllocated = pollingListNeeded;
            pollingOps[pollingSlotsAllocated] = (pt_Continuation*)-1;
            pollingList = (struct pollfd*)(&pollingOps[pollingSlotsAllocated + 1]);
            
        }

#if defined(DEBUG)
        if (pollingListNeeded > pt_debug.pollingListMax)
            pt_debug.pollingListMax = pollingListNeeded;
#endif

        /*
        ** This is interrupt processing. If this thread was interrupted,
        ** the thread state will have the PT_THREAD_ABORTED bit set. This
        ** overrides good completions as well as timeouts.
        **
        ** BTW, it does no good to hold the lock here. This lock doesn't
        ** protect the thread structure in any way. Testing the bit and
        ** (perhaps) resetting it are safe 'cause it's the only modifiable
        ** bit in that word.
        */
        if (pt_tq.thread->state & PT_THREAD_ABORTED)
        {
            my_op->status = pt_continuation_abort;
            pt_tq.thread->state &= ~PT_THREAD_ABORTED;
        }


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
                next_op = pt_FinishTimedInternal(op);
                if (op == my_op) goto recycle;
                else op = next_op;
                PR_ASSERT(NULL != pt_tq.head);
            }
            else
            {
                op->status = pt_continuation_pending;
                if (pollingListUsed >= pollingSlotsAllocated)
                {
#if defined(DEBUG)
                    pt_debug.predictionsFoiled += 1;
#endif
                    break;
                }
                PR_ASSERT((pt_Continuation*)-1 == pollingOps[pollingSlotsAllocated]);
                pollingOps[pollingListUsed] = op;
                pollingList[pollingListUsed].revents = 0;
                pollingList[pollingListUsed].fd = op->arg1.osfd;
                pollingList[pollingListUsed].events = op->event;
                pollingListUsed += 1;
                op = op->next;
            }
        }

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

        PR_Unlock(pt_tq.ml);

        /*
         * If 'op' isn't NULL at this point, then we didn't get to
         * the end of the list. That means that more items got added
         * to the list than we anticipated. So, forget this iteration,
         * go around the horn again.
         *
         * One would hope this doesn't happen all that often.
         */
        if (NULL != op) continue;  /* make it rethink things */

        PR_ASSERT((pt_Continuation*)-1 == pollingOps[pollingSlotsAllocated]);

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

            for (pollIndex = 0; pollIndex < pollingListUsed; ++pollIndex)
            {
                PRIntn fd = pollingList[pollIndex].fd;
                PRInt16 events = pollingList[pollIndex].events;
                PRInt16 revents = pollingList[pollIndex].revents;

                op = pollingOps[pollIndex];  /* this is the operation */

                /* (ref: Bug #153459)
                ** In case of POLLERR we let the operation retry in hope
                ** of getting a more definitive OS error.
                */
                if ((revents & POLLNVAL)  /* busted in all cases */
                || ((events & POLLOUT) && (revents & POLLHUP)))  /* write op & hup */
                {
                    PR_Lock(pt_tq.ml);
                    op->result.code = -1;
                    if (POLLNVAL & revents) op->syserrno = EBADF;
                    else if (POLLHUP & revents) op->syserrno = EPIPE;
                    (void)pt_FinishTimedInternal(op);
                    if (op == my_op) goto recycle;
                    PR_Unlock(pt_tq.ml);
                }
                else if ((0 != revents)
                && (pt_continuation_pending == op->status)
                && (op->function(op, revents)))
                {
                /*
                 * Only good?(?) revents left. Operations not pending
                 * will be pruned next time we build a list. This operation
                 * will be pruned if the continueation indicates it is
                 * finished.
                 */

                    PR_Lock(pt_tq.ml);
                    (void)pt_FinishTimedInternal(op);
                    if (op == my_op) goto recycle;
                    PR_Unlock(pt_tq.ml);
                }
            }
        }

        /*
         * This is timeout processing. It is done after checking
         * for good completions. Those that just made it under the
         * wire are lucky, but none the less, valid.
         */
        now = PR_IntervalNow();
        PR_Lock(pt_tq.ml);
        while ((NULL != pt_tq.head)
        && (PR_INTERVAL_NO_TIMEOUT != pt_tq.head->timeout))
        {
            op = pt_tq.head;  /* get a copy of this before finishing it */
            if ((PRInt32)(op->absolute - now) > 0) break;
            /* 
             * The head element of the timed queue has timed out. Record
             * the reason for completion and take it out of the list.
             */
            op->result.code = -1;
            op->syserrno = ETIMEDOUT;
            (void)pt_FinishTimedInternal(op);

            /* 
             * If it's 'my_op' then we have to switch threads. Exit w/o
             * finishing the scan. The scan will be completed when another
             * thread calls in acting as the continuation thread. 
             */
            if (op == my_op) goto recycle;  /* exit w/o unlocking */
        }
        PR_Unlock(pt_tq.ml);
    }

    PR_NOT_REACHED("This is a while(true) loop /w no breaks");

recycle:
    /*
    ** Recycling the continuation thread.
    **
    ** The thread we were using for I/O continuations just completed 
    ** the I/O it submitted. It has to return to it's caller. We need
    ** another thread to act in the continuation role. We can do that
    ** by taking any operation off the timed queue, setting its state
    ** to 'recycle' and notifying the condition.
    **
    ** Selecting a likely thread seems like magic. I'm going to try
    ** using one that has the longest (or no) timeout, pt_tq.tail.
    ** If that slot's empty, then there's no outstanding I/O and we
    ** don't need a thread at all.
    **
    ** BTW, we're locked right now, and we'll be returning with the
    ** the lock held as well. Seems odd, doesn't it?
    */

    /* $$$ should this be called with the lock held? $$$ */
    PR_SetThreadPriority(pt_tq.thread, priority);  /* reset back to caller's */

    PR_ASSERT((NULL == pt_tq.head) == (0 == pt_tq.op_count));
    PR_ASSERT((NULL == pt_tq.head) == (NULL == pt_tq.tail));
    PR_ASSERT(pt_continuation_done == my_op->status);
    
    if (NULL != pt_tq.tail)
    {
        if (pt_tq.tail->status != pt_continuation_abort)
        {
            pt_tq.tail->status = pt_continuation_recycle;
        }
        PR_NotifyCondVar(pt_tq.tail->complete);
#if defined(DEBUG)
        pt_debug.recyclesNeeded += 1;
#endif
    }
#if defined(DEBUG)
     else pt_debug.quiescentIO += 1;
#endif

}  /* pt_ContinuationThreadInternal */

static PRIntn pt_Continue(pt_Continuation *op)
{
    PRStatus rv;
    PRThread *self = PR_GetCurrentThread();
    /* lazy allocation of the thread's cv */
    if (NULL == self->io_cv)
        self->io_cv = PR_NewCondVar(pt_tq.ml);
    /* Finish filling in the blank slots */
    op->complete = self->io_cv;
    op->status = pt_continuation_pending;  /* set default value */
    PR_Lock(pt_tq.ml);  /* we provide the locking */

    pt_InsertTimedInternal(op);  /* insert in the structure */

    /*
    ** At this point, we try to decide whether there is a continuation
    ** thread, or whether we should assign this one to serve in that role.
    */
    do
    {
        if (NULL == pt_tq.thread)
        {
            /*
            ** We're the one. Call the processing function with the lock
            ** held. It will return with it held as well, though there
            ** will certainly be times within the function when it gets
            ** released.
            */
            pt_tq.thread = self;  /* I'm taking control */
            pt_ContinuationThreadInternal(op); /* go slash and burn */
            PR_ASSERT(pt_continuation_done == op->status);
            pt_tq.thread = NULL;  /* I'm abdicating my rule */
        }
        else
        {
            rv = PR_WaitCondVar(op->complete, PR_INTERVAL_NO_TIMEOUT);
            /*
             * If we get interrupted, we set state the continuation thread will
             * see and allow it to finish the I/O operation w/ error. That way
             * the rule that only the continuation thread is removing elements
             * from the list is still valid.
             *
             * Don't call interrupt on the continuation thread. That'll just
             * irritate him. He's cycling around at least every mx_poll_ticks
             * anyhow and should notice the request in there. When he does
             * notice, this operation will be finished and the op's status
             * marked as pt_continuation_done.
             */
            if ((PR_FAILURE == rv)  /* the wait was interrupted */
            && (PR_PENDING_INTERRUPT_ERROR == PR_GetError()))
            {
                if (pt_continuation_done == op->status)
                {
                    /*
                     * The op is done and has been removed
                     * from the timed queue.  We must not
                     * change op->status, otherwise this
                     * thread will go around the loop again.
                     *
                     * It's harsh to mark the op failed with
                     * interrupt error when the io is already
                     * done, but we should indicate the fact
                     * that the thread was interrupted.  So
                     * we set the aborted flag to abort the
                     * thread's next blocking call.  Is this
                     * the right thing to do?
                     */
                    self->state |= PT_THREAD_ABORTED;
                }
                else
                {
                    /* go around the loop again */
                    op->status = pt_continuation_abort;
                }
            }
            /*
             * If we're to recycle, continue within this loop. This will
             * cause this thread to become the continuation thread.
             */

        }
    } while (pt_continuation_done != op->status);


    PR_Unlock(pt_tq.ml);  /* we provided the locking */

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
#if defined(SOLARIS)
    op->result.code = read(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
#else
    op->result.code = recv(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recv_cont */

static PRBool pt_send_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
#if defined(SOLARIS)
    bytes = write(op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
#else
    bytes = send(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
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
    else if ((-1 == bytes) && (EWOULDBLOCK != op->syserrno)
    && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_send_cont */

static PRBool pt_write_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    bytes = write(op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
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
    else if ((-1 == bytes) && (EWOULDBLOCK != op->syserrno)
    && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_write_cont */

static PRBool pt_writev_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    struct iovec *iov = (struct iovec*)op->arg2.buffer;
    /*
     * Same rules as write, but continuing seems to be a bit more
     * complicated. As the number of bytes sent grows, we have to
     * redefine the vector we're pointing at. We might have to
     * modify an individual vector parms or we might have to eliminate
     * a pair altogether.
     */
    bytes = writev(op->arg1.osfd, iov, op->arg3.amount);
    op->syserrno = errno;
    if (bytes > 0)  /* this is progress */
    {
        PRIntn iov_index;
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
    else if ((-1 == bytes) && (EWOULDBLOCK != op->syserrno)
    && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
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
    else if ((-1 == bytes) && (EWOULDBLOCK != op->syserrno)
    && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
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

#ifdef AIX
static PRBool pt_aix_transmitfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct sf_parms *sf_struct = (struct sf_parms *) op->arg2.buffer;
    int rv;

    rv = AIX_SEND_FILE(&op->arg1.osfd, sf_struct, op->arg4.flags);
    op->syserrno = errno;

    if (rv != -1) {
        op->result.code += sf_struct->bytes_sent;
    } else if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN) {
        op->result.code = -1;
    } else {
        return PR_FALSE;
    }

    if (rv == 1) {    /* more data to send */
        return PR_FALSE;
    }

    return PR_TRUE;
}
#endif  /* AIX */

#ifdef HPUX11
static PRBool pt_hpux_transmitfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct iovec *hdtrl = (struct iovec *) op->arg2.buffer;
    int count;

    count = sendfile(op->arg1.osfd, op->filedesc, op->arg3.offset, 0,
            hdtrl, op->arg4.flags);
    PR_ASSERT(count <= op->nbytes_to_send);
    op->syserrno = errno;

    if (count != -1) {
        op->result.code += count;
    } else if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN) {
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

void _PR_InitIO()
{
    PRIntn rv;

    pt_tq.ml = PR_NewLock();
    PR_ASSERT(NULL != pt_tq.ml);

#if defined(DEBUG)
    memset(&pt_debug, 0, sizeof(PTDebug));
    pt_debug.timeStarted = PR_Now();
#endif

    pt_tq.thread = NULL;

    _pr_flock_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_flock_lock);
    _pr_rename_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_rename_lock); 

    _PR_InitFdCache();  /* do that */   

    _pr_stdin = pt_SetMethods(0, PR_DESC_FILE);
    _pr_stdout = pt_SetMethods(1, PR_DESC_FILE);
    _pr_stderr = pt_SetMethods(2, PR_DESC_FILE);
    PR_ASSERT(_pr_stdin && _pr_stdout && _pr_stderr);
}  /* _PR_InitIO */

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
    if ((NULL == fd) || (NULL == fd->secret)
        || ((_PR_FILEDESC_OPEN != fd->secret->state)
        && (_PR_FILEDESC_CLOSED != fd->secret->state)))
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if (pt_TestAbort()) return PR_FAILURE;

    if (_PR_FILEDESC_OPEN == fd->secret->state)
    {
        if (-1 == close(fd->secret->md.osfd))
        {
            pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
            return PR_FAILURE;
        }
        fd->secret->state = _PR_FILEDESC_CLOSED;
    }
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* pt_Close */

static PRInt32 pt_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return bytes;

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

    if (pt_TestAbort()) return bytes;

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
    PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_len, PRIntervalTime timeout)
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
    rv = bytes = writev(fd->secret->md.osfd, (const struct iovec*)iov, iov_len);
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
        osiov = (struct iovec*)PR_MALLOC(osiov_len * sizeof(struct iovec));
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
    return _PR_MD_LSEEK(fd, offset, whence);
}  /* pt_Seek */

static PRInt64 pt_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK64(fd, offset, whence);
}  /* pt_Seek64 */

static PRInt32 pt_Available_f(PRFileDesc *fd)
{
    PRInt32 result, cur, end;

    cur = _PR_MD_LSEEK(fd, 0, PR_SEEK_CUR);

    if (cur >= 0)
        end = _PR_MD_LSEEK(fd, 0, PR_SEEK_END);

    if ((cur < 0) || (end < 0)) {
        return -1;
    }

    result = end - cur;
    _PR_MD_LSEEK(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available_f */

static PRInt64 pt_Available64_f(PRFileDesc *fd)
{
    PRInt64 result, cur, end;
    PRInt64 minus_one;

    LL_I2L(minus_one, -1);
    cur = _PR_MD_LSEEK64(fd, LL_ZERO, PR_SEEK_CUR);

    if (LL_GE_ZERO(cur))
        end = _PR_MD_LSEEK64(fd, LL_ZERO, PR_SEEK_END);

    if (!LL_GE_ZERO(cur) || !LL_GE_ZERO(end)) return minus_one;

    LL_SUB(result, end, cur);
    (void)_PR_MD_LSEEK64(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available64_f */

static PRInt32 pt_Available_s(PRFileDesc *fd)
{
    PRInt32 rv, bytes = -1;
    if (pt_TestAbort()) return bytes;

    rv = ioctl(fd->secret->md.osfd, FIONREAD, &bytes);

    if (rv == -1)
        pt_MapError(_PR_MD_MAP_SOCKETAVAILABLE_ERROR, errno);
    return bytes;
}  /* pt_Available_s */

static PRInt64 pt_Available64_s(PRFileDesc *fd)
{
    PRInt64 rv;
    LL_I2L(rv, pt_Available_s(fd));
    return rv;
}  /* pt_Available64_s */

static PRStatus pt_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo */

static PRStatus pt_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO64(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo64 */

static PRStatus pt_Synch(PRFileDesc *fd)
{
    return (NULL == fd) ? PR_FAILURE : PR_SUCCESS;
} /* pt_Synch */

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
    pt_SockLen addr_len;
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = addr->raw.family;
    rv = connect(fd->secret->md.osfd, (struct sockaddr*)&addrCopy, addr_len);
#else
    rv = connect(fd->secret->md.osfd, (struct sockaddr*)addr, addr_len);
#endif
    syserrno = errno;
    if ((-1 == rv) && (EINPROGRESS == syserrno) && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
#ifdef _PR_HAVE_SOCKADDR_LEN
            op.arg2.buffer = (void*)&addrCopy;
#else
            op.arg2.buffer = (void*)addr;
#endif
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
    int err;
    PRInt32 osfd;
    PRFileDesc *bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);

    if (NULL == bottom) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (pd->out_flags & PR_POLL_NVAL) {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((pd->out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR)) == 0) {
        PR_ASSERT(pd->out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
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
#ifdef _PR_HAVE_SOCKADDR_LEN
    /* ignore the sa_len field of struct sockaddr */
    if (addr)
    {
        addr->raw.family = ((struct sockaddr*)addr)->sa_family;
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    newfd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP);
    if (newfd == NULL) close(osfd);  /* $$$ whoops! this doesn't work $$$ */
    else
    {
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
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
    pt_SockLen addr_len;
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);

    if (addr->raw.family == AF_UNIX)
    {
        /* Disallow relative pathnames */
        if (addr->local.path[0] != '/')
        {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
        }
    }
    else
    {
        rv = setsockopt(
            fd->secret->md.osfd, SOL_SOCKET, SO_REUSEADDR,
            (_PRSockOptVal_t) &one, sizeof(one));
        if (rv == -1)
        {
            pt_MapError(_PR_MD_MAP_SETSOCKOPT_ERROR, errno);
            return PR_FAILURE;
        }
    }

    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = addr->raw.family;
    rv = bind(fd->secret->md.osfd, (struct sockaddr*)&addrCopy, addr_len);
#else
    rv = bind(fd->secret->md.osfd, (struct sockaddr*)addr, addr_len);
#endif

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

static PRInt16 pt_Poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}  /* pt_Poll */

static PRInt32 pt_Recv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return bytes;

    /* recv() is a much slower call on pre-2.6 Solaris than read(). */
#if defined(SOLARIS)
    PR_ASSERT(0 == flags);
    bytes = read(fd->secret->md.osfd, buf, amount);
#else
    bytes = recv(fd->secret->md.osfd, buf, amount, flags);
#endif
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
     *     #  define send        cma_send
     *     extern int  cma_send (int , void *, int, int );
     * So we need to cast away the 'const' of argument #2 for send().
     */
#if defined (HPUX) && defined(_PR_DCETHREADS)
#define PT_SENDBUF_CAST (void *)
#else
#define PT_SENDBUF_CAST
#endif

    if (pt_TestAbort()) return bytes;

    /*
     * On pre-2.6 Solaris, send() is much slower than write().
     * On 2.6 and beyond, with in-kernel sockets, send() and
     * write() are fairly equivalent in performance.
     */
#if defined(SOLARIS)
    PR_ASSERT(0 == flags);
    bytes = write(fd->secret->md.osfd, PT_SENDBUF_CAST buf, amount);
#else
    bytes = send(fd->secret->md.osfd, PT_SENDBUF_CAST buf, amount, flags);
#endif
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
    pt_SockLen addr_len;
#ifdef _PR_HAVE_SOCKADDR_LEN
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return bytes;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = addr->raw.family;
    bytes = sendto(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)&addrCopy, addr_len);
#else
    bytes = sendto(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)addr, addr_len);
#endif
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
#ifdef _PR_HAVE_SOCKADDR_LEN
        op.arg5.addr = (PRNetAddr*)&addrCopy;
#else
        op.arg5.addr = (PRNetAddr*)addr;
#endif
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

    if (pt_TestAbort()) return bytes;

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
#ifdef _PR_HAVE_SOCKADDR_LEN
    if (bytes >= 0)
    {
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_RECVFROM_ERROR, syserrno);
    return bytes;
}  /* pt_RecvFrom */

#ifdef AIX
#ifndef HAVE_SEND_FILE
static pthread_once_t pt_aix_sendfile_once_block = PTHREAD_ONCE_INIT;

static void pt_aix_sendfile_init_routine(void)
{
    void *handle = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
    pt_aix_sendfile_fptr = (ssize_t (*)()) dlsym(handle, "send_file");
    dlclose(handle);
}

/* 
 * pt_AIXDispatchTransmitFile
 */
static PRInt32 pt_AIXDispatchTransmitFile(PRFileDesc *sd, PRFileDesc *fd,
      const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
      PRIntervalTime timeout)
{
    int rv;

    rv = pthread_once(&pt_aix_sendfile_once_block,
            pt_aix_sendfile_init_routine);
    PR_ASSERT(0 == rv);
    if (pt_aix_sendfile_fptr) {
        return pt_AIXTransmitFile(sd, fd, headers, hlen, flags, timeout);
    } else {
        return _PR_UnixTransmitFile(sd, fd, headers, hlen, flags, timeout);
    }
}
#endif /* !HAVE_SEND_FILE */

/*
 * pt_AIXTransmitFile
 *
 *    Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *    bytes of headers is sent before sending the file.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *    
 *    return number of bytes sent or -1 on error
 *
 *      This implementation takes advantage of the send_file() system
 *      call available in AIX 4.3.2.
 */

static PRInt32 pt_AIXTransmitFile(PRFileDesc *sd, PRFileDesc *fd, 
        const void *headers, PRInt32 hlen, PRTransmitFileFlags flags,
        PRIntervalTime timeout)
{
    struct sf_parms sf_struct;
    uint_t send_flags;
    ssize_t rv;
    int syserrno;
    PRInt32 count;

    sf_struct.header_data = (void *) headers;  /* cast away the 'const' */
    sf_struct.header_length = hlen;
    sf_struct.file_descriptor = fd->secret->md.osfd;
    sf_struct.file_size = 0;
    sf_struct.file_offset = 0;
    sf_struct.file_bytes = -1;
    sf_struct.trailer_data = NULL;
    sf_struct.trailer_length = 0;
    sf_struct.bytes_sent = 0;

    send_flags = 0;

    do {
        rv = AIX_SEND_FILE(&sd->secret->md.osfd, &sf_struct, send_flags);
    } while (rv == -1 && (syserrno = errno) == EINTR);

    if (rv == -1) {
        if (syserrno == EAGAIN || syserrno == EWOULDBLOCK) {
            count = 0; /* Not a real error.  Need to continue. */
        } else {
            count = -1;
        }
    } else {
        count = sf_struct.bytes_sent;
    }

    if ((rv == 1) || ((rv == -1) && (count == 0))) {
        pt_Continuation op;

        op.arg1.osfd = sd->secret->md.osfd;
        op.arg2.buffer = &sf_struct;
        op.arg4.flags = send_flags;
        op.result.code = count;
        op.timeout = timeout;
        op.function = pt_aix_transmitfile_cont;
        op.event = POLLOUT | POLLPRI;
        count = pt_Continue(&op);
        syserrno = op.syserrno;
    }

    if (count == -1) {
        _MD_aix_map_sendfile_error(syserrno);
        return -1;
    }
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        PR_Close(sd);
    }
    return count;
}
#endif /* AIX */

#ifdef HPUX11
/*
 * pt_HPUXTransmitFile
 *
 *    Send file fd across socket sd. If headers is non-NULL, 'hlen'
 *    bytes of headers is sent before sending the file.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *    
 *    return number of bytes sent or -1 on error
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
    /* The socket must be in blocking mode. */
    if (sd->secret->nonblocking)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

#ifdef HPUX11
    return pt_HPUXTransmitFile(sd, fd, headers, hlen, flags, timeout);
#elif defined(AIX)
#ifdef HAVE_SEND_FILE
    return pt_AIXTransmitFile(sd, fd, headers, hlen, flags, timeout);
#else
    return pt_AIXDispatchTransmitFile(sd, fd, headers, hlen, flags, timeout);
#endif /* HAVE_SEND_FILE */
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

    if (pt_TestAbort()) return rv;
    /* The socket must be in blocking mode. */
    if (sd->secret->nonblocking)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return rv;
    }

    /*
    ** The timeout does not apply to the accept portion of the
    ** operation - it waits indefinitely.
    */
    accepted = PR_Accept(sd, &remote, PR_INTERVAL_NO_TIMEOUT);
    if (NULL == accepted) return rv;

    rv = PR_Recv(accepted, buf, amount, 0, timeout);
    if (rv >= 0)
    {
        /* copy the new info out where caller can see it */
        PRPtrdiff aligned = (PRPtrdiff)buf + amount + sizeof(void*) - 1;
        *raddr = (PRNetAddr*)(aligned & ~(sizeof(void*) - 1));
        memcpy(*raddr, &remote, PR_NETADDR_SIZE(&remote));
        *nd = accepted;
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
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETSOCKNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
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

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETPEERNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
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
                PRUint8 xbool;
                length = sizeof(xbool);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, &length);
                PR_ASSERT((-1 == rv) || (sizeof(xbool) == length));
                data->value.mcast_loopback = (0 == xbool) ? PR_FALSE : PR_TRUE;
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
                PRUint8 xbool = data->value.mcast_loopback ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, sizeof(xbool));
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
    pt_Available_f,
    pt_Available64_f,
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
    pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRGetsockoptFN)_PR_InvalidStatus,    
    (PRSetsockoptFN)_PR_InvalidStatus,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus
};

static PRIOMethods _pr_tcp_methods = {
    PR_DESC_SOCKET_TCP,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_s,
    pt_Available64_s,
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
    pt_Poll,
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
    pt_Available_s,
    pt_Available64_s,
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
    pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,
    (PRTransmitfileFN)_PR_InvalidInt,
    pt_GetSockName,
    pt_GetPeerName,
    pt_GetSockOpt,
    pt_SetSockOpt,
    pt_GetSocketOption,
    pt_SetSocketOption
};


static PRIOMethods _pr_socketpollfd_methods = {
    PR_DESC_SOCKET_POLL,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
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
	pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRGetsockoptFN)_PR_InvalidStatus,    
    (PRSetsockoptFN)_PR_InvalidStatus,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus
};

#if defined(_PR_FCNTL_FLAGS)
#undef _PR_FCNTL_FLAGS
#endif

#if defined(HPUX) || defined(OSF1) || defined(SOLARIS) || defined (IRIX) \
    || defined(AIX) || defined(LINUX) || defined(FREEBSD) || defined(NETBSD) \
    || defined(OPENBSD) || defined(BSDI)
#define _PR_FCNTL_FLAGS O_NONBLOCK
#else
#error "Can't determine architecture"
#endif

static PRFileDesc *pt_SetMethods(PRIntn osfd, PRDescType type)
{
    PRInt32 flags, one = 1;
    PRFileDesc *fd = _PR_Getfd();
    
    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->state = _PR_FILEDESC_OPEN;
        /* By default, a Unix fd is not closed on exec. */
        PR_ASSERT(0 == fcntl(osfd, F_GETFD, 0));
        fd->secret->inheritable = PR_TRUE;
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

PR_IMPLEMENT(const PRIOMethods*) PR_GetFileMethods()
{
    return &_pr_file_methods;
}  /* PR_GetFileMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetTCPMethods()
{
    return &_pr_tcp_methods;
}  /* PR_GetTCPMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetUDPMethods()
{
    return &_pr_udp_methods;
}  /* PR_GetUDPMethods */

static const PRIOMethods* PR_GetSocketPollFdMethods()
{
    return &_pr_socketpollfd_methods;
}  /* PR_GetUDPMethods */

PR_IMPLEMENT(PRFileDesc*) PR_AllocFileDesc(
    PRInt32 osfd, const PRIOMethods *methods)
{
    PRFileDesc *fd = _PR_Getfd();

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
    /* By default, a Unix fd is not closed on exec. */
    PR_ASSERT(0 == fcntl(osfd, F_GETFD, 0));
    fd->secret->inheritable = PR_TRUE;
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
        && PF_UNIX != domain)
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

    osfd = _md_iovector._open64(name, osflags, mode);
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
    PRInt32 rv = _PR_MD_GETFILEINFO(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo64(const char *fn, PRFileInfo64 *info)
{
    PRInt32 rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    rv = _PR_MD_GETFILEINFO64(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo64 */

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
        if (closedir(dir->md.d) == -1)
        {
            _PR_MD_MAP_CLOSEDIR_ERROR(errno);
            return PR_FAILURE;
        }
        dir->md.d = NULL;
        PR_DELETE(dir);
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

    if (pt_TestAbort()) return -1;

    if (0 == npds) PR_Sleep(timeout);
    else
    {
#define STACK_POLL_DESC_COUNT 4
        struct pollfd stack_syspoll[STACK_POLL_DESC_COUNT];
        struct pollfd *syspoll;
        PRIntn index, msecs;

        if (npds <= STACK_POLL_DESC_COUNT)
        {
            syspoll = stack_syspoll;
        }
        else
        {
            syspoll = (struct pollfd*)PR_MALLOC(npds * sizeof(struct pollfd));
            if (NULL == syspoll)
            {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                return -1;
            }
        }

        for (index = 0; index < npds; ++index)
        {
            PRInt16 in_flags_read = 0, in_flags_write = 0;
            PRInt16 out_flags_read = 0, out_flags_write = 0;

            if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
            {
                if (pds[index].in_flags & PR_POLL_READ)
                {
                    in_flags_read = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_WRITE,
                        &out_flags_read);
                }
                if (pds[index].in_flags & PR_POLL_WRITE)
                {
                    in_flags_write = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_READ,
                        &out_flags_write);
                }
                if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
                {
                    /* this one is ready right now */
                    if (0 == ready)
                    {
                        /*
                         * We will return without calling the system
                         * poll function.  So zero the out_flags
                         * fields of all the poll descriptors before
                         * this one.
                         */
                        int i;
                        for (i = 0; i < index; i++)
                        {
                            pds[i].out_flags = 0;
                        }
                    }
                    ready += 1;
                    pds[index].out_flags = out_flags_read | out_flags_write;
                }
                else
                {
                    /* now locate the NSPR layer at the bottom of the stack */
                    PRFileDesc *bottom = PR_GetIdentitiesLayer(
                        pds[index].fd, PR_NSPR_IO_LAYER);
                    PR_ASSERT(NULL != bottom);  /* what to do about that? */
                    pds[index].out_flags = 0;  /* pre-condition */
                    if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                    {
                        if (0 == ready)
                        {
                            syspoll[index].fd = bottom->secret->md.osfd;
                            syspoll[index].events = 0;
                            if (in_flags_read & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_read & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (in_flags_write & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_write & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (pds[index].in_flags & PR_POLL_EXCEPT)
                                syspoll[index].events |= POLLPRI;
                        }
                    }
                    else
                    {
                        if (0 == ready)
                        {
                            int i;
                            for (i = 0; i < index; i++)
                            {
                                pds[i].out_flags = 0;
                            }
                        }
                        ready += 1;  /* this will cause an abrupt return */
                        pds[index].out_flags = PR_POLL_NVAL;  /* bogii */
                    }
                }
            }
            else
            {
                /* make poll() ignore this entry */
                syspoll[index].fd = -1;
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

                if (EINTR == oserror)
                {
                    if (timeout == PR_INTERVAL_NO_TIMEOUT)
                        goto retry;
                    else if (timeout == PR_INTERVAL_NO_WAIT)
                        ready = 0;  /* don't retry, just time out */
                    else
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
                }
                else
                {
                    _PR_MD_MAP_POLL_ERROR(oserror);
                }
            }
            else if (ready > 0)
            {
                for (index = 0; index < npds; ++index)
                {
                    PRInt16 out_flags = 0;
                    if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
                    {
                        if (0 != syspoll[index].revents)
                        {
                            if (syspoll[index].revents & POLLIN)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_READ)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_READ)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLOUT)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLPRI)
                                out_flags |= PR_POLL_EXCEPT;
                            if (syspoll[index].revents & POLLERR)
                                out_flags |= PR_POLL_ERR;
                            if (syspoll[index].revents & POLLNVAL)
                                out_flags |= PR_POLL_NVAL;
                            if (syspoll[index].revents & POLLHUP)
                                out_flags |= PR_POLL_HUP;
                        }
                    }
                    pds[index].out_flags = out_flags;
                }
            }
        }

        if (syspoll != stack_syspoll)
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

/*
** Set the inheritance attribute of a file descriptor.
*/
PR_IMPLEMENT(PRStatus) PR_SetFDInheritable(
    PRFileDesc *fd,
    PRBool inheritable)
{
    /*
     * Only a non-layered, NSPR file descriptor can be inherited
     * by a child process.
     */
    if (fd->identity != PR_NSPR_IO_LAYER)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (fd->secret->inheritable != inheritable)
    {
        if (fcntl(fd->secret->md.osfd, F_SETFD,
        inheritable ? 0 : FD_CLOEXEC) == -1)
        {
            return PR_FAILURE;
        }
        fd->secret->inheritable = inheritable;
    }
    return PR_SUCCESS;
}

/*****************************************************************************/
/***************************** I/O friends methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_ImportFile(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_FILE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportFile */

PR_IMPLEMENT(PRFileDesc*) PR_ImportTCPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportTCPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_ImportUDPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_UDP);
    if (NULL != fd) close(osfd);
    return fd;
}  /* PR_ImportUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_CreateSocketPollFd(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->inheritable = PR_FALSE;
    	fd->secret->state = _PR_FILEDESC_OPEN;
        fd->methods = PR_GetSocketPollFdMethods();
    }

    return fd;
}  /* PR_CreateSocketPollFD */

PR_IMPLEMENT(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd)
{
    if (NULL == fd)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    fd->secret->state = _PR_FILEDESC_CLOSED;
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* PR_DestroySocketPollFd */

PR_IMPLEMENT(PRInt32) PR_FileDesc2NativeHandle(PRFileDesc *bottom)
{
    PRInt32 osfd = -1;
    bottom = (NULL == bottom) ?
        NULL : PR_GetIdentitiesLayer(bottom, PR_NSPR_IO_LAYER);
    if (NULL == bottom) PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    else osfd = bottom->secret->md.osfd;
    return osfd;
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

    if (pt_TestAbort()) return -1;

    if (-1 == stat(name, buf)) {
        pt_MapError(_PR_MD_MAP_STAT_ERROR, errno);
        return -1;
    } else {
        return 0;
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
