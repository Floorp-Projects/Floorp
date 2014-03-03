/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

/************************************************************************/

/*
 * Notifies just get posted to the monitor. The actual notification is done
 * when the monitor is fully exited so that MP systems don't contend for a
 * monitor that they can't enter.
 */
static void _PR_PostNotifyToMonitor(PRMonitor *mon, PRBool broadcast)
{
    PR_ASSERT(mon != NULL);
    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mon);

    /* mon->notifyTimes is protected by the monitor, so we don't need to
     * acquire mon->lock.
     */
    if (broadcast)
        mon->notifyTimes = -1;
    else if (mon->notifyTimes != -1)
        mon->notifyTimes += 1;
}

static void _PR_PostNotifiesFromMonitor(PRCondVar *cv, PRIntn times)
{
    PRStatus rv;

    /*
     * Time to actually notify any waits that were affected while the monitor
     * was entered.
     */
    PR_ASSERT(cv != NULL);
    PR_ASSERT(times != 0);
    if (times == -1) {
        rv = PR_NotifyAllCondVar(cv);
        PR_ASSERT(rv == PR_SUCCESS);
    } else {
        while (times-- > 0) {
            rv = PR_NotifyCondVar(cv);
            PR_ASSERT(rv == PR_SUCCESS);
        }
    }
}

