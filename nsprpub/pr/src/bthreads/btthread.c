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

#include "prlog.h"
#include "primpl.h"
#include "prcvar.h"
#include "prpdce.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define BT_THREAD_PRIMORD   0x01    /* this is the primordial thread */
#define BT_THREAD_SYSTEM    0x02    /* this is a system thread */

struct _BT_Bookeeping
{
    PRLock *ml;                 /* a lock to protect ourselves */
    PRCondVar *cv;              /* used to signal global things */
    PRInt32 threadCount;	/* user thred count */

} bt_book = { 0 };

/*
** A structure at the root of the thread private data.  Each member of
** the array keys[] points to a hash table based on the thread's ID.
*/

struct _BT_PrivateData
{
    PRLock *lock;		/* A lock to coordinate access */
    struct _BT_PrivateHash *keys[128];	/* Up to 128 keys, pointing to a hash table */

} bt_privateRoot = { 0 };

/*
** A dynamically allocated structure that contains 256 hash buckets that
** contain a linked list of thread IDs.  The hash is simply the last 8 bits
** of the thread_id.  ( current thread_id & 0x000000FF )
*/

struct _BT_PrivateHash
{
    void (PR_CALLBACK *destructor)(void *arg);	/* The destructor */
    struct _BT_PrivateEntry *next[256]; /* Pointer to the first element in the list */
};

/*
** A dynamically allocated structure that is a member of a linked list of
** thread IDs.
*/

struct _BT_PrivateEntry
{
    struct _BT_PrivateEntry *next;		/* Pointer to the next thread */
    thread_id threadID;			/* The BeOS thread ID */
    void *data;				/* The data */
};

PRUint32 _bt_mapPriority( PRThreadPriority priority );
PR_IMPLEMENT(void *) _bt_getThreadPrivate(PRUintn index);

void
_PR_InitThreads (PRThreadType type, PRThreadPriority priority,
                 PRUintn maxPTDs)
{
    PRThread *primordialThread;
    PRLock   *tempLock;
    PRUintn   tempKey;
    PRUint32  beThreadPriority;

    /*
    ** Create a NSPR structure for our primordial thread.
    */
    
    primordialThread = PR_NEWZAP(PRThread);
    if( NULL == primordialThread )
    {
        PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
        return;
    }

    /*
    ** Set the priority to the desired level.
    */

    beThreadPriority = _bt_mapPriority( priority );
    
    set_thread_priority( find_thread( NULL ), beThreadPriority );
    
    primordialThread->state |= BT_THREAD_PRIMORD;
    primordialThread->priority = priority;

    /*
    ** Initialize the thread tracking data structures
    */

    bt_privateRoot.lock = PR_NewLock();
    if( NULL == bt_privateRoot.lock )
    {
	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }

    /*
    ** Grab a key.  We're guaranteed to be key #0, since we are
    ** always the first one in.
    */

    if( PR_NewThreadPrivateIndex( &tempKey, NULL ) != PR_SUCCESS )
    {
	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }
	
    PR_ASSERT( tempKey == 0 );

    /*
    ** Stuff our new PRThread structure into our thread specific
    ** slot.
    */
    
    if( PR_SetThreadPrivate( (PRUint8) 0, (void *) primordialThread ) == PR_FAILURE )
    {
    	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
    	return;
    }

    /*
    ** Allocate some memory to hold our global lock.  We never clean it
    ** up later, but BeOS automatically frees memory when the thread
    ** dies.
    */
    
    bt_book.ml = PR_NewLock();
    if( NULL == bt_book.ml )
    {
    	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }

    tempLock = PR_NewLock();
    if( NULL == tempLock )
    {
    	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }

    bt_book.cv = PR_NewCondVar( tempLock );
    if( NULL == bt_book.cv )
    {
	PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
	return;
    }
}

