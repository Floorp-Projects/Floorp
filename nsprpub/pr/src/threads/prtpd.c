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

#include "primpl.h"

#include <string.h>

/*
** Thread Private Data
**
** There is an aribitrary limit on the number of keys that will be allocated
** by the runtime. It's largish, so it is intended to be a sanity check, not
** an impediment.
**
** There is a counter, initialized to zero and incremented every time a
** client asks for a new key, that holds the high water mark for keys. All
** threads logically have the same high water mark and are permitted to
** ask for TPD up to that key value.
**
** The vector to hold the TPD are allocated when PR_SetThreadPrivate() is
** called. The size of the vector will be some value greater than or equal
** to the current high water mark. Each thread has its own TPD length and
** vector.
**
** Threads that get private data for keys they have not set (or perhaps
** don't even exist for that thread) get a NULL return. If the key is
** beyond the high water mark, an error will be returned.
*/

#define _PR_TPD_MODULO 8                /* vectors are extended by this much */
#define _PR_TPD_LIMIT 128               /* arbitary limit on the TPD slots */
static PRUintn _pr_tpd_highwater = 0;   /* next TPD key to be assigned */
static PRUintn _pr_tpd_length = 0;      /* current length of destructor vector */
PRThreadPrivateDTOR *_pr_tpd_destructors = NULL;
                                        /* the destructors are associated with
                                            the keys, therefore asserting that
                                            the TPD key depicts the data's 'type' */

/* Lock protecting the index assignment of per-thread-private data table */
#ifdef _PR_NO_PREEMPT
#define _PR_NEW_LOCK_TPINDEX()
#define _PR_FREE_LOCK_TPINDEX()
#define _PR_LOCK_TPINDEX()
#define _PR_UNLOCK_TPINDEX()
#else
#ifdef _PR_LOCAL_THREADS_ONLY
#define _PR_NEW_LOCK_TPINDEX()
#define _PR_FREE_LOCK_TPINDEX()
#define _PR_LOCK_TPINDEX() _PR_INTSOFF(_is)
#define _PR_UNLOCK_TPINDEX() _PR_INTSON(_is)
#else
static PRLock *_pr_threadPrivateIndexLock;
#define _PR_NEW_LOCK_TPINDEX() (_pr_threadPrivateIndexLock = PR_NewLock())
#define _PR_FREE_LOCK_TPINDEX() (PR_DestroyLock(_pr_threadPrivateIndexLock))
#define _PR_LOCK_TPINDEX() PR_Lock(_pr_threadPrivateIndexLock)
#define _PR_UNLOCK_TPINDEX() PR_Unlock(_pr_threadPrivateIndexLock)
#endif
#endif

/*
** Initialize the thread private data manipulation
*/
void _PR_InitTPD()
{
	_PR_NEW_LOCK_TPINDEX();
}

/*
** Clean up the thread private data manipulation
*/
void _PR_CleanupTPD(void)
{
	_PR_FREE_LOCK_TPINDEX();
}

/*
** This routine returns a new index for per-thread-private data table. 
** The index is visible to all threads within a process. This index can 
** be used with the PR_SetThreadPrivate() and PR_GetThreadPrivate() routines 
** to save and retrieve data associated with the index for a thread.
**
** The index independently maintains specific values for each binding thread. 
** A thread can only get access to its own thread-specific-data.
**
** Upon a new index return the value associated with the index for all threads
** is NULL, and upon thread creation the value associated with all indices for 
** that thread is NULL. 
**
** 	"dtor" is the destructor function to invoke when the private
**	   data is destroyed
**
** Returns PR_FAILURE if the total number of indices will exceed the maximun 
** allowed.
*/

PR_IMPLEMENT(PRStatus) PR_NewThreadPrivateIndex(
    PRUintn *newIndex, PRThreadPrivateDTOR dtor)
{
    PRStatus rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();


    if (_pr_tpd_highwater >= _PR_TPD_LIMIT)
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
	    rv = PR_FAILURE;  /* that's just wrong */
    }
    else
    {
        PRThreadPrivateDTOR *old = NULL;
		PRIntn _is;

        _PR_LOCK_TPINDEX();
        if (_pr_tpd_highwater >= _pr_tpd_length)
        {
            old = _pr_tpd_destructors;
            _pr_tpd_destructors = PR_CALLOC(
                (_pr_tpd_length + _PR_TPD_MODULO) * sizeof(PRThreadPrivateDTOR*));
            if (NULL == _pr_tpd_destructors)
            {
                _pr_tpd_destructors = old; old = NULL;
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	            rv = PR_FAILURE;  /* that's just wrong */
	            goto failed;  /* then extract one's self */
	        }
	        else
	        {
                memcpy(
                    _pr_tpd_destructors, old,
                    _pr_tpd_length * sizeof(PRThreadPrivateDTOR*));
                _pr_tpd_length += _PR_TPD_MODULO;
	        }
        }
        
        *newIndex = _pr_tpd_highwater++;  /* this is really all we wanted */
        _pr_tpd_destructors[*newIndex] = dtor;  /* record destructor @index */

failed:
        _PR_UNLOCK_TPINDEX();
        if (NULL != old) PR_DELETE(old);
        rv = PR_SUCCESS;
    }

    return rv;
}

