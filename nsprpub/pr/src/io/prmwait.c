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

#include "prlog.h"
#include "prmem.h"
#include "primpl.h"
#include "prmwait.h"
#include "prerror.h"
#include "pprmwait.h"

static PRLock *mw_lock = NULL;
static _PRGlobalState *mw_state = NULL;

static PRIntervalTime max_polling_interval;

/******************************************************************/
/******************************************************************/
/************************ The private portion *********************/
/******************************************************************/
/******************************************************************/
static PRStatus MW_Init(void)
{
    if (NULL != mw_lock) return PR_SUCCESS;
    if (NULL != (mw_lock = PR_NewLock()))
    {
        _PRGlobalState *state = PR_NEWZAP(_PRGlobalState);
        if (state == NULL) goto failed;

        PR_INIT_CLIST(&state->group_list);

        PR_Lock(mw_lock);
        if (NULL == mw_state)  /* is it still NULL? */
        {
            mw_state = state;  /* if yes, set our value */
            state = NULL;  /* and indicate we've done the job */
            max_polling_interval = PR_MillisecondsToInterval(MAX_POLLING_INTERVAL);
        }
        PR_Unlock(mw_lock);
        if (NULL != state) PR_DELETE(state);
        return PR_SUCCESS;
    }

failed:
    return PR_FAILURE;
}  /* MW_Init */

static PRWaitGroup *MW_Init2(void)
{
    PRWaitGroup *group = mw_state->group;  /* it's the null group */
    if (NULL == group)  /* there is this special case */
    {
        group = PR_CreateWaitGroup(_PR_DEFAULT_HASH_LENGTH);
        if (NULL == group) goto failed_alloc;
        PR_Lock(mw_lock);
        if (NULL == mw_state->group)
        {
            mw_state->group = group;
            group = NULL;
        }
        PR_Unlock(mw_lock);
        if (group != NULL) (void)PR_DestroyWaitGroup(group);
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
    ** We try to put the entry in by rehashing three times. After
    ** that we declare defeat and force the table to be reconstructed.
    ** Since some fds might be added more than once, won't that cause
    ** collisions even in an empty table?
    */
    PRIntn rehash = 11;
    PRRecvWait **waiter;
    PRUintn hidx = _MW_HASH(desc->fd, hash->length);
    do
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
        hidx = _MW_REHASH(desc->fd, hidx, hash->length);
    } while (--rehash > 0);
    return _prmw_rehash;    
}  /* MW_AddHashInternal */

static _PR_HashStory MW_ExpandHashInternal(PRWaitGroup *group)
{
    PRRecvWait **desc;
    PRUint32 pidx, length = 0;
    _PRWaiterHash *newHash, *oldHash = group->waiter;
    

    static const PRInt32 prime_number[] = {
        _PR_DEFAULT_HASH_LENGTH, 179, 521, 907, 1427,
        2711, 3917, 5021, 8219, 11549, 18911, 26711, 33749, 44771};
    PRUintn primes = (sizeof(prime_number) / sizeof(PRIntn));

    /* look up the next size we'd like to use for the hash table */
    for (pidx = 0; pidx < primes; ++pidx)
    {
        if (prime_number[pidx] == oldHash->length)
        {
            length = prime_number[pidx + 1];
            break;
        }
    }
    if (0 == length)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return _prmw_error;  /* we're hosed */
    }

    /* allocate the new hash table and fill it in with the old */
    newHash = (_PRWaiterHash*)PR_CALLOC(
        sizeof(_PRWaiterHash) + (length * sizeof(PRRecvWait*)));

    newHash->length = length;
    for (desc = &oldHash->recv_wait; newHash->count < oldHash->count; ++desc)
    {
        if (NULL != *desc)
        {
            if (_prmw_success != MW_AddHashInternal(*desc, newHash))
            {
                PR_ASSERT(!"But, but, but ...");
                PR_DELETE(newHash);
                return _prmw_error;
            }
        }
    }
    PR_DELETE(group->waiter);
    group->waiter = newHash;
    return _prmw_success;
}  /* MW_ExpandHashInternal */

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

static PRRecvWait **_MW_LookupInternal(PRWaitGroup *group, PRFileDesc *fd)
{
    /*
    ** Find the receive wait object corresponding to the file descriptor.
    ** Only search the wait group specified.
    */
    PRRecvWait **desc;
    PRIntn rehash = 11;
    _PRWaiterHash *hash = group->waiter;
    PRUintn hidx = _MW_HASH(fd, hash->length);
    
    while (rehash-- > 0)
    {
        desc = (&hash->recv_wait) + hidx;
        if ((*desc != NULL) && ((*desc)->fd == fd)) return desc;
        hidx = _MW_REHASH(fd, hidx, hash->length);
    }
    return NULL;
}  /* _MW_LookupInternal */

