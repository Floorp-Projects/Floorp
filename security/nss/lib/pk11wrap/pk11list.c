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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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


