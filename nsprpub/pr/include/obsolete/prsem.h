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

#ifndef prsem_h___
#define prsem_h___

/*
** API for counting semaphores. Semaphores are counting synchronizing 
** variables based on a lock and a condition variable.  They are lightweight 
** contention control for a given count of resources.
*/
#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef struct PRSemaphore PRSemaphore;

/*
** Create a new semaphore object.
*/
PR_EXTERN(PRSemaphore*) PR_NewSem(PRUintn value);

/*
** Destroy the given semaphore object.
**
*/
PR_EXTERN(void) PR_DestroySem(PRSemaphore *sem);

/*
** Wait on a Semaphore.
** 
** This routine allows a calling thread to wait or proceed depending upon the 
** state of the semahore sem. The thread can proceed only if the counter value 
** of the semaphore sem is currently greater than 0. If the value of semaphore 
** sem is positive, it is decremented by one and the routine returns immediately 
** allowing the calling thread to continue. If the value of semaphore sem is 0, 
** the calling thread blocks awaiting the semaphore to be released by another 
** thread.
** 
** This routine can return PR_PENDING_INTERRUPT if the waiting thread 
** has been interrupted.
*/
PR_EXTERN(PRStatus) PR_WaitSem(PRSemaphore *sem);

/*
** This routine increments the counter value of the semaphore. If other threads 
** are blocked for the semaphore, then the scheduler will determine which ONE 
** thread will be unblocked.
*/
PR_EXTERN(void) PR_PostSem(PRSemaphore *sem);

/*
** Returns the value of the semaphore referenced by sem without affecting
** the state of the semaphore.  The value represents the semaphore vaule
F** at the time of the call, but may not be the actual value when the
** caller inspects it.
*/
PR_EXTERN(PRUintn) PR_GetValueSem(PRSemaphore *sem);

PR_END_EXTERN_C

#endif /* prsem_h___ */
