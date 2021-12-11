/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"
#include "pprmwait.h"

#define _MW_REHASH_MAX 11

static PRLock *mw_lock = NULL;
static _PRGlobalState *mw_state = NULL;

static PRIntervalTime max_polling_interval;

#ifdef WINNT

typedef struct TimerEvent {
    PRIntervalTime absolute;
    void (*func)(void *);
    void *arg;
    LONG ref_count;
    PRCList links;
} TimerEvent;

#define TIMER_EVENT_PTR(_qp) \
    ((TimerEvent *) ((char *) (_qp) - offsetof(TimerEvent, links)))

struct {
    PRLock *ml;
    PRCondVar *new_timer;
    PRCondVar *cancel_timer;
    PRThread *manager_thread;
    PRCList timer_queue;
} tm_vars;

static PRStatus TimerInit(void);
static void TimerManager(void *arg);
static TimerEvent *CreateTimer(PRIntervalTime timeout,
                               void (*func)(void *), void *arg);
static PRBool CancelTimer(TimerEvent *timer);

static void TimerManager(void *arg)
{
    PRIntervalTime now;
    PRIntervalTime timeout;
    PRCList *head;
    TimerEvent *timer;

    PR_Lock(tm_vars.ml);
    while (1)
    {
        if (PR_CLIST_IS_EMPTY(&tm_vars.timer_queue))
        {
            PR_WaitCondVar(tm_vars.new_timer, PR_INTERVAL_NO_TIMEOUT);
        }
        else
        {
            now = PR_IntervalNow();
            head = PR_LIST_HEAD(&tm_vars.timer_queue);
            timer = TIMER_EVENT_PTR(head);
            if ((PRInt32) (now - timer->absolute) >= 0)
            {
                PR_REMOVE_LINK(head);
                /*
                 * make its prev and next point to itself so that
                 * it's obvious that it's not on the timer_queue.
                 */
                PR_INIT_CLIST(head);
                PR_ASSERT(2 == timer->ref_count);
                PR_Unlock(tm_vars.ml);
                timer->func(timer->arg);
                PR_Lock(tm_vars.ml);
                timer->ref_count -= 1;
                if (0 == timer->ref_count)
                {
                    PR_NotifyAllCondVar(tm_vars.cancel_timer);
                }
            }
            else
            {
                timeout = (PRIntervalTime)(timer->absolute - now);
                PR_WaitCondVar(tm_vars.new_timer, timeout);
            }
        }
    }
    PR_Unlock(tm_vars.ml);
}

static TimerEvent *CreateTimer(
    PRIntervalTime timeout,
    void (*func)(void *),
    void *arg)
{
    TimerEvent *timer;
    PRCList *links, *tail;
    TimerEvent *elem;

    timer = PR_NEW(TimerEvent);
    if (NULL == timer)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return timer;
    }
    timer->absolute = PR_IntervalNow() + timeout;
    timer->func = func;
    timer->arg = arg;
    timer->ref_count = 2;
    PR_Lock(tm_vars.ml);
    tail = links = PR_LIST_TAIL(&tm_vars.timer_queue);
    while (links->prev != tail)
    {
        elem = TIMER_EVENT_PTR(links);
        if ((PRInt32)(timer->absolute - elem->absolute) >= 0)
        {
            break;
        }
        links = links->prev;
    }
    PR_INSERT_AFTER(&timer->links, links);
    PR_NotifyCondVar(tm_vars.new_timer);
    PR_Unlock(tm_vars.ml);
    return timer;
}

static PRBool CancelTimer(TimerEvent *timer)
{
    PRBool canceled = PR_FALSE;

    PR_Lock(tm_vars.ml);
    timer->ref_count -= 1;
    if (timer->links.prev == &timer->links)
    {
        while (timer->ref_count == 1)
        {
            PR_WaitCondVar(tm_vars.cancel_timer, PR_INTERVAL_NO_TIMEOUT);
        }
    }
    else
    {
        PR_REMOVE_LINK(&timer->links);
        canceled = PR_TRUE;
    }
    PR_Unlock(tm_vars.ml);
    PR_DELETE(timer);
    return canceled;
}

static PRStatus TimerInit(void)
{
    tm_vars.ml = PR_NewLock();
    if (NULL == tm_vars.ml)
    {
        goto failed;
    }
    tm_vars.new_timer = PR_NewCondVar(tm_vars.ml);
    if (NULL == tm_vars.new_timer)
    {
        goto failed;
    }
    tm_vars.cancel_timer = PR_NewCondVar(tm_vars.ml);
    if (NULL == tm_vars.cancel_timer)
    {
        goto failed;
    }
    PR_INIT_CLIST(&tm_vars.timer_queue);
    tm_vars.manager_thread = PR_CreateThread(
                                 PR_SYSTEM_THREAD, TimerManager, NULL, PR_PRIORITY_NORMAL,
                                 PR_LOCAL_THREAD, PR_UNJOINABLE_THREAD, 0);
    if (NULL == tm_vars.manager_thread)
    {
        goto failed;
    }
    return PR_SUCCESS;

failed:
    if (NULL != tm_vars.cancel_timer)
    {
        PR_DestroyCondVar(tm_vars.cancel_timer);
    }
    if (NULL != tm_vars.new_timer)
    {
        PR_DestroyCondVar(tm_vars.new_timer);
    }
    if (NULL != tm_vars.ml)
    {
        PR_DestroyLock(tm_vars.ml);
    }
    return PR_FAILURE;
}

#endif /* WINNT */

/******************************************************************/
/******************************************************************/
/************************ The private portion *********************/
/******************************************************************/
/******************************************************************/
void _PR_InitMW(void)
{
#ifdef WINNT
    /*
     * We use NT 4's InterlockedCompareExchange() to operate
     * on PRMWStatus variables.
     */
    PR_ASSERT(sizeof(LONG) == sizeof(PRMWStatus));
    TimerInit();
#endif
    mw_lock = PR_NewLock();
    PR_ASSERT(NULL != mw_lock);
    mw_state = PR_NEWZAP(_PRGlobalState);
    PR_ASSERT(NULL != mw_state);
    PR_INIT_CLIST(&mw_state->group_list);
    max_polling_interval = PR_MillisecondsToInterval(MAX_POLLING_INTERVAL);
}  /* _PR_InitMW */

