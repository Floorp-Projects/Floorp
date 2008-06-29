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

#if defined(WIN95)
/*
** Some local variables report warnings on Win95 because the code paths 
** using them are conditioned on HAVE_CUSTOME_USER_THREADS.
** The pragma suppresses the warning.
** 
*/
#pragma warning(disable : 4101)
#endif

#define _PR_TPD_LIMIT 128               /* arbitary limit on the TPD slots */
static PRInt32 _pr_tpd_length = 0;      /* current length of destructor vector */
static PRInt32 _pr_tpd_highwater = 0;   /* next TPD key to be assigned */
static PRThreadPrivateDTOR *_pr_tpd_destructors = NULL;
                                        /* the destructors are associated with
                                            the keys, therefore asserting that
                                            the TPD key depicts the data's 'type' */

/*
** Initialize the thread private data manipulation
*/
void _PR_InitTPD(void)
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
**       data is set or destroyed
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

    index = PR_AtomicIncrement(&_pr_tpd_highwater) - 1;  /* allocate index */
    if (_PR_TPD_LIMIT <= index)
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
        rv = PR_FAILURE;  /* that's just wrong */
    }
    else
    {
        _pr_tpd_destructors[index] = dtor;  /* record destructor @index */
        *newIndex = (PRUintn)index;  /* copy into client's location */
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

PR_IMPLEMENT(PRStatus) PR_SetThreadPrivate(PRUintn index, void *priv)
{
    PRThread *self = PR_GetCurrentThread();

    /*
    ** The index being set might not have a sufficient vector in this
    ** thread. But if the index has been allocated, it's okay to go
    ** ahead and extend this one now.
    */
    if ((index >= _PR_TPD_LIMIT) || (index >= _pr_tpd_highwater))
    {
        PR_SetError(PR_TPD_RANGE_ERROR, 0);
        return PR_FAILURE;
    }

    PR_ASSERT(((NULL == self->privateData) && (0 == self->tpdLength))
        || ((NULL != self->privateData) && (0 != self->tpdLength)));

    if ((NULL == self->privateData) || (self->tpdLength <= index))
    {
        void *extension = PR_CALLOC(_pr_tpd_length * sizeof(void*));
        if (NULL == extension)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_FAILURE;
        }
        if (self->privateData) {
            (void)memcpy(
                extension, self->privateData,
                self->tpdLength * sizeof(void*));
            PR_DELETE(self->privateData);
        }
        self->tpdLength = _pr_tpd_length;
        self->privateData = (void**)extension;
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

    PR_ASSERT(index < self->tpdLength);
    self->privateData[index] = priv;

    return PR_SUCCESS;
}

/*
** Recover the per-thread-private data for the current thread. "index" is
** the index into the per-thread private data table. 
**
** The returned value may be NULL which is indistinguishable from an error 
** condition.
**
*/

PR_IMPLEMENT(void*) PR_GetThreadPrivate(PRUintn index)
{
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
#define _PR_TPD_DESTRUCTOR_ITERATIONS 4

    if (NULL != self->privateData)  /* we have some */
    {
        PRBool clean;
        PRUint32 index;
        PRInt32 passes = _PR_TPD_DESTRUCTOR_ITERATIONS;
        PR_ASSERT(0 != self->tpdLength);
        do
        {
            clean = PR_TRUE;
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
        } while ((--passes > 0) && !clean);  /* limit # of passes */
        /*
        ** We give up after a fixed number of passes. Any non-NULL
        ** thread-private data value with a registered destructor
        ** function is not destroyed.
        */
        memset(self->privateData, 0, self->tpdLength * sizeof(void*));
    }
}  /* _PR_DestroyThreadPrivate */

#endif /* !XP_BEOS */
