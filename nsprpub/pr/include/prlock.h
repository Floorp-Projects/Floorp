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
** File:		prlock.h
** Description:	API to basic locking functions of NSPR.
**
**
** NSPR provides basic locking mechanisms for thread synchronization.  Locks 
** are lightweight resource contention controls that prevent multiple threads 
** from accessing something (code/data) simultaneously.
**/

#ifndef prlock_h___
#define prlock_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

/*
 * PRLock --
 *
 *     NSPR represents the lock as an opaque entity to the client of the
 *	   API.  All routines operate on a pointer to this opaque entity.
 */

typedef struct PRLock PRLock;

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/***********************************************************************
** FUNCTION:    PR_NewLock
** DESCRIPTION:
**  Returns a pointer to a newly created opaque lock object.
** INPUTS:      void
** OUTPUTS:     void
** RETURN:      PRLock*
**   If the lock can not be created because of resource constraints, NULL
**   is returned.
**  
***********************************************************************/
PR_EXTERN(PRLock*) PR_NewLock(void);

/***********************************************************************
** FUNCTION:    PR_DestroyLock
** DESCRIPTION:
**  Destroys a given opaque lock object.
** INPUTS:      PRLock *lock
**              Lock to be freed.
** OUTPUTS:     void
** RETURN:      None
***********************************************************************/
PR_EXTERN(void) PR_DestroyLock(PRLock *lock);

/***********************************************************************
** FUNCTION:    PR_Lock
** DESCRIPTION:
**  Lock a lock.
** INPUTS:      PRLock *lock
**              Lock to locked.
** OUTPUTS:     void
** RETURN:      None
***********************************************************************/
PR_EXTERN(void) PR_Lock(PRLock *lock);

/***********************************************************************
** FUNCTION:    PR_Unlock
** DESCRIPTION:
**  Unlock a lock.  Unlocking an unlocked lock has undefined results.
** INPUTS:      PRLock *lock
**              Lock to unlocked.
** OUTPUTS:     void
** RETURN:      PR_STATUS
**              Returns PR_FAILURE if the caller does not own the lock.
***********************************************************************/
PR_EXTERN(PRStatus) PR_Unlock(PRLock *lock);

PR_END_EXTERN_C

#endif /* prlock_h___ */