void _PR_CleanupMW(void)
{
    PR_DestroyLock(mw_lock);
    mw_lock = NULL;
    if (mw_state->group) {
        PR_DestroyWaitGroup(mw_state->group);
        /* mw_state->group is set to NULL as a side effect. */
    }
    PR_DELETE(mw_state);
}  /* _PR_CleanupMW */

static PRWaitGroup *MW_Init2(void)
{
    PRWaitGroup *group = mw_state->group;  /* it's the null group */
    if (NULL == group)  /* there is this special case */
    {
        group = PR_CreateWaitGroup(_PR_DEFAULT_HASH_LENGTH);
        if (NULL == group) {
            goto failed_alloc;
        }
        PR_Lock(mw_lock);
        if (NULL == mw_state->group)
        {
            mw_state->group = group;
            group = NULL;
        }
        PR_Unlock(mw_lock);
        if (group != NULL) {
            (void)PR_DestroyWaitGroup(group);
        }
        group = mw_state->group;  /* somebody beat us to it */
    }
failed_alloc:
    return group;  /* whatever */
}  /* MW_Init2 */

static _PR_HashStory MW_AddHashInternal(PRRecvWait *desc, _PRWaiterHash *hash)
{
    /*
    ** The entries are put in the table using the fd (PRFileDesc*) of
    ** the receive descriptor as the key. This allows us to locate
    ** the appropriate entry aqain when the poll operation finishes.
    **
    ** The pointer to the file descriptor object is first divided by
    ** the natural alignment of a pointer in the belief that object
    ** will have at least that many zeros in the low order bits.
    ** This may not be a good assuption.
    **
    ** We try to put the entry in by rehashing _MW_REHASH_MAX times. After
    ** that we declare defeat and force the table to be reconstructed.
    ** Since some fds might be added more than once, won't that cause
    ** collisions even in an empty table?
    */
    PRIntn rehash = _MW_REHASH_MAX;
    PRRecvWait **waiter;
    PRUintn hidx = _MW_HASH(desc->fd, hash->length);
    PRUintn hoffset = 0;

    while (rehash-- > 0)
    {
        waiter = &hash->recv_wait;
        if (NULL == waiter[hidx])
        {
            waiter[hidx] = desc;
            hash->count += 1;
#if 0
            printf("Adding 0x%x->0x%x ", desc, desc->fd);
            printf(
                "table[%u:%u:*%u]: 0x%x->0x%x\n",
                hidx, hash->count, hash->length, waiter[hidx], waiter[hidx]->fd);
#endif
            return _prmw_success;
        }
        if (desc == waiter[hidx])
        {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);  /* desc already in table */
            return _prmw_error;
        }
#if 0
        printf("Failing 0x%x->0x%x ", desc, desc->fd);
        printf(
            "table[*%u:%u:%u]: 0x%x->0x%x\n",
            hidx, hash->count, hash->length, waiter[hidx], waiter[hidx]->fd);
#endif
        if (0 == hoffset)
        {
            hoffset = _MW_HASH2(desc->fd, hash->length);
            PR_ASSERT(0 != hoffset);
        }
        hidx = (hidx + hoffset) % (hash->length);
    }
    return _prmw_rehash;
}  /* MW_AddHashInternal */

static _PR_HashStory MW_ExpandHashInternal(PRWaitGroup *group)
{
    PRRecvWait **desc;
    PRUint32 pidx, length;
    _PRWaiterHash *newHash, *oldHash = group->waiter;
    PRBool retry;
    _PR_HashStory hrv;

    static const PRInt32 prime_number[] = {
        _PR_DEFAULT_HASH_LENGTH, 179, 521, 907, 1427,
        2711, 3917, 5021, 8219, 11549, 18911, 26711, 33749, 44771
    };
    PRUintn primes = (sizeof(prime_number) / sizeof(PRInt32));

    /* look up the next size we'd like to use for the hash table */
    for (pidx = 0; pidx < primes; ++pidx)
    {
        if (prime_number[pidx] == oldHash->length)
        {
            break;
        }
    }
    /* table size must be one of the prime numbers */
    PR_ASSERT(pidx < primes);

    /* if pidx == primes - 1, we can't expand the table any more */
    while (pidx < primes - 1)
    {
        /* next size */
        ++pidx;
        length = prime_number[pidx];

        /* allocate the new hash table and fill it in with the old */
        newHash = (_PRWaiterHash*)PR_CALLOC(
                      sizeof(_PRWaiterHash) + (length * sizeof(PRRecvWait*)));
        if (NULL == newHash)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return _prmw_error;
        }

        newHash->length = length;
        retry = PR_FALSE;
        for (desc = &oldHash->recv_wait;
             newHash->count < oldHash->count; ++desc)
        {
            PR_ASSERT(desc < &oldHash->recv_wait + oldHash->length);
            if (NULL != *desc)
            {
                hrv = MW_AddHashInternal(*desc, newHash);
                PR_ASSERT(_prmw_error != hrv);
                if (_prmw_success != hrv)
                {
                    PR_DELETE(newHash);
                    retry = PR_TRUE;
                    break;
                }
            }
        }
        if (retry) {
            continue;
        }

        PR_DELETE(group->waiter);
        group->waiter = newHash;
        group->p_timestamp += 1;
        return _prmw_success;
    }

    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return _prmw_error;  /* we're hosed */
}  /* MW_ExpandHashInternal */

