/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "primpl.h"

#include <string.h>

#if defined(HPUX) && defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)

#include <pthread.h>
#define HAVE_UNIX98_RWLOCK
#define RWLOCK_T pthread_rwlock_t
#define RWLOCK_INIT(lock) pthread_rwlock_init(lock, NULL)
#define RWLOCK_DESTROY(lock) pthread_rwlock_destroy(lock)
#define RWLOCK_RDLOCK(lock) pthread_rwlock_rdlock(lock)
#define RWLOCK_WRLOCK(lock) pthread_rwlock_wrlock(lock)
#define RWLOCK_UNLOCK(lock) pthread_rwlock_unlock(lock)

#elif defined(SOLARIS) && (defined(_PR_PTHREADS) \
        || defined(_PR_GLOBAL_THREADS_ONLY))

#include <synch.h>
#define HAVE_UI_RWLOCK
#define RWLOCK_T rwlock_t
#define RWLOCK_INIT(lock) rwlock_init(lock, USYNC_THREAD, NULL)
#define RWLOCK_DESTROY(lock) rwlock_destroy(lock)
#define RWLOCK_RDLOCK(lock) rw_rdlock(lock)
#define RWLOCK_WRLOCK(lock) rw_wrlock(lock)
#define RWLOCK_UNLOCK(lock) rw_unlock(lock)

#endif

/*
 * Reader-writer lock
 */
struct PRRWLock {
	char			*rw_name;			/* lock name					*/
	PRUint32		rw_rank;			/* rank of the lock				*/

#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	RWLOCK_T		rw_lock;
#else
    PRLock			*rw_lock;
	PRInt32			rw_lock_cnt;		/* ==  0, if unlocked			*/
										/* == -1, if write-locked		*/
										/* > 0	, # of read locks		*/
	PRUint32		rw_reader_cnt;		/* number of waiting readers	*/
	PRUint32		rw_writer_cnt;		/* number of waiting writers	*/
	PRCondVar   	*rw_reader_waitq;	/* cvar for readers 			*/
	PRCondVar   	*rw_writer_waitq;	/* cvar for writers				*/
#ifdef DEBUG
    PRThread 		*rw_owner;			/* lock owner for write-lock	*/
#endif
#endif
};

#ifdef DEBUG
#define _PR_RWLOCK_RANK_ORDER_DEBUG	/* enable deadlock detection using
									   rank-order for locks
									*/
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG

static PRUintn	pr_thread_rwlock_key;			/* TPD key for lock stack */
static PRUintn	pr_thread_rwlock_alloc_failed;

#define	_PR_RWLOCK_RANK_ORDER_LIMIT	10

typedef struct thread_rwlock_stack {
	PRInt32		trs_index;									/* top of stack */
	PRRWLock	*trs_stack[_PR_RWLOCK_RANK_ORDER_LIMIT];	/* stack of lock
														 	   pointers */

} thread_rwlock_stack;

static void _PR_SET_THREAD_RWLOCK_RANK(PRRWLock *rwlock);
static PRUint32 _PR_GET_THREAD_RWLOCK_RANK(void);
static void _PR_UNSET_THREAD_RWLOCK_RANK(PRRWLock *rwlock);
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
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	int err;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

    rwlock = PR_NEWZAP(PRRWLock);
    if (rwlock == NULL)
		return NULL;

	rwlock->rw_rank = lock_rank;
	if (lock_name != NULL) {
		rwlock->rw_name = (char*) PR_Malloc(strlen(lock_name) + 1);
    	if (rwlock->rw_name == NULL) {
			PR_DELETE(rwlock);
			return(NULL);
		}
		strcpy(rwlock->rw_name, lock_name);
	} else {
		rwlock->rw_name = NULL;
	}
	
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	err = RWLOCK_INIT(&rwlock->rw_lock);
	if (err != 0) {
		PR_SetError(PR_UNKNOWN_ERROR, err);
		PR_Free(rwlock->rw_name);
		PR_DELETE(rwlock);
		return NULL;
	}
	return rwlock;
#else
	rwlock->rw_lock = PR_NewLock();
    if (rwlock->rw_lock == NULL) {
		goto failed;
	}
	rwlock->rw_reader_waitq = PR_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_reader_waitq == NULL) {
		goto failed;
	}
	rwlock->rw_writer_waitq = PR_NewCondVar(rwlock->rw_lock);
    if (rwlock->rw_writer_waitq == NULL) {
		goto failed;
	}
	rwlock->rw_reader_cnt = 0;
	rwlock->rw_writer_cnt = 0;
	rwlock->rw_lock_cnt = 0;
	return rwlock;

failed:
	if (rwlock->rw_reader_waitq != NULL) {
		PR_DestroyCondVar(rwlock->rw_reader_waitq);	
	}
	if (rwlock->rw_lock != NULL) {
		PR_DestroyLock(rwlock->rw_lock);
	}
	PR_Free(rwlock->rw_name);
	PR_DELETE(rwlock);
	return NULL;
#endif
}

