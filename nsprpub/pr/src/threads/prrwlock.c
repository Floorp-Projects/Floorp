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

#include "nspr.h"
#include "prrwlock.h"

#include <string.h>

/*
 * Reader-writer lock
 */
struct PRRWLock {
    PRLock			*rw_lock;
	char			*rw_name;			/* lock name					*/
	PRUint32		rw_rank;			/* rank of the lock				*/
	PRInt32			rw_lock_cnt;		/* ==  0, if unlocked			*/
										/* == -1, if write-locked		*/
										/* > 0	, # of read locks		*/
	PRUint32		rw_reader_cnt;		/* number of waiting readers	*/
	PRUint32		rw_writer_cnt;		/* number of waiting writers	*/
	PRCondVar   	*rw_reader_waitq;	/* cvar for readers 			*/
	PRCondVar   	*rw_writer_waitq;	/* cvar for writers				*/
    PRThread 		*rw_owner;			/* lock owner for write-lock	*/
};

#ifdef DEBUG
#define _PR_RWLOCK_RANK_ORDER_DEBUG	/* enable deadlock detection using
									   rank-order for locks
									*/
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG

static PRUintn	ps_thread_rwlock_initialized;
static PRUintn	ps_thread_rwlock;				/* TPD key for lock stack */
static PRUintn	ps_thread_rwlock_alloc_failed;

#define	_PR_RWLOCK_RANK_ORDER_LIMIT	10

typedef struct thread_rwlock_stack {
	PRInt32		trs_index;									/* top of stack */
	PRRWLock	*trs_stack[_PR_RWLOCK_RANK_ORDER_LIMIT];	/* stack of lock
														 	   pointers */

} thread_rwlock_stack;

static void _PR_SET_THREAD_RWLOCK_RANK(PRThread *me, PRRWLock *rwlock);
static PRUint32 _PR_GET_THREAD_RWLOCK_RANK(PRThread *me);
static void _PR_UNSET_THREAD_RWLOCK_RANK(PRThread *me, PRRWLock *rwlock);
static void _PR_RELEASE_LOCK_STACK(void *lock_stack);

#endif

/*
 * Reader/Writer Locks
 */

/*
 * PR_NewRWLock
 *		Create a reader-writer lock, with the given lock rank and lock name
 *	
 */

PR_IMPLEMENT(PRRWLock *)
PR_NewRWLock(PRUint32 lock_rank, const char *lock_name)
{
    PRRWLock *rwlock;

    rwlock = PR_NEWZAP(PRRWLock);
    if (rwlock == NULL)
		return NULL;
	
	rwlock->rw_lock = PR_NewLock();
    if (rwlock->rw_lock == NULL) {
		PR_DELETE(rwlock);
		return(NULL);
	}
	rwlock->rw_reader_waitq = PR_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_reader_waitq == NULL) {
		PR_DestroyLock(rwlock->rw_lock);
		PR_DELETE(rwlock);
		return(NULL);
	}
	rwlock->rw_writer_waitq = PR_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_writer_waitq == NULL) {
		PR_DestroyCondVar(rwlock->rw_reader_waitq);	
		PR_DestroyLock(rwlock->rw_lock);
		PR_DELETE(rwlock);
		return(NULL);
	}
	if (lock_name != NULL) {
		rwlock->rw_name = (char*) PR_Malloc(strlen(lock_name) + 1);
    	if (rwlock->rw_name == NULL) {
			PR_DestroyCondVar(rwlock->rw_reader_waitq);	
			PR_DestroyCondVar(rwlock->rw_writer_waitq);	
			PR_DestroyLock(rwlock->rw_lock);
			PR_DELETE(rwlock);
			return(NULL);
		}
		strcpy(rwlock->rw_name, lock_name);
	} else {
		rwlock->rw_name = NULL;
	}
	rwlock->rw_rank = lock_rank;
	rwlock->rw_reader_cnt = 0;
	rwlock->rw_writer_cnt = 0;
	rwlock->rw_lock_cnt = 0;

    return rwlock;
}