#ifndef WINNT
static void _MW_DoneInternal(
    PRWaitGroup *group, PRRecvWait **waiter, PRMWStatus outcome)
{
    /*
    ** Add this receive wait object to the list of finished I/O
    ** operations for this particular group. If there are other
    ** threads waiting on the group, notify one. If not, arrange
    ** for this thread to return.
    */

#if 0
    printf("Removing 0x%x->0x%x\n", *waiter, (*waiter)->fd);
#endif
    (*waiter)->outcome = outcome;
    PR_APPEND_LINK(&((*waiter)->internal), &group->io_ready);
    PR_NotifyCondVar(group->io_complete);
    PR_ASSERT(0 != group->waiter->count);
    group->waiter->count -= 1;
    *waiter = NULL;
}  /* _MW_DoneInternal */
#endif /* WINNT */

static PRRecvWait **_MW_LookupInternal(PRWaitGroup *group, PRFileDesc *fd)
{
    /*
    ** Find the receive wait object corresponding to the file descriptor.
    ** Only search the wait group specified.
    */
    PRRecvWait **desc;
    PRIntn rehash = _MW_REHASH_MAX;
    _PRWaiterHash *hash = group->waiter;
    PRUintn hidx = _MW_HASH(fd, hash->length);
    PRUintn hoffset = 0;

    while (rehash-- > 0)
    {
        desc = (&hash->recv_wait) + hidx;
        if ((*desc != NULL) && ((*desc)->fd == fd)) {
            return desc;
        }
        if (0 == hoffset)
        {
            hoffset = _MW_HASH2(fd, hash->length);
            PR_ASSERT(0 != hoffset);
        }
        hidx = (hidx + hoffset) % (hash->length);
    }
    return NULL;
}  /* _MW_LookupInternal */

#ifndef WINNT
static PRStatus _MW_PollInternal(PRWaitGroup *group)
{
    PRRecvWait **waiter;
    PRStatus rv = PR_FAILURE;
    PRInt32 count, count_ready;
    PRIntervalTime polling_interval;

    group->poller = PR_GetCurrentThread();

    while (PR_TRUE)
    {
        PRIntervalTime now, since_last_poll;
        PRPollDesc *poll_list;

        while (0 == group->waiter->count)
        {
            PRStatus st;
            st = PR_WaitCondVar(group->new_business, PR_INTERVAL_NO_TIMEOUT);
            if (_prmw_running != group->state)
            {
                PR_SetError(PR_INVALID_STATE_ERROR, 0);
                goto aborted;
            }
            if (_MW_ABORTED(st)) {
                goto aborted;
            }
        }

        /*
        ** There's something to do. See if our existing polling list
        ** is large enough for what we have to do?
        */

        while (group->polling_count < group->waiter->count)
        {
            PRUint32 old_count = group->waiter->count;
            PRUint32 new_count = PR_ROUNDUP(old_count, _PR_POLL_COUNT_FUDGE);
            PRSize new_size = sizeof(PRPollDesc) * new_count;
            PRPollDesc *old_polling_list = group->polling_list;

            PR_Unlock(group->ml);
            poll_list = (PRPollDesc*)PR_CALLOC(new_size);
            if (NULL == poll_list)
            {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                PR_Lock(group->ml);
                goto failed_alloc;
            }
            if (NULL != old_polling_list) {
                PR_DELETE(old_polling_list);
            }
            PR_Lock(group->ml);
            if (_prmw_running != group->state)
            {
                PR_DELETE(poll_list);
                PR_SetError(PR_INVALID_STATE_ERROR, 0);
                goto aborted;
            }
            group->polling_list = poll_list;
            group->polling_count = new_count;
        }

        now = PR_IntervalNow();
        polling_interval = max_polling_interval;
        since_last_poll = now - group->last_poll;

        waiter = &group->waiter->recv_wait;
        poll_list = group->polling_list;
        for (count = 0; count < group->waiter->count; ++waiter)
        {
            PR_ASSERT(waiter < &group->waiter->recv_wait
                      + group->waiter->length);
            if (NULL != *waiter)  /* a live one! */
            {
                if ((PR_INTERVAL_NO_TIMEOUT != (*waiter)->timeout)
                    && (since_last_poll >= (*waiter)->timeout)) {
                    _MW_DoneInternal(group, waiter, PR_MW_TIMEOUT);
                }
                else
                {
                    if (PR_INTERVAL_NO_TIMEOUT != (*waiter)->timeout)
                    {
                        (*waiter)->timeout -= since_last_poll;
                        if ((*waiter)->timeout < polling_interval) {
                            polling_interval = (*waiter)->timeout;
                        }
                    }
                    PR_ASSERT(poll_list < group->polling_list
                              + group->polling_count);
                    poll_list->fd = (*waiter)->fd;
                    poll_list->in_flags = PR_POLL_READ;
                    poll_list->out_flags = 0;
#if 0
                    printf(
                        "Polling 0x%x[%d]: [fd: 0x%x, tmo: %u]\n",
                        poll_list, count, poll_list->fd, (*waiter)->timeout);
#endif
                    poll_list += 1;
                    count += 1;
                }
            }
        }

        PR_ASSERT(count == group->waiter->count);

        /*
        ** If there are no more threads waiting for completion,
        ** we need to return.
        */
        if ((!PR_CLIST_IS_EMPTY(&group->io_ready))
            && (1 == group->waiting_threads)) {
            break;
        }

        if (0 == count) {
            continue;    /* wait for new business */
        }

        group->last_poll = now;

        PR_Unlock(group->ml);

        count_ready = PR_Poll(group->polling_list, count, polling_interval);

        PR_Lock(group->ml);

        if (_prmw_running != group->state)
        {
            PR_SetError(PR_INVALID_STATE_ERROR, 0);
            goto aborted;
        }
        if (-1 == count_ready)
        {
            goto failed_poll;  /* that's a shame */
        }
        else if (0 < count_ready)
        {
            for (poll_list = group->polling_list; count > 0;
                 poll_list++, count--)
            {
                PR_ASSERT(
                    poll_list < group->polling_list + group->polling_count);
                if (poll_list->out_flags != 0)
                {
                    waiter = _MW_LookupInternal(group, poll_list->fd);
                    /*
                    ** If 'waiter' is NULL, that means the wait receive
                    ** descriptor has been canceled.
                    */
                    if (NULL != waiter) {
                        _MW_DoneInternal(group, waiter, PR_MW_SUCCESS);
                    }
                }
            }
        }
        /*
        ** If there are no more threads waiting for completion,
        ** we need to return.
        ** This thread was "borrowed" to do the polling, but it really
        ** belongs to the client.
        */
        if ((!PR_CLIST_IS_EMPTY(&group->io_ready))
            && (1 == group->waiting_threads)) {
            break;
        }
    }

    rv = PR_SUCCESS;

aborted:
failed_poll:
failed_alloc:
    group->poller = NULL;  /* we were that, not we ain't */
    if ((_prmw_running == group->state) && (group->waiting_threads > 1))
    {
        /* Wake up one thread to become the new poller. */
        PR_NotifyCondVar(group->io_complete);
    }
    return rv;  /* we return with the lock held */
}  /* _MW_PollInternal */
#endif /* !WINNT */

