/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <kernel/OS.h>
#include <support/TLS.h>

#include "prlog.h"
#include "primpl.h"
#include "prcvar.h"
#include "prpdce.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* values for PRThread.state */
#define BT_THREAD_PRIMORD   0x01    /* this is the primordial thread */
#define BT_THREAD_SYSTEM    0x02    /* this is a system thread */
#define BT_THREAD_JOINABLE  0x04	/* this is a joinable thread */

struct _BT_Bookeeping
{
    PRLock *ml;                 /* a lock to protect ourselves */
	sem_id cleanUpSem;		/* the primoridal thread will block on this
							   sem while waiting for the user threads */
    PRInt32 threadCount;	/* user thred count */

} bt_book = { NULL, B_ERROR, 0 };


#define BT_TPD_LIMIT 128	/* number of TPD slots we'll provide (arbitrary) */

/* these will be used to map an index returned by PR_NewThreadPrivateIndex()
   to the corresponding beos native TLS slot number, and to the destructor
   for that slot - note that, because it is allocated globally, this data
   will be automatically zeroed for us when the program begins */
static int32 tpd_beosTLSSlots[BT_TPD_LIMIT];
static PRThreadPrivateDTOR tpd_dtors[BT_TPD_LIMIT];

static vint32 tpd_slotsUsed=0;	/* number of currently-allocated TPD slots */
static int32 tls_prThreadSlot;	/* TLS slot in which PRThread will be stored */

/* this mutex will be used to synchronize access to every
   PRThread.md.joinSem and PRThread.md.is_joining (we could
   actually allocate one per thread, but that seems a bit excessive,
   especially considering that there will probably be little
   contention, PR_JoinThread() is allowed to block anyway, and the code
   protected by the mutex is short/fast) */
static PRLock *joinSemLock;

static PRUint32 _bt_MapNSPRToNativePriority( PRThreadPriority priority );
static PRThreadPriority _bt_MapNativeToNSPRPriority( PRUint32 priority );
static void _bt_CleanupThread(void *arg);
static PRThread *_bt_AttachThread();

void
_PR_InitThreads (PRThreadType type, PRThreadPriority priority,
                 PRUintn maxPTDs)
{
    PRThread *primordialThread;
    PRUint32  beThreadPriority;

	/* allocate joinSem mutex */
	joinSemLock = PR_NewLock();
	if (joinSemLock == NULL)
	{
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
		return;
    }

    /*
    ** Create and initialize NSPR structure for our primordial thread.
    */
    
    primordialThread = PR_NEWZAP(PRThread);
    if( NULL == primordialThread )
    {
        PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
        return;
    }

	primordialThread->md.joinSem = B_ERROR;

    /*
    ** Set the priority to the desired level.
    */

    beThreadPriority = _bt_MapNSPRToNativePriority( priority );
    
    set_thread_priority( find_thread( NULL ), beThreadPriority );
    
    primordialThread->priority = priority;


	/* set the thread's state - note that the thread is not joinable */
    primordialThread->state |= BT_THREAD_PRIMORD;
	if (type == PR_SYSTEM_THREAD)
		primordialThread->state |= BT_THREAD_SYSTEM;

    /*
    ** Allocate a TLS slot for the PRThread structure (just using
    ** native TLS, as opposed to NSPR TPD, will make PR_GetCurrentThread()
    ** somewhat faster, and will leave one more TPD slot for our client)
    */
	
	tls_prThreadSlot = tls_allocate();

    /*
    ** Stuff our new PRThread structure into our thread specific
    ** slot.
    */

	tls_set(tls_prThreadSlot, primordialThread);
    
	/* allocate lock for bt_book */
    bt_book.ml = PR_NewLock();
    if( NULL == bt_book.ml )
    {
    	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }
}

PRUint32
_bt_MapNSPRToNativePriority( PRThreadPriority priority )
    {
    switch( priority )
    {
    	case PR_PRIORITY_LOW:	 return( B_LOW_PRIORITY );
	case PR_PRIORITY_NORMAL: return( B_NORMAL_PRIORITY );
	case PR_PRIORITY_HIGH:	 return( B_DISPLAY_PRIORITY );
	case PR_PRIORITY_URGENT: return( B_URGENT_DISPLAY_PRIORITY );
	default:		 return( B_NORMAL_PRIORITY );
    }
}

PRThreadPriority
_bt_MapNativeToNSPRPriority(PRUint32 priority)
    {
	if (priority < B_NORMAL_PRIORITY)
		return PR_PRIORITY_LOW;
	if (priority < B_DISPLAY_PRIORITY)
		return PR_PRIORITY_NORMAL;
	if (priority < B_URGENT_DISPLAY_PRIORITY)
		return PR_PRIORITY_HIGH;
	return PR_PRIORITY_URGENT;
}