static PRStatus _MW_PollInternal(PRWaitGroup *group)
{
    PRRecvWait **waiter;
    PRStatus rv = PR_FAILURE;
    PRUintn count, count_ready;
    PRIntervalTime polling_interval;

    group->poller = PR_GetCurrentThread();

    PR_Unlock(group->ml);

    while (PR_TRUE)
    {
        PRIntervalTime now, since_last_poll;
        PRPollDesc *poll_list = group->polling_list;
        /*
        ** There's something to do. See if our existing polling list
        ** is large enough for what we have to do?
        */

        while (group->polling_count < group->waiter->count)
        {
            PRUint32 old_count = group->waiter->count;
            PRUint32 new_count = PR_ROUNDUP(old_count, _PR_POLL_COUNT_FUDGE);
            PRSize new_size = sizeof(PRPollDesc) * new_count;
            poll_list = (PRPollDesc*)PR_CALLOC(new_size);
            if (NULL == poll_list) goto failed_alloc;
            if (NULL != group->polling_list)
                PR_DELETE(group->polling_list);
            group->polling_list = poll_list;
            group->polling_count = new_count;
        }

        now = PR_IntervalNow();
        polling_interval = max_polling_interval;
        since_last_poll = now - group->last_poll;
        PR_Lock(group->ml);
        waiter = &group->waiter->recv_wait;

        for (count = 0; count < group->waiter->count; ++waiter)
        {
            if (NULL != *waiter)  /* a live one! */
            {
                if (since_last_poll >= (*waiter)->timeout)
                    _MW_DoneInternal(group, waiter, PR_MW_TIMEOUT);
                else
                {
                    if (PR_INTERVAL_NO_TIMEOUT != (*waiter)->timeout)
                    {
                        (*waiter)->timeout -= since_last_poll;
                        if ((*waiter)->timeout < polling_interval)
                            polling_interval = (*waiter)->timeout;
                    }
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
        if (0 == count) break;

        group->last_poll = now;

        PR_Unlock(group->ml);

        count_ready = PR_Poll(group->polling_list, count, polling_interval);

        PR_Lock(group->ml);

        if (-1 == count_ready) goto failed_poll;  /* that's a shame */
        for (poll_list = group->polling_list; count > 0; poll_list++, count--)
        {
            if (poll_list->out_flags != 0)
            {
                waiter = _MW_LookupInternal(group, poll_list->fd);
                if (NULL != waiter)
                    _MW_DoneInternal(group, waiter, PR_MW_SUCCESS);
            }
        }
        /*
        ** If there are no more threads waiting for completion,
        ** we need to return.
        ** This thread was "borrowed" to do the polling, but it really
        ** belongs to the client.
        */
        if ((_prmw_running != group->state)
        || (0 == group->waiting_threads)) break;
        PR_Unlock(group->ml);
    }

    rv = PR_SUCCESS;

failed_poll:
failed_alloc:
    group->poller = NULL;  /* we were that, not we ain't */
    return rv;  /* we return with the lock held */
}  /* _MW_PollInternal */

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
    && (0 == group->waiting_threads)
    && PR_CLIST_IS_EMPTY(&group->io_ready)
    && (0 == group->waiter->count))
    {
        rv = group->state = _prmw_stopped;
        PR_NotifyCondVar(group->mw_manage);
    }
    return rv;
}  /* MW_TestForShutdownInternal */

static void _MW_InitialRecv(PRCList *io_ready)
{
    PRRecvWait *desc = (PRRecvWait*)io_ready;
    if ((NULL == desc->buffer.start)
    || (0 == desc->buffer.length))
        desc->bytesRecv = 0;
    else
    {
        desc->bytesRecv = desc->fd->methods->recv(
            desc->fd, desc->buffer.start,
            desc->buffer.length, 0, desc->timeout);
        if (desc->bytesRecv < 0)  /* SetError should already be there */
            desc->outcome = PR_MW_FAILURE;
    }
}  /* _MW_InitialRecv */

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
    if (PR_FAILURE == MW_Init()) goto failed_init;
    if ((NULL == group) && (NULL == (group = MW_Init2()))) goto failed_init;

    PR_ASSERT(NULL != desc->fd);

    desc->outcome = PR_MW_PENDING;  /* nice, well known value */
    desc->bytesRecv = 0;  /* likewise, though this value is ambiguious */

    PR_Lock(group->ml);

    if (_prmw_running != group->state)
    {
        /* Not allowed to add after cancelling the group */
        desc->outcome = PR_MW_INTERRUPT;
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        goto invalid_state;
    }

    /*
    ** If the waiter count is zero at this point, there's no telling
    ** how long we've been idle. Therefore, initialize the beginning
    ** of the timing interval. As long as the list doesn't go empty,
    ** it will maintain itself.
    */
    if (0 == group->waiter->count)
        group->last_poll = PR_IntervalNow();
    do
    {
        hrv = MW_AddHashInternal(desc, group->waiter);
        if (_prmw_rehash != hrv) break;
        hrv = MW_ExpandHashInternal(group);  /* gruesome */
        if (_prmw_success != hrv) break;
    } while (PR_TRUE);

    PR_NotifyCondVar(group->new_business);  /* tell the world */
    rv = (_prmw_success == hrv) ? PR_SUCCESS : PR_FAILURE;
    
failed_init:
invalid_state:
    PR_Unlock(group->ml);
    return rv;
}  /* PR_AddWaitFileDesc */