static PRMWGroupState MW_TestForShutdownInternal(PRWaitGroup *group)
{
    PRMWGroupState rv = group->state;
    /*
    ** Looking at the group's fields is safe because
    ** once the group's state is no longer running, it
    ** cannot revert and there is a safe check on entry
    ** to make sure no more threads are made to wait.
    */
    if ((_prmw_stopping == rv)
        && (0 == group->waiting_threads))
    {
        rv = group->state = _prmw_stopped;
        PR_NotifyCondVar(group->mw_manage);
    }
    return rv;
}  /* MW_TestForShutdownInternal */

#ifndef WINNT
static void _MW_InitialRecv(PRCList *io_ready)
{
    PRRecvWait *desc = (PRRecvWait*)io_ready;
    if ((NULL == desc->buffer.start)
        || (0 == desc->buffer.length)) {
        desc->bytesRecv = 0;
    }
    else
    {
        desc->bytesRecv = (desc->fd->methods->recv)(
                              desc->fd, desc->buffer.start,
                              desc->buffer.length, 0, desc->timeout);
        if (desc->bytesRecv < 0) { /* SetError should already be there */
            desc->outcome = PR_MW_FAILURE;
        }
    }
}  /* _MW_InitialRecv */
#endif

#ifdef WINNT
static void NT_TimeProc(void *arg)
{
    _MDOverlapped *overlapped = (_MDOverlapped *)arg;
    PRRecvWait *desc =  overlapped->data.mw.desc;
    PRFileDesc *bottom;

    if (InterlockedCompareExchange((LONG *)&desc->outcome,
                                   (LONG)PR_MW_TIMEOUT, (LONG)PR_MW_PENDING) != (LONG)PR_MW_PENDING)
    {
        /* This wait recv descriptor has already completed. */
        return;
    }

    /* close the osfd to abort the outstanding async io request */
    /* $$$$
    ** Little late to be checking if NSPR's on the bottom of stack,
    ** but if we don't check, we can't assert that the private data
    ** is what we think it is.
    ** $$$$
    */
    bottom = PR_GetIdentitiesLayer(desc->fd, PR_NSPR_IO_LAYER);
    PR_ASSERT(NULL != bottom);
    if (NULL != bottom)  /* now what!?!?! */
    {
        bottom->secret->state = _PR_FILEDESC_CLOSED;
        if (closesocket(bottom->secret->md.osfd) == SOCKET_ERROR)
        {
            fprintf(stderr, "closesocket failed: %d\n", WSAGetLastError());
            PR_NOT_REACHED("What shall I do?");
        }
    }
    return;
}  /* NT_TimeProc */

static PRStatus NT_HashRemove(PRWaitGroup *group, PRFileDesc *fd)
{
    PRRecvWait **waiter;

    _PR_MD_LOCK(&group->mdlock);
    waiter = _MW_LookupInternal(group, fd);
    if (NULL != waiter)
    {
        group->waiter->count -= 1;
        *waiter = NULL;
    }
    _PR_MD_UNLOCK(&group->mdlock);
    return (NULL != waiter) ? PR_SUCCESS : PR_FAILURE;
}

PRStatus NT_HashRemoveInternal(PRWaitGroup *group, PRFileDesc *fd)
{
    PRRecvWait **waiter;

    waiter = _MW_LookupInternal(group, fd);
    if (NULL != waiter)
    {
        group->waiter->count -= 1;
        *waiter = NULL;
    }
    return (NULL != waiter) ? PR_SUCCESS : PR_FAILURE;
}
#endif /* WINNT */