PRUint32
_bt_mapPriority( PRThreadPriority priority )
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

    struct _BT_PrivateHash *hashTable;

    /* Set within the current thread the pointer to our object. This
       object will be deleted when the thread termintates.  */

    result = PR_SetThreadPrivate( 0, (void *) thred );
    PR_ASSERT( result == PR_SUCCESS );

    thred->startFunc(thred->arg);  /* run the dang thing */

    /*
    ** Call the destructor, if available.
    */

    PR_Lock( bt_privateRoot.lock );

    for( i = 0; i < 128; i++ )
    {
	hashTable = bt_privateRoot.keys[i];

	if( hashTable != NULL )
	{
	    if( hashTable->destructor != NULL )
	    {
		privData = _bt_getThreadPrivate( i );

		if( privData != NULL )
		{
		    PR_Unlock( bt_privateRoot.lock );
		    hashTable->destructor( privData );
		    PR_Lock( bt_privateRoot.lock );
		}
	    }
	}
    }

    PR_Unlock( bt_privateRoot.lock );

    /* decrement our thread counters */

    PR_Lock( bt_book.ml );

    if (thred->state & BT_THREAD_SYSTEM) {
#if 0
        bt_book.system -= 1;
#endif
    } else 
    {
	bt_book.threadCount--;

	if( 0 == bt_book.threadCount )
	{
            PR_NotifyAllCondVar(bt_book.cv);
	}
    }

    PR_Unlock( bt_book.ml );

    if( thred->md.is_joinable == 1 )
    {
	/*
	** This is a joinable thread.  Keep suspending
	** until is_joining is set to 1
	*/

	if( thred->md.is_joining == 0 )
	{
	    suspend_thread( thred->md.tid );
	}
    }

    /* delete the thread object */
    PR_DELETE(thred);

    result = PR_SetThreadPrivate( (PRUint8) 0, (void *) NULL );
    PR_ASSERT( result == PR_SUCCESS );
    exit_thread( NULL );
}