/*
** Destroy the given RWLock "lock".
*/
PR_IMPLEMENT(void)
PR_DestroyRWLock(PRRWLock *rwlock)
{
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	int err;
	err = RWLOCK_DESTROY(&rwlock->rw_lock);
	PR_ASSERT(err == 0);
#else
	PR_ASSERT(rwlock->rw_reader_cnt == 0);
	PR_DestroyCondVar(rwlock->rw_reader_waitq);	
	PR_DestroyCondVar(rwlock->rw_writer_waitq);	
	PR_DestroyLock(rwlock->rw_lock);
#endif
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
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
int err;
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * assert that rank ordering is not violated; the rank of 'rwlock' should
	 * be equal to or greater than the highest rank of all the locks held by
	 * the thread.
	 */
	PR_ASSERT((rwlock->rw_rank == PR_RWLOCK_RANK_NONE) || 
					(rwlock->rw_rank >= _PR_GET_THREAD_RWLOCK_RANK()));
#endif

#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	err = RWLOCK_RDLOCK(&rwlock->rw_lock);
	PR_ASSERT(err == 0);
#else
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
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_SET_THREAD_RWLOCK_RANK(rwlock);
#endif
}

/*
** Write-lock the RWLock.
*/
PR_IMPLEMENT(void)
PR_RWLock_Wlock(PRRWLock *rwlock)
{
#if defined(DEBUG)
PRThread *me = PR_GetCurrentThread();
#endif
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
int err;
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * assert that rank ordering is not violated; the rank of 'rwlock' should
	 * be equal to or greater than the highest rank of all the locks held by
	 * the thread.
	 */
	PR_ASSERT((rwlock->rw_rank == PR_RWLOCK_RANK_NONE) || 
					(rwlock->rw_rank >= _PR_GET_THREAD_RWLOCK_RANK()));
#endif

#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	err = RWLOCK_WRLOCK(&rwlock->rw_lock);
	PR_ASSERT(err == 0);
#else
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
#ifdef DEBUG
	PR_ASSERT(me != NULL);
	rwlock->rw_owner = me;
#endif
	PR_Unlock(rwlock->rw_lock);
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_SET_THREAD_RWLOCK_RANK(rwlock);
#endif
}

/*
** Unlock the RW lock.
*/
PR_IMPLEMENT(void)
PR_RWLock_Unlock(PRRWLock *rwlock)
{
#if defined(DEBUG)
PRThread *me = PR_GetCurrentThread();
#endif
#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
int err;
#endif

#if defined(HAVE_UNIX98_RWLOCK) || defined(HAVE_UI_RWLOCK)
	err = RWLOCK_UNLOCK(&rwlock->rw_lock);
	PR_ASSERT(err == 0);
#else
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
		PR_ASSERT(rwlock->rw_lock_cnt == -1);

		rwlock->rw_lock_cnt = 0;
#ifdef DEBUG
    	PR_ASSERT(rwlock->rw_owner == me);
    	rwlock->rw_owner = NULL;
#endif
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
#endif

#ifdef _PR_RWLOCK_RANK_ORDER_DEBUG
	/*
	 * update thread's lock rank
	 */
	_PR_UNSET_THREAD_RWLOCK_RANK(rwlock);
#endif
	return;
}

#ifndef _PR_RWLOCK_RANK_ORDER_DEBUG

void _PR_InitRWLocks(void) { }

#else

void _PR_InitRWLocks(void)
{
	/*
	 * allocated thread-private-data index for rwlock list
	 */
	if (PR_NewThreadPrivateIndex(&pr_thread_rwlock_key,
			_PR_RELEASE_LOCK_STACK) == PR_FAILURE) {
		pr_thread_rwlock_alloc_failed = 1;
		return;
	}
}

/*
 * _PR_SET_THREAD_RWLOCK_RANK
 *		Set a thread's lock rank, which is the highest of the ranks of all
 *		the locks held by the thread. Pointers to the locks are added to a
 *		per-thread list, which is anchored off a thread-private data key.
 */

static void
_PR_SET_THREAD_RWLOCK_RANK(PRRWLock *rwlock)
{
thread_rwlock_stack *lock_stack;
PRStatus rv;

	/*
	 * allocate a lock stack
	 */
	if ((lock_stack = PR_GetThreadPrivate(pr_thread_rwlock_key)) == NULL) {
		lock_stack = (thread_rwlock_stack *)
						PR_CALLOC(1 * sizeof(thread_rwlock_stack));
		if (lock_stack) {
			rv = PR_SetThreadPrivate(pr_thread_rwlock_key, lock_stack);
			if (rv == PR_FAILURE) {
				PR_DELETE(lock_stack);
				pr_thread_rwlock_alloc_failed = 1;
				return;
			}
		} else {
			pr_thread_rwlock_alloc_failed = 1;
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
}

static void
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
	
static PRUint32
_PR_GET_THREAD_RWLOCK_RANK(void)
{
	thread_rwlock_stack *lock_stack;

	if ((lock_stack = PR_GetThreadPrivate(pr_thread_rwlock_key)) == NULL)
		return (PR_RWLOCK_RANK_NONE);
	else
		return(lock_stack->trs_stack[lock_stack->trs_index - 1]->rw_rank);
}

/*
 * _PR_UNSET_THREAD_RWLOCK_RANK
 *
 *		remove the rwlock from the lock stack. Since locks may not be
 *		unlocked in a FIFO order, the entire lock stack is searched.
 */
	
static void
_PR_UNSET_THREAD_RWLOCK_RANK(PRRWLock *rwlock)
{
	thread_rwlock_stack *lock_stack;
	int new_index = 0, index, done = 0;

	lock_stack = PR_GetThreadPrivate(pr_thread_rwlock_key);

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