/******************************************************************/
/******************************************************************/
/********************** The public API portion ********************/
/******************************************************************/
/******************************************************************/
PR_IMPLEMENT(PRStatus) PR_AddWaitFileDesc(
    PRWaitGroup *group, PRRecvWait *desc)
{
    _PR_HashStory hrv;
    PRStatus rv = PR_FAILURE;
#ifdef WINNT
    _MDOverlapped *overlapped;
    HANDLE hFile;
    BOOL bResult;
    DWORD dwError;
    PRFileDesc *bottom;
#endif

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    if ((NULL == group) && (NULL == (group = MW_Init2())))
    {
        return rv;
    }

    PR_ASSERT(NULL != desc->fd);

    desc->outcome = PR_MW_PENDING;  /* nice, well known value */
    desc->bytesRecv = 0;  /* likewise, though this value is ambiguious */

    PR_Lock(group->ml);

    if (_prmw_running != group->state)
    {
        /* Not allowed to add after cancelling the group */
        desc->outcome = PR_MW_INTERRUPT;
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        PR_Unlock(group->ml);
        return rv;
    }

#ifdef WINNT
    _PR_MD_LOCK(&group->mdlock);
#endif

    /*
    ** If the waiter count is zero at this point, there's no telling
    ** how long we've been idle. Therefore, initialize the beginning
    ** of the timing interval. As long as the list doesn't go empty,
    ** it will maintain itself.
    */
    if (0 == group->waiter->count) {
        group->last_poll = PR_IntervalNow();
    }

    do
    {
        hrv = MW_AddHashInternal(desc, group->waiter);
        if (_prmw_rehash != hrv) {
            break;
        }
        hrv = MW_ExpandHashInternal(group);  /* gruesome */
        if (_prmw_success != hrv) {
            break;
        }
    } while (PR_TRUE);

#ifdef WINNT
    _PR_MD_UNLOCK(&group->mdlock);
#endif

    PR_NotifyCondVar(group->new_business);  /* tell the world */
    rv = (_prmw_success == hrv) ? PR_SUCCESS : PR_FAILURE;
    PR_Unlock(group->ml);

#ifdef WINNT
    overlapped = PR_NEWZAP(_MDOverlapped);
    if (NULL == overlapped)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        NT_HashRemove(group, desc->fd);
        return rv;
    }
    overlapped->ioModel = _MD_MultiWaitIO;
    overlapped->data.mw.desc = desc;
    overlapped->data.mw.group = group;
    if (desc->timeout != PR_INTERVAL_NO_TIMEOUT)
    {
        overlapped->data.mw.timer = CreateTimer(
                                        desc->timeout,
                                        NT_TimeProc,
                                        overlapped);
        if (0 == overlapped->data.mw.timer)
        {
            NT_HashRemove(group, desc->fd);
            PR_DELETE(overlapped);
            /*
             * XXX It appears that a maximum of 16 timer events can
             * be outstanding. GetLastError() returns 0 when I try it.
             */
            PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, GetLastError());
            return PR_FAILURE;
        }
    }

    /* Reach to the bottom layer to get the OS fd */
    bottom = PR_GetIdentitiesLayer(desc->fd, PR_NSPR_IO_LAYER);
    PR_ASSERT(NULL != bottom);
    if (NULL == bottom)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    hFile = (HANDLE)bottom->secret->md.osfd;
    if (!bottom->secret->md.io_model_committed)
    {
        PRInt32 st;
        st = _md_Associate(hFile);
        PR_ASSERT(0 != st);
        bottom->secret->md.io_model_committed = PR_TRUE;
    }
    bResult = ReadFile(hFile,
                       desc->buffer.start,
                       (DWORD)desc->buffer.length,
                       NULL,
                       &overlapped->overlapped);
    if (FALSE == bResult && (dwError = GetLastError()) != ERROR_IO_PENDING)
    {
        if (desc->timeout != PR_INTERVAL_NO_TIMEOUT)
        {
            if (InterlockedCompareExchange((LONG *)&desc->outcome,
                                           (LONG)PR_MW_FAILURE, (LONG)PR_MW_PENDING)
                == (LONG)PR_MW_PENDING)
            {
                CancelTimer(overlapped->data.mw.timer);
            }
            NT_HashRemove(group, desc->fd);
            PR_DELETE(overlapped);
        }
        _PR_MD_MAP_READ_ERROR(dwError);
        rv = PR_FAILURE;
    }
#endif

    return rv;
}  /* PR_AddWaitFileDesc */

PR_IMPLEMENT(PRRecvWait*) PR_WaitRecvReady(PRWaitGroup *group)
{
    PRCList *io_ready = NULL;
#ifdef WINNT
    PRThread *me = _PR_MD_CURRENT_THREAD();
    _MDOverlapped *overlapped;
#endif

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }
    if ((NULL == group) && (NULL == (group = MW_Init2()))) {
        goto failed_init;
    }

    PR_Lock(group->ml);

    if (_prmw_running != group->state)
    {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        goto invalid_state;
    }

    group->waiting_threads += 1;  /* the polling thread is counted */

#ifdef WINNT
    _PR_MD_LOCK(&group->mdlock);
    while (PR_CLIST_IS_EMPTY(&group->io_ready))
    {
        _PR_THREAD_LOCK(me);
        me->state = _PR_IO_WAIT;
        PR_APPEND_LINK(&me->waitQLinks, &group->wait_list);
        if (!_PR_IS_NATIVE_THREAD(me))
        {
            _PR_SLEEPQ_LOCK(me->cpu);
            _PR_ADD_SLEEPQ(me, PR_INTERVAL_NO_TIMEOUT);
            _PR_SLEEPQ_UNLOCK(me->cpu);
        }
        _PR_THREAD_UNLOCK(me);
        _PR_MD_UNLOCK(&group->mdlock);
        PR_Unlock(group->ml);
        _PR_MD_WAIT(me, PR_INTERVAL_NO_TIMEOUT);
        me->state = _PR_RUNNING;
        PR_Lock(group->ml);
        _PR_MD_LOCK(&group->mdlock);
        if (_PR_PENDING_INTERRUPT(me)) {
            PR_REMOVE_LINK(&me->waitQLinks);
            _PR_MD_UNLOCK(&group->mdlock);
            me->flags &= ~_PR_INTERRUPT;
            me->io_suspended = PR_FALSE;
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
            goto aborted;
        }
    }
    io_ready = PR_LIST_HEAD(&group->io_ready);
    PR_ASSERT(io_ready != NULL);
    PR_REMOVE_LINK(io_ready);
    _PR_MD_UNLOCK(&group->mdlock);
    overlapped = (_MDOverlapped *)
                 ((char *)io_ready - offsetof(_MDOverlapped, data));
    io_ready = &overlapped->data.mw.desc->internal;