PR_IMPLEMENT(PRThread*)
    PR_CreateThread (PRThreadType type, void (*start)(void* arg), void* arg,
                     PRThreadPriority priority, PRThreadScope scope,
                     PRThreadState state, PRUint32 stackSize)
{
    PRUint32 bePriority;

    PRThread* thred = PR_NEWZAP(PRThread);

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (thred != NULL) {
        thred->arg = arg;
        thred->startFunc = start;
        thred->priority = priority;

	if( state == PR_JOINABLE_THREAD )
	{
	    thred->md.is_joinable = 1;
	}
	else
	{
	    thred->md.is_joinable = 0;
	}

	thred->md.is_joining = 0;

        /* keep some books */

	PR_Lock( bt_book.ml );

        if (PR_SYSTEM_THREAD == type) {
            thred->state |= BT_THREAD_SYSTEM;
#if 0
            bt_book.system += 1;
#endif
        } else {
	    bt_book.threadCount++;
        }

	PR_Unlock( bt_book.ml );

	bePriority = _bt_mapPriority( priority );

        thred->md.tid = spawn_thread((thread_func)_bt_root, "moz-thread",
                                     bePriority, thred);
        if (thred->md.tid < B_OK) {
            PR_SetError(PR_UNKNOWN_ERROR, thred->md.tid);
            PR_DELETE(thred);
            thred = NULL;
        }

        if (resume_thread(thred->md.tid) < B_OK) {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
            PR_DELETE(thred);
            thred = NULL;
        }

    } else {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return thred;
}

PR_IMPLEMENT(PRStatus)
    PR_JoinThread (PRThread* thred)
{
    status_t eval, status;

    PR_ASSERT(thred != NULL);

    if( thred->md.is_joinable != 1 )
    {
	PR_SetError( PR_UNKNOWN_ERROR, 0 );
	return( PR_FAILURE );
    }

    thred->md.is_joining = 1;

    status = wait_for_thread(thred->md.tid, &eval);

    if (status < B_NO_ERROR) {

        PR_SetError(PR_UNKNOWN_ERROR, status);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRThread*)
    PR_GetCurrentThread ()
{
    void* thred;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    thred = PR_GetThreadPrivate( (PRUint8) 0 );
    PR_ASSERT(NULL != thred);

    return (PRThread*)thred;
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
    return PR_JOINABLE_THREAD;
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
    bePriority = _bt_mapPriority( newPri );
    set_thread_priority( thred->md.tid, bePriority );
}

PR_IMPLEMENT(PRStatus)
    PR_NewThreadPrivateIndex (PRUintn* newIndex,
                              PRThreadPrivateDTOR destructor)
{
    PRUintn  index;
    struct _BT_PrivateHash *tempPointer;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    /*
    ** Grab the lock, or hang until it is free.  This is critical code,
    ** and only one thread at a time should be going through it.
    */

    PR_Lock( bt_privateRoot.lock );

    /*
    ** Run through the array of keys, find the first one that's zero.
    ** Exit if we hit the top of the array.
    */

    index = 0;

    while( bt_privateRoot.keys[index] != 0 )
    {
	index++;

	if( 128 == index )
	{
	    PR_Unlock( bt_privateRoot.lock );
	    return( PR_FAILURE );
	}
    }

    /*
    ** Index has the first available zeroed slot.  Allocate a
    ** _BT_PrivateHash structure, all zeroed.  Assuming that goes
    ** well, return the index.
    */

    tempPointer = PR_NEWZAP( struct _BT_PrivateHash );

    if( 0 == tempPointer ) {

	PR_Unlock( bt_privateRoot.lock );
	return( PR_FAILURE );
    }

    bt_privateRoot.keys[index] = tempPointer;
    tempPointer->destructor = destructor;

    PR_Unlock( bt_privateRoot.lock );

    *newIndex = index;

    return( PR_SUCCESS );
}

PR_IMPLEMENT(PRStatus)
    PR_SetThreadPrivate (PRUintn index, void* priv)
{
    thread_id	currentThread;
    PRUint8	hashBucket;
    void       *tempPointer;

    struct _BT_PrivateHash  *hashTable;
    struct _BT_PrivateEntry *currentEntry;
    struct _BT_PrivateEntry *previousEntry;

    /*
    ** Sanity checking
    */

    if( index < 0 || index > 127 ) return( PR_FAILURE );

    /*
    ** Grab the thread ID for this thread.  Assign it to a hash bucket.
    */

    currentThread = find_thread( NULL );
    hashBucket = currentThread & 0x000000FF;

    /*
    ** Lock out all other threads then grab the proper hash table based
    ** on the passed index.
    */

    PR_Lock( bt_privateRoot.lock );

    hashTable = bt_privateRoot.keys[index];

    if( 0 == hashTable )
    {
	PR_Unlock( bt_privateRoot.lock );
	return( PR_FAILURE );
    }

    /*
    ** Search through the linked list the end is reached or an existing
    ** entry is found.
    */

    currentEntry = hashTable->next[ hashBucket ];
    previousEntry = NULL;

    while( currentEntry != 0 )
    {
	if( currentEntry->threadID == currentThread )
	{
	    /*
	    ** Found a structure previously created for this thread.
	    ** Is there a destructor to be called?
	    */

	    if( hashTable->destructor != NULL )
	    {
		if( currentEntry->data != NULL )
		{
		    PR_Unlock( bt_privateRoot.lock );
		    hashTable->destructor( currentEntry->data );
		    PR_Lock( bt_privateRoot.lock );
		}
	    }

	    /*
	    ** If the data was not NULL, and there was a destructor,
	    ** it has already been called.  Overwrite the existing
	    ** data and return with success.
	    */

	    currentEntry->data = priv;
	    PR_Unlock( bt_privateRoot.lock );
	    return( PR_SUCCESS );
	}

	previousEntry = currentEntry;
	currentEntry  = previousEntry->next;
    }

    /*
    ** If we're here, we didn't find an entry for this thread.  Create
    ** one and attach it to the end of the list.
    */

    currentEntry = PR_NEWZAP( struct _BT_PrivateEntry );

    if( 0 == currentEntry )
    {
	PR_Unlock( bt_privateRoot.lock );
	return( PR_FAILURE );
    }

    currentEntry->threadID = currentThread;
    currentEntry->data     = priv;

    if( 0 == previousEntry )
    {
	/*
	** This is a special case.  This is the first entry in the list
	** so set the hash table to point to this entry.
	*/

	hashTable->next[ hashBucket ] = currentEntry;
    }
    else
    {
	previousEntry->next = currentEntry;
    }

    PR_Unlock( bt_privateRoot.lock );

    return( PR_SUCCESS );
}

PR_IMPLEMENT(void*)
    _bt_getThreadPrivate(PRUintn index)
{
    thread_id	currentThread;
    PRUint8	hashBucket;
    void       *tempPointer;

    struct _BT_PrivateHash  *hashTable;
    struct _BT_PrivateEntry *currentEntry;

    /*
    ** Sanity checking
    */

    if( index < 0 || index > 127 ) return( NULL );

    /*
    ** Grab the thread ID for this thread.  Assign it to a hash bucket.
    */

    currentThread = find_thread( NULL );
    hashBucket = currentThread & 0x000000FF;

    /*
    ** Grab the proper hash table based on the passed index.
    */

    hashTable = bt_privateRoot.keys[index];

    if( 0 == hashTable )
    {   
	return( NULL );
    }

    /*
    ** Search through the linked list the end is reached or an existing
    ** entry is found.
    */

    currentEntry = hashTable->next[ hashBucket ];

    while( currentEntry != 0 )
    {   
	if( currentEntry->threadID == currentThread )
	{   
	    /*
	    ** Found a structure previously created for this thread.
	    ** Copy out the data, unlock, and return.
	    */

	    tempPointer = currentEntry->data;
	    return( tempPointer );
	}

	currentEntry  = currentEntry->next;
    }

    /*
    ** Ooops, we ran out of entries.  This thread isn't listed.
    */

    return( NULL );
}

PR_IMPLEMENT(void*)
    PR_GetThreadPrivate (PRUintn index)
{
    void *returnValue;

    PR_Lock( bt_privateRoot.lock );
    returnValue = _bt_getThreadPrivate( index );
    PR_Unlock( bt_privateRoot.lock );

    return( returnValue );
}


PR_IMPLEMENT(PRStatus)
    PR_Interrupt (PRThread* thred)
{
    PRIntn rv;

    PR_ASSERT(thred != NULL);
    rv = resume_thread( thred->md.tid );

    if( rv == B_BAD_THREAD_STATE )
    {
	/*
	** We have a thread that's not suspended, but is
	** blocked.  Suspend it THEN resume it.  The
	** function call that's hanging will return
	** B_INTERRUPTED
	*/

	rv = suspend_thread( thred->md.tid );
	if( rv != B_NO_ERROR )
	{
	    PR_SetError( PR_UNKNOWN_ERROR, rv );
	    return( PR_FAILURE );
	}
	rv = resume_thread( thred->md.tid );
	if( rv != B_NO_ERROR )
	{
	    PR_SetError( PR_UNKNOWN_ERROR, rv );
	    return( PR_FAILURE );
	}
    }

    if( rv != B_NO_ERROR )
    {
	PR_SetError( PR_UNKNOWN_ERROR, rv );
	return( PR_FAILURE );
    }

    return( PR_SUCCESS );
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
    PRThread *me = PR_CurrentThread();

    PR_ASSERT(me->state & BT_THREAD_PRIMORD);
    if ((me->state & BT_THREAD_PRIMORD) == 0) {
        return PR_FAILURE;
    }

    PR_Lock( bt_book.ml );

    while( bt_book.threadCount > 0 )
    {
	PR_Unlock( bt_book.ml );
        PR_WaitCondVar(bt_book.cv, PR_INTERVAL_NO_TIMEOUT);
	PR_Lock( bt_book.ml );
    }

    PR_Unlock( bt_book.ml );

#if 0
    /* I am not sure if it's safe to delete the cv and lock here, since
     * there may still be "system" threads around. If this call isn't
     * immediately prior to exiting, then there's a problem. */
    if (0 == bt_book.system) {
        PR_DestroyCondVar(bt_book.cv); bt_book.cv = NULL;
        PR_DestroyLock(bt_book.ml); bt_book.ml = NULL;
    }
    PR_DELETE(me);
#endif

    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
    PR_ProcessExit (PRIntn status)
{
    exit(status);
}
