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

#ifndef prolock_h___
#define prolock_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

#if defined(DEBUG) || defined(FORCE_NSPR_ORDERED_LOCKS)

/*
** A locking mechanism, built on the existing PRLock definiion,
** is provided that will permit applications to define a Lock
** Hierarchy (or Lock Ordering) schema. An application designed
** using the Ordered Lock functions will terminate with a
** diagnostic message when a lock inversion condition is
** detected. 
** 
** The lock ordering detection is complile-time enabled only. in
** optimized builds of NSPR, the Ordered Lock functions map
** directly to PRLock functions, providing no lock order
** detection.
** 
** The Ordered Lock Facility is compiled in when DEBUG is defined at
** compile time. Ordered Lock can be forced on in optimized builds by
** defining FORCE_NSPR_ORDERED_LOCK at compile time. Both the
** application using Ordered Lock and NSPR must be compiled with the
** facility enabled to achieve the desired results.
** 
** Application designers should use the macro interfaces to the Ordered
** Lock facility to ensure that it is compiled out in optimized builds.
**
** Application designers are responsible for defining their own
** lock hierarchy. 
**
** Ordered Lock is thread-safe and SMP safe.
**
** See Also: prlock.h
**
** /lth. 10-Jun-1998.
**
*/

/*
** Opaque type for ordered lock.
** ... Don't even think of looking in here.
**
*/

typedef void * PROrderedLock;

/* -----------------------------------------------------------------------
** FUNCTION: PR_CreateOrderedLock() -- Create an Ordered Lock
** 
** DESCRIPTION: PR_CreateOrderedLock() creates an ordered lock.
** 
** INPUTS:
**  order: user defined order of this lock.
**  name: name of the lock. For debugging purposes.
** 
** OUTPUTS: returned
** 
** RETURNS: PR_OrderedLock pointer
** 
** RESTRICTIONS:
** 
*/
#define PR_CREATE_ORDERED_LOCK(order,name)\
    PR_CreateOrderedLock((order),(name))

PR_EXTERN(PROrderedLock *) 
    PR_CreateOrderedLock( 
        PRInt32 order,
        const char *name
);

/* -----------------------------------------------------------------------
** FUNCTION: PR_DestroyOrderedLock() -- Destroy an Ordered Lock
** 
** DESCRIPTION: PR_DestroyOrderedLock() destroys the ordered lock
** referenced by lock.
** 
** INPUTS: lock: pointer to a PROrderedLock
** 
** OUTPUTS: the lock is destroyed
** 
** RETURNS: void
** 
** RESTRICTIONS:
** 
*/
#define PR_DESTROY_ORDERED_LOCK(lock) PR_DestroyOrderedLock((lock))

PR_EXTERN(void) 
    PR_DestroyOrderedLock( 
        PROrderedLock *lock 
);

/* -----------------------------------------------------------------------
** FUNCTION: PR_LockOrderedLock() -- Lock an ordered lock
** 
** DESCRIPTION: PR_LockOrderedLock() locks the ordered lock
** referenced by lock. If the order of lock is less than or equal
** to the order of the highest lock held by the locking thread,
** the function asserts.
** 
** INPUTS: lock: a pointer to a PROrderedLock
** 
** OUTPUTS: The lock is held or the fucntion asserts.
** 
** RETURNS: void
** 
** RESTRICTIONS:
** 
*/
#define PR_LOCK_ORDERED_LOCK(lock) PR_LockOrderedLock((lock))

PR_EXTERN(void) 
    PR_LockOrderedLock( 
        PROrderedLock *lock 
);

/* -----------------------------------------------------------------------
** FUNCTION: PR_UnlockOrderedLock() -- unlock and Ordered Lock
** 
** DESCRIPTION: PR_UnlockOrderedLock() unlocks the lock referenced
** by lock.
** 
** INPUTS: lock: a pointer to a PROrderedLock
** 
** OUTPUTS: the lock is unlocked
** 
** RETURNS:
**  PR_SUCCESS
**  PR_FAILURE
** 
** RESTRICTIONS:
** 
*/
#define PR_UNLOCK_ORDERED_LOCK(lock) PR_UnlockOrderedLock((lock))

PR_EXTERN(PRStatus) 
    PR_UnlockOrderedLock( 
        PROrderedLock *lock 
);

#else /* !(defined(DEBUG) || defined(FORCE_NSPR_ORDERED_LOCKS)) */
/*
** Map PROrderedLock and methods onto PRLock when ordered locking
** is not compiled in.
**  
*/
#include <prlock.h>

typedef PRLock PROrderedLock;

#define PR_CREATE_ORDERED_LOCK(order) PR_NewLock()
#define PR_DESTROY_ORDERED_LOCK(lock) PR_DestroyLock((lock))
#define PR_LOCK_ORDERED_LOCK(lock) PR_Lock((lock))
#define PR_UNLOCK_ORDERED_LOCK(lock) PR_Unlock((lock))

#endif /* !(defined(DEBUG) || defined(FORCE_NSPR_ORDERED_LOCKS)) */

PR_END_EXTERN_C

#endif /* prolock_h___ */













