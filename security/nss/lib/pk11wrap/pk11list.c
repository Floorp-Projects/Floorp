/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Locking and queue management primatives
 *
 */

#include "seccomon.h"
#include "nssilock.h"
#include "secmod.h"
#include "secmodi.h"
#include "secmodti.h"
#include "nssrwlk.h"

/*
 * create a new lock for a Module List
 */
SECMODListLock *SECMOD_NewListLock()
{
    return NSSRWLock_New( 10, "moduleListLock");
}

/*
 * destroy the lock
 */
void SECMOD_DestroyListLock(SECMODListLock *lock) 
{
    NSSRWLock_Destroy(lock);
}


/*
 * Lock the List for Read: NOTE: this assumes the reading isn't so common
 * the writing will be starved.
 */
void SECMOD_GetReadLock(SECMODListLock *modLock) 
{
    NSSRWLock_LockRead(modLock);
}

/*
 * Release the Read lock
 */
void SECMOD_ReleaseReadLock(SECMODListLock *modLock) 
{
    NSSRWLock_UnlockRead(modLock);
}


/*
 * lock the list for Write
 */
void SECMOD_GetWriteLock(SECMODListLock *modLock) 
{
    NSSRWLock_LockWrite(modLock);
}


/*
 * Release the Write Lock: NOTE, this code is pretty inefficient if you have
 * lots of write collisions.
 */
void SECMOD_ReleaseWriteLock(SECMODListLock *modLock) 
{
    NSSRWLock_UnlockWrite(modLock);
}


/*
 * must Hold the Write lock
 */
void
SECMOD_RemoveList(SECMODModuleList **parent, SECMODModuleList *child) 
{
    *parent = child->next;
    child->next = NULL;
}

/*
 * if lock is not specified, it must already be held
 */
void
SECMOD_AddList(SECMODModuleList *parent, SECMODModuleList *child, 
							SECMODListLock *lock) 
{
    if (lock) { SECMOD_GetWriteLock(lock); }

    child->next = parent->next;
    parent->next = child;

   if (lock) { SECMOD_ReleaseWriteLock(lock); }
}