#else
    do
    {
        /*
        ** If the I/O ready list isn't empty, have this thread
        ** return with the first receive wait object that's available.
        */
        if (PR_CLIST_IS_EMPTY(&group->io_ready))
        {
            /*
            ** Is there a polling thread yet? If not, grab this thread
            ** and use it.
            */
            if (NULL == group->poller)
            {
                /*
                ** This thread will stay do polling until it becomes the only one
                ** left to service a completion. Then it will return and there will
                ** be none left to actually poll or to run completions.
                **
                ** The polling function should only return w/ failure or
                ** with some I/O ready.
                */
                if (PR_FAILURE == _MW_PollInternal(group)) {
                    goto failed_poll;
                }
            }
            else
            {
                /*
                ** There are four reasons a thread can be awakened from
                ** a wait on the io_complete condition variable.
                ** 1. Some I/O has completed, i.e., the io_ready list
                **    is nonempty.
                ** 2. The wait group is canceled.
                ** 3. The thread is interrupted.
                ** 4. The current polling thread has to leave and needs
                **    a replacement.
                ** The logic to find a new polling thread is made more
                ** complicated by all the other possible events.
                ** I tried my best to write the logic clearly, but
                ** it is still full of if's with continue and goto.
                */
                PRStatus st;
                do
                {
                    st = PR_WaitCondVar(group->io_complete, PR_INTERVAL_NO_TIMEOUT);
                    if (_prmw_running != group->state)
                    {
                        PR_SetError(PR_INVALID_STATE_ERROR, 0);
                        goto aborted;
                    }
                    if (_MW_ABORTED(st) || (NULL == group->poller)) {
                        break;
                    }
                } while (PR_CLIST_IS_EMPTY(&group->io_ready));

                /*
                ** The thread is interrupted and has to leave.  It might
                ** have also been awakened to process ready i/o or be the
                ** new poller.  To be safe, if either condition is true,
                ** we awaken another thread to take its place.
                */
                if (_MW_ABORTED(st))
                {
                    if ((NULL == group->poller
                         || !PR_CLIST_IS_EMPTY(&group->io_ready))
                        && group->waiting_threads > 1) {
                        PR_NotifyCondVar(group->io_complete);
                    }
                    goto aborted;
                }

                /*
                ** A new poller is needed, but can I be the new poller?
                ** If there is no i/o ready, sure.  But if there is any
                ** i/o ready, it has a higher priority.  I want to
                ** process the ready i/o first and wake up another
                ** thread to be the new poller.
                */
                if (NULL == group->poller)
                {
                    if (PR_CLIST_IS_EMPTY(&group->io_ready)) {
                        continue;
                    }
                    if (group->waiting_threads > 1) {
                        PR_NotifyCondVar(group->io_complete);
                    }
                }
            }
            PR_ASSERT(!PR_CLIST_IS_EMPTY(&group->io_ready));
        }
        io_ready = PR_LIST_HEAD(&group->io_ready);
        PR_NotifyCondVar(group->io_taken);
        PR_ASSERT(io_ready != NULL);
        PR_REMOVE_LINK(io_ready);
    } while (NULL == io_ready);

failed_poll:

#endif

aborted:

    group->waiting_threads -= 1;
invalid_state:
    (void)MW_TestForShutdownInternal(group);
    PR_Unlock(group->ml);

failed_init:
    if (NULL != io_ready)
    {
        /* If the operation failed, record the reason why */
        switch (((PRRecvWait*)io_ready)->outcome)
        {
            case PR_MW_PENDING:
                PR_ASSERT(0);
                break;
            case PR_MW_SUCCESS:
#ifndef WINNT
                _MW_InitialRecv(io_ready);
#endif
                break;
#ifdef WINNT
            case PR_MW_FAILURE:
                _PR_MD_MAP_READ_ERROR(overlapped->data.mw.error);
                break;
#endif
            case PR_MW_TIMEOUT:
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                break;
            case PR_MW_INTERRUPT:
                PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                break;
            default: break;
        }
#ifdef WINNT
        if (NULL != overlapped->data.mw.timer)
        {
            PR_ASSERT(PR_INTERVAL_NO_TIMEOUT
                      != overlapped->data.mw.desc->timeout);
            CancelTimer(overlapped->data.mw.timer);
        }
        else
        {
            PR_ASSERT(PR_INTERVAL_NO_TIMEOUT
                      == overlapped->data.mw.desc->timeout);
        }
        PR_DELETE(overlapped);
#endif
    }
    return (PRRecvWait*)io_ready;
}  /* PR_WaitRecvReady */