PR_IMPLEMENT(PRRecvWait*) PR_WaitRecvReady(PRWaitGroup *group)
{
    PRStatus rv = PR_SUCCESS;
    PRCList *io_ready = NULL;
    if (PR_FAILURE == MW_Init()) goto failed_init;
    if ((NULL == group) && (NULL == (group = MW_Init2()))) goto failed_init;

    PR_Lock(group->ml);

    if (_prmw_running != group->state)
    {
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        goto invalid_state;
    }

    group->waiting_threads += 1;  /* the polling thread is counted */

    do
    {
        /*
        ** If the I/O ready list isn't empty, have this thread
        ** return with the first receive wait object that's available.
        */
        if (PR_CLIST_IS_EMPTY(&group->io_ready))
        {
            while ((NULL == group->waiter) || (0 == group->waiter->count))
            {
                if (_prmw_running != group->state) goto aborted;
                rv = PR_WaitCondVar(group->new_business, PR_INTERVAL_NO_TIMEOUT);
                if (_MW_ABORTED(rv)) goto aborted;
            }

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
                if (PR_FAILURE == _MW_PollInternal(group)) goto failed_poll;
                if (PR_CLIST_IS_EMPTY(&group->io_ready)) continue;  /* timeout */
            }
            else
            {
                while (PR_CLIST_IS_EMPTY(&group->io_ready))
                {
                    rv = PR_WaitCondVar(group->io_complete, PR_INTERVAL_NO_TIMEOUT);
                    if (_MW_ABORTED(rv)) goto aborted;
                }
            }
        }
        io_ready = PR_LIST_HEAD(&group->io_ready);
        PR_NotifyCondVar(group->io_taken);
        PR_ASSERT(io_ready != NULL);
        PR_REMOVE_LINK(io_ready);

        /* If the operation failed, record the reason why */
        switch (((PRRecvWait*)io_ready)->outcome)
        {
            case PR_MW_PENDING:
                PR_ASSERT(PR_MW_PENDING != ((PRRecvWait*)io_ready)->outcome);
                break;
            case PR_MW_SUCCESS:
                _MW_InitialRecv(io_ready);
                break;
            case PR_MW_TIMEOUT:
                PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
                break;
            case PR_MW_INTERRUPT:
                PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
                break;
            default: break;
        }
    } while (NULL == io_ready);

aborted:
failed_poll:
    group->waiting_threads -= 1;
invalid_state:
    (void)MW_TestForShutdownInternal(group);
    PR_Unlock(group->ml);

failed_init:

    return (PRRecvWait*)io_ready;
}  /* PR_WaitRecvReady */

PR_IMPLEMENT(PRStatus) PR_CancelWaitFileDesc(PRWaitGroup *group, PRRecvWait *desc)
{
    PRRecvWait **recv_wait;
    PRStatus rv = PR_SUCCESS;
    if (PR_FAILURE == MW_Init()) return rv;
    if (NULL == group) group = mw_state->group;
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
        goto stopping;
    }

    if (NULL != (recv_wait = _MW_LookupInternal(group, desc->fd)))
    {
        /* it was in the wait table */
        _MW_DoneInternal(group, recv_wait, PR_MW_INTERRUPT);
        goto found;
    }
    if (!PR_CLIST_IS_EMPTY(&group->io_ready))
    {
        /* is it already complete? */
        PRCList *head = PR_LIST_HEAD(&group->io_ready);
        do
        {
            PRRecvWait *done = (PRRecvWait*)head;
            if (done == desc) goto found;
            head = PR_NEXT_LINK(head);
        } while (head != &group->io_ready);
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    rv = PR_FAILURE;

found:
stopping:
    PR_Unlock(group->ml);
    return rv;
}  /* PR_CancelWaitFileDesc */

