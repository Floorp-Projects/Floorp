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
** File:		prrwlock.h
** Description:	API to basic reader-writer lock functions of NSPR.
**
**/

#ifndef prrwlock_h___
#define prrwlock_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
 * PRRWLock --
 *
 *	The reader writer lock, PRRWLock, is an opaque object to the clients
 *	of NSPR.  All routines operate on a pointer to this opaque entity.
 */


typedef struct PRRWLock PRRWLock;

#define	PR_RWLOCK_RANK_NONE	0


/***********************************************************************
** FUNCTION:    PR_NewRWLock
** DESCRIPTION:
**  Returns a pointer to a newly created reader-writer lock object.
** INPUTS:      Lock rank
**				Lock name
** OUTPUTS:     void
** RETURN:      PRRWLock*
**   If the lock cannot be created because of resource constraints, NULL
**   is returned.
**  
***********************************************************************/
PR_EXTERN(PRRWLock*) PR_NewRWLock(PRUint32 lock_rank, const char *lock_name);

/***********************************************************************
** FUNCTION:    PR_DestroyRWLock
** DESCRIPTION:
**  Destroys a given RW lock object.
** INPUTS:      PRRWLock *lock - Lock to be freed.
** OUTPUTS:     void
** RETURN:      None
***********************************************************************/
PR_EXTERN(void) PR_DestroyRWLock(PRRWLock *lock);

/***********************************************************************
** FUNCTION:    PR_RWLock_Rlock
** DESCRIPTION:
**  Apply a read lock (non-exclusive) on a RWLock
** INPUTS:      PRRWLock *lock - Lock to be read-locked.
** OUTPUTS:     void
** RETURN:      None
***********************************************************************/
PR_EXTERN(void) PR_RWLock_Rlock(PRRWLock *lock);

/***********************************************************************
** FUNCTION:    PR_RWLock_Wlock
** DESCRIPTION:
**  Apply a write lock (exclusive) on a RWLock
** INPUTS:      PRRWLock *lock - Lock to write-locked.
** OUTPUTS:     void
** RETURN:      None
***********************************************************************/
PR_EXTERN(void) PR_RWLock_Wlock(PRRWLock *lock);

/***********************************************************************
** FUNCTION:    PR_RWLock_Unlock
** DESCRIPTION:
**  Release a RW lock. Unlocking an unlocked lock has undefined results.
** INPUTS:      PRRWLock *lock - Lock to unlocked.
** OUTPUTS:     void
** RETURN:      void
***********************************************************************/
PR_EXTERN(void) PR_RWLock_Unlock(PRRWLock *lock);

PR_END_EXTERN_C

#endif /* prrwlock_h___ */