/*
** Define some per-thread-private data.
** 	"index" is an index into the per-thread private data table
** 	"priv" is the per-thread-private data 
**
** If the per-thread private data table has a previously registered
** destructor function and a non-NULL per-thread-private data value,
** the destructor function is invoked.
**
** This can return PR_FAILURE if index is invalid (ie., beyond the current
** high water mark) or memory is insufficient to allocate an exanded vector.
*/

PR_IMPLEMENT(PRStatus) PR_SetThreadPrivate(PRUintn index, void *priv)
{
    PRStatus rv = PR_SUCCESS;
    PRThread *self = PR_GetCurrentThread();

    /*
    ** The index being set might not have a sufficient vector in this
    ** thread. But if the index has been allocated, it's okay to go
    ** ahead and extend this one now.
    */
    if (index >= _pr_tpd_highwater)
    {
        PR_ASSERT(index < _pr_tpd_highwater);
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
	    rv = PR_FAILURE;
    }
    else
    {
		PRIntn _is;

        _PR_LOCK_TPINDEX();
        if ((NULL == self->privateData) || (self->tpdLength <= index))
        {
            void *extension = PR_CALLOC(_pr_tpd_length * sizeof(void*));
            PR_ASSERT(
                ((NULL == self->privateData) && (0 == self->tpdLength))
                || ((NULL != self->privateData) && (0 != self->tpdLength)));
            if (NULL != extension)
            {
                (void)memcpy(
                    extension, self->privateData,
                    self->tpdLength * sizeof(void*));
                self->tpdLength = _pr_tpd_length;
                self->privateData = extension;
            }
        }
        /*
        ** There wasn't much chance of having to call the destructor
        ** unless the slot already existed.
        */
        else if (self->privateData[index] && _pr_tpd_destructors[index])
        {
            void *data = self->privateData[index];
            self->privateData[index] = NULL;
	        _PR_UNLOCK_TPINDEX();
	        (*_pr_tpd_destructors[index])(data);
            _PR_LOCK_TPINDEX();
        }

        /*
        ** If the thread's private data is still NULL, then we couldn't
        ** fix the problem. We must be outa-memory (again).
        */
        if (NULL == self->privateData)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            rv = PR_FAILURE;
        }
        else self->privateData[index] = priv;
	    _PR_UNLOCK_TPINDEX();
	}

	return rv;
}

/*
** Recover the per-thread-private data for the current thread. "index" is
** the index into the per-thread private data table. 
**
** The returned value may be NULL which is indistinguishable from an error 
** condition.
**
*/

PR_IMPLEMENT(void*) PR_GetThreadPrivate(uintn index)
{
    PRThread *self = PR_GetCurrentThread();
    void *tpd = ((NULL == self->privateData) || (index >= self->tpdLength)) ?
        NULL : self->privateData[index];

    return tpd;
}

PR_IMPLEMENT(PRStatus) PR_SetThreadExit(PRUintn index, PRThreadExit func, void *arg)
{
    _PRPerThreadExit *pte;
    PRThread *thread = _PR_MD_CURRENT_THREAD();

    if (index >= thread->numExits) {
	if (thread->ptes) {
	    thread->ptes = (_PRPerThreadExit*)
		PR_REALLOC(thread->ptes, (index+1) * sizeof(_PRPerThreadExit));
	} else {
	    thread->ptes = (_PRPerThreadExit*)
	        PR_CALLOC(index+1 * sizeof(_PRPerThreadExit));
	}
	if (!thread->ptes) {
	    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	    return PR_FAILURE;
	}
	thread->numExits = index + 1;
    }
    pte = &thread->ptes[index];
    pte->func = func;
    pte->arg = arg;
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRThreadExit) PR_GetThreadExit(PRUintn index, void **argp)
{
    _PRPerThreadExit *pte;
    PRThread *thread = _PR_MD_CURRENT_THREAD();

    if (index >= thread->numExits) {
	if (argp) *argp = 0;
	return 0;
    }
    pte = &thread->ptes[index];
    if (argp) *argp = pte->arg;
    return pte->func;
}