PRUint32
_bt_mapNativeToNSPRPriority( int32 priority )
{
    switch( priority )
    {
    	case PR_PRIORITY_LOW:	 return( B_LOW_PRIORITY );
	case PR_PRIORITY_NORMAL: return( B_NORMAL_PRIORITY );
	case PR_PRIORITY_HIGH:	 return( B_DISPLAY_PRIORITY );
	case PR_PRIORITY_URGENT: return( B_URGENT_DISPLAY_PRIORITY );
	default:		 return( B_NORMAL_PRIORITY );
    }
}

/* This method is called by all NSPR threads as they exit */
void _bt_CleanupThread(void *arg)
{
	PRThread *me = PR_GetCurrentThread();
	int32 i;

	/* first, clean up all thread-private data */
	for (i = 0; i < tpd_slotsUsed; i++)
	{
		void *oldValue = tls_get(tpd_beosTLSSlots[i]);
		if ( oldValue != NULL && tpd_dtors[i] != NULL )
			(*tpd_dtors[i])(oldValue);
	}

	/* if this thread is joinable, wait for someone to join it */
	if (me->state & BT_THREAD_JOINABLE)
	{
		/* protect access to our joinSem */
		PR_Lock(joinSemLock);

		if (me->md.is_joining)
		{
			/* someone is already waiting to join us (they've
			   allocated a joinSem for us) - let them know we're
			   ready */
			delete_sem(me->md.joinSem);

			PR_Unlock(joinSemLock);

		}
		else
    {
			/* noone is currently waiting for our demise - it
			   is our responsibility to allocate the joinSem
			   and block on it */
			me->md.joinSem = create_sem(0, "join sem");

			/* we're done accessing our joinSem */
			PR_Unlock(joinSemLock);

			/* wait for someone to join us */
			while (acquire_sem(me->md.joinSem) == B_INTERRUPTED);
	    }
	}

	/* if this is a user thread, we must update our books */
	if ((me->state & BT_THREAD_SYSTEM) == 0)
	{
		/* synchronize access to bt_book */
    PR_Lock( bt_book.ml );

		/* decrement the number of currently-alive user threads */
	bt_book.threadCount--;

		if (bt_book.threadCount == 0 && bt_book.cleanUpSem != B_ERROR) {
			/* we are the last user thread, and the primordial thread is
			   blocked in PR_Cleanup() waiting for us to finish - notify
			   it */
			delete_sem(bt_book.cleanUpSem);
	}

    PR_Unlock( bt_book.ml );
	}

	/* finally, delete this thread's PRThread */
	PR_DELETE(me);
}

/**
 * This is a wrapper that all threads invoke that allows us to set some
 * things up prior to a thread's invocation and clean up after a thread has
 * exited.
 */
static void*
_bt_root (void* arg)
	{
    PRThread *thred = (PRThread*)arg;
    PRIntn rv;
    void *privData;
    status_t result;
    int i;

	/* save our PRThread object into our TLS */
	tls_set(tls_prThreadSlot, thred);

    thred->startFunc(thred->arg);  /* run the dang thing */

	/* clean up */
	_bt_CleanupThread(NULL);

	return 0;
}

PR_IMPLEMENT(PRThread*)
    PR_CreateThread (PRThreadType type, void (*start)(void* arg), void* arg,
                     PRThreadPriority priority, PRThreadScope scope,
                     PRThreadState state, PRUint32 stackSize)
{
    PRUint32 bePriority;

    PRThread* thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

	thred = PR_NEWZAP(PRThread);
 	if (thred == NULL)
	{
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }

    thred->md.joinSem = B_ERROR;

        thred->arg = arg;
        thred->startFunc = start;
        thred->priority = priority;

	if( state == PR_JOINABLE_THREAD )
	{
	    thred->state |= BT_THREAD_JOINABLE;
	}

        /* keep some books */

	PR_Lock( bt_book.ml );

	if (type == PR_USER_THREAD)
	{
	    bt_book.threadCount++;
        }

	PR_Unlock( bt_book.ml );

	bePriority = _bt_MapNSPRToNativePriority( priority );

        thred->md.tid = spawn_thread((thread_func)_bt_root, "moz-thread",
                                     bePriority, thred);
        if (thred->md.tid < B_OK) {
            PR_SetError(PR_UNKNOWN_ERROR, thred->md.tid);
            PR_DELETE(thred);
			return NULL;
        }

        if (resume_thread(thred->md.tid) < B_OK) {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
            PR_DELETE(thred);
			return NULL;
        }

    return thred;
    }

PR_IMPLEMENT(PRThread*)
	PR_AttachThread(PRThreadType type, PRThreadPriority priority,
					PRThreadStack *stack)
{
	/* PR_GetCurrentThread() will attach a thread if necessary */
	return PR_GetCurrentThread();
}