PR_IMPLEMENT(PRStatus) PR_CancelWaitFileDesc(PRWaitGroup *group, PRRecvWait *desc)
{
#if !defined(WINNT)
    PRRecvWait **recv_wait;
#endif
    PRStatus rv = PR_SUCCESS;
    if (NULL == group) {
        group = mw_state->group;
    }
    PR_ASSERT(NULL != group);
    if (NULL == group)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_Lock(group->ml);

    if (_prmw_running != group->state)
    {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        rv = PR_FAILURE;
        goto unlock;
    }

#ifdef WINNT
    if (InterlockedCompareExchange((LONG *)&desc->outcome,
                                   (LONG)PR_MW_INTERRUPT, (LONG)PR_MW_PENDING) == (LONG)PR_MW_PENDING)
    {
        PRFileDesc *bottom = PR_GetIdentitiesLayer(desc->fd, PR_NSPR_IO_LAYER);
        PR_ASSERT(NULL != bottom);
        if (NULL == bottom)
        {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            goto unlock;
        }
        bottom->secret->state = _PR_FILEDESC_CLOSED;
#if 0
        fprintf(stderr, "cancel wait recv: closing socket\n");
#endif
        if (closesocket(bottom->secret->md.osfd) == SOCKET_ERROR)
        {
            fprintf(stderr, "closesocket failed: %d\n", WSAGetLastError());
            exit(1);
        }
    }
#else
    if (NULL != (recv_wait = _MW_LookupInternal(group, desc->fd)))
    {
        /* it was in the wait table */
        _MW_DoneInternal(group, recv_wait, PR_MW_INTERRUPT);
        goto unlock;
    }
    if (!PR_CLIST_IS_EMPTY(&group->io_ready))
    {
        /* is it already complete? */
        PRCList *head = PR_LIST_HEAD(&group->io_ready);
        do
        {
            PRRecvWait *done = (PRRecvWait*)head;
            if (done == desc) {
                goto unlock;
            }
            head = PR_NEXT_LINK(head);
        } while (head != &group->io_ready);
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    rv = PR_FAILURE;

#endif
unlock:
    PR_Unlock(group->ml);
    return rv;
}  /* PR_CancelWaitFileDesc */

PR_IMPLEMENT(PRRecvWait*) PR_CancelWaitGroup(PRWaitGroup *group)
{
    PRRecvWait **desc;
    PRRecvWait *recv_wait = NULL;
#ifdef WINNT
    _MDOverlapped *overlapped;
    PRRecvWait **end;
    PRThread *me = _PR_MD_CURRENT_THREAD();
#endif

    if (NULL == group) {
        group = mw_state->group;
    }
    PR_ASSERT(NULL != group);
    if (NULL == group)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    PR_Lock(group->ml);
    if (_prmw_stopped != group->state)
    {
        if (_prmw_running == group->state) {
            group->state = _prmw_stopping;    /* so nothing new comes in */
        }
        if (0 == group->waiting_threads) { /* is there anybody else? */
            group->state = _prmw_stopped;    /* we can stop right now */
        }
        else
        {
            PR_NotifyAllCondVar(group->new_business);
            PR_NotifyAllCondVar(group->io_complete);
        }
        while (_prmw_stopped != group->state) {
            (void)PR_WaitCondVar(group->mw_manage, PR_INTERVAL_NO_TIMEOUT);
        }
    }

#ifdef WINNT
    _PR_MD_LOCK(&group->mdlock);
#endif
    /* make all the existing descriptors look done/interrupted */
#ifdef WINNT
    end = &group->waiter->recv_wait + group->waiter->length;
    for (desc = &group->waiter->recv_wait; desc < end; ++desc)
    {
        if (NULL != *desc)
        {
            if (InterlockedCompareExchange((LONG *)&(*desc)->outcome,
                                           (LONG)PR_MW_INTERRUPT, (LONG)PR_MW_PENDING)
                == (LONG)PR_MW_PENDING)
            {
                PRFileDesc *bottom = PR_GetIdentitiesLayer(
                                         (*desc)->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);
                if (NULL == bottom)
                {
                    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
                    goto invalid_arg;
                }
                bottom->secret->state = _PR_FILEDESC_CLOSED;
#if 0
                fprintf(stderr, "cancel wait group: closing socket\n");
#endif
                if (closesocket(bottom->secret->md.osfd) == SOCKET_ERROR)
                {
                    fprintf(stderr, "closesocket failed: %d\n",
                            WSAGetLastError());
                    exit(1);
                }
            }
        }
    }
    while (group->waiter->count > 0)
    {
        _PR_THREAD_LOCK(me);
        me->state = _PR_IO_WAIT;
        PR_APPEND_LINK(&me->waitQLinks, &group->wait_list);
        if (!_PR_IS_NATIVE_THREAD(me))
        {
            _PR_SLEEPQ_LOCK(me->cpu);
            _PR_ADD_SLEEPQ(me, PR_INTERVAL_NO_TIMEOUT);
            _PR_SLEEPQ_UNLOCK(me->cpu);
        }
        _PR_THREAD_UNLOCK(me);
        _PR_MD_UNLOCK(&group->mdlock);
        PR_Unlock(group->ml);
        _PR_MD_WAIT(me, PR_INTERVAL_NO_TIMEOUT);
        me->state = _PR_RUNNING;
        PR_Lock(group->ml);
        _PR_MD_LOCK(&group->mdlock);
    }
#else
    for (desc = &group->waiter->recv_wait; group->waiter->count > 0; ++desc)
    {
        PR_ASSERT(desc < &group->waiter->recv_wait + group->waiter->length);
        if (NULL != *desc) {
            _MW_DoneInternal(group, desc, PR_MW_INTERRUPT);
        }
    }
#endif

    /* take first element of finished list and return it or NULL */
    if (PR_CLIST_IS_EMPTY(&group->io_ready)) {
        PR_SetError(PR_GROUP_EMPTY_ERROR, 0);
    }
    else
    {
        PRCList *head = PR_LIST_HEAD(&group->io_ready);
        PR_REMOVE_AND_INIT_LINK(head);
#ifdef WINNT
        overlapped = (_MDOverlapped *)
                     ((char *)head - offsetof(_MDOverlapped, data));
        head = &overlapped->data.mw.desc->internal;
        if (NULL != overlapped->data.mw.timer)
        {
            PR_ASSERT(PR_INTERVAL_NO_TIMEOUT
                      != overlapped->data.mw.desc->timeout);
            CancelTimer(overlapped->data.mw.timer);
        }
        else
        {
            PR_ASSERT(PR_INTERVAL_NO_TIMEOUT
                      == overlapped->data.mw.desc->timeout);
        }
        PR_DELETE(overlapped);
#endif
        recv_wait = (PRRecvWait*)head;
    }
#ifdef WINNT
invalid_arg:
    _PR_MD_UNLOCK(&group->mdlock);
#endif
    PR_Unlock(group->ml);

    return recv_wait;
}  /* PR_CancelWaitGroup */

PR_IMPLEMENT(PRWaitGroup*) PR_CreateWaitGroup(PRInt32 size /* ignored */)
{
    PRWaitGroup *wg;

    if (NULL == (wg = PR_NEWZAP(PRWaitGroup)))
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto failed;
    }
    /* the wait group itself */
    wg->ml = PR_NewLock();
    if (NULL == wg->ml) {
        goto failed_lock;
    }
    wg->io_taken = PR_NewCondVar(wg->ml);
    if (NULL == wg->io_taken) {
        goto failed_cvar0;
    }
    wg->io_complete = PR_NewCondVar(wg->ml);
    if (NULL == wg->io_complete) {
        goto failed_cvar1;
    }
    wg->new_business = PR_NewCondVar(wg->ml);
    if (NULL == wg->new_business) {
        goto failed_cvar2;
    }
    wg->mw_manage = PR_NewCondVar(wg->ml);
    if (NULL == wg->mw_manage) {
        goto failed_cvar3;
    }

    PR_INIT_CLIST(&wg->group_link);
    PR_INIT_CLIST(&wg->io_ready);

    /* the waiters sequence */
    wg->waiter = (_PRWaiterHash*)PR_CALLOC(
                     sizeof(_PRWaiterHash) +
                     (_PR_DEFAULT_HASH_LENGTH * sizeof(PRRecvWait*)));
    if (NULL == wg->waiter)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        goto failed_waiter;
    }
    wg->waiter->count = 0;
    wg->waiter->length = _PR_DEFAULT_HASH_LENGTH;

