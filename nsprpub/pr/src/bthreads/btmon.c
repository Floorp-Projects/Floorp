/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include <kernel/OS.h>

#include "primpl.h"

/*
** Create a new monitor. Monitors are re-entrant locks with a single built-in
** condition variable.
**
** This may fail if memory is tight or if some operating system resource
** is low.
*/
PR_IMPLEMENT(PRMonitor*)
    PR_NewMonitor (void)
{
    PRMonitor *mon;
    PRCondVar *cvar;
    PRLock    *lock;

    mon = PR_NEWZAP( PRMonitor );
    if( mon )
    {
	lock = PR_NewLock();
	if( !lock )
        {
	    PR_DELETE( mon );
	    return( 0 );
	}

	cvar = PR_NewCondVar( lock );
	if( !cvar )
	{
	    PR_DestroyLock( lock );
	    PR_DELETE( mon );
	    return( 0 );
	}

	mon->cvar = cvar;
	mon->name = NULL;
    }

    return( mon );
}

PR_IMPLEMENT(PRMonitor*) PR_NewNamedMonitor(const char* name)
{
    PRMonitor* mon = PR_NewMonitor();
    mon->name = name;
    return mon;
}

/*
** Destroy a monitor. The caller is responsible for guaranteeing that the
** monitor is no longer in use. There must be no thread waiting on the
** monitor's condition variable and that the lock is not held.
**
*/
PR_IMPLEMENT(void)
    PR_DestroyMonitor (PRMonitor *mon)
{
    PR_DestroyLock( mon->cvar->lock );
    PR_DestroyCondVar( mon->cvar );
    PR_DELETE( mon );
}

/*
** Enter the lock associated with the monitor. If the calling thread currently
** is in the monitor, the call to enter will silently succeed. In either case,
** it will increment the entry count by one.
*/
PR_IMPLEMENT(void)
    PR_EnterMonitor (PRMonitor *mon)
{
    if( mon->cvar->lock->owner == find_thread( NULL ) )
    {
	mon->entryCount++;

    } else
    {
	PR_Lock( mon->cvar->lock );
	mon->entryCount = 1;
    }
}

/*
** Decrement the entry count associated with the monitor. If the decremented
** entry count is zero, the monitor is exited. Returns PR_FAILURE if the
** calling thread has not entered the monitor.
*/
PR_IMPLEMENT(PRStatus)
    PR_ExitMonitor (PRMonitor *mon)
{
    if( mon->cvar->lock->owner != find_thread( NULL ) )
    {
	return( PR_FAILURE );
    }
    if( --mon->entryCount == 0 )
    {
	return( PR_Unlock( mon->cvar->lock ) );
    }
    return( PR_SUCCESS );
}

/*
** Wait for a notify on the monitor's condition variable. Sleep for "ticks"
** amount of time (if "ticks" is PR_INTERVAL_NO_TIMEOUT then the sleep is
** indefinite).
**
** While the thread is waiting it exits the monitor (as if it called
** PR_ExitMonitor as many times as it had called PR_EnterMonitor).  When
** the wait has finished the thread regains control of the monitors lock
** with the same entry count as before the wait began.
**
** The thread waiting on the monitor will be resumed when the monitor is
** notified (assuming the thread is the next in line to receive the
** notify) or when the "ticks" timeout elapses.
**
** Returns PR_FAILURE if the caller has not entered the monitor.
*/
PR_IMPLEMENT(PRStatus)
    PR_Wait (PRMonitor *mon, PRIntervalTime ticks)
{
    PRUint32 entryCount;
    PRUintn  status;
    PRThread *meThread;
    thread_id me = find_thread( NULL );
    meThread = PR_GetCurrentThread();

    if( mon->cvar->lock->owner != me ) return( PR_FAILURE );

    entryCount = mon->entryCount;
    mon->entryCount = 0;

    status = PR_WaitCondVar( mon->cvar, ticks );

    mon->entryCount = entryCount;

    return( status );
}

/*
** Notify a thread waiting on the monitor's condition variable. If a thread
** is waiting on the condition variable (using PR_Wait) then it is awakened
** and attempts to reenter the monitor.
*/
PR_IMPLEMENT(PRStatus)
    PR_Notify (PRMonitor *mon)
{
    if( mon->cvar->lock->owner != find_thread( NULL ) )
    {
	return( PR_FAILURE );
    }

    PR_NotifyCondVar( mon->cvar );
    return( PR_SUCCESS );
}

/*
** Notify all of the threads waiting on the monitor's condition variable.
** All of threads waiting on the condition are scheduled to reenter the
** monitor.
*/
PR_IMPLEMENT(PRStatus)
    PR_NotifyAll (PRMonitor *mon)
{
    if( mon->cvar->lock->owner != find_thread( NULL ) )
    {
	return( PR_FAILURE );
    }

    PR_NotifyAllCondVar( mon->cvar );
    return( PR_SUCCESS );
}

PR_IMPLEMENT(PRIntn)
    PR_GetMonitorEntryCount(PRMonitor *mon)
{
    return( mon->entryCount );
}

