/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/************************************************************************/

/*
** Create a new monitor.
*/
PR_IMPLEMENT(PRMonitor*) PR_NewMonitor()
{
    PRMonitor *mon;
	PRCondVar *cvar;
	PRLock *lock;

    mon = PR_NEWZAP(PRMonitor);
    if (mon) {
		lock = PR_NewLock();
	    if (!lock) {
			PR_DELETE(mon);
			return 0;
    	}

	    cvar = PR_NewCondVar(lock);
	    if (!cvar) {
	    	PR_DestroyLock(lock);
			PR_DELETE(mon);
			return 0;
    	}
    	mon->cvar = cvar;
	mon->name = NULL;
    }
    return mon;
}

PR_IMPLEMENT(PRMonitor*) PR_NewNamedMonitor(const char* name)
{
    PRMonitor* mon = PR_NewMonitor();
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
	PR_DestroyLock(mon->cvar->lock);
    PR_DestroyCondVar(mon->cvar);
    PR_DELETE(mon);
}

/*
** Enter the lock associated with the monitor.
*/
PR_IMPLEMENT(void) PR_EnterMonitor(PRMonitor *mon)
{
    if (mon->cvar->lock->owner == _PR_MD_CURRENT_THREAD()) {
		mon->entryCount++;
    } else {
		PR_Lock(mon->cvar->lock);
		mon->entryCount = 1;
    }
}

/*
** Test and then enter the lock associated with the monitor if it's not
** already entered by some other thread. Return PR_FALSE if some other
** thread owned the lock at the time of the call.
*/
PR_IMPLEMENT(PRBool) PR_TestAndEnterMonitor(PRMonitor *mon)
{
    if (mon->cvar->lock->owner == _PR_MD_CURRENT_THREAD()) {
		mon->entryCount++;
		return PR_TRUE;
    } else {
		if (PR_TestAndLock(mon->cvar->lock)) {
	    	mon->entryCount = 1;
	   	 	return PR_TRUE;
		}
    }
    return PR_FALSE;
}

/*
** Exit the lock associated with the monitor once.
*/
PR_IMPLEMENT(PRStatus) PR_ExitMonitor(PRMonitor *mon)
{
    if (mon->cvar->lock->owner != _PR_MD_CURRENT_THREAD()) {
        return PR_FAILURE;
    }
    if (--mon->entryCount == 0) {
		return PR_Unlock(mon->cvar->lock);
    }
    return PR_SUCCESS;
}

/*
** Return the number of times that the current thread has entered the
** lock. Returns zero if the current thread has not entered the lock.
*/
PR_IMPLEMENT(PRIntn) PR_GetMonitorEntryCount(PRMonitor *mon)
{
    return (mon->cvar->lock->owner == _PR_MD_CURRENT_THREAD()) ?
        mon->entryCount : 0;
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
** This routine can return PR_PENDING_INTERRUPT if the waiting thread 
** has been interrupted.
*/
PR_IMPLEMENT(PRStatus) PR_Wait(PRMonitor *mon, PRIntervalTime ticks)
{
    PRUintn entryCount;
	PRStatus status;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (mon->cvar->lock->owner != me) return PR_FAILURE;

    entryCount = mon->entryCount;
    mon->entryCount = 0;

	status = _PR_WaitCondVar(me, mon->cvar, mon->cvar->lock, ticks);

    mon->entryCount = entryCount;

    return status;
}

/*
** Notify the highest priority thread waiting on the condition
** variable. If a thread is waiting on the condition variable (using
** PR_Wait) then it is awakened and begins waiting on the monitor's lock.
*/
PR_IMPLEMENT(PRStatus) PR_Notify(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    if (mon->cvar->lock->owner != me) return PR_FAILURE;
    PR_NotifyCondVar(mon->cvar);
    return PR_SUCCESS;
}

/*
** Notify all of the threads waiting on the condition variable. All of
** threads are notified in turn. The highest priority thread will
** probably acquire the monitor first when the monitor is exited.
*/
PR_IMPLEMENT(PRStatus) PR_NotifyAll(PRMonitor *mon)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    if (mon->cvar->lock->owner != me) return PR_FAILURE;
    PR_NotifyAllCondVar(mon->cvar);
    return PR_SUCCESS;
}

/************************************************************************/

PRUint32 _PR_MonitorToString(PRMonitor *mon, char *buf, PRUint32 buflen)
{
    PRUint32 nb;

    if (mon->cvar->lock->owner) {
	nb = PR_snprintf(buf, buflen, "[%p] owner=%d[%p] count=%ld",
			 mon, mon->cvar->lock->owner->id,
			 mon->cvar->lock->owner, mon->entryCount);
    } else {
	nb = PR_snprintf(buf, buflen, "[%p]", mon);
    }
    return nb;
}