/*
** Destroy the given RWLock "lock".
*/
PR_IMPLEMENT(void)
PR_DestroyRWLock(PRRWLock *rwlock)
{
	PR_ASSERT(rwlock->rw_reader_cnt == 0);
	PR_DestroyCondVar(rwlock->rw_reader_waitq);	
	PR_DestroyCondVar(rwlock->rw_writer_waitq);	
	PR_DestroyLock(rwlock->rw_lock);
	if (rwlock->rw_name != NULL)
		PR_Free(rwlock->rw_name);
    PR_DELETE(rwlock);
}

/*
** Read-lock the RWLock.
*/
PR_IMPLEMENT(void)
PR_RWLock_Rlock(PRRWLock *rwlock)
{
PRThread *me = PR_GetCurrentThread();

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG

	/*
	 * assert that rank ordering is not violated; the rank of 'rwlock' should
	 * be equal to or greater than the highest rank of all the locks held by
	 * the thread.
	 */
	PR_ASSERT((rwlock->rw_rank == PR_RWLOCK_RANK_NONE) || 
					(rwlock->rw_rank >= _PR_GET_THREAD_RWLOCK_RANK(me)));
#endif
	PR_Lock(rwlock->rw_lock);
	/*
	 * wait if write-locked or if a writer is waiting; preference for writers
	 */
	while ((rwlock->rw_lock_cnt < 0) ||
			(rwlock->rw_writer_cnt > 0)) {
		rwlock->rw_reader_cnt++;
		PR_WaitCondVar(rwlock->rw_reader_waitq, PR_INTERVAL_NO_TIMEOUT);
		rwlock->rw_reader_cnt--;
	}
	/*
	 * Increment read-lock count
	 */
	rwlock->rw_lock_cnt++;

	PR_Unlock(rwlock->rw_lock);

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_SET_THREAD_RWLOCK_RANK(me,rwlock);
#endif
}

/*
** Write-lock the RWLock.
*/
PR_IMPLEMENT(void)
PR_RWLock_Wlock(PRRWLock *rwlock)
{
PRInt32 lock_acquired = 0;
PRThread *me = PR_GetCurrentThread();

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * assert that rank ordering is not violated; the rank of 'rwlock' should
	 * be equal to or greater than the highest rank of all the locks held by
	 * the thread.
	 */
	PR_ASSERT((rwlock->rw_rank == PR_RWLOCK_RANK_NONE) || 
					(rwlock->rw_rank >= _PR_GET_THREAD_RWLOCK_RANK(me)));
#endif
	PR_Lock(rwlock->rw_lock);
	/*
	 * wait if read locked
	 */
	while (rwlock->rw_lock_cnt != 0) {
		rwlock->rw_writer_cnt++;
		PR_WaitCondVar(rwlock->rw_writer_waitq, PR_INTERVAL_NO_TIMEOUT);
		rwlock->rw_writer_cnt--;
	}
	/*
	 * apply write lock
	 */
	rwlock->rw_lock_cnt--;
	PR_ASSERT(rwlock->rw_lock_cnt == -1);
	PR_ASSERT(me != NULL);
    rwlock->rw_owner = me;
	PR_Unlock(rwlock->rw_lock);

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_SET_THREAD_RWLOCK_RANK(me,rwlock);
#endif
}

/*
** Unlock the RW lock.
*/
PR_IMPLEMENT(void)
PR_RWLock_Unlock(PRRWLock *rwlock)
{
PRThread *me = PR_GetCurrentThread();

	PR_Lock(rwlock->rw_lock);
	/*
	 * lock must be read or write-locked
	 */
	PR_ASSERT(rwlock->rw_lock_cnt != 0);
	if (rwlock->rw_lock_cnt > 0) {

		/*
		 * decrement read-lock count
		 */
		rwlock->rw_lock_cnt--;
		if (rwlock->rw_lock_cnt == 0) {
			/*
			 * lock is not read-locked anymore; wakeup a waiting writer
			 */
			if (rwlock->rw_writer_cnt > 0)
				PR_NotifyCondVar(rwlock->rw_writer_waitq);
		}
	} else {
		PRThread *me = PR_GetCurrentThread();

		PR_ASSERT(rwlock->rw_lock_cnt == -1);

		rwlock->rw_lock_cnt = 0;
    	PR_ASSERT(rwlock->rw_owner == me);
    	rwlock->rw_owner = NULL;
		/*
		 * wakeup a writer, if present; preference for writers
		 */
		if (rwlock->rw_writer_cnt > 0)
			PR_NotifyCondVar(rwlock->rw_writer_waitq);
		/*
		 * else, wakeup all readers, if any
		 */
		else if (rwlock->rw_reader_cnt > 0)
			PR_NotifyAllCondVar(rwlock->rw_reader_waitq);
	}
	PR_Unlock(rwlock->rw_lock);

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_UNSET_THREAD_RWLOCK_RANK(me, rwlock);
#endif
	return;
}

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG

/*
 * _PR_SET_THREAD_RWLOCK_RANK
 *		Set a thread's lock rank, which is the highest of the ranks of all
 *		the locks held by the thread. Pointers to the locks are added to a
 *		per-thread list, which is anchored off a thread-private data key.
 */

void
_PR_SET_THREAD_RWLOCK_RANK(PRThread *me, PRRWLock *rwlock)
{
thread_rwlock_stack *lock_stack;
PRStatus rv;

	/*
	 * allocated thread-private-data for rwlock list, if not already allocated
	 */
	if (!ps_thread_rwlock_initialized) {
		/*
		 * allocate tpd, only if not failed already
		 */
		if (!ps_thread_rwlock_alloc_failed) {
			if (PR_NewThreadPrivateIndex(&ps_thread_rwlock,
										_PR_RELEASE_LOCK_STACK)
												== PR_FAILURE) {
				ps_thread_rwlock_alloc_failed = 1;
				return;
			}
		} else
			return;
	}
	/*
	 * allocate a lock stack
	 */
	if ((lock_stack = PR_GetThreadPrivate(ps_thread_rwlock)) == NULL) {
		lock_stack = (thread_rwlock_stack *)
						PR_CALLOC(1 * sizeof(thread_rwlock_stack));
		if (lock_stack) {
			rv = PR_SetThreadPrivate(ps_thread_rwlock, lock_stack);
			if (rv == PR_FAILURE) {
				PR_DELETE(lock_stack);
				ps_thread_rwlock_alloc_failed = 1;
				return;
			}
		} else {
			ps_thread_rwlock_alloc_failed = 1;
			return;
		}
	}
	/*
	 * add rwlock to lock stack, if limit is not exceeded
	 */
	if (lock_stack) {
		if (lock_stack->trs_index < _PR_RWLOCK_RANK_ORDER_LIMIT)
			lock_stack->trs_stack[lock_stack->trs_index++] = rwlock;	
	}
	ps_thread_rwlock_initialized = 1;
}

void
_PR_RELEASE_LOCK_STACK(void *lock_stack)
{
	PR_ASSERT(lock_stack);
	PR_DELETE(lock_stack);
}

/*
 * _PR_GET_THREAD_RWLOCK_RANK
 *
 *		return thread's lock rank. If thread-private-data for the lock
 *		stack is not allocated, return PR_RWLOCK_RANK_NONE.
 */
	
PRUint32
_PR_GET_THREAD_RWLOCK_RANK(PRThread *me)
{
	thread_rwlock_stack *lock_stack;

	if (ps_thread_rwlock_initialized) {
		if ((lock_stack = PR_GetThreadPrivate(ps_thread_rwlock)) == NULL)
			return (PR_RWLOCK_RANK_NONE);
		else
			return(lock_stack->trs_stack[lock_stack->trs_index - 1]->rw_rank);

	} else
			return (PR_RWLOCK_RANK_NONE);
}

/*
 * _PR_UNSET_THREAD_RWLOCK_RANK
 *
 *		remove the rwlock from the lock stack. Since locks may not be
 *		unlocked in a FIFO order, the entire lock stack is searched.
 */
	
void
_PR_UNSET_THREAD_RWLOCK_RANK(PRThread *me, PRRWLock *rwlock)
{
	thread_rwlock_stack *lock_stack;
	int new_index = 0, index, done = 0;

	if (!ps_thread_rwlock_initialized)
		return;

	lock_stack = PR_GetThreadPrivate(ps_thread_rwlock);

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

#endif 	/* _PR_RWLOCK_RANK_ORDER_DEBUG */