PR_IMPLEMENT(void)
	PR_DetachThread()
{
	/* we don't support detaching */
}

PR_IMPLEMENT(PRStatus)
    PR_JoinThread (PRThread* thred)
{
    status_t eval, status;

    PR_ASSERT(thred != NULL);

	if ((thred->state & BT_THREAD_JOINABLE) == 0)
    {
	PR_SetError( PR_INVALID_ARGUMENT_ERROR, 0 );
	return( PR_FAILURE );
    }

	/* synchronize access to the thread's joinSem */
	PR_Lock(joinSemLock);
	
	if (thred->md.is_joining)
	{
		/* another thread is already waiting to join the specified
		   thread - we must fail */
		PR_Unlock(joinSemLock);
		return PR_FAILURE;
	}

	/* let others know we are waiting to join */
	thred->md.is_joining = PR_TRUE;

	if (thred->md.joinSem == B_ERROR)
	{
		/* the thread hasn't finished yet - it is our responsibility to
		   allocate a joinSem and wait on it */
		thred->md.joinSem = create_sem(0, "join sem");

		/* we're done changing the joinSem now */
		PR_Unlock(joinSemLock);

		/* wait for the thread to finish */
		while (acquire_sem(thred->md.joinSem) == B_INTERRUPTED);

	}
	else
	{
		/* the thread has already finished, and has allocated the
		   joinSem itself - let it know it can finally die */
		delete_sem(thred->md.joinSem);
		
		PR_Unlock(joinSemLock);
    }

	/* make sure the thread is dead */
    wait_for_thread(thred->md.tid, &eval);

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRThread*)
    PR_GetCurrentThread ()
{
    PRThread* thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    thred = (PRThread *)tls_get( tls_prThreadSlot);
	if (thred == NULL)
	{
		/* this thread doesn't have a PRThread structure (it must be
		   a native thread not created by the NSPR) - assimilate it */
		thred = _bt_AttachThread();
	}
    PR_ASSERT(NULL != thred);

    return thred;
}

PR_IMPLEMENT(PRThreadScope)
    PR_GetThreadScope (const PRThread* thred)
{
    PR_ASSERT(thred != NULL);
    return PR_GLOBAL_THREAD;
}

PR_IMPLEMENT(PRThreadType)
    PR_GetThreadType (const PRThread* thred)
{
    PR_ASSERT(thred != NULL);
    return (thred->state & BT_THREAD_SYSTEM) ?
        PR_SYSTEM_THREAD : PR_USER_THREAD;
}

PR_IMPLEMENT(PRThreadState)
    PR_GetThreadState (const PRThread* thred)
{
    PR_ASSERT(thred != NULL);
    return (thred->state & BT_THREAD_JOINABLE)?
    					PR_JOINABLE_THREAD: PR_UNJOINABLE_THREAD;
}

PR_IMPLEMENT(PRThreadPriority)
    PR_GetThreadPriority (const PRThread* thred)
{
    PR_ASSERT(thred != NULL);
    return thred->priority;
}  /* PR_GetThreadPriority */

PR_IMPLEMENT(void) PR_SetThreadPriority(PRThread *thred,
                                        PRThreadPriority newPri)
{
    PRUint32 bePriority;

    PR_ASSERT( thred != NULL );

    thred->priority = newPri;
    bePriority = _bt_MapNSPRToNativePriority( newPri );
    set_thread_priority( thred->md.tid, bePriority );
}

PR_IMPLEMENT(PRStatus)
    PR_NewThreadPrivateIndex (PRUintn* newIndex,
                              PRThreadPrivateDTOR destructor)
{
	int32    index;

    if (!_pr_initialized) _PR_ImplicitInitialization();

	/* reserve the next available tpd slot */
	index = atomic_add( &tpd_slotsUsed, 1 );
	if (index >= BT_TPD_LIMIT)
	{
		/* no slots left - decrement value, then fail */
		atomic_add( &tpd_slotsUsed, -1 );
		PR_SetError( PR_TPD_RANGE_ERROR, 0 );
	    return( PR_FAILURE );
	}

	/* allocate a beos-native TLS slot for this index (the new slot
	   automatically contains NULL) */
	tpd_beosTLSSlots[index] = tls_allocate();

	/* remember the destructor */
	tpd_dtors[index] = destructor;

    *newIndex = (PRUintn)index;

    return( PR_SUCCESS );
}

PR_IMPLEMENT(PRStatus)
    PR_SetThreadPrivate (PRUintn index, void* priv)
{
	void *oldValue;

    /*
    ** Sanity checking
    */

    if(index < 0 || index >= tpd_slotsUsed || index >= BT_TPD_LIMIT)
    {
		PR_SetError( PR_TPD_RANGE_ERROR, 0 );
	return( PR_FAILURE );
    }

	/* if the old value isn't NULL, and the dtor for this slot isn't
	   NULL, we must destroy the data */
	oldValue = tls_get(tpd_beosTLSSlots[index]);
	if (oldValue != NULL && tpd_dtors[index] != NULL)
		(*tpd_dtors[index])(oldValue);

	/* save new value */
	tls_set(tpd_beosTLSSlots[index], priv);

	    return( PR_SUCCESS );
	}

