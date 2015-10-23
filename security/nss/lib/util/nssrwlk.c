/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nssrwlk.h"
#include "nspr.h"

PR_BEGIN_EXTERN_C

/*
 * Reader-writer lock
 */
struct nssRWLockStr {
    PZLock *        rw_lock;
    char   *        rw_name;            /* lock name                    */
    PRUint32        rw_rank;            /* rank of the lock             */
    PRInt32         rw_writer_locks;    /* ==  0, if unlocked           */
    PRInt32         rw_reader_locks;    /* ==  0, if unlocked           */
                                        /* > 0  , # of read locks       */
    PRUint32        rw_waiting_readers; /* number of waiting readers    */
    PRUint32        rw_waiting_writers; /* number of waiting writers    */
    PZCondVar *     rw_reader_waitq;    /* cvar for readers             */
    PZCondVar *     rw_writer_waitq;    /* cvar for writers             */
    PRThread  *     rw_owner;           /* lock owner for write-lock    */
                                        /* Non-null if write lock held. */
};

PR_END_EXTERN_C

#include <string.h>

#ifdef DEBUG_RANK_ORDER
#define NSS_RWLOCK_RANK_ORDER_DEBUG /* enable deadlock detection using
                                       rank-order for locks
                                    */
#endif

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG

static PRUintn  nss_thread_rwlock_initialized;
static PRUintn  nss_thread_rwlock;               /* TPD key for lock stack */
static PRUintn  nss_thread_rwlock_alloc_failed;

#define _NSS_RWLOCK_RANK_ORDER_LIMIT 10

typedef struct thread_rwlock_stack {
    PRInt32     trs_index;                                  /* top of stack */
    NSSRWLock    *trs_stack[_NSS_RWLOCK_RANK_ORDER_LIMIT];  /* stack of lock
                                                               pointers */
} thread_rwlock_stack;

/* forward static declarations. */
static PRUint32 nssRWLock_GetThreadRank(PRThread *me);
static void     nssRWLock_SetThreadRank(PRThread *me, NSSRWLock *rwlock);
static void     nssRWLock_UnsetThreadRank(PRThread *me, NSSRWLock *rwlock);
static void     nssRWLock_ReleaseLockStack(void *lock_stack);

#endif

#define UNTIL(x) while(!(x))

/*
 * Reader/Writer Locks
 */

/*
 * NSSRWLock_New
 *      Create a reader-writer lock, with the given lock rank and lock name
 *
 */

NSSRWLock *
NSSRWLock_New(PRUint32 lock_rank, const char *lock_name)
{
    NSSRWLock *rwlock;

    rwlock = PR_NEWZAP(NSSRWLock);
    if (rwlock == NULL)
        return NULL;

    rwlock->rw_lock = PZ_NewLock(nssILockRWLock);
    if (rwlock->rw_lock == NULL) {
	goto loser;
    }
    rwlock->rw_reader_waitq = PZ_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_reader_waitq == NULL) {
	goto loser;
    }
    rwlock->rw_writer_waitq = PZ_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_writer_waitq == NULL) {
	goto loser;
    }
    if (lock_name != NULL) {
        rwlock->rw_name = (char*) PR_Malloc((PRUint32)strlen(lock_name) + 1);
        if (rwlock->rw_name == NULL) {
	    goto loser;
        }
        strcpy(rwlock->rw_name, lock_name);
    } else {
        rwlock->rw_name = NULL;
    }
    rwlock->rw_rank            = lock_rank;
    rwlock->rw_waiting_readers = 0;
    rwlock->rw_waiting_writers = 0;
    rwlock->rw_reader_locks    = 0;
    rwlock->rw_writer_locks    = 0;

    return rwlock;

loser:
    NSSRWLock_Destroy(rwlock);
    return(NULL);
}

/*
** Destroy the given RWLock "lock".
*/
void
NSSRWLock_Destroy(NSSRWLock *rwlock)
{
    PR_ASSERT(rwlock != NULL);
    PR_ASSERT(rwlock->rw_waiting_readers == 0);

    /* XXX Shouldn't we lock the PZLock before destroying this?? */

    if (rwlock->rw_name)
    	PR_Free(rwlock->rw_name);
    if (rwlock->rw_reader_waitq)
    	PZ_DestroyCondVar(rwlock->rw_reader_waitq);
    if (rwlock->rw_writer_waitq)
	PZ_DestroyCondVar(rwlock->rw_writer_waitq);
    if (rwlock->rw_lock)
	PZ_DestroyLock(rwlock->rw_lock);
    PR_DELETE(rwlock);
}

