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

/*
** As of this time, BeOS has its own TPD implementation.  Integrating
** this standard one is a TODO for anyone with a bit of spare time on
** their hand.  For now, we just #ifdef out this whole file and use
** the routines in pr/src/btthreads/
*/

#ifndef XP_BEOS

#include "primpl.h"

#include <string.h>

#define _PR_TPD_MODULO 8                /* vectors are extended by this much */
#define _PR_TPD_LIMIT 128               /* arbitary limit on the TPD slots */
static PRInt32 _pr_tpd_length = 0;      /* current length of destructor vector */
static PRInt32 _pr_tpd_highwater = 0;   /* next TPD key to be assigned */
PRThreadPrivateDTOR *_pr_tpd_destructors = NULL;
                                        /* the destructors are associated with
                                            the keys, therefore asserting that
                                            the TPD key depicts the data's 'type' */

/* Lock protecting the index assignment of per-thread-private data table */

/*
** Initialize the thread private data manipulation
*/
void _PR_InitTPD()
{
    _pr_tpd_destructors = (PRThreadPrivateDTOR*)
        PR_CALLOC(_PR_TPD_LIMIT * sizeof(PRThreadPrivateDTOR*));
    PR_ASSERT(NULL != _pr_tpd_destructors);
    _pr_tpd_length = _PR_TPD_LIMIT;
}

/*
** Clean up the thread private data manipulation
*/
void _PR_CleanupTPD(void)
{
}  /* _PR_CleanupTPD */

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
**     "dtor" is the destructor function to invoke when the private
**       data is destroyed
**
** Returns PR_FAILURE if the total number of indices will exceed the maximun 
** allowed.
*/

PR_IMPLEMENT(PRStatus) PR_NewThreadPrivateIndex(
    PRUintn *newIndex, PRThreadPrivateDTOR dtor)
{
    PRStatus rv;
    PRInt32 index;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    PR_ASSERT(NULL != newIndex);
    PR_ASSERT(NULL != _pr_tpd_destructors);

    index = PR_AtomicIncrement(&_pr_tpd_highwater);  /* allocate index */
    if (_PR_TPD_LIMIT < index)
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
        rv = PR_FAILURE;  /* that's just wrong */
    }
    else
    {
        _pr_tpd_destructors[index] = dtor;  /* record destructor @index */
        *newIndex = (PRIntn)index;  /* copy into client's location */
        rv = PR_SUCCESS;  /* that's okay */
    }

    return rv;
}

/*
** Define some per-thread-private data.
**     "index" is an index into the per-thread private data table
**     "priv" is the per-thread-private data 
**
** If the per-thread private data table has a previously registered
** destructor function and a non-NULL per-thread-private data value,
** the destructor function is invoked.
**
** This can return PR_FAILURE if index is invalid (ie., beyond the current
** high water mark) or memory is insufficient to allocate an exanded vector.
*/

PR_IMPLEMENT(PRStatus) PR_SetThreadPrivate(PRUintn his, void *priv)
{
    PRStatus rv = PR_SUCCESS;
    PRInt32 index = (PRInt32)his;
    PRThread *self = PR_GetCurrentThread();

    /*
    ** The index being set might not have a sufficient vector in this
    ** thread. But if the index has been allocated, it's okay to go
    ** ahead and extend this one now.
    */
    if (index > _pr_tpd_highwater)
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
        rv = PR_FAILURE;
    }
    else
    {
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
                self->privateData = (void**)extension;
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
            (*_pr_tpd_destructors[index])(data);
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

PR_IMPLEMENT(void*) PR_GetThreadPrivate(PRUintn his)
{
    PRInt32 index = (PRInt32)his;
    PRThread *self = PR_GetCurrentThread();
    void *tpd = ((NULL == self->privateData) || (index >= self->tpdLength)) ?
        NULL : self->privateData[index];

    return tpd;
}

/*
** Destroy the thread's private data, if any exists. This is called at
** thread termination time only. There should be no threading issues
** since this is being called by the thread itself.
*/
void _PR_DestroyThreadPrivate(PRThread* self)
{
    if (NULL != self->privateData)  /* we have some */
    {
        PRBool clean = PR_TRUE;
        PRInt32 index, passes = 4;
        PR_ASSERT(0 != self->tpdLength);
        do
        {
            for (index = 0; index < self->tpdLength; ++index)
            {
                void *priv = self->privateData[index];  /* extract */
                if (NULL != priv)  /* we have data at this index */
                {
                    if (NULL != _pr_tpd_destructors[index])
                    {
                        self->privateData[index] = NULL;  /* precondition */
                        (*_pr_tpd_destructors[index])(priv);  /* destroy */
                        clean = PR_FALSE;  /* unknown side effects */
                    }
                }
            }
        } while ((passes-- > 0) && !clean);  /* limit # of passes */
        PR_DELETE(self->privateData);  /* that's just this thread's vector */
    }
}  /* _PR_DestroyThreadPrivate */

#if 0
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

#endif

#endif /* !XP_BEOS */