PR_IMPLEMENT(PRRecvWait*) PR_CancelWaitGroup(PRWaitGroup *group)
{
    PRRecvWait **desc;
    PRRecvWait *recv_wait = NULL;
    if (PR_FAILURE == MW_Init())
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }
    if (NULL == group) group = mw_state->group;
    PR_ASSERT(NULL != group);
    if (NULL == group)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    PR_Lock(group->ml);
    if (_prmw_stopped != group->state)
    {
        if (_prmw_running == group->state)
            group->state = _prmw_stopping;  /* so nothing new comes in */
        if (0 == group->waiting_threads)  /* is there anybody else? */
            group->state = _prmw_stopped;  /* we can stop right now */
        while (_prmw_stopped != group->state)
            (void)PR_WaitCondVar(group->mw_manage, PR_INTERVAL_NO_TIMEOUT);

        /* make all the existing descriptors look done/interrupted */
        for (desc = &group->waiter->recv_wait; group->waiter->count > 0; ++desc)
        {
            if (NULL != *desc)
                _MW_DoneInternal(group, desc, PR_MW_INTERRUPT);
        }

        PR_NotifyAllCondVar(group->new_business);
    }

    /* take first element of finished list and return it or NULL */
    if (PR_CLIST_IS_EMPTY(&group->io_ready))
        PR_SetError(PR_GROUP_EMPTY_ERROR, 0);
    else
    {
        PRCList *head = PR_LIST_HEAD(&group->io_ready);
        PR_REMOVE_AND_INIT_LINK(head);
        recv_wait = (PRRecvWait*)head;
    }
    PR_Unlock(group->ml);

    return recv_wait;
}  /* PR_CancelWaitGroup */

PR_IMPLEMENT(PRWaitGroup*) PR_CreateWaitGroup(PRInt32 size /* ignored */)
{
    PRWaitGroup *wg = NULL;
    if (PR_FAILURE == MW_Init()) goto failed;

    if (NULL == (wg = PR_NEWZAP(PRWaitGroup))) goto failed;
    /* the wait group itself */
    wg->ml = PR_NewLock();
    if (NULL == wg->ml) goto failed_lock;
    wg->io_taken = PR_NewCondVar(wg->ml);
    if (NULL == wg->io_taken) goto failed_cvar0;
    wg->io_complete = PR_NewCondVar(wg->ml);
    if (NULL == wg->io_complete) goto failed_cvar1;
    wg->new_business = PR_NewCondVar(wg->ml);
    if (NULL == wg->new_business) goto failed_cvar2;
    wg->mw_manage = PR_NewCondVar(wg->ml);
    if (NULL == wg->mw_manage) goto failed_cvar3;

    PR_INIT_CLIST(&wg->group_link);
    PR_INIT_CLIST(&wg->io_ready);

    /* the waiters sequence */
    wg->waiter = (_PRWaiterHash*)PR_CALLOC(
        sizeof(_PRWaiterHash) +
        (_PR_DEFAULT_HASH_LENGTH * sizeof(PRRecvWait*)));
    if (NULL == wg->waiter) goto failed_waiter;
    wg->waiter->count = 0;
    wg->waiter->length = _PR_DEFAULT_HASH_LENGTH;

    PR_Lock(mw_lock);
    PR_APPEND_LINK(&wg->group_link, &mw_state->group_list);
    PR_Unlock(mw_lock);
    return wg;

failed_waiter:
    PR_DestroyCondVar(wg->mw_manage);
failed_cvar3:
    PR_DestroyCondVar(wg->new_business);
failed_cvar2:
    PR_DestroyCondVar(wg->io_taken);
failed_cvar1:
    PR_DestroyCondVar(wg->io_complete);
failed_cvar0:
    PR_DestroyLock(wg->ml);
failed_lock:
    PR_DELETE(wg);

failed:
    return wg;
}  /* MW_CreateWaitGroup */

PR_IMPLEMENT(PRStatus) PR_DestroyWaitGroup(PRWaitGroup *group)
{
    PRStatus rv = PR_SUCCESS;
    if (NULL == group) group = mw_state->group;
    PR_ASSERT(NULL != group);
    if (NULL != group)
    {
        if (_prmw_stopped != group->state)  /* quick, unsafe test */
        {
            PRMWGroupState mws;
            /* One shot to correct the situation */
            PR_Lock(group->ml);
            if (group->state < _prmw_stopped)  /* safer test */
                group->state = _prmw_stopping;
            mws = MW_TestForShutdownInternal(group);
            PR_Unlock(group->ml);
            if (_prmw_stopped != mws)  /* quick test again */
            {
                PR_SetError(PR_INVALID_STATE_ERROR, 0);
                return PR_FAILURE;
            }
        }

        PR_Lock(mw_lock);
        PR_REMOVE_LINK(&group->group_link);
        PR_Unlock(mw_lock);

        PR_DELETE(group->waiter);
        PR_DestroyCondVar(group->new_business);
        PR_DestroyCondVar(group->io_complete);
        PR_DestroyCondVar(group->io_taken);
        PR_DestroyLock(group->ml);
        if (group == mw_state->group) mw_state->group = NULL;
        PR_DELETE(group);
    }
    return rv;
}  /* PR_DestroyWaitGroup */

/* prmwait.c */