#ifdef WINNT
    _PR_MD_NEW_LOCK(&wg->mdlock);
    PR_INIT_CLIST(&wg->wait_list);
#endif /* WINNT */

    PR_Lock(mw_lock);
    PR_APPEND_LINK(&wg->group_link, &mw_state->group_list);
    PR_Unlock(mw_lock);
    return wg;

failed_waiter:
    PR_DestroyCondVar(wg->mw_manage);
failed_cvar3:
    PR_DestroyCondVar(wg->new_business);
failed_cvar2:
    PR_DestroyCondVar(wg->io_complete);
failed_cvar1:
    PR_DestroyCondVar(wg->io_taken);
failed_cvar0:
    PR_DestroyLock(wg->ml);
failed_lock:
    PR_DELETE(wg);
    wg = NULL;

failed:
    return wg;
}  /* MW_CreateWaitGroup */

PR_IMPLEMENT(PRStatus) PR_DestroyWaitGroup(PRWaitGroup *group)
{
    PRStatus rv = PR_SUCCESS;
    if (NULL == group) {
        group = mw_state->group;
    }
    PR_ASSERT(NULL != group);
    if (NULL != group)
    {
        PR_Lock(group->ml);
        if ((group->waiting_threads == 0)
            && (group->waiter->count == 0)
            && PR_CLIST_IS_EMPTY(&group->io_ready))
        {
            group->state = _prmw_stopped;
        }
        else
        {
            PR_SetError(PR_INVALID_STATE_ERROR, 0);
            rv = PR_FAILURE;
        }
        PR_Unlock(group->ml);
        if (PR_FAILURE == rv) {
            return rv;
        }

        PR_Lock(mw_lock);
        PR_REMOVE_LINK(&group->group_link);
        PR_Unlock(mw_lock);

#ifdef WINNT
        /*
         * XXX make sure wait_list is empty and waiter is empty.
         * These must be checked while holding mdlock.
         */
        _PR_MD_FREE_LOCK(&group->mdlock);
#endif

        PR_DELETE(group->waiter);
        PR_DELETE(group->polling_list);
        PR_DestroyCondVar(group->mw_manage);
        PR_DestroyCondVar(group->new_business);
        PR_DestroyCondVar(group->io_complete);
        PR_DestroyCondVar(group->io_taken);
        PR_DestroyLock(group->ml);
        if (group == mw_state->group) {
            mw_state->group = NULL;
        }
        PR_DELETE(group);
    }
    else
    {
        /* The default wait group is not created yet. */
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        rv = PR_FAILURE;
    }
    return rv;
}  /* PR_DestroyWaitGroup */

/**********************************************************************
***********************************************************************
******************** Wait group enumerations **************************
***********************************************************************
**********************************************************************/

PR_IMPLEMENT(PRMWaitEnumerator*) PR_CreateMWaitEnumerator(PRWaitGroup *group)
{
    PRMWaitEnumerator *enumerator = PR_NEWZAP(PRMWaitEnumerator);
    if (NULL == enumerator) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    else
    {
        enumerator->group = group;
        enumerator->seal = _PR_ENUM_SEALED;
    }
    return enumerator;
}  /* PR_CreateMWaitEnumerator */

PR_IMPLEMENT(PRStatus) PR_DestroyMWaitEnumerator(PRMWaitEnumerator* enumerator)
{
    PR_ASSERT(NULL != enumerator);
    PR_ASSERT(_PR_ENUM_SEALED == enumerator->seal);
    if ((NULL == enumerator) || (_PR_ENUM_SEALED != enumerator->seal))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    enumerator->seal = _PR_ENUM_UNSEALED;
    PR_Free(enumerator);
    return PR_SUCCESS;
}  /* PR_DestroyMWaitEnumerator */

PR_IMPLEMENT(PRRecvWait*) PR_EnumerateWaitGroup(
    PRMWaitEnumerator *enumerator, const PRRecvWait *previous)
{
    PRRecvWait *result = NULL;

    /* entry point sanity checking */
    PR_ASSERT(NULL != enumerator);
    PR_ASSERT(_PR_ENUM_SEALED == enumerator->seal);
    if ((NULL == enumerator)
        || (_PR_ENUM_SEALED != enumerator->seal)) {
        goto bad_argument;
    }

    /* beginning of enumeration */
    if (NULL == previous)
    {
        if (NULL == enumerator->group)
        {
            enumerator->group = mw_state->group;
            if (NULL == enumerator->group)
            {
                PR_SetError(PR_GROUP_EMPTY_ERROR, 0);
                return NULL;
            }
        }
        enumerator->waiter = &enumerator->group->waiter->recv_wait;
        enumerator->p_timestamp = enumerator->group->p_timestamp;
        enumerator->thread = PR_GetCurrentThread();
        enumerator->index = 0;
    }
    /* continuing an enumeration */
    else
    {
        PRThread *me = PR_GetCurrentThread();
        PR_ASSERT(me == enumerator->thread);
        if (me != enumerator->thread) {
            goto bad_argument;
        }

        /* need to restart the enumeration */
        if (enumerator->p_timestamp != enumerator->group->p_timestamp) {
            return PR_EnumerateWaitGroup(enumerator, NULL);
        }
    }

    /* actually progress the enumeration */
#if defined(WINNT)
    _PR_MD_LOCK(&enumerator->group->mdlock);
#else
    PR_Lock(enumerator->group->ml);
#endif
    while (enumerator->index++ < enumerator->group->waiter->length)
    {
        if (NULL != (result = *(enumerator->waiter)++)) {
            break;
        }
    }
#if defined(WINNT)
    _PR_MD_UNLOCK(&enumerator->group->mdlock);
#else
    PR_Unlock(enumerator->group->ml);
#endif

    return result;  /* what we live for */

bad_argument:
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return NULL;  /* probably ambiguous */
}  /* PR_EnumerateWaitGroup */

/* prmwait.c */