/*
** Read-lock the RWLock.
*/
void
NSSRWLock_LockRead(NSSRWLock *rwlock)
{
    PRThread *me = PR_GetCurrentThread();

    PZ_Lock(rwlock->rw_lock);
#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG

    /*
     * assert that rank ordering is not violated; the rank of 'rwlock' should
     * be equal to or greater than the highest rank of all the locks held by
     * the thread.
     */
    PR_ASSERT((rwlock->rw_rank == NSS_RWLOCK_RANK_NONE) ||
              (rwlock->rw_rank >= nssRWLock_GetThreadRank(me)));
#endif
    /*
     * wait if write-locked or if a writer is waiting; preference for writers
     */
    UNTIL ( (rwlock->rw_owner == me) ||		  /* I own it, or        */
	   ((rwlock->rw_owner == NULL) &&	  /* no-one owns it, and */
	    (rwlock->rw_waiting_writers == 0))) { /* no-one is waiting to own */

	rwlock->rw_waiting_readers++;
	PZ_WaitCondVar(rwlock->rw_reader_waitq, PR_INTERVAL_NO_TIMEOUT);
	rwlock->rw_waiting_readers--;
    }
    rwlock->rw_reader_locks++; 		/* Increment read-lock count */

    PZ_Unlock(rwlock->rw_lock);

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG
    nssRWLock_SetThreadRank(me, rwlock);/* update thread's lock rank */
#endif
}

/* Unlock a Read lock held on this RW lock.
*/
void
NSSRWLock_UnlockRead(NSSRWLock *rwlock)
{
    PZ_Lock(rwlock->rw_lock);

    PR_ASSERT(rwlock->rw_reader_locks > 0); /* lock must be read locked */

    if ((  rwlock->rw_reader_locks  > 0)  &&	/* caller isn't screwey */
        (--rwlock->rw_reader_locks == 0)  &&	/* not read locked any more */
	(  rwlock->rw_owner        == NULL) &&	/* not write locked */
	(  rwlock->rw_waiting_writers > 0)) {	/* someone's waiting. */

	PZ_NotifyCondVar(rwlock->rw_writer_waitq); /* wake him up. */
    }

    PZ_Unlock(rwlock->rw_lock);

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG
    /*
     * update thread's lock rank
     */
    nssRWLock_UnsetThreadRank(me, rwlock);
#endif
    return;
}

/*
** Write-lock the RWLock.
*/
void
NSSRWLock_LockWrite(NSSRWLock *rwlock)
{
    PRThread *me = PR_GetCurrentThread();

    PZ_Lock(rwlock->rw_lock);
#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG
    /*
     * assert that rank ordering is not violated; the rank of 'rwlock' should
     * be equal to or greater than the highest rank of all the locks held by
     * the thread.
     */
    PR_ASSERT((rwlock->rw_rank == NSS_RWLOCK_RANK_NONE) ||
                    (rwlock->rw_rank >= nssRWLock_GetThreadRank(me)));
#endif
    /*
     * wait if read locked or write locked.
     */
    PR_ASSERT(rwlock->rw_reader_locks >= 0);
    PR_ASSERT(me != NULL);

    UNTIL ( (rwlock->rw_owner == me) ||           /* I own write lock, or */
	   ((rwlock->rw_owner == NULL) &&	  /* no writer   and */
	    (rwlock->rw_reader_locks == 0))) {    /* no readers, either. */

        rwlock->rw_waiting_writers++;
        PZ_WaitCondVar(rwlock->rw_writer_waitq, PR_INTERVAL_NO_TIMEOUT);
        rwlock->rw_waiting_writers--;
	PR_ASSERT(rwlock->rw_reader_locks >= 0);
    }

    PR_ASSERT(rwlock->rw_reader_locks == 0);
    /*
     * apply write lock
     */
    rwlock->rw_owner = me;
    rwlock->rw_writer_locks++; 		/* Increment write-lock count */

    PZ_Unlock(rwlock->rw_lock);

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG
    /*
     * update thread's lock rank
     */
    nssRWLock_SetThreadRank(me,rwlock);
#endif
}

/* Unlock a Read lock held on this RW lock.
*/
void
NSSRWLock_UnlockWrite(NSSRWLock *rwlock)
{
    PRThread *me = PR_GetCurrentThread();

    PZ_Lock(rwlock->rw_lock);
    PR_ASSERT(rwlock->rw_owner == me); /* lock must be write-locked by me.  */
    PR_ASSERT(rwlock->rw_writer_locks > 0); /* lock must be write locked */

    if (  rwlock->rw_owner        == me  &&	/* I own it, and            */
          rwlock->rw_writer_locks  > 0   &&	/* I own it, and            */
        --rwlock->rw_writer_locks == 0) {	/* I'm all done with it     */

	rwlock->rw_owner = NULL;		/* I don't own it any more. */

	/* Give preference to waiting writers. */
	if (rwlock->rw_waiting_writers > 0) {
	    if (rwlock->rw_reader_locks == 0)
		PZ_NotifyCondVar(rwlock->rw_writer_waitq);
	} else if (rwlock->rw_waiting_readers > 0) {
	    PZ_NotifyAllCondVar(rwlock->rw_reader_waitq);
	}
    }
    PZ_Unlock(rwlock->rw_lock);

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG
    /*
     * update thread's lock rank
     */
    nssRWLock_UnsetThreadRank(me, rwlock);
#endif
    return;
}

