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
 * File:		prpdce.h
 * Description:	This file is the API defined to allow for DCE (aka POSIX)
 *				thread emulation in an NSPR environment. It is not the
 *				intent that this be a fully supported API.
 */

#if !defined(PRPDCE_H)
#define PRPDCE_H

#include "prlock.h"
#include "prcvar.h"
#include "prtypes.h"
#include "prinrval.h"

PR_BEGIN_EXTERN_C

#define _PR_NAKED_CV_LOCK (PRLock*)0xdce1dce1

/*
** Test and acquire a lock.
**
** If the lock is acquired by the calling thread, the
** return value will be PR_SUCCESS. If the lock is
** already held, by another thread or this thread, the
** result will be PR_FAILURE.
*/
PR_EXTERN(PRStatus) PRP_TryLock(PRLock *lock);

/*
** Create a naked condition variable
**
** A "naked" condition variable is one that is not created bound
** to a lock. The CV created with this function is the only type
** that may be used in the subsequent "naked" condition variable
** operations (see PRP_NakedWait, PRP_NakedNotify, PRP_NakedBroadcast);
*/
PR_EXTERN(PRCondVar*) PRP_NewNakedCondVar(void);

/*
** Destroy a naked condition variable
**
** Destroy the condition variable created by PR_NewNakedCondVar.
*/
PR_EXTERN(void) PRP_DestroyNakedCondVar(PRCondVar *cvar);

/*
** Wait on a condition
**
** Wait on the condition variable 'cvar'. It is asserted that
** the lock protecting the condition 'lock' is held by the
** calling thread. If more time expires than that declared in
** 'timeout' the condition will be notified. Waits can be
** interrupted by another thread.
**
** NB: The CV ('cvar') must be one created using PR_NewNakedCondVar.
*/
PR_EXTERN(PRStatus) PRP_NakedWait(
	PRCondVar *cvar, PRLock *lock, PRIntervalTime timeout);

/*
** Notify a thread waiting on a condition
**
** Notify the condition specified 'cvar'.
**
** NB: The CV ('cvar') must be one created using PR_NewNakedCondVar.
*/
PR_EXTERN(PRStatus) PRP_NakedNotify(PRCondVar *cvar);

/*
** Notify all threads waiting on a condition
**
** Notify the condition specified 'cvar'.
**
** NB: The CV ('cvar') must be one created using PR_NewNakedCondVar.
*/
PR_EXTERN(PRStatus) PRP_NakedBroadcast(PRCondVar *cvar);

PR_END_EXTERN_C

#endif /* PRPDCE_H */