PR_IMPLEMENT(void*)
    PR_GetThreadPrivate (PRUintn index)
{
	/* make sure the index is valid */
	if (index < 0 || index >= tpd_slotsUsed || index >= BT_TPD_LIMIT)
    {   
		PR_SetError( PR_TPD_RANGE_ERROR, 0 );
		return NULL;
    }

	/* return the value */
	return tls_get( tpd_beosTLSSlots[index] );
	}


PR_IMPLEMENT(PRStatus)
    PR_Interrupt (PRThread* thred)
{
    PRIntn rv;

    PR_ASSERT(thred != NULL);

    /*
    ** there seems to be a bug in beos R5 in which calling
    ** resume_thread() on a blocked thread returns B_OK instead
    ** of B_BAD_THREAD_STATE (beos bug #20000422-19095).  as such,
    ** to interrupt a thread, we will simply suspend then resume it
    ** (no longer call resume_thread(), check for B_BAD_THREAD_STATE,
    ** the suspend/resume to wake up a blocked thread).  this wakes
    ** up blocked threads properly, and doesn't hurt unblocked threads
    ** (they simply get stopped then re-started immediately)
    */

    rv = suspend_thread( thred->md.tid );
    if( rv != B_NO_ERROR )
    {
        /* this doesn't appear to be a valid thread_id */
        PR_SetError( PR_UNKNOWN_ERROR, rv );
        return PR_FAILURE;
    }

    rv = resume_thread( thred->md.tid );
    if( rv != B_NO_ERROR )
    {
        PR_SetError( PR_UNKNOWN_ERROR, rv );
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
    PR_ClearInterrupt ()
{
}

PR_IMPLEMENT(PRStatus)
    PR_Yield ()
{
    /* we just sleep for long enough to cause a reschedule (100
       microseconds) */
    snooze(100);
}

#define BT_MILLION 1000000UL

PR_IMPLEMENT(PRStatus)
    PR_Sleep (PRIntervalTime ticks)
{
    bigtime_t tps;
    status_t status;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    tps = PR_IntervalToMicroseconds( ticks );

    status = snooze(tps);
    if (status == B_NO_ERROR) return PR_SUCCESS;

    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, status);
    return PR_FAILURE;
}

PR_IMPLEMENT(PRStatus)
    PR_Cleanup ()
{
    PRThread *me = PR_GetCurrentThread();

    PR_ASSERT(me->state & BT_THREAD_PRIMORD);
    if ((me->state & BT_THREAD_PRIMORD) == 0) {
        return PR_FAILURE;
    }

    PR_Lock( bt_book.ml );

	if (bt_book.threadCount != 0)
    {
		/* we'll have to wait for some threads to finish - create a
		   sem to block on */
		bt_book.cleanUpSem = create_sem(0, "cleanup sem");
    }

    PR_Unlock( bt_book.ml );

	/* note that, if all the user threads were already dead, we
	   wouldn't have created a sem above, so this acquire_sem()
	   will fail immediately */
	while (acquire_sem(bt_book.cleanUpSem) == B_INTERRUPTED);

    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
    PR_ProcessExit (PRIntn status)
{
    exit(status);
}

PRThread *_bt_AttachThread()
{
	PRThread *thread;
	thread_info tInfo;

	/* make sure this thread doesn't already have a PRThread structure */
	PR_ASSERT(tls_get(tls_prThreadSlot) == NULL);

	/* allocate a PRThread structure for this thread */
	thread = PR_NEWZAP(PRThread);
	if (thread == NULL)
	{
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
		return NULL;
	}

	/* get the native thread's current state */
	get_thread_info(find_thread(NULL), &tInfo);

	/* initialize new PRThread */
	thread->md.tid = tInfo.thread;
	thread->md.joinSem = B_ERROR;
	thread->priority = _bt_MapNativeToNSPRPriority(tInfo.priority);

	/* attached threads are always non-joinable user threads */
	thread->state = 0;

	/* increment user thread count */
	PR_Lock(bt_book.ml);
	bt_book.threadCount++;
	PR_Unlock(bt_book.ml);

	/* store this thread's PRThread */
	tls_set(tls_prThreadSlot, thread);
	
	/* the thread must call _bt_CleanupThread() before it dies, in order
	   to clean up its PRThread, synchronize with the primordial thread,
	   etc. */
	on_exit_thread(_bt_CleanupThread, NULL);
	
	return thread;
}