/*
** Create a new monitor.
*/
PR_IMPLEMENT(PRMonitor*) PR_NewMonitor()
{
    PRMonitor *mon;
    PRStatus rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    mon = PR_NEWZAP(PRMonitor);
    if (mon == NULL) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }

    rv = _PR_InitLock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    if (rv != PR_SUCCESS)
        goto error1;

    mon->owner = NULL;

    rv = _PR_InitCondVar(&mon->entryCV, &mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    if (rv != PR_SUCCESS)
        goto error2;

    rv = _PR_InitCondVar(&mon->waitCV, &mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    if (rv != PR_SUCCESS)
        goto error3;

    mon->notifyTimes = 0;
    mon->entryCount = 0;
    mon->name = NULL;
    return mon;

error3:
    _PR_FreeCondVar(&mon->entryCV);
error2:
    _PR_FreeLock(&mon->lock);
error1:
    PR_Free(mon);
    return NULL;
}

PR_IMPLEMENT(PRMonitor*) PR_NewNamedMonitor(const char* name)
{
    PRMonitor* mon = PR_NewMonitor();
    if (mon)
        mon->name = name;
    return mon;
}

/*
** Destroy a monitor. There must be no thread waiting on the monitor's
** condition variable. The caller is responsible for guaranteeing that the
** monitor is no longer in use.
*/
PR_IMPLEMENT(void) PR_DestroyMonitor(PRMonitor *mon)
{
    PR_ASSERT(mon != NULL);
    _PR_FreeCondVar(&mon->waitCV);
    _PR_FreeCondVar(&mon->entryCV);
    _PR_FreeLock(&mon->lock);
#if defined(DEBUG)
    memset(mon, 0xaf, sizeof(PRMonitor));
#endif
    PR_Free(mon);
}

/*
** Enter the lock associated with the monitor.
*/
PR_IMPLEMENT(void) PR_EnterMonitor(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRStatus rv;

    PR_ASSERT(mon != NULL);
    PR_Lock(&mon->lock);
    if (mon->entryCount != 0) {
        if (mon->owner == me)
            goto done;
        while (mon->entryCount != 0) {
            rv = PR_WaitCondVar(&mon->entryCV, PR_INTERVAL_NO_TIMEOUT);
            PR_ASSERT(rv == PR_SUCCESS);
        }
    }
    /* and now I have the monitor */
    PR_ASSERT(mon->notifyTimes == 0);
    PR_ASSERT(mon->owner == NULL);
    mon->owner = me;

done:
    mon->entryCount += 1;
    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
}

/*
** Test and then enter the lock associated with the monitor if it's not
** already entered by some other thread. Return PR_FALSE if some other
** thread owned the lock at the time of the call.
*/
PR_IMPLEMENT(PRBool) PR_TestAndEnterMonitor(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRStatus rv;

    PR_ASSERT(mon != NULL);
    PR_Lock(&mon->lock);
    if (mon->entryCount != 0) {
        if (mon->owner == me)
            goto done;
        rv = PR_Unlock(&mon->lock);
        PR_ASSERT(rv == PR_SUCCESS);
        return PR_FALSE;
    }
    /* and now I have the monitor */
    PR_ASSERT(mon->notifyTimes == 0);
    PR_ASSERT(mon->owner == NULL);
    mon->owner = me;

done:
    mon->entryCount += 1;
    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    return PR_TRUE;
}

/*
** Exit the lock associated with the monitor once.
*/
PR_IMPLEMENT(PRStatus) PR_ExitMonitor(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRStatus rv;

    PR_ASSERT(mon != NULL);
    PR_Lock(&mon->lock);
    /* the entries should be > 0 and we'd better be the owner */
    PR_ASSERT(mon->entryCount > 0);
    PR_ASSERT(mon->owner == me);
    if (mon->entryCount == 0 || mon->owner != me)
    {
        rv = PR_Unlock(&mon->lock);
        PR_ASSERT(rv == PR_SUCCESS);
        return PR_FAILURE;
    }

    mon->entryCount -= 1;  /* reduce by one */
    if (mon->entryCount == 0)
    {
        /* and if it transitioned to zero - notify an entry waiter */
        /* make the owner unknown */
        mon->owner = NULL;
        if (mon->notifyTimes != 0) {
            _PR_PostNotifiesFromMonitor(&mon->waitCV, mon->notifyTimes);
            mon->notifyTimes = 0;
        }
        rv = PR_NotifyCondVar(&mon->entryCV);
        PR_ASSERT(rv == PR_SUCCESS);
    }
    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    return PR_SUCCESS;
}

/*
** Return the number of times that the current thread has entered the
** lock. Returns zero if the current thread has not entered the lock.
*/
PR_IMPLEMENT(PRIntn) PR_GetMonitorEntryCount(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    PRStatus rv;
    PRIntn count = 0;

    PR_Lock(&mon->lock);
    if (mon->owner == me)
        count = mon->entryCount;
    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    return count;
}

PR_IMPLEMENT(void) PR_AssertCurrentThreadInMonitor(PRMonitor *mon)
{
#if defined(DEBUG) || defined(FORCE_PR_ASSERT)
    PRStatus rv;

    PR_Lock(&mon->lock);
    PR_ASSERT(mon->entryCount != 0 &&
              mon->owner == _PR_MD_CURRENT_THREAD());
    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
#endif
}

/*
** Wait for a notify on the condition variable. Sleep for "ticks" amount
** of time (if "tick" is 0 then the sleep is indefinite). While
** the thread is waiting it exits the monitors lock (as if it called
** PR_ExitMonitor as many times as it had called PR_EnterMonitor).  When
** the wait has finished the thread regains control of the monitors lock
** with the same entry count as before the wait began.
**
** The thread waiting on the monitor will be resumed when the monitor is
** notified (assuming the thread is the next in line to receive the
** notify) or when the "ticks" elapses.
**
** Returns PR_FAILURE if the caller has not locked the lock associated
** with the condition variable.
** This routine can return PR_PENDING_INTERRUPT_ERROR if the waiting thread
** has been interrupted.
*/
PR_IMPLEMENT(PRStatus) PR_Wait(PRMonitor *mon, PRIntervalTime ticks)
{
    PRStatus rv;
    PRUint32 saved_entries;
    PRThread *saved_owner;

    PR_ASSERT(mon != NULL);
    PR_Lock(&mon->lock);
    /* the entries better be positive */
    PR_ASSERT(mon->entryCount > 0);
    /* and it better be owned by us */
    PR_ASSERT(mon->owner == _PR_MD_CURRENT_THREAD());  /* XXX return failure */

    /* tuck these away 'till later */
    saved_entries = mon->entryCount;
    mon->entryCount = 0;
    saved_owner = mon->owner;
    mon->owner = NULL;
    /* If we have pending notifies, post them now. */
    if (mon->notifyTimes != 0) {
        _PR_PostNotifiesFromMonitor(&mon->waitCV, mon->notifyTimes);
        mon->notifyTimes = 0;
    }
    rv = PR_NotifyCondVar(&mon->entryCV);
    PR_ASSERT(rv == PR_SUCCESS);

    rv = PR_WaitCondVar(&mon->waitCV, ticks);
    PR_ASSERT(rv == PR_SUCCESS);

    while (mon->entryCount != 0) {
        rv = PR_WaitCondVar(&mon->entryCV, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(rv == PR_SUCCESS);
    }
    PR_ASSERT(mon->notifyTimes == 0);
    /* reinstate the interesting information */
    mon->entryCount = saved_entries;
    mon->owner = saved_owner;

    rv = PR_Unlock(&mon->lock);
    PR_ASSERT(rv == PR_SUCCESS);
    return rv;
}

/*
** Notify the highest priority thread waiting on the condition
** variable. If a thread is waiting on the condition variable (using
** PR_Wait) then it is awakened and begins waiting on the monitor's lock.
*/
PR_IMPLEMENT(PRStatus) PR_Notify(PRMonitor *mon)
{
    _PR_PostNotifyToMonitor(mon, PR_FALSE);
    return PR_SUCCESS;
}

/*
** Notify all of the threads waiting on the condition variable. All of
** threads are notified in turn. The highest priority thread will
** probably acquire the monitor first when the monitor is exited.
*/
PR_IMPLEMENT(PRStatus) PR_NotifyAll(PRMonitor *mon)
{
    _PR_PostNotifyToMonitor(mon, PR_TRUE);
    return PR_SUCCESS;
}

/************************************************************************/

PRUint32 _PR_MonitorToString(PRMonitor *mon, char *buf, PRUint32 buflen)
{
    PRUint32 nb;

    if (mon->owner) {
	nb = PR_snprintf(buf, buflen, "[%p] owner=%d[%p] count=%ld",
			 mon, mon->owner->id, mon->owner, mon->entryCount);
    } else {
	nb = PR_snprintf(buf, buflen, "[%p]", mon);
    }
    return nb;
}