/* This is primarily for debugging, i.e. for inclusion in ASSERT calls. */
PRBool
NSSRWLock_HaveWriteLock(NSSRWLock *rwlock) {
    PRBool ownWriteLock;
    PRThread *me = PR_GetCurrentThread();

    /* This lock call isn't really necessary.
    ** If this thread is the owner, that fact cannot change during this call,
    ** because this thread is in this call.
    ** If this thread is NOT the owner, the owner could change, but it 
    ** could not become this thread.  
    */
#if UNNECESSARY
    PZ_Lock(rwlock->rw_lock);	
#endif
    ownWriteLock = (PRBool)(me == rwlock->rw_owner);
#if UNNECESSARY
    PZ_Unlock(rwlock->rw_lock);
#endif
    return ownWriteLock;
}

#ifdef NSS_RWLOCK_RANK_ORDER_DEBUG

/*
 * nssRWLock_SetThreadRank
 *      Set a thread's lock rank, which is the highest of the ranks of all
 *      the locks held by the thread. Pointers to the locks are added to a
 *      per-thread list, which is anchored off a thread-private data key.
 */

static void
nssRWLock_SetThreadRank(PRThread *me, NSSRWLock *rwlock)
{
    thread_rwlock_stack *lock_stack;
    PRStatus rv;

    /*
     * allocated thread-private-data for rwlock list, if not already allocated
     */
    if (!nss_thread_rwlock_initialized) {
        /*
         * allocate tpd, only if not failed already
         */
        if (!nss_thread_rwlock_alloc_failed) {
            if (PR_NewThreadPrivateIndex(&nss_thread_rwlock,
                                        nssRWLock_ReleaseLockStack)
                                                == PR_FAILURE) {
                nss_thread_rwlock_alloc_failed = 1;
                return;
            }
        } else
            return;
    }
    /*
     * allocate a lock stack
     */
    if ((lock_stack = PR_GetThreadPrivate(nss_thread_rwlock)) == NULL) {
        lock_stack = (thread_rwlock_stack *)
                        PR_CALLOC(1 * sizeof(thread_rwlock_stack));
        if (lock_stack) {
            rv = PR_SetThreadPrivate(nss_thread_rwlock, lock_stack);
            if (rv == PR_FAILURE) {
                PR_DELETE(lock_stack);
                nss_thread_rwlock_alloc_failed = 1;
                return;
            }
        } else {
            nss_thread_rwlock_alloc_failed = 1;
            return;
        }
    }
    /*
     * add rwlock to lock stack, if limit is not exceeded
     */
    if (lock_stack) {
        if (lock_stack->trs_index < _NSS_RWLOCK_RANK_ORDER_LIMIT)
            lock_stack->trs_stack[lock_stack->trs_index++] = rwlock;
    }
    nss_thread_rwlock_initialized = 1;
}

static void
nssRWLock_ReleaseLockStack(void *lock_stack)
{
    PR_ASSERT(lock_stack);
    PR_DELETE(lock_stack);
}

/*
 * nssRWLock_GetThreadRank
 *
 *      return thread's lock rank. If thread-private-data for the lock
 *      stack is not allocated, return NSS_RWLOCK_RANK_NONE.
 */

static PRUint32
nssRWLock_GetThreadRank(PRThread *me)
{
    thread_rwlock_stack *lock_stack;

    if (nss_thread_rwlock_initialized) {
        if ((lock_stack = PR_GetThreadPrivate(nss_thread_rwlock)) == NULL)
            return (NSS_RWLOCK_RANK_NONE);
        else
            return(lock_stack->trs_stack[lock_stack->trs_index - 1]->rw_rank);

    } else
            return (NSS_RWLOCK_RANK_NONE);
}

/*
 * nssRWLock_UnsetThreadRank
 *
 *      remove the rwlock from the lock stack. Since locks may not be
 *      unlocked in a FIFO order, the entire lock stack is searched.
 */

static void
nssRWLock_UnsetThreadRank(PRThread *me, NSSRWLock *rwlock)
{
    thread_rwlock_stack *lock_stack;
    int new_index = 0, index, done = 0;

    if (!nss_thread_rwlock_initialized)
        return;

    lock_stack = PR_GetThreadPrivate(nss_thread_rwlock);

    PR_ASSERT(lock_stack != NULL);

    index = lock_stack->trs_index - 1;
    while (index-- >= 0) {
        if ((lock_stack->trs_stack[index] == rwlock) && !done)  {
            /*
             * reset the slot for rwlock
             */
            lock_stack->trs_stack[index] = NULL;
            done = 1;
        }
        /*
         * search for the lowest-numbered empty slot, above which there are
         * no non-empty slots
         */
        if ((lock_stack->trs_stack[index] != NULL) && !new_index)
            new_index = index + 1;
        if (done && new_index)
            break;
    }
    /*
     * set top of stack to highest numbered empty slot
     */
    lock_stack->trs_index = new_index;

}

#endif  /* NSS_RWLOCK_RANK_ORDER_DEBUG */
